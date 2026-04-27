#include "ui.h"

#include "display.h"
#include "screen_timeout.h"
#include "squareline/ui.h"
#include "wifi_popup.h"

static uint32_t timeout_minutes_from_idx(uint32_t idx);
static void timeout_changed_cb(lv_event_t* e);
static void ui_connect_timeout_settings(void);

static void on_ta_clicked(lv_event_t* e) {
    lv_obj_remove_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_textarea(ui_Keyboard1, lv_event_get_target_obj(e));
}

static void on_ta_defocused(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
}

void ui_build(lv_display_t* disp) {
    (void)disp;
    ui_init();

    lv_obj_set_parent(ui_Keyboard1, lv_scr_act());
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(ui_Keyboard1, 1024, 248);
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(ui_ta_locationinput, on_ta_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ta_locationinput, on_ta_defocused, LV_EVENT_DEFOCUSED, NULL);

    // TODO(ui_binder): The block below (initial timeout read + ui_connect_timeout_settings)
    // should be moved to ui_binder once it is ready, so that ui.c is not coupled to
    // screen_timeout directly.
    if (ui_dd_timeout) {
        uint32_t sel     = lv_dropdown_get_selected(ui_dd_timeout);
        uint32_t minutes = timeout_minutes_from_idx(sel);
        screen_timeout_set_seconds(minutes * 60U);
    }
    ui_connect_timeout_settings();

    // TODO(ui_binder): wifi_popup_init call should also be moved to ui_binder.
    wifi_popup_init(ui_tabsettings);
}

// TODO(ui_binder): timeout_minutes_from_idx, timeout_changed_cb, and
// ui_connect_timeout_settings should move to ui_binder once it is ready.
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
    screen_timeout_set_seconds(minutes * 60U);
    screen_timeout_record_activity();
}

static void ui_connect_timeout_settings(void) {
    if (!ui_dd_timeout) {
        return;
    }
    lv_obj_add_event_cb(ui_dd_timeout, timeout_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
