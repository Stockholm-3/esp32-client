/**
 * @file data_fetcher.h
 * @brief Scheduled data-fetcher for elpris and weather endpoints.
 *
 * @details
 * Two independent fetch schedules are managed by a single module:
 *
 *   - **Elpris**  — fires once per day at a configurable wall-clock hour
 *                   (default 14:00 local time).
 *   - **Weather** — fires on a fixed interval (default every 15 minutes).
 *
 * Both fetches are driven from the SMW (State Machine Worker) scheduler so
 * no additional FreeRTOS tasks are created.  The module tracks Wi-Fi / DNS
 * readiness internally; all network activity is suppressed until
 * @ref data_fetcher_notify_wifi_state is called with a connected-with-DNS
 * state, and suspended again on any disconnect.
 *
 * On a successful fetch the raw response body is written to the LittleFS-
 * backed cache via the global @c cache_*() API using the keys defined by
 * @ref DATA_FETCHER_CACHE_KEY_ELPRIS and @ref DATA_FETCHER_CACHE_KEY_WEATHER.
 *
 * On failure the fetch is retried up to @c max_retries times with
 * exponential back-off before the next scheduled window is awaited.
 *
 * @par Typical usage
 * @code
 *   DataFetcherConfig cfg = data_fetcher_default_config();
 *   cfg.elpris_url  = "https://example.com/v1/elpris?city=stockholm";
 *   cfg.weather_url = "https://example.com/v1/weather?lat=59.33&lon=18.06";
 *
 *   data_fetcher_init(&cfg, &g_smw_worker);
 * @endcode
 *
 * After initialisation call @ref data_fetcher_notify_wifi_state whenever the
 * Wi-Fi manager state changes (mirrors the pattern used by @c loc_server and
 * @c ui_binder in this project).
 */

#ifndef DATA_FETCHER_H
#define DATA_FETCHER_H

#include "smw.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * Cache keys
 * ====================================================================== */

/** @brief Cache key used to store the most recent elpris payload. */
#define DATA_FETCHER_CACHE_KEY_ELPRIS "data_fetcher:elpris"

/** @brief Cache key used to store the most recent weather payload. */
#define DATA_FETCHER_CACHE_KEY_WEATHER "data_fetcher:weather"

/** @brief Identifies which data source produced a fresh cache entry. */
typedef enum {
    DATA_FETCHER_KIND_ELPRIS  = 0,
    DATA_FETCHER_KIND_WEATHER = 1,
} DataFetcherKind;

/**
 * @brief Invoked from the SMW worker context after a payload has been
 *        successfully written to the cache.
 *
 * @param kind      Which fetcher produced the data.
 * @param cache_key Cache key the payload is stored under (use cache_get_alloc
 *                  to read it, cache_free to release).
 * @param user_ctx  User-supplied context pointer from @ref DataFetcherConfig.
 */
typedef void (*DataFetcherOnCachedCb)(DataFetcherKind kind, const char* cache_key, void* user_ctx);

/* =========================================================================
 * Default tunables
 * ====================================================================== */

/** @brief Default hour of day (0–23, local time) at which elpris is fetched. */
#define DATA_FETCHER_ELPRIS_HOUR_DEFAULT 14

/** @brief Default weather poll interval in seconds. */
#define DATA_FETCHER_WEATHER_INTERVAL_SEC_DEFAULT (15U * 60U)

/** @brief Default maximum number of consecutive retry attempts per fetch. */
#define DATA_FETCHER_MAX_RETRIES_DEFAULT 5U

/** @brief Initial retry back-off delay in milliseconds. */
#define DATA_FETCHER_RETRY_BASE_MS_DEFAULT 5000U

/** @brief Maximum retry back-off delay in milliseconds (caps the exponential growth). */
#define DATA_FETCHER_RETRY_MAX_MS_DEFAULT (5U * 60U * 1000U)

/** @brief TTL (seconds) written to the cache for elpris entries (24 h). */
#define DATA_FETCHER_ELPRIS_CACHE_TTL_SEC (24U * 3600U)

