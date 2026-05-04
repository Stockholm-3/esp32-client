/*
 * screen_timeout.c — inactivity dim + screensaver logic
 */

#include "screen_timeout.h"

#include "display.h"
#include "lvgl.h"

// ── State
// ────────────────────────────────────────────────────────────────────
static lv_timer_t* g_s_timer         = NULL;
static uint32_t g_s_last_activity_ms = 0;
static uint32_t g_s_timeout_seconds  = 5U * 60U;
static bool g_s_active               = false;
static lv_obj_t* g_s_dim_overlay     = NULL;

// ── Timer callback (fires every 1 s via LVGL timer)
// ─────────────────────────────────────
static void timeout_timer_cb(lv_timer_t* timer) {
    (void)timer;

    if (g_s_timeout_seconds == 0U || (int)g_s_active) {
        return;
    }

    uint32_t elapsed_ms = lv_tick_get() - g_s_last_activity_ms;
    uint32_t timeout_ms = g_s_timeout_seconds * 1000U;

    if (elapsed_ms >= timeout_ms) {
        g_s_active = true;
        if (g_s_dim_overlay) {
            lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        display_set_backlight(0);
    } else if (elapsed_ms >= timeout_ms / 2U) {
        if (g_s_dim_overlay) {
            lv_obj_remove_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ── Public API
// ───────────────────────────────────────────────────────────────
void screen_timeout_init(uint32_t default_timeout_seconds) {
    g_s_timeout_seconds  = default_timeout_seconds;
    g_s_last_activity_ms = lv_tick_get();

    g_s_timer = lv_timer_create(timeout_timer_cb, 1000U, NULL);

    g_s_dim_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_s_dim_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_s_dim_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_s_dim_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(g_s_dim_overlay, 0, 0);
    lv_obj_remove_flag(g_s_dim_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
}

void screen_timeout_record_activity(void) {
    g_s_last_activity_ms = lv_tick_get();
    if (g_s_dim_overlay) {
        lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (g_s_active) {
        g_s_active = false;
        display_set_backlight(255);
    }
}

void screen_timeout_set_seconds(uint32_t timeout_seconds) {
    g_s_timeout_seconds = timeout_seconds;

    if (timeout_seconds == 0U && (int)g_s_active) {
        g_s_active = false;
        display_set_backlight(255);
    }

    screen_timeout_record_activity();
}
