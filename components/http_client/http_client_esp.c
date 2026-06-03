#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "http_client.h"

#include <stdlib.h>
#include <string.h>

#define HTTP_CLIENT_DEFAULT_TIMEOUT_MS 10000
#define HTTP_CLIENT_DEFAULT_MAX_CONC 1 /* safe default for ESP32 heap */
#define HTTP_CLIENT_INITIAL_BUF_SIZE 1024
#define HTTP_CLIENT_MAX_BUF_SIZE (128 * 1024)

static const char* g_tag = "http_client";

static bool g_initialized                  = false;
static int g_default_timeout               = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
static HttpClientTlsConfig g_tls           = {0};
static SemaphoreHandle_t g_concurrency_sem = NULL;

static const esp_http_client_method_t K_METHOD_MAP[] = {
    [HTTP_CLIENT_METHOD_GET]    = HTTP_METHOD_GET,
    [HTTP_CLIENT_METHOD_POST]   = HTTP_METHOD_POST,
    [HTTP_CLIENT_METHOD_PUT]    = HTTP_METHOD_PUT,
    [HTTP_CLIENT_METHOD_PATCH]  = HTTP_METHOD_PATCH,
    [HTTP_CLIENT_METHOD_DELETE] = HTTP_METHOD_DELETE,
    [HTTP_CLIENT_METHOD_HEAD]   = HTTP_METHOD_HEAD,
};

static const char* g_k_method_str[] = {
    [HTTP_CLIENT_METHOD_GET] = "GET",       [HTTP_CLIENT_METHOD_POST] = "POST",
    [HTTP_CLIENT_METHOD_PUT] = "PUT",       [HTTP_CLIENT_METHOD_PATCH] = "PATCH",
    [HTTP_CLIENT_METHOD_DELETE] = "DELETE", [HTTP_CLIENT_METHOD_HEAD] = "HEAD",
};

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
    bool oom;
} RxBuf;

/*
 * The public header forward-declares HttpClientAsyncHandle as an incomplete
 * type. The full layout is only ever seen by this translation unit.
 */
struct HttpClientAsyncHandle {
    char* url_copy;
    char* body_copy;
    HttpClientHeader* headers_copy;
    HttpClientRequest req_copy;

    HttpClientDoneCb cb;
    void* user_ctx;
    esp_http_client_handle_t client;
    bool done;
    RxBuf rx;
};

/* ---------------------------------------------------------------------------
 * Concurrency helpers
 * ------------------------------------------------------------------------- */

/** Acquire one slot. Blocks until a slot is available. */
static void concurrency_acquire(void) { xSemaphoreTake(g_concurrency_sem, portMAX_DELAY); }

/** Release one slot. Always safe to call after a matching acquire. */
static void concurrency_release(void) { xSemaphoreGive(g_concurrency_sem); }

/* ---------------------------------------------------------------------------
 * Module lifecycle
 * ------------------------------------------------------------------------- */

int http_client_init(const HttpClientConfig* config) {
    if (g_initialized) {
        ESP_LOGW(g_tag, "Already initialized");
        return 0;
    }

    int slots = HTTP_CLIENT_DEFAULT_MAX_CONC;

    if (config) {
        if (config->default_timeout_ms > 0) {
            g_default_timeout = config->default_timeout_ms;
        }
        g_tls = config->tls;
        if (config->max_concurrent_requests > 0) {
            slots = config->max_concurrent_requests;
        }
    }

    g_concurrency_sem = xSemaphoreCreateCounting(slots, slots);
    if (!g_concurrency_sem) {
        ESP_LOGE(g_tag, "Failed to create concurrency semaphore");
        return -1;
    }

    g_initialized = true;
    ESP_LOGI(g_tag, "Initialized (TLS verify: %s, max concurrent: %d)",
             g_tls.skip_verify ? "disabled" : "enabled", slots);
    return 0;
}

void http_client_deinit(void) {
    g_initialized     = false;
    g_default_timeout = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
    memset(&g_tls, 0, sizeof(g_tls));
    if (g_concurrency_sem) {
        vSemaphoreDelete(g_concurrency_sem);
        g_concurrency_sem = NULL;
    }
}

/* ---------------------------------------------------------------------------
 * Header helpers
 * ------------------------------------------------------------------------- */

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
 * RX buffer
 * ------------------------------------------------------------------------- */

