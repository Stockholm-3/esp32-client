// time_manager_linux_stub.c
#include "time_manager.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "time_manager";
static TimeState current_state = TIME_STATE_UNSYNCED;
static TimeEventCb event_callback = NULL;
static struct tm current_time;
static bool initialized = false;

// Use system time for Linux/simulator
void time_manager_init(TimeEventCb cb) {
    event_callback = cb;
    
    // Get current system time
    time_t now = time(NULL);
    localtime_r(&now, &current_time);
    
    initialized = true;
    current_state = TIME_STATE_SYNCED;
    
    ESP_LOGI(TAG, "Time manager initialized (Linux stub - using system time)");
    
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &current_time);
    ESP_LOGI(TAG, "System time: %s", time_str);
    
    if (event_callback) {
        event_callback(current_state, &current_time);
    }
}

bool time_manager_get_time(struct tm *timeinfo) {
    if (!initialized) {
        return false;
    }
    
    // Always return current system time
    time_t now = time(NULL);
    localtime_r(&now, timeinfo);
    return true;
}

TimeState time_manager_get_state(void) {
    return current_state;
}

void time_manager_resync(void) {
    ESP_LOGI(TAG, "Resync requested - updating system time");
    time_t now = time(NULL);
    localtime_r(&now, &current_time);
    
    if (event_callback) {
        event_callback(current_state, &current_time);
    }
}

void time_manager_poll(void) {
    // Nothing needed - system time is always current
    // This function exists for API compatibility
}