#include "elpris_api.h"
#include "esp_log.h"
#include "http_client.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

static const char* TAG = "ELPRIS_API";
#define BASE_URL "https://www.elprisetjustnu.se/api/v1/prices"
static const char* PRICE_GROUP_STR[] = {"SE1", "SE2", "SE3", "SE4"};

// Simple cache
typedef struct {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    ElprisPriceGroup group;
    ElprisData data;
    bool valid;
} CacheEntry;

#define MAX_CACHE 5
static CacheEntry s_cache[MAX_CACHE];
static int s_cache_count = 0;

// Swedish time with DST handling
static void get_swedish_time(struct tm* out_tm) {
    time_t now = time(NULL);
    struct tm utc;
    gmtime_r(&now, &utc);
    
    int year = utc.tm_year + 1900;
    struct tm dst_start = {0};
    dst_start.tm_year = year - 1900;
    dst_start.tm_mon = 2;      // March
    dst_start.tm_mday = 31;
    dst_start.tm_hour = 1;
    mktime(&dst_start);
    dst_start.tm_mday -= dst_start.tm_wday;  // Last Sunday
    
    struct tm dst_end = {0};
    dst_end.tm_year = year - 1900;
    dst_end.tm_mon = 9;        // October
    dst_end.tm_mday = 31;
    dst_end.tm_hour = 1;
    mktime(&dst_end);
    dst_end.tm_mday -= dst_end.tm_wday;      // Last Sunday
    
    time_t dst_start_ts = mktime(&dst_start);
    time_t dst_end_ts = mktime(&dst_end);
    int offset_hours = (now >= dst_start_ts && now < dst_end_ts) ? 2 : 1;
    
    time_t local_time = now + (offset_hours * 3600);
    gmtime_r(&local_time, out_tm);
}

static void get_latest_date(uint16_t* year, uint8_t* month, uint8_t* day) {
    struct tm swedish_time;
    get_swedish_time(&swedish_time);
    if (swedish_time.tm_hour >= 13) {
        swedish_time.tm_mday += 1;
        mktime(&swedish_time);
    }
    *year = swedish_time.tm_year + 1900;
    *month = swedish_time.tm_mon + 1;
    *day = swedish_time.tm_mday;
}

// Manual JSON parser for the specific API response format
// Format: [{"time_start":"2024-01-15T00:00:00Z","time_end":"...","SEK_per_kWh":0.2934},...]
static void parse_and_fill(const char* response, ElprisData* out_data) {
    memset(out_data, 0, sizeof(ElprisData));
    
    // Initialize min/max
    out_data->min_price_sek = 999.0f;
    out_data->max_price_sek = 0.0f;
    out_data->min_hour = 0;
    out_data->max_hour = 0;
    
    float hourly_total[24] = {0};
    int hourly_count[24] = {0};
    float total = 0.0f;
    int total_count = 0;
    
    const char* ptr = response;
    int hour = 0;
    float price = 0.0f;
    
    // Parse each object in the array
    while (*ptr) {
        // Find "time_start"
        const char* time_start = strstr(ptr, "\"time_start\":\"");
        if (!time_start) break;
        
        // Parse the timestamp to get hour
        // Format: "2024-01-15T14:30:00Z"
        const char* time_str = time_start + 14; // Skip "\"time_start\":\""
        int h = 0;
        if (sscanf(time_str, "%*d-%*d-%*dT%d", &h) == 1) {
            hour = h;
        } else {
            ptr = time_start + 1;
            continue;
        }
        
        // Find "SEK_per_kWh"
        const char* price_start = strstr(ptr, "\"SEK_per_kWh\":");
        if (!price_start) break;
        
        // Parse the price value
        const char* price_str = price_start + 14; // Skip "\"SEK_per_kWh\":"
        price = 0.0f;
        
        // Handle different number formats (could be integer or float)
        if (sscanf(price_str, "%f", &price) == 1) {
            // Valid price parsed
        } else {
            ptr = price_start + 1;
            continue;
        }
        
        // Store price in öre (1/100 SEK) for the chart
        uint32_t price_ore = (uint32_t)round(price * 100.0f);
        out_data->hourly_prices[hour] = price_ore;
        
        // Accumulate for averages
        hourly_total[hour] += price;
        hourly_count[hour]++;
        total += price;
        total_count++;
        
        // Update min/max
        if (price < out_data->min_price_sek) {
            out_data->min_price_sek = price;
            out_data->min_hour = hour;
        }
        if (price > out_data->max_price_sek) {
            out_data->max_price_sek = price;
            out_data->max_hour = hour;
        }
        
        // Move to next object
        ptr = price_str + 1;
    }
    
    // Calculate hourly averages and find overall min/max
    for (int h = 0; h < 24; h++) {
        if (hourly_count[h] > 0) {
            float hour_avg = hourly_total[h] / hourly_count[h];
            // Update min/max based on hourly averages
            if (hour_avg < out_data->min_price_sek) {
                out_data->min_price_sek = hour_avg;
                out_data->min_hour = h;
            }
            if (hour_avg > out_data->max_price_sek) {
                out_data->max_price_sek = hour_avg;
                out_data->max_hour = h;
            }
        }
    }
    
    if (total_count > 0) {
        out_data->avg_price_sek = total / total_count;
        out_data->valid = true;
        ESP_LOGI(TAG, "Parsed: avg=%.2f kr, min=%.2f kr at %02d:00, max=%.2f kr at %02d:00",
                 out_data->avg_price_sek, out_data->min_price_sek, out_data->min_hour,
                 out_data->max_price_sek, out_data->max_hour);
    } else {
        ESP_LOGE(TAG, "No valid price data found in response");
    }
}

