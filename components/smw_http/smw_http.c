#include "smw_http.h"

#include "esp_log.h"

#include <stdlib.h>
#include <string.h>

static const char* TAG = "smw_http";

typedef struct {
    HttpClientAsyncHandle* handle;
    HttpClientRequest req;
    HttpClientDoneCb user_cb;
    void* user_ctx;
    bool started;
} SmwHttpTaskCtx;

static SmwTaskStatus smw_http_poll(void* context, uint32_t now_ms) {
    (void)now_ms;
    SmwHttpTaskCtx* ctx = (SmwHttpTaskCtx*)context;

    if (!ctx->started) {
        // Initialize the async operation
        ctx->handle = http_client_async_begin(&ctx->req, ctx->user_cb, ctx->user_ctx);
        if (!ctx->handle) {
            ESP_LOGE(TAG, "Failed to begin async HTTP request");
            free(ctx);
            return (SmwTaskStatus){.next_wake_ms = 0, .code = SMW_TASK_DONE};
        }
        ctx->started = true;
        return (SmwTaskStatus){.next_wake_ms = 50, .code = SMW_TASK_RUNNING};
    }

    HttpClientPollResult poll_res = http_client_async_poll(ctx->handle);
    if (poll_res == HTTP_CLIENT_POLL_DONE) {
        // The async handle is finished; clean up resources and free context
        http_client_async_free(ctx->handle);
        free(ctx);
        return (SmwTaskStatus){.next_wake_ms = 0, .code = SMW_TASK_DONE};
    }

    // Poll again after 50ms
    return (SmwTaskStatus){.next_wake_ms = 50, .code = SMW_TASK_RUNNING};
}

SmwResult smw_register_http_request(SmwWorker* worker, const HttpClientRequest* req,
                                    HttpClientDoneCb cb, void* user_ctx, size_t* out_task_id) {
    // Allocate task context on the heap to preserve state across poll ticks
    SmwHttpTaskCtx* ctx = calloc(1, sizeof(SmwHttpTaskCtx));
    if (!ctx) {
        return SMW_ERROR_FULL;
    }

    ctx->req      = *req;
    ctx->user_cb  = cb;
    ctx->user_ctx = user_ctx;
    ctx->started  = false;

    SmwResult res = smw_register_task(worker, smw_http_poll, ctx, out_task_id);
    if (res != SMW_OK) {
        free(ctx);
    }
    return res;
}
