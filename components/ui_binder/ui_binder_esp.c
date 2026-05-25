#include "display.h"
#include "lvgl.h"
#include "squareline/screens/ui_scr_home.h"
#include "ui_binder.h"
#include "elpris_api.h"
#include "time_manager.h"
#include "esp_log.h"  
#include "squareline/screens/ui_tab_elpris.h" 

#include <stdio.h>

static ui_binder_location_cb_t g_s_location_cb  = NULL;
static ui_binder_dropdown_cb_t g_s_price_cb     = NULL;
static ui_binder_dropdown_cb_t g_s_timeout_cb   = NULL;
static ui_binder_location_cb_t g_s_location_cb2 = NULL;
static ui_binder_dropdown_cb_t g_s_price_cb2    = NULL;
static ui_binder_dropdown_cb_t g_s_timeout_cb2  = NULL;
static ui_binder_bool_cb_t g_s_ap_enabled_cb    = NULL;
static ui_binder_bool_cb_t g_s_ap_enabled_cb2   = NULL;
static ui_binder_bool_cb_t g_s_lwc_cb           = NULL;

// Current price group (default SE3)
static ElprisPriceGroup g_price_group = ELPRIS_SE3;

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
    int selected = (int)lv_dropdown_get_selected(ui_dd_price);
    
    // Map dropdown selection to price group
    switch (selected) {
        case 0: g_price_group = ELPRIS_SE1; break;
        case 1: g_price_group = ELPRIS_SE2; break;
        case 2: g_price_group = ELPRIS_SE3; break;
        case 3: g_price_group = ELPRIS_SE4; break;
        default: g_price_group = ELPRIS_SE3; break;
    }
    
    // Refresh prices when group changes
    ui_binder_refresh_elpris();
    
    if (g_s_price_cb) {
        g_s_price_cb(selected);
    }
    if (g_s_price_cb2) {
        g_s_price_cb2(selected);
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

void ui_binder_init(void) {
    lv_obj_add_event_cb(ui_ta_locationinput, on_location_defocused, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ui_dd_price, on_price_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_dd_timeout, on_timeout_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sw_ap_enabled, on_ap_enabled_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(ui_sw_local_web_client, on_lwc_changed, LV_EVENT_VALUE_CHANGED, NULL);
}

void ui_binder_update_localtime(const struct tm* t) {
    char buf[6];
    snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    if (display_lvgl_lock(100)) {
        lv_label_set_text(ui_lbl_localtime, buf);
        display_lvgl_unlock();
    }
    
    // Update current hour for elpris "Now" indicator
    if (t) {
        ui_elpris_set_current_hour(t->tm_hour);
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
    // Update price group based on index
    switch (index) {
        case 0: g_price_group = ELPRIS_SE1; break;
        case 1: g_price_group = ELPRIS_SE2; break;
        case 2: g_price_group = ELPRIS_SE3; break;
        case 3: g_price_group = ELPRIS_SE4; break;
        default: g_price_group = ELPRIS_SE3; break;
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

void ui_binder_on_location_changed2(ui_binder_location_cb_t cb) { g_s_location_cb2 = cb; }
void ui_binder_on_price_changed2(ui_binder_dropdown_cb_t cb) { g_s_price_cb2 = cb; }
void ui_binder_on_timeout_changed2(ui_binder_dropdown_cb_t cb) { g_s_timeout_cb2 = cb; }

void ui_binder_on_ap_enabled_changed(ui_binder_bool_cb_t cb) { g_s_ap_enabled_cb = cb; }
void ui_binder_on_ap_enabled_changed2(ui_binder_bool_cb_t cb) { g_s_ap_enabled_cb2 = cb; }
void ui_binder_on_local_web_client_changed(ui_binder_bool_cb_t cb) { g_s_lwc_cb = cb; }

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

// ========== ELPRIS (Electricity Price) Functions ==========

void ui_binder_refresh_elpris(void) {
    // Check if time is synced
    if (time_manager_get_state() != TIME_STATE_SYNCED) {
        ESP_LOGW("UI_BINDER", "Time not synced, skipping elpris fetch");
        return;
    }
    
    ElprisData prices;
    
    // Fetch latest prices for selected price group
    if (elpris_fetch_latest(g_price_group, &prices) != ESP_OK) {
        ESP_LOGE("UI_BINDER", "Failed to fetch elpris data");
        return;
    }
    
    // Update the UI
    ui_elpris_update_display(&prices);
}

void ui_binder_set_price_group(ElprisPriceGroup group) {
    g_price_group = group;
    
    // Update dropdown to match
    int index = 0;
    switch (group) {
        case ELPRIS_SE1: index = 0; break;
        case ELPRIS_SE2: index = 1; break;
        case ELPRIS_SE3: index = 2; break;
        case ELPRIS_SE4: index = 3; break;
    }
    
    if (display_lvgl_lock(100)) {
        lv_dropdown_set_selected(ui_dd_price, (uint32_t)index);
        display_lvgl_unlock();
    }
    
    // Refresh prices
    ui_binder_refresh_elpris();
}