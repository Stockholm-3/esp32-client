/**
 * @file data_fetcher.h
 * @brief Generic scheduled HTTP data fetcher with LittleFS-backed caching.
 *
 * Each registered @ref FetchDescriptor spawns one FreeRTOS task that loops
 * independently, sleeping between fetches.  There is no shared scheduler —
 * jobs cannot block each other.
 *
 * Two schedule modes are supported:
 *   - @c FETCH_SCHEDULE_INTERVAL — re-fetches every N seconds after the last
 *                                  successful fetch.
 *   - @c FETCH_SCHEDULE_DAILY    — fires once per calendar day at a
 *                                  configurable wall-clock hour.
 *
 * Network activity is gated on DNS availability.  Call
 * @ref data_fetcher_notify_wifi_state from your Wi-Fi manager callback.
 * Individual jobs can also be triggered immediately via
 * @ref data_fetcher_request_now.
 *
 * @par Minimal usage
 * @code
 *   static const FetchDescriptor descs[] = {
 *       {
 *           .id                    = "weather",
 *           .url                   = "https://api.example.com/weather",
 *           .cache_key             = "fetch:weather",
 *           .cache_ttl_sec         = 30 * 60,
 *           .schedule.type         = FETCH_SCHEDULE_INTERVAL,
 *           .schedule.interval_sec = 15 * 60,
 *           .fetch_on_startup      = true,
 *       },
 *   };
 *
 *   DataFetcherConfig cfg  = data_fetcher_default_config();
 *   cfg.descriptors        = descs;
 *   cfg.descriptor_count   = ARRAY_SIZE(descs);
 *   cfg.on_cached          = my_on_cached_cb;
 *   data_fetcher_init(&cfg);
 * @endcode
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
 * Schedule descriptor
 * ------------------------------------------------------------------------- */

typedef enum {
    FETCH_SCHEDULE_INTERVAL = 0,
    FETCH_SCHEDULE_DAILY,
} FetchScheduleType;

typedef struct {
    FetchScheduleType type;
    uint32_t interval_sec; /**< Used when type == FETCH_SCHEDULE_INTERVAL. */
    uint8_t daily_hour;    /**< Local hour (0–23), used when type == FETCH_SCHEDULE_DAILY. */
} FetchSchedule;

/* ---------------------------------------------------------------------------
 * Per-endpoint descriptor
 * ------------------------------------------------------------------------- */

/**
 * Describes a single HTTP endpoint to fetch and cache.
 * All pointer fields must remain valid for the lifetime of the module.
 */
typedef struct {
    /**
     * Short identifier used in log messages (e.g. "weather").
     * Must be unique across all descriptors passed to @ref data_fetcher_init.
     */
    const char* id;

    const char* url;       /**< Fully-qualified HTTP/HTTPS URL. */
    const char* cache_key; /**< Key passed to cache_put() on success. */
    uint32_t cache_ttl_sec;

    FetchSchedule schedule;

    /**
     * If true, fetch as soon as DNS is available after boot regardless of the
     * normal schedule.
     */
    bool fetch_on_startup;

    /**
     * Milliseconds to wait after DNS becomes available before the startup
     * fetch.  Lets multiple descriptors stagger their initial requests.
     * Ignored when fetch_on_startup is false.
     */
    uint32_t startup_delay_ms;
} FetchDescriptor;

/* ---------------------------------------------------------------------------
 * Callbacks
 * ------------------------------------------------------------------------- */

/**
 * Called after a successful fetch, before the data is written to the cache.
 *
 * The implementation may inspect or rewrite the payload.  To replace the
 * buffer, free *buf, allocate a new one, and update both *buf and *len.
 * The module will free *buf after cache_put() returns, so the returned
 * pointer must be heap-allocated.
 *
 * Return true to proceed with caching, false to discard the response
 * (the fetch is still considered successful and the retry counter resets).
 *
 * @param desc      Descriptor whose fetch just completed.
 * @param buf       Pointer to the response buffer pointer (may be replaced).
 * @param len       Pointer to the buffer length (must be updated if replaced).
 * @param user_ctx  The on_transform_ctx value from @ref DataFetcherConfig.
 */
typedef bool (*DataFetcherTransformCb)(const FetchDescriptor* desc, uint8_t** buf, size_t* len,
                                       void* user_ctx);

/**
 * Called after the payload has been written to the cache.
 *
 * @param desc      Descriptor whose fetch just completed.
 * @param user_ctx  The on_cached_ctx value from @ref DataFetcherConfig.
 */
typedef void (*DataFetcherOnCachedCb)(const FetchDescriptor* desc, void* user_ctx);

/* ---------------------------------------------------------------------------
 * Module configuration
 * ------------------------------------------------------------------------- */

#define DATA_FETCHER_MAX_RETRIES_DEFAULT 5U
#define DATA_FETCHER_RETRY_BASE_MS_DEFAULT 5000U
#define DATA_FETCHER_RETRY_MAX_MS_DEFAULT (5U * 60U * 1000U)

/** Stack size (bytes) allocated for each fetch task. */
#define DATA_FETCHER_TASK_STACK_DEFAULT 4096U

/** FreeRTOS priority for fetch tasks. */
#define DATA_FETCHER_TASK_PRIORITY_DEFAULT 5U

typedef struct {
    const FetchDescriptor* descriptors;
    size_t descriptor_count;

    uint32_t max_retries;
    uint32_t retry_base_ms;
    uint32_t retry_max_ms;

    uint32_t task_stack_size;  /**< Per-task stack in bytes. 0 = use default. */
    UBaseType_t task_priority; /**< FreeRTOS task priority. 0 = use default. */

    /** Optional: called before caching to inspect or rewrite the payload. */
    DataFetcherTransformCb on_transform;
    void* on_transform_ctx;

    /** Optional: called after each successful cache write. */
    DataFetcherOnCachedCb on_cached;
    void* on_cached_ctx;
} DataFetcherConfig;

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * Returns a @ref DataFetcherConfig with all fields set to their defaults.
 * Populate descriptors and descriptor_count before calling
 * @ref data_fetcher_init.
 */
DataFetcherConfig data_fetcher_default_config(void);

/**
 * Initialises the module and spawns one FreeRTOS task per descriptor.
 * May only be called once.
 *
 * @param config  Fully populated configuration.  The descriptors array must
 *                remain valid for the lifetime of the module.
 * @return 0 on success, -1 on invalid arguments or task creation failure.
 */
int data_fetcher_init(const DataFetcherConfig* config);

/**
 * Notifies the module of a Wi-Fi manager state change.
 * Fetching is enabled only while state is WIFI_MANAGER_STATE_CONNECTED_WITH_DNS.
 */
void data_fetcher_notify_wifi_state(WifiManagerState state);

/**
 * Triggers an immediate out-of-schedule fetch for the descriptor whose id
 * matches descriptor_id.  Has no effect if the job is already fetching or
 * DNS is unavailable.
 */
void data_fetcher_request_now(const char* descriptor_id);

#ifdef __cplusplus
}
#endif

#endif /* DATA_FETCHER_H */
