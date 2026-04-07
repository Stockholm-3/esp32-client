/*
 * ui.c — Application UI, pure LVGL, no hardware dependencies.
 *        Works on both the real WS7B board and the Linux/SDL simulator.
 */

#include "ui.h"
#include "lvgl.h"
#include <stdio.h>

// ── Shared UI state needed by the timer callback
// ──────────────────────────────
static lv_obj_t *s_label_counter = NULL;
static lv_obj_t *s_arc = NULL;
static lv_obj_t *s_bar = NULL;
static int32_t s_counter = 0;

// ── Timer callback — updates counter label, arc, and bar every second ────────
static void ui_update_timer_cb(lv_timer_t *timer) {
    s_counter++;

    char buf[32];
    snprintf(buf, sizeof(buf), "Time with Bingus: %d", s_counter);
    lv_label_set_text(s_label_counter, buf);

    // Animate arc 0→60 over 60 seconds
    lv_arc_set_value(s_arc, (int16_t)(s_counter % 61));

    // Animate bar 0→100 over 10 seconds
    lv_bar_set_value(s_bar, (int32_t)(s_counter % 11) * 10, LV_ANIM_ON);
}

// ── Button event handler
// ──────────────────────────────────────────────────────
static void btn_event_cb(lv_event_t *e) {
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    static int press_count = 0;
    press_count++;
    char buf[32];
    snprintf(buf, sizeof(buf), "Pressed: %d", press_count);
    lv_label_set_text(label, buf);
}

// ── Slider event handler
// ──────────────────────────────────────────────────────
static void slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    char buf[32];
    snprintf(buf, sizeof(buf), "Value: %d", (int)lv_slider_get_value(slider));
    lv_label_set_text(label, buf);
}

