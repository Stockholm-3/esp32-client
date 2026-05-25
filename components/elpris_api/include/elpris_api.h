#ifndef ELPRIS_API_H
#define ELPRIS_API_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define ELPRIS_HOURS 24

typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint32_t hourly_prices[ELPRIS_HOURS];  // Price in öre (1/100 SEK)
    float avg_price_sek;
    float min_price_sek;
    float max_price_sek;
    uint8_t min_hour;
    uint8_t max_hour;
    bool valid;
} ElprisData;

typedef enum {
    ELPRIS_SE1 = 0,
    ELPRIS_SE2,
    ELPRIS_SE3,
    ELPRIS_SE4
} ElprisPriceGroup;

// Initialize the API
void elpris_api_init(void);

// Fetch data for a specific date
esp_err_t elpris_fetch_date(uint16_t year, uint8_t month, uint8_t day,
                            ElprisPriceGroup group, ElprisData* out_data);

// Fetch latest available data (today or tomorrow after 13:00)
esp_err_t elpris_fetch_latest(ElprisPriceGroup group, ElprisData* out_data);

#endif