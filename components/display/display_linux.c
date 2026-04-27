/*
 * display_linux.c — SDL2-backed LVGL display for the Linux simulator target.
 *
 * The SDL event loop and lv_timer_handler() run on a background pthread so
 * that app_main() can proceed on the main thread exactly as it would on the
 * ESP target.  display_lvgl_lock/unlock() guard all LVGL calls with a
 * pthread mutex.
 */

#include "display.h"
#include "esp_log.h"
#include "lvgl.h"
#include "ws7b_config.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char* TAG = "display_sim";

static SDL_Window* s_window     = NULL;
static SDL_Renderer* s_renderer = NULL;
static SDL_Texture* s_texture   = NULL;
static uint8_t* s_fb            = NULL;

/*
 * Written by the SDL event loop, read by the LVGL indev callback.
 * Declared volatile; access does not need a mutex because int16_t/bool writes
 * are atomic on all platforms this runs on and partial reads cannot cause
 * functional harm (worst case: one frame of stale coordinates).
 */
static volatile int16_t s_mouse_x    = 0;
static volatile int16_t s_mouse_y    = 0;
static volatile bool s_mouse_pressed = false;

static pthread_mutex_t s_lvgl_mux = PTHREAD_MUTEX_INITIALIZER;
static volatile bool s_setup_done = false;

static void (*g_s_activity_cb)(void) = NULL;

void display_set_activity_callback(void (*cb)(void)) { g_s_activity_cb = cb; }

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    const int32_t bpp        = 2; /* RGB565 — 2 bytes per pixel */
    const int32_t dst_stride = WS7B_LCD_H_RES * bpp;
    const int32_t w          = area->x2 - area->x1 + 1;
    const int32_t h          = area->y2 - area->y1 + 1;

    for (int32_t row = 0; row < h; row++) {
        uint8_t* dst = s_fb + (area->y1 + row) * dst_stride + area->x1 * bpp;
        uint8_t* src = px_map + row * w * bpp;
        memcpy(dst, src, (size_t)(w * bpp));
    }

    if (lv_display_flush_is_last(disp)) {
        SDL_UpdateTexture(s_texture, NULL, s_fb, dst_stride);
        SDL_RenderClear(s_renderer);
        SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
        SDL_RenderPresent(s_renderer);
    }

    lv_display_flush_ready(disp);
}

static void lvgl_mouse_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;
    if (s_mouse_pressed) {
        if (g_s_activity_cb) {
            g_s_activity_cb();
        }
    }
    data->point.x = s_mouse_x;
    data->point.y = s_mouse_y;
    data->state   = s_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void* simulation_thread(void* arg) {
    (void)arg;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
        return NULL;
    }

    s_window =
        SDL_CreateWindow("WS7B Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         WS7B_LCD_H_RES, WS7B_LCD_V_RES, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!s_window) {
        ESP_LOGE(TAG, "SDL window error: %s", SDL_GetError());
        return NULL;
    }

    /* Software renderer — widest compatibility across Linux distros and VMs */
    s_renderer = SDL_CreateRenderer(s_window, -1, SDL_RENDERER_SOFTWARE);
    s_texture  = SDL_CreateTexture(s_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                   WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    s_fb       = calloc(1, (size_t)WS7B_LCD_H_RES * WS7B_LCD_V_RES * 2);

    s_setup_done = true; /* Unblock display_init() */

    SDL_Event e;
    while (1) {
        lv_tick_inc(5);

        if (display_lvgl_lock(-1)) {
            lv_timer_handler();
            display_lvgl_unlock();
        }

        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                exit(0);
                break;
            case SDL_MOUSEMOTION:
                s_mouse_x = (int16_t)e.motion.x;
                s_mouse_y = (int16_t)e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT)
                    s_mouse_pressed = true;
                break;
            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT)
                    s_mouse_pressed = false;
                break;
            default:
                break;
            }
        }

        usleep(5000); /* ~5 ms per iteration */
    }

    return NULL;
}

esp_err_t display_init(lv_display_t** disp_out, lv_indev_t** touch_out) {
    /*
     * lv_init() must be called before the SDL thread starts so that
     * lv_timer_handler() inside the thread has a valid LVGL context.
     * lv_display_create() and lv_indev_create() below are then called on
     * the same thread that owns the context.
     */
    lv_init();

    pthread_t thread;
    if (pthread_create(&thread, NULL, simulation_thread, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to create simulation thread");
        return ESP_FAIL;
    }
    pthread_detach(thread);

    /* Spin until SDL has finished window + texture creation */
    while (!s_setup_done) {
        usleep(10000);
    }

    /* LVGL display — partial render mode with a small scratch buffer */
    size_t buf_bytes   = (size_t)WS7B_LCD_H_RES * 40 * sizeof(lv_color_t);
    void* buf1         = malloc(buf_bytes);
    lv_display_t* disp = lv_display_create(WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    lv_display_set_buffers(disp, buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* LVGL mouse input device */
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_mouse_cb);

    if (disp_out)
        *disp_out = disp;
    if (touch_out)
        *touch_out = indev;

    ESP_LOGI(TAG, "Simulator display ready (%dx%d)", WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    return ESP_OK;
}

bool display_lvgl_lock(int timeout_ms) {
    if (timeout_ms < 0) {
        return pthread_mutex_lock(&s_lvgl_mux) == 0;
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }
    return pthread_mutex_timedlock(&s_lvgl_mux, &ts) == 0;
}

void display_lvgl_unlock(void) { pthread_mutex_unlock(&s_lvgl_mux); }

void display_set_backlight(uint8_t brightness) { (void)brightness; }

uint32_t display_get_idle_percent(void) { return 0; }
