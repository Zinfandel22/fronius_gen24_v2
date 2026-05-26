#include "display_driver.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

/* ---------------------------------------------------------------
   RM67162 display via QSPI
   --------------------------------------------------------------- */
static Arduino_DataBus *g_bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCLK, LCD_SDA0, LCD_SDA1, LCD_SDA2, LCD_SDA3);

static Arduino_GFX *g_gfx = new Arduino_RM67162(g_bus, LCD_RST);

/* ---------------------------------------------------------------
   LVGL display buffers — allocated from PSRAM
   One-tenth of the screen each; double-buffered for throughput.
   --------------------------------------------------------------- */
static lv_disp_draw_buf_t g_draw_buf;
static lv_color_t *g_buf1 = nullptr;
static lv_color_t *g_buf2 = nullptr;

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    /* draw16bitRGBBitmap accepts RGB565 which matches LV_COLOR_DEPTH 16.
       If colours look byte-swapped, set LV_COLOR_16_SWAP 1 in lv_conf.h. */
    g_gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(drv);
}

/* ---------------------------------------------------------------
   CST816S touch controller via I2C
   --------------------------------------------------------------- */
static volatile TouchGesture g_last_gesture = GESTURE_NONE;

static void cst816_reset(void) {
    digitalWrite(TOUCH_RST, LOW);
    delay(10);
    digitalWrite(TOUCH_RST, HIGH);
    delay(50);
}

/* Read 6 registers starting at 0x00: gesture, fingers, XH, XL, YH, YL */
static bool cst816_read(uint16_t *x, uint16_t *y, TouchGesture *gesture) {
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    Wire.write(0x00);
    if (Wire.endTransmission(false) != 0) return false;

    Wire.requestFrom((uint8_t)TOUCH_I2C_ADDR, (uint8_t)6);
    if (Wire.available() < 6) return false;

    uint8_t gest    = Wire.read();
    uint8_t fingers = Wire.read();
    uint8_t xh      = Wire.read();
    uint8_t xl      = Wire.read();
    uint8_t yh      = Wire.read();
    uint8_t yl      = Wire.read();

    *gesture = static_cast<TouchGesture>(gest);
    *x = ((uint16_t)(xh & 0x0F) << 8) | xl;
    *y = ((uint16_t)(yh & 0x0F) << 8) | yl;
    return fingers > 0;
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    uint16_t x, y;
    TouchGesture gest;

    if (cst816_read(&x, &y, &gest)) {
        data->point.x = (lv_coord_t)x;
        data->point.y = (lv_coord_t)y;
        data->state   = LV_INDEV_STATE_PR;
        if (gest != GESTURE_NONE) {
            g_last_gesture = gest;
        }
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

/* ---------------------------------------------------------------
   Public API
   --------------------------------------------------------------- */
void display_driver_init(void) {
    /* Touch reset & I2C */
    pinMode(TOUCH_RST, OUTPUT);
    cst816_reset();
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    /* Display */
    g_gfx->begin();
    g_gfx->fillScreen(BLACK);

    /* LVGL init */
    lv_init();

    /* Allocate draw buffers from PSRAM */
    const size_t buf_px = LCD_WIDTH * (LCD_HEIGHT / 10);
    g_buf1 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    g_buf2 = (lv_color_t *)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_disp_draw_buf_init(&g_draw_buf, g_buf1, g_buf2, buf_px);

    /* Register display driver */
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res   = LCD_WIDTH;
    disp_drv.ver_res   = LCD_HEIGHT;
    disp_drv.flush_cb  = lvgl_flush_cb;
    disp_drv.draw_buf  = &g_draw_buf;
    lv_disp_drv_register(&disp_drv);

    /* Register touch input device */
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);
}

TouchGesture display_get_gesture(void) {
    TouchGesture g = g_last_gesture;
    g_last_gesture  = GESTURE_NONE;
    return g;
}
