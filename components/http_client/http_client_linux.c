#include "http_client.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HTTP_CLIENT_DEFAULT_TIMEOUT_MS 10000
#define HTTP_CLIENT_MAX_BUF_SIZE (128 * 1024)

static bool g_initialized        = false;
static int g_default_timeout     = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
static HttpClientTlsConfig g_tls = {0};

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
} RxBuf;

struct HttpClientAsyncHandle {
    CURL* curl;
    CURLM* multi;
    RxBuf buf;
    HttpClientResponse resp;
    bool done;
    bool started;
    HttpClientDoneCb cb;
    void* user_ctx;
    HttpClientRequest req;
    char* url_copy;
    char* body_copy;
    HttpClientHeader* headers_copy;
    struct curl_slist* curl_headers;
};

static size_t write_callback(void* src, size_t size, size_t nmemb, void* userp) {
    size_t n   = size * nmemb;
    RxBuf* buf = (RxBuf*)userp;

    size_t available = HTTP_CLIENT_MAX_BUF_SIZE - buf->len;
    if (n > available) {
        fprintf(stderr, "[http_client] Response exceeds HTTP_CLIENT_MAX_BUF_SIZE, truncating\n");
        n = available;
    }

    if (buf->len + n > buf->cap) {
        size_t new_cap = buf->cap ? buf->cap * 2 : 1024;
        while (new_cap < buf->len + n) {
            new_cap *= 2;
        }
        uint8_t* p = realloc(buf->data, new_cap);
        if (!p) {
            fprintf(stderr, "[http_client] RX buffer realloc failed\n");
            return 0;
        }
        buf->data = p;
        buf->cap  = new_cap;
    }

    memcpy(buf->data + buf->len, src, n);
    buf->len += n;
    return size * nmemb;
}

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

static const char* k_method_str[] = {
    [HTTP_CLIENT_METHOD_GET] = "GET",       [HTTP_CLIENT_METHOD_POST] = "POST",
    [HTTP_CLIENT_METHOD_PUT] = "PUT",       [HTTP_CLIENT_METHOD_PATCH] = "PATCH",
    [HTTP_CLIENT_METHOD_DELETE] = "DELETE", [HTTP_CLIENT_METHOD_HEAD] = "HEAD",
};

static HttpClientTlsConfig resolve_tls(const HttpClientTlsConfig* req_tls) {
    HttpClientTlsConfig tls = g_tls;
    if (req_tls && req_tls->ca_cert) {
        tls.ca_cert = req_tls->ca_cert;
    }
    if (req_tls && req_tls->client_cert) {
        tls.client_cert = req_tls->client_cert;
    }
    if (req_tls && req_tls->client_key) {
        tls.client_key = req_tls->client_key;
    }
    if (req_tls && req_tls->skip_verify) {
        tls.skip_verify = true;
    }
    return tls;
}

int http_client_init(const HttpClientConfig* config) {
    if (g_initialized) {
        return 0;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fprintf(stderr, "[http_client] curl_global_init failed\n");
        return -1;
    }
    if (config) {
        if (config->default_timeout_ms > 0) {
            g_default_timeout = config->default_timeout_ms;
        }
        g_tls = config->tls;
    }
    g_initialized = true;
    printf("[http_client] Initialized (TLS verify: %s)\n",
           g_tls.skip_verify ? "disabled" : "enabled");
    return 0;
}

