#pragma once
#include "fronius.h"

/* Screens available via touch swipe */
typedef enum {
    SCREEN_MAIN = 0,   /* solar arc + consumption + SOC + grid */
    /* SCREEN_DETAIL,  // reserved for a future detail/history screen */
    SCREEN_COUNT
} ScreenId;

/* Build all LVGL screens. Call once after display_driver_init(). */
void ui_init(void);

/* Push new sensor readings into the active screen's widgets. */
void ui_update(const PowerData *data);

/* Switch to a different screen (animated slide). */
void ui_switch_screen(ScreenId id);

ScreenId ui_current_screen(void);
