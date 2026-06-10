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
 *   1. DNS — call data_fetcher_notify_wifi_state() from your Wi-Fi callback.
 *   2. Clock — call data_fetcher_notify_time_sync() from your SNTP callback.
 *
 * The clock gate exists because mbedTLS validates certificate validity windows
 * against time(NULL). If the clock is still at epoch every TLS handshake fails
 * with MBEDTLS_ERR_X509_CERT_VERIFY_FAILED (-0x2700), which fragments heap and
 * causes all subsequent connections to fail with MBEDTLS_ERR_SSL_ALLOC_FAILED
 * (-0x008D). Both gates must be open before a single TLS connection is made.
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
    const char*   id;            /* Unique short name used in logs, e.g. "weather" */
    const char*   url;           /* Fully-qualified HTTPS URL                       */
    const char*   cache_key;     /* Key passed to cache_put() on success            */
    uint32_t      cache_ttl_sec;
    FetchSchedule schedule;
    bool          fetch_on_startup;  /* Fetch as soon as both gates open at boot    */
    uint32_t      startup_delay_ms;  /* Stagger delay before startup fetch          */
} FetchDescriptor;

/* ---------------------------------------------------------------------------
 * Callbacks
 * ------------------------------------------------------------------------- */

/**
 * Called after a successful fetch, before caching.
 * May inspect or replace the buffer (free old, alloc new, update *buf/len).
 * Return true to cache, false to discard (still counts as success, no retry).
 */
typedef bool (*DataFetcherTransformCb)(const FetchDescriptor* desc,
                                       uint8_t** buf, size_t* len,
                                       void* user_ctx);

/** Called after the payload has been written to the cache. */
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
 * Notify the module of a Wi-Fi state change.
 * The DNS gate is open only while state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS.
 */
void data_fetcher_notify_wifi_state(WifiManagerState state);

/**
 * Notify the module that the system clock has been synchronised (e.g. SNTP).
 * Opens the clock gate and allows TLS connections to proceed.
 *
 * MUST be called from your SNTP/time_manager sync callback — NOT from the
 * Wi-Fi connected callback. The clock may not be valid yet at that point.
 * Safe to call multiple times; only the first call has any effect.
 */
void data_fetcher_notify_time_sync(void);

/**
 * Trigger an immediate out-of-schedule fetch for the given descriptor id.
 * No effect if either gate is closed or the id is not found.
 */
void data_fetcher_request_now(const char* descriptor_id);

#ifdef __cplusplus
}
#endif

#endif /* DATA_FETCHER_H */
