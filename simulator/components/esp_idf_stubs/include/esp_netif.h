#ifndef ESP_NETIF_H
#define ESP_NETIF_H

#include "esp_err.h"

static inline void* esp_netif_create_default_wifi_sta(void) { return (void*)1; }

static inline esp_err_t esp_netif_init(void) { return ESP_OK; }

#endif
