#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include <string.h>

static const char *TAG = "wifi_manager";

static volatile WifiManagerState current_state = WIFI_MANAGER_STATE_IDLE;

static bool initialized = false;

static int retry_count = 0;
static int64_t next_retry_time = 0;
static bool retry_pending = false;

static WifiManagerConfig cfg = {
    .max_retries = 10, .base_retry_ms = 500, .max_retry_ms = 10000};

static WifiManagerEventCb user_cb = NULL;

static void set_state(WifiManagerState state) {
    if (current_state != state) {
        current_state = state;
        if (user_cb)
            user_cb(state);
    }
}

static int get_backoff_delay_ms(void) {
    int delay = cfg.base_retry_ms << retry_count;
    if (delay > cfg.max_retry_ms)
        delay = cfg.max_retry_ms;
    return delay;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        set_state(WIFI_MANAGER_STATE_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        set_state(WIFI_MANAGER_STATE_DISCONNECTED);

        if (retry_count < cfg.max_retries) {
            int delay = get_backoff_delay_ms();
            next_retry_time = esp_timer_get_time() + (delay * 1000);
            retry_pending = true;
            ESP_LOGW(TAG, "Disconnected, retry in %d ms", delay);
            retry_count++;
        } else {
            ESP_LOGE(TAG, "Max retries reached");
            set_state(WIFI_MANAGER_STATE_FAILED);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        retry_count = 0;
        retry_pending = false;
        set_state(WIFI_MANAGER_STATE_CONNECTED);
    }
}

int wifi_manager_start(const char *ssid, const char *password,
                       const WifiManagerConfig *user_cfg) {
    if (initialized) {
        ESP_LOGE(TAG, "Wi-Fi manager already initialized");
        return -1;
    }

    if (user_cfg)
        cfg = *user_cfg;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid,
            sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password,
            sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    set_state(WIFI_MANAGER_STATE_CONNECTING);
    initialized = true;
    return 0;
}

void wifi_manager_stop(void) {
    esp_wifi_stop();
    retry_pending = false;
    retry_count = 0;
    set_state(WIFI_MANAGER_STATE_IDLE);
    initialized = false;
}

void wifi_manager_poll(void) {
    if (!retry_pending)
        return;

    int64_t now = esp_timer_get_time();
    if (now >= next_retry_time) {
        ESP_LOGI(TAG, "Retrying Wi-Fi connection...");
        retry_pending = false;
        set_state(WIFI_MANAGER_STATE_CONNECTING);
        esp_wifi_connect();
    }
}

WifiManagerState wifi_manager_get_state(void) { return current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { user_cb = cb; }
