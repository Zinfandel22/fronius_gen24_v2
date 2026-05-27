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

/* Switch to a different screen (animated slide). */
void ui_switch_screen(ScreenId id);

ScreenId ui_current_screen(void);
