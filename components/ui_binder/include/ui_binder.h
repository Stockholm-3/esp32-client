#pragma once
#include <time.h>
#include "lvgl.h"

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