// ── Public: build the full UI
// ─────────────────────────────────────────────────
void ui_build(lv_disp_t *disp) {
    // Reset shared state so the UI can be rebuilt cleanly (e.g. in unit tests)
    s_counter = 0;
    s_label_counter = NULL;
    s_arc = NULL;
    s_bar = NULL;

    lv_obj_t *scr = disp ? lv_disp_get_scr_act(disp) : lv_scr_act();

    // Dark blue background
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0d1b2a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    // ── Title label
    // ───────────────────────────────────────────────────────────
    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "ESP BINGUS Simulator");
    lv_obj_set_style_text_color(title, lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // ── Uptime counter label
    // ──────────────────────────────────────────────────
    s_label_counter = lv_label_create(scr);
    lv_label_set_text(s_label_counter, "Time with Bingus: 0s");
    lv_obj_set_style_text_color(s_label_counter, lv_color_hex(0x90caf9),
                                LV_PART_MAIN);
    lv_obj_set_style_text_font(s_label_counter, &lv_font_montserrat_24,
                               LV_PART_MAIN);
    lv_obj_align(s_label_counter, LV_ALIGN_TOP_MID, 0, 60);

    // ── Left panel: arc
    // ───────────────────────────────────────────────────────
    lv_obj_t *left_panel = lv_obj_create(scr);
    lv_obj_set_size(left_panel, 300, 300);
    lv_obj_align(left_panel, LV_ALIGN_LEFT_MID, 30, 20);
    lv_obj_set_style_bg_color(left_panel, lv_color_hex(0x1a2940), LV_PART_MAIN);
    lv_obj_set_style_border_color(left_panel, lv_color_hex(0x42a5f5),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(left_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(left_panel, 12, LV_PART_MAIN);

    s_arc = lv_arc_create(left_panel);
    lv_obj_set_size(s_arc, 200, 200);
    lv_arc_set_range(s_arc, 0, 60);
    lv_arc_set_value(s_arc, 0);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x42a5f5),
                               LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x1e3a5f), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 12, LV_PART_MAIN);
    lv_obj_center(s_arc);

    lv_obj_t *arc_label = lv_label_create(left_panel);
    lv_label_set_text(arc_label, "60s bingus cycle");
    lv_obj_set_style_text_color(arc_label, lv_color_hex(0xaaaaaa),
                                LV_PART_MAIN);
    lv_obj_align(arc_label, LV_ALIGN_BOTTOM_MID, 0, -10);

    // ── Centre panel: sliders
    // ─────────────────────────────────────────────────
    lv_obj_t *mid_panel = lv_obj_create(scr);
    lv_obj_set_size(mid_panel, 340, 300);
    lv_obj_align(mid_panel, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(mid_panel, lv_color_hex(0x1a2940), LV_PART_MAIN);
    lv_obj_set_style_border_color(mid_panel, lv_color_hex(0x66bb6a),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(mid_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(mid_panel, 12, LV_PART_MAIN);

    lv_obj_t *mid_title = lv_label_create(mid_panel);
    lv_label_set_text(mid_title, "Bingus slider");
    lv_obj_set_style_text_color(mid_title, lv_color_hex(0x66bb6a),
                                LV_PART_MAIN);
    lv_obj_align(mid_title, LV_ALIGN_TOP_MID, 0, 5);

    lv_obj_t *under_title = lv_label_create(mid_panel);
    lv_label_set_text(under_title, "Set the correct color to match a bingus");
    lv_obj_set_style_text_color(under_title, lv_color_hex(0xffffff),
                                LV_PART_MAIN);
    lv_obj_align_to(under_title, mid_title, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    const char *slider_names[] = {"Red", "Green", "Blue"};
    uint32_t slider_colors[] = {0xef5350, 0x66bb6a, 0x42a5f5};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *name_lbl = lv_label_create(mid_panel);
        lv_label_set_text(name_lbl, slider_names[i]);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(slider_colors[i]),
                                    LV_PART_MAIN);
        lv_obj_align(name_lbl, LV_ALIGN_TOP_LEFT, 10, 45 + i * 70);

        lv_obj_t *slider = lv_slider_create(mid_panel);
        lv_obj_set_width(slider, 220);
        lv_slider_set_value(slider, 50, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(slider, lv_color_hex(slider_colors[i]),
                                  LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(slider, lv_color_hex(slider_colors[i]),
                                  LV_PART_KNOB);
        lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 10, 65 + i * 70);

        lv_obj_t *val_lbl = lv_label_create(mid_panel);
        lv_label_set_text(val_lbl, "Value: 50");
        lv_obj_set_style_text_color(val_lbl, lv_color_hex(0xaaaaaa),
                                    LV_PART_MAIN);
        lv_obj_align(val_lbl, LV_ALIGN_TOP_RIGHT, -10, 65 + i * 70);

        lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED,
                            val_lbl);
    }

    // ── Right panel: buttons
    // ──────────────────────────────────────────────────
    lv_obj_t *right_panel = lv_obj_create(scr);
    lv_obj_set_size(right_panel, 280, 300);
    lv_obj_align(right_panel, LV_ALIGN_RIGHT_MID, -30, 20);
    lv_obj_set_style_bg_color(right_panel, lv_color_hex(0x1a2940),
                              LV_PART_MAIN);
    lv_obj_set_style_border_color(right_panel, lv_color_hex(0xffa726),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(right_panel, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(right_panel, 12, LV_PART_MAIN);

    lv_obj_t *right_title = lv_label_create(right_panel);
    lv_label_set_text(right_title, "Buttons");
    lv_obj_set_style_text_color(right_title, lv_color_hex(0xffa726),
                                LV_PART_MAIN);
    lv_obj_align(right_title, LV_ALIGN_TOP_MID, 0, 5);

    const char *btn_labels[] = {"Bingus A", "Bingus B", "Bingus C"};
    uint32_t btn_colors[] = {0xef5350, 0x66bb6a, 0x42a5f5};

    for (int i = 0; i < 3; i++) {
        lv_obj_t *btn = lv_btn_create(right_panel);
        lv_obj_set_size(btn, 200, 50);
        lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, 45 + i * 70);
        lv_obj_set_style_bg_color(btn, lv_color_hex(btn_colors[i]),
                                  LV_PART_MAIN);
        lv_obj_set_style_radius(btn, 8, LV_PART_MAIN);

        lv_obj_t *btn_lbl = lv_label_create(btn);
        lv_label_set_text(btn_lbl, btn_labels[i]);
        lv_obj_set_style_text_color(btn_lbl, lv_color_hex(0xffffff),
                                    LV_PART_MAIN);
        lv_obj_center(btn_lbl);

        lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_CLICKED, btn_lbl);
    }

    // ── Bottom bar
    // ────────────────────────────────────────────────────────────
    lv_obj_t *bar_panel = lv_obj_create(scr);
    lv_obj_set_size(bar_panel, 900, 60);
    lv_obj_align(bar_panel, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(bar_panel, lv_color_hex(0x1a2940), LV_PART_MAIN);
    lv_obj_set_style_border_color(bar_panel, lv_color_hex(0x555555),
                                  LV_PART_MAIN);
    lv_obj_set_style_border_width(bar_panel, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(bar_panel, 8, LV_PART_MAIN);

    lv_obj_t *bar_lbl = lv_label_create(bar_panel);
    lv_label_set_text(bar_lbl, "10s");
    lv_obj_set_style_text_color(bar_lbl, lv_color_hex(0xaaaaaa), LV_PART_MAIN);
    lv_obj_align(bar_lbl, LV_ALIGN_LEFT_MID, 10, 0);

    s_bar = lv_bar_create(bar_panel);
    lv_obj_set_size(s_bar, 820, 20);
    lv_bar_set_range(s_bar, 0, 100);
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, lv_color_hex(0xffa726), LV_PART_INDICATOR);
    lv_obj_align(s_bar, LV_ALIGN_RIGHT_MID, -10, 0);

    // ── 1-second update timer
    // ─────────────────────────────────────────────────
    lv_timer_create(ui_update_timer_cb, 1000, NULL);
}
