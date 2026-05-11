/**
 * @file data_fetcher.c
 * @brief Scheduled data-fetcher for elpris and weather endpoints — implementation.
 *
 * @details
 * Architecture overview
 * ---------------------
 * Two independent logical fetchers (elpris and weather) share a common
 * internal engine (@ref FetchJob).  Each job is driven by a single SMW task
 * that is registered once during @ref data_fetcher_init and runs indefinitely.
 *
 * State machine per job (FetchJobState)
 * --------------------------------------
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │                                                                 │
 *  │   IDLE ──[schedule elapsed OR force_now]──► WAITING_FOR_DNS    │
 *  │                                                  │              │
 *  │                                          [dns ready]           │
 *  │                                                  ▼              │
 *  │                                           REQUESTING           │
 *  │                                          /          \           │
 *  │                                   [success]       [error]      │
 *  │                                       │               │        │
 *  │                                    CACHING        RETRY_WAIT   │
 *  │                                       │           /       \    │
 *  │                                       │    [retries left] [max]│
 *  │                                       │         │          │   │
 *  │                                       └──► IDLE ◄──────────┘   │
 *  │                                                                 │
 *  └─────────────────────────────────────────────────────────────────┘
 *
 * The WAITING_FOR_DNS state exists so that a fetch triggered while DNS is
 * unavailable is not silently dropped — it is held pending until connectivity
 * is restored.
 *
 * Wi-Fi gating
 * ------------
 * @ref data_fetcher_notify_wifi_state sets / clears a shared volatile flag
 * (@ref g_dns_ready).  Jobs in REQUESTING or RETRY_WAIT that lose DNS abort
 * immediately and reset to IDLE; any pending HTTP handle is freed.
 *
 * Retry back-off
 * --------------
 * On failure the job waits @c retry_base_ms * 2^attempt milliseconds, capped
 * at @c retry_max_ms, before re-entering REQUESTING.  After @c max_retries
 * consecutive failures the job resets to IDLE and awaits the next scheduled
 * window.
 */

#include "data_fetcher.h"

#include "cache.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h" /* portMAX_DELAY, configTICK_RATE_HZ */
#include "freertos/task.h"     /* xTaskGetTickCount                  */
#include "http_client.h"
#include "smw.h"
#include "smw_http.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* =========================================================================
 * Internal constants
 * ====================================================================== */

/** @brief SMW poll wake interval while a HTTP request is in-flight (ms). */
#define POLL_INTERVAL_ACTIVE_MS 50U

/** @brief SMW poll wake interval while the job is idle / sleeping (ms). */
#define POLL_INTERVAL_IDLE_MS 5000U

/** @brief SMW poll wake interval while waiting for DNS (ms). */
#define POLL_INTERVAL_DNS_WAIT_MS 2000U

/** @brief Log tag for this module. */
static const char* g_tag = "data_fetcher";

/* =========================================================================
 * Internal types
 * ====================================================================== */

/** @brief Per-job operational state. */
typedef enum {
    /** Job is dormant; waiting for the next scheduled trigger time. */
    FETCH_STATE_IDLE = 0,
    /** Trigger time has elapsed but DNS is not yet available; holding. */
    FETCH_STATE_WAITING_FOR_DNS,
    /** HTTP request is in-flight via the SMW async machinery. */
    FETCH_STATE_REQUESTING,
    /** Response received; writing payload to cache. */
    FETCH_STATE_CACHING,
    /** A fetch attempt failed; observing back-off delay before retry. */
    FETCH_STATE_RETRY_WAIT,
} FetchJobState;

/**
 * @brief Identifies which of the two logical fetchers a job represents.
 *
 * Used only for logging; the behaviour is otherwise identical.
 */
typedef enum {
    FETCH_KIND_ELPRIS  = 0,
    FETCH_KIND_WEATHER = 1,
} FetchKind;

/* Forward declaration required so the is_due function pointer below can
 * reference FetchJob* inside the struct body before the typedef is complete.
 * Without the tag the compiler creates a hidden, unrelated 'struct FetchJob'
 * scoped to the parameter list, causing the incompatible-pointer-type errors. */
typedef struct FetchJob FetchJob;

