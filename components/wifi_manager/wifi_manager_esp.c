#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"
#include "settings_manager.h"
#include "wifi_manager.h"

#include <stdlib.h>
#include <string.h>

static const char* g_tag = "wifi_manager";

static volatile WifiManagerState g_current_state = WIFI_MANAGER_STATE_IDLE;

static bool g_initialized              = false;
static bool g_scan_active              = false;
static int g_retry_count               = 0;
static TimerHandle_t g_retry_timer     = NULL;
static TimerHandle_t g_stable_timer    = NULL;
static esp_netif_t* g_sta_netif        = NULL;
static WifiManagerScanDoneCb g_scan_cb = NULL;
static WifiManagerEventCb g_user_cb    = NULL;
static esp_netif_t* g_ap_netif         = NULL;
static bool g_static_ip_active         = false;
static TimerHandle_t g_gw_check_timer  = NULL;
static esp_ping_handle_t g_ping_hdl    = NULL;

static WifiManagerConfig g_cfg = {
    .max_retries           = 10,
    .base_retry_ms         = 500,
    .max_retry_ms          = 10000,
    .sta_static_ip_enabled = false,
    .sta_ip                = "",
    .sta_gateway           = "",
    .sta_netmask           = "255.255.255.0",
};

bool is_dns_ready(void) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        return false;
    }

    esp_netif_dns_info_t dns;
    // Check the primary DNS server slot
    if (esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
        // Ensure the DNS address is populated and not an empty 0.0.0.0 loop
        if (dns.ip.u_addr.ip4.addr != 0) {
            return true;
        }
    }
    return false;
}

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
    xTaskCreate(NULL, NULL, 0, NULL, 0, NULL); // Safe placeholder
    xTimerStart(g_retry_timer, 0);
}

static void retry_timer_cb(TimerHandle_t timer) {
    (void)timer;
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    esp_wifi_connect();
}

