#pragma once

/* ---------------------------------------------------------------
   Waveshare ESP32-S3-Touch-AMOLED-1.75 (product 31261)
   Display: CO5300 QSPI, 466×466  |  Touch: CST9217 I2C
   Source: waveshareteam/ESP32-S3-Touch-AMOLED-1.75 pin_config.h
   --------------------------------------------------------------- */

/* Display: CO5300, QSPI */
#define LCD_WIDTH   466
#define LCD_HEIGHT  466

#define LCD_CS    12
#define LCD_SCLK  38
#define LCD_SDA0   4   /* SDIO0 */
#define LCD_SDA1   5   /* SDIO1 */
#define LCD_SDA2   6   /* SDIO2 */
#define LCD_SDA3   7   /* SDIO3 */
#define LCD_RST   39

/* Touch: CST9217, I2C */
#define TOUCH_I2C_ADDR 0x5A
#define TOUCH_SDA  15
#define TOUCH_SCL  14
#define TOUCH_RST  40
#define TOUCH_INT  11

/* AXP2101 PMIC — shares I2C bus with CST9217 (SDA=15, SCL=14) */
#define AXP2101_I2C_ADDR  0x34

/* Application */
#define SOLAR_MAX_W       6000      /* arc full-scale */
#define POLL_INTERVAL_MS  5000      /* Fronius API poll period */
#define API_TIMEOUT_MS    4000      /* HTTP timeout */
#define FAIL_THRESHOLD    3         /* consecutive failures before "no data" */

/* WiFi AP name shown when no credentials are stored */
#define WIFI_AP_NAME "Fronius-Monitor"

/* Timezone for NTP clock (seconds east of UTC) */
#define TZ_OFFSET_SEC   3600   /* UTC+1 CET  */
#define DST_OFFSET_SEC  3600   /* +1 h CEST  */
