#include "wifi_popup.h"

#include "squareline/screens/ui_scr_home.h"

#include <string.h>

// TabView y=24, tab bar height=78 → tab content starts at screen y=102
#define TAB_Y 102
// Vertical offset from tab content top
#define POPUP_Y_OFS 50

// Password popup content height (no keyboard — keyboard is a separate sibling object)
#define PW_CONTENT_H 200 // header(44) + net_info(50) + field(60)
#define KB_H 248

static lv_obj_t* g_wifi_backdrop            = NULL;
static lv_obj_t* g_panel_wifi_popup         = NULL;
static lv_obj_t* g_pw_backdrop              = NULL;
static lv_obj_t* g_panel_password_popup     = NULL;
static lv_obj_t* g_pw_keyboard              = NULL;
static lv_obj_t* g_lbl_pw_net_name          = NULL;
static lv_obj_t* g_ta_password              = NULL;
static lv_obj_t* g_net_container            = NULL;
static char g_selected_ssid[33]             = "";
static wifi_popup_connect_cb_t g_connect_cb = NULL;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void style_panel(lv_obj_t* p) {
    lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, 0);
}

static lv_obj_t* make_label(lv_obj_t* parent, int x, int y, int w, int h, const char* text,
                            lv_color_t color, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_size(l, w, h);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_color(l, color, 0);
    if (font) {
        lv_obj_set_style_text_font(l, font, 0);
    }
    return l;
}

static lv_obj_t* make_backdrop(lv_color_t color, lv_opa_t opa, lv_event_cb_t cb) {
    lv_obj_t* bd = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(bd, 0, 0);
    lv_obj_set_size(bd, 1024, 600);
    style_panel(bd);
    lv_obj_set_style_bg_color(bd, color, 0);
    lv_obj_set_style_bg_opa(bd, opa, 0);
    lv_obj_set_style_radius(bd, 0, 0);
    lv_obj_add_flag(bd, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(bd, cb, LV_EVENT_CLICKED, NULL);
    return bd;
}

// ── Event callbacks ───────────────────────────────────────────────────────────

static void close_wifi_popup_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(g_panel_wifi_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_wifi_backdrop, LV_OBJ_FLAG_HIDDEN);
}

static void on_scan_again_cb(lv_event_t* e) {
    (void)e;
    wifi_manager_scan_start(wifi_popup_update_networks);
}

static void show_wifi_popup_cb(lv_event_t* e) {
    (void)e;
    lv_obj_remove_flag(g_wifi_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_panel_wifi_popup, LV_OBJ_FLAG_HIDDEN);
    wifi_manager_scan_start(wifi_popup_update_networks);
}

static void open_password_popup_cb(lv_event_t* e) {
    lv_obj_t* net_panel = lv_event_get_current_target_obj(e);
    lv_obj_t* name_lbl  = lv_obj_get_child(net_panel, 0);
    lv_label_set_text(g_lbl_pw_net_name, lv_label_get_text(name_lbl));
    lv_textarea_set_text(g_ta_password, "");
    lv_obj_add_flag(g_panel_wifi_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_wifi_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_pw_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_panel_password_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_pw_keyboard, LV_OBJ_FLAG_HIDDEN);
    const char* ssid = lv_label_get_text(name_lbl);
    lv_label_set_text(g_lbl_pw_net_name, ssid);
    strncpy(g_selected_ssid, ssid, 32);
    g_selected_ssid[32] = '\0';
}

static void on_connect_pressed_cb(lv_event_t* e) {
    (void)e;
    if (g_connect_cb) {
        g_connect_cb(g_selected_ssid, lv_textarea_get_text(g_ta_password));
    }
    lv_obj_add_flag(g_panel_password_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_pw_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_pw_backdrop, LV_OBJ_FLAG_HIDDEN);
}

static void close_password_popup_cb(lv_event_t* e) {
    (void)e;
    lv_obj_add_flag(g_panel_password_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_pw_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g_pw_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_wifi_backdrop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g_panel_wifi_popup, LV_OBJ_FLAG_HIDDEN);
}

// ── Network panels ────────────────────────────────────────────────────────────

static lv_obj_t* make_net_panel(lv_obj_t* parent, const char* name, const char* sub) {
    lv_obj_t* p = lv_obj_create(parent);
    lv_obj_set_pos(p, 0, 0);
    lv_obj_set_size(p, 420, 52);
    style_panel(p);
    lv_obj_set_style_bg_color(p, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);

    make_label(p, 36, 10, 240, 18, name, lv_color_hex(0xE0E0F0), &lv_font_montserrat_16);
    make_label(p, 36, 30, 240, 14, sub, lv_color_hex(0x8888AA), &lv_font_montserrat_14);

    lv_obj_add_event_cb(p, open_password_popup_cb, LV_EVENT_CLICKED, NULL);
    return p;
}

// ── WiFi popup ────────────────────────────────────────────────────────────────

