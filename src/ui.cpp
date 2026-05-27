#include "ui.h"
#include "config.h"
#include "fronius_logo.h"
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
#define COL_CLOCK_HAND  lv_color_hex(0xFFFFFF)
#define COL_SEC_HAND    lv_color_hex(0xE67E22)
#define COL_HOUR_DOT    lv_color_hex(0x808080)

/* ---------------------------------------------------------------
   Main screen widgets
   --------------------------------------------------------------- */
static lv_obj_t *g_scr_main    = nullptr;
static lv_obj_t *g_arc_solar   = nullptr;
static lv_obj_t *g_lbl_solar    = nullptr;
static lv_obj_t *g_lbl_inverter = nullptr;
static lv_obj_t *g_lbl_consume  = nullptr;
static lv_obj_t *g_lbl_soc     = nullptr;
static lv_obj_t *g_lbl_grid    = nullptr;
static lv_obj_t *g_lbl_nodata  = nullptr;

/* ---------------------------------------------------------------
   Clock screen widgets
   --------------------------------------------------------------- */
static lv_obj_t *g_scr_clock   = nullptr;
static lv_obj_t *g_arc_soc     = nullptr;
static lv_obj_t *g_lbl_soc_clk = nullptr;
static lv_obj_t *g_line_hour   = nullptr;
static lv_obj_t *g_line_min    = nullptr;
static lv_obj_t *g_line_sec    = nullptr;
static lv_obj_t *g_pivot       = nullptr;

/* LVGL v9 stores a pointer to the points array, so these must be static */
static lv_point_precise_t g_pts_hour[2] = {{233, 233}, {233, 233}};
static lv_point_precise_t g_pts_min[2]  = {{233, 233}, {233, 233}};
static lv_point_precise_t g_pts_sec[2]  = {{233, 233}, {233, 233}};

/* Solar arc input watts (NVS "solar_input"); arc full scale = value × 1.1 */
static uint32_t g_solar_input_w = 6000;

static ScreenId g_current = SCREEN_MAIN;

/* ---------------------------------------------------------------
   Boot screen widgets
   --------------------------------------------------------------- */
static lv_obj_t *g_scr_boot        = nullptr;
static lv_obj_t *g_lbl_boot_status = nullptr;

/* ---------------------------------------------------------------
   Clock hand helper — updates tip point and refreshes the line
   --------------------------------------------------------------- */
static void update_hand(lv_obj_t *line, lv_point_precise_t pts[2],
                        int32_t len, float angle_deg) {
    float rad  = angle_deg * (float)M_PI / 180.0f;
    pts[1].x   = 233 + (int32_t)(len * sinf(rad));
    pts[1].y   = 233 - (int32_t)(len * cosf(rad));
    lv_line_set_points(line, pts, 2);
}

/* ---------------------------------------------------------------
   Boot screen timer callback — fires 4 s after WiFi IP is shown
   --------------------------------------------------------------- */
