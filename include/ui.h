#pragma once
#include "fronius.h"
#include <time.h>

/* Screens available via touch swipe */
typedef enum {
    SCREEN_MAIN  = 0,   /* solar arc + consumption + SOC + grid */
    SCREEN_CLOCK = 1,   /* analog clock + battery SOC arc */
    SCREEN_COUNT
} ScreenId;

/* Build all LVGL screens. Call once after display_driver_init(). */
void ui_init(void);

/* Push new sensor readings into the active screen's widgets. */
void ui_update(const PowerData *data);

/* Push sensor readings + current time into the clock screen's widgets. */
void ui_update_clock(const PowerData *data, const struct tm *t);

/* Switch to a different screen (animated slide).
   forward=true  → new screen enters from right (swiped left)
   forward=false → new screen enters from left  (swiped right) */
void ui_switch_screen(ScreenId id, bool forward);

/* Update solar arc full-scale. Call after WiFiManager saves a new value. */
void ui_set_solar_max(uint32_t w);

/* Show IP on boot screen and start the 4-second auto-transition to SCREEN_MAIN. */
void ui_show_boot_ip(const char *ip);

ScreenId ui_current_screen(void);
