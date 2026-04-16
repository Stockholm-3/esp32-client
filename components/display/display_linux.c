#include "display.h"
#include "esp_log.h"
#include "lvgl.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

static const char* TAG = "display_sim";

// --- Private State ---
static SDL_Window* g_window       = NULL;
static SDL_Renderer* g_renderer   = NULL;
static SDL_Texture* g_texture     = NULL;
static uint8_t* g_fb              = NULL;
static pthread_mutex_t g_lvgl_mux = PTHREAD_MUTEX_INITIALIZER;
static bool g_setup_complete      = false;

static int16_t g_mouse_x    = 0;
static int16_t g_mouse_y    = 0;
static bool g_mouse_pressed = false;

// --- Internal SDL/LVGL Logic ---

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t w          = area->x2 - area->x1 + 1;
    int32_t h          = area->y2 - area->y1 + 1;
    int32_t bpp        = 2; // RGB565
    int32_t dst_stride = WS7B_LCD_H_RES * bpp;

    for (int32_t row = 0; row < h; row++) {
        uint8_t* dst = g_fb + ((area->y1 + row) * dst_stride) + area->x1 * bpp;
        uint8_t* src = px_map + (row * w * bpp);
        memcpy(dst, src, (size_t)(w * bpp));
    }

    if (lv_display_flush_is_last(disp)) {
        SDL_UpdateTexture(g_texture, NULL, g_fb, dst_stride);
        SDL_RenderClear(g_renderer);
        SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
        SDL_RenderPresent(g_renderer);
    }
    lv_display_flush_ready(disp);
}

static void lvgl_mouse_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    data->point.x = g_mouse_x;
    data->point.y = g_mouse_y;
    data->state   = g_mouse_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void* simulation_task(void* arg) {
    // 1. Initialize SDL inside the thread
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        ESP_LOGE(TAG, "SDL_Init Error: %s", SDL_GetError());
        return NULL;
    }

    g_window =
        SDL_CreateWindow("ESP32-Client Simulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         WS7B_LCD_H_RES, WS7B_LCD_V_RES, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    if (!g_window) {
        ESP_LOGE(TAG, "Window Error: %s", SDL_GetError());
        return NULL;
    }

    // Using Software renderer for maximum compatibility across Linux distros/VMs
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    g_texture  = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STREAMING,
                                   WS7B_LCD_H_RES, WS7B_LCD_V_RES);

    g_fb = calloc(1, WS7B_LCD_H_RES * WS7B_LCD_V_RES * 2);

    g_setup_complete = true;

    SDL_Event e;
    while (1) {
        // Increment LVGL Ticks (5ms increments)
        lv_tick_inc(5);

        // Run LVGL Timer Handler
        if (display_lvgl_lock(-1)) {
            lv_timer_handler();
            display_lvgl_unlock();
        }

        // Handle SDL Events
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                exit(0);

            if (e.type == SDL_MOUSEMOTION) {
                g_mouse_x = (int16_t)e.motion.x;
                g_mouse_y = (int16_t)e.motion.y;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                g_mouse_pressed = true;
            }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) {
                g_mouse_pressed = false;
            }
        }

        usleep(5000); // 5ms sleep
    }
    return NULL;
}

// --- Public Interface ---

esp_err_t display_init(lv_display_t** disp_out, lv_indev_t** touch_out) {
    lv_init();

    // Create the background simulation thread
    pthread_t thread;
    if (pthread_create(&thread, NULL, simulation_task, NULL) != 0) {
        return ESP_FAIL;
    }
    pthread_detach(thread);

    // Wait for SDL to finish setup in the other thread before creating LVGL objects
    while (!g_setup_complete) {
        usleep(10000);
    }

    // Create LVGL Display object
    size_t buf_bytes   = WS7B_LCD_H_RES * 40 * 2;
    void* buf1         = malloc(buf_bytes);
    lv_display_t* disp = lv_display_create(WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    lv_display_set_buffers(disp, buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    // Create LVGL Input object
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_mouse_cb);

    if (disp_out)
        *disp_out = disp;
    if (touch_out)
        *touch_out = indev;

    ESP_LOGI(TAG, "Simulation UI Thread started successfully");
    return ESP_OK;
}

bool display_lvgl_lock(int timeout_ms) { return pthread_mutex_lock(&g_lvgl_mux) == 0; }

void display_lvgl_unlock(void) { pthread_mutex_unlock(&g_lvgl_mux); }

void display_set_backlight(uint8_t brightness) { (void)brightness; }
uint32_t display_get_idle_percent(void) { return 0; }
