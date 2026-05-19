/*
 * screensaver.c — dark background + bouncing image screensaver
 */

#include "screensaver.h"

#include "lvgl.h"
#include "screensaver_image.h"

#define SCREENSAVER_SPEED_X 150
#define SCREENSAVER_SPEED_Y 120
#define SCREENSAVER_BG_COLOR lv_color_black()

static lv_obj_t* g_ss_bg_rect       = NULL;
static lv_obj_t* g_ss_img           = NULL;
static lv_timer_t* g_ss_timer       = NULL;
static bool g_ss_active             = false;
static int32_t g_ss_pos_x           = 0;
static int32_t g_ss_pos_y           = 0;
static int32_t g_ss_vel_x           = SCREENSAVER_SPEED_X;
static int32_t g_ss_vel_y           = SCREENSAVER_SPEED_Y;
static uint32_t g_ss_last_update_ms = 0;
static uint16_t g_ss_scale          = 256; // 256 = 100%

// Scaled image dimensions used for bounce boundary calculations
static inline int32_t get_scaled_w(void) { return ((int32_t)BINGUS.header.w * g_ss_scale) / 256; }

static inline int32_t get_scaled_h(void) { return ((int32_t)BINGUS.header.h * g_ss_scale) / 256; }

static inline int32_t get_max_x(void) {
    return lv_display_get_horizontal_resolution(NULL) - get_scaled_w();
}

static inline int32_t get_max_y(void) {
    return lv_display_get_vertical_resolution(NULL) - get_scaled_h();
}

static void screensaver_timer_cb(lv_timer_t* timer) {
    (void)timer;

    if (!g_ss_active || !g_ss_img) {
        return;
    }

    uint32_t now_ms = lv_tick_get();
    if (g_ss_last_update_ms == 0) {
        g_ss_last_update_ms = now_ms;
        return;
    }

    uint32_t dt_ms      = now_ms - g_ss_last_update_ms;
    g_ss_last_update_ms = now_ms;

    if (dt_ms > 100) {
        dt_ms = 100;
    }

    g_ss_pos_x += (g_ss_vel_x * (int32_t)dt_ms) / 1000;
    g_ss_pos_y += (g_ss_vel_y * (int32_t)dt_ms) / 1000;

    int32_t max_x = get_max_x();
    int32_t max_y = get_max_y();

    // Clamp max to 0 in case image is larger than screen
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }

    if (g_ss_pos_x <= 0) {
        g_ss_pos_x = 0;
        g_ss_vel_x = SCREENSAVER_SPEED_X;
    } else if (g_ss_pos_x >= max_x) {
        g_ss_pos_x = max_x;
        g_ss_vel_x = -SCREENSAVER_SPEED_X;
    }

    if (g_ss_pos_y <= 0) {
        g_ss_pos_y = 0;
        g_ss_vel_y = SCREENSAVER_SPEED_Y;
    } else if (g_ss_pos_y >= max_y) {
        g_ss_pos_y = max_y;
        g_ss_vel_y = -SCREENSAVER_SPEED_Y;
    }

    lv_obj_set_pos(g_ss_img, g_ss_pos_x, g_ss_pos_y);
}

void screensaver_init(uint16_t scale) {
    g_ss_scale = scale;

    // Dark background
    g_ss_bg_rect = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_ss_bg_rect, LV_PCT(100), LV_PCT(100));
    lv_obj_set_pos(g_ss_bg_rect, 0, 0);
    lv_obj_set_style_bg_color(g_ss_bg_rect, SCREENSAVER_BG_COLOR, 0);
    lv_obj_set_style_bg_opa(g_ss_bg_rect, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(g_ss_bg_rect, 0, 0);
    lv_obj_remove_flag(g_ss_bg_rect, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_ss_bg_rect, LV_OBJ_FLAG_HIDDEN);

    // Image object
    g_ss_img = lv_image_create(lv_layer_top());
    lv_image_set_src(g_ss_img, &BINGUS);
    lv_image_set_scale(g_ss_img, g_ss_scale);
    // Pivot at top-left so position math is straightforward
    lv_image_set_pivot(g_ss_img, 0, 0);
    lv_obj_set_pos(g_ss_img, g_ss_pos_x, g_ss_pos_y);
    lv_obj_remove_flag(g_ss_img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_ss_img, LV_OBJ_FLAG_HIDDEN);

    g_ss_timer          = lv_timer_create(screensaver_timer_cb, 16, NULL);
    g_ss_active         = false;
    g_ss_last_update_ms = 0;
}

void screensaver_show(void) {
    if (g_ss_bg_rect && g_ss_img) {
        g_ss_active         = true;
        g_ss_last_update_ms = lv_tick_get();
        lv_obj_remove_flag(g_ss_bg_rect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_ss_img, LV_OBJ_FLAG_HIDDEN);
    }
}

void screensaver_hide(void) {
    if (g_ss_bg_rect && g_ss_img) {
        g_ss_active = false;
        lv_obj_add_flag(g_ss_bg_rect, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g_ss_img, LV_OBJ_FLAG_HIDDEN);
        g_ss_last_update_ms = 0;
    }
}

bool screensaver_is_active(void) { return g_ss_active; }