/** @brief TTL (seconds) written to the cache for weather entries (30 min). */
#define DATA_FETCHER_WEATHER_CACHE_TTL_SEC (30U * 60U)

/* =========================================================================
 * Configuration
 * ====================================================================== */

/**
 * @brief Compile-time configuration for the data fetcher.
 *
 * Populate the URL fields before passing this structure to
 * @ref data_fetcher_init.  All other fields have sensible defaults provided
 * by @ref data_fetcher_default_config.
 */
typedef struct {
    /**
     * @brief Fully-qualified URL for the elpris endpoint.
     *
     * Must remain valid for the lifetime of the module (i.e. point to a
     * string literal or a buffer that outlives the application).
     * Set to NULL to disable elpris fetching entirely.
     */
    const char* elpris_url;

    /**
     * @brief Fully-qualified URL for the weather endpoint.
     *
     * Must remain valid for the lifetime of the module.
     * Set to NULL to disable weather fetching entirely.
     */
    const char* weather_url;

    /**
     * @brief Hour of day (0–23, local/wall-clock time) at which the daily
     *        elpris fetch is triggered.
     */
    uint8_t elpris_trigger_hour;

    /**
     * @brief Weather poll period in seconds.
     *
     * The first fetch is issued immediately once DNS becomes available;
     * subsequent fetches fire every @c weather_interval_sec seconds.
     */
    uint32_t weather_interval_sec;

    /**
     * @brief Maximum number of consecutive retry attempts before the module
     *        gives up and waits for the next scheduled window.
     */
    uint32_t max_retries;

    /**
     * @brief Initial retry delay in milliseconds (doubles on each attempt,
     *        capped at @c retry_max_ms).
     */
    uint32_t retry_base_ms;

    /**
     * @brief Maximum retry delay in milliseconds.
     */
    uint32_t retry_max_ms;

    /** @brief Called after each successful cache write. NULL = no notification. */
    DataFetcherOnCachedCb on_cached;

    /** @brief Passed through unchanged to @c on_cached. */
    void* on_cached_ctx;

} DataFetcherConfig;

/* =========================================================================
 * Public API
 * ====================================================================== */

/**
 * @brief Returns a @ref DataFetcherConfig populated with all default values.
 *
 * URL fields are left NULL; the caller must supply them before calling
 * @ref data_fetcher_init.
 *
 * @return Default configuration structure.
 */
DataFetcherConfig data_fetcher_default_config(void);

/**
 * @brief Initialises the data fetcher and registers SMW tasks.
 *
 * @param config  Pointer to a fully populated configuration structure.
 *                The contents are copied; the caller may free or reuse the
 *                structure after this call returns.
 * @param worker  SMW worker instance that will drive the fetch tasks.
 *
 * @return  0 on success, -1 on failure (e.g. NULL pointer, SMW queue full).
 */
int data_fetcher_init(const DataFetcherConfig* config, SmwWorker* worker);

/**
 * @brief Notifies the data fetcher of a Wi-Fi manager state change.
 *
 * Must be called from the same Wi-Fi manager callback registered in main.c.
 * Fetching is enabled only while the state is
 * @c WIFI_MANAGER_STATE_CONNECTED_WITH_DNS and suspended for all other
 * states.
 *
 * @param state  New Wi-Fi manager state.
 */
void data_fetcher_notify_wifi_state(WifiManagerState state);

/**
 * @brief Triggers an immediate out-of-schedule weather fetch.
 *
 * Useful after a manual user action (e.g. pressing a "refresh" button on the
 * UI).  Has no effect if Wi-Fi / DNS is not available, or if a fetch is
 * already in progress.
 */
void data_fetcher_request_weather_now(void);

/**
 * @brief Triggers an immediate out-of-schedule elpris fetch.
 *
 * @see data_fetcher_request_weather_now
 */
void data_fetcher_request_elpris_now(void);

#ifdef __cplusplus
}
#endif

#endif /* DATA_FETCHER_H */
