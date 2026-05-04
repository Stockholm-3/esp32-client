#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "wifi_manager.h"

#include <stdlib.h>
#include <string.h>

static const char* g_tag = "wifi_manager";

static volatile WifiManagerState g_current_state = WIFI_MANAGER_STATE_IDLE;

static bool g_initialized              = false;
static bool g_scan_active              = false;
static int g_retry_count               = 0;
static TimerHandle_t g_retry_timer     = NULL;
static WifiManagerScanDoneCb g_scan_cb = NULL;
static WifiManagerEventCb g_user_cb    = NULL;

static WifiManagerConfig g_cfg = {
    .max_retries   = 10,
    .base_retry_ms = 500,
    .max_retry_ms  = 10000,
};

static void set_state(WifiManagerState state, WifiManagerFailReason reason) {
    if (g_current_state != state) {
        g_current_state = state;
        if (g_user_cb) {
            g_user_cb(state, reason);
        }
    }
}

static int get_backoff_delay_ms(void) {
    int multiplier = 1;
    for (int i = 0; i < g_retry_count; i++) {
        multiplier *= 2;
        if (multiplier * g_cfg.base_retry_ms >= g_cfg.max_retry_ms) {
            return g_cfg.max_retry_ms;
        }
    }
    return multiplier * g_cfg.base_retry_ms;
}

static void schedule_retry(void) {
    if (g_retry_count >= g_cfg.max_retries) {
        g_retry_count = 0;
    }
    int delay = get_backoff_delay_ms();
    ESP_LOGW(g_tag, "Retry %d in %d ms", g_retry_count + 1, delay);
    g_retry_count++;
    xTimerChangePeriod(g_retry_timer, pdMS_TO_TICKS(delay), 0);
    xTimerStart(g_retry_timer, 0);
}

static void retry_timer_cb(TimerHandle_t timer) {
    (void)timer;
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    esp_wifi_connect();
}

static void deliver_scan_results(void) {
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count == 0 || g_scan_cb == NULL) {
        g_scan_cb = NULL;
        return;
    }

    wifi_ap_record_t* records = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (!records) {
        ESP_LOGE(g_tag, "Scan result alloc failed");
        g_scan_cb = NULL;
        return;
    }

    if (esp_wifi_scan_get_ap_records(&ap_count, records) != ESP_OK) {
        ESP_LOGE(g_tag, "Failed to retrieve scan records");
        free(records);
        g_scan_cb = NULL;
        return;
    }

    WifiManagerApInfo* results = calloc(ap_count, sizeof(WifiManagerApInfo));
    if (!results) {
        ESP_LOGE(g_tag, "AP info alloc failed");
        free(records);
        g_scan_cb = NULL;
        return;
    }

    for (uint16_t i = 0; i < ap_count; i++) {
        strncpy(results[i].ssid, (char*)records[i].ssid, sizeof(results[i].ssid) - 1);
        results[i].rssi     = records[i].rssi;
        results[i].authmode = (uint8_t)records[i].authmode;
    }

    WifiManagerScanDoneCb cb = g_scan_cb;
    g_scan_cb                = NULL;
    cb(results, ap_count);

    free(results);
    free(records);
}

static bool is_auth_failure(uint8_t reason) {
    switch (reason) {
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_MIC_FAILURE:
        return true;
    default:
        return false;
    }
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* info = (wifi_event_sta_disconnected_t*)event_data;

            if (info->reason == WIFI_REASON_NO_AP_FOUND) {
                ESP_LOGE(g_tag, "SSID not found");
                xTimerStop(g_retry_timer, 0);
                g_retry_count = 0;
                set_state(WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_FAIL_REASON_NO_AP);
            } else if (is_auth_failure(info->reason)) {
                ESP_LOGE(g_tag, "Auth failure, reason: %d", info->reason);
                xTimerStop(g_retry_timer, 0);
                g_retry_count = 0;
                set_state(WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_FAIL_REASON_AUTH);
            } else {
                ESP_LOGW(g_tag, "Disconnected, reason: %d", info->reason);
                set_state(WIFI_MANAGER_STATE_DISCONNECTED, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
                schedule_retry();
            }
            break;
        }

        case WIFI_EVENT_SCAN_DONE:
            g_scan_active = false;
            deliver_scan_results();
            if (g_current_state == WIFI_MANAGER_STATE_SCANNING) {
                set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
            }
            break;

        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(g_tag, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        g_retry_count = 0;
        xTimerStop(g_retry_timer, 0);
        set_state(WIFI_MANAGER_STATE_CONNECTED, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    }
}

int wifi_manager_start(const char* ssid, const char* password, const WifiManagerConfig* config) {
    if (g_initialized) {
        ESP_LOGE(g_tag, "Already initialized");
        return -1;
    }

    if (config) {
        g_cfg = *config;
    }

    g_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(g_cfg.base_retry_ms), pdFALSE, NULL,
                                 retry_timer_cb);
    if (!g_retry_timer) {
        ESP_LOGE(g_tag, "Failed to create retry timer");
        return -1;
    }

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    g_initialized = true;
    return 0;
}

void wifi_manager_stop(void) {
    xTimerStop(g_retry_timer, 0);
    xTimerDelete(g_retry_timer, 0);
    g_retry_timer = NULL;

    esp_wifi_stop();
    g_retry_count = 0;
    g_scan_active = false;
    g_scan_cb     = NULL;
    g_initialized = false;
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
}

void wifi_manager_reconnect(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "Not initialized");
        return;
    }
    xTimerStop(g_retry_timer, 0);
    g_retry_count = 0;
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    esp_wifi_connect();
}

int wifi_manager_change_network(const char* ssid, const char* password) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return -1;
    }

    ESP_LOGI(g_tag, "Changing network to '%s'", ssid);

    xTimerStop(g_retry_timer, 0);
    g_retry_count = 0;
    esp_wifi_disconnect();

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    esp_wifi_connect();
    return 0;
}

int wifi_manager_scan(WifiManagerScanDoneCb cb) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return -1;
    }
    if (g_scan_active) {
        ESP_LOGW(g_tag, "Scan already in progress");
        return -1;
    }
    if (!cb) {
        ESP_LOGE(g_tag, "Scan callback must not be NULL");
        return -1;
    }

    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, false);
    if (err != ESP_OK) {
        ESP_LOGE(g_tag, "Scan start failed: %s", esp_err_to_name(err));
        return -1;
    }

    g_scan_cb     = cb;
    g_scan_active = true;
    set_state(WIFI_MANAGER_STATE_SCANNING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    return 0;
}

WifiManagerState wifi_manager_get_state(void) { return g_current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { g_user_cb = cb; }
