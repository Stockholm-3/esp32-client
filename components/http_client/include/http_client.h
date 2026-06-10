#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * ESP32 HTTP/HTTPS client — synchronous, serialised queue.
 *
 * A single "http_worker" FreeRTOS task owns every TLS session. Callers post
 * a request and block until the worker finishes it. Because only one request
 * is live at a time, only one ~23 KB mbedTLS context exists on the heap
 * simultaneously, preventing MBEDTLS_ERR_SSL_ALLOC_FAILED when multiple
 * tasks try to open TLS connections concurrently.
 *
 * All public symbols are prefixed with HttpClient to avoid clashing with
 * esp_http_client.h which defines HTTP_METHOD_GET etc. in the global namespace.
 */

typedef enum {
    HTTP_CLIENT_METHOD_GET = 0,
    HTTP_CLIENT_METHOD_POST,
    HTTP_CLIENT_METHOD_PUT,
    HTTP_CLIENT_METHOD_PATCH,
    HTTP_CLIENT_METHOD_DELETE,
    HTTP_CLIENT_METHOD_HEAD,
} HttpClientMethod;

typedef struct HttpClientHeader {
    const char*              key;
    const char*              value;
    struct HttpClientHeader* next;
} HttpClientHeader;

/*
 * TLS configuration. Used in HttpClientConfig (global default) and
 * HttpClientRequest (per-request override).
 *
 * Precedence: per-request fields override the global default.
 * Default (all zero/NULL): the Espressif IDF certificate bundle is used,
 * which covers all major CAs and is updated with each IDF release.
 */
typedef struct {
    const char* ca_cert;     /* Custom CA cert PEM. NULL = use IDF bundle.          */
    const char* client_cert; /* Client cert PEM for mutual TLS. NULL = none.        */
    const char* client_key;  /* Client private key PEM for mutual TLS. NULL = none. */
    bool        skip_verify; /* Disable server cert verification. Dev only.         */
} HttpClientTlsConfig;

/*
 * Global config passed to http_client_init().
 * Zero-initialise to use all defaults.
 */
typedef struct {
    HttpClientTlsConfig tls;
    int                 default_timeout_ms; /* 0 = built-in default (15 s) */
} HttpClientConfig;

/*
 * Per-request descriptor. Zero-initialise and set only the fields you need.
 * body/body_len are ignored for methods that carry no body (GET, HEAD).
 * headers is a linked list built with http_client_header_append().
 * timeout_ms 0 falls back to the global default.
 * tls fields override the global TLS config; zero = inherit.
 */
typedef struct {
    const char*       url;
    HttpClientMethod  method;
    HttpClientHeader* headers;
    const char*       body;
    size_t            body_len;   /* 0 = strlen(body) */
    int               timeout_ms; /* 0 = global default */
    HttpClientTlsConfig tls;
} HttpClientRequest;

/*
 * Response filled in by http_client_perform() on success.
 * buffer is heap-allocated; the caller must free it with free().
 * status is the HTTP status code, or 0 if the request never completed.
 */
typedef struct {
    uint8_t* buffer;
    size_t   length;
    int      status;
} HttpClientResponse;

/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the HTTP client module.
 *
 * Creates the internal request queue and spawns the single serialising
 * worker task. Must be called once before any other http_client_* function.
 *
 * @param config  Optional global config. NULL uses built-in defaults.
 * @return 0 on success, -1 on failure.
 */
int http_client_init(const HttpClientConfig* config);

/**
 * @brief Tear down the HTTP client module and release all resources.
 */
void http_client_deinit(void);

/**
 * @brief Append a header to a linked list.
 *
 * Pass a pointer to a NULL pointer to start a new list.
 * Free with http_client_headers_free() when done.
 *
 * @return 0 on success, -1 on alloc failure.
 */
int http_client_header_append(HttpClientHeader** head, const char* key, const char* value);

/**
 * @brief Free a header list built with http_client_header_append().
 */
void http_client_headers_free(HttpClientHeader* head);

/**
 * @brief Perform a synchronous HTTP request. Blocks until complete.
 *
 * Multiple tasks may call this concurrently; requests are serialised
 * internally through the worker queue with no TLS heap contention.
 *
 * On success, resp->buffer is heap-allocated and must be freed by the caller.
 *
 * @return 0 on success, -1 on failure.
 */
int http_client_perform(const HttpClientRequest* req, HttpClientResponse* resp);

#endif /* HTTP_CLIENT_H */
