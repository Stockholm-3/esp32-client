/**
 * @file http_client_esp.c
 * @brief ESP32 HTTP/HTTPS client — serialised request queue.
 *
 * A single "http_worker" FreeRTOS task owns every TLS session. Callers post
 * a WorkItem to g_req_queue and block on a per-item binary semaphore until
 * the worker finishes. Because only one item is live at a time, at most one
 * ~23 KB mbedTLS context exists on the heap simultaneously, preventing
 * MBEDTLS_ERR_SSL_ALLOC_FAILED when multiple tasks fetch concurrently.
 *
 * TLS trust (first match wins):
 *   1. skip_verify true  — no verification (dev only).
 *   2. ca_cert set       — caller-supplied PEM as sole trust anchor.
 *   3. default           — embedded roots.pem (EMBED_TXTFILES).
 *                          Update with: make update-certs
 *
 * URL encoding:
 *   req->url is percent-encoded transparently before being handed to
 *   esp_http_client. The scheme, host, path, and query string are preserved;
 *   only non-ASCII bytes and reserved characters inside query values are
 *   encoded. Callers never need to pre-encode URLs.
 */

#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_tls.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "http_client.h"

#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Tunables
 * ------------------------------------------------------------------------- */

#define HTTP_CLIENT_DEFAULT_TIMEOUT_MS 15000
#define HTTP_CLIENT_INITIAL_BUF_SIZE 2048
#define HTTP_CLIENT_MAX_BUF_SIZE (256 * 1024)
#define HTTP_CLIENT_QUEUE_DEPTH 8
#define HTTP_CLIENT_WORKER_STACK 6144
#define HTTP_CLIENT_WORKER_PRIO 5

/* ---------------------------------------------------------------------------
 * Internal types
 * ------------------------------------------------------------------------- */

static const char* g_tag = "http_client";

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
    bool oom;
} RxBuf;

typedef struct {
    /* request (deep copies owned by this struct) */
    char* url;
    HttpClientMethod method;
    HttpClientHeader* headers;
    char* body;
    size_t body_len;
    int timeout_ms;
    HttpClientTlsConfig tls;

    /* result (written by worker) */
    HttpClientResponse* resp;
    int err;

    /* synchronisation */
    SemaphoreHandle_t done_sem;
} WorkItem;

/* ---------------------------------------------------------------------------
 * Module globals
 * ------------------------------------------------------------------------- */

static bool g_initialized         = false;
static int g_default_timeout      = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
static HttpClientTlsConfig g_tls  = {0};
static QueueHandle_t g_req_queue  = NULL;
static TaskHandle_t g_worker_task = NULL;

static const char* const K_METHOD_STR[] = {
    [HTTP_CLIENT_METHOD_GET] = "GET",       [HTTP_CLIENT_METHOD_POST] = "POST",
    [HTTP_CLIENT_METHOD_PUT] = "PUT",       [HTTP_CLIENT_METHOD_PATCH] = "PATCH",
    [HTTP_CLIENT_METHOD_DELETE] = "DELETE", [HTTP_CLIENT_METHOD_HEAD] = "HEAD",
};

static const esp_http_client_method_t K_METHOD_MAP[] = {
    [HTTP_CLIENT_METHOD_GET]    = HTTP_METHOD_GET,
    [HTTP_CLIENT_METHOD_POST]   = HTTP_METHOD_POST,
    [HTTP_CLIENT_METHOD_PUT]    = HTTP_METHOD_PUT,
    [HTTP_CLIENT_METHOD_PATCH]  = HTTP_METHOD_PATCH,
    [HTTP_CLIENT_METHOD_DELETE] = HTTP_METHOD_DELETE,
    [HTTP_CLIENT_METHOD_HEAD]   = HTTP_METHOD_HEAD,
};

/* Embedded root CA bundle — built from certs/roots.pem via EMBED_TXTFILES.
 * Update with: make update-certs  (downloads GTS R1-R4, ISRG X1/X2,
 * Amazon CA1, DigiCert Global Root CA) */
extern const uint8_t G_ROOTS_PEM_START[] asm("_binary_roots_pem_start");
extern const uint8_t G_ROOTS_PEM_END[] asm("_binary_roots_pem_end");

/* ---------------------------------------------------------------------------
 * URL percent-encoding
 *
 * Encodes a full URL in-place by walking the string once:
 *   - Scheme, "://", host, path separators ("/"), "?", "&", "=" are treated
 *     as structural characters and passed through verbatim.
 *   - Everything else that is not an RFC 3986 unreserved character is encoded.
 *
 * This is deliberately simple: it does not parse the URL into components.
 * Instead it uses the heuristic that structural characters (:/?&#=@) appear
 * before the first non-ASCII byte in any real URL.  The only characters that
 * commonly appear un-encoded in query values and need escaping are non-ASCII
 * bytes (e.g. UTF-8 city names).
 *
 * Returns a heap-allocated encoded string that the caller must free(), or
 * NULL on allocation failure.
 * ------------------------------------------------------------------------- */

