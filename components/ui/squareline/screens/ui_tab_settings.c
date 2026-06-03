#include "../ui.h"
#include "kb_swedish.h"
#include "../ui_theme.h"
#include "../ui_events.h"
#include "ui_scr_home.h"

lv_obj_t* ui_panel_settings_main = NULL;
lv_obj_t* ui_panel_wifi          = NULL;
lv_obj_t* ui_lbl_wifi_title      = NULL;
lv_obj_t* ui_lbl_wifi_sub        = NULL;
lv_obj_t* ui_lbl_wifi_name       = NULL;
lv_obj_t* ui_btn_wifi_change     = NULL;
lv_obj_t* ui_lbl_btn_wifi_change = NULL;
lv_obj_t* ui_sw_wifi             = NULL;
lv_obj_t* ui_panel_location      = NULL;
lv_obj_t* ui_lbl_loc_title       = NULL;
lv_obj_t* ui_lbl_loc_sub         = NULL;
lv_obj_t* ui_ta_locationinput    = NULL;
lv_obj_t* ui_panel_price         = NULL;
lv_obj_t* ui_lbl_price_title     = NULL;
lv_obj_t* ui_lbl_price_sub       = NULL;
lv_obj_t* ui_dd_price            = NULL;
lv_obj_t* ui_panel_timeout       = NULL;
lv_obj_t* ui_lbl_timeout_title   = NULL;
lv_obj_t* ui_lbl_timeout_sub     = NULL;
lv_obj_t* ui_dd_timeout          = NULL;
lv_obj_t* ui_Keyboard1           = NULL;
lv_obj_t* ui_sw_alerts           = NULL;
lv_obj_t* ui_sw_ap_enabled       = NULL;
lv_obj_t* ui_sw_local_web_client = NULL;
lv_obj_t* ui_lbl_settings_ip    = NULL;

void ui_event_ta_locationinput(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED)   show_keyboard(e);
    if (code == LV_EVENT_DEFOCUSED) hide_keyboard(e);
}

