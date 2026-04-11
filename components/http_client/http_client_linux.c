#include "http_client.h"

#include <curl/curl.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    HttpResponse* resp;
} CurlContextT;

// libcurl write callback
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size   = size * nmemb;
    CurlContextT* ctx   = (CurlContextT*)userp;

    if (!ctx || !ctx->resp || !ctx->resp->buffer) {
        return 0;
    }

    HttpResponse* resp = ctx->resp;

    // prevent overflow
    size_t space_left = resp->buffer_size - resp->length;
    size_t copy_len   = total_size < space_left ? total_size : space_left;

    if (copy_len > 0) {
        memcpy(resp->buffer + resp->length, contents, copy_len);
        resp->length += copy_len;
    }

    return total_size; // tell curl we consumed everything
}

static esp_err_t perform_request(const char* url, const char* post_data, HttpResponse* response) {
    if (!response || !response->buffer || !url) {
        return ESP_ERR_INVALID_ARG;
    }

    response->length = 0;

    CURL* curl = curl_easy_init();
    if (!curl) {
        return ESP_FAIL;
    }

    CurlContextT ctx = {.resp = response};

    struct curl_slist* headers = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);

    // optional: follow redirects
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    if (post_data) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);

        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        if (headers) {
            curl_slist_free_all(headers);
        }
        return ESP_FAIL;
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    printf("[HTTP] Status = %ld, received = %zu bytes\n", http_code, response->length);

    curl_easy_cleanup(curl);
    if (headers) {
        curl_slist_free_all(headers);
    }

    return ESP_OK;
}

esp_err_t http_get(const char* url, HttpResponse* response) {
    return perform_request(url, NULL, response);
}

esp_err_t http_post(const char* url, const char* data, HttpResponse* response) {
    return perform_request(url, data, response);
}
