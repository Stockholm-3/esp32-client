// test_main.c - Using your wifi_manager and time_manager
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "time_manager.h"

static const char *TAG = "sntp_test";

static void on_time_synced(TimeState state, struct tm *timeinfo) {
    if (state == TIME_STATE_SYNCED && timeinfo) {
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", timeinfo);
        ESP_LOGI(TAG, "✅ TIME SYNCED: %s", time_str);
    }
}

static void on_wifi_state(WifiManagerState state) {
    if (state == WIFI_MANAGER_STATE_CONNECTED) {
        ESP_LOGI(TAG, "WiFi connected, starting NTP...");
        time_manager_init(on_time_synced);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== SNTP Test ===");
    
    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    // Start WiFi
    wifi_manager_register_callback(on_wifi_state);
    
    WifiManagerConfig cfg = {
        .max_retries = 5,
        .base_retry_ms = 500,
        .max_retry_ms = 5000
    };
    
    wifi_manager_start("YOUR_WIFI_SSID", "YOUR_WIFI_PASSWORD", &cfg);
    
    // Main loop
    while (1) {
        wifi_manager_poll();
        time_manager_poll();
        
        struct tm now;
        if (time_manager_get_time(&now)) {
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &now);
            ESP_LOGI(TAG, "Time: %s", time_str);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}