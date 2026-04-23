/*
 * display_esp.c — LVGL wiring for the Waveshare 7" ESP32-S3 board.
 *
 * This file is concerned exclusively with LVGL.  All hardware bring-up is
 * handled by ws7b_board; this file retrieves the resulting handles and uses
 * them to configure the LVGL display and input subsystems.
 */

#include "display.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ws7b_board.h"

#include <string.h>

static const char* g_tag = "display";

static SemaphoreHandle_t g_s_lvgl_mux = NULL;
static SemaphoreHandle_t g_s_vsync    = NULL;

/*
 * Called from ISR context by the RGB panel driver at the end of every frame.
 * Signals the flush callback that it is safe to swap the active framebuffer.
 */
static IRAM_ATTR bool on_frame_buf_complete(esp_lcd_panel_handle_t panel,
                                            const esp_lcd_rgb_panel_event_data_t* edata,
                                            void* user_ctx) {
    (void)panel;
    (void)edata;
    (void)user_ctx;

    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(g_s_vsync, &need_yield);
    return need_yield == pdTRUE;
}

/*
 * RENDER_MODE_DIRECT issues one flush call per dirty region per frame.
 * Only the final call (lv_display_flush_is_last) triggers the full-screen
 * zero-copy buffer swap and vsync wait; earlier calls just acknowledge so
 * LVGL can continue rendering the remaining dirty regions.
 */
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    (void)area;

    if (lv_display_flush_is_last(disp)) {
        esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
        esp_lcd_panel_draw_bitmap(panel, 0, 0, WS7B_LCD_H_RES, WS7B_LCD_V_RES, px_map);

        /*
         * Drain any vsync that arrived before draw_bitmap completed, then
         * block until the new frame is confirmed on-screen.
         */
        xSemaphoreTake(g_s_vsync, 0);
        xSemaphoreTake(g_s_vsync, portMAX_DELAY);
    }

    lv_display_flush_ready(disp);
}

static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    esp_lcd_touch_handle_t tp            = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    uint8_t cnt                          = 0;
    esp_lcd_touch_point_data_t points[1] = {0};

    esp_lcd_touch_read_data(tp);
    esp_lcd_touch_get_data(tp, points, &cnt, 1);

    if (cnt > 0) {
        data->point.x = (int32_t)points[0].x;
        data->point.y = (int32_t)points[0].y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void* arg) {
    (void)arg;
    lv_tick_inc(WS7B_LVGL_TICK_MS);
}

static void lvgl_task(void* arg) {
    (void)arg;
    ESP_LOGI(g_tag, "LVGL task running");

    uint32_t delay_ms = WS7B_LVGL_TASK_MAX_DELAY_MS;

    while (1) {
        if (display_lvgl_lock(-1)) {
            delay_ms = lv_timer_handler();
            display_lvgl_unlock();
        }

        if (delay_ms > WS7B_LVGL_TASK_MAX_DELAY_MS) {
            delay_ms = WS7B_LVGL_TASK_MAX_DELAY_MS;
        }
        if (delay_ms < WS7B_LVGL_TASK_MIN_DELAY_MS) {
            delay_ms = WS7B_LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static lv_display_t* setup_lvgl_display(esp_lcd_panel_handle_t panel) {
    lv_display_t* disp = lv_display_create(WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    assert(disp);

    /*
     * Retrieve the two PSRAM framebuffers owned by the RGB panel driver and
     * hand them directly to LVGL.  RENDER_MODE_DIRECT lets LVGL render
     * straight into these buffers; the flush callback does a zero-copy
     * vsync-locked swap rather than a memcpy.
     */
    void* buf1 = NULL;
    void* buf2 = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &buf1, &buf2));
    size_t fb_bytes = (size_t)WS7B_LCD_H_RES * WS7B_LCD_V_RES * sizeof(lv_color_t);

    lv_display_set_buffers(disp, buf1, buf2, fb_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, panel);

    return disp;
}

static lv_indev_t* setup_lvgl_touch(esp_lcd_touch_handle_t touch) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
    lv_indev_set_user_data(indev, touch);
    return indev;
}

/* NOLINTNEXTLINE(readability-function-cognitive-complexity) */
esp_err_t display_init(lv_display_t** disp_out, lv_indev_t** touch_out) {
    ESP_RETURN_ON_ERROR(ws7b_board_init(), g_tag, "Board init failed");

    esp_lcd_panel_handle_t panel = ws7b_board_get_panel();
    esp_lcd_touch_handle_t touch = ws7b_board_get_touch();

    g_s_vsync = xSemaphoreCreateBinary();
    assert(g_s_vsync);

    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = on_frame_buf_complete,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel, &cbs, NULL));

    lv_init();

    /* Periodic tick — drives lv_tick_inc() from a high-resolution timer */
    const esp_timer_create_args_t TICK_ARGS = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&TICK_ARGS, &tick_timer));
    /* Cast to uint64_t before multiplying to avoid implicit int widening */
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, (uint64_t)WS7B_LVGL_TICK_MS * 1000U));

    /*
     * Recursive mutex — allows nested display_lvgl_lock() calls from the
     * same task (e.g. a callback that itself calls into LVGL) without
     * deadlocking.
     */
    g_s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(g_s_lvgl_mux);

    lv_display_t* disp = setup_lvgl_display(panel);
    lv_indev_t* indev  = touch ? setup_lvgl_touch(touch) : NULL;

    BaseType_t ret = xTaskCreatePinnedToCore(lvgl_task, "lvgl", WS7B_LVGL_TASK_STACK, NULL,
                                             WS7B_LVGL_TASK_PRIORITY, NULL, tskNO_AFFINITY);

    if (ret != pdPASS) {
        ESP_LOGE(g_tag, "LVGL task create failed");
        return ESP_FAIL;
    }

    display_set_backlight(255);

    if (disp_out) {
        *disp_out = disp;
    }
    if (touch_out) {
        *touch_out = indev;
    }

    ESP_LOGI(g_tag, "Display ready");
    return ESP_OK;
}

void display_set_backlight(uint8_t brightness) { ws7b_board_set_backlight(brightness); }

bool display_lvgl_lock(int timeout_ms) {
    assert(g_s_lvgl_mux);
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(g_s_lvgl_mux, ticks) == pdTRUE;
}

void display_lvgl_unlock(void) {
    assert(g_s_lvgl_mux);
    xSemaphoreGiveRecursive(g_s_lvgl_mux);
}

uint32_t display_get_idle_percent(void) {
    static uint32_t s_last_idle  = 0;
    static uint32_t s_last_total = 0;

    UBaseType_t count   = uxTaskGetNumberOfTasks();
    TaskStatus_t* tasks = pvPortMalloc(count * sizeof(TaskStatus_t));
    if (!tasks) {
        return 0;
    }

    uint32_t total_time = 0;
    count               = uxTaskGetSystemState(tasks, count, &total_time);

    uint32_t idle = 0;
    for (UBaseType_t i = 0; i < count; i++) {
        if (strncmp(tasks[i].pcTaskName, "IDLE", 4) == 0) {
            idle += tasks[i].ulRunTimeCounter;
        }
    }
    vPortFree(tasks);

    uint32_t d_idle  = idle - s_last_idle;
    uint32_t d_total = total_time - s_last_total;
    s_last_idle      = idle;
    s_last_total     = total_time;

    return (d_total == 0) ? 0 : (d_idle * 100U) / d_total;
}
