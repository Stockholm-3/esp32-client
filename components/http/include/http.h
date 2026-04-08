#ifndef HTTP_H
#define HTTP_H

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// HTTP response container
typedef struct {
    uint8_t *buffer;    // Pointer to user buffer
    size_t buffer_size; // Total buffer size
    size_t length;      // Actual data received
} http_response_t;

// HTTP GET
esp_err_t http_get(const char *url, http_response_t *response);

// HTTP POST
esp_err_t http_post(const char *url, const char *data,
                    http_response_t *response);

#endif // HTTP_H
