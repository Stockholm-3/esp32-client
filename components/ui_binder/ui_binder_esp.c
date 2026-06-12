#include "display.h"
#include "esp_log.h"
#include "fs.h"
#include "lvgl.h"
#include "squareline/screens/ui_scr_home.h"
#include "ui_binder.h"

#include <stdio.h>
#include <string.h>

static ui_binder_location_cb_t g_s_location_cb      = NULL;
static ui_binder_dropdown_cb_t g_s_price_cb         = NULL;
static ui_binder_dropdown_cb_t g_s_timeout_cb       = NULL;
static ui_binder_location_cb_t g_s_location_cb2     = NULL;
static ui_binder_dropdown_cb_t g_s_price_cb2        = NULL;
static ui_binder_dropdown_cb_t g_s_timeout_cb2      = NULL;
static ui_binder_bool_cb_t g_s_ap_enabled_cb        = NULL;
static ui_binder_bool_cb_t g_s_ap_enabled_cb2       = NULL;
static ui_binder_bool_cb_t g_s_lwc_cb               = NULL;
static ui_binder_button_cb_t g_s_weather_refresh_cb = NULL;

static void load_mock_elpris(void) {
    char* buf = fs_read_str("/storage/response.json");
    if (!buf) {
        ESP_LOGW("ui_binder", "response.json not found on storage");
        return;
    }
    ui_tab_elpris_handle_server_response(buf, strlen(buf));
    free(buf);
}

static void on_location_defocused(lv_event_t* e) {
    (void)e;
    if (g_s_location_cb) {
        g_s_location_cb(lv_textarea_get_text(ui_ta_locationinput));
    }
    if (g_s_location_cb2) {
        g_s_location_cb2(lv_textarea_get_text(ui_ta_locationinput));
    }
}

void ui_binder_update_wifi_status(WifiManagerState state) {
    const char* text;
    if (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS) {
        text = "Online";
    } else if (state == WIFI_MANAGER_STATE_CONNECTED) {
        text = "Connected";
    } else if (state == WIFI_MANAGER_STATE_CONNECTING) {
        text = "Connecting...";
    } else if (state == WIFI_MANAGER_STATE_FAILED) {
        text = "Failed";
    } else if (state == WIFI_MANAGER_STATE_DISCONNECTED) {
        text = "Disconnected";
    } else {
        text = "No internet";
    }
    if (display_lvgl_lock(100)) {
        lv_label_set_text(ui_lbl_wifi_status, text);
        display_lvgl_unlock();
    }
}

void ui_binder_update_wifi_name(const char* ssid) {
    if (display_lvgl_lock(100)) {
        lv_label_set_text(ui_lbl_wifi_name, ssid);
        display_lvgl_unlock();
    }
}

static void on_price_changed(lv_event_t* e) {
    (void)e;
    if (g_s_price_cb) {
        g_s_price_cb((int)lv_dropdown_get_selected(ui_dd_price));
    }
    if (g_s_price_cb2) {
        g_s_price_cb2((int)lv_dropdown_get_selected(ui_dd_price));
    }
}

static void on_timeout_changed(lv_event_t* e) {
    (void)e;
    if (g_s_timeout_cb) {
        g_s_timeout_cb((int)lv_dropdown_get_selected(ui_dd_timeout));
    }
    if (g_s_timeout_cb2) {
        g_s_timeout_cb2((int)lv_dropdown_get_selected(ui_dd_timeout));
    }
}

static void on_ap_enabled_changed(lv_event_t* e) {
    (void)e;
    bool enabled = lv_obj_has_state(ui_sw_ap_enabled, LV_STATE_CHECKED);
    if (g_s_ap_enabled_cb) {
        g_s_ap_enabled_cb(enabled);
    }
    if (g_s_ap_enabled_cb2) {
        g_s_ap_enabled_cb2(enabled);
    }
}

static void on_lwc_changed(lv_event_t* e) {
    (void)e;
    bool enabled = lv_obj_has_state(ui_sw_local_web_client, LV_STATE_CHECKED);
    if (g_s_lwc_cb) {
        g_s_lwc_cb(enabled);
    }
}

static void on_wifi_sw_changed(lv_event_t* e) {
    (void)e;
    if (lv_obj_has_state(ui_sw_wifi, LV_STATE_CHECKED)) {
        lv_obj_clear_state(ui_btn_wifi_change, LV_STATE_DISABLED);
    } else {
        lv_obj_add_state(ui_btn_wifi_change, LV_STATE_DISABLED);
    }
}

