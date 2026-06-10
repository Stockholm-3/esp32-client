/**
 * @file data_fetcher.h
 * @brief Generic scheduled HTTP data fetcher with LittleFS-backed caching.
 *
 * Each registered FetchDescriptor spawns one FreeRTOS task. Tasks are
 * independent — no shared scheduler, no blocking each other.
 *
 * Two schedule modes:
 *   FETCH_SCHEDULE_INTERVAL — re-fetch every N seconds after last success.
 *   FETCH_SCHEDULE_DAILY    — fire once per day at a given wall-clock hour.
 *
 * Two gates must both be open before any fetch attempt:
 *   1. DNS  — call data_fetcher_notify_wifi_state() from your Wi-Fi callback.
 *   2. Clock — call data_fetcher_notify_time_sync() from your SNTP callback.
 *
 * Startup behaviour (fetch_on_startup = true):
 *   The cache is checked first. If a valid (non-expired) entry exists it is
 *   served immediately via on_cached without hitting the network. A fetch is
 *   only made if the cache is missing or expired. This avoids redundant
 *   network traffic on every reboot when the cached data is still fresh.
 */

#ifndef DATA_FETCHER_H
#define DATA_FETCHER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * Schedule
 * ------------------------------------------------------------------------- */

typedef enum {
    FETCH_SCHEDULE_INTERVAL = 0,
    FETCH_SCHEDULE_DAILY,
} FetchScheduleType;

typedef struct {
    FetchScheduleType type;
    uint32_t interval_sec; /* FETCH_SCHEDULE_INTERVAL: seconds between fetches */
    uint8_t  daily_hour;   /* FETCH_SCHEDULE_DAILY: local hour 0-23            */
} FetchSchedule;

/* ---------------------------------------------------------------------------
 * Per-endpoint descriptor
 * All pointer fields must remain valid for the lifetime of the module.
 * ------------------------------------------------------------------------- */

typedef struct {
    const char*   id;           /* Unique short name used in logs, e.g. "weather" */
    const char*   url;          /* Fully-qualified HTTPS URL                       */
    const char*   cache_key;    /* Key passed to cache_put() on success            */
    uint32_t      cache_ttl_sec;
    FetchSchedule schedule;

    /**
     * If true: on startup, serve from cache if fresh; fetch only if stale or
     * missing. This avoids a network hit on every reboot when data is recent.
     * If false: never fetch on startup, only on the normal schedule.
     */
    bool fetch_on_startup;
} FetchDescriptor;

/* ---------------------------------------------------------------------------
 * Callbacks
 * ------------------------------------------------------------------------- */

/**
 * Called after a successful fetch, before caching.
 * May replace the buffer (free old, alloc new, update *buf/len).
 * Return true to cache, false to discard (still counts as success, no retry).
 */
typedef bool (*DataFetcherTransformCb)(const FetchDescriptor* desc,
                                       uint8_t** buf, size_t* len,
                                       void* user_ctx);

/**
 * Called after data is ready in cache — either because a fresh fetch just
 * completed, or because a valid cached entry was found on startup.
 */
typedef void (*DataFetcherOnCachedCb)(const FetchDescriptor* desc, void* user_ctx);

/* ---------------------------------------------------------------------------
 * Configuration
 * ------------------------------------------------------------------------- */

#define DATA_FETCHER_MAX_RETRIES_DEFAULT    5U
#define DATA_FETCHER_RETRY_BASE_MS_DEFAULT  5000U
#define DATA_FETCHER_RETRY_MAX_MS_DEFAULT   (5U * 60U * 1000U)
#define DATA_FETCHER_TASK_STACK_DEFAULT     4096U
#define DATA_FETCHER_TASK_PRIORITY_DEFAULT  5U

typedef struct {
    const FetchDescriptor* descriptors;
    size_t                 descriptor_count;

    uint32_t    max_retries;
    uint32_t    retry_base_ms;
    uint32_t    retry_max_ms;
    uint32_t    task_stack_size; /* 0 = default */
    UBaseType_t task_priority;   /* 0 = default */

    DataFetcherTransformCb on_transform;
    void*                  on_transform_ctx;

    /** Called when data is available in cache (fresh fetch or cache hit). */
    DataFetcherOnCachedCb  on_cached;
    void*                  on_cached_ctx;
} DataFetcherConfig;

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/** Returns a DataFetcherConfig with all fields set to built-in defaults. */
DataFetcherConfig data_fetcher_default_config(void);

/**
 * Initialise the module and spawn one task per descriptor. Call once.
 * @return 0 on success, -1 on failure.
 */
int data_fetcher_init(const DataFetcherConfig* config);

/**
 * Notify of Wi-Fi state change.
 * DNS gate is open only while state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS.
 */
void data_fetcher_notify_wifi_state(WifiManagerState state);

/**
 * Notify that the system clock has been synchronised (SNTP).
 * Opens the clock gate — required before any TLS connection is attempted.
 * Safe to call multiple times; only the first call has effect.
 */
void data_fetcher_notify_time_sync(void);

/**
 * Trigger an immediate out-of-schedule fetch for the given descriptor id.
 * If a fetch is already in progress the flag is set and fires immediately
 * after the current fetch completes.
 */
void data_fetcher_request_now(const char* descriptor_id);

#ifdef __cplusplus
}
#endif

#endif /* DATA_FETCHER_H */
