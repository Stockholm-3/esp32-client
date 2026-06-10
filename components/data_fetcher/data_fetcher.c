/**
 * @file data_fetcher.c
 * @brief Generic scheduled HTTP data fetcher — FreeRTOS task-based implementation.
 *
 * Each descriptor gets one task running a simple loop:
 *   1. Wait until both DNS and clock gates are open.
 *   2. On first run with fetch_on_startup: honour startup_delay_ms, then fetch.
 *   3. On subsequent runs: wait until the schedule is due or force_now is set.
 *   4. Fetch with exponential back-off on failure up to max_retries.
 *   5. Sleep until next window, checking force_now every POLL_MS.
 *   6. Repeat.
 *
 * The clock gate (g_clock_ready) is critical. mbedTLS verifies certificate
 * validity dates against time(NULL). If the clock is still at epoch, every TLS
 * handshake fails with -0x2700 (MBEDTLS_ERR_X509_CERT_VERIFY_FAILED), which
 * leaves heap fragmented and causes all subsequent connections to fail with
 * -0x008D (MBEDTLS_ERR_SSL_ALLOC_FAILED). Call data_fetcher_notify_time_sync()
 * from your SNTP callback to open this gate.
 */

#include "data_fetcher.h"

#include "cache.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "http_client.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* TAG = "data_fetcher";

#define POLL_MS 2000U

/* ---------------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------------- */

typedef struct {
    const FetchDescriptor* desc;
    volatile bool          force_now;
} FetchJob;

/* ---------------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------------- */

