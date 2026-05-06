#include "../ui.h"
#include "../ui_theme.h"

lv_obj_t* ui_panel_batt_hero   = NULL;
lv_obj_t* ui_arc_battery       = NULL;
lv_obj_t* ui_lbl_batt_pct      = NULL;
lv_obj_t* ui_lbl_batt_status   = NULL;
lv_obj_t* ui_panel_batt_stats  = NULL;
lv_obj_t* ui_lbl_batt_voltage  = NULL;
lv_obj_t* ui_lbl_batt_current  = NULL;
lv_obj_t* ui_lbl_batt_cycles   = NULL;
lv_obj_t* ui_panel_batt_trend  = NULL;
lv_obj_t* ui_chart_batt_trend  = NULL;

// Returns value label so caller can update it later
static lv_obj_t* make_stat_card(lv_obj_t* parent, const char* title, const char* value) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_width(card, 180);
    lv_obj_set_height(card, 80);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(card, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(card, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(card, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_title = lv_label_create(card);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* lbl_val = lv_label_create(card);
    lv_label_set_text(lbl_val, value);
    lv_obj_set_style_text_color(lbl_val, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_val, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_val, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return lbl_val;
}

void ui_tab_battery_init(void) {
    // ── Hero card ────────────────────────────────────────────────────────────
    ui_panel_batt_hero = lv_obj_create(ui_tabbattery);
    lv_obj_set_size(ui_panel_batt_hero, 1004, 230);
    lv_obj_set_pos(ui_panel_batt_hero, 10, 10);
    lv_obj_remove_flag(ui_panel_batt_hero, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_panel_batt_hero, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_panel_batt_hero, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_panel_batt_hero, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_batt_hero, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_panel_batt_hero, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_batt_hero, 16, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Arc (270° sweep, 0–100%)
    ui_arc_battery = lv_arc_create(ui_panel_batt_hero);
    lv_obj_set_size(ui_arc_battery, 160, 160);
    lv_obj_align(ui_arc_battery, LV_ALIGN_LEFT_MID, 0, 0);
    lv_arc_set_rotation(ui_arc_battery, 135);
    lv_arc_set_bg_angles(ui_arc_battery, 0, 270);
    lv_arc_set_value(ui_arc_battery, 78);
    lv_arc_set_range(ui_arc_battery, 0, 100);
    lv_obj_remove_flag(ui_arc_battery, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(ui_arc_battery, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_arc_battery, 12, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_color(ui_arc_battery, UI_COLOR_BG3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_arc_width(ui_arc_battery, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_arc_battery, LV_OPA_TRANSP, LV_PART_KNOB | LV_STATE_DEFAULT);

    // Percentage label centered in arc
    ui_lbl_batt_pct = lv_label_create(ui_panel_batt_hero);
    lv_label_set_text(ui_lbl_batt_pct, "78%");
    lv_obj_set_style_text_font(ui_lbl_batt_pct, &lv_font_montserrat_40, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_batt_pct, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(ui_lbl_batt_pct, ui_arc_battery, LV_ALIGN_CENTER, 0, -8);

    // Status text below percentage
    ui_lbl_batt_status = lv_label_create(ui_panel_batt_hero);
    lv_label_set_text(ui_lbl_batt_status, "Charging\nETA 1h 12m");
    lv_obj_set_style_text_font(ui_lbl_batt_status, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_lbl_batt_status, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(ui_lbl_batt_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align_to(ui_lbl_batt_status, ui_arc_battery, LV_ALIGN_CENTER, 0, 22);

    // Stats row to the right of the arc
    ui_panel_batt_stats = lv_obj_create(ui_panel_batt_hero);
    lv_obj_set_size(ui_panel_batt_stats, 770, 100);
    lv_obj_align(ui_panel_batt_stats, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_remove_flag(ui_panel_batt_stats, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_panel_batt_stats, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_batt_stats, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_batt_stats, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(ui_panel_batt_stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_panel_batt_stats, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    ui_lbl_batt_voltage = make_stat_card(ui_panel_batt_stats, "VOLTAGE", "3.85 V");
    ui_lbl_batt_current = make_stat_card(ui_panel_batt_stats, "CURRENT", "1.2 A");
    ui_lbl_batt_cycles  = make_stat_card(ui_panel_batt_stats, "CYCLES",  "142");

    // ── Trend card ───────────────────────────────────────────────────────────
    ui_panel_batt_trend = lv_obj_create(ui_tabbattery);
    lv_obj_set_size(ui_panel_batt_trend, 1004, 240);
    lv_obj_set_pos(ui_panel_batt_trend, 10, 252);
    lv_obj_remove_flag(ui_panel_batt_trend, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_panel_batt_trend, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_panel_batt_trend, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_panel_batt_trend, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_batt_trend, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_panel_batt_trend, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_batt_trend, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_trend_title = lv_label_create(ui_panel_batt_trend);
    lv_label_set_text(lbl_trend_title, "LAST 24 HOURS");
    lv_obj_set_style_text_font(lbl_trend_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_trend_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(lbl_trend_title, LV_ALIGN_TOP_LEFT, 0, 0);

    ui_chart_batt_trend = lv_chart_create(ui_panel_batt_trend);
    lv_obj_set_size(ui_chart_batt_trend, 970, 190);
    lv_obj_align(ui_chart_batt_trend, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(ui_chart_batt_trend, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_remove_flag(ui_chart_batt_trend, LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(ui_chart_batt_trend, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_chart_batt_trend, 24);
    lv_chart_set_range(ui_chart_batt_trend, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(ui_chart_batt_trend, 4, 0);
    lv_obj_set_style_bg_color(ui_chart_batt_trend, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_chart_batt_trend, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_chart_batt_trend, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_batt_trend, UI_COLOR_LINE, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_chart_series_t* ser = lv_chart_add_series(
        ui_chart_batt_trend, UI_COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    static lv_coord_t batt_trend_data[] = {
        55, 52, 50, 48, 47, 45, 44, 43, 43, 60, 65, 68,
        72, 74, 75, 76, 76, 77, 77, 78, 78, 78, 78, 78
    };
    lv_chart_set_ext_y_array(ui_chart_batt_trend, ser, batt_trend_data);

    lv_obj_set_style_line_width(ui_chart_batt_trend, 2, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_size(ui_chart_batt_trend, 0, 0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_chart_batt_trend, UI_COLOR_ACCENT, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_chart_batt_trend, 50, LV_PART_ITEMS | LV_STATE_DEFAULT);

    // X-axis hour labels
    lv_obj_t* xaxis = lv_scale_create(ui_chart_batt_trend);
    lv_scale_set_mode(xaxis, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    lv_obj_set_size(xaxis, lv_pct(100), 20);
    lv_obj_align(xaxis, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_y(xaxis,
                 20 + lv_obj_get_style_pad_bottom(ui_chart_batt_trend, LV_PART_MAIN) +
                     lv_obj_get_style_border_width(ui_chart_batt_trend, LV_PART_MAIN));
    lv_obj_set_style_line_width(xaxis, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(xaxis, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_width(xaxis, 1, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(xaxis, UI_COLOR_INK3, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(xaxis, &lv_font_montserrat_12, LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_scale_set_range(xaxis, 0, 23);
    lv_scale_set_total_tick_count(xaxis, 47);
    lv_scale_set_major_tick_every(xaxis, 8);
    static const char* hour_labels[] = {"0h", "3h", "6h", "9h", "12h",
                                        "15h", "18h", "21h", "23h", NULL};
    lv_scale_set_text_src(xaxis, hour_labels);
}