/**
 * @brief Encapsulates all mutable state for a single scheduled fetch job.
 *
 * One instance exists per logical fetcher (elpris, weather).
 */
struct FetchJob {
    /* ---- static config (set once during init) ------------------------- */
    FetchKind kind;
    const char* url;        /**< Endpoint URL (points into g_cfg). */
    const char* cache_key;  /**< Cache key string literal.          */
    uint32_t cache_ttl_sec; /**< TTL passed to cache_put().         */

    /* ---- scheduling --------------------------------------------------- */
    /**
     * @brief Returns true when the job should fire given the current time.
     *
     * For elpris this checks whether the wall-clock hour matches
     * @c DataFetcherConfig.elpris_trigger_hour and the day has changed.
     * For weather this checks whether @c interval_sec has elapsed since
     * the last successful fetch.
     */
    bool (*is_due)(struct FetchJob* self, uint32_t now_ms);

    uint32_t interval_ms;   /**< Weather only: poll period in ms.   */
    uint8_t trigger_hour;   /**< Elpris only: hour of day (0–23).   */
    int last_trigger_day;   /**< Elpris only: tm_yday of last fire.  */
    uint32_t last_fetch_ms; /**< Timestamp of last successful fetch. */
    bool first_run;         /**< True until the first fetch fires.   */

    uint32_t initial_delay_ms; /**< Boot stagger: don't fire until this many ms after DNS. */
    uint32_t dns_ready_at_ms;

    /* ---- runtime state ------------------------------------------------ */
    FetchJobState state;
    uint32_t retry_count;
    uint32_t retry_deadline_ms; /**< Absolute ms timestamp when back-off expires. */

    /* ---- async HTTP handle -------------------------------------------- */
    HttpClientAsyncHandle* http_handle;

    /* ---- response mailbox (written by HTTP done-cb, read by poll) ----- */
    volatile bool response_ready;
    HttpClientResponse* response; /**< Heap-allocated; freed after caching. */
    volatile int response_err;

    /* ---- force-trigger flag (set by public API) ------------------------ */
    volatile bool force_now;
};

/* =========================================================================
 * Module-level globals
 * ====================================================================== */

static bool g_initialized        = false;
static DataFetcherConfig g_cfg   = {0};
static SmwWorker* g_worker       = NULL;
static volatile bool g_dns_ready = false;

/** @brief Job instances (elpris=0, weather=1). */
static FetchJob g_jobs[2];

/* =========================================================================
 * Forward declarations
 * ====================================================================== */

static SmwTaskStatus elpris_poll(void* context, uint32_t now_ms);
static SmwTaskStatus weather_poll(void* context, uint32_t now_ms);
static SmwTaskStatus job_poll(FetchJob* job, uint32_t now_ms);
static void job_begin_request(FetchJob* job);
static void job_handle_response(FetchJob* job, uint32_t now_ms);
static void job_schedule_retry(FetchJob* job, uint32_t now_ms);
static void job_reset_to_idle(FetchJob* job);
static void job_abort_in_flight(FetchJob* job);
static uint32_t compute_backoff_ms(const FetchJob* job);
static void on_http_done(HttpClientResponse* resp, int err, void* user_ctx);

/* =========================================================================
 * Schedule predicates
 * ====================================================================== */

/**
 * @brief Schedule predicate for the elpris job.
 *
 * Fires once per day when the local wall-clock hour first equals
 * @c trigger_hour.  Uses @c tm_yday so midnight roll-overs are handled
 * correctly across month and year boundaries.
 */
static bool elpris_is_due(FetchJob* self, uint32_t now_ms) {
    (void)now_ms;

    if (self->first_run) {
        return (now_ms - self->dns_ready_at_ms) >= self->initial_delay_ms;
    }

    struct tm timeinfo;
    time_t t = time(NULL);
    localtime_r(&t, &timeinfo);

    if (timeinfo.tm_hour != (int)self->trigger_hour) {
        return false;
    }
    if (timeinfo.tm_yday == self->last_trigger_day) {
        return false;
    }
    return true;
}

/**
 * @brief Schedule predicate for the weather job.
 *
 * Fires immediately on the first run; thereafter every @c interval_ms
 * milliseconds since the last successful fetch.
 */
