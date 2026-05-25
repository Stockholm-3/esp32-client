#include "../ui.h"
#include "../ui_theme.h"
#include "../images/weather/ui_images_weather.h"
#include "display.h"
#include "ui_binder.h"
#include "cJSON.h"

#include <math.h>
#include <string.h>
#include <time.h>

lv_obj_t* ui_panel_carousel    = NULL;
lv_obj_t* ui_btn_weather_refresh = NULL;

lv_obj_t* ui_btn_day1          = NULL;
lv_obj_t* ui_lbl_day_name1     = NULL;
lv_obj_t* ui_img_day_icon1     = NULL;
lv_obj_t* ui_lbl_day_hi1       = NULL;
lv_obj_t* ui_lbl_day_lo1       = NULL;
lv_obj_t* ui_btn_day2          = NULL;
lv_obj_t* ui_lbl_day_name2     = NULL;
lv_obj_t* ui_img_day_icon2     = NULL;
lv_obj_t* ui_lbl_day_hi2       = NULL;
lv_obj_t* ui_lbl_day_lo2       = NULL;
lv_obj_t* ui_btn_day3          = NULL;
lv_obj_t* ui_lbl_day_name3     = NULL;
lv_obj_t* ui_img_day_icon3     = NULL;
lv_obj_t* ui_lbl_day_hi3       = NULL;
lv_obj_t* ui_lbl_day_lo3       = NULL;
lv_obj_t* ui_btn_day4          = NULL;
lv_obj_t* ui_lbl_day_name4     = NULL;
lv_obj_t* ui_img_day_icon4     = NULL;
lv_obj_t* ui_lbl_day_hi4       = NULL;
lv_obj_t* ui_lbl_day_lo4       = NULL;
lv_obj_t* ui_btn_day5          = NULL;
lv_obj_t* ui_lbl_day_name5     = NULL;
lv_obj_t* ui_img_day_icon5     = NULL;
lv_obj_t* ui_lbl_day_hi5       = NULL;
lv_obj_t* ui_lbl_day_lo5       = NULL;
lv_obj_t* ui_btn_day6          = NULL;
lv_obj_t* ui_lbl_day_name6     = NULL;
lv_obj_t* ui_img_day_icon6     = NULL;
lv_obj_t* ui_lbl_day_hi6       = NULL;
lv_obj_t* ui_lbl_day_lo6       = NULL;
lv_obj_t* ui_btn_day7          = NULL;
lv_obj_t* ui_lbl_day_name7     = NULL;
lv_obj_t* ui_img_day_icon7     = NULL;
lv_obj_t* ui_lbl_day_hi7       = NULL;
lv_obj_t* ui_lbl_day_lo7       = NULL;

lv_obj_t* ui_panel_detail      = NULL;
lv_obj_t* ui_lbl_detail_name   = NULL;

static const lv_image_dsc_t* weather_code_to_icon(int code) {
    if (code == 0 || code == 1 || code == 2) {
        return &UI_IMG_WX_SUNNY_PNG;
    }
    if (code >= 3 && code <= 49) {
        return &UI_IMG_WX_PARTLY_PNG;
    }
    if (code >= 60 && code <= 69) {
        return &UI_IMG_WX_SNOW_PNG;
    }
    if (code >= 50 && code <= 79) {
        return &UI_IMG_WX_CLOUDY_PNG;
    }
    if ((code >= 80 && code <= 82) || (code >= 95 && code <= 99)) {
        return &UI_IMG_WX_RAIN_PNG;
    }
    return &UI_IMG_WX_PARTLY_PNG;
}

static void weather_timestamp_to_label(const char* timestamp, char* out, size_t out_size) {
    if (!timestamp || !out || out_size == 0) {
        return;
    }
    const char* time_part = strchr(timestamp, 'T');
    if (time_part && strlen(time_part) >= 6) {
        snprintf(out, out_size, "%.*s", 5, time_part + 1);
    } else {
        snprintf(out, out_size, "%s", timestamp);
    }
}