static char* url_encode_alloc(const char* src) {
    static const char HEX[] = "0123456789ABCDEF";

    /* First pass: calculate the required output size. */
    size_t out_len = 0;
    for (const unsigned char* p = (const unsigned char*)src; *p; ++p) {
        /* RFC 3986 unreserved + structural characters we never encode. */
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_' || *p == '.' || *p == '~' ||
            *p == ':' || *p == '/' || *p == '?' || *p == '#' ||
            *p == '[' || *p == ']' || *p == '@' ||
            *p == '!' || *p == '$' || *p == '&' || *p == '\'' ||
            *p == '(' || *p == ')' || *p == '*' || *p == '+' ||
            *p == ',' || *p == ';' || *p == '=') {
            out_len += 1;
        } else {
            out_len += 3; /* %XX */
        }
    }

    char* out = malloc(out_len + 1);
    if (!out) {
        return NULL;
    }

    /* Second pass: encode. */
    char* dst = out;
    for (const unsigned char* p = (const unsigned char*)src; *p; ++p) {
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
            (*p >= '0' && *p <= '9') ||
            *p == '-' || *p == '_' || *p == '.' || *p == '~' ||
            *p == ':' || *p == '/' || *p == '?' || *p == '#' ||
            *p == '[' || *p == ']' || *p == '@' ||
            *p == '!' || *p == '$' || *p == '&' || *p == '\'' ||
            *p == '(' || *p == ')' || *p == '*' || *p == '+' ||
            *p == ',' || *p == ';' || *p == '=') {
            *dst++ = (char)*p;
        } else {
            *dst++ = '%';
            *dst++ = HEX[*p >> 4];
            *dst++ = HEX[*p & 0x0F];
        }
    }
    *dst = '\0';
    return out;
}

/* ---------------------------------------------------------------------------
 * RX buffer
 * ------------------------------------------------------------------------- */

static bool rxbuf_append(RxBuf* buf, const void* src, size_t n) {
    if ((int)buf->oom || n == 0) {
        return (!buf->oom) != 0;
    }

    if (buf->len + n > buf->cap) {
        size_t new_cap = buf->cap ? buf->cap * 2U : HTTP_CLIENT_INITIAL_BUF_SIZE;
        while (new_cap < buf->len + n) {
            new_cap *= 2U;
        }

        if (new_cap > (size_t)HTTP_CLIENT_MAX_BUF_SIZE) {
            ESP_LOGE(g_tag, "Response exceeds max buf size (%d B)", HTTP_CLIENT_MAX_BUF_SIZE);
            buf->oom = true;
            return false;
        }
        uint8_t* p = realloc(buf->data, new_cap);
        if (!p) {
            ESP_LOGE(g_tag, "RX buffer realloc failed (need %zu B)", new_cap);
            buf->oom = true;
            return false;
        }
        buf->data = p;
        buf->cap  = new_cap;
    }
    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return true;
}

/* ---------------------------------------------------------------------------
 * TLS resolution
 * ------------------------------------------------------------------------- */

static HttpClientTlsConfig resolve_tls(const HttpClientTlsConfig* req_tls) {
    HttpClientTlsConfig tls = g_tls;
    if (req_tls->ca_cert) {
        tls.ca_cert = req_tls->ca_cert;
    }
    if (req_tls->client_cert) {
        tls.client_cert = req_tls->client_cert;
    }
    if (req_tls->client_key) {
        tls.client_key = req_tls->client_key;
    }
    if (req_tls->skip_verify) {
        tls.skip_verify = true;
    }
    return tls;
}

/* ---------------------------------------------------------------------------
 * Event handler
 * ------------------------------------------------------------------------- */