static bool weather_is_due(FetchJob* self, uint32_t now_ms) {
    if (self->first_run) {
        return (now_ms - self->dns_ready_at_ms) >= self->initial_delay_ms;
    }
    return (now_ms - self->last_fetch_ms) >= self->interval_ms;
}

/* =========================================================================
 * Back-off helper
 * ====================================================================== */

/**
 * @brief Computes the back-off delay for the current retry attempt.
 *
 * Implements binary exponential back-off:
 *   delay = min(base * 2^attempt, max)
 *
 * @param job  Job whose retry_count drives the calculation.
 * @return     Delay in milliseconds.
 */
static uint32_t compute_backoff_ms(const FetchJob* job) {
    uint32_t delay = g_cfg.retry_base_ms;
    for (uint32_t i = 0; i < job->retry_count; i++) {
        delay *= 2U;
        if (delay >= g_cfg.retry_max_ms) {
            return g_cfg.retry_max_ms;
        }
    }
    return delay;
}

/* =========================================================================
 * HTTP done callback
 * ====================================================================== */

/**
 * @brief Invoked by the HTTP async layer when a request completes (or fails).
 *
 * This callback may be called from a different execution context, so it only
 * writes to the volatile mailbox fields; the actual response handling and
 * cache write are performed in the job's SMW poll function.
 *
 * @param resp      Heap-allocated response (may be NULL on error).
 * @param err       0 on success, non-zero on failure.
 * @param user_ctx  Pointer to the owning @ref FetchJob.
 */
static void on_http_done(HttpClientResponse* resp, int err, void* user_ctx) {
    FetchJob* job = (FetchJob*)user_ctx;

    job->response       = resp;
    job->response_err   = err;
    job->response_ready = true;
}

/* =========================================================================
 * Job lifecycle helpers
 * ====================================================================== */

/**
 * @brief Initiates an asynchronous HTTP GET for the job's configured URL.
 *
 * Transitions the job to FETCH_STATE_REQUESTING on success.
 * Logs an error and schedules a retry if the HTTP layer refuses the request.
 */