static void weather_date_to_weekday(const char* iso_date, char* out, size_t out_size) {
    int y, mo, d;
    if (sscanf(iso_date, "%d-%d-%d", &y, &mo, &d) == 3) {
        struct tm t = {0};
        t.tm_year = y - 1900;
        t.tm_mon  = mo - 1;
        t.tm_mday = d;
        mktime(&t);
        static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(out, out_size, "%s", names[t.tm_wday]);
    } else {
        snprintf(out, out_size, "---");
    }
}

static void on_weather_refresh(lv_event_t* e) {
    (void)e;
    ui_binder_trigger_weather_refresh();
}
lv_obj_t* ui_lbl_detail_desc   = NULL;
lv_obj_t* ui_lbl_detail_hi     = NULL;
lv_obj_t* ui_lbl_detail_lo     = NULL;

#define WEATHER_HOUR_MAX 48
#define METRIC_COUNT     5

typedef enum {
    METRIC_TEMP = 0,
    METRIC_HUMIDITY,
    METRIC_PRECIP,
    METRIC_WIND,
    METRIC_PRESSURE,
} WeatherMetric;

static int32_t   g_hour_data[METRIC_COUNT][WEATHER_HOUR_MAX];
static int                g_hour_count = 0;
static lv_obj_t*          g_chart      = NULL;
static lv_chart_series_t* g_chart_ser  = NULL;
static lv_obj_t*          g_dd_metric   = NULL;
static lv_obj_t*          g_lbl_unit    = NULL;
static lv_chart_cursor_t* g_chart_cursor  = NULL;
static lv_obj_t*          g_lbl_chart_info = NULL;
static char               g_hour_times[WEATHER_HOUR_MAX][6];

static void chart_update_metric(int m) {
    if (!g_chart || !g_chart_ser || g_hour_count == 0) return;
    lv_chart_set_point_count(g_chart, (uint16_t)g_hour_count);
    int32_t mn = g_hour_data[m][0], mx = g_hour_data[m][0];
    for (int i = 1; i < g_hour_count; i++) {
        if (g_hour_data[m][i] < mn) mn = g_hour_data[m][i];
        if (g_hour_data[m][i] > mx) mx = g_hour_data[m][i];
    }
    int32_t pad = (mx - mn) / 5 + 1;
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, mn - pad, mx + pad);
    lv_chart_set_series_ext_y_array(g_chart, g_chart_ser, g_hour_data[m]);
    lv_chart_refresh(g_chart);
    static const char* const k_units[METRIC_COUNT] = {
        "\xc2\xb0""C", "%", "mm\xc3\x97""10", "km/h", "hPa"
    };
    if (g_lbl_unit) lv_label_set_text(g_lbl_unit, k_units[m]);
}

static void on_metric_changed(lv_event_t* e) {
    (void)e;
    chart_update_metric((int)lv_dropdown_get_selected(g_dd_metric));
}

static void on_chart_pressed(lv_event_t* e) {
    (void)e;
    if (g_hour_count == 0) return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev) return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    lv_area_t coords;
    lv_obj_get_coords(g_chart, &coords);
    int32_t chart_w = lv_area_get_width(&coords);
    if (chart_w <= 0) return;

    int32_t rel_x = pt.x - coords.x1;
    rel_x = LV_CLAMP(0, rel_x, chart_w - 1);
    uint32_t idx = (uint32_t)((int64_t)rel_x * g_hour_count / chart_w);
    if ((int)idx >= g_hour_count) idx = (uint32_t)(g_hour_count - 1);

    static uint32_t s_last_idx = UINT32_MAX;
    if (idx == s_last_idx) return;
    s_last_idx = idx;

    lv_chart_set_cursor_point(g_chart, g_chart_cursor, g_chart_ser, idx);

    int m = g_dd_metric ? (int)lv_dropdown_get_selected(g_dd_metric) : METRIC_TEMP;
    static const char* const k_units[METRIC_COUNT] = {
        "\xc2\xb0""C", "%", "mm", "km/h", "hPa"
    };
    int32_t raw = g_hour_data[m][idx];
    char buf[32];
    if (m == METRIC_PRECIP)
        snprintf(buf, sizeof(buf), "%s  %.1f %s", g_hour_times[idx], raw / 10.0, k_units[m]);
    else if (m == METRIC_TEMP)
        snprintf(buf, sizeof(buf), "%s  %+d%s", g_hour_times[idx], (int)raw, k_units[m]);
    else
        snprintf(buf, sizeof(buf), "%s  %d %s", g_hour_times[idx], (int)raw, k_units[m]);

    if (g_lbl_chart_info) lv_label_set_text(g_lbl_chart_info, buf);
}

