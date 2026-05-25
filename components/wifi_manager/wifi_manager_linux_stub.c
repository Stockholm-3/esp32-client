#include "esp_log.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/*
 * Dumb Linux host stub.
 * Instantly transitions to a working, connected state synchronously.
 * No timers, no background processing, no dependencies.
 */

static const char* g_tag = "wifi_manager_stub";

static volatile WifiManagerState g_current_state = WIFI_MANAGER_STATE_IDLE;
static bool g_initialized                        = false;
static WifiManagerEventCb g_user_cb              = NULL;

static void set_state(WifiManagerState state, WifiManagerFailReason reason) {
    if (g_current_state != state) {
        g_current_state = state;
        if (g_user_cb) {
            g_user_cb(state, reason);
        }
        ESP_LOGI(g_tag, "State -> %d", state);
    }
}

bool is_dns_ready(void) { return true; }

int wifi_manager_start(const WifiManagerConfig* config) {
    (void)config;
    if (g_initialized) {
        ESP_LOGE(g_tag, "Already initialized");
        return -1;
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Subsystems initialized. Driver operational in STA mode.");
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    return 0;
}

int wifi_manager_connect(const char* ssid, const char* password) {
    (void)password;
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Cannot connect: Wi-Fi manager not started.");
        return -1;
    }

    if (ssid == NULL || strlen(ssid) == 0) {
        ESP_LOGE(g_tag, "Cannot connect: Invalid SSID targets provided.");
        return -1;
    }

    ESP_LOGI(g_tag, "Initiating instant connection to target SSID: '%s'", ssid);

    // Synchronously sequence straight through the state transitions
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    set_state(WIFI_MANAGER_STATE_CONNECTED, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    set_state(WIFI_MANAGER_STATE_CONNECTED_WITH_DNS, WIFI_MANAGER_FAIL_REASON_UNKNOWN);

    return 0;
}

void wifi_manager_stop(void) {
    g_initialized = false;
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
}

void wifi_manager_reconnect(void) {
    if (!g_initialized) {
        return;
    }
    wifi_manager_connect("SavedNetwork", NULL);
}

int wifi_manager_change_network(const char* ssid, const char* password) {
    return wifi_manager_connect(ssid, password);
}

int wifi_manager_scan(WifiManagerScanDoneCb cb) {
    if (!g_initialized || !cb) {
        return -1;
    }

    static const WifiManagerApInfo FAKE_APS[] = {
        {.ssid = "HomeNetwork", .rssi = -45, .authmode = 3},
        {.ssid = "OfficeWiFi", .rssi = -67, .authmode = 3},
    };

    set_state(WIFI_MANAGER_STATE_SCANNING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    cb(FAKE_APS, sizeof(FAKE_APS) / sizeof(FAKE_APS[0]));
    set_state(WIFI_MANAGER_STATE_CONNECTED_WITH_DNS, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    return 0;
}

void wifi_manager_connect_to_saved_wifi(void) {
    if (g_initialized) {
        // Automatically jump online on boot if requested
        wifi_manager_connect("SavedNetwork", NULL);
    }
}

WifiManagerState wifi_manager_get_state(void) { return g_current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { g_user_cb = cb; }

void wifi_manager_set_ap_enabled(bool enabled) { (void)enabled; }
