#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui.h"
#include "ws7b_board.h"

static const char *TAG = "main";

void app_main(void) {
    lv_disp_t *disp = NULL;
    lv_indev_t *touch = NULL;
    ESP_ERROR_CHECK(ws7b_board_init(&disp, &touch));
    ESP_LOGI(TAG, "init done, heap free: %lu", esp_get_free_heap_size());

    if (!ws7b_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }
    ui_build(disp);
    ws7b_lvgl_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
