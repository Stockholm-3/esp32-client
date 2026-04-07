#include "sim_board.h"
#include "esp_log.h"
#include "lvgl.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

static const char *TAG = "sim_board";

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *texture = NULL;

static lv_color_t *buf1 = NULL;
static lv_disp_draw_buf_t draw_buf;

static lv_disp_t *disp = NULL;
static lv_indev_t *mouse_indev = NULL;

static int16_t mouse_x = 0, mouse_y = 0;
static bool mouse_pressed = false;

/* Mutex for LVGL */
static pthread_mutex_t lvgl_mutex = PTHREAD_MUTEX_INITIALIZER;

/* LVGL flush callback -> SDL rendering */
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *area,
                     lv_color_t *color_p) {
    if (!texture || !renderer) {
        lv_disp_flush_ready(drv);
        return;
    }

    SDL_Rect rect = {area->x1, area->y1, area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1};

    /* Update the SDL texture using correct pitch */
    SDL_UpdateTexture(texture, &rect, (const void *)color_p,
                      SIM_LCD_H_RES * sizeof(lv_color_t));

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    lv_disp_flush_ready(drv);
}

/* LVGL input device callback -> mouse */
static void mouse_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    data->point.x = mouse_x;
    data->point.y = mouse_y;
    data->state =
        mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* Lock/unlock LVGL */
bool sim_board_lvgl_lock(int timeout_ms) {
    (void)timeout_ms;
    return pthread_mutex_lock(&lvgl_mutex) == 0;
}
void sim_board_lvgl_unlock(void) { pthread_mutex_unlock(&lvgl_mutex); }

/* ────────────── Simulator Init ────────────── */
esp_err_t sim_board_init(lv_disp_t **disp_out, lv_indev_t **touch_out) {
    ESP_LOGI(TAG, "Initializing SDL + LVGL");

    /* SDL Init */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ESP_LOGE(TAG, "SDL_Init failed: %s", SDL_GetError());
        return ESP_FAIL;
    }

    window = SDL_CreateWindow("LVGL Simulator", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SIM_LCD_H_RES,
                              SIM_LCD_V_RES, 0);
    assert(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    assert(renderer);

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING, SIM_LCD_H_RES,
                                SIM_LCD_V_RES);
    assert(texture);

    /* LVGL init */
    lv_init();
    buf1 = malloc(SIM_LCD_H_RES * 100 * sizeof(lv_color_t));
    assert(buf1);
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SIM_LCD_H_RES * 100);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SIM_LCD_H_RES;
    disp_drv.ver_res = SIM_LCD_V_RES;
    disp_drv.flush_cb = flush_cb;
    disp_drv.draw_buf = &draw_buf;
    disp = lv_disp_drv_register(&disp_drv);

    lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = mouse_read_cb;
    mouse_indev = lv_indev_drv_register(&indev_drv);

    if (disp_out)
        *disp_out = disp;
    if (touch_out)
        *touch_out = mouse_indev;

    ESP_LOGI(TAG, "sim_board ready");
    return ESP_OK;
}

/* ────────────── Main simulator loop ────────────── */
void sim_board_loop(void) {
    SDL_Event e;
    while (1) {
        // LVGL ticking
        sim_board_lvgl_lock(-1);
        lv_timer_handler();
        sim_board_lvgl_unlock();
        lv_tick_inc(5);

        // SDL events
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                exit(0);
            case SDL_MOUSEMOTION:
                mouse_x = e.motion.x;
                mouse_y = e.motion.y;
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouse_pressed = true;
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_pressed = false;
                break;
            }
        }

        SDL_Delay(5);
    }
}
