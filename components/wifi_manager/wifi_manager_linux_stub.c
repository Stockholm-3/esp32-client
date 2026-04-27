#include "esp_log.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

static const char* g_tag = "wifi_manager_stub";

static volatile WifiManagerState g_current_state = WIFI_MANAGER_STATE_IDLE;
static bool g_initialized                        = false;

static int g_retry_count   = 0;
static int g_max_retries   = 5;
static int g_base_retry_ms = 500;
static int g_max_retry_ms  = 2000;

static WifiManagerEventCb g_user_cb = NULL;
static int64_t g_next_retry_time    = 0;
static bool g_retry_pending         = false;

static void set_state(WifiManagerState state) {
    if (g_current_state != state) {
        g_current_state = state;
        if (g_user_cb) {
            g_user_cb(state);
        }
        ESP_LOGI(g_tag, "State changed to %d", state);
    }
}

static int get_backoff_delay_ms(void) {
    int delay = g_base_retry_ms * (1 << g_retry_count);
    if (delay > g_max_retry_ms) {
        delay = g_max_retry_ms;
    }
    return delay;
}
/* mock data for wifi simulation*/
void wifi_manager_hw_preinit(void) {}

void wifi_manager_scan_start(WifiScanDoneCb cb) {
    if (!cb)
        return;
    static const WifiApInfo mock[] = {
        {"HomeNetwork_5G", -45, true},
        {"Office_WiFi", -67, true},
        {"Guest", -80, false},
    };
    cb(mock, 3);
}

int wifi_manager_start(const char* ssid, const char* password, const WifiManagerConfig* config) {
    if (g_initialized) {
        ESP_LOGE(g_tag, "Already initialized!");
        return -1;
    }

    ESP_LOGI(g_tag, "Starting Wi-Fi for SSID '%s'", ssid);

    if (config) {
        g_max_retries   = config->max_retries;
        g_base_retry_ms = config->base_retry_ms;
        g_max_retry_ms  = config->max_retry_ms;
    }

    g_initialized     = true;
    g_retry_count     = 0;
    g_retry_pending   = true;
    g_next_retry_time = ((int64_t)time(NULL) * 1000) + 1000;
    set_state(WIFI_MANAGER_STATE_CONNECTING);
    return 0;
}

void wifi_manager_stop(void) {
    ESP_LOGI(g_tag, "Stopping Wi-Fi");
    g_initialized   = false;
    g_retry_pending = false;
    g_retry_count   = 0;
    set_state(WIFI_MANAGER_STATE_IDLE);
}

void wifi_manager_process(void) {
    if (!g_retry_pending) {
        return;
    }

    int64_t now = (int64_t)time(NULL) * 1000;
    if (now >= g_next_retry_time) {
        if (g_retry_count < g_max_retries) {
            g_retry_count++;
            ESP_LOGW(g_tag, "Fake retry %d/%d", g_retry_count, g_max_retries);
            g_next_retry_time = now + get_backoff_delay_ms();
            set_state(WIFI_MANAGER_STATE_CONNECTING);
        } else {
            ESP_LOGE(g_tag, "Max retries reached, failed");
            set_state(WIFI_MANAGER_STATE_FAILED);
            g_retry_pending = false;
        }

        if (g_retry_count == 3) {
            set_state(WIFI_MANAGER_STATE_CONNECTED);
            g_retry_pending = false;
        }
    }
}

WifiManagerState wifi_manager_get_state(void) { return g_current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { g_user_cb = cb; }
