#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "settings_manager.h"
#include "time_manager.h"
#include "ui.h"
#include "ui_binder.h"

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
    display_lvgl_unlock();

    ui_binder_init();
    settings_manager_init();
    time_manager_init(NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        struct tm timeinfo;
        if (time_manager_get_time(&timeinfo)) {
            ui_binder_update_localtime(&timeinfo);
        }
    }
}