static bool rxbuf_append(RxBuf* buf, const void* src, size_t n) {
    if (buf->oom) {
        return false;
    }
    if (buf->len + n > buf->cap) {
        size_t new_cap = buf->cap ? buf->cap * 2U : HTTP_CLIENT_INITIAL_BUF_SIZE;
        while (new_cap < buf->len + n) {
            new_cap *= 2U;
        }
        if (new_cap > (size_t)HTTP_CLIENT_MAX_BUF_SIZE) {
            ESP_LOGE(g_tag, "Response exceeds HTTP_CLIENT_MAX_BUF_SIZE");
            buf->oom = true;
            return false;
        }
        uint8_t* p = realloc(buf->data, new_cap);
        if (!p) {
            ESP_LOGE(g_tag, "RX buffer realloc failed");
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
 * Event handlers
 * ------------------------------------------------------------------------- */

static esp_err_t sync_event_handler(esp_http_client_event_t* evt) {
    RxBuf* rx = (RxBuf*)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && !esp_http_client_is_chunked_response(evt->client)) {
        rxbuf_append(rx, evt->data, (size_t)evt->data_len);
    }
    return ESP_OK;
}

static esp_err_t async_event_handler(esp_http_client_event_t* evt) {
    HttpClientAsyncHandle* h = (HttpClientAsyncHandle*)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA && !esp_http_client_is_chunked_response(evt->client)) {
        rxbuf_append(&h->rx, evt->data, (size_t)evt->data_len);
    }
    return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * Client builder
 * ------------------------------------------------------------------------- */

/* Concatenated root CAs — GTS R1, ISRG Root X1 (Let's Encrypt),
 * Amazon Root CA1, DigiCert Global Root CA.
 * mbedTLS tries each in order; add more roots to certs/roots.pem as needed.
 * Sources: pki.goog, letsencrypt.org, amazontrust.com, digicert.com */
extern const uint8_t G_ROOTS_START[] asm("_binary_roots_pem_start");
extern const uint8_t G_ROOTS_END[] asm("_binary_roots_pem_end");

/**
 * @brief Allocate and configure an esp_http_client for one request.
 *
 * TLS trust hierarchy (evaluated in order, first match wins):
 *   1. skip_verify == true  → no verification (development/testing only)
 *   2. tls->ca_cert != NULL → caller-supplied PEM; used as sole trust anchor
 *   3. default              → pinned GTS Root R1 (covers *.freeduck.dev and
 *                             all Google Trust Services signed certs)
 */
static esp_http_client_handle_t build_client(const HttpClientRequest* req,
                                             const HttpClientTlsConfig* tls, bool is_async,
                                             http_event_handle_cb event_handler, void* user_data) {
    esp_http_client_config_t cfg = {
        .url             = req->url,
        .method          = K_METHOD_MAP[req->method],
        .timeout_ms      = req->timeout_ms > 0 ? req->timeout_ms : g_default_timeout,
        .client_cert_pem = tls->client_cert,
        .client_key_pem  = tls->client_key,
        .event_handler   = event_handler,
        .user_data       = user_data,
        .is_async        = is_async,
    };

    if (tls->skip_verify) {
        ESP_LOGW(g_tag, "TLS verification DISABLED for %s — not for production", req->url);
        cfg.skip_cert_common_name_check = true;
    } else if (tls->ca_cert) {
        cfg.cert_pem = tls->ca_cert;
    } else {
        cfg.cert_pem = (const char*)G_ROOTS_START;
    }

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(g_tag, "esp_http_client_init failed for %s", req->url);
        return NULL;
    }

    for (const HttpClientHeader* h = req->headers; h; h = h->next) {
        esp_http_client_set_header(client, h->key, h->value);
    }

    if (req->body) {
        size_t body_len = req->body_len > 0 ? req->body_len : strlen(req->body);
        esp_http_client_set_post_field(client, req->body, (int)body_len);
    }

    return client;
}

/* ---------------------------------------------------------------------------
 * Synchronous API
 * ------------------------------------------------------------------------- */

int http_client_perform(const HttpClientRequest* req, HttpClientResponse* resp) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return -1;
    }
    if (!req || !req->url || !resp) {
        return -1;
    }
    memset(resp, 0, sizeof(*resp));

    HttpClientTlsConfig tls = resolve_tls(&req->tls);

    // Acquire FIRST — before any TLS allocation
    concurrency_acquire();

    RxBuf rx                        = {0};
    esp_http_client_handle_t client = build_client(req, &tls, false, sync_event_handler, &rx);
    if (!client) {
        ESP_LOGE(g_tag, "Failed to init HTTP client");
        concurrency_release(); // must release on this error path too
        return -1;
    }

    int ret       = -1;
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        resp->status = esp_http_client_get_status_code(client);
        resp->buffer = rx.data;
        resp->length = rx.len;
        ESP_LOGI(g_tag, "%s %s -> %d (%zu bytes)", g_k_method_str[req->method], req->url,
                 resp->status, resp->length);
        ret = 0;
    } else {
        ESP_LOGE(g_tag, "%s %s failed: %s", g_k_method_str[req->method], req->url,
                 esp_err_to_name(err));
        free(rx.data);
    }

    esp_http_client_cleanup(client);
    concurrency_release();
    return ret;
}

