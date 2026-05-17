/**
 * @file main.c
 * @brief Application entry point for the ESP32 client.
 *
 * @details Initialises peripherals (display, BME280, NVS, network stack),
 *          registers Wi-Fi and sensor callbacks, and runs the main task loop.
 */

#include "bme280_sensor.h"
#include "display.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "loc_server.h"
#include "nvs_flash.h"
#include "screen_timeout.h"
#include "settings_manager.h"
#include "smw.h"
#include "time_manager.h"
#include "ui.h"
#include "ui_binder.h"
#include "wifi_manager.h"
#include "wifi_popup.h"
#include "ws7b_board.h"

#ifndef CONFIG_IDF_TARGET_LINUX
#    include "mdns.h"
#endif

/** @brief Maximum number of tasks in the SMW scheduler queue. */
#define SMW_MAX_TASKS 100

/** @brief Log tag for this module. */
static const char* g_tag = "main";

/** @brief SSID of the currently active Wi-Fi network (32 chars + NUL). */
static char g_current_ssid[33] = "";
/** @brief Most recent BME280 sensor reading. */
static Bme280Reading g_bme_reading = {0};
/** @brief Set to true when a new BME280 sample has arrived and not yet consumed. */
static volatile bool g_bme_updated = false;

/** @brief SMW scheduler instance. */
static SmwWorker g_smw_worker;
/** @brief Task array for the SMW scheduler. */
static SmwTask g_smw_tasks[SMW_MAX_TASKS];

/**
 * @brief Returns the current system time in milliseconds.
 * @return Elapsed milliseconds since system start (uint32_t).
 */
uint32_t get_system_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

/**
 * @brief Callback invoked when a new BME280 sample is ready.
 *
 * @param reading   Pointer to the latest sensor reading (temperature, pressure, humidity).
 * @param user_ctx  User-supplied context pointer (unused).
 */
static void on_bme280_sample(const Bme280Reading* reading, void* user_ctx) {
    (void)user_ctx;
    ESP_LOGI(g_tag, "BME280 — temp: %.2f °C  pressure: %.2f hPa  humidity: %.2f %%RH",
             (double)reading->temperature_c, (double)reading->pressure_hpa,
             (double)reading->humidity_pct);
    g_bme_reading = *reading;
    g_bme_updated = true;
}

/**
 * @brief Callback invoked when the user submits Wi-Fi credentials via the popup.
 *
 * @param ssid      Network SSID to connect to.
 * @param password  Network password.
 */
static void on_wifi_connect(const char* ssid, const char* password) {
    strncpy(g_current_ssid, ssid, 32);
    g_current_ssid[32] = '\0';
    wifi_manager_change_network(ssid, password);
    settings_manager_save_wifi(ssid, password);
}

/**
 * @brief Callback invoked on Wi-Fi manager state changes.
 *
 * @param state   New connection state.
 * @param reason  Failure reason (currently unused).
 */
static void on_wifi_state(WifiManagerState state, WifiManagerFailReason reason) {
    (void)reason;
    ui_binder_update_wifi_status(state);
    if (state == WIFI_MANAGER_STATE_CONNECTED) {
        ui_binder_update_wifi_name(g_current_ssid);
        time_manager_init(NULL);
        loc_server_start();

        char ip_str[16] = "---";
#ifndef CONFIG_IDF_TARGET_LINUX
        esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            }
        }
#endif
        ui_binder_update_local_ip(ip_str);
    }
    loc_server_notify_wifi_state(state);
}

static void on_ap_toggled(bool enabled) {
    wifi_manager_set_ap_enabled(enabled);
    if (enabled) {
        loc_server_start();
    }
}

/**
 * @brief Application main function (FreeRTOS entry point).
 *
 * @details Initialisation sequence:
 *  1. NVS Flash
 *  2. Network interface and default event loop
 *  3. Display (LVGL)
 *  4. BME280 sensor (non-fatal if absent)
 *  5. UI, screen timeout, ui_binder
 *  6. Settings manager
 *  7. Wi-Fi manager (restores saved credentials)
 *  8. SMW scheduler
 *  9. Main loop (1 s tick: SMW processing, clock update, BME280 UI refresh)
 */
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifndef CONFIG_IDF_TARGET_LINUX
    mdns_init();
    mdns_instance_name_set("ESP32 Settings");
#endif

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

    settings_manager_init();
    loc_server_init();

    wifi_popup_on_connect(on_wifi_connect);
    wifi_manager_register_callback(on_wifi_state);
    ui_binder_on_ap_enabled_changed2(on_ap_toggled);

#ifndef CONFIG_IDF_TARGET_LINUX
    const char* mdns_host = settings_manager_get_mdns_hostname();
    if (mdns_host[0] != '\0') {
        mdns_hostname_set(mdns_host);
    } else {
        mdns_hostname_set("esp32-client");
    }

    if (settings_manager_get_ap_enabled()) {
        wifi_manager_set_ap_enabled(true);
        loc_server_start();
    }
#endif

    const char* ssid = settings_manager_get_ssid();
    const char* pass = settings_manager_get_password();
    if (ssid[0] != '\0') {
        bool lwc_enabled           = settings_manager_get_local_web_client_enabled();
        WifiManagerConfig wifi_cfg = {0};
        if (lwc_enabled) {
            wifi_cfg.sta_static_ip_enabled = true;
            strncpy(wifi_cfg.sta_ip, settings_manager_get_sta_static_ip(),
                    sizeof(wifi_cfg.sta_ip) - 1);
            strncpy(wifi_cfg.sta_gateway, settings_manager_get_sta_gateway(),
                    sizeof(wifi_cfg.sta_gateway) - 1);
            strncpy(wifi_cfg.sta_netmask, settings_manager_get_sta_netmask(),
                    sizeof(wifi_cfg.sta_netmask) - 1);
        }
        wifi_manager_start(ssid, pass, (int)lwc_enabled ? &wifi_cfg : NULL);
    }

    smw_init(&g_smw_worker, g_smw_tasks, SMW_MAX_TASKS);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        smw_process(&g_smw_worker, get_system_ms());

        struct tm timeinfo;
        if (time_manager_get_time(&timeinfo)) {
            ui_binder_update_localtime(&timeinfo);
        }
        if (g_bme_updated) {
            g_bme_updated = false;
            ui_binder_update_bme280(&g_bme_reading);
        }
    }
}
