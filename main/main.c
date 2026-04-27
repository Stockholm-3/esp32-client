#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen_timeout.h"
#include "ui.h"

static const char* g_tag = "main";

void app_main(void) {
    lv_display_t* disp = NULL;
    lv_indev_t* touch  = NULL;

    ESP_ERROR_CHECK(display_init(&disp, &touch));
    ESP_LOGI(g_tag, "Display initialized");

    if (!display_lvgl_lock(-1)) {
        ESP_LOGE(g_tag, "Failed to acquire LVGL lock");
        return;
    }

    ui_build(disp);
    screen_timeout_init(5U * 60U);
    display_set_activity_callback(screen_timeout_record_activity);
    display_lvgl_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
