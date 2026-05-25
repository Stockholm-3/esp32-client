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

lv_obj_t* ui_panel_hours       = NULL;
lv_obj_t* ui_panel_hour1       = NULL;
lv_obj_t* ui_lbl_hour_time1    = NULL;
lv_obj_t* ui_lbl_hour_temp1    = NULL;
lv_obj_t* ui_lbl_hour_precip1  = NULL;
lv_obj_t* ui_img_hour_icon1    = NULL;
lv_obj_t* ui_panel_hour2       = NULL;
lv_obj_t* ui_lbl_hour_time2    = NULL;
lv_obj_t* ui_lbl_hour_temp2    = NULL;
lv_obj_t* ui_lbl_hour_precip2  = NULL;
lv_obj_t* ui_img_hour_icon2    = NULL;
lv_obj_t* ui_panel_hour3       = NULL;
lv_obj_t* ui_lbl_hour_time3    = NULL;
lv_obj_t* ui_lbl_hour_temp3    = NULL;
lv_obj_t* ui_lbl_hour_precip3  = NULL;
lv_obj_t* ui_img_hour_icon3    = NULL;
lv_obj_t* ui_panel_hour4       = NULL;
lv_obj_t* ui_lbl_hour_time4    = NULL;
lv_obj_t* ui_lbl_hour_temp4    = NULL;
lv_obj_t* ui_lbl_hour_precip4  = NULL;
lv_obj_t* ui_img_hour_icon4    = NULL;
lv_obj_t* ui_panel_hour5       = NULL;
lv_obj_t* ui_lbl_hour_time5    = NULL;
lv_obj_t* ui_lbl_hour_temp5    = NULL;
lv_obj_t* ui_lbl_hour_precip5  = NULL;
lv_obj_t* ui_img_hour_icon5    = NULL;
lv_obj_t* ui_panel_hour6       = NULL;
lv_obj_t* ui_lbl_hour_time6    = NULL;
lv_obj_t* ui_lbl_hour_temp6    = NULL;
lv_obj_t* ui_lbl_hour_precip6  = NULL;
lv_obj_t* ui_img_hour_icon6    = NULL;
lv_obj_t* ui_panel_hour7       = NULL;
lv_obj_t* ui_lbl_hour_time7    = NULL;
lv_obj_t* ui_lbl_hour_temp7    = NULL;
lv_obj_t* ui_lbl_hour_precip7  = NULL;
lv_obj_t* ui_img_hour_icon7    = NULL;
lv_obj_t* ui_panel_hour8       = NULL;
lv_obj_t* ui_lbl_hour_time8    = NULL;
lv_obj_t* ui_lbl_hour_temp8    = NULL;
lv_obj_t* ui_lbl_hour_precip8  = NULL;
lv_obj_t* ui_img_hour_icon8    = NULL;
lv_obj_t* ui_panel_hour9       = NULL;
lv_obj_t* ui_lbl_hour_time9    = NULL;
lv_obj_t* ui_lbl_hour_temp9    = NULL;
lv_obj_t* ui_lbl_hour_precip9  = NULL;
lv_obj_t* ui_img_hour_icon9    = NULL;

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

    int hour_count = count < 9 ? count : 9;

    // ── Агрегація по днях (поза LVGL lock) ───────────────────────────────────
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

    lv_obj_t* hour_time_arr[9]   = {ui_lbl_hour_time1,   ui_lbl_hour_time2,   ui_lbl_hour_time3,
                                     ui_lbl_hour_time4,   ui_lbl_hour_time5,   ui_lbl_hour_time6,
                                     ui_lbl_hour_time7,   ui_lbl_hour_time8,   ui_lbl_hour_time9};
    lv_obj_t* hour_icon_arr[9]   = {ui_img_hour_icon1,   ui_img_hour_icon2,   ui_img_hour_icon3,
                                     ui_img_hour_icon4,   ui_img_hour_icon5,   ui_img_hour_icon6,
                                     ui_img_hour_icon7,   ui_img_hour_icon8,   ui_img_hour_icon9};
    lv_obj_t* hour_temp_arr[9]   = {ui_lbl_hour_temp1,   ui_lbl_hour_temp2,   ui_lbl_hour_temp3,
                                     ui_lbl_hour_temp4,   ui_lbl_hour_temp5,   ui_lbl_hour_temp6,
                                     ui_lbl_hour_temp7,   ui_lbl_hour_temp8,   ui_lbl_hour_temp9};
    lv_obj_t* hour_precip_arr[9] = {ui_lbl_hour_precip1, ui_lbl_hour_precip2, ui_lbl_hour_precip3,
                                     ui_lbl_hour_precip4, ui_lbl_hour_precip5, ui_lbl_hour_precip6,
                                     ui_lbl_hour_precip7, ui_lbl_hour_precip8, ui_lbl_hour_precip9};

    if (!display_lvgl_lock(100)) {
        cJSON_Delete(root);
        return;
    }

    // Карусель 7 днів
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

    // Годинна стрічка (перші 9 записів)
    const char* current_time = NULL;
    const char* current_desc = NULL;

    for (int i = 0; i < hour_count; ++i) {
        cJSON* item = cJSON_GetArrayItem(forecast, i);
        if (!cJSON_IsObject(item)) continue;

        cJSON* temp_obj   = cJSON_GetObjectItemCaseSensitive(item, "temperature");
        cJSON* time_obj   = cJSON_GetObjectItemCaseSensitive(item, "time");
        cJSON* precip_obj = cJSON_GetObjectItemCaseSensitive(item, "precipitation");
        cJSON* code_obj   = cJSON_GetObjectItemCaseSensitive(item, "weather_code");

        double temp   = cJSON_IsNumber(temp_obj)   ? temp_obj->valuedouble   : NAN;
        double precip = cJSON_IsNumber(precip_obj) ? precip_obj->valuedouble : 0.0;
        int    code   = cJSON_IsNumber(code_obj)   ? code_obj->valueint      : -1;
        const char* time = cJSON_IsString(time_obj) ? time_obj->valuestring  : NULL;

        char time_buf[16] = "";
        weather_timestamp_to_label(time, time_buf, sizeof(time_buf));

        char temp_buf[16];
        if (isnan(temp)) {
            snprintf(temp_buf, sizeof(temp_buf), "--");
        } else {
            snprintf(temp_buf, sizeof(temp_buf), "%+g%s", temp, current_unit);
        }
        char precip_buf[16];
        snprintf(precip_buf, sizeof(precip_buf), "%.1f mm", precip);

        if (hour_time_arr[i])   lv_label_set_text(hour_time_arr[i], time_buf);
        if (hour_temp_arr[i])   lv_label_set_text(hour_temp_arr[i], temp_buf);
        if (hour_precip_arr[i]) lv_label_set_text(hour_precip_arr[i], precip_buf);
        if (hour_icon_arr[i])   lv_image_set_src(hour_icon_arr[i], weather_code_to_icon(code));

        if (i == 0) current_time = time;
    }

    // Detail panel — опис поточного моменту + min/max за сьогодні
    if (cJSON_IsObject(first_item)) {
        cJSON* desc_obj = cJSON_GetObjectItemCaseSensitive(first_item, "weather_description");
        if (cJSON_IsString(desc_obj)) current_desc = desc_obj->valuestring;
    }

    char name_buf[16] = "Now";
    if (current_time) weather_timestamp_to_label(current_time, name_buf, sizeof(name_buf));
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

