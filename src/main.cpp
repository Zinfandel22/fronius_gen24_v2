#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <time.h>

#include "config.h"
#include "display_driver.h"
#include "fronius.h"
#include "ui.h"

/* ---------------------------------------------------------------
   Shared state between fronius_task (core 0) and the UI (core 1)
   --------------------------------------------------------------- */
static PowerData         g_data       = {};
static SemaphoreHandle_t g_data_mutex = nullptr;

/* Inverter IP stored in NVS, filled by WiFiManager on first boot */
static char g_inverter_ip[16] = "192.168.1.1";

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
    /* Load stored inverter IP */
    Preferences prefs;
    prefs.begin("fronius", true);
    String stored = prefs.getString("ip", "");
    prefs.end();
    if (stored.length() > 0) {
        stored.toCharArray(g_inverter_ip, sizeof(g_inverter_ip));
    }
    Serial.printf("[wifi] stored inverter IP: '%s'\n", g_inverter_ip);

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

    WiFiManagerParameter ip_param(
        "inverter_ip", "Inverter IP Address", g_inverter_ip, 16);

    WiFiManager wm;
    wm.addParameter(&ip_param);
    wm.setConfigPortalTimeout(180);
    wm.setDebugOutput(true);   /* WiFiManager logs to Serial */

    Serial.printf("[wifi] connecting (AP name: %s if portal opens)\n", WIFI_AP_NAME);
    wm.autoConnect(WIFI_AP_NAME);

    Serial.printf("[wifi] connected, local IP: %s\n",
                  WiFi.localIP().toString().c_str());

    /* Persist whatever IP is now in the portal field */
    strlcpy(g_inverter_ip, ip_param.getValue(), sizeof(g_inverter_ip));
    Serial.printf("[wifi] inverter IP to use: '%s'\n", g_inverter_ip);
    prefs.begin("fronius", false);
    prefs.putString("ip", g_inverter_ip);
    prefs.end();
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

    ui_init();
    Serial.println("[boot] UI init done");

    PowerData connecting = {};
    connecting.valid = false;
    ui_update(&connecting);

    wifi_setup();

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
    lv_timer_handler();

    /* Handle swipe gestures for screen switching */
    TouchGesture gest = display_get_gesture();
    if (gest == GESTURE_SWIPE_LEFT) {
        ScreenId next = (ScreenId)((ui_current_screen() + 1) % SCREEN_COUNT);
        ui_switch_screen(next);
    } else if (gest == GESTURE_SWIPE_RIGHT) {
        ScreenId prev = (ScreenId)((ui_current_screen() + SCREEN_COUNT - 1) % SCREEN_COUNT);
        ui_switch_screen(prev);
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
    }

    delay(5);
}