static esp_err_t on_http_event(esp_http_client_event_t* evt) {
    switch (evt->event_id) {

    case HTTP_EVENT_ON_DATA:
        if (evt->data && evt->data_len > 0) {
            rxbuf_append((RxBuf*)evt->user_data, evt->data, (size_t)evt->data_len);
        }
        break;

    case HTTP_EVENT_ERROR: {
        /*
         * evt->data in HTTP_EVENT_ERROR is the esp_tls_error_handle_t.
         * This is the correct pattern per the official Espressif example:
         *   esp-idf/examples/protocols/esp_http_client/main/esp_http_client_example.c
         *
         * tls_flags is a bitmask:
         *   0x01   BADCERT_EXPIRED      — certificate has expired
         *   0x02   BADCERT_REVOKED      — certificate revoked
         *   0x04   BADCERT_CN_MISMATCH  — CN does not match hostname
         *   0x08   BADCERT_NOT_TRUSTED  — not signed by any root in roots.pem
         *                                 -> run: make update-certs
         *   0x0200 BADCERT_FUTURE       — validity starts in the future
         *                                 -> clock not synced yet
         */
        int tls_code  = 0;
        int tls_flags = 0;
        esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &tls_code, &tls_flags);

        if (tls_code || tls_flags) {
            ESP_LOGE(g_tag, "TLS error: mbedtls_code=0x%04X  cert_verify_flags=0x%04X", tls_code,
                     tls_flags);
            if (tls_flags & 0x01) {
                ESP_LOGE(g_tag, "  -> BADCERT_EXPIRED");
            }
            if (tls_flags & 0x02) {
                ESP_LOGE(g_tag, "  -> BADCERT_REVOKED");
            }
            if (tls_flags & 0x04) {
                ESP_LOGE(g_tag, "  -> BADCERT_CN_MISMATCH");
            }
            if (tls_flags & 0x08) {
                ESP_LOGE(g_tag,
                         "  -> BADCERT_NOT_TRUSTED (wrong root CA — run: make update-certs)");
            }
            if (tls_flags & 0x0200) {
                ESP_LOGE(g_tag, "  -> BADCERT_FUTURE (clock not synced)");
            }
        }
        break;
    }

    default:
        break;
    }
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Worker: execute one request
 * ------------------------------------------------------------------------- */

static void execute_work_item(WorkItem* item) {
    const HttpClientTlsConfig* tls = &item->tls;
    RxBuf rx                       = {0};

    ESP_LOGI(g_tag, ">> %s %s  (heap total: %lu B  internal: %lu B)", K_METHOD_STR[item->method],
             item->url, (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    esp_http_client_config_t cfg = {
        .url               = item->url,
        .method            = K_METHOD_MAP[item->method],
        .timeout_ms        = item->timeout_ms > 0 ? item->timeout_ms : g_default_timeout,
        .client_cert_pem   = tls->client_cert,
        .client_key_pem    = tls->client_key,
        .event_handler     = on_http_event,
        .user_data         = &rx,
        .is_async          = false,
        .keep_alive_enable = false,
    };

    if (tls->skip_verify) {
        ESP_LOGW(g_tag, "TLS verification DISABLED for %s", item->url);
        cfg.skip_cert_common_name_check = true;
    } else if (tls->ca_cert) {
        cfg.cert_pem = tls->ca_cert;
    } else {
        cfg.cert_pem = (const char*)G_ROOTS_PEM_START;
    }

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(g_tag, "esp_http_client_init failed");
        item->err = -1;
        return;
    }

    for (const HttpClientHeader* h = item->headers; h; h = h->next) {
        esp_http_client_set_header(client, h->key, h->value);
    }

    if (item->body) {
        size_t blen = item->body_len > 0 ? item->body_len : strlen(item->body);
        esp_http_client_set_post_field(client, item->body, (int)blen);
    }

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK && !rx.oom) {
        HttpClientResponse* resp = calloc(1, sizeof(HttpClientResponse));
        if (!resp) {
            ESP_LOGE(g_tag, "OOM for HttpClientResponse");
            free(rx.data);
            item->err = -1;
        } else {
            resp->status = esp_http_client_get_status_code(client);
            resp->buffer = rx.data;
            resp->length = rx.len;
            item->resp   = resp;
            item->err    = 0;
            ESP_LOGI(g_tag, "%s %s -> %d (%zu B)", K_METHOD_STR[item->method], item->url,
                     resp->status, resp->length);
        }
    } else {
        ESP_LOGE(g_tag, "%s %s failed: %s", K_METHOD_STR[item->method], item->url,
                 rx.oom ? "response too large" : esp_err_to_name(err));
        free(rx.data);
        item->err = -1;
    }

    esp_http_client_cleanup(client);
}

/* ---------------------------------------------------------------------------
 * Worker task
 * ------------------------------------------------------------------------- */

static void http_worker_task(void* arg) {
    (void)arg;
    ESP_LOGI(g_tag, "Worker started");
    WorkItem* item = NULL;
    while (xQueueReceive(g_req_queue, &item, portMAX_DELAY) == pdTRUE) {
        execute_work_item(item);
        xSemaphoreGive(item->done_sem);
    }
    vTaskDelete(NULL);
}

/* ---------------------------------------------------------------------------
 * WorkItem helpers
 * ------------------------------------------------------------------------- */

