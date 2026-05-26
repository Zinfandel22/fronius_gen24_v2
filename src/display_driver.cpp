#include "display_driver.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

/* ---------------------------------------------------------------
   CO5300 display via QSPI
   col_offset1=6 centres the 466-wide frame in the 480-wide panel.
   --------------------------------------------------------------- */
static Arduino_DataBus *g_bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDA0, LCD_SDA1, LCD_SDA2, LCD_SDA3);

static Arduino_GFX *g_gfx = new Arduino_CO5300(
    g_bus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);

/* ---------------------------------------------------------------
   LVGL display buffers — allocated from PSRAM
   --------------------------------------------------------------- */
static uint8_t *g_buf1 = nullptr;
static uint8_t *g_buf2 = nullptr;

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    g_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(disp);
}

/* ---------------------------------------------------------------
   CST9217 touch controller via I2C
   Register 0xD000 returns 15 bytes:
     [5]     = finger count (& 0x7F)
     [6..10] = first touch point (5 bytes)
       pdat[1]      = X high 8 bits (of 12-bit value)
       pdat[2]      = Y high 8 bits
       pdat[3] hi   = X low 4 bits
       pdat[3] lo   = Y low 4 bits
   --------------------------------------------------------------- */
static volatile TouchGesture g_last_gesture = GESTURE_NONE;

static void touch_reset(void) {
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
}

static bool cst9217_read(uint16_t *x, uint16_t *y, uint8_t *num_points) {
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0xD0);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) {
        *num_points = 0;
        return false;
    }

    uint8_t buf[15];
    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)sizeof(buf));
    if (Wire.available() < (int)sizeof(buf)) {
        *num_points = 0;
        return false;
    }
    for (size_t i = 0; i < sizeof(buf); i++) {
        buf[i] = Wire.read();
    }

    *num_points = buf[5] & 0x7F;
    if (*num_points == 0) return false;

    const uint8_t *pdat = buf + 6;
    *x = ((uint16_t)pdat[1] << 4) | (pdat[3] >> 4);
    *y = ((uint16_t)pdat[2] << 4) | (pdat[3] & 0x0F);
    return true;
}

/* ---------------------------------------------------------------
   Software swipe detection
   Tracks start/end positions across one finger-down → finger-up cycle.
   --------------------------------------------------------------- */
#define SWIPE_MIN_PX  40   /* minimum travel to count as a swipe */

static lv_indev_state_t g_prev_touch = LV_INDEV_STATE_RELEASED;
static int16_t g_start_x = 0, g_start_y = 0;
static int16_t g_last_x  = 0, g_last_y  = 0;

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    uint16_t x, y;
    uint8_t num;
    bool touched = cst9217_read(&x, &y, &num);

    if (touched) {
        data->point.x = (int32_t)x;
        data->point.y = (int32_t)y;
        data->state   = LV_INDEV_STATE_PRESSED;

        if (g_prev_touch == LV_INDEV_STATE_RELEASED) {
            g_start_x = (int16_t)x;
            g_start_y = (int16_t)y;
        }
        g_last_x = (int16_t)x;
        g_last_y = (int16_t)y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        if (g_prev_touch == LV_INDEV_STATE_PRESSED) {
            int16_t dx  = g_last_x - g_start_x;
            int16_t dy  = g_last_y - g_start_y;
            int16_t adx = dx < 0 ? -dx : dx;
            int16_t ady = dy < 0 ? -dy : dy;

            if (adx >= SWIPE_MIN_PX && adx >= ady * 2) {
                g_last_gesture = (dx < 0) ? GESTURE_SWIPE_LEFT : GESTURE_SWIPE_RIGHT;
            } else if (ady >= SWIPE_MIN_PX && ady >= adx * 2) {
                g_last_gesture = (dy < 0) ? GESTURE_SWIPE_UP : GESTURE_SWIPE_DOWN;
            }
        }
    }
    g_prev_touch = data->state;
}

/* ---------------------------------------------------------------
   Public API
   --------------------------------------------------------------- */
void display_driver_init(void) {
    pinMode(TOUCH_RST, OUTPUT);
    touch_reset();
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    g_gfx->begin();
    g_gfx->fillScreen(0x0000);  /* RGB565 black */

    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)millis);

    const size_t buf_px    = LCD_WIDTH * (LCD_HEIGHT / 10);
    const size_t buf_bytes = buf_px * sizeof(lv_color_t);
    g_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    g_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);

    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, g_buf1, g_buf2, buf_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
}

TouchGesture display_get_gesture(void) {
    TouchGesture g = g_last_gesture;
    g_last_gesture  = GESTURE_NONE;
    return g;
}
