#include "ui.h"

#include "display.h"
#include "squareline/screens/ui_scr_elpris.h"
#include "squareline/screens/ui_scr_home.h"
#include "squareline/screens/ui_scr_settings.h"
#include "squareline/screens/ui_scr_weather.h"
#include "squareline/ui.h"

static uint32_t timeout_minutes_from_idx(uint32_t idx);
static void timeout_changed_cb(lv_event_t* e);
static void ui_connect_timeout_settings(void);

static void go_home_cb(lv_event_t* e) {
    (void)e;
    lv_scr_load(ui_scr_home);
}

static void go_weather_cb(lv_event_t* e) {
    (void)e;
    lv_scr_load(ui_scr_weather);
}

static void go_elpris_cb(lv_event_t* e) {
    (void)e;
    lv_scr_load(ui_scr_elpris);
}

static void go_settings_cb(lv_event_t* e) {
    (void)e;
    lv_scr_load(ui_scr_settings);
}

static void ui_connect_nav(void) {
    lv_obj_add_event_cb(ui_btn_home, go_home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_home2, go_home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_home3, go_home_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_home4, go_home_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_btn__weather, go_weather_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn__weather2, go_weather_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn__weather3, go_weather_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn__weather4, go_weather_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_btn_elpris, go_elpris_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_elpris2, go_elpris_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_elpris3, go_elpris_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_elpris4, go_elpris_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(ui_btn_settings, go_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_settings2, go_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_settings3, go_settings_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_btn_settings4, go_settings_cb, LV_EVENT_CLICKED, NULL);
}

void ui_build(lv_disp_t* disp) {
    (void)disp;
    ui_init();
    ui_connect_nav();

    if (ui_Dropdown2) {
        uint32_t sel     = lv_dropdown_get_selected(ui_Dropdown2);
        uint32_t minutes = timeout_minutes_from_idx(sel);
        display_set_screensaver_timeout_seconds(minutes * 60U);
    }
    ui_connect_timeout_settings();
}

static uint32_t timeout_minutes_from_idx(uint32_t idx) {
    static const uint32_t timeout_minutes[] = {5U, 10U, 15U, 20U, 25U, 30U};
    if (idx >= (sizeof(timeout_minutes) / sizeof(timeout_minutes[0]))) {
        return timeout_minutes[0];
    }
    return timeout_minutes[idx];
}

static void timeout_changed_cb(lv_event_t* e) {
    lv_obj_t* dropdown = lv_event_get_target(e);
    uint32_t sel       = lv_dropdown_get_selected(dropdown);
    uint32_t minutes   = timeout_minutes_from_idx(sel);
    display_set_screensaver_timeout_seconds(minutes * 60U);
    display_record_activity();
}

static void ui_connect_timeout_settings(void) {
    if (!ui_Dropdown2) {
        return;
    }
    lv_obj_add_event_cb(ui_Dropdown2, timeout_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
}