static void stable_timer_cb(TimerHandle_t timer) {
    (void)timer;

    if (g_current_state != WIFI_MANAGER_STATE_CONNECTED || !g_sta_netif) {
        return;
    }

    esp_netif_dns_info_t dns;
    if (esp_netif_get_dns_info(g_sta_netif, ESP_NETIF_DNS_MAIN, &dns) == ESP_OK) {
        if (dns.ip.u_addr.ip4.addr != 0) {

            esp_netif_dns_info_t fallback_dns = {0};
            fallback_dns.ip.type              = ESP_IPADDR_TYPE_V4;

            // Set Backup DNS to Cloudflare (1.1.1.1)
            fallback_dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("1.1.1.1");
            esp_netif_set_dns_info(g_sta_netif, ESP_NETIF_DNS_BACKUP, &fallback_dns);

            // Set Alternative DNS to Google (8.8.8.8)
            fallback_dns.ip.u_addr.ip4.addr = esp_ip4addr_aton("8.8.8.8");
            esp_netif_set_dns_info(g_sta_netif, ESP_NETIF_DNS_FALLBACK, &fallback_dns);

            ESP_LOGI(g_tag, "Link verified stable. Main DNS: " IPSTR " | Fallbacks configured.",
                     IP2STR(&dns.ip.u_addr.ip4));
            set_state(WIFI_MANAGER_STATE_CONNECTED_WITH_DNS, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
        } else {
            ESP_LOGW(g_tag, "Stabilization finished, but DNS table is empty. Re-trying...");
            xTimerStart(g_stable_timer, 0);
        }
    }
}

void wifi_manager_connect_to_saved_wifi(void) {
    if (!g_initialized) {
        ESP_LOGE("WIFI_PROFILES", "Wi-Fi manager must be started before auto-connecting.");
        return;
    }

    ESP_LOGI("WIFI_PROFILES", "Checking NVS for saved Wi-Fi credentials...");

    SavedWifiNetwork saved_list[MAX_SAVED_NETWORKS];
    uint8_t saved_count = settings_manager_get_all_networks(saved_list, MAX_SAVED_NETWORKS);

    if (saved_count == 0) {
        ESP_LOGW("WIFI_PROFILES", "Zero saved Wi-Fi networks found in memory.");
        set_state(WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_FAIL_REASON_NO_AP);
        return;
    }

    ESP_LOGI("WIFI_PROFILES", "Found %d saved network profile(s) in flash memory.", saved_count);
    ESP_LOGI("WIFI_PROFILES", "Scanning for nearby airwaves...");

    // The stack is already running, we can just execute our active foreground scan safely
    wifi_scan_config_t scan_config = {.show_hidden = true};
    esp_wifi_scan_start(&scan_config, true); // True blocks execution until finished

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    wifi_ap_record_t* ap_info = malloc(sizeof(wifi_ap_record_t) * ap_count);
    esp_wifi_scan_get_ap_records(&ap_count, ap_info);

    int best_saved_index = -1;
    int8_t highest_rssi  = -100;

    for (int i = 0; i < ap_count; i++) {
        for (int j = 0; j < saved_count; j++) {
            if (strcmp((char*)ap_info[i].ssid, saved_list[j].ssid) == 0) {
                if (ap_info[i].rssi > highest_rssi) {
                    highest_rssi     = ap_info[i].rssi;
                    best_saved_index = j;
                }
            }
        }
    }

    free(ap_info);

    if (best_saved_index != -1) {
        ESP_LOGI("WIFI_PROFILES", "Match found! Decided to connect to '%s' (Signal: %d dBm)",
                 saved_list[best_saved_index].ssid, highest_rssi);

        // Feed the target profile directly to our active Wi-Fi hardware configuration
        wifi_config_t wifi_config = {0};
        strncpy((char*)wifi_config.sta.ssid, saved_list[best_saved_index].ssid,
                sizeof(wifi_config.sta.ssid) - 1);
        strncpy((char*)wifi_config.sta.password, saved_list[best_saved_index].password,
                sizeof(wifi_config.sta.password) - 1);

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        // Trigger the internal background asynchronous loop cleanly
        set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
        esp_wifi_connect();
    } else {
        ESP_LOGE("WIFI_PROFILES",
                 "None of the %d saved networks are currently within physical range.", saved_count);
        set_state(WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_FAIL_REASON_NO_AP);
    }
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

static void on_ping_end(esp_ping_handle_t hdl, void* args) {
    uint32_t received = 0;
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_delete_session(hdl);
    g_ping_hdl = NULL;

    if (received == 0) {
        ESP_LOGW(g_tag, "Static IP: gateway unreachable, falling back to DHCP");
        g_static_ip_active = false;
        esp_netif_dhcpc_start(g_sta_netif);
    } else {
        ESP_LOGI(g_tag, "Static IP: gateway reachable (%lu replies)", (unsigned long)received);
    }
}

static void gw_check_timer_cb(TimerHandle_t timer) {
    ip_addr_t target = {0};
    if (!ipaddr_aton(g_cfg.sta_gateway, &target)) {
        return;
    }

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr       = target;
    cfg.count             = 3;
    cfg.timeout_ms        = 1000;
    cfg.interval_ms       = 500;

    esp_ping_callbacks_t cbs = {.on_ping_end = on_ping_end};
    if (esp_ping_new_session(&cfg, &cbs, &g_ping_hdl) != ESP_OK) {
        ESP_LOGW(g_tag, "Failed to create ping session, falling back to DHCP");
        g_static_ip_active = false;
        esp_netif_dhcpc_start(g_sta_netif);
        return;
    }
    esp_ping_start(g_ping_hdl);
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id,
                               void* event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            break;

        case WIFI_EVENT_STA_CONNECTED:
            if ((int)g_static_ip_active && g_gw_check_timer) {
                xTimerChangePeriod(g_gw_check_timer, pdMS_TO_TICKS(3000), 0);
                xTimerStart(g_gw_check_timer, 0);
            }
            break;

        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* info = (wifi_event_sta_disconnected_t*)event_data;

            // CRITICAL: Stop the validation process instantly if the link cuts out
            xTimerStop(g_stable_timer, 0);

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
        ESP_LOGI(g_tag, "Got IP: " IPSTR ". Starting 500ms stabilization grace period...",
                 IP2STR(&event->ip_info.ip));

        g_retry_count = 0;
        xTimerStop(g_retry_timer, 0);

        // Save the interface link context for the background timer thread
        g_sta_netif = event->esp_netif;

        // Move to basic CONNECTED status first
        set_state(WIFI_MANAGER_STATE_CONNECTED, WIFI_MANAGER_FAIL_REASON_UNKNOWN);

        // Arm the stabilization countdown timer (500ms)
        xTimerStart(g_stable_timer, 0);
    }
}

int wifi_manager_start(const WifiManagerConfig* config) {
    if (g_initialized) {
        ESP_LOGE(g_tag, "Already initialized");
        return -1;
    }

    if (config) {
        if (config->max_retries > 0) {
            g_cfg.max_retries = config->max_retries;
        }
        if (config->base_retry_ms > 0) {
            g_cfg.base_retry_ms = config->base_retry_ms;
        }
        if (config->max_retry_ms > 0) {
            g_cfg.max_retry_ms = config->max_retry_ms;
        }
        g_cfg.sta_static_ip_enabled = config->sta_static_ip_enabled;
        memcpy(g_cfg.sta_ip, config->sta_ip, sizeof(g_cfg.sta_ip));
        memcpy(g_cfg.sta_gateway, config->sta_gateway, sizeof(g_cfg.sta_gateway));
        memcpy(g_cfg.sta_netmask, config->sta_netmask, sizeof(g_cfg.sta_netmask));
    }

    g_retry_timer = xTimerCreate("wifi_retry", pdMS_TO_TICKS(g_cfg.base_retry_ms), pdFALSE, NULL,
                                 retry_timer_cb);
    g_stable_timer =
        xTimerCreate("wifi_stable", pdMS_TO_TICKS(500), pdFALSE, NULL, stable_timer_cb);

    if (!g_retry_timer || !g_stable_timer) {
        ESP_LOGE(g_tag, "Failed to create core software timers");
        if (g_retry_timer) {
            xTimerDelete(g_retry_timer, 0);
        }
        if (g_stable_timer) {
            xTimerDelete(g_stable_timer, 0);
        }
        return -1;
    }

    g_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    // Hardware starts here in STA mode, but we do NOT call esp_wifi_connect() yet
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    g_initialized = true;
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    return 0;
}

void wifi_manager_stop(void) {
    xTimerStop(g_retry_timer, 0);
    xTimerStop(g_stable_timer, 0);

    xTimerDelete(g_retry_timer, 0);
    xTimerDelete(g_stable_timer, 0);

    g_retry_timer  = NULL;
    g_stable_timer = NULL;
    g_sta_netif    = NULL;

    if (g_gw_check_timer) {
        xTimerStop(g_gw_check_timer, 0);
        xTimerDelete(g_gw_check_timer, 0);
        g_gw_check_timer = NULL;
    }

    if (g_ping_hdl) {
        esp_ping_stop(g_ping_hdl);
        esp_ping_delete_session(g_ping_hdl);
        g_ping_hdl = NULL;
    }

    esp_wifi_stop();
    g_retry_count      = 0;
    g_scan_active      = false;
    g_scan_cb          = NULL;
    g_initialized      = false;
    g_static_ip_active = false;
    g_sta_netif        = NULL;
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
}

void wifi_manager_reconnect(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "Not initialized");
        return;
    }
    xTimerStop(g_retry_timer, 0);
    xTimerStop(g_stable_timer, 0);
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
    xTimerStop(g_stable_timer, 0);
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
    return 0;
}

WifiManagerState wifi_manager_get_state(void) { return g_current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { g_user_cb = cb; }

void wifi_manager_set_ap_enabled(bool enabled) {
    if (enabled) {
        if (g_ap_netif != NULL) {
            return;
        }
        g_ap_netif = esp_netif_create_default_wifi_ap();

        wifi_config_t ap_config = {
            .ap =
                {
                    .ssid           = "ESP32-Settings",
                    .ssid_len       = 0,
                    .password       = "",
                    .max_connection = 4,
                    .authmode       = WIFI_AUTH_OPEN,
                    .channel        = 1,
                },
        };

        if (g_initialized) {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        }
        ESP_LOGI(g_tag, "AP enabled: ESP32-Settings");
    } else {
        if (g_ap_netif == NULL) {
            return;
        }
        if (g_initialized) {
            ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        }
        esp_netif_destroy(g_ap_netif);
        g_ap_netif = NULL;
        ESP_LOGI(g_tag, "AP disabled");
    }
}