void http_client_deinit(void) {
    if (!g_initialized) {
        return;
    }
    curl_global_cleanup();
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
        fprintf(stderr, "[http_client] Not initialized — call http_client_init() first\n");
        return -1;
    }
    if (!req || !req->url || !resp) {
        return -1;
    }
    memset(resp, 0, sizeof(*resp));

    RxBuf buf               = {0};
    HttpClientTlsConfig tls = resolve_tls(&req->tls);

    CURL* curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    int timeout = req->timeout_ms > 0 ? req->timeout_ms : g_default_timeout;

    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout);

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
    if (tls.client_cert) {
        curl_easy_setopt(curl, CURLOPT_SSLCERT, tls.client_cert);
    }
    if (tls.client_key) {
        curl_easy_setopt(curl, CURLOPT_SSLKEY, tls.client_key);
    }

    switch (req->method) {
    case HTTP_CLIENT_METHOD_POST:
    case HTTP_CLIENT_METHOD_PUT:
    case HTTP_CLIENT_METHOD_PATCH:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, k_method_str[req->method]);
        if (req->body) {
            size_t len = req->body_len > 0 ? req->body_len : strlen(req->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req->body);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);
        }
        break;
    case HTTP_CLIENT_METHOD_DELETE:
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case HTTP_CLIENT_METHOD_HEAD:
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
        break;
    case HTTP_CLIENT_METHOD_GET:
    default:
        break;
    }

    struct curl_slist* curl_headers = NULL;
    for (const HttpClientHeader* h = req->headers; h; h = h->next) {
        char line[512];
        snprintf(line, sizeof(line), "%s: %s", h->key, h->value);
        curl_headers = curl_slist_append(curl_headers, line);
    }
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }

    CURLcode res = curl_easy_perform(curl);
    int ret      = -1;
    if (res == CURLE_OK) {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        resp->status = (int)status;
        resp->buffer = buf.data;
        resp->length = buf.len;
        printf("[http_client] %s %s -> %d (%zu bytes)\n", k_method_str[req->method], req->url,
               resp->status, resp->length);
        ret = 0;
    } else {
        fprintf(stderr, "[http_client] %s %s failed: %s\n", k_method_str[req->method], req->url,
                curl_easy_strerror(res));
        free(buf.data);
        resp->buffer = NULL;
        resp->length = 0;
        resp->status = 0;
    }

    if (curl_headers) {
        curl_slist_free_all(curl_headers);
    }
    curl_easy_cleanup(curl);
    return ret;
}

