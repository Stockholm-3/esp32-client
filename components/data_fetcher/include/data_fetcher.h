/**
 * @file data_fetcher.h
 * @brief Generic scheduled HTTP data fetcher — public API.
 *
 * Startup behaviour (fetch_on_startup = true):
 *   After both gates open (DNS + clock), the cache is checked first.
 *   - Cache hit (fresh):  on_cached is fired immediately, no network request.
 *   - Cache miss/expired: build_url (if set) is called; NULL return means
 *     "not ready yet" and the task sleeps POLL_MS before retrying.
 *     Once a non-NULL URL is returned the fetch proceeds normally.
 *
 * Scheduling after startup:
 *   INTERVAL jobs sleep for interval_sec after each successful fetch.
 *   DAILY jobs fire once per calendar day at the configured local hour.
 *   force_now (via data_fetcher_request_now) bypasses both.
 *
 * Gates:
 *   g_dns_ready   — set by data_fetcher_notify_wifi_state()
 *   g_clock_ready — set by data_fetcher_notify_time_sync()
 *   Both must be true before any TLS connection.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "wifi_manager.h"

/* ---------------------------------------------------------------------------
 * Defaults (may be overridden in DataFetcherConfig)
 * ------------------------------------------------------------------------- */

#define DATA_FETCHER_MAX_RETRIES_DEFAULT   5U
#define DATA_FETCHER_RETRY_BASE_MS_DEFAULT 5000U
#define DATA_FETCHER_RETRY_MAX_MS_DEFAULT  60000U
#define DATA_FETCHER_TASK_STACK_DEFAULT    4096U
#define DATA_FETCHER_TASK_PRIORITY_DEFAULT 3U

/* ---------------------------------------------------------------------------
 * Schedule
 * ------------------------------------------------------------------------- */

typedef enum {
    FETCH_SCHEDULE_INTERVAL, /**< Repeat every interval_sec seconds. */
    FETCH_SCHEDULE_DAILY,    /**< Fire once per day at daily_hour (local time). */
} FetchScheduleType;

typedef struct {
    FetchScheduleType type;
    union {
        uint32_t interval_sec; /**< FETCH_SCHEDULE_INTERVAL */
        uint8_t  daily_hour;   /**< FETCH_SCHEDULE_DAILY — 0-23, local time */
    };
} FetchSchedule;

/* ---------------------------------------------------------------------------
 * FetchDescriptor
 * ------------------------------------------------------------------------- */

/**
 * @brief Describes one HTTP resource to fetch on a schedule.
 *
 * URL resolution (evaluated just before every fetch attempt):
 *
 *   If build_url is NULL:
 *     The static @p url pointer is used as-is (must remain valid for the
 *     lifetime of the fetcher).
 *
 *   If build_url is non-NULL:
 *     build_url(desc, url_buf, url_buf_size, ctx) is called. It must write a
 *     NUL-terminated URL into url_buf and return url_buf, or return NULL if
 *     the URL cannot be determined yet (e.g. clock not synced). Returning NULL
 *     causes the task to sleep POLL_MS and retry — no network attempt is made.
 *
 *     url_buf and url_buf_size must point to caller-owned storage that outlives
 *     the fetcher (stack of a never-returning task, or static/global memory).
 *     The static @p url field is unused when build_url is set.
 */
typedef struct FetchDescriptor {
    const char* id;          /**< Short identifier, used for logs and request_now(). */
    const char* url;         /**< Static URL, used when build_url is NULL. */
    const char* cache_key;   /**< Cache key, e.g. "fetch:weather_min". */
    uint32_t    cache_ttl_sec;

    FetchSchedule schedule;
    bool          fetch_on_startup; /**< Fetch (or serve from cache) immediately after gates open. */

    /**
     * @brief Optional dynamic URL builder.
     *
     * Called just before each fetch attempt (and before cache checks on
     * startup). Write the URL into @p buf (size @p buf_size) and return @p buf.
     * Return NULL to defer — the task will sleep and retry.
     *
     * @p ctx receives build_url_ctx from this descriptor.
     */
    char* (*build_url)(const struct FetchDescriptor* desc,
                       char* buf, size_t buf_size,
                       void* ctx);
    char*  url_buf;      /**< Buffer passed to build_url. */
    size_t url_buf_size; /**< Size of url_buf. */
    void*  build_url_ctx;
} FetchDescriptor;

/* ---------------------------------------------------------------------------
 * DataFetcherConfig
 * ------------------------------------------------------------------------- */

typedef void (*DataFetcherOnCached)(const FetchDescriptor* desc, void* ctx);

/**
 * @brief Called after a raw HTTP response is received, before caching.
 *
 * May modify @p buf / @p len in-place (e.g. decompress, strip a wrapper).
 * Return true to cache and fire on_cached; false to discard (no retry).
 */
typedef bool (*DataFetcherOnTransform)(const FetchDescriptor* desc,
                                       uint8_t** buf, size_t* len,
                                       void* ctx);

typedef struct {
    const FetchDescriptor* descriptors;
    size_t                 descriptor_count;

    uint32_t max_retries;
    uint32_t retry_base_ms;
    uint32_t retry_max_ms;
    uint32_t task_stack_size;
    uint32_t task_priority;

    DataFetcherOnCached    on_cached;
    void*                  on_cached_ctx;
    DataFetcherOnTransform on_transform;
    void*                  on_transform_ctx;
} DataFetcherConfig;

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/** Returns a DataFetcherConfig filled with all defaults. */
DataFetcherConfig data_fetcher_default_config(void);

/**
 * @brief Initialise the fetcher and spawn one FreeRTOS task per descriptor.
 * @return 0 on success, -1 on error.
 */
int data_fetcher_init(const DataFetcherConfig* config);

/** Open or close the DNS gate. Tasks unblock or pause accordingly. */
void data_fetcher_notify_wifi_state(WifiManagerState state);

/** Open the clock gate. TLS handshakes are permitted after this call. */
void data_fetcher_notify_time_sync(void);

/**
 * @brief Request an immediate fetch for the descriptor with the given id.
 *
 * Safe to call from any task. The fetch task will wake within POLL_MS.
 */
void data_fetcher_request_now(const char* descriptor_id);
