#include "display.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
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
    ui_binder_init();
    display_lvgl_unlock();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
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
