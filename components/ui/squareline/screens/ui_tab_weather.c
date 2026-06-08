#include "../images/weather/ui_images_weather.h"
#include "../ui.h"
#include "../ui_theme.h"
#include "cJSON.h"
#include "ui_binder.h"
#include "ui_scr_home.h"

#include <math.h>
#include <string.h>
#include <time.h>

lv_obj_t* ui_panel_carousel      = NULL;
lv_obj_t* ui_btn_weather_refresh = NULL;

lv_obj_t* ui_btn_day1      = NULL;
lv_obj_t* ui_lbl_day_name1 = NULL;
lv_obj_t* ui_img_day_icon1 = NULL;
lv_obj_t* ui_lbl_day_hi1   = NULL;
lv_obj_t* ui_lbl_day_lo1   = NULL;
lv_obj_t* ui_btn_day2      = NULL;
lv_obj_t* ui_lbl_day_name2 = NULL;
lv_obj_t* ui_img_day_icon2 = NULL;
lv_obj_t* ui_lbl_day_hi2   = NULL;
lv_obj_t* ui_lbl_day_lo2   = NULL;
lv_obj_t* ui_btn_day3      = NULL;
lv_obj_t* ui_lbl_day_name3 = NULL;
lv_obj_t* ui_img_day_icon3 = NULL;
lv_obj_t* ui_lbl_day_hi3   = NULL;
lv_obj_t* ui_lbl_day_lo3   = NULL;
lv_obj_t* ui_btn_day4      = NULL;
lv_obj_t* ui_lbl_day_name4 = NULL;
lv_obj_t* ui_img_day_icon4 = NULL;
lv_obj_t* ui_lbl_day_hi4   = NULL;
lv_obj_t* ui_lbl_day_lo4   = NULL;
lv_obj_t* ui_btn_day5      = NULL;
lv_obj_t* ui_lbl_day_name5 = NULL;
lv_obj_t* ui_img_day_icon5 = NULL;
lv_obj_t* ui_lbl_day_hi5   = NULL;
lv_obj_t* ui_lbl_day_lo5   = NULL;
lv_obj_t* ui_btn_day6      = NULL;
lv_obj_t* ui_lbl_day_name6 = NULL;
lv_obj_t* ui_img_day_icon6 = NULL;
lv_obj_t* ui_lbl_day_hi6   = NULL;
lv_obj_t* ui_lbl_day_lo6   = NULL;
lv_obj_t* ui_btn_day7      = NULL;
lv_obj_t* ui_lbl_day_name7 = NULL;
lv_obj_t* ui_img_day_icon7 = NULL;
lv_obj_t* ui_lbl_day_hi7   = NULL;
lv_obj_t* ui_lbl_day_lo7   = NULL;

lv_obj_t* ui_panel_detail    = NULL;
lv_obj_t* ui_lbl_detail_name = NULL;

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
        t.tm_year   = y - 1900;
        t.tm_mon    = mo - 1;
        t.tm_mday   = d;
        mktime(&t);
        static const char* names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        snprintf(out, out_size, "%s", names[t.tm_wday]);
    } else {
        snprintf(out, out_size, "---");
    }
}

lv_obj_t* ui_lbl_detail_desc = NULL;
lv_obj_t* ui_lbl_detail_hi   = NULL;
lv_obj_t* ui_lbl_detail_lo   = NULL;

typedef struct {
    char wday[8];
    char hi[24];
    char lo[24];
    char desc[64];
    bool valid;
} DayDisplay;

static DayDisplay g_day_display[7];
static lv_obj_t* g_refresh_label = NULL;

static void on_weather_refresh(lv_event_t* e) {
    (void)e;
    if (g_refresh_label)
        lv_label_set_text(g_refresh_label, "Refreshing");
    lv_obj_add_state(ui_btn_weather_refresh, LV_STATE_DISABLED);
    ui_binder_trigger_weather_refresh();
}

#define WEATHER_HOUR_MAX 96
#define WEATHER_HR_MAX 200
#define METRIC_COUNT 5

typedef enum {
    METRIC_TEMP = 0,
    METRIC_HUMIDITY,
    METRIC_PRECIP,
    METRIC_WIND,
    METRIC_PRESSURE,
} WeatherMetric;

static int32_t g_hour_data[METRIC_COUNT][WEATHER_HOUR_MAX];
static int g_hour_count = 0;
static char g_hour_times[WEATHER_HOUR_MAX][6];

static int32_t g_min_data[METRIC_COUNT][WEATHER_HOUR_MAX];
static char g_min_times[WEATHER_HOUR_MAX][6];
static int g_min_count = 0;
static int g_min_codes[WEATHER_HOUR_MAX];
static char g_min_descs[WEATHER_HOUR_MAX][48];
static int32_t g_min_feels[WEATHER_HOUR_MAX];
static char g_sunrise_time[6] = "";
static char g_sunset_time[6]  = "";