HttpClientAsyncHandle* http_client_async_begin(const HttpClientRequest* req, HttpClientDoneCb cb,
                                               void* user_ctx) {
    if (!g_initialized) {
        fprintf(stderr, "[http_client] Error: Not initialized. Call http_client_init() first.\n");
        return NULL;
    }
    if (!req) {
        fprintf(stderr, "[http_client] Error: Request object is NULL.\n");
        return NULL;
    }
    if (!req->url) {
        fprintf(stderr, "[http_client] Error: Request URL is NULL.\n");
        return NULL;
    }
    if (!cb) {
        fprintf(stderr, "[http_client] Error: Callback function is NULL.\n");
        return NULL;
    }

    HttpClientAsyncHandle* h = calloc(1, sizeof(HttpClientAsyncHandle));
    if (!h) {
        return NULL;
    }
    // ... (rest of your function)

    h->url_copy = strdup(req->url);
    if (!h->url_copy) {
        free(h);
        return NULL;
    }

    if (req->body) {
        h->body_copy = strdup(req->body);
        if (!h->body_copy) {
            free(h->url_copy);
            free(h);
            return NULL;
        }
    } else {
        h->body_copy = NULL;
    }

    if (req->headers) {
        h->headers_copy = headers_clone(req->headers);
        if (!h->headers_copy) {
            free(h->body_copy);
            free(h->url_copy);
            free(h);
            return NULL;
        }
    } else {
        h->headers_copy = NULL;
    }

    h->req         = *req;
    h->req.url     = h->url_copy;
    h->req.body    = h->body_copy;
    h->req.headers = h->headers_copy;
    h->cb          = cb;
    h->user_ctx    = user_ctx;

    h->curl = curl_easy_init();
    if (!h->curl) {
        http_client_async_free(h);
        return NULL;
    }

    h->multi = curl_multi_init();
    if (!h->multi) {
        http_client_async_free(h);
        return NULL;
    }

    int timeout = h->req.timeout_ms > 0 ? h->req.timeout_ms : g_default_timeout;
    curl_easy_setopt(h->curl, CURLOPT_URL, h->req.url);
    curl_easy_setopt(h->curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(h->curl, CURLOPT_WRITEDATA, &h->buf);
    curl_easy_setopt(h->curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(h->curl, CURLOPT_TIMEOUT_MS, (long)timeout);

    HttpClientTlsConfig tls = resolve_tls(&h->req.tls);
    if (tls.skip_verify) {
        curl_easy_setopt(h->curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(h->curl, CURLOPT_SSL_VERIFYHOST, 0L);
    } else {
        curl_easy_setopt(h->curl, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(h->curl, CURLOPT_SSL_VERIFYHOST, 2L);
        if (tls.ca_cert) {
            curl_easy_setopt(h->curl, CURLOPT_CAINFO, tls.ca_cert);
        }
    }
    if (tls.client_cert) {
        curl_easy_setopt(h->curl, CURLOPT_SSLCERT, tls.client_cert);
    }
    if (tls.client_key) {
        curl_easy_setopt(h->curl, CURLOPT_SSLKEY, tls.client_key);
    }

    switch (h->req.method) {
    case HTTP_CLIENT_METHOD_POST:
    case HTTP_CLIENT_METHOD_PUT:
    case HTTP_CLIENT_METHOD_PATCH:
        curl_easy_setopt(h->curl, CURLOPT_CUSTOMREQUEST, k_method_str[h->req.method]);
        if (h->req.body) {
            size_t len = h->req.body_len > 0 ? h->req.body_len : strlen(h->req.body);
            curl_easy_setopt(h->curl, CURLOPT_POSTFIELDS, h->req.body);
            curl_easy_setopt(h->curl, CURLOPT_POSTFIELDSIZE, (long)len);
        }
        break;
    case HTTP_CLIENT_METHOD_DELETE:
        curl_easy_setopt(h->curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        break;
    case HTTP_CLIENT_METHOD_HEAD:
        curl_easy_setopt(h->curl, CURLOPT_NOBODY, 1L);
        break;
    case HTTP_CLIENT_METHOD_GET:
    default:
        break;
    }

    struct curl_slist* curl_headers = NULL;
    for (const HttpClientHeader* iter = h->req.headers; iter; iter = iter->next) {
        char line[512];
        snprintf(line, sizeof(line), "%s: %s", iter->key, iter->value);
        curl_headers = curl_slist_append(curl_headers, line);
    }
    if (curl_headers) {
        curl_easy_setopt(h->curl, CURLOPT_HTTPHEADER, curl_headers);
        h->curl_headers = curl_headers;
    }

    curl_multi_add_handle(h->multi, h->curl);
    h->started = true;
    h->done    = false;

    return h;
}

HttpClientPollResult http_client_async_poll(HttpClientAsyncHandle* h) {
    if (!h) {
        return HTTP_CLIENT_POLL_DONE;
    }
    if (h->done) {
        return HTTP_CLIENT_POLL_DONE;
    }

    int still_running = 0;
    CURLMcode mc      = curl_multi_perform(h->multi, &still_running);

    if (mc != CURLM_OK) {
        h->done                 = true;
        HttpClientResponse resp = {0};
        if (h->cb) {
            h->cb(&resp, -1, h->user_ctx);
        }
        return HTTP_CLIENT_POLL_DONE;
    }

    if (still_running == 0) {
        h->done = true;

        int msgs_left;
        CURLMsg* msg;
        CURLcode res = CURLE_FAILED_INIT;

        while ((msg = curl_multi_info_read(h->multi, &msgs_left))) {
            if (msg->msg == CURLMSG_DONE) {
                res = msg->data.result;
                break;
            }
        }

        HttpClientResponse* resp = calloc(1, sizeof(HttpClientResponse));
        if (!resp) {
            return HTTP_CLIENT_POLL_DONE;
        }

        if (res == CURLE_OK) {
            long status = 0;
            curl_easy_getinfo(h->curl, CURLINFO_RESPONSE_CODE, &status);
            resp->status = (int)status;
            resp->buffer = h->buf.data;
            resp->length = h->buf.len;

            printf("[http_client] Async %s %s -> %d (%zu bytes)\n", k_method_str[h->req.method],
                   h->req.url, resp->status, resp->length);

            if (h->cb) {
                h->cb(resp, 0, h->user_ctx);
            }
        } else {
            fprintf(stderr, "[http_client] Async %s %s failed: %s\n", k_method_str[h->req.method],
                    h->req.url, curl_easy_strerror(res));

            free(h->buf.data);
            resp->buffer = NULL;
            resp->length = 0;
            resp->status = 0;

            if (h->cb) {
                h->cb(resp, -1, h->user_ctx);
            }
        }
        return HTTP_CLIENT_POLL_DONE;
    }

    return HTTP_CLIENT_POLL_BUSY;
}

void http_client_async_free(HttpClientAsyncHandle* h) {
    if (!h) {
        return;
    }
    if (h->multi) {
        if (h->curl) {
            curl_multi_remove_handle(h->multi, h->curl);
            curl_easy_cleanup(h->curl);
        }
        curl_multi_cleanup(h->multi);
    }
    if (h->curl_headers) {
        curl_slist_free_all(h->curl_headers);
    }
    free(h->url_copy);
    free(h->body_copy);
    if (h->headers_copy) {
        http_client_headers_free(h->headers_copy);
    }
    free(h);
}

int http_client_perform_async(const HttpClientRequest* req, HttpClientDoneCb cb, void* user_ctx) {
    HttpClientAsyncHandle* h = http_client_async_begin(req, cb, user_ctx);
    if (!h) {
        return -1;
    }
    return 0;
}
