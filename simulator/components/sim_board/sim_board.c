/*
 * sim_board.c — SDL2 + LVGL desktop simulator board
 *
 * THE REAL FIX FOR PARTIAL-FLUSH ARTIFACTS:
 *
 * With a partial draw buffer (smaller than the screen), LVGL calls flush_cb
 * many times per frame — once per dirty rectangle. Passing each partial rect
 * straight to SDL_RenderPresent causes visible tearing because SDL presents
 * a half-composited frame.
 *
 * The correct approach (also used inside LVGL's own SDL driver and noted in
 * the v8.3.3 changelog "fix(sdl): clear streaming/target texture with
 * FillRect"):
 *
 *   1. Maintain a full-screen software framebuffer (s_fb) in plain RAM.
 *   2. In flush_cb, copy each dirty rect from LVGL's render buffer into s_fb
 *      row-by-row (respecting stride). This is a pure memcpy — no SDL calls.
 *   3. On the LAST flush of the frame (lv_disp_flush_is_last), upload the
 *      entire s_fb to the SDL texture in one SDL_UpdateTexture call, then
 *      present. The screen always sees a complete, fully-composited frame.
 *
 * Alternatively, setting disp_drv.full_refresh = 1 forces LVGL to always
 * hand you the whole screen in one flush_cb call, which also eliminates the
 * artifact — but wastes CPU redrawing unchanged areas. The framebuffer
 * approach is what pros use: partial rendering, full-frame presentation.
 *
 * Other correctness notes:
 *   - lv_disp_drv_t / lv_indev_drv_t must be static (LVGL holds raw pointers).
 *   - lv_tick_inc() must be called BEFORE lv_timer_handler().
 *   - SDL events run outside the LVGL lock to avoid contention.
 */

#include "sim_board.h"

#include "esp_log.h"
#include "lvgl.h"

#include <SDL2/SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static const char* g_tag = "sim_board";

/* ── SDL handles ─────────────────────────────────────────────────────────── */
static SDL_Window* g_s_window     = NULL;
static SDL_Renderer* g_s_renderer = NULL;
static SDL_Texture* g_s_texture   = NULL;

/* ── Software framebuffer — full screen, always up-to-date ──────────────── */
static lv_color_t* g_s_fb = NULL; /* [SIM_LCD_H_RES * SIM_LCD_V_RES] */

/* ── LVGL partial draw buffer (1/10 screen) ──────────────────────────────── */
static lv_color_t* g_s_buf1 = NULL;
static lv_disp_draw_buf_t g_s_draw_buf;

/* Static — LVGL stores raw pointers to these for the program lifetime */
static lv_disp_drv_t g_s_disp_drv;
static lv_indev_drv_t g_s_indev_drv;

/* ── Input state ─────────────────────────────────────────────────────────── */
static int16_t g_s_mouse_x    = 0;
static int16_t g_s_mouse_y    = 0;
static bool g_s_mouse_pressed = false;

/* ── LVGL mutex ──────────────────────────────────────────────────────────── */
static pthread_mutex_t g_s_lvgl_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── LVGL flush callback ─────────────────────────────────────────────────── */
static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    /*
     * Step 1: Copy this dirty rect from LVGL's render buffer into our
     * full-screen software framebuffer, row by row.
     *
     * LVGL's render buffer holds pixels densely for [area->x1..x2] width,
     * but our framebuffer has full SIM_LCD_H_RES stride.  We must copy
     * one row at a time.
     */
    int32_t w = area->x2 - area->x1 + 1;
    int32_t h = area->y2 - area->y1 + 1;

    for (int32_t row = 0; row < h; row++) {
        lv_color_t* dst = g_s_fb + ((area->y1 + row) * SIM_LCD_H_RES) + area->x1;
        lv_color_t* src = color_p + (row * w);
        memcpy(dst, src, (size_t)w * sizeof(lv_color_t));
    }

    /*
     * Step 2: Only push to SDL on the LAST flush of this frame.
     * At that point s_fb contains the fully composited frame.
     */
    if (lv_disp_flush_is_last(drv)) {
        SDL_UpdateTexture(g_s_texture, NULL, g_s_fb, SIM_LCD_H_RES * (int)sizeof(lv_color_t));
        SDL_RenderClear(g_s_renderer);
        SDL_RenderCopy(g_s_renderer, g_s_texture, NULL, NULL);
        SDL_RenderPresent(g_s_renderer);
    }

    lv_disp_flush_ready(drv);
}