static int32_t g_hr_data[METRIC_COUNT][WEATHER_HR_MAX];
static char g_hr_times[WEATHER_HR_MAX][6];
static int g_hr_count = 0;
static int g_day_hr_start[7];
static int g_day_hr_count[7];

static lv_obj_t* g_chart                 = NULL;
static lv_chart_series_t* g_chart_ser    = NULL;
static lv_obj_t* g_dd_metric             = NULL;
static lv_obj_t* g_lbl_unit              = NULL;
static lv_chart_cursor_t* g_chart_cursor = NULL;
static lv_obj_t* g_lbl_chart_info        = NULL;

static void chart_update_metric(int m) {
    if (!g_chart || !g_chart_ser || g_hour_count == 0)
        return;
    lv_chart_set_point_count(g_chart, (uint16_t)g_hour_count);
    int32_t mn = g_hour_data[m][0], mx = g_hour_data[m][0];
    for (int i = 1; i < g_hour_count; i++) {
        if (g_hour_data[m][i] < mn)
            mn = g_hour_data[m][i];
        if (g_hour_data[m][i] > mx)
            mx = g_hour_data[m][i];
    }
    int32_t range = mx - mn;
    int32_t pad   = range / 4 + 3;
    lv_chart_set_axis_range(g_chart, LV_CHART_AXIS_PRIMARY_Y, mn - pad, mx + pad);
    lv_chart_set_series_ext_y_array(g_chart, g_chart_ser, g_hour_data[m]);
    lv_chart_refresh(g_chart);
    static const char* const k_units[METRIC_COUNT] = {"\xc2\xb0"
                                                      "C",
                                                      "%",
                                                      "mm\xc3\x97"
                                                      "10",
                                                      "km/h", "hPa"};
    if (g_lbl_unit)
        lv_label_set_text(g_lbl_unit, k_units[m]);
}

static void on_day_btn_clicked(lv_event_t* e) {
    lv_obj_t* btn    = lv_event_get_target(e);
    lv_obj_t* parent = lv_obj_get_parent(btn);
    uint32_t cnt     = lv_obj_get_child_count(parent);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(parent, i);
        if (child != btn)
            lv_obj_remove_state(child, LV_STATE_CHECKED);
    }
    lv_obj_add_state(btn, LV_STATE_CHECKED);

    uint32_t idx = lv_obj_get_index(btn);
    if (idx < 7 && g_day_display[idx].valid) {
        lv_label_set_text(ui_lbl_detail_name, g_day_display[idx].wday);
        lv_label_set_text(ui_lbl_detail_hi, g_day_display[idx].hi);
        lv_label_set_text(ui_lbl_detail_lo, g_day_display[idx].lo);
        lv_label_set_text(ui_lbl_detail_desc,
                          g_day_display[idx].desc[0] ? g_day_display[idx].desc : "-");
    }

    if (idx == 0) {
        for (int m = 0; m < METRIC_COUNT; m++)
            memcpy(g_hour_data[m], g_min_data[m], sizeof(int32_t) * (size_t)g_min_count);
        memcpy(g_hour_times, g_min_times, sizeof(g_hour_times[0]) * (size_t)g_min_count);
        g_hour_count = g_min_count;
    } else if (idx < 7 && g_day_hr_start[idx] >= 0 && g_day_hr_count[idx] > 0) {
        int start   = g_day_hr_start[idx];
        int day_cnt = g_day_hr_count[idx];
        if (day_cnt > WEATHER_HOUR_MAX)
            day_cnt = WEATHER_HOUR_MAX;
        for (int m = 0; m < METRIC_COUNT; m++)
            memcpy(g_hour_data[m], &g_hr_data[m][start], sizeof(int32_t) * (size_t)day_cnt);
        memcpy(g_hour_times, &g_hr_times[start], sizeof(g_hour_times[0]) * (size_t)day_cnt);
        g_hour_count = day_cnt;
    }
    int sel = g_dd_metric ? (int)lv_dropdown_get_selected(g_dd_metric) : METRIC_TEMP;
    chart_update_metric(sel);
}

static void on_metric_changed(lv_event_t* e) {
    (void)e;
    chart_update_metric((int)lv_dropdown_get_selected(g_dd_metric));
}

