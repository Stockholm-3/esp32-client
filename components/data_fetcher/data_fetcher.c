/**
 * @file data_fetcher.c
 * @brief Generic scheduled HTTP data fetcher — FreeRTOS task-based implementation.
 *
 * Each registered descriptor gets its own task that runs a simple fetch loop:
 *   1. Wait for DNS.
 *   2. Honour startup_delay_ms on first run.
 *   3. Perform a blocking HTTP GET.
 *   4. On success: optionally transform the payload, then cache it.
 *   5. On failure: exponential back-off up to max_retries, then sleep until
 *      the next scheduled window.
 *   6. Sleep until the next due time and repeat from step 1.
 *
 * DNS loss between steps is handled by re-checking g_dns_ready before each
 * network call and aborting the retry loop when connectivity drops.
 */

#include "data_fetcher.h"

#include "cache.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "http_client.h"
#include "wifi_manager.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* g_tag = "data_fetcher";

/** How often a task polls for DNS while waiting. */
#define DNS_POLL_MS 2000U

/** Sleep granularity when waiting for the next scheduled window. */
#define SCHEDULE_POLL_MS 5000U

/* ---------------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------------- */

typedef struct {
    const FetchDescriptor* desc;
    volatile bool force_now;
} FetchJob;

/* ---------------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------------- */

static bool g_initialized        = false;
static DataFetcherConfig g_cfg   = {0};
static volatile bool g_dns_ready = false;

static FetchJob* g_jobs   = NULL;
static size_t g_job_count = 0;

/* ---------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

static void sleep_ms(uint32_t ms) { vTaskDelay(pdMS_TO_TICKS(ms)); }

static uint32_t compute_backoff_ms(uint32_t retry_count) {
    uint32_t delay = g_cfg.retry_base_ms;
    for (uint32_t i = 0; i < retry_count; i++) {
        delay *= 2U;
        if (delay >= g_cfg.retry_max_ms) {
            return g_cfg.retry_max_ms;
        }
    }
    return delay;
}

/** Blocks until DNS is available, polling every DNS_POLL_MS. */
static void wait_for_dns(const char* id) {
    if ((bool)g_dns_ready) {
        return;
    }
    ESP_LOGW(g_tag, "[%s] Waiting for DNS", id);
    while (!(bool)g_dns_ready) {
        sleep_ms(DNS_POLL_MS);
    }
    ESP_LOGI(g_tag, "[%s] DNS available", id);
}

/**
 * Returns true when the job is due to fire.
 * On first run with fetch_on_startup, compares elapsed time since
 * dns_ready_at_ms against startup_delay_ms.
 */
static bool job_is_due(const FetchJob* job, bool first_run, uint32_t last_fetch_ms,
                       int last_trigger_day, uint32_t dns_ready_at_ms) {
    const FetchDescriptor* d = job->desc;

    if ((int)first_run && (int)d->fetch_on_startup) {
        return (now_ms() - dns_ready_at_ms) >= d->startup_delay_ms;
    }

    if (d->schedule.type == FETCH_SCHEDULE_INTERVAL) {
        if (first_run) {
            /* No startup fetch; defer until one full interval elapses. */
            return (now_ms() - dns_ready_at_ms) >= (d->schedule.interval_sec * 1000U);
        }
        return (now_ms() - last_fetch_ms) >= (d->schedule.interval_sec * 1000U);
    }

    if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
        struct tm tm;
        time_t t = time(NULL);
        localtime_r(&t, &tm);
        return ((tm.tm_hour == (int)d->schedule.daily_hour) && (tm.tm_yday != last_trigger_day)) !=
               0;
    }

    return false;
}

/* ---------------------------------------------------------------------------
 * Fetch execution
 * ------------------------------------------------------------------------- */

/**
 * Attempts one HTTP GET.  On success, runs the optional transform callback,
 * then caches the result.
 *
 * @return true on success (fetch + cache), false on any error.
 */
