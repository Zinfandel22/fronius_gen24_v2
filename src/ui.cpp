#include "ui.h"
#include "config.h"
#include <lvgl.h>
#include <stdio.h>
#include <math.h>

/* ---------------------------------------------------------------
   Colour palette
   --------------------------------------------------------------- */
#define COL_BG          lv_color_hex(0x000000)   /* AMOLED black     */
#define COL_ARC_BG      lv_color_hex(0x1C2833)   /* dark arc track   */
#define COL_SOLAR       lv_color_hex(0x2ECC71)   /* green            */
#define COL_SOLAR_LBL   lv_color_hex(0xF4D03F)   /* yellow           */
#define COL_CONSUME     lv_color_hex(0x5DADE2)   /* blue             */
#define COL_BATTERY     lv_color_hex(0x2ECC71)   /* green            */
#define COL_GRID_BAR    lv_color_hex(0x1A2530)   /* very dark        */
#define COL_GRID_NEUT   lv_color_hex(0x7F8C8D)   /* grey             */
#define COL_GRID_IMPORT lv_color_hex(0xE74C3C)   /* red              */
#define COL_GRID_EXPORT lv_color_hex(0x2ECC71)   /* green            */
#define COL_NO_DATA     lv_color_hex(0xE74C3C)   /* red              */

/* ---------------------------------------------------------------
   Main screen widgets
   --------------------------------------------------------------- */
static lv_obj_t *g_scr_main   = nullptr;
static lv_obj_t *g_arc_solar  = nullptr;
static lv_obj_t *g_lbl_solar  = nullptr;
static lv_obj_t *g_lbl_consume = nullptr;
static lv_obj_t *g_lbl_soc    = nullptr;
static lv_obj_t *g_lbl_grid   = nullptr;
static lv_obj_t *g_lbl_nodata = nullptr;

static ScreenId g_current = SCREEN_MAIN;

/* ---------------------------------------------------------------
   Main screen builder
   --------------------------------------------------------------- */
static void build_main_screen(void) {
    g_scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_main, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr_main, LV_OPA_COVER, 0);

    /* --- Solar arc ---
       270° sweep, gap at the bottom.
       lv_arc rotation=135 puts the 0° start at the lower-left;
       bg_angles 0→270 sweeps clockwise to the lower-right.
       The arc size is capped to the display width so it fills
       edge-to-edge horizontally. */
    const lv_coord_t ARC_SIZE = LCD_WIDTH - 8;
    const lv_coord_t GRID_H   = 56;
    const lv_coord_t ARC_Y    = (LCD_HEIGHT - GRID_H - ARC_SIZE) / 2;

    g_arc_solar = lv_arc_create(g_scr_main);
    lv_obj_set_size(g_arc_solar, ARC_SIZE, ARC_SIZE);
    lv_obj_align(g_arc_solar, LV_ALIGN_TOP_MID, 0, ARC_Y);

    lv_arc_set_rotation(g_arc_solar, 135);
    lv_arc_set_bg_angles(g_arc_solar, 0, 270);
    lv_arc_set_range(g_arc_solar, 0, SOLAR_MAX_W);
    lv_arc_set_value(g_arc_solar, 0);

    /* No interactive knob — display only */
    lv_obj_remove_style(g_arc_solar, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(g_arc_solar, LV_OBJ_FLAG_CLICKABLE);

    /* Arc track (background) */
    lv_obj_set_style_arc_color(g_arc_solar, COL_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_arc_solar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_arc_solar, 0, LV_PART_MAIN);

    /* Arc indicator (value) */
    lv_obj_set_style_arc_color(g_arc_solar, COL_SOLAR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_INDICATOR);

    /* --- Solar watts label (top-centre inside arc) --- */
    g_lbl_solar = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_solar, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(g_lbl_solar, COL_SOLAR_LBL, 0);
    lv_label_set_text(g_lbl_solar, "-- W");
    lv_obj_align(g_lbl_solar, LV_ALIGN_TOP_MID, 0, ARC_Y + 60);

    /* --- Consumption label (centre) --- */
    g_lbl_consume = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_consume, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_lbl_consume, COL_CONSUME, 0);
    lv_label_set_text(g_lbl_consume, "Use: -- W");
    lv_obj_align(g_lbl_consume, LV_ALIGN_CENTER, 0, -18);

    /* --- Battery SOC label (below consumption) --- */
    g_lbl_soc = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_soc, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_lbl_soc, COL_BATTERY, 0);
    lv_label_set_text(g_lbl_soc, "Bat: --%");
    lv_obj_align(g_lbl_soc, LV_ALIGN_CENTER, 0, 18);

    /* --- Grid bar (bottom strip) --- */
    lv_obj_t *grid_bar = lv_obj_create(g_scr_main);
    lv_obj_set_size(grid_bar, LCD_WIDTH, GRID_H);
    lv_obj_align(grid_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(grid_bar, COL_GRID_BAR, 0);
    lv_obj_set_style_bg_opa(grid_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid_bar, 0, 0);
    lv_obj_set_style_radius(grid_bar, 0, 0);
    lv_obj_set_style_pad_all(grid_bar, 0, 0);
    lv_obj_clear_flag(grid_bar, LV_OBJ_FLAG_SCROLLABLE);

    g_lbl_grid = lv_label_create(grid_bar);
    lv_obj_set_style_text_font(g_lbl_grid, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_NEUT, 0);
    lv_label_set_text(g_lbl_grid, "Grid: -- W");
    lv_obj_center(g_lbl_grid);

    /* --- "No data" overlay (hidden until API fails) --- */
    g_lbl_nodata = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_nodata, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(g_lbl_nodata, COL_NO_DATA, 0);
    lv_label_set_text(g_lbl_nodata, "No data");
    lv_obj_center(g_lbl_nodata);
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

    char buf[48];

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

    /* Battery SOC */
    if (data->soc_pct < 0.0f) {
        lv_label_set_text(g_lbl_soc, "Bat: --%");
    } else {
        snprintf(buf, sizeof(buf), "Bat: %.0f%%", data->soc_pct);
        lv_label_set_text(g_lbl_soc, buf);
    }

    /* Grid — colour reflects direction */
    float g = data->grid_w;
    if (fabsf(g) < 20.0f) {
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_NEUT, 0);
        lv_label_set_text(g_lbl_grid, "Grid: ~0 W");
    } else if (g < 0.0f) {
        /* Importing from grid */
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_IMPORT, 0);
        snprintf(buf, sizeof(buf), "Grid: +%.0f W  \xe2\x96\xb2", fabsf(g));
        lv_label_set_text(g_lbl_grid, buf);
    } else {
        /* Exporting to grid */
        lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_EXPORT, 0);
        snprintf(buf, sizeof(buf), "Grid: \xe2\x88\x92%.0f W  \xe2\x96\xbc", g);
        lv_label_set_text(g_lbl_grid, buf);
    }
}

void ui_switch_screen(ScreenId id) {
    if (id == g_current || id >= SCREEN_COUNT) return;
    /* Placeholder — add lv_scr_load_anim() here when new screens are added */
    g_current = id;
}

ScreenId ui_current_screen(void) {
    return g_current;
}