/* ── LVGL input callback → mouse ────────────────────────────────────────── */
static void mouse_read_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    (void)drv;
    data->point.x = g_s_mouse_x;
    data->point.y = g_s_mouse_y;
    data->state   = (int)g_s_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* ── Lock / unlock ───────────────────────────────────────────────────────── */
bool sim_board_lvgl_lock(int timeout_ms) {
    (void)timeout_ms;
    return pthread_mutex_lock(&g_s_lvgl_mutex) == 0;
}

void sim_board_lvgl_unlock(void) { pthread_mutex_unlock(&g_s_lvgl_mutex); }

/* ── Board init ──────────────────────────────────────────────────────────── */
esp_err_t sim_board_init(lv_disp_t** disp_out, lv_indev_t** touch_out) {
    ESP_LOGI(g_tag, "Initializing SDL + LVGL");

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ESP_LOGE(g_tag, "SDL_Init failed: %s", SDL_GetError());
        return ESP_FAIL;
    }

    g_s_window = SDL_CreateWindow("LVGL Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  SIM_LCD_H_RES, SIM_LCD_V_RES, 0);
    assert(g_s_window);

    g_s_renderer = SDL_CreateRenderer(g_s_window, -1, SDL_RENDERER_ACCELERATED);
    assert(g_s_renderer);

    /*
     * RGB565 matches lv_color_t at LV_COLOR_DEPTH=16 (the IDF default).
     * Switch to SDL_PIXELFORMAT_ARGB8888 if you ever move to 32-bit color.
     */
    g_s_texture = SDL_CreateTexture(g_s_renderer, SDL_PIXELFORMAT_RGB565,
                                    SDL_TEXTUREACCESS_STREAMING, SIM_LCD_H_RES, SIM_LCD_V_RES);
    assert(g_s_texture);

    /* Full-screen software framebuffer — zero-initialised (black) */
    g_s_fb = calloc(SIM_LCD_H_RES * SIM_LCD_V_RES, sizeof(lv_color_t));
    assert(g_s_fb);

    /* LVGL partial render buffer — 1/10 screen */
    lv_init();
    g_s_buf1 = malloc(SIM_LCD_H_RES * (SIM_LCD_V_RES / 10) * sizeof(lv_color_t));
    assert(g_s_buf1);
    lv_disp_draw_buf_init(&g_s_draw_buf, g_s_buf1, NULL, SIM_LCD_H_RES * (SIM_LCD_V_RES / 10));

    lv_disp_drv_init(&g_s_disp_drv);
    g_s_disp_drv.hor_res  = SIM_LCD_H_RES;
    g_s_disp_drv.ver_res  = SIM_LCD_V_RES;
    g_s_disp_drv.flush_cb = flush_cb;
    g_s_disp_drv.draw_buf = &g_s_draw_buf;
    /* do NOT set full_refresh — partial rendering + fb accumulation is correct
     */
    lv_disp_t* disp = lv_disp_drv_register(&g_s_disp_drv);
    assert(disp);

    lv_indev_drv_init(&g_s_indev_drv);
    g_s_indev_drv.type    = LV_INDEV_TYPE_POINTER;
    g_s_indev_drv.read_cb = mouse_read_cb;
    lv_indev_t* indev     = lv_indev_drv_register(&g_s_indev_drv);
    assert(indev);

    if (disp_out) {
        *disp_out = disp;
    }
    if (touch_out) {
        *touch_out = indev;
    }

    ESP_LOGI(g_tag, "sim_board ready");
    return ESP_OK;
}

/* ── Main loop ───────────────────────────────────────────────────────────── */
void sim_board_loop(void) {
    SDL_Event e;
    while (1) {
        /* Tick FIRST — lv_timer_handler() needs an up-to-date clock */
        lv_tick_inc(5);

        sim_board_lvgl_lock(-1);
        lv_timer_handler();
        sim_board_lvgl_unlock();

        /* SDL events run outside the LVGL lock — no contention with the UI */
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                exit(0);
            case SDL_MOUSEMOTION:
                g_s_mouse_x = (int16_t)e.motion.x;
                g_s_mouse_y = (int16_t)e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                g_s_mouse_pressed = true;
                break;
            case SDL_MOUSEBUTTONUP:
                g_s_mouse_pressed = false;
                break;
            default:
                break;
            }
        }

        SDL_Delay(5);
    }
}