static void job_begin_request(FetchJob* job) {
    ESP_LOGI(g_tag, "[%s] Starting fetch → %s",
             job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather", job->url);

    HttpClientRequest req = {0};
    req.url               = job->url;
    req.method            = HTTP_CLIENT_METHOD_GET;

    job->response_ready = false;
    job->response       = NULL;
    job->response_err   = 0;

    job->http_handle = http_client_async_begin(&req, on_http_done, job);
    if (!job->http_handle) {
        ESP_LOGE(g_tag, "[%s] http_client_async_begin failed",
                 job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
        job_schedule_retry(job, (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
        return;
    }

    job->state = FETCH_STATE_REQUESTING;
}

/**
 * @brief Processes a completed HTTP response: validates it and writes to cache.
 *
 * On success, transitions to FETCH_STATE_IDLE and resets retry counters.
 * On failure, delegates to @ref job_schedule_retry.
 *
 * @param job     The job whose @c response mailbox has been populated.
 * @param now_ms  Current system timestamp in milliseconds.
 */
static void job_handle_response(FetchJob* job, uint32_t now_ms) {
    const char* kind_str = job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather";

    HttpClientResponse* resp = job->response;
    int err                  = job->response_err;

    /* Clear mailbox before any early-return path. */
    job->response       = NULL;
    job->response_ready = false;

    if (err != 0 || resp == NULL) {
        ESP_LOGE(g_tag, "[%s] Fetch failed (err=%d)", kind_str, err);
        if (resp) {
            free(resp->buffer);
            free(resp);
        }
        job_schedule_retry(job, now_ms);
        return;
    }

    if (resp->status < 200 || resp->status >= 300) {
        ESP_LOGE(g_tag, "[%s] HTTP %d — treating as failure", kind_str, resp->status);
        free(resp->buffer);
        free(resp);
        job_schedule_retry(job, now_ms);
        return;
    }

    if (resp->buffer == NULL || resp->length == 0U) {
        ESP_LOGW(g_tag, "[%s] HTTP %d but empty body — treating as failure", kind_str,
                 resp->status);
        free(resp->buffer);
        free(resp);
        job_schedule_retry(job, now_ms);
        return;
    }

    /* ---- Write to cache ------------------------------------------------ */
    int rc = cache_put(job->cache_key, resp->buffer, resp->length, job->cache_ttl_sec);
    if (rc != CACHE_OK) {
        ESP_LOGE(g_tag, "[%s] cache_put failed (rc=%d) — data not persisted", kind_str, rc);
        /* Non-fatal: the response was valid; still treat as success so we
         * don't hammer the server with retries due to a filesystem issue. */
    } else {
        ESP_LOGI(g_tag, "[%s] Cached %zu bytes (TTL %lu s) under key \"%s\"", kind_str,
                 resp->length, (unsigned long)job->cache_ttl_sec, job->cache_key);

        /* Notify the caller that fresh data is available in the cache. */
        if (g_cfg.on_cached) {
            DataFetcherKind public_kind = (job->kind == FETCH_KIND_ELPRIS)
                                              ? DATA_FETCHER_KIND_ELPRIS
                                              : DATA_FETCHER_KIND_WEATHER;
            g_cfg.on_cached(public_kind, job->cache_key, g_cfg.on_cached_ctx);
        }
    }

    free(resp->buffer);
    free(resp);

    /* ---- Success bookkeeping ------------------------------------------ */
    job->last_fetch_ms = now_ms;
    job->retry_count   = 0U;
    job->first_run     = false;

    if (job->kind == FETCH_KIND_ELPRIS) {
        struct tm timeinfo;
        time_t t = time(NULL);
        localtime_r(&t, &timeinfo);
        job->last_trigger_day = timeinfo.tm_yday;
    }

    job_reset_to_idle(job);
}

/**
 * @brief Increments the retry counter and either arms the back-off timer or
 *        gives up and resets to idle.
 *
 * @param job     The job that just failed.
 * @param now_ms  Current system timestamp in milliseconds.
 */
static void job_schedule_retry(FetchJob* job, uint32_t now_ms) {
    const char* kind_str = job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather";

    job->retry_count++;

    if (job->retry_count > g_cfg.max_retries) {
        ESP_LOGE(g_tag, "[%s] Exhausted %lu retries — giving up until next window", kind_str,
                 (unsigned long)g_cfg.max_retries);
        job->retry_count = 0U;
        job_reset_to_idle(job);
        return;
    }

    uint32_t delay_ms = compute_backoff_ms(job);
    ESP_LOGW(g_tag, "[%s] Retry %lu/%lu in %lu ms", kind_str, (unsigned long)job->retry_count,
             (unsigned long)g_cfg.max_retries, (unsigned long)delay_ms);

    job->retry_deadline_ms = now_ms + delay_ms;
    job->state             = FETCH_STATE_RETRY_WAIT;
}

/**
 * @brief Resets the job to the idle state, freeing any in-flight handle.
 *
 * @param job  Job to reset.
 */
static void job_reset_to_idle(FetchJob* job) {
    job_abort_in_flight(job);
    job->state     = FETCH_STATE_IDLE;
    job->force_now = false;
}

/**
 * @brief Frees an in-flight HTTP handle if one exists.
 *
 * Safe to call when @c http_handle is NULL.
 *
 * @param job  Job whose handle should be freed.
 */
static void job_abort_in_flight(FetchJob* job) {
    if (job->http_handle) {
        http_client_async_free(job->http_handle);
        job->http_handle = NULL;
    }
    /* Discard any already-delivered response that we won't process. */
    if (job->response) {
        free(job->response->buffer);
        free(job->response);
        job->response = NULL;
    }
    job->response_ready = false;
}

/* =========================================================================
 * Core poll engine — shared by both jobs
 * ====================================================================== */

/**
 * @brief Central state-machine step for a single fetch job.
 *
 * Called from the SMW task poll callbacks for elpris and weather.
 *
 * @param job     The job to advance.
 * @param now_ms  Current system timestamp from the SMW scheduler.
 * @return        SMW task status; @c SMW_TASK_RUNNING always (jobs run forever).
 */
static SmwTaskStatus job_poll(FetchJob* job, uint32_t now_ms) { // NOLINT(readability-function-size)
    /* ---- Global DNS gate --------------------------------------------- */
    bool dns = (bool)g_dns_ready;

    /* If we lose DNS while a request is in-flight, abort it cleanly. */
    if (!dns && job->state == FETCH_STATE_REQUESTING) {
        ESP_LOGW(g_tag, "[%s] DNS lost mid-request — aborting",
                 job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
        job_abort_in_flight(job);
        job->state = FETCH_STATE_IDLE;
    }

    /* If we lose DNS during a retry wait, return to idle to avoid stale
     * timestamps causing an immediate fire on reconnect. */
    if (!dns && job->state == FETCH_STATE_RETRY_WAIT) {
        ESP_LOGW(g_tag, "[%s] DNS lost during retry wait — resetting",
                 job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
        job->retry_count = 0U;
        job->state       = FETCH_STATE_IDLE;
    }

    switch (job->state) {

    /* ------------------------------------------------------------------ */
    case FETCH_STATE_IDLE: {
        bool triggered = ((int)(bool)job->force_now || (int)job->is_due(job, now_ms)) != 0;
        if (!triggered) {
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_IDLE_MS, .code = SMW_TASK_RUNNING};
        }

        job->force_now = false;

        if (!dns) {
            ESP_LOGW(g_tag, "[%s] Triggered but DNS unavailable — holding",
                     job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
            job->state = FETCH_STATE_WAITING_FOR_DNS;
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_DNS_WAIT_MS,
                                   .code         = SMW_TASK_RUNNING};
        }

        job_begin_request(job);
        return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS, .code = SMW_TASK_RUNNING};
    }

    /* ------------------------------------------------------------------ */
    case FETCH_STATE_WAITING_FOR_DNS: {
        if (!dns) {
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_DNS_WAIT_MS,
                                   .code         = SMW_TASK_RUNNING};
        }
        /* Respect the boot stagger even when we were already waiting. */
        if ((int)job->first_run && (now_ms - job->dns_ready_at_ms) < job->initial_delay_ms) {
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS,
                                   .code         = SMW_TASK_RUNNING};
        }
        ESP_LOGI(g_tag, "[%s] DNS now available — starting held request",
                 job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
        job_begin_request(job);
        return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS, .code = SMW_TASK_RUNNING};
    }

    /* ------------------------------------------------------------------ */
    case FETCH_STATE_REQUESTING: {
        if (!job->http_handle) {
            /* Should not happen; guard against stale state. */
            ESP_LOGE(g_tag, "[%s] REQUESTING but no HTTP handle — resetting",
                     job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
            job_schedule_retry(job, now_ms);
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS,
                                   .code         = SMW_TASK_RUNNING};
        }

        /* Drive the underlying async HTTP state machine forward. */
        HttpClientPollResult poll_res = http_client_async_poll(job->http_handle);

        if (poll_res == HTTP_CLIENT_POLL_BUSY) {
            /* Still in-flight; come back next tick. */
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS,
                                   .code         = SMW_TASK_RUNNING};
        }

        /* HTTP layer is done — the done-callback has fired and populated the
         * mailbox.  Free the handle before processing the response so that
         * job_handle_response can call job_schedule_retry cleanly. */
        http_client_async_free(job->http_handle);
        job->http_handle = NULL;

        job->state = FETCH_STATE_CACHING;

        /* Deliberate fall-through to CACHING. */
    }
    /* FALLTHROUGH */

    /* ------------------------------------------------------------------ */
    case FETCH_STATE_CACHING: {
        if (!job->response_ready) {
            /* Mailbox not yet written — wait one more tick.
             * This window is extremely narrow (same poll cycle) but the guard
             * makes the state machine robust against any ordering edge-cases. */
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS,
                                   .code         = SMW_TASK_RUNNING};
        }
        job_handle_response(job, now_ms);
        return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_IDLE_MS, .code = SMW_TASK_RUNNING};
    }

    /* ------------------------------------------------------------------ */
    case FETCH_STATE_RETRY_WAIT: {
        if ((int32_t)(now_ms - job->retry_deadline_ms) < 0) {
            /* Still waiting. */
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS,
                                   .code         = SMW_TASK_RUNNING};
        }

        if (!dns) {
            /* Back-off elapsed but DNS is gone; park until it returns. */
            ESP_LOGW(g_tag, "[%s] Back-off elapsed but no DNS — holding",
                     job->kind == FETCH_KIND_ELPRIS ? "elpris" : "weather");
            job->state = FETCH_STATE_WAITING_FOR_DNS;
            return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_DNS_WAIT_MS,
                                   .code         = SMW_TASK_RUNNING};
        }

        job_begin_request(job);
        return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_ACTIVE_MS, .code = SMW_TASK_RUNNING};
    }

    default:
        ESP_LOGE(g_tag, "Unknown FetchJobState %d — resetting to idle", (int)job->state);
        job_reset_to_idle(job);
        return (SmwTaskStatus){.next_wake_ms = POLL_INTERVAL_IDLE_MS, .code = SMW_TASK_RUNNING};
    }
}

