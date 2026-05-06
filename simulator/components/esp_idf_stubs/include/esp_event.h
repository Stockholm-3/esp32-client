#ifndef ESP_EVENT_H
#define ESP_EVENT_H

#include "esp_err.h"

#include <stdint.h>

typedef void* esp_event_handler_instance_t;
typedef const char* esp_event_base_t;

#define ESP_EVENT_ANY_ID ((int32_t)-1)

static inline esp_err_t esp_event_loop_create_default(void) { return ESP_OK; }

static inline esp_err_t
esp_event_handler_instance_register(esp_event_base_t event_base, int32_t event_id,
                                    void* event_handler, void* event_handler_arg,
                                    esp_event_handler_instance_t* instance) {
    return ESP_OK;
}

static inline esp_err_t
esp_event_handler_instance_unregister(esp_event_base_t event_base, int32_t event_id,
                                      esp_event_handler_instance_t instance) {
    return ESP_OK;
}

#endif