void ui_tab_weather_handle_server_response(const char* json, size_t len) {
    if (!json || len == 0) {
        return;
    }

    cJSON* root = cJSON_ParseWithLength(json, len);
    if (!root) {
        return;
    }

    cJSON* data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!data) {
        cJSON_Delete(root);
        return;
    }
    cJSON* forecast = cJSON_GetObjectItemCaseSensitive(data, "hourly_forecast");
    if (!cJSON_IsArray(forecast)) {
        cJSON_Delete(root);
        return;
    }

    int count = cJSON_GetArraySize(forecast);
    if (count <= 0) {
        cJSON_Delete(root);
        return;
    }

    // ── Day aggregation (outside LVGL lock) ──────────────────────────────────
    typedef struct {
        char   date[11];
        double min_temp;
        double max_temp;
        int    weather_code;
        bool   valid;
    } DaySummary;

    DaySummary days[7];
    for (int i = 0; i < 7; i++) {
        days[i].min_temp     = 1e6;
        days[i].max_temp     = -1e6;
        days[i].weather_code = -1;
        days[i].valid        = false;
        days[i].date[0]      = '\0';
    }
    int day_count = 0;

    const char* current_unit = "";
    cJSON* first_item = cJSON_GetArrayItem(forecast, 0);
    if (cJSON_IsObject(first_item)) {
        cJSON* u = cJSON_GetObjectItemCaseSensitive(first_item, "temperature_unit");
        if (cJSON_IsString(u)) current_unit = u->valuestring;
    }

    for (int i = 0; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(forecast, i);
        if (!cJSON_IsObject(item)) continue;
        cJSON* time_obj = cJSON_GetObjectItemCaseSensitive(item, "time");
        cJSON* temp_obj = cJSON_GetObjectItemCaseSensitive(item, "temperature");
        cJSON* code_obj = cJSON_GetObjectItemCaseSensitive(item, "weather_code");
        if (!cJSON_IsString(time_obj) || strlen(time_obj->valuestring) < 10) continue;
        const char* ts = time_obj->valuestring;

        int di = -1;
        for (int j = 0; j < day_count; j++) {
            if (strncmp(days[j].date, ts, 10) == 0) { di = j; break; }
        }
        if (di < 0 && day_count < 7) {
            di = day_count++;
            strncpy(days[di].date, ts, 10);
            days[di].date[10] = '\0';
            days[di].valid    = true;
        }
        if (di < 0) continue;

        double temp = cJSON_IsNumber(temp_obj) ? temp_obj->valuedouble : NAN;
        if (!isnan(temp)) {
            if (temp < days[di].min_temp) days[di].min_temp = temp;
            if (temp > days[di].max_temp) days[di].max_temp = temp;
        }
        int code = cJSON_IsNumber(code_obj) ? code_obj->valueint : -1;
        if (code >= 0) {
            if (strlen(ts) >= 13 && ts[11] == '1' && ts[12] == '2')
                days[di].weather_code = code;
            else if (days[di].weather_code < 0)
                days[di].weather_code = code;
        }
    }

    // ── Fill chart data arrays ───────────────────────────────────────────────
    int hmax = count < WEATHER_HOUR_MAX ? count : WEATHER_HOUR_MAX;
    const char* current_time = NULL;
    for (int i = 0; i < hmax; i++) {
        cJSON* item     = cJSON_GetArrayItem(forecast, i);
        if (!cJSON_IsObject(item)) { hmax = i; break; }
        cJSON* time_obj  = cJSON_GetObjectItemCaseSensitive(item, "time");
        cJSON* temp_obj  = cJSON_GetObjectItemCaseSensitive(item, "temperature");
        cJSON* hum_obj   = cJSON_GetObjectItemCaseSensitive(item, "humidity");
        cJSON* prec_obj  = cJSON_GetObjectItemCaseSensitive(item, "precipitation");
        cJSON* wind_obj  = cJSON_GetObjectItemCaseSensitive(item, "windspeed");
        cJSON* press_obj = cJSON_GetObjectItemCaseSensitive(item, "pressure");

        g_hour_data[METRIC_TEMP][i]     = cJSON_IsNumber(temp_obj)  ? (int32_t)round(temp_obj->valuedouble)       : 0;
        g_hour_data[METRIC_HUMIDITY][i] = cJSON_IsNumber(hum_obj)   ? (int32_t)round(hum_obj->valuedouble)        : 0;
        g_hour_data[METRIC_PRECIP][i]   = cJSON_IsNumber(prec_obj)  ? (int32_t)round(prec_obj->valuedouble * 10)  : 0;
        g_hour_data[METRIC_WIND][i]     = cJSON_IsNumber(wind_obj)  ? (int32_t)round(wind_obj->valuedouble)       : 0;
        g_hour_data[METRIC_PRESSURE][i] = cJSON_IsNumber(press_obj) ? (int32_t)round(press_obj->valuedouble)      : 0;

        if (cJSON_IsString(time_obj))
            weather_timestamp_to_label(time_obj->valuestring, g_hour_times[i], sizeof(g_hour_times[i]));
        else
            g_hour_times[i][0] = '\0';

        if (i == 0) current_time = cJSON_IsString(time_obj) ? time_obj->valuestring : NULL;
    }
    g_hour_count = hmax;

    // ── LVGL update ──────────────────────────────────────────────────────────
    lv_obj_t* day_name_arr[7] = {ui_lbl_day_name1, ui_lbl_day_name2, ui_lbl_day_name3,
                                 ui_lbl_day_name4, ui_lbl_day_name5, ui_lbl_day_name6,
                                 ui_lbl_day_name7};
    lv_obj_t* day_hi_arr[7]   = {ui_lbl_day_hi1, ui_lbl_day_hi2, ui_lbl_day_hi3,
                                  ui_lbl_day_hi4, ui_lbl_day_hi5, ui_lbl_day_hi6,
                                  ui_lbl_day_hi7};
    lv_obj_t* day_lo_arr[7]   = {ui_lbl_day_lo1, ui_lbl_day_lo2, ui_lbl_day_lo3,
                                  ui_lbl_day_lo4, ui_lbl_day_lo5, ui_lbl_day_lo6,
                                  ui_lbl_day_lo7};
    lv_obj_t* day_icon_arr[7] = {ui_img_day_icon1, ui_img_day_icon2, ui_img_day_icon3,
                                 ui_img_day_icon4, ui_img_day_icon5, ui_img_day_icon6,
                                 ui_img_day_icon7};

    if (!display_lvgl_lock(100)) {
        cJSON_Delete(root);
        return;
    }

    // 7-day carousel
    for (int i = 0; i < 7; i++) {
        if (!days[i].valid) break;
        char wday[8];
        weather_date_to_weekday(days[i].date, wday, sizeof(wday));
        char hi_buf[16], lo_buf[16];
        snprintf(hi_buf, sizeof(hi_buf), "%+g%s", days[i].max_temp, current_unit);
        snprintf(lo_buf, sizeof(lo_buf), "%+g%s", days[i].min_temp, current_unit);
        if (day_name_arr[i]) lv_label_set_text(day_name_arr[i], wday);
        if (day_hi_arr[i])   lv_label_set_text(day_hi_arr[i], hi_buf);
        if (day_lo_arr[i])   lv_label_set_text(day_lo_arr[i], lo_buf);
        if (day_icon_arr[i]) lv_image_set_src(day_icon_arr[i],
                                 weather_code_to_icon(days[i].weather_code));
    }

    // Chart
    int sel = g_dd_metric ? (int)lv_dropdown_get_selected(g_dd_metric) : METRIC_TEMP;
    chart_update_metric(sel);

    // Detail panel — current conditions + today's min/max
    const char* current_desc = NULL;
    if (cJSON_IsObject(first_item)) {
        cJSON* desc_obj = cJSON_GetObjectItemCaseSensitive(first_item, "weather_description");
        if (cJSON_IsString(desc_obj)) current_desc = desc_obj->valuestring;
    }

    char name_buf[8] = "---";
    if (days[0].valid) weather_date_to_weekday(days[0].date, name_buf, sizeof(name_buf));
    lv_label_set_text(ui_lbl_detail_name, name_buf);
    lv_label_set_text(ui_lbl_detail_desc, current_desc ? current_desc : "-");

    if (days[0].valid) {
        char hi_buf[32], lo_buf[32];
        snprintf(hi_buf, sizeof(hi_buf), "Max %+g%s", days[0].max_temp, current_unit);
        snprintf(lo_buf, sizeof(lo_buf), "Min %+g%s", days[0].min_temp, current_unit);
        lv_label_set_text(ui_lbl_detail_hi, hi_buf);
        lv_label_set_text(ui_lbl_detail_lo, lo_buf);
    }

    display_lvgl_unlock();
    cJSON_Delete(root);
}

