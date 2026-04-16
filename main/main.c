#include "env_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"
#include "ws7b_board.h"

static const char* g_tag = "main";

void app_main(void) {
    lv_disp_t* disp   = NULL;
    lv_indev_t* touch = NULL;

    esp_err_t ret = ws7b_board_init(&disp, &touch);
    if (ret != ESP_OK) {
        ESP_LOGE(g_tag, "Board init failed: %s", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }

    sensor_init();
    ESP_LOGI(g_tag, "Display initialized");

    if (!ws7b_lvgl_lock(5000)) {
        ESP_LOGE(g_tag, "Failed to acquire LVGL lock");
        return;
    }

    ui_build(disp);
    ws7b_lvgl_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