static void build_wifi_popup(void) {
    g_wifi_backdrop = make_backdrop(lv_color_hex(0x000000), LV_OPA_50, close_wifi_popup_cb);

    g_panel_wifi_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(g_panel_wifi_popup, 302, TAB_Y + POPUP_Y_OFS);
    lv_obj_set_size(g_panel_wifi_popup, 420, 390);
    style_panel(g_panel_wifi_popup);
    lv_obj_set_style_bg_color(g_panel_wifi_popup, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_bg_opa(g_panel_wifi_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_panel_wifi_popup, lv_color_hex(0x4A6FA5), 0);
    lv_obj_set_style_border_width(g_panel_wifi_popup, 1, 0);
    lv_obj_set_style_radius(g_panel_wifi_popup, 12, 0);
    lv_obj_add_flag(g_panel_wifi_popup, LV_OBJ_FLAG_HIDDEN);

    // Header
    lv_obj_t* hdr = lv_obj_create(g_panel_wifi_popup);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 420, 44);
    style_panel(hdr);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x252538), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);

    make_label(hdr, 16, 12, 200, 20, "Available networks", lv_color_hex(0xE0E0F0),
               &lv_font_montserrat_16);

    lv_obj_t* btn_close = lv_button_create(hdr);
    lv_obj_set_pos(btn_close, 378, 8);
    lv_obj_set_size(btn_close, 28, 28);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x252538), 0);
    lv_obj_set_style_shadow_width(btn_close, 0, 0);
    lv_obj_t* lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0x666666), 0);
    lv_obj_center(lbl_x);
    lv_obj_add_event_cb(btn_close, close_wifi_popup_cb, LV_EVENT_CLICKED, NULL);

    g_net_container = lv_obj_create(g_panel_wifi_popup);
    lv_obj_set_pos(g_net_container, 0, 44);
    lv_obj_set_size(g_net_container, 420, 300);
    lv_obj_set_style_pad_all(g_net_container, 0, 0);
    lv_obj_set_style_border_width(g_net_container, 0, 0);
    lv_obj_set_style_bg_opa(g_net_container, LV_OPA_TRANSP, 0);
    lv_obj_set_layout(g_net_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(g_net_container, LV_FLEX_FLOW_COLUMN);

    // Footer
    lv_obj_t* ftr = lv_obj_create(g_panel_wifi_popup);
    lv_obj_set_pos(ftr, 0, 344);
    lv_obj_set_size(ftr, 420, 46);
    style_panel(ftr);
    lv_obj_set_style_bg_color(ftr, lv_color_hex(0x252538), 0);
    lv_obj_set_style_bg_opa(ftr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ftr, 0, 0);

    make_label(ftr, 16, 14, 200, 18, "Last scan: just now", lv_color_hex(0x8888AA),
               &lv_font_montserrat_14);

    lv_obj_t* btn_scan = lv_button_create(ftr);
    lv_obj_set_pos(btn_scan, 306, 8);
    lv_obj_set_size(btn_scan, 100, 30);
    lv_obj_set_style_bg_color(btn_scan, lv_color_hex(0x4A6FA5), 0);
    lv_obj_set_style_radius(btn_scan, 5, 0);
    lv_obj_set_style_shadow_width(btn_scan, 0, 0);
    lv_obj_t* lbl_scan = lv_label_create(btn_scan);
    lv_label_set_text(lbl_scan, "Scan again");
    lv_obj_set_style_text_color(lbl_scan, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_scan);
    lv_obj_add_event_cb(btn_scan, on_scan_again_cb, LV_EVENT_CLICKED, NULL);
}

// ── Password popup ────────────────────────────────────────────────────────────

