#include "ui.h"
#include "config.h"
#include <lvgl.h>
#include <stdio.h>
#include <math.h>

/* ---------------------------------------------------------------
   Colour palette
   --------------------------------------------------------------- */
#define COL_BG          lv_color_hex(0x000000)
#define COL_ARC_BG      lv_color_hex(0x1C2833)
#define COL_SOLAR       lv_color_hex(0x2ECC71)
#define COL_SOLAR_LBL   lv_color_hex(0xF4D03F)
#define COL_CONSUME     lv_color_hex(0x5DADE2)
#define COL_BAT_FULL    lv_color_hex(0x5DADE2)   /* blue  — full / idle   */
#define COL_BAT_CHARGE  lv_color_hex(0x2ECC71)   /* green — charging      */
#define COL_BAT_DISC    lv_color_hex(0xE74C3C)   /* red   — discharging   */
#define COL_GRID_NEUT   lv_color_hex(0x7F8C8D)
#define COL_GRID_IMPORT lv_color_hex(0x2ECC71)   /* green — importing     */
#define COL_GRID_EXPORT lv_color_hex(0xE74C3C)   /* red   — exporting     */
#define COL_NO_DATA     lv_color_hex(0xE74C3C)

/* ---------------------------------------------------------------
   Main screen widgets
   --------------------------------------------------------------- */
static lv_obj_t *g_scr_main    = nullptr;
static lv_obj_t *g_arc_solar   = nullptr;
static lv_obj_t *g_lbl_solar   = nullptr;
static lv_obj_t *g_lbl_consume = nullptr;
static lv_obj_t *g_lbl_soc     = nullptr;
static lv_obj_t *g_lbl_grid    = nullptr;
static lv_obj_t *g_lbl_nodata  = nullptr;

static ScreenId g_current = SCREEN_MAIN;

/* ---------------------------------------------------------------
   Main screen builder
   --------------------------------------------------------------- */
static void build_main_screen(void) {
    g_scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_main, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr_main, LV_OPA_COVER, 0);

    /* --- Solar arc: 270° sweep, gap at bottom --- */
    const int32_t ARC_SIZE = LCD_WIDTH - 8;
    const int32_t ARC_Y    = (LCD_HEIGHT - ARC_SIZE) / 2;

    g_arc_solar = lv_arc_create(g_scr_main);
    lv_obj_set_size(g_arc_solar, ARC_SIZE, ARC_SIZE);
    lv_obj_align(g_arc_solar, LV_ALIGN_TOP_MID, 0, ARC_Y);

    lv_arc_set_rotation(g_arc_solar, 135);
    lv_arc_set_bg_angles(g_arc_solar, 0, 270);
    lv_arc_set_range(g_arc_solar, 0, SOLAR_MAX_W);
    lv_arc_set_value(g_arc_solar, 0);

    lv_obj_remove_style(g_arc_solar, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g_arc_solar, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(g_arc_solar, COL_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_arc_solar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_arc_solar, 0, LV_PART_MAIN);

    lv_obj_set_style_arc_color(g_arc_solar, COL_SOLAR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_INDICATOR);

    /* --- Solar watts (top, inside arc) --- */
    g_lbl_solar = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_solar, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_solar, COL_SOLAR_LBL, 0);
    lv_label_set_text(g_lbl_solar, "-- W");
    lv_obj_align(g_lbl_solar, LV_ALIGN_TOP_MID, 0, ARC_Y + 52);

    /* --- Consumption (centre, shifted up) --- */
    g_lbl_consume = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_consume, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_consume, COL_CONSUME, 0);
    lv_label_set_text(g_lbl_consume, "Use: -- W");
    lv_obj_align(g_lbl_consume, LV_ALIGN_CENTER, 0, -58);

    /* --- Battery SOC (centre) --- */
    g_lbl_soc = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_soc, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_soc, COL_BAT_FULL, 0);
    lv_label_set_text(g_lbl_soc, "Bat: --%");
    lv_obj_align(g_lbl_soc, LV_ALIGN_CENTER, 0, 0);

    /* --- Grid (centre, shifted down) --- */
    g_lbl_grid = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_grid, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_NEUT, 0);
    lv_label_set_text(g_lbl_grid, "Grid: -- W");
    lv_obj_align(g_lbl_grid, LV_ALIGN_CENTER, 0, 58);

    /* --- "No data" — bottom of screen, hidden until API fails --- */
    g_lbl_nodata = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_nodata, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_nodata, COL_NO_DATA, 0);
    lv_label_set_text(g_lbl_nodata, "No data");
    lv_obj_align(g_lbl_nodata, LV_ALIGN_BOTTOM_MID, 0, -28);
    lv_obj_add_flag(g_lbl_nodata, LV_OBJ_FLAG_HIDDEN);
}

/* ---------------------------------------------------------------
   Public API
   --------------------------------------------------------------- */
void ui_init(void) {
    build_main_screen();
    lv_scr_load(g_scr_main);
}

void ui_update(const PowerData *data) {
    if (!data->valid) {
        lv_obj_clear_flag(g_lbl_nodata, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_add_flag(g_lbl_nodata, LV_OBJ_FLAG_HIDDEN);

    char buf[32];

    /* Solar arc + label */
    int32_t solar_clamped = (int32_t)fminf(data->solar_w, (float)SOLAR_MAX_W);
    lv_arc_set_value(g_arc_solar, solar_clamped);

    if (data->solar_w < 1.0f) {
        lv_label_set_text(g_lbl_solar, "-- W");
    } else {
        snprintf(buf, sizeof(buf), "%.0f W", data->solar_w);
        lv_label_set_text(g_lbl_solar, buf);
    }

    /* Consumption */
    snprintf(buf, sizeof(buf), "Use: %.0f W", data->consumption_w);
    lv_label_set_text(g_lbl_consume, buf);

    /* Battery SOC — colour reflects charge state */
    if (data->soc_pct < 0.0f) {
        lv_obj_set_style_text_color(g_lbl_soc, COL_BAT_FULL, 0);
        lv_label_set_text(g_lbl_soc, "Bat: --%");
    } else {
        lv_color_t bat_col;
        if (data->battery_w > 20.0f) {
            bat_col = COL_BAT_DISC;          /* discharging */
        } else if (data->battery_w < -20.0f) {
            bat_col = COL_BAT_CHARGE;        /* charging    */
        } else {
            bat_col = COL_BAT_FULL;          /* full / idle */
        }
        lv_obj_set_style_text_color(g_lbl_soc, bat_col, 0);
        snprintf(buf, sizeof(buf), "Bat: %.0f%%", data->soc_pct);
        lv_label_set_text(g_lbl_soc, buf);
    }

    /* Grid — colour reflects direction, plain ASCII only */
    float g = data->grid_w;
    if (fabsf(g) < 20.0f) {
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_NEUT, 0);
        lv_label_set_text(g_lbl_grid, "Grid: ~0 W");
    } else if (g < 0.0f) {
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_IMPORT, 0);
        snprintf(buf, sizeof(buf), "Grid: %.0f W", fabsf(g));
        lv_label_set_text(g_lbl_grid, buf);
    } else {
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_EXPORT, 0);
        snprintf(buf, sizeof(buf), "Grid: %.0f W", g);
        lv_label_set_text(g_lbl_grid, buf);
    }
}

void ui_switch_screen(ScreenId id) {
    if (id == g_current || id >= SCREEN_COUNT) return;
    g_current = id;
}

ScreenId ui_current_screen(void) {
    return g_current;
}
