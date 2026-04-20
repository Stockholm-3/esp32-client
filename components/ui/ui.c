#include "ui.h"

#include "squareline/ui.h"
#include "wifi_popup.h"

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

    // Reparent keyboard to screen level — sibling of TabView, not clipped by tab content
    lv_obj_set_parent(ui_Keyboard1, lv_scr_act());
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_size(ui_Keyboard1, 1024, 248);
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(ui_ta_locationinput, on_ta_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(ui_ta_locationinput, on_ta_defocused, LV_EVENT_DEFOCUSED, NULL);

    wifi_popup_init(ui_tabsettings);
}
