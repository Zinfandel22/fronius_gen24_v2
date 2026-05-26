#pragma once

/* ---------------------------------------------------------------
   Waveshare ESP32-S3 AMOLED 1.75" pin definitions
   Source: Waveshare wiki / schematic for ESP32-S3-LCD-1.75
   --------------------------------------------------------------- */

/* Display: RM67162, QSPI */
#define LCD_WIDTH   368
#define LCD_HEIGHT  448

#define LCD_CS    12
#define LCD_SCLK  10
#define LCD_SDA0  11   /* D0 / MOSI */
#define LCD_SDA1  13   /* D1 */
#define LCD_SDA2   9   /* D2 */
#define LCD_SDA3   8   /* D3 */
#define LCD_RST   17
#define LCD_TE    18   /* tear-effect, optional */

/* Touch: CST816S, I2C */
#define TOUCH_I2C_ADDR 0x15
#define TOUCH_SDA   6
#define TOUCH_SCL   7
#define TOUCH_RST   4
#define TOUCH_INT   5

/* Application */
#define SOLAR_MAX_W       6600      /* arc full-scale */
#define POLL_INTERVAL_MS  5000      /* Fronius API poll period */
#define API_TIMEOUT_MS    4000      /* HTTP timeout */
#define FAIL_THRESHOLD    3         /* consecutive failures before "no data" */

/* WiFi AP name shown when no credentials are stored */
#define WIFI_AP_NAME "Fronius-Monitor"
