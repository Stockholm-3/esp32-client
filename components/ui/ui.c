#include "ui.h"

#include "squareline/screens/ui_scr_elpris.h"
#include "squareline/screens/ui_scr_home.h"
#include "squareline/screens/ui_scr_settings.h"
#include "squareline/screens/ui_scr_weather.h"
#include "squareline/ui.h"

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
}
