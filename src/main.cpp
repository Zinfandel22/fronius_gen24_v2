#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>

#include "config.h"
#include "display_driver.h"
#include "fronius.h"
#include "ui.h"
#include <XPowersLib.h>

/* ---------------------------------------------------------------
   Shared state between fronius_task (core 0) and the UI (core 1)
   --------------------------------------------------------------- */
static PowerData         g_data       = {};
static SemaphoreHandle_t g_data_mutex = nullptr;

/* Inverter IP stored in NVS, filled by WiFiManager on first boot */
static char g_inverter_ip[16] = "192.168.1.1";

static WebServer g_ota_server(80);

/* AXP2101 board PMIC (shares I2C bus with CST9217 touch) */
static XPowersAXP2101 g_pmic;
static bool           g_pmic_ok = false;

/* ---------------------------------------------------------------
   Fronius polling task — runs on core 0
   --------------------------------------------------------------- */
static void fronius_task(void *arg) {
    const char *ip    = (const char *)arg;
    uint8_t     fails = 0;

    Serial.printf("[fronius] task started, polling %s every %d ms\n",
                  ip, POLL_INTERVAL_MS);

    while (true) {
        PowerData tmp = {};
        if (fronius_fetch(ip, &tmp)) {
            fails = 0;
            Serial.printf("[fronius] OK  solar=%.0f W  inv=%.0f W  load=%.0f W  grid=%+.0f W  soc=%.0f%%\n",
                          tmp.solar_w, tmp.inverter_w, tmp.consumption_w, tmp.grid_w, tmp.soc_pct);

            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                g_data = tmp;
                xSemaphoreGive(g_data_mutex);
            } else {
                Serial.println("[fronius] WARN mutex take timed out on write");
            }
        } else {
            ++fails;
            Serial.printf("[fronius] FAIL #%d (threshold %d)\n", fails, FAIL_THRESHOLD);
            if (fails >= FAIL_THRESHOLD) {
                if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                    g_data.valid = false;
                    xSemaphoreGive(g_data_mutex);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

/* ---------------------------------------------------------------
   WiFiManager setup
   Blocks until connected. Inverter IP saved to / loaded from NVS.
   --------------------------------------------------------------- */
static void wifi_setup(void) {
    /* Load stored inverter IP and solar max */
    Preferences prefs;
    prefs.begin("fronius", true);
    String stored = prefs.getString("ip", "");
    uint32_t stored_solar = prefs.getUInt("solar_input", 6000);
    prefs.end();
    if (stored.length() > 0) {
        stored.toCharArray(g_inverter_ip, sizeof(g_inverter_ip));
    }
    Serial.printf("[wifi] stored inverter IP: '%s', solar max: %lu W\n",
                  g_inverter_ip, (unsigned long)stored_solar);

    /* Check for factory-reset gesture: hold touch INT low for 3 s at boot */
    pinMode(TOUCH_INT, INPUT_PULLUP);
    if (digitalRead(TOUCH_INT) == LOW) {
        Serial.println("[wifi] touch held — waiting 3 s for factory reset...");
        delay(3000);
        if (digitalRead(TOUCH_INT) == LOW) {
            Serial.println("[wifi] FACTORY RESET: clearing NVS and WiFi credentials");
            Preferences p;
            p.begin("fronius", false);
            p.clear();
            p.end();
            WiFiManager wm;
            wm.resetSettings();
        }
    }

    char solar_buf[8];
    snprintf(solar_buf, sizeof(solar_buf), "%lu", (unsigned long)stored_solar);

    WiFiManagerParameter ip_param(
        "inverter_ip", "Inverter IP Address", g_inverter_ip, 16);
    WiFiManagerParameter solar_param(
        "solar_input", "Solar Max Watts", solar_buf, 6);

    WiFiManager wm;
    wm.addParameter(&ip_param);
    wm.addParameter(&solar_param);
    wm.setConfigPortalTimeout(180);
    wm.setDebugOutput(true);   /* WiFiManager logs to Serial */

    Serial.printf("[wifi] connecting (AP name: %s if portal opens)\n", WIFI_AP_NAME);
    wm.autoConnect(WIFI_AP_NAME);

    Serial.printf("[wifi] connected, local IP: %s\n",
                  WiFi.localIP().toString().c_str());

    /* Persist whatever is now in the portal fields */
    strlcpy(g_inverter_ip, ip_param.getValue(), sizeof(g_inverter_ip));
    Serial.printf("[wifi] inverter IP to use: '%s'\n", g_inverter_ip);

    uint32_t solar_val = (uint32_t)atoi(solar_param.getValue());
    if (solar_val < 100 || solar_val > 99999) solar_val = 6000;
    Serial.printf("[wifi] solar max to use: %lu W\n", (unsigned long)solar_val);

    prefs.begin("fronius", false);
    prefs.putString("ip", g_inverter_ip);
    prefs.putUInt("solar_input", solar_val);
    prefs.end();

    ui_set_solar_max(solar_val);
}

/* ---------------------------------------------------------------
   Arduino entry points
   --------------------------------------------------------------- */
void setup(void) {
    Serial.begin(115200);
    delay(200);   /* let USB-CDC settle before first print */
    Serial.println("\n[boot] Fronius Gen24 Monitor starting");

    display_driver_init();
    Serial.println("[boot] display init done");

    /* Wire is already up (100 kHz, SDA=15, SCL=14) from display_driver_init() */
    g_pmic_ok = g_pmic.begin(Wire, AXP2101_I2C_ADDR, TOUCH_SDA, TOUCH_SCL);
    Serial.printf("[pmic] AXP2101 init: %s\n", g_pmic_ok ? "OK" : "not found");

    ui_init();
    Serial.println("[boot] UI init done");

    /* Force initial render so boot screen appears before WiFi blocks */
    for (int i = 0; i < 5; i++) {
        lv_timer_handler();
        delay(10);
    }

    wifi_setup();
    ui_show_boot_ip(WiFi.localIP().toString().c_str());

    g_ota_server.on("/", []() {
        g_ota_server.sendHeader("Location", "/update");
        g_ota_server.send(302, "text/plain", "");
    });
    ElegantOTA.begin(&g_ota_server);
    g_ota_server.begin();
    Serial.printf("[ota] update page at http://%s/update\n",
                  WiFi.localIP().toString().c_str());

    configTime(TZ_OFFSET_SEC, DST_OFFSET_SEC, "pool.ntp.org");
    Serial.println("[boot] NTP sync started");

    g_data_mutex = xSemaphoreCreateMutex();

    /* Fronius task pinned to core 0; Arduino loop() runs on core 1 */
    BaseType_t rc = xTaskCreatePinnedToCore(
        fronius_task,
        "fronius",
        16384,              /* 16 KB — HTTP + JSON needs headroom */
        (void *)g_inverter_ip,
        1,
        nullptr,
        0
    );
    Serial.printf("[boot] fronius task create: %s\n",
                  rc == pdPASS ? "OK" : "FAILED");
}

void loop(void) {
    g_ota_server.handleClient();
    ElegantOTA.loop();
    lv_timer_handler();

    /* Handle swipe gestures for screen switching */
    TouchGesture gest = display_get_gesture();
    ScreenId cur = ui_current_screen();
    if (gest == GESTURE_SWIPE_RIGHT) {
        if      (cur == SCREEN_MAIN)                       ui_switch_screen(SCREEN_CLOCK, true);
        else if (cur == SCREEN_CLOCK || cur == SCREEN_PV)  ui_switch_screen(SCREEN_MAIN,  true);
    } else if (gest == GESTURE_SWIPE_LEFT) {
        if      (cur == SCREEN_MAIN)                       ui_switch_screen(SCREEN_CLOCK, false);
        else if (cur == SCREEN_CLOCK || cur == SCREEN_PV)  ui_switch_screen(SCREEN_MAIN,  false);
    } else if (gest == GESTURE_SWIPE_DOWN) {
        if      (cur == SCREEN_CLOCK)   ui_switch_screen(SCREEN_PV,     true,  true);
        else if (cur == SCREEN_PHASES)  ui_switch_screen(SCREEN_MAIN,   true,  true);
        else if (cur == SCREEN_STATUS)  ui_switch_screen(SCREEN_PHASES, true,  true);
    } else if (gest == GESTURE_SWIPE_UP) {
        if      (cur == SCREEN_PV)      ui_switch_screen(SCREEN_CLOCK,  false, true);
        else if (cur == SCREEN_MAIN)    ui_switch_screen(SCREEN_PHASES, false, true);
        else if (cur == SCREEN_PHASES)  ui_switch_screen(SCREEN_STATUS, false, true);
    }

    /* Copy shared data snapshot and update widgets at ~2 Hz */
    static uint32_t last_update = 0;
    static bool      last_valid  = false;
    uint32_t now = millis();
    if (now - last_update >= 500) {
        last_update = now;
        PowerData snapshot = {};
        if (g_data_mutex &&
            xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            snapshot = g_data;
            xSemaphoreGive(g_data_mutex);
        }

        /* Log only on valid-state transitions to avoid serial spam */
        if (snapshot.valid != last_valid) {
            Serial.printf("[ui] data state changed → %s\n",
                          snapshot.valid ? "VALID" : "NO DATA");
            last_valid = snapshot.valid;
        }

        ui_update(&snapshot);

        struct tm timeinfo = {};
        getLocalTime(&timeinfo);
        ui_update_clock(&snapshot, &timeinfo);
        ui_update_phases(&snapshot);

        /* Board battery + WiFi status */
        int8_t bat_pct = -1;
        bool   charging = false;
        if (g_pmic_ok && g_pmic.isBatteryConnect()) {
            bat_pct  = (int8_t)g_pmic.getBatteryPercent();
            charging = g_pmic.isCharging();
        }
        int8_t rssi = (int8_t)WiFi.RSSI();
        ui_update_status(WiFi.localIP().toString().c_str(), rssi, bat_pct, charging);
    }

    delay(5);
}
