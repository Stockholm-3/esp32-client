#include "esp_http_client.h"
#include "esp_log.h"
#include "http_client.h"

#include <string.h>

static const char* g_tag = "http_client";

// Internal context passed to event handler
typedef struct {
    HttpResponse* resp;
} HttpContextT;

// Event handler (collects data into buffer)
static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    HttpContextT* ctx = (HttpContextT*)evt->user_data;
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (!esp_http_client_is_chunked_response(evt->client)) {
            if (ctx && ctx->resp && ctx->resp->buffer) {
                size_t copy_len = evt->data_len;
                // Prevent overflow
                if (ctx->resp->length + copy_len > ctx->resp->buffer_size) {
                    copy_len = ctx->resp->buffer_size - ctx->resp->length;
                    ESP_LOGW(g_tag, "Response truncated!");
                }
                memcpy(ctx->resp->buffer + ctx->resp->length, evt->data, copy_len);
                ctx->resp->length += copy_len;
            }
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t http_get(const char* url, HttpResponse* response) {
    if (!response || !response->buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    response->length = 0;
    HttpContextT ctx = {
        .resp = response,
    };
    esp_http_client_config_t config = {
        .url           = url,
        .event_handler = http_event_handler,
        .user_data     = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(g_tag, "Failed to init client");
        return ESP_FAIL;
    }
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(g_tag, "GET Status = %d, received = %d bytes", status, (int)response->length);
    } else {
        ESP_LOGE(g_tag, "GET failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t http_post(const char* url, const char* data, HttpResponse* response) {
    if (!response || !response->buffer) {
        return ESP_ERR_INVALID_ARG;
    }
    response->length = 0;
    HttpContextT ctx = {
        .resp = response,
    };
    esp_http_client_config_t config = {
        .url           = url,
        .event_handler = http_event_handler,
        .user_data     = &ctx,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(g_tag, "Failed to init client");
        return ESP_FAIL;
    }
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (data) {
        esp_http_client_set_post_field(client, data, (int)strlen(data));
    }
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(g_tag, "POST Status = %d, received = %d bytes", status, (int)response->length);
    } else {
        ESP_LOGE(g_tag, "POST failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}