static bool do_fetch(const FetchDescriptor* desc) {
    ESP_LOGI(g_tag, "[%s] Fetching → %s", desc->id, desc->url);

    HttpClientRequest req = {0};
    req.url               = desc->url;
    req.method            = HTTP_CLIENT_METHOD_GET;

    HttpClientResponse resp = {0};
    if (http_client_perform(&req, &resp) != 0) {
        ESP_LOGE(g_tag, "[%s] http_client_perform failed", desc->id);
        return false;
    }

    if (resp.status < 200 || resp.status >= 300) {
        ESP_LOGE(g_tag, "[%s] HTTP %d", desc->id, resp.status);
        free(resp.buffer);
        return false;
    }

    if (resp.buffer == NULL || resp.length == 0U) {
        ESP_LOGW(g_tag, "[%s] HTTP %d with empty body", desc->id, resp.status);
        free(resp.buffer);
        return false;
    }

    uint8_t* buf = resp.buffer;
    size_t len   = resp.length;

    /* Optional transform — may replace buf/len or return false to discard. */
    if (g_cfg.on_transform) {
        bool keep = g_cfg.on_transform(desc, &buf, &len, g_cfg.on_transform_ctx);
        if (!keep) {
            ESP_LOGI(g_tag, "[%s] Transform discarded response", desc->id);
            free(buf);
            /* Still counts as a successful fetch — no retry. */
            return true;
        }
    }

    int rc = cache_put(desc->cache_key, buf, len, desc->cache_ttl_sec);
    free(buf);

    if (rc != CACHE_OK) {
        /* Cache failures are non-fatal; the data was valid. */
        ESP_LOGE(g_tag, "[%s] cache_put failed (rc=%d)", desc->id, rc);
    } else {
        ESP_LOGI(g_tag, "[%s] Cached %zu B (TTL %lu s) → \"%s\"", desc->id, len,
                 (unsigned long)desc->cache_ttl_sec, desc->cache_key);

        if (g_cfg.on_cached) {
            g_cfg.on_cached(desc, g_cfg.on_cached_ctx);
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

    bool first_run           = true;
    uint32_t last_fetch_ms   = 0;
    int last_trigger_day     = -1;
    uint32_t dns_ready_at_ms = 0;

    while (1) {
        wait_for_dns(d->id);

        /* Record when DNS first became available for startup-delay accounting. */
        if (dns_ready_at_ms == 0U) {
            dns_ready_at_ms = now_ms();
        }

        /* Wait until due or force-triggered. */
        while (!job_is_due(job, first_run, last_fetch_ms, last_trigger_day, dns_ready_at_ms) &&
               !(bool)job->force_now) {

            /* If DNS drops while we're waiting, reset the dns_ready timestamp so
             * the next reconnect restarts the startup-delay window cleanly. */
            if (!(bool)g_dns_ready) {
                dns_ready_at_ms = 0;
                wait_for_dns(d->id);
                dns_ready_at_ms = now_ms();
            }

            sleep_ms(SCHEDULE_POLL_MS);
        }

        job->force_now = false;

        /* Retry loop for this fetch window. */
        bool success     = false;
        uint32_t retries = 0;

        while (!success) {
            if (!(bool)g_dns_ready) {
                ESP_LOGW(g_tag, "[%s] DNS lost — aborting fetch window", d->id);
                break;
            }

            success = do_fetch(d);

            if (!success) {
                retries++;
                if (retries > g_cfg.max_retries) {
                    ESP_LOGE(g_tag, "[%s] Exhausted %lu retries — skipping window", d->id,
                             (unsigned long)g_cfg.max_retries);
                    break;
                }

                uint32_t delay = compute_backoff_ms(retries - 1U);
                ESP_LOGW(g_tag, "[%s] Retry %lu/%lu in %lu ms", d->id, (unsigned long)retries,
                         (unsigned long)g_cfg.max_retries, (unsigned long)delay);
                sleep_ms(delay);
            }
        }

        if (success) {
            last_fetch_ms = now_ms();
            first_run     = false;

            if (d->schedule.type == FETCH_SCHEDULE_DAILY) {
                struct tm tm;
                time_t t = time(NULL);
                localtime_r(&t, &tm);
                last_trigger_day = tm.tm_yday;
            }
        }

        /* For interval jobs: sleep until the next window is roughly due rather
         * than spinning immediately.  SCHEDULE_POLL_MS polling in the outer
         * loop handles the exact wake-up. */
        if (d->schedule.type == FETCH_SCHEDULE_INTERVAL && (int)success) {
            uint32_t sleep_target = (d->schedule.interval_sec * 1000U);
            /* Sleep in chunks so DNS loss is detected promptly. */
            uint32_t slept = 0;
            while (slept < sleep_target) {
                uint32_t chunk = sleep_target - slept;
                if (chunk > SCHEDULE_POLL_MS) {
                    chunk = SCHEDULE_POLL_MS;
                }
                sleep_ms(chunk);
                slept += chunk;
                if (!(bool)g_dns_ready) {
                    break;
                }
                if ((bool)job->force_now) {
                    break;
                }
            }
        } else {
            /* Daily jobs or failed windows: just let the outer loop re-check. */
            sleep_ms(SCHEDULE_POLL_MS);
        }
    }
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

DataFetcherConfig data_fetcher_default_config(void) {
    DataFetcherConfig cfg = {0};
    cfg.max_retries       = DATA_FETCHER_MAX_RETRIES_DEFAULT;
    cfg.retry_base_ms     = DATA_FETCHER_RETRY_BASE_MS_DEFAULT;
    cfg.retry_max_ms      = DATA_FETCHER_RETRY_MAX_MS_DEFAULT;
    cfg.task_stack_size   = DATA_FETCHER_TASK_STACK_DEFAULT;
    cfg.task_priority     = DATA_FETCHER_TASK_PRIORITY_DEFAULT;
    return cfg;
}

int data_fetcher_init(const DataFetcherConfig* config) {
    if (!config) {
        ESP_LOGE(g_tag, "data_fetcher_init: NULL config");
        return -1;
    }
    if (!config->descriptors || config->descriptor_count == 0) {
        ESP_LOGE(g_tag, "data_fetcher_init: no descriptors");
        return -1;
    }
    if (g_initialized) {
        ESP_LOGW(g_tag, "Already initialised — ignoring duplicate init");
        return 0;
    }

    g_cfg = *config;
    if (g_cfg.task_stack_size == 0U) {
        g_cfg.task_stack_size = DATA_FETCHER_TASK_STACK_DEFAULT;
    }
    if (g_cfg.task_priority == 0U) {
        g_cfg.task_priority = DATA_FETCHER_TASK_PRIORITY_DEFAULT;
    }

    g_jobs = (FetchJob*)calloc(config->descriptor_count, sizeof(FetchJob));
    if (!g_jobs) {
        ESP_LOGE(g_tag, "Out of memory for FetchJob array");
        return -1;
    }
    g_job_count = config->descriptor_count;

    for (size_t i = 0; i < g_job_count; i++) {
        FetchJob* job               = &g_jobs[i];
        const FetchDescriptor* desc = &config->descriptors[i];

        job->desc      = desc;
        job->force_now = false;

        /* Task name: "df_<id>", truncated to 15 chars (FreeRTOS limit). */
        char task_name[16];
        snprintf(task_name, sizeof(task_name), "df_%s", desc->id);

        BaseType_t rc = xTaskCreate(fetch_task, task_name, g_cfg.task_stack_size, job,
                                    g_cfg.task_priority, NULL);
        if (rc != pdPASS) {
            ESP_LOGE(g_tag, "[%s] xTaskCreate failed", desc->id);
            free(g_jobs);
            g_jobs      = NULL;
            g_job_count = 0;
            return -1;
        }

        if (desc->schedule.type == FETCH_SCHEDULE_INTERVAL) {
            ESP_LOGI(g_tag, "[%s] Registered — interval %lu s%s → %s", desc->id,
                     (unsigned long)desc->schedule.interval_sec,
                     desc->fetch_on_startup ? ", startup fetch" : "", desc->url);
        } else {
            ESP_LOGI(g_tag, "[%s] Registered — daily at %02u:00%s → %s", desc->id,
                     (unsigned)desc->schedule.daily_hour,
                     desc->fetch_on_startup ? ", startup fetch" : "", desc->url);
        }
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Initialised — %zu task(s), retries=%lu, back-off %lu–%lu ms", g_job_count,
             (unsigned long)g_cfg.max_retries, (unsigned long)g_cfg.retry_base_ms,
             (unsigned long)g_cfg.retry_max_ms);
    return 0;
}

void data_fetcher_notify_wifi_state(WifiManagerState state) {
    bool was_ready = (bool)g_dns_ready;
    bool now_ready = (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS);

    g_dns_ready = now_ready;

    if (!was_ready && (int)now_ready) {
        ESP_LOGI(g_tag, "DNS available — %zu fetch task(s) unblocked", g_job_count);
    } else if ((int)was_ready && !now_ready) {
        ESP_LOGW(g_tag, "DNS lost — fetch tasks will suspend after current attempt");
    }
}

void data_fetcher_request_now(const char* descriptor_id) {
    if (!g_initialized) {
        ESP_LOGW(g_tag, "data_fetcher_request_now: not initialised");
        return;
    }
    if (!descriptor_id) {
        ESP_LOGW(g_tag, "data_fetcher_request_now: NULL id");
        return;
    }

    for (size_t i = 0; i < g_job_count; i++) {
        FetchJob* job = &g_jobs[i];
        if (strcmp(job->desc->id, descriptor_id) != 0) {
            continue;
        }
        ESP_LOGI(g_tag, "[%s] Force-fetch requested", descriptor_id);
        job->force_now = true;
        return;
    }

    ESP_LOGW(g_tag, "data_fetcher_request_now: no job with id \"%s\"", descriptor_id);
}
