#pragma once
#include <lvgl.h>

/* Gesture codes reported by CST816S register 0x00 */
enum TouchGesture : uint8_t {
    GESTURE_NONE        = 0x00,
    GESTURE_SWIPE_UP    = 0x01,
    GESTURE_SWIPE_DOWN  = 0x02,
    GESTURE_SWIPE_LEFT  = 0x03,
    GESTURE_SWIPE_RIGHT = 0x04,
    GESTURE_SINGLE_TAP  = 0x05,
    GESTURE_DOUBLE_TAP  = 0x0B,
    GESTURE_LONG_PRESS  = 0x0C,
};

/*
 * Initialise RM67162 display, register LVGL flush + touch input callbacks.
 * Must be called before any lv_* widget calls.
 */
void display_driver_init(void);

/* Returns the last gesture detected; cleared after each read. */
TouchGesture display_get_gesture(void);