static void boot_timer_cb(lv_timer_t *t) {
    lv_timer_delete(t);
    lv_scr_load_anim(g_scr_main, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    /* g_current is already SCREEN_MAIN */
}

/* ---------------------------------------------------------------
   Main screen builder
   --------------------------------------------------------------- */
static void build_main_screen(void) {
    g_scr_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_main, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr_main, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_scr_main, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Solar arc: 270° sweep, gap at bottom --- */
    const int32_t ARC_SIZE = LCD_WIDTH - 8;
    const int32_t ARC_Y    = (LCD_HEIGHT - ARC_SIZE) / 2;

    g_arc_solar = lv_arc_create(g_scr_main);
    lv_obj_set_size(g_arc_solar, ARC_SIZE, ARC_SIZE);
    lv_obj_align(g_arc_solar, LV_ALIGN_TOP_MID, 0, ARC_Y);

    lv_arc_set_rotation(g_arc_solar, 135);
    lv_arc_set_bg_angles(g_arc_solar, 0, 270);
    lv_arc_set_value(g_arc_solar, 0);

    lv_obj_remove_style(g_arc_solar, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(g_arc_solar, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(g_arc_solar, COL_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_arc_solar, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_arc_solar, 0, LV_PART_MAIN);

    lv_obj_set_style_arc_color(g_arc_solar, COL_SOLAR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_arc_solar, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc_solar, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc_solar, false, LV_PART_MAIN);

    /* --- Solar watts (top, inside arc) --- */
    g_lbl_solar = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_solar, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_solar, COL_SOLAR_LBL, 0);
    lv_obj_set_width(g_lbl_solar, 220);
    lv_obj_set_style_text_align(g_lbl_solar, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_solar, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_solar, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_solar, "-- W");
    lv_obj_align(g_lbl_solar, LV_ALIGN_TOP_MID, 0, ARC_Y + 52);

    /* --- Inverter AC output (below PV label) --- */
    g_lbl_inverter = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_inverter, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(g_lbl_inverter, COL_SOLAR_LBL, 0);
    lv_obj_set_width(g_lbl_inverter, 220);
    lv_obj_set_style_text_align(g_lbl_inverter, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_inverter, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_inverter, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_inverter, "Inv: -- W");
    lv_obj_align(g_lbl_inverter, LV_ALIGN_TOP_MID, 0, ARC_Y + 52 + 50);

    /* --- Consumption (centre, shifted up) --- */
    g_lbl_consume = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_consume, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_consume, COL_CONSUME, 0);
    lv_obj_set_width(g_lbl_consume, 320);
    lv_obj_set_style_text_align(g_lbl_consume, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_consume, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_consume, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_consume, "Use: -- W");
    lv_obj_align(g_lbl_consume, LV_ALIGN_CENTER, 0, -58);

    /* --- Battery SOC (centre) --- */
    g_lbl_soc = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_soc, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_soc, COL_BAT_FULL, 0);
    lv_obj_set_width(g_lbl_soc, 280);
    lv_obj_set_style_text_align(g_lbl_soc, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_soc, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_soc, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_soc, "Bat: --%");
    lv_obj_align(g_lbl_soc, LV_ALIGN_CENTER, 0, 0);

    /* --- Grid (centre, shifted down) --- */
    g_lbl_grid = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_grid, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_grid, COL_GRID_NEUT, 0);
    lv_obj_set_width(g_lbl_grid, 320);
    lv_obj_set_style_text_align(g_lbl_grid, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_grid, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_grid, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_grid, "Grid: -- W");
    lv_obj_align(g_lbl_grid, LV_ALIGN_CENTER, 0, 58);

    /* --- "No data" — bottom of screen, blank until API fails --- */
    g_lbl_nodata = lv_label_create(g_scr_main);
    lv_obj_set_style_text_font(g_lbl_nodata, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(g_lbl_nodata, COL_NO_DATA, 0);
    lv_obj_set_size(g_lbl_nodata, 220, 50);          /* fixed size — bg always covers area */
    lv_obj_set_style_text_align(g_lbl_nodata, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_nodata, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_nodata, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_nodata, "");              /* empty = hidden; text swap avoids ghost */
    lv_obj_align(g_lbl_nodata, LV_ALIGN_BOTTOM_MID, 0, -28);
}

/* ---------------------------------------------------------------
   Clock screen builder
   --------------------------------------------------------------- */