static void make_hour_cell(lv_obj_t* parent, const char* time, const lv_image_dsc_t* icon,
                            const char* temp, const char* precip,
                            lv_obj_t** out_panel, lv_obj_t** out_time, lv_obj_t** out_img,
                            lv_obj_t** out_temp, lv_obj_t** out_precip) {
    *out_panel = lv_obj_create(parent);
    lv_obj_set_flex_grow(*out_panel, 1);
    lv_obj_set_height(*out_panel, LV_PCT(100));
    lv_obj_remove_flag(*out_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(*out_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(*out_panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(*out_panel, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(*out_panel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(*out_panel, UI_COLOR_LINE_SOFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(*out_panel, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(*out_panel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(*out_panel, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    *out_time = lv_label_create(*out_panel);
    lv_label_set_text(*out_time, time);
    lv_obj_set_style_text_color(*out_time, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_time, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    *out_img = lv_image_create(*out_panel);
    lv_image_set_src(*out_img, icon);
    lv_image_set_scale(*out_img, 112);
    lv_obj_set_size(*out_img, 28, 28);

    *out_temp = lv_label_create(*out_panel);
    lv_label_set_text(*out_temp, temp);
    lv_obj_set_style_text_color(*out_temp, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_temp, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    *out_precip = lv_label_create(*out_panel);
    lv_label_set_text(*out_precip, precip);
    lv_obj_set_style_text_color(*out_precip, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(*out_precip, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
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

    lv_obj_t* lbl_title = lv_label_create(hourly_card);
    lv_label_set_text(lbl_title, "HOURLY FORECAST");
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_panel_hours = lv_obj_create(hourly_card);
    lv_obj_set_flex_grow(ui_panel_hours, 1);
    lv_obj_set_width(ui_panel_hours, lv_pct(100));
    lv_obj_remove_flag(ui_panel_hours, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(ui_panel_hours, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_hours, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_hours, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(ui_panel_hours, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_flow(ui_panel_hours, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_panel_hours, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    static const char* h_times[]  = {"08", "09", "10", "11", "12", "13", "14", "15", "16"};
    static const char* h_temps[]  = {"+11\xc2\xb0", "+12\xc2\xb0", "+13\xc2\xb0", "+14\xc2\xb0",
                                     "+15\xc2\xb0", "+14\xc2\xb0", "+13\xc2\xb0", "+12\xc2\xb0",
                                     "+11\xc2\xb0"};
    static const char* h_precips[] = {"0%", "0%", "0%", "0%", "5%", "10%", "15%", "20%", "30%"};
    static const lv_image_dsc_t* h_icons[] = {
        &UI_IMG_WX_PARTLY_PNG,  &UI_IMG_WX_PARTLY_PNG, &UI_IMG_WX_SUNNY_PNG,
        &UI_IMG_WX_SUNNY_PNG,   &UI_IMG_WX_PARTLY_PNG, &UI_IMG_WX_PARTLY_PNG,
        &UI_IMG_WX_CLOUDY_PNG,  &UI_IMG_WX_CLOUDY_PNG, &UI_IMG_WX_RAIN_PNG,
    };
    lv_obj_t** h_panels[]  = {&ui_panel_hour1, &ui_panel_hour2, &ui_panel_hour3,
                               &ui_panel_hour4, &ui_panel_hour5, &ui_panel_hour6,
                               &ui_panel_hour7, &ui_panel_hour8, &ui_panel_hour9};
    lv_obj_t** h_time_lbl[] = {&ui_lbl_hour_time1, &ui_lbl_hour_time2, &ui_lbl_hour_time3,
                                &ui_lbl_hour_time4, &ui_lbl_hour_time5, &ui_lbl_hour_time6,
                                &ui_lbl_hour_time7, &ui_lbl_hour_time8, &ui_lbl_hour_time9};
    lv_obj_t** h_imgs[]     = {&ui_img_hour_icon1, &ui_img_hour_icon2, &ui_img_hour_icon3,
                                &ui_img_hour_icon4, &ui_img_hour_icon5, &ui_img_hour_icon6,
                                &ui_img_hour_icon7, &ui_img_hour_icon8, &ui_img_hour_icon9};
    lv_obj_t** h_temp_lbl[] = {&ui_lbl_hour_temp1, &ui_lbl_hour_temp2, &ui_lbl_hour_temp3,
                                &ui_lbl_hour_temp4, &ui_lbl_hour_temp5, &ui_lbl_hour_temp6,
                                &ui_lbl_hour_temp7, &ui_lbl_hour_temp8, &ui_lbl_hour_temp9};
    lv_obj_t** h_prec_lbl[] = {&ui_lbl_hour_precip1, &ui_lbl_hour_precip2, &ui_lbl_hour_precip3,
                                &ui_lbl_hour_precip4, &ui_lbl_hour_precip5, &ui_lbl_hour_precip6,
                                &ui_lbl_hour_precip7, &ui_lbl_hour_precip8, &ui_lbl_hour_precip9};

    for (int i = 0; i < 9; i++) {
        make_hour_cell(ui_panel_hours, h_times[i], h_icons[i], h_temps[i], h_precips[i],
                       h_panels[i], h_time_lbl[i], h_imgs[i], h_temp_lbl[i], h_prec_lbl[i]);
    }

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
