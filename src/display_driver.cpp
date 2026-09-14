#include "display_driver.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <TouchDrvCSTXXX.hpp>
#include <lvgl.h>

/* ---------------------------------------------------------------
   CO5300 display via QSPI
   col_offset1=6 centres the 466-wide frame in the 480-wide panel.
   --------------------------------------------------------------- */
static Arduino_DataBus *g_bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDA0, LCD_SDA1, LCD_SDA2, LCD_SDA3);

static Arduino_CO5300 *g_gfx = new Arduino_CO5300(
    g_bus, LCD_RST, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
static TouchDrvCST92xx g_touch;

/* ---------------------------------------------------------------
   LVGL display buffers — allocated from PSRAM
   --------------------------------------------------------------- */
static uint8_t *g_buf1 = nullptr;
static uint8_t *g_buf2 = nullptr;
static uint8_t *g_rot_buf = nullptr;

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;

    if (LCD_ROTATION == 1 &&
        area->x1 == 0 && area->y1 == 0 &&
        area->x2 == LCD_WIDTH - 1 && area->y2 == LCD_HEIGHT - 1) {
        const uint16_t *source = (const uint16_t *)px_map;
        uint16_t *rotated = (uint16_t *)g_rot_buf;

        for (uint32_t y = 0; y < LCD_HEIGHT; y++) {
            for (uint32_t x = 0; x < LCD_WIDTH; x++) {
                rotated[x * LCD_WIDTH + (LCD_WIDTH - 1 - y)] =
                    source[y * LCD_WIDTH + x];
            }
        }
        g_gfx->draw16bitRGBBitmap(0, 0, rotated, LCD_WIDTH, LCD_HEIGHT);
    } else {
        g_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    }
    lv_display_flush_ready(disp);
}

/* ---------------------------------------------------------------
   CST9217 touch controller via I2C
   Register 0xD000 returns 15 bytes (per SensorLib TouchDrvCST92xx):
     [0]     = first touch: id (>> 4), event (& 0x0F) — 0x06=pressed, 0x00=released
     [1]     = first touch X high 8 bits (of 12-bit value)
     [2]     = first touch Y high 8 bits
     [3]     = first touch: X low 4 bits (hi nibble), Y low 4 bits (lo nibble)
     [4]     = first touch extra byte
     [5]     = finger count (& 0x7F)
     [6]     = 0xAB ACK marker — must be present for a valid read
     [7..11] = second touch point (same 5-byte format)
     [12..14]= padding
   After every read, MUST write back {0xD0, 0x00, 0xAB} or the chip
   refuses the next transaction (root cause of ESP_ERR_INVALID_STATE).
   --------------------------------------------------------------- */
static volatile TouchGesture g_last_gesture = GESTURE_NONE;

static void touch_reset(void) {
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
}

/* ---------------------------------------------------------------
   I2C bus recovery
   When a slave holds SDA low mid-byte the ESP32 I2C peripheral
   reports ESP_ERR_INVALID_STATE on every subsequent transaction.
   The fix is the SMBUS / NXP "9-clock" recovery: drop Wire, bit-
   bang up to 9 SCL pulses until SDA is released, send a STOP, then
   reinitialise Wire.
   --------------------------------------------------------------- */
static void i2c_bus_recover(void) {
    /* 1. Hardware-reset the touch chip.
     *    The 9-clock procedure frees the ESP32 peripheral but the CST9217
     *    stays in its mid-read state machine until physically reset. */
    digitalWrite(TOUCH_RST, LOW);
    delayMicroseconds(500);
    digitalWrite(TOUCH_RST, HIGH);

    /* 2. Release Wire so we can bit-bang the bus lines directly. */
    Wire.end();

    /* 3. Drive both lines high. */
    pinMode(TOUCH_SCL, OUTPUT);
    pinMode(TOUCH_SDA, OUTPUT);
    digitalWrite(TOUCH_SCL, HIGH);
    digitalWrite(TOUCH_SDA, HIGH);
    delayMicroseconds(20);

    /* 4. Up to 9 SCL pulses — clocks out any byte the slave is mid-sending. */
    for (int i = 0; i < 9; i++) {
        digitalWrite(TOUCH_SCL, LOW);
        delayMicroseconds(5);
        digitalWrite(TOUCH_SCL, HIGH);
        delayMicroseconds(5);
        pinMode(TOUCH_SDA, INPUT_PULLUP);
        bool free = digitalRead(TOUCH_SDA);
        pinMode(TOUCH_SDA, OUTPUT);
        digitalWrite(TOUCH_SDA, HIGH);
        if (free) break;
    }

    /* 5. STOP condition: SDA low → high while SCL is high. */
    digitalWrite(TOUCH_SDA, LOW);
    delayMicroseconds(5);
    digitalWrite(TOUCH_SCL, HIGH);
    delayMicroseconds(5);
    digitalWrite(TOUCH_SDA, HIGH);
    delayMicroseconds(10);

    /* 6. Release pins to inputs so Wire can claim them cleanly. */
    pinMode(TOUCH_SCL, INPUT);
    pinMode(TOUCH_SDA, INPUT);

    /* 7. Wait for the CST9217 to finish its reset sequence before talking to it. */
    delay(30);

    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    Wire.setClock(100000);
}

