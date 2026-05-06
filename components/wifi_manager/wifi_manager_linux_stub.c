#include "esp_log.h"
#include "wifi_manager.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * Linux host stub. Uses a POSIX timer (SIGALRM) to mirror the FreeRTOS timer
 * used on ESP so retry backoff behaviour is exercised without hardware.
 *
 * Simulates:
 *   - Auth failure on the first attempt (reason: WIFI_MANAGER_FAIL_REASON_AUTH)
 *     if the password is "badpassword", to exercise the failure path.
 *   - Successful connection on STUB_SUCCESS_RETRY for all other passwords.
 *
 * NVS, netif, and event loop init are intentionally absent — same as the ESP
 * target, those are the application's responsibility.
 */

#define STUB_SUCCESS_RETRY 3

static const char* g_tag = "wifi_manager_stub";

static volatile WifiManagerState g_current_state = WIFI_MANAGER_STATE_IDLE;

static bool g_initialized = false;
static bool g_scan_active = false;
static int g_retry_count  = 0;
static timer_t g_retry_timer;
static bool g_timer_created            = false;
static bool g_bad_password             = false;
static WifiManagerScanDoneCb g_scan_cb = NULL;
static WifiManagerEventCb g_user_cb    = NULL;

static WifiManagerConfig g_cfg = {
    .max_retries   = 5,
    .base_retry_ms = 500,
    .max_retry_ms  = 2000,
};

static void set_state(WifiManagerState state, WifiManagerFailReason reason) {
    if (g_current_state != state) {
        g_current_state = state;
        if (g_user_cb) {
            g_user_cb(state, reason);
        }
        ESP_LOGI(g_tag, "State -> %d", state);
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

static void arm_timer(int delay_ms) {
    struct itimerspec ts = {
        .it_value    = {.tv_sec = delay_ms / 1000, .tv_nsec = (delay_ms % 1000) * 1000000L},
        .it_interval = {0},
    };
    timer_settime(g_retry_timer, 0, &ts, NULL);
}

static void stop_timer(void) {
    struct itimerspec ts = {0};
    timer_settime(g_retry_timer, 0, &ts, NULL);
}

static void schedule_retry(void) {
    if (g_retry_count >= g_cfg.max_retries) {
        g_retry_count = 0;
    }
    int delay = get_backoff_delay_ms();
    ESP_LOGW(g_tag, "Retry %d in %d ms", g_retry_count + 1, delay);
    g_retry_count++;
    arm_timer(delay);
}

static void retry_timer_cb(int sig) {
    (void)sig;

    if (g_bad_password) {
        ESP_LOGE(g_tag, "Stub: auth failure");
        stop_timer();
        g_retry_count = 0;
        set_state(WIFI_MANAGER_STATE_FAILED, WIFI_MANAGER_FAIL_REASON_AUTH);
        return;
    }

    if (g_retry_count < STUB_SUCCESS_RETRY) {
        set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
        schedule_retry();
        return;
    }

    ESP_LOGI(g_tag, "Stub: simulating successful connection");
    g_retry_count = 0;
    set_state(WIFI_MANAGER_STATE_CONNECTED, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
}

int wifi_manager_start(const char* ssid, const char* password, const WifiManagerConfig* config) {
    if (g_initialized) {
        ESP_LOGE(g_tag, "Already initialized");
        return -1;
    }

    if (config) {
        g_cfg = *config;
    }

    g_bad_password = (strcmp(password, "badpassword") == 0);

    signal(SIGALRM, retry_timer_cb);

    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo  = SIGALRM,
    };

    if (timer_create(CLOCK_MONOTONIC, &sev, &g_retry_timer) != 0) {
        ESP_LOGE(g_tag, "Failed to create retry timer");
        return -1;
    }

    g_timer_created = true;
    g_initialized   = true;
    g_retry_count   = 0;

    ESP_LOGI(g_tag, "Starting Wi-Fi for SSID '%s'", ssid);
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    arm_timer(1000);

    return 0;
}

void wifi_manager_stop(void) {
    if (g_timer_created) {
        stop_timer();
        timer_delete(g_retry_timer);
        g_timer_created = false;
    }

    g_initialized  = false;
    g_retry_count  = 0;
    g_scan_active  = false;
    g_scan_cb      = NULL;
    g_bad_password = false;
    set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
}

void wifi_manager_reconnect(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "Not initialized");
        return;
    }
    stop_timer();
    g_retry_count = 0;
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    arm_timer(0);
}

int wifi_manager_change_network(const char* ssid, const char* password) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return -1;
    }

    ESP_LOGI(g_tag, "Stub: changing network to '%s'", ssid);

    g_bad_password = (strcmp(password, "badpassword") == 0);

    stop_timer();
    g_retry_count = 0;
    set_state(WIFI_MANAGER_STATE_CONNECTING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    arm_timer(500);
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

    static const WifiManagerApInfo FAKE_APS[] = {
        {.ssid = "HomeNetwork", .rssi = -45, .authmode = 3},
        {.ssid = "OfficeWiFi", .rssi = -67, .authmode = 3},
        {.ssid = "GuestNetwork", .rssi = -80, .authmode = 0},
    };

    g_scan_active = true;
    set_state(WIFI_MANAGER_STATE_SCANNING, WIFI_MANAGER_FAIL_REASON_UNKNOWN);

    cb(FAKE_APS, sizeof(FAKE_APS) / sizeof(FAKE_APS[0]));

    g_scan_active = false;
    if (g_current_state == WIFI_MANAGER_STATE_SCANNING) {
        set_state(WIFI_MANAGER_STATE_IDLE, WIFI_MANAGER_FAIL_REASON_UNKNOWN);
    }

    return 0;
}

WifiManagerState wifi_manager_get_state(void) { return g_current_state; }

void wifi_manager_register_callback(WifiManagerEventCb cb) { g_user_cb = cb; }
