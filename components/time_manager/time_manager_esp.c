#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "time_manager.h"

#include <string.h>
#include <time.h>

static const char* g_tag            = "time_manager";
static TimeState g_current_state    = TIME_STATE_UNSYNCED;
static TimeEventCb g_event_callback = NULL;
static struct tm g_cached_time;
static bool g_time_valid = false;

static void sntp_sync_callback(struct timeval* tv) {
    ESP_LOGI(g_tag, "SNTP time sync completed");

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    if (timeinfo.tm_year > 120) {
        g_cached_time   = timeinfo;
        g_time_valid    = true;
        g_current_state = TIME_STATE_SYNCED;

        if (g_event_callback) {
            g_event_callback(g_current_state, &timeinfo);
        }
    } else {
        ESP_LOGW(g_tag, "Invalid time received");
        g_current_state = TIME_STATE_FAILED;
    }
}

void time_manager_init(TimeEventCb cb) {
    g_event_callback = cb;

    // Set timezone (customize as needed)
    setenv("TZ", "CET-1CEST-2,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    // Initialize SNTP using esp_sntp API (for ESP-IDF v4.4+)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    // esp_sntp_setservername(1, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_callback);
    esp_sntp_init();

    g_current_state = TIME_STATE_SYNCING;
    ESP_LOGI(g_tag, "Time manager initialized (ESP32/NTP mode)");

    if (g_event_callback) {
        g_event_callback(g_current_state, NULL);
    }
}

bool time_manager_get_time(struct tm* timeinfo) {
    if (!g_time_valid) {
        return false;
    }

    time_t now = time(NULL);
    localtime_r(&now, timeinfo);
    return true;
}

TimeState time_manager_get_state(void) { return g_current_state; }

void time_manager_resync(void) {
    if (g_current_state == TIME_STATE_SYNCED) {
        ESP_LOGI(g_tag, "Manual resync requested");
        esp_sntp_stop();
        esp_sntp_init();
        g_current_state = TIME_STATE_SYNCING;
        g_time_valid    = false;
    }
}

void time_manager_poll(void) {
    // Nothing needed - SNTP runs in background
}