static void build_clock_screen(void) {
    g_scr_clock = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_clock, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr_clock, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_scr_clock, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Battery SOC arc (same 270° geometry as solar arc) --- */
    const int32_t ARC_SIZE = LCD_WIDTH - 8;
    const int32_t ARC_Y    = (LCD_HEIGHT - ARC_SIZE) / 2;

    g_arc_soc = lv_arc_create(g_scr_clock);
    lv_obj_set_size(g_arc_soc, ARC_SIZE, ARC_SIZE);
    lv_obj_align(g_arc_soc, LV_ALIGN_TOP_MID, 0, ARC_Y);

    lv_arc_set_rotation(g_arc_soc, 135);
    lv_arc_set_bg_angles(g_arc_soc, 0, 270);
    lv_arc_set_range(g_arc_soc, 0, 100);
    lv_arc_set_value(g_arc_soc, 0);

    lv_obj_remove_style(g_arc_soc, NULL, LV_PART_KNOB);
    lv_obj_remove_flag(g_arc_soc, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_style_arc_color(g_arc_soc, COL_ARC_BG, LV_PART_MAIN);
    lv_obj_set_style_arc_width(g_arc_soc, 14, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_arc_soc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(g_arc_soc, 0, LV_PART_MAIN);

    lv_obj_set_style_arc_color(g_arc_soc, COL_BAT_FULL, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(g_arc_soc, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc_soc, false, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(g_arc_soc, false, LV_PART_MAIN);

    /* --- Hour marker dots (12 circles at radius 175 px from centre) --- */
    const int32_t CX = LCD_WIDTH  / 2;
    const int32_t CY = LCD_HEIGHT / 2;
    const int32_t MARKER_R = 175;
    const int32_t MARKER_D = 8;

    for (int h = 0; h < 12; h++) {
        float angle_rad = ((float)h * 30.0f - 90.0f) * (float)M_PI / 180.0f;
        int32_t mx = CX + (int32_t)(MARKER_R * cosf(angle_rad)) - MARKER_D / 2;
        int32_t my = CY + (int32_t)(MARKER_R * sinf(angle_rad)) - MARKER_D / 2;

        lv_obj_t *dot = lv_obj_create(g_scr_clock);
        lv_obj_set_size(dot, MARKER_D, MARKER_D);
        lv_obj_set_pos(dot, mx, my);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, COL_HOUR_DOT, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    }

    /* --- Clock hands --- */
    g_pts_hour[0] = {CX, CY};
    g_pts_hour[1] = {CX, CY};
    g_pts_min[0]  = {CX, CY};
    g_pts_min[1]  = {CX, CY};
    g_pts_sec[0]  = {CX, CY};
    g_pts_sec[1]  = {CX, CY};

    g_line_hour = lv_line_create(g_scr_clock);
    lv_line_set_points(g_line_hour, g_pts_hour, 2);
    lv_obj_set_style_line_width(g_line_hour, 6, 0);
    lv_obj_set_style_line_color(g_line_hour, COL_CLOCK_HAND, 0);
    lv_obj_set_style_line_rounded(g_line_hour, true, 0);

    g_line_min = lv_line_create(g_scr_clock);
    lv_line_set_points(g_line_min, g_pts_min, 2);
    lv_obj_set_style_line_width(g_line_min, 4, 0);
    lv_obj_set_style_line_color(g_line_min, COL_CLOCK_HAND, 0);
    lv_obj_set_style_line_rounded(g_line_min, true, 0);

    g_line_sec = lv_line_create(g_scr_clock);
    lv_line_set_points(g_line_sec, g_pts_sec, 2);
    lv_obj_set_style_line_width(g_line_sec, 2, 0);
    lv_obj_set_style_line_color(g_line_sec, COL_SEC_HAND, 0);
    lv_obj_set_style_line_rounded(g_line_sec, true, 0);

    /* --- Centre pivot dot --- */
    g_pivot = lv_obj_create(g_scr_clock);
    lv_obj_set_size(g_pivot, 10, 10);
    lv_obj_set_style_radius(g_pivot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_pivot, COL_CLOCK_HAND, 0);
    lv_obj_set_style_bg_opa(g_pivot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_pivot, 0, 0);
    lv_obj_align(g_pivot, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(g_pivot, LV_OBJ_FLAG_SCROLLABLE);

    /* --- SOC label (small, in arc gap at bottom) --- */
    g_lbl_soc_clk = lv_label_create(g_scr_clock);
    lv_obj_set_style_text_font(g_lbl_soc_clk, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_lbl_soc_clk, COL_BAT_FULL, 0);
    lv_obj_set_width(g_lbl_soc_clk, 80);
    lv_obj_set_style_text_align(g_lbl_soc_clk, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_color(g_lbl_soc_clk, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_lbl_soc_clk, LV_OPA_COVER, 0);
    lv_label_set_text(g_lbl_soc_clk, "--%");
    lv_obj_align(g_lbl_soc_clk, LV_ALIGN_BOTTOM_MID, 0, -10);
}

/* ---------------------------------------------------------------
   Boot screen builder
   --------------------------------------------------------------- */
static void build_boot_screen(void) {
    g_scr_boot = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(g_scr_boot, COL_BG, 0);
    lv_obj_set_style_bg_opa(g_scr_boot, LV_OPA_COVER, 0);
    lv_obj_clear_flag(g_scr_boot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *img = lv_image_create(g_scr_boot);
    lv_image_set_src(img, &fronius_logo);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 130);

    lv_obj_t *lbl_sub = lv_label_create(g_scr_boot);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_sub, COL_SOLAR_LBL, 0);
    lv_obj_set_width(lbl_sub, 340);
    lv_obj_set_style_text_align(lbl_sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_sub, "Gen24 Monitor");
    lv_obj_align(lbl_sub, LV_ALIGN_TOP_MID, 0, 270);

    g_lbl_boot_status = lv_label_create(g_scr_boot);
    lv_obj_set_style_text_font(g_lbl_boot_status, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(g_lbl_boot_status, COL_GRID_NEUT, 0);
    lv_obj_set_width(g_lbl_boot_status, 340);
    lv_obj_set_style_text_align(g_lbl_boot_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(g_lbl_boot_status, "Connecting...");
    lv_obj_align(g_lbl_boot_status, LV_ALIGN_TOP_MID, 0, 320);
}

/* ---------------------------------------------------------------
   Public API
   --------------------------------------------------------------- */
void ui_init(void) {
    build_main_screen();
    lv_arc_set_range(g_arc_solar, 0, (uint32_t)(g_solar_input_w * 1.1f));
    lv_arc_set_value(g_arc_solar, 0);

    build_clock_screen();
    build_boot_screen();
    lv_scr_load(g_scr_boot);
}

void ui_update(const PowerData *data) {
    if (!data->valid) {
        lv_label_set_text(g_lbl_nodata, "No data");
        lv_label_set_text(g_lbl_inverter, "Inv: -- W");
        return;
    }
    lv_label_set_text(g_lbl_nodata, "");

    char buf[32];

    /* Solar arc + label */
    int32_t solar_clamped = (int32_t)fminf(data->solar_w, g_solar_input_w * 1.1f);
    lv_arc_set_value(g_arc_solar, solar_clamped);
    lv_obj_invalidate(g_arc_solar);   /* force full bounding-box repaint */

    if (data->solar_w < 1.0f) {
        lv_label_set_text(g_lbl_solar, "-- W");
    } else {
        snprintf(buf, sizeof(buf), "%.0f W", data->solar_w);
        lv_label_set_text(g_lbl_solar, buf);
    }

    /* Inverter AC output */
    if (data->inverter_w < 1.0f) {
        lv_label_set_text(g_lbl_inverter, "Inv: -- W");
    } else {
        snprintf(buf, sizeof(buf), "Inv: %.0f W", data->inverter_w);
        lv_label_set_text(g_lbl_inverter, buf);
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

void ui_update_clock(const PowerData *data, const struct tm *t) {
    /* --- SOC arc + label --- */
    lv_color_t col = COL_BAT_FULL;
    if (data->valid && data->soc_pct >= 0.0f) {
        if (data->battery_w > 20.0f) {
            col = COL_BAT_DISC;
        } else if (data->battery_w < -20.0f) {
            col = COL_BAT_CHARGE;
        }
        int32_t soc = (int32_t)fminf(fmaxf(data->soc_pct, 0.0f), 100.0f);
        lv_arc_set_value(g_arc_soc, soc);
        lv_obj_invalidate(g_arc_soc);    /* force full bounding-box repaint */

        char buf[8];
        snprintf(buf, sizeof(buf), "%.0f%%", data->soc_pct);
        lv_label_set_text(g_lbl_soc_clk, buf);
    } else {
        lv_arc_set_value(g_arc_soc, 0);
        lv_obj_invalidate(g_arc_soc);
        lv_label_set_text(g_lbl_soc_clk, "--%");
    }
    lv_obj_set_style_arc_color(g_arc_soc, col, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(g_lbl_soc_clk, col, 0);

    /* --- Clock hands --- */
    int h = t->tm_hour;
    int m = t->tm_min;
    int s = t->tm_sec;

    float hour_deg   = (float)(h % 12) * 30.0f + (float)m * 0.5f;
    float minute_deg = (float)m * 6.0f + (float)s * 0.1f;
    float second_deg = (float)s * 6.0f;

    update_hand(g_line_hour, g_pts_hour, 90,  hour_deg);
    update_hand(g_line_min,  g_pts_min,  130, minute_deg);
    update_hand(g_line_sec,  g_pts_sec,  148, second_deg);
}

void ui_switch_screen(ScreenId id, bool forward) {
    if (id == g_current || id >= SCREEN_COUNT) return;

    lv_obj_t *targets[] = { g_scr_main, g_scr_clock };
    lv_scr_load_anim(targets[id],
        forward ? LV_SCR_LOAD_ANIM_MOVE_LEFT : LV_SCR_LOAD_ANIM_MOVE_RIGHT,
        200, 0, false);
    g_current = id;
}

void ui_set_solar_max(uint32_t w) {
    g_solar_input_w = w;
    lv_arc_set_range(g_arc_solar, 0, (uint32_t)(w * 1.1f));
    lv_arc_set_value(g_arc_solar, lv_arc_get_value(g_arc_solar));
    lv_obj_invalidate(g_arc_solar);
}

void ui_show_boot_ip(const char *ip) {
    lv_label_set_text(g_lbl_boot_status, ip);
    lv_timer_create(boot_timer_cb, 4000, NULL);
}

ScreenId ui_current_screen(void) {
    return g_current;
}