void ui_binder_init(void) {
    load_mock_elpris();
    lv_obj_add_event_cb(ui_ta_locationinput, on_location_defocused, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ui_ta_locationinput, on_location_defocused, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ui_dd_price, on_price_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_dd_timeout, on_timeout_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sw_ap_enabled, on_ap_enabled_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sw_local_web_client, on_lwc_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sw_wifi, on_wifi_sw_changed, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_binder_update_localtime(const struct tm* t) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    if (display_lvgl_lock(100)) {
        lv_label_set_text(ui_lbl_localtime, buf);
        display_lvgl_unlock();
    }
}

void ui_binder_set_location(const char* city) {
    if (display_lvgl_lock(100)) {
        lv_textarea_set_text(ui_ta_locationinput, city);
        display_lvgl_unlock();
    }
}

void ui_binder_set_price_zone(int index) {
    if (display_lvgl_lock(100)) {
        lv_dropdown_set_selected(ui_dd_price, (uint32_t)index);
        display_lvgl_unlock();
    }
}

void ui_binder_set_timeout(int index) {
    if (display_lvgl_lock(100)) {
        lv_dropdown_set_selected(ui_dd_timeout, (uint32_t)index);
        lv_obj_send_event(ui_dd_timeout, LV_EVENT_VALUE_CHANGED, NULL);
        display_lvgl_unlock();
    }
}

void ui_binder_on_location_changed(ui_binder_location_cb_t cb) { g_s_location_cb = cb; }
void ui_binder_on_price_changed(ui_binder_dropdown_cb_t cb) { g_s_price_cb = cb; }
void ui_binder_on_timeout_changed(ui_binder_dropdown_cb_t cb) { g_s_timeout_cb = cb; }

void ui_binder_on_location_changed2(ui_binder_location_cb_t cb) { g_s_location_cb2 = cb; }
void ui_binder_on_price_changed2(ui_binder_dropdown_cb_t cb) { g_s_price_cb2 = cb; }
void ui_binder_on_timeout_changed2(ui_binder_dropdown_cb_t cb) { g_s_timeout_cb2 = cb; }

void ui_binder_on_ap_enabled_changed(ui_binder_bool_cb_t cb) { g_s_ap_enabled_cb = cb; }
void ui_binder_on_ap_enabled_changed2(ui_binder_bool_cb_t cb) { g_s_ap_enabled_cb2 = cb; }
void ui_binder_on_local_web_client_changed(ui_binder_bool_cb_t cb) { g_s_lwc_cb = cb; }
void ui_binder_on_weather_refresh(ui_binder_button_cb_t cb) { g_s_weather_refresh_cb = cb; }

void ui_binder_trigger_weather_refresh(void) {
    if (g_s_weather_refresh_cb) {
        g_s_weather_refresh_cb();
    }
}

void ui_binder_update_elpris(const char* json, size_t len) {
    if (!json || len == 0) {
        return;
    }
    if (display_lvgl_lock(100)) {
        ui_tab_elpris_handle_server_response(json, len);
        display_lvgl_unlock();
    }
}

void ui_binder_update_weather_min(const char* json, size_t len) {
    if (!json || len == 0) {
        return;
    }
    if (display_lvgl_lock(100)) {
        ui_tab_weather_handle_server_response(json, len, 0);
        display_lvgl_unlock();
    }
}

void ui_binder_update_weather_hr(const char* json, size_t len) {
    if (!json || len == 0) {
        return;
    }
    if (display_lvgl_lock(100)) {
        ui_tab_weather_handle_server_response(json, len, 1);
        display_lvgl_unlock();
    }
}

void ui_binder_set_ap_enabled(bool enabled) {
    if (display_lvgl_lock(100)) {
        if (enabled) {
            lv_obj_add_state(ui_sw_ap_enabled, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(ui_sw_ap_enabled, LV_STATE_CHECKED);
        }
        display_lvgl_unlock();
    }
}

void ui_binder_set_local_web_client_enabled(bool enabled) {
    if (display_lvgl_lock(100)) {
        if (enabled) {
            lv_obj_add_state(ui_sw_local_web_client, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(ui_sw_local_web_client, LV_STATE_CHECKED);
        }
        display_lvgl_unlock();
    }
}

void ui_binder_update_local_ip(const char* ip) {
    char buf[20];
    snprintf(buf, sizeof(buf), "IP: %s", ip);
    if (display_lvgl_lock(100)) {
        lv_label_set_text(ui_lbl_settings_ip, buf);
        display_lvgl_unlock();
    }
}

void ui_binder_update_bme280(const Bme280Reading* reading) {
    char buf[10];
    if (!display_lvgl_lock(100)) {
        return;
    }
    snprintf(buf, sizeof(buf), "%.1f", (double)reading->temperature_c);
    lv_label_set_text(ui_lbl_temp_val, buf);
    lv_arc_set_value(ui_arc_temp, (int32_t)reading->temperature_c);
    snprintf(buf, sizeof(buf), "%.0f", (double)reading->pressure_hpa);
    lv_label_set_text(ui_lbl_press_val, buf);
    lv_arc_set_value(ui_arc_pressure, (int32_t)reading->pressure_hpa);
    snprintf(buf, sizeof(buf), "%.0f", (double)reading->humidity_pct);
    lv_label_set_text(ui_lbl_hum_val, buf);
    lv_arc_set_value(ui_arc_humidity, (int32_t)reading->humidity_pct);
    display_lvgl_unlock();
}