void elpris_api_init(void) {
    ESP_LOGI(TAG, "Initialized");
    s_cache_count = 0;
    memset(s_cache, 0, sizeof(s_cache));
}

esp_err_t elpris_fetch_date(uint16_t year, uint8_t month, uint8_t day,
                            ElprisPriceGroup group, ElprisData* out_data) {
    if (!out_data) return ESP_ERR_INVALID_ARG;
    
    // Check cache
    for (int i = 0; i < s_cache_count; i++) {
        if (s_cache[i].year == year && s_cache[i].month == month &&
            s_cache[i].day == day && s_cache[i].group == group && s_cache[i].valid) {
            ESP_LOGI(TAG, "Cache hit for %04u-%02u-%02u %s", year, month, day, PRICE_GROUP_STR[group]);
            memcpy(out_data, &s_cache[i].data, sizeof(ElprisData));
            return ESP_OK;
        }
    }
    
    char url[128];
    snprintf(url, sizeof(url), "%s/%04u/%02u-%02u_%s.json",
             BASE_URL, year, month, day, PRICE_GROUP_STR[group]);
    ESP_LOGI(TAG, "Fetching: %s", url);
    
    HttpClientRequest req = {
        .method = HTTP_CLIENT_METHOD_GET,
        .url = url,
        .timeout_ms = 30000,
        .tls = {.skip_verify = false},
    };
    HttpClientResponse resp = {0};
    
    if (http_client_perform(&req, &resp) != 0 || resp.status != 200) {
        ESP_LOGE(TAG, "HTTP request failed, status: %d", resp.status);
        if (resp.buffer) free(resp.buffer);
        return ESP_FAIL;
    }
    
    parse_and_fill((char*)resp.buffer, out_data);
    free(resp.buffer);
    
    if (out_data->valid) {
        // Cache it (FIFO)
        if (s_cache_count >= MAX_CACHE) {
            for (int i = 1; i < s_cache_count; i++) {
                s_cache[i-1] = s_cache[i];
            }
            s_cache_count--;
        }
        s_cache[s_cache_count].year = year;
        s_cache[s_cache_count].month = month;
        s_cache[s_cache_count].day = day;
        s_cache[s_cache_count].group = group;
        s_cache[s_cache_count].data = *out_data;
        s_cache[s_cache_count].valid = true;
        s_cache_count++;
        ESP_LOGI(TAG, "Cached data for %04u-%02u-%02u %s", year, month, day, PRICE_GROUP_STR[group]);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "No valid data for %04u-%02u-%02u %s", year, month, day, PRICE_GROUP_STR[group]);
    return ESP_FAIL;
}

esp_err_t elpris_fetch_latest(ElprisPriceGroup group, ElprisData* out_data) {
    uint16_t year;
    uint8_t month, day;
    get_latest_date(&year, &month, &day);
    ESP_LOGI(TAG, "Fetching latest for %04u-%02u-%02u %s", year, month, day, PRICE_GROUP_STR[group]);
    return elpris_fetch_date(year, month, day, group, out_data);
}