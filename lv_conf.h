/**
 * lv_conf.h — LVGL v8 configuration for Fronius Gen24 Monitor
 * Waveshare ESP32-S3 AMOLED 1.75" (368×448, RM67162, 8 MB OPI PSRAM)
 */

#if 1  /* must be 1 to enable this file */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR
 *====================*/
#define LV_COLOR_DEPTH 16
/* If colours look wrong on the display, try flipping this to 1 */
#define LV_COLOR_16_SWAP 0

/*====================
   MEMORY
 *====================*/
/* Use standard malloc/free (sourced from PSRAM on ESP32-S3 with heap_caps) */
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC   malloc
#define LV_MEM_CUSTOM_FREE    free
#define LV_MEM_CUSTOM_REALLOC realloc

/*====================
   HAL / TICK
 *====================*/
/* LVGL uses Arduino millis() automatically — no lv_tick_inc() calls needed */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE   "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR ((uint32_t)millis())

#define LV_DPI_DEF 130

/*====================
   DRAW
 *====================*/
#define LV_DRAW_COMPLEX 1
#define LV_SHADOW_CACHE_SIZE 0
#define LV_CIRCLE_CACHE_SIZE 4

/*====================
   GPU — all off
 *====================*/
#define LV_USE_GPU_STM32_DMA2D  0
#define LV_USE_GPU_SWM341_DMACC 0
#define LV_USE_GPU_NXP_PXP      0
#define LV_USE_GPU_NXP_VG_LITE  0
#define LV_USE_GPU_SDL          0

/*====================
   LOGGING
 *====================*/
#define LV_USE_LOG 0

/*====================
   ASSERTS
 *====================*/
#define LV_USE_ASSERT_NULL   1
#define LV_USE_ASSERT_MALLOC 1
#define LV_USE_ASSERT_OBJ    0
#define LV_USE_ASSERT_STYLE  0

/*====================
   COMPILER
 *====================*/
#define LV_ATTRIBUTE_FAST_MEM
#define LV_ATTRIBUTE_LARGE_CONST
#define LV_EXPORT_CONST_INT(int_value) struct _silence_gcc_warning

/*====================
   FONTS
 *====================*/
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1

/* Disable everything else to save flash */
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0
#define LV_FONT_UNSCII_8  0
#define LV_FONT_UNSCII_16 0

#define LV_FONT_CUSTOM_DECLARE
#define LV_FONT_DEFAULT &lv_font_montserrat_16

/*====================
   TEXT
 *====================*/
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_TXT_BREAK_CHARS " ,.;:-_"
#define LV_TXT_LINE_BREAK_LONG_LEN 0
#define LV_TXT_COLOR_CMD "#"
#define LV_USE_BIDI 0
#define LV_USE_ARABIC_PERSIAN_CHARS 0

/*====================
   WIDGETS — only what we use
 *====================*/
#define LV_USE_ARC   1
#define LV_USE_BAR   0
#define LV_USE_BTN   1
#define LV_USE_LABEL 1
#define LV_LABEL_TEXT_SELECTION 0
#define LV_LABEL_LONG_TXT_HINT  1
#define LV_USE_LINE  0

/* All unused widgets off */
#define LV_USE_ANIMIMG    0
#define LV_USE_CALENDAR   0
#define LV_USE_CHART      0
#define LV_USE_CHECKBOX   0
#define LV_USE_COLORWHEEL 0
#define LV_USE_DROPDOWN   0
#define LV_USE_IMG        0
#define LV_USE_IMGBTN     0
#define LV_USE_KEYBOARD   0
#define LV_USE_LED        0
#define LV_USE_LIST       0
#define LV_USE_MENU       0
#define LV_USE_METER      0
#define LV_USE_MSGBOX     0
#define LV_USE_ROLLER     0
#define LV_USE_SLIDER     0
#define LV_USE_SPAN       0
#define LV_USE_SPINBOX    0
#define LV_USE_SPINNER    0
#define LV_USE_SWITCH     0
#define LV_USE_TABLE      0
#define LV_USE_TABVIEW    0
#define LV_USE_TEXTAREA   0
#define LV_USE_TILEVIEW   0
#define LV_USE_WIN        0

/*====================
   THEMES
 *====================*/
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 1
#define LV_THEME_DEFAULT_GROW 0
#define LV_THEME_DEFAULT_TRANSITION_TIME 80
#define LV_USE_THEME_BASIC 0
#define LV_USE_THEME_MONO  0

/*====================
   LAYOUT
 *====================*/
#define LV_USE_FLEX 0
#define LV_USE_GRID 0

/*====================
   3RD PARTY — all off
 *====================*/
#define LV_USE_FS_STDIO  0
#define LV_USE_FS_POSIX  0
#define LV_USE_FS_WIN32  0
#define LV_USE_FS_FATFS  0
#define LV_USE_PNG    0
#define LV_USE_BMP    0
#define LV_USE_SJPG   0
#define LV_USE_GIF    0
#define LV_USE_QRCODE 0
#define LV_USE_RLOTTIE 0
#define LV_USE_FFMPEG  0

/*====================
   OTHERS
 *====================*/
#define LV_USE_SNAPSHOT        0
#define LV_USE_MONKEY          0
#define LV_USE_GRIDNAV         0
#define LV_USE_FRAGMENT        0
#define LV_USE_IMGFONT         0
#define LV_USE_GPU_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR     0
#define LV_USE_REFR_DEBUG      0
#define LV_BUILD_EXAMPLES      0
#define LV_USE_DEMO_WIDGETS    0
#define LV_USE_DEMO_KEYPAD_AND_ENCODER 0
#define LV_USE_DEMO_BENCHMARK  0
#define LV_USE_DEMO_STRESS     0
#define LV_USE_DEMO_MUSIC      0

#endif /* LV_CONF_H */
#endif /* Set to 1 */
