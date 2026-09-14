#pragma once
#include <lvgl.h>

/* Software-detected swipe gestures */
enum TouchGesture : uint8_t {
    GESTURE_NONE        = 0,
    GESTURE_SWIPE_UP    = 1,
    GESTURE_SWIPE_DOWN  = 2,
    GESTURE_SWIPE_LEFT  = 3,
    GESTURE_SWIPE_RIGHT = 4,
};

/*
 * Initialise CO5300 display, register LVGL flush + CST9217 touch callbacks.
 * Must be called before any lv_* widget calls.
 */
void display_driver_init(void);

/* Set panel brightness from 0 (off) to 255 (maximum). */
void display_set_brightness(uint8_t brightness);

/* Returns the last swipe gesture detected; cleared after each read. */
TouchGesture display_get_gesture(void);
