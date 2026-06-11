/**
 * @file data_fetcher.c
 * @brief Generic scheduled HTTP data fetcher — FreeRTOS task-based implementation.
 *
 * Startup behaviour (fetch_on_startup = true):
 *   After both gates open (DNS + clock), the cache is checked first.
 *   - Cache hit (fresh):  on_cached is fired immediately, no network request.
 *   - Cache miss/expired: fetch now, then cache and fire on_cached.
 *   This means a reboot with fresh cached data costs zero network traffic.
 *
 * URL + cache-key resolution:
 *   If FetchDescriptor.build_url is NULL the static .url and .cache_key fields
 *   are used directly.
 *   If build_url is set it is called just before every fetch (and before cache
 *   checks on startup). It writes both the URL and the cache key into the
 *   caller-supplied buffers and returns the URL pointer, or NULL to defer.
 *   Because the callback reads live settings on every invocation, a city or
 *   price-zone change takes effect on the very next fetch — no restart or
 *   manual invalidation required.
 *
 * Scheduling after startup:
 *   INTERVAL jobs sleep for interval_sec after each successful fetch.
 *   DAILY jobs fire once per calendar day at the configured local hour.
 *   force_now (via data_fetcher_request_now) bypasses both.
 *
 * Gates:
 *   g_dns_ready  — set by data_fetcher_notify_wifi_state()
 *   g_clock_ready — set by data_fetcher_notify_time_sync()
 *   Both must be true before any TLS connection. The clock gate prevents
 *   MBEDTLS_ERR_X509_CERT_VERIFY_FAILED (-0x2700) from an unsynced clock,
 *   which otherwise fragments heap and cascades into -0x008D on all subsequent
 *   connections.
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

static const char* g_tag = "data_fetcher";

#define POLL_MS 2000U

/* ---------------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------------- */

typedef struct {
    const FetchDescriptor* desc;
    volatile bool force_now;

    /*
     * Resolved URL and cache key from the most recent build_url call.
     * For static descriptors these point directly at desc->url / desc->cache_key
     * and are never written. For dynamic descriptors they point at the
     * descriptor's url_buf / cache_key_buf after each successful resolution.
     * Using these in do_fetch / serve_from_cache_if_fresh ensures URL and key
     * are always in sync and always reflect the current settings.
     */
    const char* resolved_url;
    const char* resolved_key;
} FetchJob;

/* ---------------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------------- */

static bool g_initialized          = false;
static DataFetcherConfig g_cfg     = {0};
static volatile bool g_dns_ready   = false;
static volatile bool g_clock_ready = false;
static FetchJob* g_jobs            = NULL;
static size_t g_job_count          = 0;

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static void sleep_ms(uint32_t ms) {
    if (ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(ms));
    }
}

/**
 * Sleep total_ms in POLL_MS chunks.
 * Returns true if force_now fired during sleep.
 * Returns false if DNS was lost during sleep.
 */
static bool interruptible_sleep(FetchJob* job, uint32_t total_ms) {
    uint32_t remaining = total_ms;
    while (remaining > 0) {
        uint32_t chunk = remaining < POLL_MS ? remaining : POLL_MS;
        sleep_ms(chunk);
        remaining -= chunk;
        if (job->force_now) {
            return true;
        }
        if (!g_dns_ready) {
            return false;
        }
    }
    return false;
}

static uint32_t compute_backoff_ms(uint32_t retry_index) {
    uint32_t delay = g_cfg.retry_base_ms;
    for (uint32_t i = 0; i < retry_index; i++) {
        delay *= 2U;
        if (delay >= g_cfg.retry_max_ms) {
            return g_cfg.retry_max_ms;
        }
    }
    return delay;
}

/**
 * Block until both DNS and clock are ready.
 * Logs once per condition so the reason for waiting is always clear.
 */
static void wait_for_ready(const char* id) {
    bool logged_dns = false;
    bool logged_clk = false;
    while (!g_dns_ready || !g_clock_ready) {
        if (!g_dns_ready && !logged_dns) {
            ESP_LOGW(g_tag, "[%s] Waiting for DNS...", id);
            logged_dns = true;
        }
        if ((int)g_dns_ready && !g_clock_ready && !logged_clk) {
            ESP_LOGW(g_tag, "[%s] DNS ready, waiting for clock sync...", id);
            logged_clk = true;
        }
        sleep_ms(POLL_MS);
    }
    if ((int)logged_dns || (int)logged_clk) {
        ESP_LOGI(g_tag, "[%s] Ready (DNS + clock)", id);
    }
}

