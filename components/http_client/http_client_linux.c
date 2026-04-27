#include "http_client.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * TLS note: on this stub, HttpClientTlsConfig fields are treated as file
 * paths rather than PEM strings, because libcurl works with cert files.
 * On the ESP target they are PEM strings passed directly to the TLS stack.
 */

#define HTTP_CLIENT_DEFAULT_TIMEOUT_MS 10000
#define HTTP_CLIENT_MAX_BUF_SIZE (128 * 1024)

static bool g_initialized        = false;
static int g_default_timeout     = HTTP_CLIENT_DEFAULT_TIMEOUT_MS;
static HttpClientTlsConfig g_tls = {0};

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

typedef struct {
    uint8_t* data;
    size_t len;
    size_t cap;
} RxBuf;

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

static const char* k_method_str[] = {
    [HTTP_CLIENT_METHOD_GET] = "GET",       [HTTP_CLIENT_METHOD_POST] = "POST",
    [HTTP_CLIENT_METHOD_PUT] = "PUT",       [HTTP_CLIENT_METHOD_PATCH] = "PATCH",
    [HTTP_CLIENT_METHOD_DELETE] = "DELETE", [HTTP_CLIENT_METHOD_HEAD] = "HEAD",
};

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

static int do_perform(const HttpClientRequest* req, HttpClientResponse* resp) {
    RxBuf buf = {0};

    HttpClientTlsConfig tls = resolve_tls(&req->tls);

    if (tls.skip_verify) {
        fprintf(stderr, "[http_client] WARNING: TLS server verification disabled for %s\n",
                req->url);
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "[http_client] curl_easy_init failed\n");
        return -1;
    }

    int timeout = req->timeout_ms > 0 ? req->timeout_ms : g_default_timeout;

    curl_easy_setopt(curl, CURLOPT_URL, req->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)timeout);

    /* TLS */
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

    /* Method and body */
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

    /* Extra headers */
    struct curl_slist* curl_headers = NULL;
    for (const HttpClientHeader* h = req->headers; h; h = h->next) {
        char line[512];
        snprintf(line, sizeof(line), "%s: %s", h->key, h->value);
        curl_headers = curl_slist_append(curl_headers, line);
    }
    if (curl_headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    }

    int ret      = -1;
    CURLcode res = curl_easy_perform(curl);

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

typedef struct {
    HttpClientRequest req;
    char* url_copy;
    char* body_copy;
    HttpClientHeader* headers_copy;
    HttpClientDoneCb cb;
    void* user_ctx;
} AsyncTask;

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

static void* async_thread(void* arg) {
    AsyncTask* task = (AsyncTask*)arg;

    HttpClientResponse* resp = calloc(1, sizeof(HttpClientResponse));
    int err                  = -1;
    if (resp) {
        err = do_perform(&task->req, resp);
    }

    task->cb(resp, err, task->user_ctx);

    free(task->url_copy);
    free(task->body_copy);
    http_client_headers_free(task->headers_copy);
    free(task);
    return NULL;
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
    return do_perform(req, resp);
}

int http_client_perform_async(const HttpClientRequest* req, HttpClientDoneCb cb, void* user_ctx) {
    if (!g_initialized) {
        fprintf(stderr, "[http_client] Not initialized — call http_client_init() first\n");
        return -1;
    }
    if (!req || !req->url || !cb) {
        return -1;
    }

    AsyncTask* task = calloc(1, sizeof(AsyncTask));
    if (!task) {
        return -1;
    }

    task->url_copy = strdup(req->url);
    if (!task->url_copy) {
        goto err_url;
    }

    if (req->body) {
        task->body_copy = strdup(req->body);
        if (!task->body_copy) {
            goto err_body;
        }
    }

    if (req->headers) {
        task->headers_copy = headers_clone(req->headers);
        if (!task->headers_copy) {
            goto err_headers;
        }
    }

    task->req         = *req;
    task->req.url     = task->url_copy;
    task->req.body    = task->body_copy;
    task->req.headers = task->headers_copy;
    task->cb          = cb;
    task->user_ctx    = user_ctx;

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int rc = pthread_create(&thread, &attr, async_thread, task);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        goto err_headers;
    }

    return 0;

err_headers:
    http_client_headers_free(task->headers_copy);
    free(task->body_copy);
err_body:
    free(task->url_copy);
err_url:
    free(task);
    return -1;
}