// Returns a row panel with title+subtitle on the left.
// Add your control widget(s) directly to the returned object.
static lv_obj_t* make_setting_row(lv_obj_t* parent,
                                   const char* title, const char* subtitle) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius(row, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(row, UI_COLOR_BG1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_hor(row, 20, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_ver(row, 14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(row, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_flex_grow(row, 1);

    lv_obj_t* info = lv_obj_create(row);
    lv_obj_set_height(info, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(info, 1);
    lv_obj_remove_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(info, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(info, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(info, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(info, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(info, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_title = lv_label_create(info);
    lv_label_set_text(lbl_title, title);
    lv_obj_set_style_text_color(lbl_title, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_sub = lv_label_create(info);
    lv_label_set_text(lbl_sub, subtitle);
    lv_obj_set_style_text_color(lbl_sub, UI_COLOR_INK3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    return row;
}

static lv_obj_t* make_dropdown(lv_obj_t* row, const char* options) {
    lv_obj_t* dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_width(dd, 160);
    lv_obj_set_style_bg_color(dd, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(dd, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(dd, UI_COLOR_LINE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(dd, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(dd, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(dd, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* list = lv_dropdown_get_list(dd);
    lv_obj_set_style_bg_color(list, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(list, UI_COLOR_LINE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(list, UI_COLOR_INK1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    return dd;
}

void ui_tab_settings_init(void) {
    // Scrollable body
    ui_panel_settings_main = lv_obj_create(ui_tabsettings);
    lv_obj_set_size(ui_panel_settings_main, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(ui_panel_settings_main, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_panel_settings_main, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(ui_panel_settings_main, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_panel_settings_main, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_panel_settings_main, 16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_panel_settings_main, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ---- WiFi row ----
    ui_panel_wifi = make_setting_row(ui_panel_settings_main, "WIFI", "Network connection");
    lv_obj_t* wifi_labels = lv_obj_get_child(ui_panel_wifi, 0);  // info column

    ui_lbl_wifi_title = lv_obj_get_child(wifi_labels, 0);
    ui_lbl_wifi_sub   = lv_obj_get_child(wifi_labels, 1);

    ui_lbl_wifi_name = lv_label_create(wifi_labels);
    lv_label_set_text(ui_lbl_wifi_name, "--");
    lv_obj_set_style_text_color(ui_lbl_wifi_name, UI_COLOR_GOOD, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_wifi_name, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* wifi_ctrl = lv_obj_create(ui_panel_wifi);
    lv_obj_set_height(wifi_ctrl, LV_SIZE_CONTENT);
    lv_obj_set_width(wifi_ctrl, LV_SIZE_CONTENT);
    lv_obj_remove_flag(wifi_ctrl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(wifi_ctrl, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(wifi_ctrl, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(wifi_ctrl, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(wifi_ctrl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(wifi_ctrl, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(wifi_ctrl, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_sw_wifi = lv_switch_create(wifi_ctrl);
    lv_obj_set_size(ui_sw_wifi, 50, 26);
    lv_obj_add_state(ui_sw_wifi, LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(ui_sw_wifi, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_sw_wifi, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

    ui_btn_wifi_change = lv_button_create(wifi_ctrl);
    lv_obj_set_size(ui_btn_wifi_change, 90, 34);
    lv_obj_set_style_radius(ui_btn_wifi_change, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_btn_wifi_change, UI_COLOR_BG3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_btn_wifi_change, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_btn_wifi_change, UI_COLOR_LINE,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_btn_wifi_change, 1, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_btn_wifi_change = lv_label_create(ui_btn_wifi_change);
    lv_label_set_text(ui_lbl_btn_wifi_change, "Change");
    lv_obj_center(ui_lbl_btn_wifi_change);
    lv_obj_set_style_text_color(ui_lbl_btn_wifi_change, UI_COLOR_INK1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_btn_wifi_change, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // ---- Location row ----
    ui_panel_location = make_setting_row(ui_panel_settings_main,
                                         "Location", "City for weather data");
    ui_lbl_loc_title = lv_obj_get_child(lv_obj_get_child(ui_panel_location, 0), 0);
    ui_lbl_loc_sub   = lv_obj_get_child(lv_obj_get_child(ui_panel_location, 0), 1);

    ui_ta_locationinput = lv_textarea_create(ui_panel_location);
    lv_obj_set_size(ui_ta_locationinput, 220, 40);
    lv_textarea_set_one_line(ui_ta_locationinput, true);
    lv_textarea_set_placeholder_text(ui_ta_locationinput, "Enter city...");
    lv_textarea_set_max_length(ui_ta_locationinput, 64);
    lv_obj_set_style_bg_color(ui_ta_locationinput, UI_COLOR_BG2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_ta_locationinput, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_ta_locationinput, UI_COLOR_LINE,
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_ta_locationinput, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_ta_locationinput, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_ta_locationinput, UI_COLOR_INK1,
                                LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_ta_locationinput, &lv_font_montserrat_14,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(ui_ta_locationinput, ui_event_ta_locationinput,
                        LV_EVENT_ALL, NULL);

    // ---- Price group row ----
    ui_panel_price = make_setting_row(ui_panel_settings_main,
                                      "Price group", "Swedish electricity zone");
    ui_lbl_price_title = lv_obj_get_child(lv_obj_get_child(ui_panel_price, 0), 0);
    ui_lbl_price_sub   = lv_obj_get_child(lv_obj_get_child(ui_panel_price, 0), 1);
    ui_dd_price = make_dropdown(ui_panel_price, "SE1\nSE2\nSE3\nSE4");
    lv_dropdown_set_selected(ui_dd_price, 2);  // default SE3

    // ---- Screen timeout row ----
    ui_panel_timeout = make_setting_row(ui_panel_settings_main,
                                        "Screen timeout", "Auto dim after inactivity");
    ui_lbl_timeout_title = lv_obj_get_child(lv_obj_get_child(ui_panel_timeout, 0), 0);
    ui_lbl_timeout_sub   = lv_obj_get_child(lv_obj_get_child(ui_panel_timeout, 0), 1);
    ui_dd_timeout = make_dropdown(ui_panel_timeout, "5 min\n10 min\n15 min\n20 min\nNever");
    lv_dropdown_set_selected(ui_dd_timeout, 1);  // default 10 min

    // ---- Price alerts row (new) ----
    lv_obj_t* row_alerts = make_setting_row(ui_panel_settings_main,
                                             "Price alerts", "Notify on cheap hours");
    ui_sw_alerts = lv_switch_create(row_alerts);
    lv_obj_set_size(ui_sw_alerts, 50, 26);
    lv_obj_set_style_bg_color(ui_sw_alerts, UI_COLOR_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_sw_alerts, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);

    // ---- ESP32-Settings AP row ----
    lv_obj_t* row_ap = make_setting_row(ui_panel_settings_main,
                                        "ESP32-Settings AP", "Hotspot for initial setup");
    ui_sw_ap_enabled = lv_switch_create(row_ap);
    lv_obj_set_size(ui_sw_ap_enabled, 50, 26);
    lv_obj_set_style_bg_color(ui_sw_ap_enabled, UI_COLOR_ACCENT,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_sw_ap_enabled, LV_OPA_COVER,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);

    // ---- Local web client row ----
    lv_obj_t* row_lwc = make_setting_row(ui_panel_settings_main,
                                         "Local web client", "Fixed IP for home network");
    ui_sw_local_web_client = lv_switch_create(row_lwc);
    lv_obj_set_size(ui_sw_local_web_client, 50, 26);
    lv_obj_set_style_bg_color(ui_sw_local_web_client, UI_COLOR_ACCENT,
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(ui_sw_local_web_client, LV_OPA_COVER,
                            LV_PART_INDICATOR | LV_STATE_CHECKED);

    // ---- Footer (status-bar sized: 24px, full width, horizontal) ----
    lv_obj_t* footer = lv_obj_create(ui_panel_settings_main);
    lv_obj_set_width(footer, lv_pct(100));
    lv_obj_set_height(footer, 24);
    lv_obj_remove_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(footer, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(footer, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(footer, UI_COLOR_BG0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(footer, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_fw = lv_label_create(footer);
    lv_label_set_text(lbl_fw, "FW --");
    lv_obj_set_style_text_color(lbl_fw, UI_COLOR_INK4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_fw, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_lbl_settings_ip = lv_label_create(footer);
    lv_label_set_text(ui_lbl_settings_ip, "IP: --");
    lv_obj_set_style_text_color(ui_lbl_settings_ip, UI_COLOR_INK4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_lbl_settings_ip, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* lbl_up = lv_label_create(footer);
    lv_label_set_text(lbl_up, "Uptime: --");
    lv_obj_set_style_text_color(lbl_up, UI_COLOR_INK4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(lbl_up, &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);

    // ---- Keyboard (hidden until textarea focused) ----
    ui_Keyboard1 = lv_keyboard_create(ui_tabsettings);
    lv_keyboard_set_mode(ui_Keyboard1, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_set_size(ui_Keyboard1, 1024, 248);
    lv_obj_align(ui_Keyboard1, LV_ALIGN_BOTTOM_MID, 0, 0);
    kb_apply_swedish(ui_Keyboard1);
    lv_obj_add_flag(ui_Keyboard1, LV_OBJ_FLAG_HIDDEN);
}
