#pragma once
#include "fronius.h"
#include <time.h>

/* Screens available via touch swipe */
typedef enum {
    SCREEN_MAIN   = 0,   /* solar arc + consumption + SOC + grid */
    SCREEN_CLOCK  = 1,   /* analog clock + battery SOC arc */
    SCREEN_PV     = 2,   /* analog clock + PV generation arc */
    SCREEN_STATUS = 3,   /* WiFi IP/RSSI + board battery SOC */
    SCREEN_COUNT
} ScreenId;

/* Build all LVGL screens. Call once after display_driver_init(). */
void ui_init(void);

/* Push new sensor readings into the active screen's widgets. */
void ui_update(const PowerData *data);

/* Push sensor readings + current time into the clock screen's widgets. */
void ui_update_clock(const PowerData *data, const struct tm *t);

/* Switch to a different screen (animated slide).
   forward=true  → new screen enters from right (left swipe) or bottom (down swipe)
   forward=false → new screen enters from left  (right swipe) or top   (up swipe)
   vertical=true → use MOVE_BOTTOM / MOVE_TOP instead of MOVE_LEFT / MOVE_RIGHT */
void ui_switch_screen(ScreenId id, bool forward, bool vertical = false);

/* Update solar arc full-scale. Call after WiFiManager saves a new value. */
void ui_set_solar_max(uint32_t w);

/* Show IP on boot screen and start the 4-second auto-transition to SCREEN_MAIN. */
void ui_show_boot_ip(const char *ip);

/* Push WiFi + board-battery readings into the status screen widgets.
   bat_pct: 0-100 or -1 when PMIC/battery unavailable. */
void ui_update_status(const char *ip, int8_t rssi, int8_t bat_pct, bool charging);

ScreenId ui_current_screen(void);
