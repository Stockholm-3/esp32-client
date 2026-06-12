/**
 * @file ui_tab_elpris.c
 * @ingroup ui
 * @brief Electricity price (Elpris) tab — layout, chart rendering and data parsing.
 *
 * Builds the Elpris tab content inside the home-screen TabView:
 * - Summary row with current, high, low and average prices for the day.
 * - LVGL line chart showing up to 96 hourly price points with colour-coded
 *   zones (cheap / average / expensive) defined by @c ui_theme.h thresholds.
 * - Animated cursor that tracks the current hour on the chart.
 *
 * Data is supplied via @ref ui_tab_elpris_handle_server_response() which
 * parses the raw JSON from the price API. The current-hour highlight is
 * refreshed by @ref ui_tab_elpris_update_now(), which should be called
 * every hour.
 */
#include "../ui.h"
#include "../ui_theme.h"
#include "ui_scr_home.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Price summary row */
lv_obj_t* ui_panel_price_header  = NULL; /**< Container for the price summary row at the top of the tab. */
lv_obj_t* ui_lbl_price_now       = NULL; /**< "Now" heading label next to the current price. */
lv_obj_t* ui_lbl_price_val       = NULL; /**< Current electricity price value (öre/kWh). */
lv_obj_t* ui_lbl_price_unit      = NULL; /**< Unit label ("öre/kWh") displayed next to the value. */
lv_obj_t* ui_lbl_price_hi        = NULL; /**< Today's highest price value. */
lv_obj_t* ui_lbl_price_hi_time   = NULL; /**< Time of day when today's highest price occurs. */
lv_obj_t* ui_lbl_price_lo        = NULL; /**< Today's lowest price value. */
lv_obj_t* ui_lbl_price_lo_time   = NULL; /**< Time of day when today's lowest price occurs. */
lv_obj_t* ui_lbl_price_avg       = NULL; /**< Today's average price value. */

/* Chart area */
lv_obj_t* ui_panel_chart         = NULL; /**< Container panel holding the price chart and its axes. */
lv_obj_t* ui_chart_elpris        = NULL; /**< LVGL line chart displaying up to 96 hourly price points. */
lv_obj_t* ui_chart_elpris_Xaxis  = NULL; /**< X-axis decoration object (time labels). */
lv_obj_t* ui_chart_elpris_Yaxis1 = NULL; /**< Y-axis decoration object (price labels). */

/* Chart legend */
lv_obj_t* ui_lbl_leg_cheap       = NULL; /**< Legend marker for the cheap price zone (green). */
lv_obj_t* ui_lbl_leg_avg         = NULL; /**< Legend marker for the average price zone (amber). */
lv_obj_t* ui_lbl_leg_exp         = NULL; /**< Legend marker for the expensive price zone (red). */
lv_obj_t* ui_lbl_leg_now         = NULL; /**< Legend marker for the current-hour cursor line. */

/** Hourly price values in öre (price_kr × 100) for up to 96 points (4 days). */
static lv_coord_t         elpris_data[96];
/** LVGL chart data series associated with @c elpris_data. */
static lv_chart_series_t* elpris_series = NULL;
/** Index into @c elpris_data for the current hour; -1 if not yet set. */
static int                g_now_index   = -1;
/** Index of the cheapest hour in @c elpris_data; -1 if not yet set. */
static int                g_min_index   = -1;
/** Index of the most expensive hour in @c elpris_data; -1 if not yet set. */
static int                g_max_index   = -1;
/** Index of the hour whose price is closest to the daily average; -1 if not yet set. */
static int                g_avg_index   = -1;
static float              g_raw[96]     = {0};
static float              g_avg_price   = 0.0f;
static float              g_min_price   = 0.0f;
static float              g_max_price   = 0.0f;

// Returns first index >= `from` where at least `min_slots` consecutive entries
// are above (if `above`) or below (!`above`) `threshold`. Returns -1 if not found.
static int find_block_start(int from, float threshold, bool above, int min_slots) {
    for (int i = from; i <= 96 - min_slots; i++) {
        bool ok = true;
        for (int j = 0; j < min_slots; j++) {
            bool cond = above ? (g_raw[i + j] > threshold)
                              : (g_raw[i + j] < threshold);
            if (!cond) { ok = false; break; }
        }
        if (ok) return i;
    }
    return -1;
}