/* ---------------------------------------------------------------------------
 * Async API
 * ------------------------------------------------------------------------- */

static HttpClientHeader* headers_clone(const HttpClientHeader* src) {
    HttpClientHeader* head = NULL;
    for (const HttpClientHeader* h = src; h; h = h->next) {
        if (http_client_header_append(&head, h->key, h->value) != 0) {
            http_client_headers_free(head);
            return NULL;
        }
    }
    return head;
}

static void handle_destroy(HttpClientAsyncHandle* h) {
    if (h->client) {
        esp_http_client_cleanup(h->client);
    }
    free(h->rx.data);
    http_client_headers_free(h->headers_copy);
    free(h->body_copy);
    free(h->url_copy);
    free(h);
}

HttpClientAsyncHandle* http_client_async_begin(const HttpClientRequest* req, HttpClientDoneCb cb,
                                               void* user_ctx) {
    if (!g_initialized) {
        ESP_LOGE(g_tag, "Not initialized");
        return NULL;
    }
    if (!req || !req->url || !cb) {
        return NULL;
    }

    HttpClientAsyncHandle* h = calloc(1, sizeof(HttpClientAsyncHandle));
    if (!h) {
        return NULL;
    }

    h->url_copy = strdup(req->url);
    if (!h->url_copy) {
        goto err;
    }

    if (req->body) {
        h->body_copy = strdup(req->body);
        if (!h->body_copy) {
            goto err;
        }
    }

    if (req->headers) {
        h->headers_copy = headers_clone(req->headers);
        if (!h->headers_copy) {
            goto err;
        }
    }

    h->req_copy         = *req;
    h->req_copy.url     = h->url_copy;
    h->req_copy.body    = h->body_copy;
    h->req_copy.headers = h->headers_copy;
    h->cb               = cb;
    h->user_ctx         = user_ctx;

    HttpClientTlsConfig tls = resolve_tls(&req->tls);

    concurrency_acquire();

    h->client = build_client(&h->req_copy, &tls, true, async_event_handler, h);
    if (!h->client) {
        ESP_LOGE(g_tag, "Failed to init HTTP client");
        concurrency_release();
        goto err;
    }

    return h;

err:
    handle_destroy(h);
    return NULL;
}

HttpClientPollResult http_client_async_poll(HttpClientAsyncHandle* h) {
    if (!h || (int)h->done) {
        return HTTP_CLIENT_POLL_DONE;
    }

    esp_err_t err = esp_http_client_perform(h->client);

    if (err == ESP_ERR_HTTP_EAGAIN) {
        return HTTP_CLIENT_POLL_BUSY;
    }

    /* Mark done before firing the callback to prevent double-processing. */
    h->done = true;

    if (err == ESP_OK) {
        HttpClientResponse* resp = calloc(1, sizeof(HttpClientResponse));
        if (!resp) {
            concurrency_release();
            h->cb(NULL, -1, h->user_ctx);
            return HTTP_CLIENT_POLL_DONE;
        }

        resp->status = esp_http_client_get_status_code(h->client);
        resp->buffer = h->rx.data;
        resp->length = h->rx.len;
        h->rx.data   = NULL;

        ESP_LOGI(g_tag, "%s %s -> %d (%zu bytes)", g_k_method_str[h->req_copy.method], h->url_copy,
                 resp->status, resp->length);

        concurrency_release();
        h->cb(resp, 0, h->user_ctx);
    } else {
        ESP_LOGE(g_tag, "%s %s connection failed: %s", g_k_method_str[h->req_copy.method],
                 h->url_copy, esp_err_to_name(err));

        concurrency_release();
        h->cb(NULL, -1, h->user_ctx);
    }

    return HTTP_CLIENT_POLL_DONE;
}

void http_client_async_free(HttpClientAsyncHandle* h) {
    if (!h) {
        return;
    }
    handle_destroy(h);
}
