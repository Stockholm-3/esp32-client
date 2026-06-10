/**
 * @file http_client_linux.c
 * @brief Linux libcurl-backed implementation of the HTTP client.
 *
 * Matches the updated, strictly synchronous API defined in http_client.h.
 * Used for host-side simulation.
 */

#include "http_client.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Tunables & Constants
 * ------------------------------------------------------------------------- */

#define HTTP_CLIENT_DEFAULT_TIMEOUT_MS 15000
#define HTTP_CLIENT_INITIAL_BUF_SIZE 2048
#define HTTP_CLIENT_MAX_BUF_SIZE (256 * 1024)

static bool g_initialized        = false;
static int g_default_timeout     = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
static HttpClientTlsConfig g_tls = {0};

static const char* const K_METHOD_STR[] = {
    [HTTP_CLIENT_METHOD_GET] = "GET",       [HTTP_CLIENT_METHOD_POST] = "POST",
    [HTTP_CLIENT_METHOD_PUT] = "PUT",       [HTTP_CLIENT_METHOD_PATCH] = "PATCH",
    [HTTP_CLIENT_METHOD_DELETE] = "DELETE", [HTTP_CLIENT_METHOD_HEAD] = "HEAD",
};

/* ---------------------------------------------------------------------------
 * RX Buffer
 * ------------------------------------------------------------------------- */

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
    bool oom;
} RxBuf;

static size_t write_callback(void* src, size_t size, size_t nmemb, void* userp) {
    size_t n   = size * nmemb;
    RxBuf* buf = (RxBuf*)userp;

    if (buf->oom || n == 0)
        return 0;

    if (buf->len + n > (size_t)HTTP_CLIENT_MAX_BUF_SIZE) {
        fprintf(stderr, "[http_client] Response exceeds max buf size (%d B)\n",
                HTTP_CLIENT_MAX_BUF_SIZE);
        buf->oom = true;
        return 0;
    }

    if (buf->len + n > buf->cap) {
        size_t new_cap = buf->cap ? buf->cap * 2U : HTTP_CLIENT_INITIAL_BUF_SIZE;
        while (new_cap < buf->len + n)
            new_cap *= 2U;

        if (new_cap > (size_t)HTTP_CLIENT_MAX_BUF_SIZE) {
            new_cap = HTTP_CLIENT_MAX_BUF_SIZE;
        }

        uint8_t* p = realloc(buf->data, new_cap);
        if (!p) {
            fprintf(stderr, "[http_client] RX buffer realloc failed\n");
            buf->oom = true;
            return 0;
        }
        buf->data = p;
        buf->cap  = new_cap;
    }

    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return n;
}

/* ---------------------------------------------------------------------------
 * TLS Resolution
 * ------------------------------------------------------------------------- */

static HttpClientTlsConfig resolve_tls(const HttpClientTlsConfig* req_tls) {
    HttpClientTlsConfig tls = g_tls;
    if (req_tls->ca_cert)
        tls.ca_cert = req_tls->ca_cert;
    if (req_tls->client_cert)
        tls.client_cert = req_tls->client_cert;
    if (req_tls->client_key)
        tls.client_key = req_tls->client_key;
    if (req_tls->skip_verify)
        tls.skip_verify = true;
    return tls;
}

/* ---------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

int http_client_init(const HttpClientConfig* config) {
    if (g_initialized) {
        fprintf(stderr, "[http_client] Already initialized\n");
        return 0;
    }

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "[http_client] curl_global_init failed\n");
        return -1;
    }

    if (config) {
        if (config->default_timeout_ms > 0)
            g_default_timeout = config->default_timeout_ms;
        g_tls = config->tls;
    }

    g_initialized = true;
    printf("[http_client] Initialized (timeout: %d ms, TLS verify: %s)\n", g_default_timeout,
           g_tls.skip_verify ? "disabled" : "enabled");
    return 0;
}

void http_client_deinit(void) {
    if (!g_initialized)
        return;

    curl_global_cleanup();
    g_initialized     = false;
    g_default_timeout = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
    memset(&g_tls, 0, sizeof(g_tls));
}

int http_client_header_append(HttpClientHeader** head, const char* key, const char* value) {
    HttpClientHeader* h = malloc(sizeof(HttpClientHeader));
    if (!h)
        return -1;
    h->key   = key;
    h->value = value;
    h->next  = NULL;

    if (!*head) {
        *head = h;
        return 0;
    }
    HttpClientHeader* tail = *head;
    while (tail->next)
        tail = tail->next;
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
        fprintf(stderr, "[http_client] Not initialized\n");
        return -1;
    }
    if (!req || !req->url || !resp)
        return -1;

    memset(resp, 0, sizeof(*resp));

    RxBuf rx                = {0};
    HttpClientTlsConfig tls = resolve_tls(&req->tls);

    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[http_client] curl_easy_init failed\n");
        return -1;
    }

    int timeout = req->timeout_ms > 0 ? req->timeout_ms : g_default_timeout;

    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &rx);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout);

    /* TLS Options */
    if (tls.skip_verify) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
        if (tls.ca_cert) {
            curl_easy_setopt(curl, CURLOPT_CAINFO, tls.ca_cert);
        }
    }

    if (tls.client_cert)
        curl_easy_setopt(curl, CURLOPT_SSLCERT, tls.client_cert);
    if (tls.client_key)
        curl_easy_setopt(curl, CURLOPT_SSLKEY, tls.client_key);

    /* Method and Body setup */
    switch (req->method) {
    case HTTP_CLIENT_METHOD_POST:
    case HTTP_CLIENT_METHOD_PUT:
    case HTTP_CLIENT_METHOD_PATCH:
    case HTTP_CLIENT_METHOD_DELETE:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, K_METHOD_STR[req->method]);
        if (req->body) {
            size_t blen = req->body_len > 0 ? req->body_len : strlen(req->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)blen);
        }
        break;
    case HTTP_CLIENT_METHOD_HEAD:
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        break;
    case HTTP_CLIENT_METHOD_GET:
    default:
        break;
    }

    /* Process Custom Headers */
    struct curl_slist* curl_headers = NULL;
    for (const HttpClientHeader* h = req->headers; h; h = h->next) {
        char line[512];
        snprintf(line, sizeof(line), "%s: %s", h->key, h->value);
        curl_headers = curl_slist_append(curl_headers, line);
    }
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }

    /* Fire Request */
    CURLcode res = curl_easy_perform(curl);
    int ret      = -1;

    if (res == CURLE_OK && !rx.oom) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp->status = (int)status;
        resp->buffer = rx.data;
        resp->length = rx.len;
        printf("[http_client] %s %s -> %d (%zu B)\n", K_METHOD_STR[req->method], req->url,
               resp->status, resp->length);
        ret = 0;
    } else {
        fprintf(stderr, "[http_client] %s %s failed: %s\n", K_METHOD_STR[req->method], req->url,
                rx.oom ? "response too large" : curl_easy_strerror(res));
        free(rx.data);
    }

    if (curl_headers) {
        curl_slist_free_all(curl_headers);
    }
    curl_easy_cleanup(curl);
    return ret;
}