/* =========================================================================
 * SMW task callbacks (thin shims into the shared engine)
 * ====================================================================== */

/**
 * @brief SMW poll callback for the elpris fetch job.
 *
 * @param context  Pointer to the elpris @ref FetchJob instance.
 * @param now_ms   Current timestamp supplied by the SMW scheduler.
 * @return         @ref SmwTaskStatus forwarded from @ref job_poll.
 */
static SmwTaskStatus elpris_poll(void* context, uint32_t now_ms) {
    return job_poll((FetchJob*)context, now_ms);
}

/**
 * @brief SMW poll callback for the weather fetch job.
 *
 * @param context  Pointer to the weather @ref FetchJob instance.
 * @param now_ms   Current timestamp supplied by the SMW scheduler.
 * @return         @ref SmwTaskStatus forwarded from @ref job_poll.
 */
static SmwTaskStatus weather_poll(void* context, uint32_t now_ms) {
    return job_poll((FetchJob*)context, now_ms);
}

/* =========================================================================
 * Public API
 * ====================================================================== */

DataFetcherConfig data_fetcher_default_config(void) {
    DataFetcherConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.elpris_url           = NULL;
    cfg.weather_url          = NULL;
    cfg.elpris_trigger_hour  = DATA_FETCHER_ELPRIS_HOUR_DEFAULT;
    cfg.weather_interval_sec = DATA_FETCHER_WEATHER_INTERVAL_SEC_DEFAULT;
    cfg.max_retries          = DATA_FETCHER_MAX_RETRIES_DEFAULT;
    cfg.retry_base_ms        = DATA_FETCHER_RETRY_BASE_MS_DEFAULT;
    cfg.retry_max_ms         = DATA_FETCHER_RETRY_MAX_MS_DEFAULT;
    return cfg;
}

