// include/time_manager.h
#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <time.h>
#include <stdbool.h>

typedef enum {
    TIME_STATE_UNSYNCED,  ///< No time source available
    TIME_STATE_SYNCING,   ///< Attempting to sync time
    TIME_STATE_SYNCED,    ///< Time is valid and synced
    TIME_STATE_FAILED     ///< Sync failed permanently
} TimeState;

/// Callback for time state changes
typedef void (*TimeEventCb)(TimeState state, struct tm *current_time);

/**
 * @brief Initialize the time manager
 * @param cb Callback for state changes (can be NULL)
 */
void time_manager_init(TimeEventCb cb);

/**
 * @brief Get current time
 * @param timeinfo Pointer to tm struct to fill
 * @return true if time is valid, false otherwise
 */
bool time_manager_get_time(struct tm *timeinfo);

/// Get current sync state
TimeState time_manager_get_state(void);

/// Manually trigger resync
void time_manager_resync(void);

/// Call this periodically (every 100ms-1s)
void time_manager_poll(void);

#endif