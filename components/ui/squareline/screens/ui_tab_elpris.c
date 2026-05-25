#include "../ui.h"
#include "../ui_theme.h"
#include "ui_scr_home.h"
#include "elpris_api.h"
#include "ui_tab_elpris.h"  


lv_obj_t* ui_panel_price_header  = NULL;
lv_obj_t* ui_lbl_price_now       = NULL;
lv_obj_t* ui_lbl_price_val       = NULL;
lv_obj_t* ui_lbl_price_unit      = NULL;
lv_obj_t* ui_lbl_price_hi        = NULL;
lv_obj_t* ui_lbl_price_lo        = NULL;
lv_obj_t* ui_lbl_price_avg       = NULL;
lv_obj_t* ui_panel_chart         = NULL;
lv_obj_t* ui_chart_elpris        = NULL;
lv_obj_t* ui_chart_elpris_Xaxis  = NULL;
lv_obj_t* ui_chart_elpris_Yaxis1 = NULL;
lv_obj_t* ui_lbl_leg_cheap       = NULL;
lv_obj_t* ui_lbl_leg_avg         = NULL;
lv_obj_t* ui_lbl_leg_exp         = NULL;
lv_obj_t* ui_lbl_leg_now         = NULL;

// Global chart data array (so elpris_api can update it)
lv_coord_t g_elpris_data[24] = {
    8, 7, 7, 6, 6, 8, 18, 35, 52, 58, 55, 50,
    42, 38, 29, 12, 5, 18, 45, 72, 100, 95, 80, 60
};

// Current hour for "Now" indicator
static uint8_t g_current_hour = 12;

static void elpris_chart_draw_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_TASK_ADDED) return;

    lv_obj_t*           chart     = lv_event_get_target(e);
    lv_draw_task_t*     draw_task = lv_event_get_draw_task(e);
    lv_draw_dsc_base_t* base_dsc  = lv_draw_task_get_draw_dsc(draw_task);

    if (base_dsc->part != LV_PART_ITEMS) return;

    lv_chart_series_t* ser = lv_chart_get_series_next(chart, NULL);
    if (!ser) return;

    lv_coord_t* y_arr = lv_chart_get_y_array(chart, ser);
    if (!y_arr) return;

    uint32_t   id  = base_dsc->id2;
    lv_coord_t val = y_arr[id];

    lv_color_t color;
    if (val < UI_ELPRIS_CHEAP_MAX)
        color = UI_COLOR_GOOD;
    else if (val < UI_ELPRIS_WARN_MAX)
        color = UI_COLOR_WARN;
    else
        color = UI_COLOR_BAD;

    lv_draw_fill_dsc_t* fill_dsc = lv_draw_task_get_fill_dsc(draw_task);
    if (fill_dsc) fill_dsc->color = color;
}

static lv_obj_t* make_summary_card(lv_obj_t* parent, int grow) {
    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_height(card, lv_pct(100));
    lv_obj_set_flex_grow(card, grow);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(card, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    return card;
}

static lv_obj_t* card_label(lv_obj_t* card, const char* text, lv_color_t color,
                             const lv_font_t* font) {
    lv_obj_t* lbl = lv_label_create(card);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl, font, LV_PART_MAIN | LV_STATE_DEFAULT);
    return lbl;
}

// Public function to update the display with new price data
void ui_elpris_update_display(const ElprisData* data) {
    if (!data || !data->valid) return;
    
    char buf[32];
    
    // Update current price (at current hour)
    uint32_t current_price = data->hourly_prices[g_current_hour];
    snprintf(buf, sizeof(buf), "%.2f", (float)current_price / 100.0f);
    lv_label_set_text(ui_lbl_price_val, buf);
    
    // Update min price
    snprintf(buf, sizeof(buf), "%.2f kr", data->min_price_sek);
    lv_label_set_text(ui_lbl_price_lo, buf);
    
    // Update max price
    snprintf(buf, sizeof(buf), "%.2f kr", data->max_price_sek);
    lv_label_set_text(ui_lbl_price_hi, buf);
    
    // Update avg price
    snprintf(buf, sizeof(buf), "%.2f kr", data->avg_price_sek);
    lv_label_set_text(ui_lbl_price_avg, buf);
    
    // Update the chart data array
    for (int i = 0; i < 24; i++) {
        g_elpris_data[i] = (lv_coord_t)data->hourly_prices[i];
    }
    
    // Refresh the chart
    if (ui_chart_elpris) {
        lv_chart_refresh(ui_chart_elpris);
    }
}

