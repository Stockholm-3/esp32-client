// time_manager_esp.c
#include "time_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"          
#include <string.h>
#include <time.h>

static const char *TAG = "time_manager";
static TimeState current_state = TIME_STATE_UNSYNCED;
static TimeEventCb event_callback = NULL;
static struct tm cached_time;
static bool time_valid = false;

static void sntp_sync_callback(struct timeval *tv) {
    ESP_LOGI(TAG, "SNTP time sync completed");
    
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    
    if (timeinfo.tm_year > 120) { 
        cached_time = timeinfo;
        time_valid = true;
        current_state = TIME_STATE_SYNCED;
        
        if (event_callback) {
            event_callback(current_state, &timeinfo);
        }
    } else {
        ESP_LOGW(TAG, "Invalid time received");
        current_state = TIME_STATE_FAILED;
    }
}

void time_manager_init(TimeEventCb cb) {
    event_callback = cb;
    
    // Set timezone (customize as needed)
    setenv("TZ", "UTC0", 1);
    tzset();
    
    // Initialize SNTP using esp_sntp API (for ESP-IDF v4.4+)
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_callback);
    esp_sntp_init();
    
    current_state = TIME_STATE_SYNCING;
    ESP_LOGI(TAG, "Time manager initialized (ESP32/NTP mode)");
    
    if (event_callback) {
        event_callback(current_state, NULL);
    }
}

bool time_manager_get_time(struct tm *timeinfo) {
    if (!time_valid) {
        return false;
    }
    
    time_t now = time(NULL);
    localtime_r(&now, timeinfo);
    return true;
}

TimeState time_manager_get_state(void) {
    return current_state;
}

void time_manager_resync(void) {
    if (current_state == TIME_STATE_SYNCED) {
        ESP_LOGI(TAG, "Manual resync requested");
        esp_sntp_stop();
        esp_sntp_init();
        current_state = TIME_STATE_SYNCING;
        time_valid = false;
    }
}

void time_manager_poll(void) {
    // Nothing needed - SNTP runs in background
}