/** Returns seconds until the next occurrence of target_hour (always > 0). */
static uint32_t secs_until_daily_hour(uint8_t target_hour) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    int diff = ((int)target_hour * 3600) - (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
    if (diff <= 0) {
        diff += 24 * 3600;
    }
    return (uint32_t)diff;
}

/* ---------------------------------------------------------------------------
 * URL + cache-key resolution
 *
 * For dynamic descriptors, calls build_url() which writes both the URL and
 * the cache key into their respective buffers. Stores the results in the job
 * so they remain consistent for the duration of one fetch window.
 *
 * Returns true if resolution succeeded (job->resolved_url and
 * job->resolved_key are valid). Returns false if the descriptor is not ready
 * yet (build_url returned NULL) — callers should sleep and retry.
 * ------------------------------------------------------------------------- */

static bool resolve(FetchJob* job) {
    const FetchDescriptor* d = job->desc;

    if (!d->build_url) {
        /* Static descriptor — pointers never change. */
        job->resolved_url = d->url;
        job->resolved_key = d->cache_key;
        return true;
    }

    const char* url = d->build_url(d, d->url_buf, d->url_buf_size, d->cache_key_buf,
                                   d->cache_key_buf_size, d->build_url_ctx);
    if (!url) {
        return false; /* not ready yet */
    }

    job->resolved_url = d->url_buf;
    job->resolved_key = d->cache_key_buf;
    return true;
}

/* ---------------------------------------------------------------------------
 * Cache-first startup check
 *
 * Returns true if a fresh (non-expired) cache entry exists and on_cached was
 * fired. Returns false if the cache is missing, expired, or the URL is not
 * yet ready (caller must fetch or defer).
 * ------------------------------------------------------------------------- */

static bool serve_from_cache_if_fresh(FetchJob* job) {
    if (!resolve(job)) {
        return false; /* not ready yet */
    }

    void* data = NULL;
    size_t len = 0;
    int rc     = cache_get_alloc(job->resolved_key, &data, &len);

    if (rc == CACHE_OK) {
        ESP_LOGI(g_tag, "[%s] Cache hit — %zu B fresh, skipping network fetch", job->desc->id, len);
        if (g_cfg.on_cached) {
            g_cfg.on_cached(job->desc, g_cfg.on_cached_ctx);
        }
        cache_free(data);
        return true;
    }

    if (rc == CACHE_ERR_EXPIRED) {
        ESP_LOGI(g_tag, "[%s] Cache expired — will fetch", job->desc->id);
    } else {
        ESP_LOGI(g_tag, "[%s] No cache entry — will fetch", job->desc->id);
    }
    return false;
}

/* ---------------------------------------------------------------------------
 * Fetch + cache
 *
 * Returns false on network/HTTP error (triggers retry) or if URL not ready.
 * Returns true on success OR deliberate discard by on_transform (no retry).
 * ------------------------------------------------------------------------- */

static bool do_fetch(FetchJob* job) {
    if (!resolve(job)) {
        ESP_LOGW(g_tag, "[%s] URL not ready — deferring fetch", job->desc->id);
        return false;
    }

    ESP_LOGI(g_tag, "[%s] Fetching %s", job->desc->id, job->resolved_url);

    HttpClientRequest req   = {.url = job->resolved_url, .method = HTTP_CLIENT_METHOD_GET};
    HttpClientResponse resp = {0};

    if (http_client_perform(&req, &resp) != 0) {
        ESP_LOGE(g_tag, "[%s] http_client_perform failed", job->desc->id);
        return false;
    }
    if (resp.status < 200 || resp.status >= 300) {
        ESP_LOGE(g_tag, "[%s] HTTP %d", job->desc->id, resp.status);
        free(resp.buffer);
        return false;
    }
    if (!resp.buffer || resp.length == 0U) {
        ESP_LOGW(g_tag, "[%s] HTTP %d empty body", job->desc->id, resp.status);
        free(resp.buffer);
        return false;
    }

    uint8_t* buf = resp.buffer;
    size_t len   = resp.length;

    if (g_cfg.on_transform) {
        if (!g_cfg.on_transform(job->desc, &buf, &len, g_cfg.on_transform_ctx)) {
            ESP_LOGI(g_tag, "[%s] Transform discarded response", job->desc->id);
            free(buf);
            return true; /* discard = success, no retry */
        }
    }

    int rc = cache_put(job->resolved_key, buf, len, job->desc->cache_ttl_sec);
    free(buf);

    if (rc != CACHE_OK) {
        ESP_LOGE(g_tag, "[%s] cache_put failed (rc=%d)", job->desc->id, rc);
    } else {
        ESP_LOGI(g_tag, "[%s] Cached %zu B (TTL %lu s)", job->desc->id, len,
                 (unsigned long)job->desc->cache_ttl_sec);
        if (g_cfg.on_cached) {
            g_cfg.on_cached(job->desc, g_cfg.on_cached_ctx);
        }
    }

    return true;
}

