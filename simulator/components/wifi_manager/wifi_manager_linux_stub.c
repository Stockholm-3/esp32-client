#include "esp_log.h"
#include "wifi_manager.h"
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

static const char *TAG = "wifi_manager_stub";

static volatile WifiManagerState current_state = WIFI_MANAGER_STATE_IDLE;
static bool initialized = false;

static int retry_count = 0;
static int max_retries = 5;
static int base_retry_ms = 500;
static int max_retry_ms = 2000;

static WifiManagerEventCb user_cb = NULL;
static int64_t next_retry_time = 0;
static bool retry_pending = false;

static void set_state(WifiManagerState state) {
    if (current_state != state) {
        current_state = state;
        if (user_cb)
            user_cb(state);
        ESP_LOGI(TAG, "State changed to %d", state);
    }
}

static int get_backoff_delay_ms(void) {
    int delay = base_retry_ms * (1 << retry_count);
    if (delay > max_retry_ms)
        delay = max_retry_ms;
    return delay;
}

int wifi_manager_start(const char *ssid, const char *password,
                       const WifiManagerConfig *config) {
    if (initialized) {
        ESP_LOGE(TAG, "Already initialized!");
        return -1;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi for SSID '%s'", ssid);

    if (config) {
        max_retries = config->max_retries;
        base_retry_ms = config->base_retry_ms;
        max_retry_ms = config->max_retry_ms;
    }

    initialized = true;
    retry_count = 0;
    retry_pending = true;
    next_retry_time = (int64_t)time(NULL) * 1000 + 1000;
    set_state(WIFI_MANAGER_STATE_CONNECTING);
    return 0;
}

void wifi_manager_stop(void) {
    ESP_LOGI(TAG, "Stopping Wi-Fi");
    initialized = false;
    retry_pending = false;
    retry_count = 0;
    set_state(WIFI_MANAGER_STATE_IDLE);
}

void wifi_manager_poll(void) {
    if (!retry_pending)
        return;

    int64_t now = (int64_t)time(NULL) * 1000;
    if (now >= next_retry_time) {
        if (retry_count < max_retries) {
            retry_count++;
            ESP_LOGW(TAG, "Fake retry %d/%d", retry_count, max_retries);
            next_retry_time = now + get_backoff_delay_ms();
            set_state(WIFI_MANAGER_STATE_CONNECTING);
        } else {
            ESP_LOGE(TAG, "Max retries reached, failed");
            set_state(WIFI_MANAGER_STATE_FAILED);
            retry_pending = false;
        }

        if (retry_count == 3) {
            set_state(WIFI_MANAGER_STATE_CONNECTED);
            retry_pending = false;
        }
    }
}

WifiManagerState wifi_manager_get_state(void) { return current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { user_cb = cb; }
