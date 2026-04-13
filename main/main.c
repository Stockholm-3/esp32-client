#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ui.h"
#include "ws7b_board.h"

static const char* g_tag = "main";

void app_main(void) {
    lv_display_t* disp = NULL;
    lv_indev_t* touch  = NULL;

    ESP_ERROR_CHECK(ws7b_board_init(&disp, &touch));
    ESP_LOGI(g_tag, "Display initialized");

    if (!ws7b_lvgl_lock(-1)) {
        ESP_LOGE(g_tag, "Failed to acquire LVGL lock");
        return;
    }

    ui_build(disp);
    ws7b_lvgl_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