static void on_chart_pressed(lv_event_t* e) {
    (void)e;
    if (g_hour_count == 0)
        return;

    lv_indev_t* indev = lv_indev_active();
    if (!indev)
        return;
    lv_point_t pt;
    lv_indev_get_point(indev, &pt);

    lv_area_t coords;
    lv_obj_get_coords(g_chart, &coords);
    int32_t chart_w = lv_area_get_width(&coords);
    if (chart_w <= 0)
        return;

    int32_t rel_x = pt.x - coords.x1;
    rel_x         = LV_CLAMP(0, rel_x, chart_w - 1);
    uint32_t idx  = (uint32_t)((int64_t)rel_x * g_hour_count / chart_w);
    if ((int)idx >= g_hour_count)
        idx = (uint32_t)(g_hour_count - 1);

    static uint32_t s_last_idx = UINT32_MAX;
    if (idx == s_last_idx)
        return;
    s_last_idx = idx;

    lv_chart_set_cursor_point(g_chart, g_chart_cursor, g_chart_ser, idx);

    int m = g_dd_metric ? (int)lv_dropdown_get_selected(g_dd_metric) : METRIC_TEMP;
    static const char* const k_units[METRIC_COUNT] = {"\xc2\xb0"
                                                      "C",
                                                      "%", "mm", "km/h", "hPa"};
    int32_t raw                                    = g_hour_data[m][idx];
    char buf[32];
    if (m == METRIC_PRECIP)
        snprintf(buf, sizeof(buf), "%s  %.1f %s", g_hour_times[idx], raw / 10.0, k_units[m]);
    else if (m == METRIC_TEMP)
        snprintf(buf, sizeof(buf), "%s  %+d%s", g_hour_times[idx], (int)raw, k_units[m]);
    else
        snprintf(buf, sizeof(buf), "%s  %d %s", g_hour_times[idx], (int)raw, k_units[m]);

    if (g_lbl_chart_info)
        lv_label_set_text(g_lbl_chart_info, buf);
}

