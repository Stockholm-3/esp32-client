#include "display.h"
#include "esp_log.h"
#include "fs.h"
#include "lvgl.h"
#include "squareline/screens/ui_scr_home.h"
#include "ui_binder.h"

#include <stdio.h>
#include <string.h>

static ui_binder_location_cb_t g_s_location_cb = NULL;
static ui_binder_dropdown_cb_t g_s_price_cb    = NULL;
static ui_binder_dropdown_cb_t g_s_timeout_cb  = NULL;

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
}

void ui_binder_update_wifi_status(WifiManagerState state) {
    const char* text = state == WIFI_MANAGER_STATE_CONNECTED    ? "WiFi"
                       : state == WIFI_MANAGER_STATE_CONNECTING ? "..."
                                                                : "No WiFi";
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
}

static void on_timeout_changed(lv_event_t* e) {
    (void)e;
    if (g_s_timeout_cb) {
        g_s_timeout_cb((int)lv_dropdown_get_selected(ui_dd_timeout));
    }
}

void ui_binder_init(void) {
    load_mock_elpris();
    lv_obj_add_event_cb(ui_ta_locationinput, on_location_defocused, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ui_dd_price, on_price_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_dd_timeout, on_timeout_changed, LV_EVENT_VALUE_CHANGED, NULL);
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
        display_lvgl_unlock();
    }
}

void ui_binder_on_location_changed(ui_binder_location_cb_t cb) { g_s_location_cb = cb; }
void ui_binder_on_price_changed(ui_binder_dropdown_cb_t cb) { g_s_price_cb = cb; }
void ui_binder_on_timeout_changed(ui_binder_dropdown_cb_t cb) { g_s_timeout_cb = cb; }

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
