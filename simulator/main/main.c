/*
 * simulator/main/main.c
 *
 * Mirror of the hardware app_main — same structure, same call order.
 * The sim_board aliases (ws7b_board_init, ws7b_lvgl_lock/unlock) make
 * this file identical in logic to main/main.c on the real board.
 */
#include "esp_log.h"
#include "lvgl.h"
#include "sim_board.h"
#include "ui.h"

static const char *TAG = "main";

void app_main(void) {
    lv_disp_t *disp = NULL;
    lv_indev_t *mouse = NULL;

    if (sim_board_init(&disp, &mouse) != ESP_OK) {
        ESP_LOGE(TAG, "sim_board_init failed");
        return;
    }

    /* Build the UI under the LVGL lock — same pattern as hardware main.c */
    if (!sim_board_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    ui_build(disp);
    sim_board_lvgl_unlock();

    ESP_LOGI(TAG, "UI built, entering main loop");

    /* Runs forever — handles SDL events + drives lv_timer_handler */
    sim_board_loop();
}