/* ---------------------------------------------------------------------------
 * Per-job task
 * ------------------------------------------------------------------------- */

static void fetch_task(void* arg) {
    FetchJob* job            = (FetchJob*)arg;
    const FetchDescriptor* d = job->desc;

    bool first_run         = true;
    uint32_t last_fetch_ms = 0;
    int last_trigger_day   = -1;

    while (1) {
        /* 1. Gate: wait for DNS + synced clock. */
        wait_for_ready(d->id);

        /* 2. Startup: serve from cache if fresh, fetch only if stale/missing.
         *    If the URL isn't ready yet both helpers return false — we sleep
         *    POLL_MS and retry the entire startup block. */
        if ((int)first_run && (int)d->fetch_on_startup) {
            if (serve_from_cache_if_fresh(job)) {
                last_fetch_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
                first_run     = false;
                goto schedule_loop;
            }

            /* If URL not ready yet, spin here until it is. */
            if (d->build_url && !resolve(job)) {
                sleep_ms(POLL_MS);
                continue; /* back to top: re-check gates, then retry startup */
            }

            goto do_fetch_now;
        }

    schedule_loop:
        /* 3. Wait until scheduled or force_now. */
        while (!job->force_now) {
            if (!g_dns_ready) {
                break;
            }

            if (d->schedule.type == FETCH_SCHEDULE_INTERVAL) {
                uint32_t elapsed =
                    (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - last_fetch_ms;
                uint32_t interval_ms = d->schedule.interval_sec * 1000U;
                if ((int)first_run || elapsed >= interval_ms) {
                    break;
                }
                uint32_t wait = interval_ms - elapsed;
                interruptible_sleep(job, wait < POLL_MS ? wait : POLL_MS);

            } else if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
                struct tm tm;
                time_t t = time(NULL);
                localtime_r(&t, &tm);
                if (tm.tm_hour == (int)d->schedule.daily_hour && tm.tm_yday != last_trigger_day) {
                    break;
                }
                uint32_t secs = secs_until_daily_hour(d->schedule.daily_hour);
                interruptible_sleep(job, secs * 1000U < 60000U ? secs * 1000U : 60000U);

            } else {
                sleep_ms(POLL_MS);
            }
        }

        if (!g_dns_ready) {
            continue;
        }

    do_fetch_now:
        job->force_now = false;

        /* 4. Fetch with exponential back-off. */
        {
            bool success     = false;
            uint32_t retries = 0;

            while (!success) {
                if (!g_dns_ready) {
                    ESP_LOGW(g_tag, "[%s] DNS lost — aborting fetch window", d->id);
                    break;
                }
                success = do_fetch(job);
                if (!success) {
                    if (++retries > g_cfg.max_retries) {
                        ESP_LOGE(g_tag, "[%s] Exhausted %lu retries", d->id,
                                 (unsigned long)g_cfg.max_retries);
                        break;
                    }
                    uint32_t delay = compute_backoff_ms(retries - 1U);
                    ESP_LOGW(g_tag, "[%s] Retry %lu/%lu in %lu ms", d->id, (unsigned long)retries,
                             (unsigned long)g_cfg.max_retries, (unsigned long)delay);
                    interruptible_sleep(job, delay);
                }
            }

            if (success) {
                last_fetch_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
                first_run     = false;
                if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
                    struct tm tm;
                    time_t t = time(NULL);
                    localtime_r(&t, &tm);
                    last_trigger_day = tm.tm_yday;
                }
            } else {
                sleep_ms(POLL_MS);
            }
        }

        goto schedule_loop;
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
        ESP_LOGE(g_tag, "Invalid config");
        return -1;
    }
    if (g_initialized) {
        ESP_LOGW(g_tag, "Already initialised");
        return 0;
    }

    g_cfg = *config;
    if (!g_cfg.task_stack_size) {
        g_cfg.task_stack_size = DATA_FETCHER_TASK_STACK_DEFAULT;
    }
    if (!g_cfg.task_priority) {
        g_cfg.task_priority = DATA_FETCHER_TASK_PRIORITY_DEFAULT;
    }

    g_jobs = calloc(config->descriptor_count, sizeof(FetchJob));
    if (!g_jobs) {
        ESP_LOGE(g_tag, "OOM");
        return -1;
    }
    g_job_count = config->descriptor_count;

    for (size_t i = 0; i < g_job_count; i++) {
        g_jobs[i].desc         = &config->descriptors[i];
        g_jobs[i].force_now    = false;
        g_jobs[i].resolved_url = NULL;
        g_jobs[i].resolved_key = NULL;

        char name[16];
        snprintf(name, sizeof(name), "df_%s", g_jobs[i].desc->id);

        if (xTaskCreate(fetch_task, name, g_cfg.task_stack_size, &g_jobs[i], g_cfg.task_priority,
                        NULL) != pdPASS) {
            ESP_LOGE(g_tag, "[%s] xTaskCreate failed", g_jobs[i].desc->id);
            free(g_jobs);
            g_jobs      = NULL;
            g_job_count = 0;
            return -1;
        }

        if (g_jobs[i].desc->schedule.type == FETCH_SCHEDULE_INTERVAL) {
            ESP_LOGI(g_tag, "[%s] interval %lu s%s → %s", g_jobs[i].desc->id,
                     (unsigned long)g_jobs[i].desc->schedule.interval_sec,
                     g_jobs[i].desc->fetch_on_startup ? " (startup)" : "",
                     g_jobs[i].desc->build_url ? "<dynamic URL>" : g_jobs[i].desc->url);
        } else {
            ESP_LOGI(g_tag, "[%s] daily %02u:00%s → %s", g_jobs[i].desc->id,
                     (unsigned)g_jobs[i].desc->schedule.daily_hour,
                     g_jobs[i].desc->fetch_on_startup ? " (startup)" : "",
                     g_jobs[i].desc->build_url ? "<dynamic URL>" : g_jobs[i].desc->url);
        }
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Initialised — %zu task(s), retries=%lu, back-off %lu–%lu ms", g_job_count,
             (unsigned long)g_cfg.max_retries, (unsigned long)g_cfg.retry_base_ms,
             (unsigned long)g_cfg.retry_max_ms);
    return 0;
}

void data_fetcher_notify_wifi_state(WifiManagerState state) {
    bool now_ready = (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS);
    if (!g_dns_ready && (int)now_ready) {
        ESP_LOGI(g_tag, "DNS gate open — %zu task(s) may unblock", g_job_count);
    } else if ((int)g_dns_ready && !now_ready) {
        ESP_LOGW(g_tag, "DNS gate closed — tasks will pause after current fetch");
    }
    g_dns_ready = now_ready;
}

void data_fetcher_notify_time_sync(void) {
    if (!g_clock_ready) {
        g_clock_ready = true;
        ESP_LOGI(g_tag, "Clock gate open — TLS handshakes now permitted");
    }
}

void data_fetcher_request_now(const char* descriptor_id) {
    if (!g_initialized || !descriptor_id) {
        return;
    }
    for (size_t i = 0; i < g_job_count; i++) {
        if (strcmp(g_jobs[i].desc->id, descriptor_id) == 0) {
            ESP_LOGI(g_tag, "[%s] Force-fetch requested", descriptor_id);
            g_jobs[i].force_now = true;
            return;
        }
    }
    ESP_LOGW(g_tag, "No job with id \"%s\"", descriptor_id);
}
