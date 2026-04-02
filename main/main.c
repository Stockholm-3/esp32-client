#include "esp_chip_info.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

static const char *TAG = "workshop";

void app_main(void) {

    ESP_LOGI(TAG, "TEST LOG BINGUS");
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "Chip: ESP32 with %d CPU cores", chip_info.cores);
    ESP_LOGI(TAG, "WiFi:%s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? " 802.11bgn" : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? " BLE" : "");
    ESP_LOGI(TAG, "Silicon revision: %d", chip_info.revision);

    printf("\n---------------------\n");
    uint32_t size = esp_get_free_heap_size();
    printf("Total: %2.2fkB\n", (float)(size) / 1024);
    printf("Largest free block: %u bytes\n",
           heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    printf("Internal: %2.2fkB\n", (float)(size) / 1024);
    size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    printf("External: %2.2fkB\n", (float)(size) / 1024);
    printf("---------------------\n\n");

    int i = 0;
    while (1) {
        printf("[%d] Hello world!\n", i);
        i++;
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