int data_fetcher_init(const DataFetcherConfig* config, SmwWorker* worker) {
    if (!config || !worker) {
        ESP_LOGE(g_tag, "data_fetcher_init: NULL argument");
        return -1;
    }

    if (g_initialized) {
        ESP_LOGW(g_tag, "Already initialised — ignoring duplicate init call");
        return 0;
    }

    g_cfg    = *config;
    g_worker = worker;

    /* ---- Elpris job --------------------------------------------------- */
    FetchJob* elpris = &g_jobs[FETCH_KIND_ELPRIS];
    memset(elpris, 0, sizeof(*elpris));
    elpris->kind             = FETCH_KIND_ELPRIS;
    elpris->url              = g_cfg.elpris_url;
    elpris->cache_key        = DATA_FETCHER_CACHE_KEY_ELPRIS;
    elpris->cache_ttl_sec    = DATA_FETCHER_ELPRIS_CACHE_TTL_SEC;
    elpris->trigger_hour     = g_cfg.elpris_trigger_hour;
    elpris->last_trigger_day = -1;
    elpris->is_due           = elpris_is_due;
    elpris->state            = FETCH_STATE_IDLE;
    elpris->first_run        = true;
    elpris->initial_delay_ms = 0U; /* fires immediately on DNS ready */

    /* ---- Weather job -------------------------------------------------- */
    FetchJob* weather = &g_jobs[FETCH_KIND_WEATHER];
    memset(weather, 0, sizeof(*weather));
    weather->kind             = FETCH_KIND_WEATHER;
    weather->url              = g_cfg.weather_url;
    weather->cache_key        = DATA_FETCHER_CACHE_KEY_WEATHER;
    weather->cache_ttl_sec    = DATA_FETCHER_WEATHER_CACHE_TTL_SEC;
    weather->interval_ms      = g_cfg.weather_interval_sec * 1000U;
    weather->is_due           = weather_is_due;
    weather->state            = FETCH_STATE_IDLE;
    weather->first_run        = true;
    weather->initial_delay_ms = 3000U; /* fires 3 s after elpris clears  */

    /* ---- Register SMW tasks ------------------------------------------- */
    SmwResult rc;

    if (g_cfg.elpris_url) {
        rc = smw_register_task(g_worker, elpris_poll, elpris, NULL);
        if (rc != SMW_OK) {
            ESP_LOGE(g_tag, "Failed to register elpris SMW task (rc=%d)", (int)rc);
            return -1;
        }
        ESP_LOGI(g_tag, "Elpris fetch scheduled — triggers daily at %02u:00 → %s",
                 (unsigned)g_cfg.elpris_trigger_hour, g_cfg.elpris_url);
    } else {
        ESP_LOGW(g_tag, "Elpris URL not configured — elpris fetching disabled");
    }

    if (g_cfg.weather_url) {
        rc = smw_register_task(g_worker, weather_poll, weather, NULL);
        if (rc != SMW_OK) {
            ESP_LOGE(g_tag, "Failed to register weather SMW task (rc=%d)", (int)rc);
            return -1;
        }
        ESP_LOGI(g_tag, "Weather fetch scheduled — interval %lu s → %s",
                 (unsigned long)g_cfg.weather_interval_sec, g_cfg.weather_url);
    } else {
        ESP_LOGW(g_tag, "Weather URL not configured — weather fetching disabled");
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Initialised (retries=%lu, base=%lu ms, max=%lu ms)",
             (unsigned long)g_cfg.max_retries, (unsigned long)g_cfg.retry_base_ms,
             (unsigned long)g_cfg.retry_max_ms);
    return 0;
}

