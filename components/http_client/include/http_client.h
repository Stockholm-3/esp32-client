#ifndef HTTP_CLIENT_H
#define HTTP_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * All public symbols are prefixed with HttpClient to avoid clashing with
 * esp_http_client.h which defines HTTP_METHOD_GET, HTTP_METHOD_POST, etc.
 * in the global namespace.
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
    const char* key;
    const char* value;
    struct HttpClientHeader* next;
} HttpClientHeader;

/*
 * TLS configuration. Used in both HttpClientConfig (global default) and
 * HttpClientRequest (per-request override).
 *
 * On ESP, all cert/key fields are PEM strings (NULL-terminated).
 * On the Linux stub, they are file paths passed to libcurl.
 *
 * Precedence: per-request TLS config takes priority over the global default.
 * If neither is set, TLS verification is performed using the system CA bundle.
 */
typedef struct {
    const char* ca_cert;     /* CA cert for server verification. NULL = system bundle.   */
    const char* client_cert; /* Client cert PEM for mutual TLS. NULL = no client auth.   */
    const char* client_key;  /* Client private key PEM for mutual TLS.                   */
    bool skip_verify;        /* Disable server cert verification. Development use only.  */
} HttpClientTlsConfig;

/*
 * Global configuration passed to http_client_init().
 * Zero-initialise to use defaults for all fields.
 */
typedef struct {
    HttpClientTlsConfig tls;
    int default_timeout_ms; /* 0 = built-in default (10 s) */
} HttpClientConfig;

/*
 * Per-request descriptor. Zero-initialise and set only the fields you need.
 * body and body_len are ignored for methods that carry no body (GET, HEAD).
 * headers is a linked list built with http_client_header_append().
 * timeout_ms of 0 falls back to the global default set in http_client_init().
 * tls overrides the global TLS config for this request only. Zero-initialise
 * to inherit the global config.
 */
typedef struct {
    const char* url;
    HttpClientMethod method;
    HttpClientHeader* headers;
    const char* body;
    size_t body_len;         /* 0 = use strlen(body)             */
    int timeout_ms;          /* 0 = use global default           */
    HttpClientTlsConfig tls; /* zero = inherit global TLS config */
} HttpClientRequest;

/*
 * Response produced when a request completes.
 * buffer is heap-allocated by the module. On the synchronous path the caller
 * must free it. On the async path the callback takes ownership (see below).
 * status is the HTTP status code, or 0 if the request never completed.
 */
typedef struct {
    uint8_t* buffer;
    size_t length;
    int status;
} HttpClientResponse;

/*
 * Return value of http_client_async_poll().
 *
 *   HTTP_CLIENT_POLL_BUSY  — request is in progress, call poll again.
 *   HTTP_CLIENT_POLL_DONE  — request finished (success or error).
 *                            The completion callback has already been invoked.
 *                            The caller must call http_client_async_free().
 */
typedef enum {
    HTTP_CLIENT_POLL_BUSY = 0,
    HTTP_CLIENT_POLL_DONE = 1,
} HttpClientPollResult;

/*
 * Completion callback invoked by http_client_async_poll() exactly once, on
 * the same call that returns HTTP_CLIENT_POLL_DONE.
 *
 * resp is heap-allocated by the module. The callee takes full ownership and
 * must free resp->buffer and resp itself.
 *
 * err is 0 on success, -1 on transport or TLS failure. When err is -1,
 * resp->buffer is NULL, resp->length is 0, and resp->status is 0.
 */
typedef void (*HttpClientDoneCb)(HttpClientResponse* resp, int err, void* user_ctx);

/*
 * Opaque handle for an in-flight async request.
 * Allocated by http_client_async_begin(), polled via http_client_async_poll(),
 * and released via http_client_async_free().
 *
 * One handle represents exactly one request. The caller owns all handles
 * and is responsible for polling and freeing them. The module maintains no
 * internal queue.
 */
typedef struct HttpClientAsyncHandle HttpClientAsyncHandle;

/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the HTTP client module.
 *
 * Must be called once before any other http_client_* function.
 * On the Linux stub this calls curl_global_init(). On ESP it stores
 * the config for use in subsequent requests.
 *
 * @param config  Optional global config. NULL uses built-in defaults.
 * @return 0 on success, -1 on failure.
 */
int http_client_init(const HttpClientConfig* config);

/**
 * @brief Tear down the HTTP client module and release global resources.
 *
 * All in-flight async handles must be freed before calling this.
 */
void http_client_deinit(void);

/**
 * @brief Append a header to a request header list.
 *
 * Pass a pointer to a NULL pointer to start a new list.
 * Free the list with http_client_headers_free() when done.
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
 * On success, resp->buffer is heap-allocated and must be freed by the caller.
 *
 * @return 0 on success, -1 on failure.
 */
int http_client_perform(const HttpClientRequest* req, HttpClientResponse* resp);

/**
 * @brief Begin an asynchronous HTTP request.
 *
 * Returns immediately with a handle that the caller drives by calling
 * http_client_async_poll() repeatedly from its own scheduler. The module
 * spawns no threads and maintains no internal queue.
 *
 * All fields in req are copied internally — the caller may free or reuse
 * req and its contents immediately after this call returns.
 *
 * @param req      Request descriptor.
 * @param cb       Completion callback, invoked once from within the poll call
 *                 that returns HTTP_CLIENT_POLL_DONE.
 * @param user_ctx Passed through to cb unchanged.
 * @return         A valid handle on success, NULL on alloc failure.
 */
HttpClientAsyncHandle* http_client_async_begin(const HttpClientRequest* req, HttpClientDoneCb cb,
                                               void* user_ctx);

/**
 * @brief Advance an async request by one scheduling step.
 *
 * Must be called from a single thread — the module does not synchronise
 * concurrent poll calls on the same handle.
 *
 * Returns HTTP_CLIENT_POLL_BUSY while the request is in progress.
 * Returns HTTP_CLIENT_POLL_DONE when the request has finished; on this call
 * the completion callback has already been invoked and the caller must
 * subsequently call http_client_async_free().
 *
 * Each call does only as much work as is available without blocking and
 * returns immediately if the socket would block.
 *
 * @return HTTP_CLIENT_POLL_BUSY or HTTP_CLIENT_POLL_DONE.
 */
HttpClientPollResult http_client_async_poll(HttpClientAsyncHandle* handle);

/**
 * @brief Release all resources associated with a completed async handle.
 *
 * Must be called after http_client_async_poll() returns HTTP_CLIENT_POLL_DONE.
 * Calling this before DONE is reached is undefined behaviour.
 */
void http_client_async_free(HttpClientAsyncHandle* handle);

#endif /* HTTP_CLIENT_H */
