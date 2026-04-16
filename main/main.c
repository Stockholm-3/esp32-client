/*
 * simulator/main/main.c
 *
 * Mirror of the hardware app_main — same structure, same call order.
 * The sim_board aliases (ws7b_board_init, ws7b_lvgl_lock/unlock) make
 * this file identical in logic to main/main.c on the real board.
 */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"
#include "time_manager.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "NTP Test - Watch the console output");
    
    // Initialize WiFi
    wifi_manager_register_callback(NULL);  // No callback needed
    
    WifiManagerConfig cfg = {
        .max_retries = 5,
        .base_retry_ms = 500,
        .max_retry_ms = 5000
    };
    
    wifi_manager_start("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD", &cfg);
    
    // Wait for WiFi to connect
    while (wifi_manager_get_state() != WIFI_MANAGER_STATE_CONNECTED) {
        wifi_manager_poll();
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "Waiting for WiFi...");
    }
    
    // WiFi connected, initialize NTP
    ESP_LOGI(TAG, "WiFi connected! Starting NTP...");
    time_manager_init(NULL);
    
    // Wait for time sync
    while (time_manager_get_state() != TIME_STATE_SYNCED) {
        time_manager_poll();
        vTaskDelay(pdMS_TO_TICKS(1000));
        ESP_LOGI(TAG, "Waiting for NTP sync...");
    }
    
    // Success! Print time forever
    ESP_LOGI(TAG, "NTP SYNC SUCCESSFUL!");
    while (1) {
        struct tm now;
        if (time_manager_get_time(&now)) {
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now);
            ESP_LOGI(TAG, "Current time: %s", time_str);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
