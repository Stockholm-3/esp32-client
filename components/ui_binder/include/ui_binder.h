#pragma once
#include <time.h>
#include "bme280_sensor.h"
#include "wifi_manager.h"

// initialization for LVGL callbacks for changes in settings (location, price zone, timeout)
void ui_binder_init(void);

// Time
void ui_binder_update_localtime(const struct tm *t);

// Placeholders for settings_manager (called when loading saved values)
void ui_binder_set_location(const char *city);
void ui_binder_set_price_zone(int index);    // 0=SE1, 1=SE2, 2=SE3, 3=SE4
void ui_binder_set_timeout(int index);       // 0=5min, 1=10min, ...

// Change callbacks — settings_manager will register them here
typedef void (*ui_binder_location_cb_t)(const char *city);
typedef void (*ui_binder_dropdown_cb_t)(int index);

void ui_binder_on_location_changed(ui_binder_location_cb_t cb);
void ui_binder_on_price_changed(ui_binder_dropdown_cb_t cb);
void ui_binder_on_timeout_changed(ui_binder_dropdown_cb_t cb);
void ui_binder_on_location_changed2(ui_binder_location_cb_t cb);
void ui_binder_on_price_changed2(ui_binder_dropdown_cb_t cb);
void ui_binder_on_timeout_changed2(ui_binder_dropdown_cb_t cb);
void ui_binder_update_wifi_status(WifiManagerState state);
void ui_binder_update_wifi_name(const char *ssid);
void ui_binder_update_bme280(const Bme280Reading *reading);

typedef void (*ui_binder_bool_cb_t)(bool enabled);
typedef void (*ui_binder_button_cb_t)(void);

void ui_binder_set_ap_enabled(bool enabled);
void ui_binder_set_local_web_client_enabled(bool enabled);

void ui_binder_on_ap_enabled_changed(ui_binder_bool_cb_t cb);
void ui_binder_on_ap_enabled_changed2(ui_binder_bool_cb_t cb);
void ui_binder_on_local_web_client_changed(ui_binder_bool_cb_t cb);
void ui_binder_on_weather_refresh(ui_binder_button_cb_t cb);
void ui_binder_trigger_weather_refresh(void);
void ui_binder_update_elpris(const char* json, size_t len);
void ui_binder_update_weather_min(const char* json, size_t len);
void ui_binder_update_weather_hr(const char* json, size_t len);
void ui_binder_update_local_ip(const char* ip);
