#include "lvgl.h"
#include "squareline/screens/ui_scr_home.h"
#include "ui_binder.h"

#include <stdio.h>

static ui_binder_location_cb_t s_location_cb     = NULL;
static ui_binder_dropdown_cb_t s_price_cb        = NULL;
static ui_binder_dropdown_cb_t s_timeout_cb      = NULL;
static ui_binder_location_cb_t s_location_cb2    = NULL;
static ui_binder_dropdown_cb_t s_price_cb2       = NULL;
static ui_binder_dropdown_cb_t s_timeout_cb2     = NULL;
static ui_binder_bool_cb_t s_ap_cb               = NULL;
static ui_binder_bool_cb_t s_ap_cb2              = NULL;
static ui_binder_bool_cb_t s_lwc_cb              = NULL;
static ui_binder_brightness_cb_t s_brightness_cb = NULL;

static void on_location_defocused(lv_event_t* e) {
    (void)e;
    if (s_location_cb)
        s_location_cb(lv_textarea_get_text(ui_ta_locationinput));
}

void ui_binder_update_wifi_status(WifiManagerState state) {
    const char* text = state == WIFI_MANAGER_STATE_CONNECTED    ? "WiFi"
                       : state == WIFI_MANAGER_STATE_CONNECTING ? "..."
                                                                : "No WiFi";
    lv_label_set_text(ui_lbl_wifi_status, text);
}

void ui_binder_update_wifi_name(const char* ssid) { lv_label_set_text(ui_lbl_wifi_name, ssid); }

static void on_price_changed(lv_event_t* e) {
    (void)e;
    if (s_price_cb)
        s_price_cb((int)lv_dropdown_get_selected(ui_dd_price));
}
static void on_timeout_changed(lv_event_t* e) {
    (void)e;
    if (s_timeout_cb)
        s_timeout_cb((int)lv_dropdown_get_selected(ui_dd_timeout));
}

static void on_brightness_changed(lv_event_t* e) {
    (void)e;
    int value = (int)lv_slider_get_value(ui_sl_brightness);
    if (s_brightness_cb)
        s_brightness_cb(value);
}

void ui_binder_init(void) {
    lv_obj_add_event_cb(ui_ta_locationinput, on_location_defocused, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ui_dd_price, on_price_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_dd_timeout, on_timeout_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sl_brightness, on_brightness_changed, LV_EVENT_RELEASED, NULL);
}

void ui_binder_update_localtime(const struct tm* t) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    lv_label_set_text(ui_lbl_localtime, buf);
}

void ui_binder_set_location(const char* city) { lv_textarea_set_text(ui_ta_locationinput, city); }
void ui_binder_set_price_zone(int index) { lv_dropdown_set_selected(ui_dd_price, (uint32_t)index); }
void ui_binder_set_timeout(int index) { lv_dropdown_set_selected(ui_dd_timeout, (uint32_t)index); }

void ui_binder_on_location_changed(ui_binder_location_cb_t cb) { s_location_cb = cb; }
void ui_binder_on_price_changed(ui_binder_dropdown_cb_t cb) { s_price_cb = cb; }
void ui_binder_on_timeout_changed(ui_binder_dropdown_cb_t cb) { s_timeout_cb = cb; }

void ui_binder_on_location_changed2(ui_binder_location_cb_t cb) { s_location_cb2 = cb; }
void ui_binder_on_price_changed2(ui_binder_dropdown_cb_t cb) { s_price_cb2 = cb; }
void ui_binder_on_timeout_changed2(ui_binder_dropdown_cb_t cb) { s_timeout_cb2 = cb; }
void ui_binder_set_ap_enabled(bool enabled) { (void)enabled; }
void ui_binder_set_local_web_client_enabled(bool enabled) { (void)enabled; }
void ui_binder_on_ap_enabled_changed(ui_binder_bool_cb_t cb) { s_ap_cb = cb; }
void ui_binder_on_ap_enabled_changed2(ui_binder_bool_cb_t cb) { s_ap_cb2 = cb; }
void ui_binder_on_local_web_client_changed(ui_binder_bool_cb_t cb) { s_lwc_cb = cb; }
void ui_binder_on_brightness_changed(ui_binder_brightness_cb_t cb) { s_brightness_cb = cb; }
void ui_binder_update_local_ip(const char* ip) { (void)ip; }

void ui_binder_set_brightness(int value) {
    lv_slider_set_value(ui_sl_brightness, (int32_t)value, LV_ANIM_OFF);
}
void ui_binder_update_bme280(const Bme280Reading* reading) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%.1f", (double)reading->temperature_c);
    lv_label_set_text(ui_lbl_temp_val, buf);
    lv_arc_set_value(ui_arc_temp, (int32_t)reading->temperature_c);
    snprintf(buf, sizeof(buf), "%.0f", (double)reading->pressure_hpa);
    lv_label_set_text(ui_lbl_press_val, buf);
    lv_arc_set_value(ui_arc_pressure, (int32_t)reading->pressure_hpa);
    snprintf(buf, sizeof(buf), "%.0f", (double)reading->humidity_pct);
    lv_label_set_text(ui_lbl_hum_val, buf);
    lv_arc_set_value(ui_arc_humidity, (int32_t)reading->humidity_pct);
}
