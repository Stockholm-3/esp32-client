#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// -- UI elements
static lv_obj_t *g_temp_label, *g_hum_label, *g_pres_label;

// Helper to create the circular metric displays
static lv_obj_t* create_metric_circle(lv_obj_t* parent, const char* title, lv_obj_t** label_out) {
    lv_obj_t* obj = lv_obj_create(parent);
    lv_obj_set_size(obj, 140, 140);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xD9D9D9), 0);
    lv_obj_set_style_border_width(obj, 0, 0);

    // Title Label
    lv_obj_t* t_lbl = lv_label_create(obj);
    lv_label_set_text(t_lbl, title);
    lv_obj_set_style_text_align(t_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(t_lbl, lv_color_hex(0x666666), 0);
    lv_obj_align(t_lbl, LV_ALIGN_TOP_MID, 0, 30);

    // Value Label
    *label_out = lv_label_create(obj);
    lv_obj_set_style_text_color(*label_out, lv_color_hex(0x111111), 0);
    lv_label_set_text(*label_out, "--");
    lv_obj_align(*label_out, LV_ALIGN_CENTER, 0, 10);

    return obj;
}

static void update_weather_cb(lv_timer_t* timer) {
    int temp = rand() % 35;
    int hum  = 40 + (rand() % 30);
    int pres = 1000 + (rand() % 20);

    if (g_temp_label) {
        lv_label_set_text_fmt(g_temp_label, "%d°C", temp);
    }
    if (g_hum_label) {
        lv_label_set_text_fmt(g_hum_label, "%d%%", hum);
    }
    if (g_pres_label) {
        lv_label_set_text_fmt(g_pres_label, "%d hPa", pres);
    }
}

void ui_build(void) {
    srand(time(NULL));

    lv_obj_t* scr = lv_scr_act();
    if (!scr) {
        return; // Safety check for simulator initialization
    }

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A1A), 0);

    // ─── TOP NAVIGATION (Replacing Flex with manual alignment for
    // compatibility) ───
    lv_obj_t* nav_bar = lv_obj_create(scr);
    lv_obj_set_size(nav_bar, 800, 70);
    lv_obj_align(nav_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(nav_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_width(nav_bar, 0, 0);
    lv_obj_set_style_radius(nav_bar, 0, 0);

    const char* btns[] = {"HOME", "WEATHER", "ELPRIS", "SETTINGS"};
    for (int i = 0; i < 4; i++) {
        lv_obj_t* b = lv_btn_create(nav_bar);
        lv_obj_set_size(b, 140, 40);
        // Manually align to ensure it works even if LV_USE_FLEX is 0
        lv_obj_align(b, LV_ALIGN_LEFT_MID, 40 + (i * 180), 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x4A6D9C), 0);

        lv_obj_t* l = lv_label_create(b);
        lv_label_set_text(l, btns[i]);
        lv_obj_center(l);
    }

    // ─── MAIN CONTENT AREA ───
    lv_obj_t* cont = lv_obj_create(scr);
    lv_obj_set_size(cont, 760, 380);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 10, 0);

    // Left Side: Circles
    lv_obj_t* c1 = create_metric_circle(cont, "LOCAL\nTEMP", &g_temp_label);
    lv_obj_align(c1, LV_ALIGN_TOP_LEFT, 20, 20);

    lv_obj_t* c2 = create_metric_circle(cont, "LOCAL\nPRESSURE", &g_pres_label);
    lv_obj_align(c2, LV_ALIGN_LEFT_MID, 160, 0);

    lv_obj_t* c3 = create_metric_circle(cont, "LOCAL\nHUMIDITY", &g_hum_label);
    lv_obj_align(c3, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    // Right Side: Forecast Panel
    lv_obj_t* forecast_panel = lv_obj_create(cont);
    lv_obj_set_size(forecast_panel, 380, 320);
    lv_obj_align(forecast_panel, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(forecast_panel, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_border_width(forecast_panel, 0, 0);
    lv_obj_set_style_radius(forecast_panel, 8, 0);

    lv_obj_t* f_lbl = lv_label_create(forecast_panel);
    lv_label_set_text(f_lbl, "CURRENT WEATHER\n+\nFORECAST FOR CURRENT DAY");
    lv_obj_set_style_text_align(f_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(f_lbl, lv_color_hex(0x333333), 0);
    lv_obj_center(f_lbl);

    lv_timer_create(update_weather_cb, 1000, NULL);
}