static void build_password_popup(void) {
    // Keyboard is a SIBLING of the popup on lv_scr_act(), NOT a child.
    // lv_keyboard_create internally calls lv_obj_align(BOTTOM_MID) which stores an alignment
    // that gets re-applied on every layout pass. Placing it as a sibling and overriding the
    // alignment with LV_ALIGN_TOP_LEFT fixes the position permanently.
    const int POPUP_X = 192;
    const int POPUP_Y = TAB_Y + POPUP_Y_OFS; // 152

    g_pw_backdrop = make_backdrop(lv_color_hex(0x000000), LV_OPA_50, close_password_popup_cb);

    g_panel_password_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(g_panel_password_popup, POPUP_X, POPUP_Y);
    lv_obj_set_size(g_panel_password_popup, 640, PW_CONTENT_H);
    style_panel(g_panel_password_popup);
    lv_obj_set_style_bg_color(g_panel_password_popup, lv_color_hex(0x2A2A3E), 0);
    lv_obj_set_style_bg_opa(g_panel_password_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_panel_password_popup, lv_color_hex(0x4A6FA5), 0);
    lv_obj_set_style_border_width(g_panel_password_popup, 1, 0);
    lv_obj_set_style_radius(g_panel_password_popup, 12, 0);
    lv_obj_add_flag(g_panel_password_popup, LV_OBJ_FLAG_HIDDEN);

    // Header
    lv_obj_t* hdr = lv_obj_create(g_panel_password_popup);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 640, 44);
    style_panel(hdr);
    lv_obj_set_style_bg_color(hdr, lv_color_hex(0x252538), 0);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(hdr, 0, 0);

    make_label(hdr, 16, 12, 200, 20, "Enter password", lv_color_hex(0xE0E0F0),
               &lv_font_montserrat_16);

    lv_obj_t* btn_close = lv_button_create(hdr);
    lv_obj_set_pos(btn_close, 598, 8);
    lv_obj_set_size(btn_close, 28, 28);
    lv_obj_set_style_bg_color(btn_close, lv_color_hex(0x252538), 0);
    lv_obj_set_style_shadow_width(btn_close, 0, 0);
    lv_obj_t* lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, "X");
    lv_obj_set_style_text_color(lbl_x, lv_color_hex(0x666666), 0);
    lv_obj_center(lbl_x);
    lv_obj_add_event_cb(btn_close, close_password_popup_cb, LV_EVENT_CLICKED, NULL);

    // Network info bar
    lv_obj_t* net_info = lv_obj_create(g_panel_password_popup);
    lv_obj_set_pos(net_info, 0, 44);
    lv_obj_set_size(net_info, 640, 50);
    style_panel(net_info);
    lv_obj_set_style_bg_color(net_info, lv_color_hex(0x1A2E1A), 0);
    lv_obj_set_style_bg_opa(net_info, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(net_info, 0, 0);

    g_lbl_pw_net_name = make_label(net_info, 36, 10, 400, 20, "NetworkName", lv_color_hex(0xE0E0F0),
                                   &lv_font_montserrat_16);
    make_label(net_info, 36, 32, 400, 14, "WPA2 \xc2\xb7 Protected", lv_color_hex(0x8888AA),
               &lv_font_montserrat_14);

    // Password field
    lv_obj_t* field = lv_obj_create(g_panel_password_popup);
    lv_obj_set_pos(field, 0, 94);
    lv_obj_set_size(field, 640, 70);
    style_panel(field);
    lv_obj_set_style_bg_opa(field, LV_OPA_TRANSP, 0);

    make_label(field, 16, 8, 100, 16, "Password", lv_color_hex(0x8888AA), &lv_font_montserrat_14);

    g_ta_password = lv_textarea_create(field);
    lv_obj_set_pos(g_ta_password, 16, 28);
    lv_obj_set_size(g_ta_password, 430, 34);
    lv_textarea_set_placeholder_text(g_ta_password, "Enter password...");
    lv_textarea_set_password_mode(g_ta_password, true);
    lv_textarea_set_one_line(g_ta_password, true);
    lv_obj_set_style_bg_color(g_ta_password, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_bg_opa(g_ta_password, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_ta_password, lv_color_hex(0x4A6FA5), 0);
    lv_obj_set_style_border_width(g_ta_password, 1, 0);

    // Connect button
    lv_obj_t* btn_connect = lv_button_create(field);
    lv_obj_set_pos(btn_connect, 458, 26);
    lv_obj_set_size(btn_connect, 150, 38);
    lv_obj_set_style_bg_color(btn_connect, lv_color_hex(0x4A6FA5), 0);
    lv_obj_t* lbl = lv_label_create(btn_connect);
    lv_label_set_text(lbl, "Connect");
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn_connect, on_connect_pressed_cb, LV_EVENT_CLICKED, NULL);

    // Keyboard: sibling of popup on lv_scr_act(), placed directly below popup content.
    // Use lv_obj_align(TOP_LEFT) to override the BOTTOM_MID alignment stored by lv_keyboard_create.
    g_pw_keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_align(g_pw_keyboard, LV_ALIGN_TOP_LEFT, POPUP_X, POPUP_Y + PW_CONTENT_H);
    lv_obj_set_size(g_pw_keyboard, 640, KB_H);
    lv_keyboard_set_mode(g_pw_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_textarea(g_pw_keyboard, g_ta_password);
    lv_obj_add_flag(g_pw_keyboard, LV_OBJ_FLAG_HIDDEN);
}

// ── Public init ───────────────────────────────────────────────────────────────

void wifi_popup_init(lv_obj_t* parent) {
    (void)parent;
    build_wifi_popup();
    build_password_popup();
    lv_obj_add_event_cb(ui_btn_wifi_change, show_wifi_popup_cb, LV_EVENT_CLICKED, NULL);
}

void wifi_popup_update_networks(const WifiApInfo* aps, uint16_t count) {
    lv_obj_clean(g_net_container);
    for (uint16_t i = 0; i < count; i++) {
        const char* sub = (int)aps[i].secured ? "WPA2" : "Open";
        make_net_panel(g_net_container, aps[i].ssid, sub);
    }
}

void wifi_popup_on_connect(wifi_popup_connect_cb_t cb) { g_connect_cb = cb; }
