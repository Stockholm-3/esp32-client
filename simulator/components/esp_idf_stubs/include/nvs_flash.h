#ifndef NVS_FLASH_H
#define NVS_FLASH_H

#include "esp_err.h"

// Define the missing NVS error codes
#define ESP_ERR_NVS_NO_FREE_PAGES 0x104
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x105

static inline esp_err_t nvs_flash_init(void) { return ESP_OK; }

static inline esp_err_t nvs_flash_erase(void) { return ESP_OK; }

#endif /* NVS_FLASH_H */