static bool cst9217_read(uint16_t *x, uint16_t *y, uint8_t *num_points) {
    int16_t raw_x = 0;
    int16_t raw_y = 0;
    *num_points = g_touch.getPoint(&raw_x, &raw_y, 1);
    if (*num_points == 0 || raw_x < 0 || raw_y < 0) return false;

    *x = (uint16_t)raw_x;
    *y = (uint16_t)raw_y;
    if (*x >= LCD_WIDTH || *y >= LCD_HEIGHT) {
        *num_points = 0;
        return false;
    }

    /* Touch coordinates are reported in the panel's unrotated orientation. */
    if (LCD_ROTATION == 1) {
        uint16_t raw_x = *x;
        *x = *y;
        *y = LCD_WIDTH - 1 - raw_x;
    }
    return true;
}

/* ---------------------------------------------------------------
   Software swipe detection
   Tracks start/end positions across one finger-down → finger-up cycle.
   --------------------------------------------------------------- */
#define SWIPE_MIN_PX      40   /* minimum travel to count as a swipe */
#define SWIPE_MIN_SAMPLES  3   /* minimum consecutive pressed reads (~90 ms at 30 ms poll)
                                  — filters out I2C glitch–induced phantom releases */

static lv_indev_state_t g_prev_touch    = LV_INDEV_STATE_RELEASED;
static int16_t          g_start_x       = 0, g_start_y = 0;
static int16_t          g_last_x        = 0, g_last_y  = 0;
static int16_t          g_press_samples = 0;

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
            g_start_x       = (int16_t)x;
            g_start_y       = (int16_t)y;
            g_press_samples = 0;
        }
        g_last_x = (int16_t)x;
        g_last_y = (int16_t)y;
        g_press_samples++;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        if (g_prev_touch == LV_INDEV_STATE_PRESSED &&
            g_press_samples >= SWIPE_MIN_SAMPLES) {
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
        g_press_samples = 0;
    }
    g_prev_touch = data->state;
}

/* ---------------------------------------------------------------
   Public API
   --------------------------------------------------------------- */
void display_driver_init(void) {
    g_touch.setPins(TOUCH_RST, TOUCH_INT);
    if (!g_touch.begin(Wire, TOUCH_I2C_ADDR, TOUCH_SDA, TOUCH_SCL)) {
        Serial.println("[touch] CST9217 initialization failed");
    } else {
        Serial.println("[touch] CST9217 initialized");
    }

    g_gfx->begin();
    g_gfx->fillScreen(0x0000);  /* RGB565 black */

    lv_init();
    lv_tick_set_cb((lv_tick_get_cb_t)millis);

    const size_t buf_px    = LCD_WIDTH * LCD_HEIGHT;
    const size_t buf_bytes = buf_px * sizeof(lv_color_t);
    g_buf1 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    g_buf2 = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);
    g_rot_buf = (uint8_t *)heap_caps_malloc(buf_bytes, MALLOC_CAP_SPIRAM);

    lv_display_t *disp = lv_display_create(LCD_WIDTH, LCD_HEIGHT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, g_buf1, g_buf2, buf_bytes,
                           LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
}

void display_set_brightness(uint8_t brightness) {
    g_gfx->setBrightness(brightness);
}

TouchGesture display_get_gesture(void) {
    TouchGesture g = g_last_gesture;
    g_last_gesture  = GESTURE_NONE;
    return g;
}
