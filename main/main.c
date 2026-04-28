#include "bme280_sensor.h"
#include "display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen_timeout.h"
#include "settings_manager.h"
#include "time_manager.h"
#include "ui.h"
#include "ui_binder.h"
#include "wifi_manager.h"
#include "wifi_popup.h"
#include "ws7b_board.h"

static const char* g_tag = "main";

static char g_current_ssid[33] = "";

static void on_bme280_sample(const Bme280Reading* reading, void* user_ctx) {
    (void)user_ctx;
    ESP_LOGI(g_tag, "BME280 — temp: %.2f °C  pressure: %.2f hPa  humidity: %.2f %%RH",
             (double)reading->temperature_c, (double)reading->pressure_hpa,
             (double)reading->humidity_pct);
}

static void on_wifi_connect(const char* ssid, const char* password) {
    strncpy(g_current_ssid, ssid, 32);
    g_current_ssid[32] = '\0';
    wifi_manager_stop();
    wifi_manager_start(ssid, password, NULL);
    settings_manager_save_wifi(ssid, password);
}

static void on_wifi_state(WifiManagerState state) {
    ui_binder_update_wifi_status(state);
    if (state == WIFI_MANAGER_STATE_CONNECTED) {
        ui_binder_update_wifi_name(g_current_ssid);
    }
}

void app_main(void) {
    lv_display_t* disp = NULL;
    lv_indev_t* touch  = NULL;
    ESP_ERROR_CHECK(display_init(&disp, &touch));
    ESP_LOGI(g_tag, "Display initialized");

    esp_err_t bme_err =
        bme280_sensor_init_with_task(ws7b_board_get_i2c_bus(), on_bme280_sample, NULL);
    if (bme_err != ESP_OK) {
        ESP_LOGW(g_tag, "BME280 not found, skipping (%s)", esp_err_to_name(bme_err));
    } else {
        ESP_LOGI(g_tag, "BME280 initialized");
    }

    if (!display_lvgl_lock(-1)) {
        ESP_LOGE(g_tag, "Failed to acquire LVGL lock");
        return;
    }
    ui_build(disp);
    screen_timeout_init(5U * 60U);
    display_set_activity_callback(screen_timeout_record_activity);
    ui_binder_init();
    display_lvgl_unlock();

    wifi_manager_hw_preinit();
    settings_manager_init();
    time_manager_init(NULL);

    wifi_popup_on_connect(on_wifi_connect);
    wifi_manager_register_callback(on_wifi_state);

    const char* ssid = settings_manager_get_ssid();
    const char* pass = settings_manager_get_password();
    if (ssid[0] != '\0') {
        wifi_manager_start(ssid, pass, NULL);
    }

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        struct tm timeinfo;
        if (time_manager_get_time(&timeinfo)) {
            ui_binder_update_localtime(&timeinfo);
        }
    }
}
