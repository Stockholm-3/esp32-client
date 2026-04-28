#include "bme280_sensor.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen_timeout.h"
#include "ui.h"
#include "ws7b_board.h"

static const char* g_tag = "main";

static void on_bme280_sample(const Bme280Reading* reading, void* user_ctx) {
    (void)user_ctx;
    ESP_LOGI(g_tag, "BME280 — temp: %.2f °C  pressure: %.2f hPa  humidity: %.2f %%RH",
             (double)reading->temperature_c, (double)reading->pressure_hpa,
             (double)reading->humidity_pct);
}

void app_main(void) {
    lv_display_t* disp = NULL;
    lv_indev_t* touch  = NULL;
    ESP_ERROR_CHECK(display_init(&disp, &touch));
    ESP_LOGI(g_tag, "Display initialized");

    ESP_ERROR_CHECK(bme280_sensor_init_with_task(ws7b_board_get_i2c_bus(), on_bme280_sample, NULL));
    ESP_LOGI(g_tag, "BME280 initialized");

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