static bool              g_initialized = false;
static DataFetcherConfig g_cfg         = {0};
static volatile bool     g_dns_ready   = false;
static volatile bool     g_clock_ready = false;
static FetchJob*         g_jobs        = NULL;
static size_t            g_job_count   = 0;

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static void sleep_ms(uint32_t ms) {
    if (ms > 0) vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * Sleep for total_ms in POLL_MS chunks.
 * Returns true immediately if force_now fires.
 * Returns false if DNS is lost during the sleep.
 */
static bool interruptible_sleep(FetchJob* job, uint32_t total_ms) {
    uint32_t remaining = total_ms;
    while (remaining > 0) {
        uint32_t chunk = remaining < POLL_MS ? remaining : POLL_MS;
        sleep_ms(chunk);
        remaining -= chunk;
        if (job->force_now)  return true;
        if (!g_dns_ready)    return false;
    }
    return false;
}

static uint32_t compute_backoff_ms(uint32_t retry_index) {
    uint32_t delay = g_cfg.retry_base_ms;
    for (uint32_t i = 0; i < retry_index; i++) {
        delay *= 2U;
        if (delay >= g_cfg.retry_max_ms) return g_cfg.retry_max_ms;
    }
    return delay;
}

/**
 * Block until both DNS is available and the clock is synchronised.
 * Logs once per condition so the reason for waiting is always visible.
 */
static void wait_for_ready(const char* id) {
    bool logged_dns = false, logged_clk = false;
    while (!g_dns_ready || !g_clock_ready) {
        if (!g_dns_ready && !logged_dns) {
            ESP_LOGW(TAG, "[%s] Waiting for DNS...", id);
            logged_dns = true;
        }
        if (g_dns_ready && !g_clock_ready && !logged_clk) {
            ESP_LOGW(TAG, "[%s] DNS ready, waiting for clock sync...", id);
            logged_clk = true;
        }
        sleep_ms(POLL_MS);
    }
    if (logged_dns || logged_clk) {
        ESP_LOGI(TAG, "[%s] Ready (DNS + clock)", id);
    }
}

/** Seconds until the next occurrence of target_hour (always > 0). */
static uint32_t secs_until_daily_hour(uint8_t target_hour) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    int diff = (int)target_hour * 3600 - (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
    if (diff <= 0) diff += 24 * 3600;
    return (uint32_t)diff;
}

/* ---------------------------------------------------------------------------
 * Fetch + cache
 * ------------------------------------------------------------------------- */

static bool do_fetch(const FetchDescriptor* desc) {
    ESP_LOGI(TAG, "[%s] Fetching %s", desc->id, desc->url);

    HttpClientRequest  req  = {.url = desc->url, .method = HTTP_CLIENT_METHOD_GET};
    HttpClientResponse resp = {0};

    if (http_client_perform(&req, &resp) != 0) {
        ESP_LOGE(TAG, "[%s] http_client_perform failed", desc->id);
        return false;
    }
    if (resp.status < 200 || resp.status >= 300) {
        ESP_LOGE(TAG, "[%s] HTTP %d", desc->id, resp.status);
        free(resp.buffer);
        return false;
    }
    if (!resp.buffer || resp.length == 0U) {
        ESP_LOGW(TAG, "[%s] HTTP %d empty body", desc->id, resp.status);
        free(resp.buffer);
        return false;
    }

    uint8_t* buf = resp.buffer;
    size_t   len = resp.length;

    if (g_cfg.on_transform) {
        if (!g_cfg.on_transform(desc, &buf, &len, g_cfg.on_transform_ctx)) {
            ESP_LOGI(TAG, "[%s] Transform discarded response", desc->id);
            free(buf);
            return true; /* discard counts as success, no retry */
        }
    }

    int rc = cache_put(desc->cache_key, buf, len, desc->cache_ttl_sec);
    free(buf);

    if (rc != CACHE_OK) {
        ESP_LOGE(TAG, "[%s] cache_put failed (rc=%d)", desc->id, rc);
    } else {
        ESP_LOGI(TAG, "[%s] Cached %zu B (TTL %lu s)", desc->id, len,
                 (unsigned long)desc->cache_ttl_sec);
        if (g_cfg.on_cached) g_cfg.on_cached(desc, g_cfg.on_cached_ctx);
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Per-job task
 * ------------------------------------------------------------------------- */

static void fetch_task(void* arg) {
    FetchJob*              job = (FetchJob*)arg;
    const FetchDescriptor* d   = job->desc;

    bool     first_run        = true;
    uint32_t last_fetch_ms    = 0;
    int      last_trigger_day = -1;

    while (1) {
        /* 1. Gate: wait for DNS + synced clock before any TLS attempt. */
        wait_for_ready(d->id);

        /* 2. Startup fetch — run immediately (after optional stagger delay). */
        if (first_run && d->fetch_on_startup) {
            if (d->startup_delay_ms > 0) {
                ESP_LOGI(TAG, "[%s] Startup delay %lu ms", d->id,
                         (unsigned long)d->startup_delay_ms);
                interruptible_sleep(job, d->startup_delay_ms);
            }
            goto do_fetch_now;
        }

        /* 3. Wait until scheduled or force_now. */
        while (!job->force_now) {
            if (!g_dns_ready) break; /* DNS lost — restart outer loop */

            if (d->schedule.type == FETCH_SCHEDULE_INTERVAL) {
                uint32_t elapsed     = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS)
                                       - last_fetch_ms;
                uint32_t interval_ms = d->schedule.interval_sec * 1000U;
                if (first_run || elapsed >= interval_ms) break; /* due */
                uint32_t wait = interval_ms - elapsed;
                interruptible_sleep(job, wait < POLL_MS ? wait : POLL_MS);

            } else if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
                struct tm tm; time_t t = time(NULL); localtime_r(&t, &tm);
                if (tm.tm_hour == (int)d->schedule.daily_hour
                    && tm.tm_yday != last_trigger_day) break; /* due */
                uint32_t secs = secs_until_daily_hour(d->schedule.daily_hour);
                /* Sleep up to 60 s at a time so force_now is responsive. */
                interruptible_sleep(job, secs * 1000U < 60000U ? secs * 1000U : 60000U);

            } else {
                sleep_ms(POLL_MS);
            }
        }

        if (!g_dns_ready) continue; /* restart outer loop */

do_fetch_now:
        job->force_now = false;

        /* 4. Fetch with exponential back-off. */
        {
            bool     success = false;
            uint32_t retries = 0;

            while (!success) {
                if (!g_dns_ready) {
                    ESP_LOGW(TAG, "[%s] DNS lost — aborting fetch window", d->id);
                    break;
                }
                success = do_fetch(d);
                if (!success) {
                    if (++retries > g_cfg.max_retries) {
                        ESP_LOGE(TAG, "[%s] Exhausted %lu retries", d->id,
                                 (unsigned long)g_cfg.max_retries);
                        break;
                    }
                    uint32_t delay = compute_backoff_ms(retries - 1U);
                    ESP_LOGW(TAG, "[%s] Retry %lu/%lu in %lu ms", d->id,
                             (unsigned long)retries, (unsigned long)g_cfg.max_retries,
                             (unsigned long)delay);
                    interruptible_sleep(job, delay);
                }
            }

            if (success) {
                last_fetch_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
                first_run     = false;
                if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
                    struct tm tm; time_t t = time(NULL); localtime_r(&t, &tm);
                    last_trigger_day = tm.tm_yday;
                }
            } else {
                sleep_ms(POLL_MS); /* brief pause before re-entering schedule loop */
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

DataFetcherConfig data_fetcher_default_config(void) {
    return (DataFetcherConfig){
        .max_retries     = DATA_FETCHER_MAX_RETRIES_DEFAULT,
        .retry_base_ms   = DATA_FETCHER_RETRY_BASE_MS_DEFAULT,
        .retry_max_ms    = DATA_FETCHER_RETRY_MAX_MS_DEFAULT,
        .task_stack_size = DATA_FETCHER_TASK_STACK_DEFAULT,
        .task_priority   = DATA_FETCHER_TASK_PRIORITY_DEFAULT,
    };
}

int data_fetcher_init(const DataFetcherConfig* config) {
    if (!config || !config->descriptors || config->descriptor_count == 0) {
        ESP_LOGE(TAG, "Invalid config");
        return -1;
    }
    if (g_initialized) {
        ESP_LOGW(TAG, "Already initialised");
        return 0;
    }

    g_cfg = *config;
    if (!g_cfg.task_stack_size) g_cfg.task_stack_size = DATA_FETCHER_TASK_STACK_DEFAULT;
    if (!g_cfg.task_priority)   g_cfg.task_priority   = DATA_FETCHER_TASK_PRIORITY_DEFAULT;

    g_jobs = calloc(config->descriptor_count, sizeof(FetchJob));
    if (!g_jobs) { ESP_LOGE(TAG, "OOM"); return -1; }
    g_job_count = config->descriptor_count;

    for (size_t i = 0; i < g_job_count; i++) {
        g_jobs[i].desc      = &config->descriptors[i];
        g_jobs[i].force_now = false;

        char name[16];
        snprintf(name, sizeof(name), "df_%s", g_jobs[i].desc->id);

        if (xTaskCreate(fetch_task, name, g_cfg.task_stack_size,
                        &g_jobs[i], g_cfg.task_priority, NULL) != pdPASS) {
            ESP_LOGE(TAG, "[%s] xTaskCreate failed", g_jobs[i].desc->id);
            free(g_jobs); g_jobs = NULL; g_job_count = 0;
            return -1;
        }

        if (g_jobs[i].desc->schedule.type == FETCH_SCHEDULE_INTERVAL) {
            ESP_LOGI(TAG, "[%s] interval %lu s%s → %s",
                     g_jobs[i].desc->id,
                     (unsigned long)g_jobs[i].desc->schedule.interval_sec,
                     g_jobs[i].desc->fetch_on_startup ? " (startup)" : "",
                     g_jobs[i].desc->url);
        } else {
            ESP_LOGI(TAG, "[%s] daily %02u:00%s → %s",
                     g_jobs[i].desc->id,
                     (unsigned)g_jobs[i].desc->schedule.daily_hour,
                     g_jobs[i].desc->fetch_on_startup ? " (startup)" : "",
                     g_jobs[i].desc->url);
        }
    }

    g_initialized = true;
    ESP_LOGI(TAG, "Initialised — %zu task(s), retries=%lu, back-off %lu–%lu ms",
             g_job_count, (unsigned long)g_cfg.max_retries,
             (unsigned long)g_cfg.retry_base_ms, (unsigned long)g_cfg.retry_max_ms);
    return 0;
}

void data_fetcher_notify_wifi_state(WifiManagerState state) {
    bool now_ready = (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS);
    if (!g_dns_ready && now_ready)
        ESP_LOGI(TAG, "DNS gate open — %zu task(s) may unblock", g_job_count);
    else if (g_dns_ready && !now_ready)
        ESP_LOGW(TAG, "DNS gate closed — tasks will pause after current fetch");
    g_dns_ready = now_ready;
}

void data_fetcher_notify_time_sync(void) {
    if (!g_clock_ready) {
        g_clock_ready = true;
        ESP_LOGI(TAG, "Clock gate open — TLS handshakes now permitted");
    }
}

void data_fetcher_request_now(const char* descriptor_id) {
    if (!g_initialized || !descriptor_id) return;
    for (size_t i = 0; i < g_job_count; i++) {
        if (strcmp(g_jobs[i].desc->id, descriptor_id) == 0) {
            ESP_LOGI(TAG, "[%s] Force-fetch requested", descriptor_id);
            g_jobs[i].force_now = true;
            return;
        }
    }
    ESP_LOGW(TAG, "No job with id \"%s\"", descriptor_id);
}