static HttpClientHeader* clone_headers(const HttpClientHeader* src) {
    HttpClientHeader* head = NULL;
    for (const HttpClientHeader* h = src; h; h = h->next) {
        if (http_client_header_append(&head, h->key, h->value) != 0) {
            http_client_headers_free(head);
            return NULL;
        }
    }
    return head;
}

static WorkItem* work_item_create(const HttpClientRequest* req) {
    WorkItem* item = calloc(1, sizeof(WorkItem));
    if (!item) {
        return NULL;
    }

    /* Percent-encode the URL so callers never need to pre-encode it.
     * Non-ASCII bytes (e.g. UTF-8 city names like "malmö") would otherwise
     * cause esp_http_client / http_parser to reject the URL immediately. */
    item->url = url_encode_alloc(req->url);
    if (!item->url) {
        goto err;
    }

    if (req->body) {
        item->body = strdup(req->body);
        if (!item->body) {
            goto err;
        }
        item->body_len = req->body_len > 0 ? req->body_len : strlen(req->body);
    }

    if (req->headers) {
        item->headers = clone_headers(req->headers);
        if (!item->headers) {
            goto err;
        }
    }

    item->method     = req->method;
    item->timeout_ms = req->timeout_ms;
    item->tls        = resolve_tls(&req->tls);

    item->done_sem = xSemaphoreCreateBinary();
    if (!item->done_sem) {
        goto err;
    }

    return item;

err:
    free(item->url);
    free(item->body);
    http_client_headers_free(item->headers);
    free(item);
    return NULL;
}

static void work_item_destroy(WorkItem* item) {
    if (!item) {
        return;
    }
    if (item->done_sem) {
        vSemaphoreDelete(item->done_sem);
    }
    free(item->url);
    free(item->body);
    http_client_headers_free(item->headers);
    free(item);
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int http_client_init(const HttpClientConfig* config) {
    if (g_initialized) {
        ESP_LOGW(g_tag, "Already initialized");
        return 0;
    }
    if (config) {
        if (config->default_timeout_ms > 0) {
            g_default_timeout = config->default_timeout_ms;
        }
        g_tls = config->tls;
    }

    g_req_queue = xQueueCreate(HTTP_CLIENT_QUEUE_DEPTH, sizeof(WorkItem*));
    if (!g_req_queue) {
        ESP_LOGE(g_tag, "Failed to create request queue");
        return -1;
    }

    if (xTaskCreate(http_worker_task, "http_worker", HTTP_CLIENT_WORKER_STACK, NULL,
                    HTTP_CLIENT_WORKER_PRIO, &g_worker_task) != pdPASS) {
        ESP_LOGE(g_tag, "Failed to create worker task");
        vQueueDelete(g_req_queue);
        g_req_queue = NULL;
        return -1;
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Initialized (timeout: %d ms, TLS verify: %s)", g_default_timeout,
             g_tls.skip_verify ? "disabled" : "enabled");
    return 0;
}

void http_client_deinit(void) {
    if (g_worker_task) {
        vTaskDelete(g_worker_task);
        g_worker_task = NULL;
    }
    if (g_req_queue) {
        vQueueDelete(g_req_queue);
        g_req_queue = NULL;
    }
    g_initialized     = false;
    g_default_timeout = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
    memset(&g_tls, 0, sizeof(g_tls));
}

int http_client_header_append(HttpClientHeader** head, const char* key, const char* value) {
    HttpClientHeader* h = malloc(sizeof(HttpClientHeader));
    if (!h) {
        return -1;
    }
    h->key   = key;
    h->value = value;
    h->next  = NULL;
    if (!*head) {
        *head = h;
        return 0;
    }
    HttpClientHeader* tail = *head;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = h;
    return 0;
}

void http_client_headers_free(HttpClientHeader* head) {
    while (head) {
        HttpClientHeader* next = head->next;
        free(head);
        head = next;
    }
}

int http_client_perform(const HttpClientRequest* req, HttpClientResponse* resp) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return -1;
    }
    if (!req || !req->url || !resp) {
        return -1;
    }

    memset(resp, 0, sizeof(*resp));

    WorkItem* item = work_item_create(req);
    if (!item) {
        ESP_LOGE(g_tag, "OOM for WorkItem");
        return -1;
    }

    if (xQueueSend(g_req_queue, &item, portMAX_DELAY) != pdTRUE) {
        ESP_LOGE(g_tag, "Failed to enqueue request");
        work_item_destroy(item);
        return -1;
    }

    xSemaphoreTake(item->done_sem, portMAX_DELAY);

    int err = item->err;
    if (err == 0) {
        *resp = *item->resp;
        free(item->resp);
        item->resp = NULL;
    }

    work_item_destroy(item);
    return err;
}
