#include <Arduino.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

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
    const char *ip      = (const char *)arg;
    uint8_t     fails   = 0;

    while (true) {
        PowerData tmp = {};
        if (fronius_fetch(ip, &tmp)) {
            fails = 0;
            if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                g_data = tmp;
                xSemaphoreGive(g_data_mutex);
            }
        } else {
            if (++fails >= FAIL_THRESHOLD) {
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
    Preferences prefs;
    prefs.begin("fronius", true);
    String stored = prefs.getString("ip", "");
    prefs.end();
    if (stored.length() > 0) {
        stored.toCharArray(g_inverter_ip, sizeof(g_inverter_ip));
    }

    WiFiManagerParameter ip_param(
        "inverter_ip", "Inverter IP Address", g_inverter_ip, 16);

    WiFiManager wm;
    wm.addParameter(&ip_param);
    wm.setConfigPortalTimeout(180);
    wm.autoConnect(WIFI_AP_NAME);

    /* Persist whatever IP is now in the field (new or unchanged) */
    strlcpy(g_inverter_ip, ip_param.getValue(), sizeof(g_inverter_ip));
    prefs.begin("fronius", false);
    prefs.putString("ip", g_inverter_ip);
    prefs.end();
}

/* ---------------------------------------------------------------
   Arduino entry points
   --------------------------------------------------------------- */
void setup(void) {
    Serial.begin(115200);

    display_driver_init();
    ui_init();

    /* Show a connecting message on the display while WiFiManager runs */
    PowerData connecting = {};
    connecting.valid = false;
    ui_update(&connecting);

    wifi_setup();

    g_data_mutex = xSemaphoreCreateMutex();

    /* Fronius task pinned to core 0; Arduino loop() runs on core 1 */
    xTaskCreatePinnedToCore(
        fronius_task,
        "fronius",
        8192,
        (void *)g_inverter_ip,
        1,           /* priority */
        nullptr,
        0            /* core 0 */
    );
}

void loop(void) {
    /* Drive LVGL — must be called repeatedly */
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
    uint32_t now = millis();
    if (now - last_update >= 500) {
        last_update = now;
        PowerData snapshot = {};
        if (xSemaphoreTake(g_data_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            snapshot = g_data;
            xSemaphoreGive(g_data_mutex);
        }
        ui_update(&snapshot);
    }

    delay(5);   /* ~200 Hz cap keeps LVGL responsive without busy-looping */
}
