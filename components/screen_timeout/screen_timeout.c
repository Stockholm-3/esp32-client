/*
 * screen_timeout.c — three-stage inactivity system: dim → screensaver → backlight off
 */

#include "screen_timeout.h"

#include "display.h"
#include "lvgl.h"
#include "screensaver.h"

// ── Stage enumeration
// ────────────────────────────────────────────────────────────────────
typedef enum {
    SCREEN_TIMEOUT_STAGE_ACTIVE = 0,    // Normal: full brightness
    SCREEN_TIMEOUT_STAGE_DIM,           // Stage 1: dim overlay visible
    SCREEN_TIMEOUT_STAGE_SCREENSAVER,   // Stage 2: screensaver bouncing
    SCREEN_TIMEOUT_STAGE_BACKLIGHT_OFF, // Stage 3: backlight off
} ScreenTimeoutStage;

// ── State
// ────────────────────────────────────────────────────────────────────
static lv_timer_t* g_s_timer         = NULL;
static uint32_t g_s_last_activity_ms = 0;
static ScreenTimeoutStage g_s_stage  = SCREEN_TIMEOUT_STAGE_ACTIVE;
static lv_obj_t* g_s_dim_overlay     = NULL;

static ScreenTimeoutConfig g_s_config = {0};

// ── Configuration Validation & Normalization
// ────────────────────────────────────────────────────────────────────
static void validate_and_normalize_config(ScreenTimeoutConfig* cfg) {
    // Convert negative values to 0 (disabled)
    if ((int32_t)cfg->dim_timeout_seconds < 0) {
        cfg->dim_timeout_seconds = 0;
    }
    if ((int32_t)cfg->screensaver_timeout_seconds < 0) {
        cfg->screensaver_timeout_seconds = 0;
    }
    if ((int32_t)cfg->backlight_off_timeout_seconds < 0) {
        cfg->backlight_off_timeout_seconds = 0;
    }

    // Ensure stages are in increasing order
    // If a later stage is lower than an earlier stage, skip the earlier one
    if (cfg->screensaver_timeout_seconds > 0 && cfg->dim_timeout_seconds > 0) {
        if (cfg->screensaver_timeout_seconds < cfg->dim_timeout_seconds) {
            cfg->dim_timeout_seconds = cfg->screensaver_timeout_seconds;
        }
    }

    if (cfg->backlight_off_timeout_seconds > 0 && cfg->screensaver_timeout_seconds > 0) {
        if (cfg->backlight_off_timeout_seconds < cfg->screensaver_timeout_seconds) {
            cfg->screensaver_timeout_seconds = cfg->backlight_off_timeout_seconds;
        }
    }

    if (cfg->backlight_off_timeout_seconds > 0 && cfg->dim_timeout_seconds > 0) {
        if (cfg->backlight_off_timeout_seconds < cfg->dim_timeout_seconds) {
            cfg->dim_timeout_seconds = cfg->backlight_off_timeout_seconds;
        }
    }
}

static void timeout_timer_cb(lv_timer_t* timer) {
    (void)timer;

    if (g_s_config.dim_timeout_seconds == 0 && g_s_config.screensaver_timeout_seconds == 0 &&
        g_s_config.backlight_off_timeout_seconds == 0) {
        return;
    }

    uint32_t elapsed_ms = lv_tick_get() - g_s_last_activity_ms;

    // Determine which stage SHOULD be active based on elapsed time
    ScreenTimeoutStage target_stage = SCREEN_TIMEOUT_STAGE_ACTIVE;

    if (g_s_config.backlight_off_timeout_seconds > 0 &&
        elapsed_ms >= g_s_config.backlight_off_timeout_seconds * 1000U) {
        target_stage = SCREEN_TIMEOUT_STAGE_BACKLIGHT_OFF;
    } else if (g_s_config.screensaver_timeout_seconds > 0 &&
               elapsed_ms >= g_s_config.screensaver_timeout_seconds * 1000U) {
        target_stage = SCREEN_TIMEOUT_STAGE_SCREENSAVER;
    } else if (g_s_config.dim_timeout_seconds > 0 &&
               elapsed_ms >= g_s_config.dim_timeout_seconds * 1000U) {
        target_stage = SCREEN_TIMEOUT_STAGE_DIM;
    }

    // Only act on stage changes
    if (target_stage == g_s_stage) {
        return;
    }

    g_s_stage = target_stage;

    switch (g_s_stage) {
    case SCREEN_TIMEOUT_STAGE_BACKLIGHT_OFF:
        if (g_s_dim_overlay) {
            lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        screensaver_hide();
        display_set_backlight(0);
        break;

    case SCREEN_TIMEOUT_STAGE_SCREENSAVER:
        if (g_s_dim_overlay) {
            lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        screensaver_show();
        break;

    case SCREEN_TIMEOUT_STAGE_DIM:
        screensaver_hide();
        if (g_s_dim_overlay) {
            lv_obj_remove_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        break;

    case SCREEN_TIMEOUT_STAGE_ACTIVE:
    default:
        break;
    }
}

// ── Public API
// ────────────────────────────────────────────────────────────────────
void screen_timeout_init(const ScreenTimeoutConfig* config) {
    if (!config) {
        // Default config: 5 min dim, 10 min screensaver, 30 min backlight off
        g_s_config.dim_timeout_seconds           = 5 * 60;
        g_s_config.screensaver_timeout_seconds   = 10 * 60;
        g_s_config.backlight_off_timeout_seconds = 30 * 60;
    } else {
        g_s_config = *config;
    }

    validate_and_normalize_config(&g_s_config);

    g_s_last_activity_ms = lv_tick_get();
    g_s_stage            = SCREEN_TIMEOUT_STAGE_ACTIVE;

    // Create dim overlay
    g_s_dim_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_s_dim_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_s_dim_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_s_dim_overlay, LV_OPA_70, 0);
    lv_obj_set_style_border_width(g_s_dim_overlay, 0, 0);
    lv_obj_remove_flag(g_s_dim_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);

    // Initialize screensaver
    screensaver_init(256);

    // Create timer
    g_s_timer = lv_timer_create(timeout_timer_cb, 1000U, NULL);
}

void screen_timeout_record_activity(void) {
    g_s_last_activity_ms = lv_tick_get();

    // Only do something if we're not already in active state
    if (g_s_stage != SCREEN_TIMEOUT_STAGE_ACTIVE) {
        g_s_stage = SCREEN_TIMEOUT_STAGE_ACTIVE;

        // Hide all overlays
        if (g_s_dim_overlay) {
            lv_obj_add_flag(g_s_dim_overlay, LV_OBJ_FLAG_HIDDEN);
        }
        screensaver_hide();

        // Restore full brightness
        display_set_backlight(255);
    }
}

void screen_timeout_set_config(const ScreenTimeoutConfig* config) {
    if (!config) {
        return;
    }

    g_s_config = *config;
    validate_and_normalize_config(&g_s_config);

    // Immediately apply changes if needed
    screen_timeout_record_activity();
}

void screen_timeout_get_config(ScreenTimeoutConfig* config) {
    if (config) {
        *config = g_s_config;
    }
}