void data_fetcher_notify_wifi_state(WifiManagerState state) {
    bool was_ready = (bool)g_dns_ready;
    bool now_ready = (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS);

    g_dns_ready = now_ready;

    if (!was_ready && (int)now_ready) {
        uint32_t now_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        g_jobs[FETCH_KIND_ELPRIS].dns_ready_at_ms  = now_ms;
        g_jobs[FETCH_KIND_WEATHER].dns_ready_at_ms = now_ms;

        if (!g_jobs[FETCH_KIND_WEATHER].first_run) {
            g_jobs[FETCH_KIND_WEATHER].force_now = true;
        }

        ESP_LOGI(g_tag, "DNS available — fetch jobs unblocked");
    } else if ((int)was_ready && !now_ready) {
        ESP_LOGW(g_tag, "DNS lost — fetch jobs suspended");
    }
}

void data_fetcher_request_weather_now(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "data_fetcher_request_weather_now: not initialised");
        return;
    }
    if (!g_cfg.weather_url) {
        ESP_LOGW(g_tag, "data_fetcher_request_weather_now: weather URL not set");
        return;
    }
    FetchJob* job = &g_jobs[FETCH_KIND_WEATHER];
    if (job->state != FETCH_STATE_IDLE) {
        ESP_LOGW(g_tag, "[weather] Force-fetch requested but job is busy (state=%d)",
                 (int)job->state);
        return;
    }
    ESP_LOGI(g_tag, "[weather] Force-fetch requested");
    job->force_now = true;
}

void data_fetcher_request_elpris_now(void) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "data_fetcher_request_elpris_now: not initialised");
        return;
    }
    if (!g_cfg.elpris_url) {
        ESP_LOGW(g_tag, "data_fetcher_request_elpris_now: elpris URL not set");
        return;
    }
    FetchJob* job = &g_jobs[FETCH_KIND_ELPRIS];
    if (job->state != FETCH_STATE_IDLE) {
        ESP_LOGW(g_tag, "[elpris] Force-fetch requested but job is busy (state=%d)",
                 (int)job->state);
        return;
    }
    ESP_LOGI(g_tag, "[elpris] Force-fetch requested");
    job->force_now = true;
}