static lv_coord_t ui_tab_elpris_price_to_chart_units(float price_kr) {
    return (lv_coord_t)roundf(price_kr * 100.0f);
}

static void ui_tab_elpris_update_chart_values(const lv_coord_t* values, size_t count) {
    size_t used = count < 96 ? count : 96;
    memcpy(elpris_data, values, used * sizeof(lv_coord_t));
    if (used < 96) memset(elpris_data + used, 0, (96 - used) * sizeof(lv_coord_t));

    lv_coord_t max_val = 0;
    for (size_t i = 0; i < 96; i++) {
        if (elpris_data[i] > max_val) max_val = elpris_data[i];
    }
    lv_coord_t y_max = max_val + 20;
    lv_chart_set_range(ui_chart_elpris, LV_CHART_AXIS_PRIMARY_Y, 0, y_max);
    lv_scale_set_range(ui_chart_elpris_Yaxis1, 0, y_max);

    lv_chart_refresh(ui_chart_elpris);
    lv_obj_invalidate(ui_chart_elpris_Yaxis1);
}

static void ui_tab_elpris_update_summary(float now, float min_val, float max_val, float avg,
                                         int now_idx, int min_idx, int max_idx, int avg_idx) {
    g_now_index = now_idx;
    g_min_index = min_idx;
    g_max_index = max_idx;
    g_avg_index = avg_idx;

    char buf[20];
    if (ui_lbl_price_val) {
        snprintf(buf, sizeof(buf), "%.2f", (double)now);
        lv_label_set_text(ui_lbl_price_val, buf);
    }
    if (ui_lbl_price_hi) {
        snprintf(buf, sizeof(buf), "%.2f kr", (double)max_val);
        lv_label_set_text(ui_lbl_price_hi, buf);
    }
    if (ui_lbl_price_hi_time) {
        snprintf(buf, sizeof(buf), "at %02u:%02u", (unsigned)(max_idx / 4), (unsigned)((max_idx % 4) * 15));
        lv_label_set_text(ui_lbl_price_hi_time, buf);
    }
    if (ui_lbl_price_lo) {
        snprintf(buf, sizeof(buf), "%.2f kr", (double)min_val);
        lv_label_set_text(ui_lbl_price_lo, buf);
    }
    if (ui_lbl_price_lo_time) {
        snprintf(buf, sizeof(buf), "at %02u:%02u", (unsigned)(min_idx / 4), (unsigned)((min_idx % 4) * 15));
        lv_label_set_text(ui_lbl_price_lo_time, buf);
    }
    if (ui_lbl_price_avg) {
        snprintf(buf, sizeof(buf), "%.2f kr", (double)avg);
        lv_label_set_text(ui_lbl_price_avg, buf);
    }

    bool is_good = now < avg * 0.85f;
    bool is_bad  = now > avg * 1.15f;
    lv_color_t state_color = is_good ? UI_COLOR_GOOD
                           : is_bad  ? UI_COLOR_BAD
                                     : UI_COLOR_WARN;
    if (ui_lbl_price_val)
        lv_obj_set_style_text_color(ui_lbl_price_val, state_color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_lbl_price_avg)
        lv_obj_set_style_text_color(ui_lbl_price_avg, UI_COLOR_WARN, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_lbl_elec_price) {
        snprintf(buf, sizeof(buf), "%.2f", (double)now);
        lv_label_set_text(ui_lbl_elec_price, buf);
        lv_obj_set_style_text_color(ui_lbl_elec_price, state_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_lbl_elec_status) {
        const char* status = is_good ? "Good time to use electricity"
                           : is_bad  ? "Expensive time to use electricity"
                                     : "Average electricity price";
        lv_label_set_text(ui_lbl_elec_status, status);
        lv_obj_set_style_text_color(ui_lbl_elec_status, state_color,
                                    LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    if (ui_dot_light_green)
        lv_obj_set_style_bg_opa(ui_dot_light_green, is_good ? 255 : 80,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_dot_light_amber)
        lv_obj_set_style_bg_opa(ui_dot_light_amber, (!is_good && !is_bad) ? 255 : 80,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_dot_light_red)
        lv_obj_set_style_bg_opa(ui_dot_light_red, is_bad ? 255 : 80,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    if (ui_lbl_elec_sub) {
        char sub[48] = "";
        if (now_idx >= 0 && now_idx < 96) {
            float thr_bad   = avg * 1.15f;
            float thr_good  = avg * 0.85f;
            float now_price = now;
            if (is_good) {
                int peak_idx = find_block_start(now_idx + 1, thr_bad, true, 4);
                if (peak_idx >= 0)
                    snprintf(sub, sizeof(sub), "Next peak %02u:%02u",
                             (unsigned)(peak_idx / 4), (unsigned)((peak_idx % 4) * 15));
                else {
                    // find when price rises above current level
                    int rise_idx = find_block_start(now_idx + 1, now_price, true, 4);
                    if (rise_idx >= 0)
                        snprintf(sub, sizeof(sub), "Rising from %02u:%02u",
                                 (unsigned)(rise_idx / 4), (unsigned)((rise_idx % 4) * 15));
                    else
                        snprintf(sub, sizeof(sub), "Low price all day");
                }
            } else if (is_bad) {
                int cheap_idx = find_block_start(now_idx + 1, thr_good, false, 4);
                if (cheap_idx >= 0)
                    snprintf(sub, sizeof(sub), "Cheaper from %02u:%02u",
                             (unsigned)(cheap_idx / 4), (unsigned)((cheap_idx % 4) * 15));
                else {
                    // find when price drops sustainably below current level
                    int ease_idx = find_block_start(now_idx + 1, now_price, false, 4);
                    if (ease_idx >= 0)
                        snprintf(sub, sizeof(sub), "Easing from %02u:%02u",
                                 (unsigned)(ease_idx / 4), (unsigned)((ease_idx % 4) * 15));
                    else
                        snprintf(sub, sizeof(sub), "High price all day");
                }
            } else {
                int peak_idx  = find_block_start(now_idx + 1, thr_bad,  true,  4);
                int cheap_idx = find_block_start(now_idx + 1, thr_good, false, 4);
                if (cheap_idx >= 0 && (peak_idx < 0 || cheap_idx < peak_idx))
                    snprintf(sub, sizeof(sub), "Cheaper from %02u:%02u",
                             (unsigned)(cheap_idx / 4), (unsigned)((cheap_idx % 4) * 15));
                else if (peak_idx >= 0)
                    snprintf(sub, sizeof(sub), "Next peak %02u:%02u",
                             (unsigned)(peak_idx / 4), (unsigned)((peak_idx % 4) * 15));
                else
                    snprintf(sub, sizeof(sub), "Stable prices today");
            }
        }
        lv_label_set_text(ui_lbl_elec_sub, sub[0] ? sub : "--");
    }
}

static bool ui_tab_elpris_parse_response(const char* json, size_t len) {
    cJSON* root = cJSON_ParseWithLength(json, len);
    if (!root) return false;

    // Support two formats:
    // 1. Flat array: [{SEK_per_kWh, time_start, ...}, ...] (local server)
    // 2. Object: {decisions: [{input_variables: {elpris: ...}}]} (legacy mock)
    cJSON* arr = cJSON_IsArray(root) ? root : cJSON_GetObjectItem(root, "decisions");
    if (!cJSON_IsArray(arr)) { cJSON_Delete(root); return false; }

    int count = cJSON_GetArraySize(arr);
    int used  = count < 96 ? count : 96;

    float raw[96] = {0};
    for (int i = 0; i < used; i++) {
        cJSON* entry = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsObject(entry)) continue;
        // flat format
        cJSON* sek = cJSON_GetObjectItem(entry, "SEK_per_kWh");
        if (cJSON_IsNumber(sek)) { raw[i] = (float)sek->valuedouble; continue; }
        // legacy format
        cJSON* input_vars = cJSON_GetObjectItem(entry, "input_variables");
        if (cJSON_IsObject(input_vars)) {
            cJSON* item = cJSON_GetObjectItem(input_vars, "elpris");
            if (cJSON_IsNumber(item)) raw[i] = (float)item->valuedouble;
        }
    }
    cJSON_Delete(root);

    // determine current slot from system time
    struct tm t;
    time_t ts = time(NULL);
    localtime_r(&ts, &t);
    int now_idx = t.tm_hour * 4 + t.tm_min / 15;
    if (now_idx >= 96) now_idx = 95;

    // compute statistics
    float min_val = raw[0], max_val = raw[0], sum = 0.0f;
    int   min_idx = 0, max_idx = 0;
    for (int i = 0; i < 96; i++) {
        if (raw[i] < min_val) { min_val = raw[i]; min_idx = i; }
        if (raw[i] > max_val) { max_val = raw[i]; max_idx = i; }
        sum += raw[i];
    }
    float avg = sum / 96.0f;

    // find bar closest to avg (skip now/min/max slots)
    int   avg_idx  = 0;
    float avg_diff = 1e9f;
    for (int i = 0; i < 96; i++) {
        if (i == now_idx || i == min_idx || i == max_idx) continue;
        float d = raw[i] - avg;
        if (d < 0) d = -d;
        if (d < avg_diff) { avg_diff = d; avg_idx = i; }
    }

    // persist raw prices and statistics for later re-use in update_now
    memcpy(g_raw, raw, sizeof(g_raw));
    g_avg_price = avg;
    g_min_price = min_val;
    g_max_price = max_val;
    g_min_index = min_idx;
    g_max_index = max_idx;
    g_avg_index = avg_idx;

    // convert to chart units
    lv_coord_t chart_vals[96];
    for (int i = 0; i < 96; i++) chart_vals[i] = ui_tab_elpris_price_to_chart_units(raw[i]);

    ui_tab_elpris_update_summary(raw[now_idx], min_val, max_val, avg,
                                 now_idx, min_idx, max_idx, avg_idx);
    ui_tab_elpris_update_chart_values(chart_vals, 96);
    return true;
}

void ui_tab_elpris_handle_server_response(const char* json, size_t len) {
    if (!json || len == 0) return;
    if (!ui_tab_elpris_parse_response(json, len)) {
        LV_LOG_WARN("elpris: failed to parse server response");
    }
}

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
    if ((int)id == g_now_index)
        color = lv_color_hex(0xFFFFFF);
    else if (g_avg_price > 0.0f && (float)val < g_avg_price * 0.85f * 100.0f)
        color = UI_COLOR_GOOD;
    else if (g_avg_price > 0.0f && (float)val < g_avg_price * 1.15f * 100.0f)
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
    ui_lbl_price_val = card_label(card_curr, "--", UI_COLOR_GOOD,
                                  &lv_font_montserrat_40);
    ui_lbl_price_unit = card_label(card_curr, "kr/kWh", UI_COLOR_INK3,
                                   &lv_font_montserrat_14);

    // Max card
    lv_obj_t* card_max = make_summary_card(ui_panel_price_header, 1);
    card_label(card_max, "MAX", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_hi      = card_label(card_max, "--", UI_COLOR_BAD, &lv_font_montserrat_18);
    ui_lbl_price_hi_time = card_label(card_max, "--:--", UI_COLOR_INK3, &lv_font_montserrat_12);

    // Min card
    lv_obj_t* card_min = make_summary_card(ui_panel_price_header, 1);
    card_label(card_min, "MIN", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_lo      = card_label(card_min, "--", UI_COLOR_GOOD, &lv_font_montserrat_18);
    ui_lbl_price_lo_time = card_label(card_min, "--:--", UI_COLOR_INK3, &lv_font_montserrat_12);

    // Avg card
    lv_obj_t* card_avg = make_summary_card(ui_panel_price_header, 1);
    card_label(card_avg, "AVG", UI_COLOR_INK3, &lv_font_montserrat_12);
    ui_lbl_price_avg = card_label(card_avg, "--", UI_COLOR_INK1, &lv_font_montserrat_18);

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

    // Title row: "15-MIN PRICES" left, legend right
    lv_obj_t* title_row = lv_obj_create(ui_panel_chart);
    lv_obj_set_width(title_row, lv_pct(100));
    lv_obj_set_height(title_row, LV_SIZE_CONTENT);
    lv_obj_remove_flag(title_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(title_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(title_row, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(title_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(title_row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_title = lv_label_create(title_row);
    lv_label_set_text(lbl_title, "15-MIN PRICES");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* legend = lv_obj_create(title_row);
    lv_obj_set_size(legend, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(legend, 0);
    lv_obj_remove_flag(legend, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(legend, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(legend, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(legend, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(legend, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(legend, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(legend, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_leg_cheap = card_label(legend, "Cheap",     UI_COLOR_GOOD, &lv_font_montserrat_12);
    ui_lbl_leg_avg   = card_label(legend, "Average",   UI_COLOR_WARN, &lv_font_montserrat_12);
    ui_lbl_leg_exp   = card_label(legend, "Expensive", UI_COLOR_BAD,  &lv_font_montserrat_12);
    ui_lbl_leg_now   = card_label(legend, "Now",       UI_COLOR_INK1, &lv_font_montserrat_12);

    // Chart widget
    ui_chart_elpris = lv_chart_create(ui_panel_chart);
    lv_obj_set_width(ui_chart_elpris, lv_pct(100));
    lv_obj_set_flex_grow(ui_chart_elpris, 1);
    lv_obj_remove_flag(ui_chart_elpris, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(ui_chart_elpris, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
    lv_obj_add_flag(ui_chart_elpris, LV_OBJ_FLAG_SEND_DRAW_TASK_EVENTS);
    lv_chart_set_type(ui_chart_elpris, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(ui_chart_elpris, 96);
    lv_chart_set_range(ui_chart_elpris, LV_CHART_AXIS_PRIMARY_Y, 0, 200);
    lv_chart_set_div_line_count(ui_chart_elpris, 5, 23);
    lv_obj_set_style_bg_color(ui_chart_elpris, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_chart_elpris, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_chart_elpris, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_chart_elpris, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_chart_elpris, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(ui_chart_elpris, UI_COLOR_LINE_SOFT,
                                LV_PART_MAIN | LV_STATE_DEFAULT);

    elpris_series = lv_chart_add_series(ui_chart_elpris, UI_COLOR_GOOD,
                                        LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_ext_y_array(ui_chart_elpris, elpris_series, elpris_data);

    lv_obj_add_event_cb(ui_chart_elpris, elpris_chart_draw_cb,
                        LV_EVENT_DRAW_TASK_ADDED, NULL);

    // X-axis scale (sibling of chart, laid out below it by flex)
    ui_chart_elpris_Xaxis = lv_scale_create(ui_panel_chart);
    lv_scale_set_mode(ui_chart_elpris_Xaxis, LV_SCALE_MODE_HORIZONTAL_BOTTOM);
    lv_obj_set_width(ui_chart_elpris_Xaxis, lv_pct(100));
    lv_obj_set_height(ui_chart_elpris_Xaxis, 22);
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
    lv_obj_set_style_pad_left(ui_chart_elpris_Xaxis, 0, 0);
    lv_obj_set_style_pad_right(ui_chart_elpris_Xaxis, 0, 0);
    lv_scale_set_range(ui_chart_elpris_Xaxis, 0, 96);
    lv_scale_set_total_tick_count(ui_chart_elpris_Xaxis, 97);
    lv_scale_set_major_tick_every(ui_chart_elpris_Xaxis, 4);
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
    lv_scale_set_range(ui_chart_elpris_Yaxis1, 0, 200);
    lv_scale_set_total_tick_count(ui_chart_elpris_Yaxis1, 13);
    lv_scale_set_major_tick_every(ui_chart_elpris_Yaxis1, 2);

}

void ui_tab_elpris_update_now(void) {
    struct tm t;
    time_t ts = time(NULL);
    localtime_r(&ts, &t);
    int new_idx = t.tm_hour * 4 + t.tm_min / 15;
    if (new_idx != g_now_index) {
        g_now_index = new_idx;
        lv_obj_invalidate(ui_chart_elpris);
        if (g_min_price > 0.0f || g_max_price > 0.0f) {
            ui_tab_elpris_update_summary(g_raw[new_idx], g_min_price, g_max_price, g_avg_price,
                                         new_idx, g_min_index, g_max_index, g_avg_index);
        }
    }
}