// Set current hour for the "Now" indicator
void ui_elpris_set_current_hour(uint8_t hour) {
    g_current_hour = hour;
}

void ui_tab_elpris_init(void) {
    lv_obj_t* body = lv_obj_create(ui_tabelpris);
    lv_obj_set_size(body, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(body, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(body, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ---- Summary cards row ----
    ui_panel_price_header = lv_obj_create(body);
    lv_obj_set_width(ui_panel_price_header, lv_pct(100));
    lv_obj_set_height(ui_panel_price_header, 120);
    lv_obj_remove_flag(ui_panel_price_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_panel_price_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_panel_price_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(ui_panel_price_header, LV_OPA_TRANSP,
                            LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_price_header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_price_header, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_panel_price_header, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Current price card (flex-grow=2)
    lv_obj_t* card_curr = make_summary_card(ui_panel_price_header, 2);
    ui_lbl_price_now = card_label(card_curr, "CURRENT PRICE", UI_COLOR_INK3,
                                  &lv_font_montserrat_12);
    ui_lbl_price_val = card_label(card_curr, "0.00", UI_COLOR_GOOD,
                                  &lv_font_montserrat_40);
    ui_lbl_price_unit = card_label(card_curr, "kr/kWh", UI_COLOR_INK3,
                                   &lv_font_montserrat_14);

    // Max card
    lv_obj_t* card_max = make_summary_card(ui_panel_price_header, 1);
    card_label(card_max, "MAX", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_hi = card_label(card_max, "-- kr", UI_COLOR_BAD, &lv_font_montserrat_18);
    card_label(card_max, "at --:--", UI_COLOR_INK3, &lv_font_montserrat_12);

    // Min card
    lv_obj_t* card_min = make_summary_card(ui_panel_price_header, 1);
    card_label(card_min, "MIN", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_lo = card_label(card_min, "-- kr", UI_COLOR_GOOD, &lv_font_montserrat_18);
    card_label(card_min, "at --:--", UI_COLOR_INK3, &lv_font_montserrat_12);

    // Avg card
    lv_obj_t* card_avg = make_summary_card(ui_panel_price_header, 1);
    card_label(card_avg, "AVG", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_avg = card_label(card_avg, "-- kr", UI_COLOR_INK1, &lv_font_montserrat_18);

    // ---- Chart card ----
    ui_panel_chart = lv_obj_create(body);
    lv_obj_set_width(ui_panel_chart, lv_pct(100));
    lv_obj_set_flex_grow(ui_panel_chart, 1);
    lv_obj_remove_flag(ui_panel_chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_panel_chart, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_panel_chart, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_radius(ui_panel_chart, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_panel_chart, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_panel_chart, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_chart, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_panel_chart, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_title = lv_label_create(ui_panel_chart);
    lv_label_set_text(lbl_title, "HOURLY PRICES");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // Chart widget
    ui_chart_elpris = lv_chart_create(ui_panel_chart);
    lv_obj_set_width(ui_chart_elpris, lv_pct(100));
    lv_obj_set_flex_grow(ui_chart_elpris, 1);
    lv_obj_remove_flag(ui_chart_elpris, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_chart_elpris, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_chart_set_type(ui_chart_elpris, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(ui_chart_elpris, 24);
    lv_chart_set_range(ui_chart_elpris, LV_CHART_AXIS_PRIMARY_Y, 0, 120);
    lv_chart_set_div_line_count(ui_chart_elpris, 5, 23);
    lv_obj_set_style_bg_color(ui_chart_elpris, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_chart_elpris, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_chart_elpris, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris, UI_COLOR_LINE_SOFT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_pad(ui_chart_elpris, LV_MAX3(10, 10, 25),
                                 LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_outline_width(ui_chart_elpris, -1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_chart_series_t* ser = lv_chart_add_series(ui_chart_elpris, UI_COLOR_GOOD,
                                                  LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(ui_chart_elpris, ser, g_elpris_data);

    lv_obj_add_event_cb(ui_chart_elpris, elpris_chart_draw_cb,
                        LV_EVENT_DRAW_TASK_ADDED, NULL);

    // X-axis scale (child of chart)
    ui_chart_elpris_Xaxis = lv_scale_create(ui_chart_elpris);
    lv_scale_set_mode(ui_chart_elpris_Xaxis, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    lv_obj_set_size(ui_chart_elpris_Xaxis, lv_pct(100), 10);
    lv_obj_set_align(ui_chart_elpris_Xaxis, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(ui_chart_elpris_Xaxis, 10);
    lv_obj_set_style_line_width(ui_chart_elpris_Xaxis, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_chart_elpris_Xaxis, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_width(ui_chart_elpris_Xaxis, 1, LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_chart_elpris_Xaxis, 3, LV_PART_ITEMS);
    lv_obj_set_style_length(ui_chart_elpris_Xaxis, 5, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(ui_chart_elpris_Xaxis, UI_COLOR_INK3,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_chart_elpris_Xaxis, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris_Xaxis, UI_COLOR_LINE_SOFT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris_Xaxis, UI_COLOR_LINE,
                                LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_scale_set_range(ui_chart_elpris_Xaxis, 0, 24);
    lv_scale_set_total_tick_count(ui_chart_elpris_Xaxis, 49);
    lv_scale_set_major_tick_every(ui_chart_elpris_Xaxis, 2);
    static const char* x_ticks[] = {
        "00", "01", "02", "03", "04", "05", "06", "07",
        "08", "09", "10", "11", "12", "13", "14", "15",
        "16", "17", "18", "19", "20", "21", "22", "23", ""
    };
    lv_scale_set_text_src(ui_chart_elpris_Xaxis, x_ticks);

    // Y-axis scale (child of chart)
    ui_chart_elpris_Yaxis1 = lv_scale_create(ui_chart_elpris);
    lv_scale_set_mode(ui_chart_elpris_Yaxis1, LV_SCALE_MODE_VERTICAL_LEFT);
    lv_obj_set_size(ui_chart_elpris_Yaxis1, 10, lv_pct(100));
    lv_obj_set_align(ui_chart_elpris_Yaxis1, LV_ALIGN_LEFT_MID);
    lv_obj_set_x(ui_chart_elpris_Yaxis1, -8);
    lv_obj_set_style_line_width(ui_chart_elpris_Yaxis1, 0, LV_PART_MAIN);
    lv_obj_set_style_line_width(ui_chart_elpris_Yaxis1, 1, LV_PART_ITEMS);
    lv_obj_set_style_line_width(ui_chart_elpris_Yaxis1, 1, LV_PART_INDICATOR);
    lv_obj_set_style_length(ui_chart_elpris_Yaxis1, 3, LV_PART_ITEMS);
    lv_obj_set_style_length(ui_chart_elpris_Yaxis1, 5, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(ui_chart_elpris_Yaxis1, UI_COLOR_INK3,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_chart_elpris_Yaxis1, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris_Yaxis1, UI_COLOR_LINE_SOFT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris_Yaxis1, UI_COLOR_LINE,
                                LV_PART_INDICATOR | LV_STATE_DEFAULT);
    lv_scale_set_range(ui_chart_elpris_Yaxis1, 0, 120);
    lv_scale_set_total_tick_count(ui_chart_elpris_Yaxis1, 13);
    lv_scale_set_major_tick_every(ui_chart_elpris_Yaxis1, 2);

    // Legend row
    lv_obj_t* legend = lv_obj_create(ui_panel_chart);
    lv_obj_set_width(legend, lv_pct(100));
    lv_obj_set_height(legend, LV_SIZE_CONTENT);
    lv_obj_remove_flag(legend, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(legend, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(legend, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(legend, 16, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_leg_cheap = card_label(legend, "Cheap", UI_COLOR_GOOD, &lv_font_montserrat_12);
    ui_lbl_leg_avg   = card_label(legend, "Average", UI_COLOR_WARN, &lv_font_montserrat_12);
    ui_lbl_leg_exp   = card_label(legend, "Expensive", UI_COLOR_BAD, &lv_font_montserrat_12);
    ui_lbl_leg_now   = card_label(legend, "Now", UI_COLOR_INK1, &lv_font_montserrat_12);
}