void ui_tab_weather_handle_server_response(const char* json, size_t len, int skip_days) {
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
    if (!cJSON_IsArray(forecast))
        forecast = cJSON_GetObjectItemCaseSensitive(data, "minutely_forecast");
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
        char date[11];
        double min_temp;
        double max_temp;
        int weather_code;
        bool valid;
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
    cJSON* first_item        = cJSON_GetArrayItem(forecast, 0);
    if (cJSON_IsObject(first_item)) {
        cJSON* u = cJSON_GetObjectItemCaseSensitive(first_item, "temperature_unit");
        if (cJSON_IsString(u))
            current_unit = u->valuestring;
    }

    for (int i = skip_days * 24; i < count; i++) {
        cJSON* item = cJSON_GetArrayItem(forecast, i);
        if (!cJSON_IsObject(item))
            continue;
        cJSON* time_obj = cJSON_GetObjectItemCaseSensitive(item, "time");
        cJSON* temp_obj = cJSON_GetObjectItemCaseSensitive(item, "temperature");
        cJSON* code_obj = cJSON_GetObjectItemCaseSensitive(item, "weather_code");
        if (!cJSON_IsString(time_obj) || strlen(time_obj->valuestring) < 10)
            continue;
        const char* ts = time_obj->valuestring;

        int di = -1;
        for (int j = 0; j < day_count; j++) {
            if (strncmp(days[j].date, ts, 10) == 0) {
                di = j;
                break;
            }
        }
        if (di < 0 && day_count < 7) {
            di = day_count++;
            strncpy(days[di].date, ts, 10);
            days[di].date[10] = '\0';
            days[di].valid    = true;
        }
        if (di < 0)
            continue;

        double temp = cJSON_IsNumber(temp_obj) ? temp_obj->valuedouble : NAN;
        if (!isnan(temp)) {
            if (temp < days[di].min_temp)
                days[di].min_temp = temp;
            if (temp > days[di].max_temp)
                days[di].max_temp = temp;
        }
        int code = cJSON_IsNumber(code_obj) ? code_obj->valueint : -1;
        if (code >= 0) {
            if (strlen(ts) >= 13 && ts[11] == '1' && ts[12] == '2')
                days[di].weather_code = code;
            else if (days[di].weather_code < 0)
                days[di].weather_code = code;
        }
    }

    // ── Determine today's date ───────────────────────────────────────────────
    // Fall back to the forecast midpoint when NTP is not yet synced (year < 2020).
    char today[11] = "";
    if (skip_days == 0) {
        time_t now_t      = time(NULL);
        struct tm* tm_now = localtime(&now_t);
        if (tm_now->tm_year + 1900 >= 2020) {
            strftime(today, sizeof(today), "%Y-%m-%d", tm_now);
        } else {
            cJSON* mid = cJSON_GetArrayItem(forecast, count / 2);
            if (cJSON_IsObject(mid)) {
                cJSON* t = cJSON_GetObjectItemCaseSensitive(mid, "time");
                if (cJSON_IsString(t))
                    strncpy(today, t->valuestring, 10);
            }
        }
    }

    // ── Fill chart data arrays ───────────────────────────────────────────────
    if (skip_days == 0) {
        // weather_min → store only today's entries in g_min_data

        int n             = 0;
        int prev_is_day   = -1;
        g_sunrise_time[0] = '\0';
        g_sunset_time[0]  = '\0';
        for (int i = 0; i < count && n < WEATHER_HOUR_MAX; i++) {
            cJSON* item = cJSON_GetArrayItem(forecast, i);
            if (!cJSON_IsObject(item))
                break;
            cJSON* time_obj = cJSON_GetObjectItemCaseSensitive(item, "time");
            if (!cJSON_IsString(time_obj))
                continue;
            const char* ts2 = time_obj->valuestring;
            if (strncmp(ts2, today, 10) != 0)
                continue; // skip other days

            cJSON* temp_obj  = cJSON_GetObjectItemCaseSensitive(item, "temperature");
            cJSON* hum_obj   = cJSON_GetObjectItemCaseSensitive(item, "humidity");
            cJSON* prec_obj  = cJSON_GetObjectItemCaseSensitive(item, "precipitation");
            cJSON* wind_obj  = cJSON_GetObjectItemCaseSensitive(item, "windspeed");
            cJSON* press_obj = cJSON_GetObjectItemCaseSensitive(item, "pressure");
            cJSON* code2_obj = cJSON_GetObjectItemCaseSensitive(item, "weather_code");
            cJSON* desc2_obj = cJSON_GetObjectItemCaseSensitive(item, "weather_description");
            cJSON* feel_obj  = cJSON_GetObjectItemCaseSensitive(item, "apparent_temperature");
            cJSON* day_obj   = cJSON_GetObjectItemCaseSensitive(item, "is_day");
            int is_day_val   = cJSON_IsNumber(day_obj) ? day_obj->valueint : 1;

            if (prev_is_day >= 0 && is_day_val != prev_is_day) {
                if (is_day_val == 1)
                    weather_timestamp_to_label(ts2, g_sunrise_time, sizeof(g_sunrise_time));
                else
                    weather_timestamp_to_label(ts2, g_sunset_time, sizeof(g_sunset_time));
            }
            prev_is_day = is_day_val;

            g_min_data[METRIC_TEMP][n] =
                cJSON_IsNumber(temp_obj) ? (int32_t)round(temp_obj->valuedouble) : 0;
            g_min_data[METRIC_HUMIDITY][n] =
                cJSON_IsNumber(hum_obj) ? (int32_t)round(hum_obj->valuedouble) : 0;
            g_min_data[METRIC_PRECIP][n] =
                cJSON_IsNumber(prec_obj) ? (int32_t)round(prec_obj->valuedouble * 10) : 0;
            g_min_data[METRIC_WIND][n] =
                cJSON_IsNumber(wind_obj) ? (int32_t)round(wind_obj->valuedouble) : 0;
            g_min_data[METRIC_PRESSURE][n] =
                cJSON_IsNumber(press_obj) ? (int32_t)round(press_obj->valuedouble) : 0;
            g_min_codes[n] = cJSON_IsNumber(code2_obj) ? code2_obj->valueint : 0;
            g_min_feels[n] = cJSON_IsNumber(feel_obj) ? (int32_t)round(feel_obj->valuedouble)
                                                      : g_min_data[METRIC_TEMP][n];
            if (cJSON_IsString(desc2_obj))
                snprintf(g_min_descs[n], sizeof(g_min_descs[n]), "%s", desc2_obj->valuestring);
            else
                g_min_descs[n][0] = '\0';
            weather_timestamp_to_label(ts2, g_min_times[n], sizeof(g_min_times[n]));
            n++;
        }
        g_min_count = n;
        for (int m = 0; m < METRIC_COUNT; m++)
            memcpy(g_hour_data[m], g_min_data[m], sizeof(int32_t) * (size_t)n);
        memcpy(g_hour_times, g_min_times, sizeof(g_hour_times[0]) * (size_t)n);
        g_hour_count = n;
    } else {
        // weather_hr → store in g_hr_data, track day boundaries per carousel slot
        for (int s = 0; s < 7; s++) {
            g_day_hr_start[s] = -1;
            g_day_hr_count[s] = 0;
        }
        g_hr_count         = 0;
        char prev_date[11] = "";
        int slot_day_idx   = -1;
        for (int i = 0; i < count && g_hr_count < WEATHER_HR_MAX; i++) {
            cJSON* item = cJSON_GetArrayItem(forecast, i);
            if (!cJSON_IsObject(item))
                break;
            cJSON* time_obj  = cJSON_GetObjectItemCaseSensitive(item, "time");
            cJSON* temp_obj  = cJSON_GetObjectItemCaseSensitive(item, "temperature");
            cJSON* hum_obj   = cJSON_GetObjectItemCaseSensitive(item, "humidity");
            cJSON* prec_obj  = cJSON_GetObjectItemCaseSensitive(item, "precipitation");
            cJSON* wind_obj  = cJSON_GetObjectItemCaseSensitive(item, "windspeed");
            cJSON* press_obj = cJSON_GetObjectItemCaseSensitive(item, "pressure");
            if (!cJSON_IsString(time_obj))
                continue;

            const char* ts2 = time_obj->valuestring;
            char date[11];
            strncpy(date, ts2, 10);
            date[10] = '\0';
            if (strncmp(date, prev_date, 10) != 0) {
                slot_day_idx++;
                strncpy(prev_date, date, sizeof(prev_date));
                if (slot_day_idx < 7)
                    g_day_hr_start[slot_day_idx] = g_hr_count;
            }
            int slot = slot_day_idx;

            g_hr_data[METRIC_TEMP][g_hr_count] =
                cJSON_IsNumber(temp_obj) ? (int32_t)round(temp_obj->valuedouble) : 0;
            g_hr_data[METRIC_HUMIDITY][g_hr_count] =
                cJSON_IsNumber(hum_obj) ? (int32_t)round(hum_obj->valuedouble) : 0;
            g_hr_data[METRIC_PRECIP][g_hr_count] =
                cJSON_IsNumber(prec_obj) ? (int32_t)round(prec_obj->valuedouble * 10) : 0;
            g_hr_data[METRIC_WIND][g_hr_count] =
                cJSON_IsNumber(wind_obj) ? (int32_t)round(wind_obj->valuedouble) : 0;
            g_hr_data[METRIC_PRESSURE][g_hr_count] =
                cJSON_IsNumber(press_obj) ? (int32_t)round(press_obj->valuedouble) : 0;
            weather_timestamp_to_label(ts2, g_hr_times[g_hr_count], sizeof(g_hr_times[g_hr_count]));

            if (slot >= 0 && slot < 7)
                g_day_hr_count[slot]++;
            g_hr_count++;
        }
    }

    // ── LVGL update ──────────────────────────────────────────────────────────
    lv_obj_t* day_name_arr[7] = {ui_lbl_day_name1, ui_lbl_day_name2, ui_lbl_day_name3,
                                 ui_lbl_day_name4, ui_lbl_day_name5, ui_lbl_day_name6,
                                 ui_lbl_day_name7};
    lv_obj_t* day_hi_arr[7]   = {ui_lbl_day_hi1, ui_lbl_day_hi2, ui_lbl_day_hi3, ui_lbl_day_hi4,
                                 ui_lbl_day_hi5, ui_lbl_day_hi6, ui_lbl_day_hi7};
    lv_obj_t* day_lo_arr[7]   = {ui_lbl_day_lo1, ui_lbl_day_lo2, ui_lbl_day_lo3, ui_lbl_day_lo4,
                                 ui_lbl_day_lo5, ui_lbl_day_lo6, ui_lbl_day_lo7};
    lv_obj_t* day_icon_arr[7] = {ui_img_day_icon1, ui_img_day_icon2, ui_img_day_icon3,
                                 ui_img_day_icon4, ui_img_day_icon5, ui_img_day_icon6,
                                 ui_img_day_icon7};

    // 7-day carousel
    lv_obj_t* day_btn_arr[7] = {ui_btn_day1, ui_btn_day2, ui_btn_day3, ui_btn_day4,
                                ui_btn_day5, ui_btn_day6, ui_btn_day7};

    // find today's index in days[] — past_hours may prepend yesterday's entries
    int today_di = 0;
    if (skip_days == 0) {
        today_di = -1;
        for (int j = 0; j < day_count; j++) {
            if (strncmp(days[j].date, today, 10) == 0) {
                today_di = j;
                break;
            }
        }
    }

    if (skip_days == 0) {
        // weather_min owns only slot 0 (today) — do not touch slots 1-6
        int di = today_di;
        if (di >= 0 && days[di].valid) {
            if (day_btn_arr[0])
                lv_obj_remove_flag(day_btn_arr[0], LV_OBJ_FLAG_HIDDEN);
            char wday[8];
            weather_date_to_weekday(days[di].date, wday, sizeof(wday));
            char hi_buf[16], lo_buf[16];
            snprintf(hi_buf, sizeof(hi_buf), "%+g%s", days[di].max_temp, current_unit);
            snprintf(lo_buf, sizeof(lo_buf), "%+g%s", days[di].min_temp, current_unit);
            if (day_name_arr[0])
                lv_label_set_text(day_name_arr[0], wday);
            if (day_hi_arr[0])
                lv_label_set_text(day_hi_arr[0], hi_buf);
            if (day_lo_arr[0])
                lv_label_set_text(day_lo_arr[0], lo_buf);
            if (day_icon_arr[0])
                lv_image_set_src(day_icon_arr[0], weather_code_to_icon(days[di].weather_code));
            snprintf(g_day_display[0].wday, sizeof(g_day_display[0].wday), "%s", wday);
            snprintf(g_day_display[0].hi, sizeof(g_day_display[0].hi), "Max %s", hi_buf);
            snprintf(g_day_display[0].lo, sizeof(g_day_display[0].lo), "Min %s", lo_buf);
            g_day_display[0].valid = true;
        } else {
            if (day_btn_arr[0])
                lv_obj_add_flag(day_btn_arr[0], LV_OBJ_FLAG_HIDDEN);
            g_day_display[0].valid = false;
        }
    } else {
        // weather_hr owns slots 1-6 (forecast)
        for (int day_idx = 0; day_idx < 7 - skip_days; day_idx++) {
            int slot = day_idx + skip_days;
            if (!days[day_idx].valid) {
                if (day_btn_arr[slot])
                    lv_obj_add_flag(day_btn_arr[slot], LV_OBJ_FLAG_HIDDEN);
                g_day_display[slot].valid = false;
                continue;
            }
            if (day_btn_arr[slot])
                lv_obj_remove_flag(day_btn_arr[slot], LV_OBJ_FLAG_HIDDEN);
            char wday[8];
            weather_date_to_weekday(days[day_idx].date, wday, sizeof(wday));
            char hi_buf[16], lo_buf[16];
            snprintf(hi_buf, sizeof(hi_buf), "%+g%s", days[day_idx].max_temp, current_unit);
            snprintf(lo_buf, sizeof(lo_buf), "%+g%s", days[day_idx].min_temp, current_unit);
            if (day_name_arr[slot])
                lv_label_set_text(day_name_arr[slot], wday);
            if (day_hi_arr[slot])
                lv_label_set_text(day_hi_arr[slot], hi_buf);
            if (day_lo_arr[slot])
                lv_label_set_text(day_lo_arr[slot], lo_buf);
            if (day_icon_arr[slot])
                lv_image_set_src(day_icon_arr[slot],
                                 weather_code_to_icon(days[day_idx].weather_code));
            snprintf(g_day_display[slot].wday, sizeof(g_day_display[slot].wday), "%s", wday);
            snprintf(g_day_display[slot].hi, sizeof(g_day_display[slot].hi), "Max %s", hi_buf);
            snprintf(g_day_display[slot].lo, sizeof(g_day_display[slot].lo), "Min %s", lo_buf);
            g_day_display[slot].valid = true;
        }
    }

    if (skip_days == 0) {
        // Chart
        int sel = g_dd_metric ? (int)lv_dropdown_get_selected(g_dd_metric) : METRIC_TEMP;
        chart_update_metric(sel);

        // Detail panel — current conditions + today's min/max
        const char* current_desc = NULL;
        if (cJSON_IsObject(first_item)) {
            cJSON* desc_obj = cJSON_GetObjectItemCaseSensitive(first_item, "weather_description");
            if (cJSON_IsString(desc_obj))
                current_desc = desc_obj->valuestring;
        }

        char name_buf[8] = "---";
        int td           = today_di;
        if (td >= 0 && days[td].valid)
            weather_date_to_weekday(days[td].date, name_buf, sizeof(name_buf));
        lv_label_set_text(ui_lbl_detail_name, name_buf);
        lv_label_set_text(ui_lbl_detail_desc, current_desc ? current_desc : "-");

        if (td >= 0 && days[td].valid) {
            char hi_buf[32], lo_buf[32];
            snprintf(hi_buf, sizeof(hi_buf), "Max %+g%s", days[td].max_temp, current_unit);
            snprintf(lo_buf, sizeof(lo_buf), "Min %+g%s", days[td].min_temp, current_unit);
            lv_label_set_text(ui_lbl_detail_hi, hi_buf);
            lv_label_set_text(ui_lbl_detail_lo, lo_buf);
            snprintf(g_day_display[0].desc, sizeof(g_day_display[0].desc), "%s",
                     current_desc ? current_desc : "");
        }

        // ── Home screen weather summary ──────────────────────────────────────────
        // Find cur_hr and start_idx: nearest entry >= current time (any minutes)
        int cur_hr    = 0;
        int start_idx = 0;
        {
            time_t now_t    = time(NULL);
            struct tm* tnow = localtime(&now_t);
            cur_hr          = (tnow->tm_year + 1900 >= 2020) ? tnow->tm_hour : 0;
            start_idx       = (g_min_count > 0) ? g_min_count - 1 : 0;
            for (int i = 0; i < g_min_count; i++) {
                int h = (g_min_times[i][0] - '0') * 10 + (g_min_times[i][1] - '0');
                if (h >= cur_hr) {
                    start_idx = i;
                    break;
                }
            }
        }

        if (ui_lbl_w_temp && g_min_count > 0) {
            char tmp[12];
            snprintf(tmp, sizeof(tmp), "%+d%s", (int)g_min_data[METRIC_TEMP][start_idx],
                     current_unit);
            lv_label_set_text(ui_lbl_w_temp, tmp);
        }
        if (ui_lbl_w_desc && g_min_count > 0)
            lv_label_set_text(ui_lbl_w_desc,
                              g_min_descs[start_idx][0] ? g_min_descs[start_idx] : "--");
        if (ui_Image1 && g_min_count > 0)
            lv_image_set_src(ui_Image1, weather_code_to_icon(g_min_codes[start_idx]));
        if (ui_lbl_w_loc && td >= 0 && days[td].valid) {
            char wday[8];
            weather_date_to_weekday(days[td].date, wday, sizeof(wday));
            lv_label_set_text(ui_lbl_w_loc, wday);
        }
        if (ui_lbl_w_temp_hi && td >= 0 && days[td].valid) {
            char hi_buf[16];
            snprintf(hi_buf, sizeof(hi_buf), "Hi %+g%s", days[td].max_temp, current_unit);
            lv_label_set_text(ui_lbl_w_temp_hi, hi_buf);
        }
        if (ui_lbl_w_temp_lo && td >= 0 && days[td].valid) {
            char lo_buf[16];
            snprintf(lo_buf, sizeof(lo_buf), "Lo %+g%s", days[td].min_temp, current_unit);
            lv_label_set_text(ui_lbl_w_temp_lo, lo_buf);
        }
        if (ui_labl_stat_wind_val && g_min_count > 0) {
            char wbuf[12];
            snprintf(wbuf, sizeof(wbuf), "%d m/s", (int)g_min_data[METRIC_WIND][start_idx]);
            lv_label_set_text(ui_labl_stat_wind_val, wbuf);
        }
        if (ui_labl_stat_feels_val && g_min_count > 0) {
            char fbuf[12];
            snprintf(fbuf, sizeof(fbuf), "%+d%s", (int)g_min_feels[start_idx], current_unit);
            lv_label_set_text(ui_labl_stat_feels_val, fbuf);
        }
        if (ui_lbl_stat_sunrise_val && g_sunrise_time[0])
            lv_label_set_text(ui_lbl_stat_sunrise_val, g_sunrise_time);
        if (ui_lbl_stat_sunset_val && g_sunset_time[0])
            lv_label_set_text(ui_lbl_stat_sunset_val, g_sunset_time);
        {
            lv_obj_t* hpanels[] = {ui_panel_home_h1, ui_panel_home_h2, ui_panel_home_h3,
                                   ui_panel_home_h4, ui_panel_home_h5, ui_panel_home_h6};
            lv_obj_t* htimes[]  = {ui_lbl_home_htime1, ui_lbl_home_htime2, ui_lbl_home_htime3,
                                   ui_lbl_home_htime4, ui_lbl_home_htime5, ui_lbl_home_htime6};
            lv_obj_t* htemps[]  = {ui_lbl_home_htemp1, ui_lbl_home_htemp2, ui_lbl_home_htemp3,
                                   ui_lbl_home_htemp4, ui_lbl_home_htemp5, ui_lbl_home_htemp6};
            int slot            = 0;
            for (int i = 0; i < g_min_count && slot < 6; i++) {
                int h = (g_min_times[i][0] - '0') * 10 + (g_min_times[i][1] - '0');
                // only on-the-hour entries, from current hour
                if (h < cur_hr)
                    continue;
                if (g_min_times[i][3] != '0' || g_min_times[i][4] != '0')
                    continue;
                if (hpanels[slot])
                    lv_obj_remove_flag(hpanels[slot], LV_OBJ_FLAG_HIDDEN);
                if (htimes[slot])
                    lv_label_set_text(htimes[slot], g_min_times[i]);
                if (htemps[slot]) {
                    char tbuf[8];
                    snprintf(tbuf, sizeof(tbuf), "%+d%s", (int)g_min_data[METRIC_TEMP][i],
                             current_unit);
                    lv_label_set_text(htemps[slot], tbuf);
                }
                if (hpanels[slot]) {
                    lv_obj_t* icon = lv_obj_get_child(hpanels[slot], 1);
                    if (icon)
                        lv_image_set_src(icon, weather_code_to_icon(g_min_codes[i]));
                }
                slot++;
            }
            for (int s = slot; s < 6; s++) {
                if (hpanels[s])
                    lv_obj_add_flag(hpanels[s], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    cJSON_Delete(root);

    if (g_refresh_label)
        lv_label_set_text(g_refresh_label, "Refresh");
    lv_obj_remove_state(ui_btn_weather_refresh, LV_STATE_DISABLED);
}

static lv_obj_t* make_day_btn(lv_obj_t* parent, const char* name, const lv_image_dsc_t* icon,
                              const char* hi, const char* lo, lv_obj_t** out_name,
                              lv_obj_t** out_img, lv_obj_t** out_hi, lv_obj_t** out_lo) {
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
    lv_obj_set_style_text_font(*out_name, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

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

    static const char* day_names[] = {"---", "---", "---", "---", "---", "---", "---"};
    static const char* day_his[]   = {"+15\xc2\xb0", "+17\xc2\xb0", "+12\xc2\xb0", "+9\xc2\xb0",
                                      "+11\xc2\xb0", "+14\xc2\xb0", "+16\xc2\xb0"};
    static const char* day_los[]   = {"+8\xc2\xb0", "+9\xc2\xb0", "+6\xc2\xb0", "+4\xc2\xb0",
                                      "+5\xc2\xb0", "+7\xc2\xb0", "+8\xc2\xb0"};
    static const lv_image_dsc_t* day_icons[] = {
        &UI_IMG_WX_PARTLY_PNG, &UI_IMG_WX_SUNNY_PNG,  &UI_IMG_WX_CLOUDY_PNG, &UI_IMG_WX_RAIN_PNG,
        &UI_IMG_WX_CLOUDY_PNG, &UI_IMG_WX_PARTLY_PNG, &UI_IMG_WX_SUNNY_PNG,
    };
    lv_obj_t** btn_arr[]  = {&ui_btn_day1, &ui_btn_day2, &ui_btn_day3, &ui_btn_day4,
                             &ui_btn_day5, &ui_btn_day6, &ui_btn_day7};
    lv_obj_t** name_arr[] = {&ui_lbl_day_name1, &ui_lbl_day_name2, &ui_lbl_day_name3,
                             &ui_lbl_day_name4, &ui_lbl_day_name5, &ui_lbl_day_name6,
                             &ui_lbl_day_name7};
    lv_obj_t** icon_arr[] = {&ui_img_day_icon1, &ui_img_day_icon2, &ui_img_day_icon3,
                             &ui_img_day_icon4, &ui_img_day_icon5, &ui_img_day_icon6,
                             &ui_img_day_icon7};
    lv_obj_t** hi_arr[]   = {&ui_lbl_day_hi1, &ui_lbl_day_hi2, &ui_lbl_day_hi3, &ui_lbl_day_hi4,
                             &ui_lbl_day_hi5, &ui_lbl_day_hi6, &ui_lbl_day_hi7};
    lv_obj_t** lo_arr[]   = {&ui_lbl_day_lo1, &ui_lbl_day_lo2, &ui_lbl_day_lo3, &ui_lbl_day_lo4,
                             &ui_lbl_day_lo5, &ui_lbl_day_lo6, &ui_lbl_day_lo7};

    for (int i = 0; i < 7; i++) {
        *btn_arr[i] = make_day_btn(ui_panel_carousel, day_names[i], day_icons[i], day_his[i],
                                   day_los[i], name_arr[i], icon_arr[i], hi_arr[i], lo_arr[i]);
        lv_obj_add_event_cb(*btn_arr[i], on_day_btn_clicked, LV_EVENT_CLICKED, NULL);
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
    lv_label_set_text(ui_lbl_detail_name, "--");
    lv_obj_set_style_text_color(ui_lbl_detail_name, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_name, &lv_font_montserrat_18,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_detail_desc = lv_label_create(detail_left);
    lv_label_set_text(ui_lbl_detail_desc, "--");
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
    lv_label_set_text(ui_lbl_detail_hi, "--");
    lv_obj_set_style_text_color(ui_lbl_detail_hi, UI_COLOR_BAD, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_detail_hi, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_detail_lo = lv_label_create(detail_right);
    lv_label_set_text(ui_lbl_detail_lo, "--");
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
    lv_label_set_text(g_lbl_unit, "\xc2\xb0"
                                  "C");
    lv_obj_set_style_text_color(g_lbl_unit, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(g_lbl_unit, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_dd_metric = lv_dropdown_create(hdr);
    lv_dropdown_set_options(g_dd_metric, "Temperature\nHumidity\nPrecipitation\nWind\nPressure");
    lv_obj_set_width(g_dd_metric, 120);
    lv_obj_set_style_text_font(g_dd_metric, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(g_dd_metric, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(g_dd_metric, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(g_dd_metric, on_metric_changed, LV_EVENT_VALUE_CHANGED, NULL);

    // ── Chart ─────────────────────────────────────────────────────────────────
    g_chart = lv_chart_create(hourly_card);
    lv_obj_set_flex_grow(g_chart, 1);
    lv_obj_set_width(g_chart, lv_pct(100));
    lv_obj_remove_flag(g_chart, LV_OBJ_FLAG_SCROLLABLE);
    lv_chart_set_type(g_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(g_chart, WEATHER_HOUR_MAX);
    lv_chart_set_div_line_count(g_chart, 5, 7);
    lv_obj_set_style_size(g_chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_chart, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(g_chart, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_line_color(g_chart, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);

    g_chart_ser    = lv_chart_add_series(g_chart, UI_COLOR_ACCENT, LV_CHART_AXIS_PRIMARY_Y);
    g_chart_cursor = lv_chart_add_cursor(g_chart, UI_COLOR_ACCENT, LV_DIR_VER);
    lv_obj_add_event_cb(g_chart, on_chart_pressed, LV_EVENT_PRESSING, NULL);

    g_lbl_chart_info = lv_label_create(hourly_card);
    lv_label_set_text(g_lbl_chart_info, "--");
    lv_obj_set_style_text_font(g_lbl_chart_info, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(g_lbl_chart_info, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ── Refresh button — 10 px above bottom edge ──────────────────────────────
    ui_btn_weather_refresh = lv_btn_create(ui_tabweather);
    lv_obj_add_flag(ui_btn_weather_refresh, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_btn_weather_refresh, 120, 36);
    lv_obj_align(ui_btn_weather_refresh, LV_ALIGN_BOTTOM_LEFT, 16, -10);
    lv_obj_set_style_radius(ui_btn_weather_refresh, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btn_weather_refresh, UI_COLOR_BG2,
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_btn_weather_refresh, UI_COLOR_LINE_SOFT,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_btn_weather_refresh, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* refresh_label = lv_label_create(ui_btn_weather_refresh);
    g_refresh_label         = refresh_label;
    lv_label_set_text(refresh_label, "Refresh");
    lv_obj_set_style_text_color(refresh_label, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(refresh_label, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_btn_weather_refresh, on_weather_refresh, LV_EVENT_CLICKED, NULL);
}
