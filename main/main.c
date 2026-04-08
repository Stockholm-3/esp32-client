#include "esp_event.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http.h"
#include "ui.h"
#include "wifi.h"
#include "ws7b_board.h"

static const char *TAG = "main";

void wifi_task(void *arg) {

    wifi_init("ssid", "password");

    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Wi-Fi connected");

    vTaskDelete(NULL);
}

void http_task(void *arg) {
    while (!wifi_is_connected()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Starting HTTP request...");

    uint8_t buffer[1024];

    http_response_t resp = {
        .buffer = buffer, .buffer_size = sizeof(buffer), .length = 0};

    if (http_get("http://httpbin.org/get", &resp) == ESP_OK) {
        ESP_LOGI(TAG, "HTTP received %d bytes", (int)resp.length);

        if (resp.length < resp.buffer_size) {
            buffer[resp.length] = '\0';
            ESP_LOGI(TAG, "Response preview: %.100s", buffer);
        }
    }

    vTaskDelete(NULL);
}

void app_main(void) {
    lv_disp_t *disp = NULL;
    lv_indev_t *touch = NULL;

    ESP_ERROR_CHECK(ws7b_board_init(&disp, &touch));
    ESP_LOGI(TAG, "Display initialized");

    if (!ws7b_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to acquire LVGL lock");
        return;
    }

    ui_build(disp);
    ws7b_lvgl_unlock();

    xTaskCreate(wifi_task, "wifi_task", 4096, NULL, 5, NULL);
    xTaskCreate(http_task, "http_task", 8192, NULL, 5, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