static lv_obj_t* make_day_btn(lv_obj_t* parent, const char* name,
                               const lv_image_dsc_t* icon, const char* hi, const char* lo,
                               lv_obj_t** out_name, lv_obj_t** out_img,
                               lv_obj_t** out_hi, lv_obj_t** out_lo) {
    lv_obj_t* btn = lv_obj_create(parent);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, LV_PCT(100));
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(btn, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(btn, UI_COLOR_ACCENT, LV_PART_MAIN | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(btn, UI_COLOR_ACCENT, LV_PART_MAIN | LV_STATE_CHECKED);

    *out_name = lv_label_create(btn);
    lv_label_set_text(*out_name, name);
    lv_obj_set_style_text_color(*out_name, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_name, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    *out_img = lv_image_create(btn);
    lv_image_set_src(*out_img, icon);
    lv_image_set_scale(*out_img, 128);
    lv_obj_set_size(*out_img, 32, 32);

    *out_hi = lv_label_create(btn);
    lv_label_set_text(*out_hi, hi);
    lv_obj_set_style_text_color(*out_hi, UI_COLOR_BAD, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_hi, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    *out_lo = lv_label_create(btn);
    lv_label_set_text(*out_lo, lo);
    lv_obj_set_style_text_color(*out_lo, UI_COLOR_HUM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_lo, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    return btn;
}


void ui_tab_weather_init(void) {
    // ── Body ─────────────────────────────────────────────────────────────────
    lv_obj_t* body = lv_obj_create(ui_tabweather);
    lv_obj_set_size(body, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(body, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(body, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(body, 56, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(body, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    // ── Day strip ─────────────────────────────────────────────────────────────
    ui_panel_carousel = lv_obj_create(body);
    lv_obj_set_width(ui_panel_carousel, lv_pct(100));
    lv_obj_set_height(ui_panel_carousel, 130);
    lv_obj_remove_flag(ui_panel_carousel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_panel_carousel, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_carousel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_carousel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_panel_carousel, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(ui_panel_carousel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_panel_carousel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    static const char* day_names[] = {"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    static const char* day_his[]   = {"+15\xc2\xb0", "+17\xc2\xb0", "+12\xc2\xb0", "+9\xc2\xb0",
                                      "+11\xc2\xb0", "+14\xc2\xb0", "+16\xc2\xb0"};
    static const char* day_los[]   = {"+8\xc2\xb0", "+9\xc2\xb0", "+6\xc2\xb0", "+4\xc2\xb0",
                                      "+5\xc2\xb0", "+7\xc2\xb0", "+8\xc2\xb0"};
    static const lv_image_dsc_t* day_icons[] = {
        &UI_IMG_WX_PARTLY_PNG, &UI_IMG_WX_SUNNY_PNG,  &UI_IMG_WX_CLOUDY_PNG,
        &UI_IMG_WX_RAIN_PNG,   &UI_IMG_WX_CLOUDY_PNG, &UI_IMG_WX_PARTLY_PNG,
        &UI_IMG_WX_SUNNY_PNG,
    };
    lv_obj_t** btn_arr[]      = {&ui_btn_day1, &ui_btn_day2, &ui_btn_day3, &ui_btn_day4,
                                  &ui_btn_day5, &ui_btn_day6, &ui_btn_day7};
    lv_obj_t** name_arr[]     = {&ui_lbl_day_name1, &ui_lbl_day_name2, &ui_lbl_day_name3,
                                  &ui_lbl_day_name4, &ui_lbl_day_name5, &ui_lbl_day_name6,
                                  &ui_lbl_day_name7};
    lv_obj_t** icon_arr[]     = {&ui_img_day_icon1, &ui_img_day_icon2, &ui_img_day_icon3,
                                  &ui_img_day_icon4, &ui_img_day_icon5, &ui_img_day_icon6,
                                  &ui_img_day_icon7};
    lv_obj_t** hi_arr[]       = {&ui_lbl_day_hi1, &ui_lbl_day_hi2, &ui_lbl_day_hi3,
                                  &ui_lbl_day_hi4, &ui_lbl_day_hi5, &ui_lbl_day_hi6,
                                  &ui_lbl_day_hi7};
    lv_obj_t** lo_arr[]       = {&ui_lbl_day_lo1, &ui_lbl_day_lo2, &ui_lbl_day_lo3,
                                  &ui_lbl_day_lo4, &ui_lbl_day_lo5, &ui_lbl_day_lo6,
                                  &ui_lbl_day_lo7};

    for (int i = 0; i < 7; i++) {
        *btn_arr[i] = make_day_btn(ui_panel_carousel, day_names[i], day_icons[i],
                                    day_his[i], day_los[i],
                                    name_arr[i], icon_arr[i], hi_arr[i], lo_arr[i]);
    }
    lv_obj_add_state(ui_btn_day1, LV_STATE_CHECKED);

    // ── Detail row ────────────────────────────────────────────────────────────
    ui_panel_detail = lv_obj_create(body);
    lv_obj_set_width(ui_panel_detail, lv_pct(100));
    lv_obj_set_height(ui_panel_detail, LV_SIZE_CONTENT);
    lv_obj_remove_flag(ui_panel_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_panel_detail, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_detail, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_detail, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_panel_detail, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(ui_panel_detail, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_panel_detail, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* detail_left = lv_obj_create(ui_panel_detail);
    lv_obj_set_flex_grow(detail_left, 1);
    lv_obj_set_height(detail_left, LV_SIZE_CONTENT);
    lv_obj_remove_flag(detail_left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(detail_left, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(detail_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(detail_left, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(detail_left, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(detail_left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(detail_left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    ui_lbl_detail_name = lv_label_create(detail_left);
    lv_label_set_text(ui_lbl_detail_name, "Monday");
    lv_obj_set_style_text_color(ui_lbl_detail_name, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_name, &lv_font_montserrat_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_detail_desc = lv_label_create(detail_left);
    lv_label_set_text(ui_lbl_detail_desc, "Partly cloudy");
    lv_obj_set_style_text_color(ui_lbl_detail_desc, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_desc, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* detail_right = lv_obj_create(ui_panel_detail);
    lv_obj_set_width(detail_right, LV_SIZE_CONTENT);
    lv_obj_set_height(detail_right, LV_SIZE_CONTENT);
    lv_obj_remove_flag(detail_right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(detail_right, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(detail_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(detail_right, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(detail_right, 18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(detail_right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(detail_right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    ui_lbl_detail_hi = lv_label_create(detail_right);
    lv_label_set_text(ui_lbl_detail_hi, "Max +15\xc2\xb0");
    lv_obj_set_style_text_color(ui_lbl_detail_hi, UI_COLOR_BAD, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_hi, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_detail_lo = lv_label_create(detail_right);
    lv_label_set_text(ui_lbl_detail_lo, "Min +8\xc2\xb0");
    lv_obj_set_style_text_color(ui_lbl_detail_lo, UI_COLOR_HUM, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_lo, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Hourly card ───────────────────────────────────────────────────────────
    lv_obj_t* hourly_card = lv_obj_create(body);
    lv_obj_set_flex_grow(hourly_card, 1);
    lv_obj_set_width(hourly_card, lv_pct(100));
    lv_obj_set_style_margin_bottom(hourly_card, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_remove_flag(hourly_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hourly_card, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(hourly_card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(hourly_card, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(hourly_card, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(hourly_card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(hourly_card, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(hourly_card, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(hourly_card, LV_FLEX_FLOW_COLUMN);

    // ── Header: title + metric dropdown ──────────────────────────────────────
    lv_obj_t* hdr = lv_obj_create(hourly_card);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, LV_SIZE_CONTENT);
    lv_obj_remove_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(hdr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(hdr, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl_title = lv_label_create(hdr);
    lv_label_set_text(lbl_title, "HOURLY FORECAST");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_lbl_unit = lv_label_create(hdr);
    lv_label_set_text(g_lbl_unit, "\xc2\xb0""C");
    lv_obj_set_style_text_color(g_lbl_unit, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_lbl_unit, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_dd_metric = lv_dropdown_create(hdr);
    lv_dropdown_set_options(g_dd_metric, "Temperature\nHumidity\nPrecipitation\nWind\nPressure");
    lv_obj_set_width(g_dd_metric, 120);
    lv_obj_set_style_text_font(g_dd_metric, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_dd_metric, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_dd_metric, UI_COLOR_LINE_SOFT,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_dd_metric, on_metric_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Chart ─────────────────────────────────────────────────────────────────
    g_chart = lv_chart_create(hourly_card);
    lv_obj_set_flex_grow(g_chart, 1);
    lv_obj_set_width(g_chart, lv_pct(100));
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, WEATHER_HOUR_MAX);
    lv_chart_set_div_line_count(g_chart, 5, 7);
    lv_obj_set_style_size(g_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(g_chart, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_chart_ser = lv_chart_add_series(g_chart, UI_COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    g_chart_cursor = lv_chart_add_cursor(g_chart, UI_COLOR_ACCENT, LV_DIR_VER);
    lv_obj_add_event_cb(g_chart, on_chart_pressed, LV_EVENT_PRESSING, NULL);

    g_lbl_chart_info = lv_label_create(hourly_card);
    lv_label_set_text(g_lbl_chart_info, "---");
    lv_obj_set_style_text_font(g_lbl_chart_info, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_lbl_chart_info, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Refresh button — 10 px above bottom edge ──────────────────────────────
    ui_btn_weather_refresh = lv_btn_create(ui_tabweather);
    lv_obj_add_flag(ui_btn_weather_refresh, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_btn_weather_refresh, 120, 36);
    lv_obj_align(ui_btn_weather_refresh, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    lv_obj_set_style_radius(ui_btn_weather_refresh, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btn_weather_refresh, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_btn_weather_refresh, UI_COLOR_LINE_SOFT,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_btn_weather_refresh, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* refresh_label = lv_label_create(ui_btn_weather_refresh);
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_set_style_text_color(refresh_label, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_btn_weather_refresh, on_weather_refresh, LV_EVENT_CLICKED, NULL);
}
