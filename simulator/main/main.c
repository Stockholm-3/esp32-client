/*
 * simulator/main/main.c
 *
 * Entry point for the Linux/SDL simulator build.
 * The sim_board header #defines ws7b_board_init / ws7b_lvgl_lock /
 * ws7b_lvgl_unlock to their sim_ equivalents, so this file is
 * intentionally identical in structure to the real main/main.c.
 */

#include "esp_log.h"
#include "lvgl.h"
#include "sim_board.h" // provides ws7b_* aliases
#include "ui.h"

static const char *TAG = "sim_main";

void app_main(void) {
    lv_disp_t *disp = NULL;
    lv_indev_t *touch = NULL;

    ESP_ERROR_CHECK(ws7b_board_init(&disp, &touch));
    ESP_LOGI(TAG, "simulator board init done");

    if (!ws7b_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    ui_build(disp);
    ws7b_lvgl_unlock();

    // esp_lvgl_port runs the LVGL task internally; just keep main alive.
    while (1) {
        // On Linux the port's internal task drives lv_timer_handler.
        // A short sleep here prevents busy-looping in app_main.
        struct timespec ts = {.tv_sec = 1, .tv_nsec = 0};
        nanosleep(&ts, NULL);
    }
}
