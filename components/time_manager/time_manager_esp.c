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
static bool g_time_valid  = false;
static bool g_initialized = false;

// SNTP sync completion callback - triggered when NTP successfully obtains time
static void sntp_sync_callback(struct timeval* tv) {
    ESP_LOGI(g_tag, "SNTP time sync completed");

    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Validate year > 2020 (tm_year is years since 1900, so 120 = 2020)
    if (timeinfo.tm_year > 120) {
        g_cached_time   = timeinfo;
        g_time_valid    = true;
        g_current_state = TIME_STATE_SYNCED;

        // Notify user callback that time is now available
        if (g_event_callback) {
            g_event_callback(g_current_state, &timeinfo);
        }
    } else {
        ESP_LOGW(g_tag, "Invalid time received");
        g_current_state = TIME_STATE_FAILED;
    }
}

// Initialize the time manager with NTP (Network Time Protocol)
// Sets up Europe/Central European Time timezone and configures SNTP
void time_manager_init(TimeEventCb cb) {
    // Guard against re-initialization while SNTP is running
    if (g_initialized) {
        ESP_LOGD(g_tag, "Already initialized, skipping init");
        if (cb) {
            g_event_callback = cb;
        }
        return;
    }

    // Store user callback for time sync events
    if (cb) {
        g_event_callback = cb;
    }

    // Set timezone to CET/CEST (Central European Time with automatic DST)
    // Format: CET-1CEST-2,M3.5.0/2,M10.5.0/3
    // CET-1 = UTC+1, CEST-2 = UTC+2 during DST
    // M3.5.0/2 = Last Sunday of March at 2am -> DST starts
    // M10.5.0/3 = Last Sunday of October at 3am -> DST ends
    setenv("TZ", "CET-1CEST-2,M3.5.0/2,M10.5.0/3", 1);
    tzset();

    // Stop existing SNTP session if running (clean restart)
    if (esp_sntp_enabled()) {
        ESP_LOGI(g_tag, "SNTP already running, restarting for new connection");
        esp_sntp_stop();
    }

    // Configure and start SNTP client
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(sntp_sync_callback);
    esp_sntp_init();

    g_initialized   = true;
    g_current_state = TIME_STATE_SYNCING;
    ESP_LOGI(g_tag, "Time manager initialized (ESP32/NTP mode)");
    
    // Notify that sync has started (time not yet valid)
    if (g_event_callback) {
        g_event_callback(g_current_state, NULL);
    }
}

// Get current system time - returns false if time hasn't been synced yet
// Fills the provided tm struct with current local time on success
bool time_manager_get_time(struct tm* timeinfo) {
    if (!g_time_valid) {
        return false;
    }

    time_t now = time(NULL);
    localtime_r(&now, timeinfo);
    return true;
}

// Returns current time synchronization state (UNSYNCED/SYNCING/SYNCED/FAILED)
TimeState time_manager_get_state(void) { return g_current_state; }

// Force a re-synchronization with NTP server
// Stops current SNTP session and restarts it
void time_manager_resync(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "Not initialized");
        return;
    }

    if (g_current_state == TIME_STATE_SYNCED || g_current_state == TIME_STATE_SYNCING) {
        ESP_LOGI(g_tag, "Resyncing time...");
        esp_sntp_stop();
        esp_sntp_init();
        g_current_state = TIME_STATE_SYNCING;
        g_time_valid    = false;
    }
}

// Periodic poll function - not needed as SNTP runs asynchronously in background
// Kept for API compatibility with other manager modules
void time_manager_poll(void) {
    // Nothing needed - SNTP runs in background
}