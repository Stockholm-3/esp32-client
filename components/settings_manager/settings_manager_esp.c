#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "settings_manager.h"
#include "ui_binder.h"
#include "wifi_manager.h"

#include <string.h>

#define NVS_NAMESPACE "settings"
#define MAX_SAVED_NETWORKS 5
#define TAG "SETTINGS_MGR"

// Global in-memory settings variables
static char g_s_location[128] = "";
static int g_s_price_zone     = 0;
static int g_s_timeout        = 0;

// The structure array holding up to 5 profiles
static SavedWifiNetwork g_wifi_list[MAX_SAVED_NETWORKS];
static uint8_t g_wifi_count = 0;

static void on_location_changed(const char* city) {
    strncpy(g_s_location, city, sizeof(g_s_location) - 1);
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "location", city);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void on_price_changed(int index) {
    g_s_price_zone = index;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "price_zone", (uint8_t)index);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void on_timeout_changed(int index) {
    g_s_timeout = index;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "timeout", (uint8_t)index);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize memory structures safely
    memset(g_wifi_list, 0, sizeof(g_wifi_list));

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        size_t len = sizeof(g_s_location);
        nvs_get_str(h, "location", g_s_location, &len);

        // Load the Wi-Fi list binary blob
        size_t blob_size = sizeof(g_wifi_list);
        esp_err_t err    = nvs_get_blob(h, "wifi_list", g_wifi_list, &blob_size);
        if (err == ESP_OK) {
            // Calculate how many actual slots are valid
            g_wifi_count = 0;
            for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
                if (g_wifi_list[i].ssid[0] != '\0') {
                    g_wifi_count++;
                }
            }
            ESP_LOGI(TAG, "Successfully loaded %d Wi-Fi profile(s) from flash.", g_wifi_count);
        } else if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "No Wi-Fi profiles database found in flash yet.");
            g_wifi_count = 0;
        }

        uint8_t val = 0;
        if (nvs_get_u8(h, "price_zone", &val) == ESP_OK) {
            g_s_price_zone = val;
        }
        if (nvs_get_u8(h, "timeout", &val) == ESP_OK) {
            g_s_timeout = val;
        }
        nvs_close(h);
    }

    ui_binder_set_location(g_s_location);
    ui_binder_set_price_zone(g_s_price_zone);
    ui_binder_set_timeout(g_s_timeout);

    ui_binder_on_location_changed(on_location_changed);
    ui_binder_on_price_changed(on_price_changed);
    ui_binder_on_timeout_changed(on_timeout_changed);
}

const char* settings_manager_get_location(void) { return g_s_location; }
int settings_manager_get_price_zone(void) { return g_s_price_zone; }
int settings_manager_get_timeout(void) { return g_s_timeout; }

// Returns the total quantity of saved profiles inside the runtime array
uint8_t settings_manager_get_all_networks(SavedWifiNetwork* out_list, uint8_t max_count) {
    uint8_t copy_count = (g_wifi_count < max_count) ? g_wifi_count : max_count;
    if (copy_count > 0 && out_list != NULL) {
        memcpy(out_list, g_wifi_list, sizeof(SavedWifiNetwork) * copy_count);
    }
    return copy_count;
}

// Fallbacks required for legacy functions or modules expecting single returns
const char* settings_manager_get_ssid(void) {
    return (g_wifi_count > 0) ? g_wifi_list[0].ssid : "";
}
const char* settings_manager_get_password(void) {
    return (g_wifi_count > 0) ? g_wifi_list[0].password : "";
}

void settings_manager_save_wifi(const char* ssid, const char* password) {
    if (ssid == NULL || ssid[0] == '\0') {
        return;
    }

    int existing_index = -1;

    for (int i = 0; i < g_wifi_count; i++) {
        if (strcmp(g_wifi_list[i].ssid, ssid) == 0) {
            existing_index = i;
            break;
        }
    }

    if (existing_index != -1) {
        SavedWifiNetwork target = g_wifi_list[existing_index];
        for (int i = existing_index; i > 0; i--) {
            g_wifi_list[i] = g_wifi_list[i - 1];
        }
        g_wifi_list[0] = target;
        strncpy(g_wifi_list[0].password, password ? password : "",
                sizeof(g_wifi_list[0].password) - 1);
        g_wifi_list[0].password[sizeof(g_wifi_list[0].password) - 1] = '\0';
        ESP_LOGI(TAG, "Existing network '%s' moved to top spot.", ssid);
    } else {
        int shift_limit =
            (g_wifi_count < MAX_SAVED_NETWORKS) ? g_wifi_count : (MAX_SAVED_NETWORKS - 1);

        for (int i = shift_limit; i > 0; i--) {
            g_wifi_list[i] = g_wifi_list[i - 1];
        }

        strncpy(g_wifi_list[0].ssid, ssid, sizeof(g_wifi_list[0].ssid) - 1);
        g_wifi_list[0].ssid[sizeof(g_wifi_list[0].ssid) - 1] = '\0';
        strncpy(g_wifi_list[0].password, password ? password : "",
                sizeof(g_wifi_list[0].password) - 1);
        g_wifi_list[0].password[sizeof(g_wifi_list[0].password) - 1] = '\0';

        if (g_wifi_count < MAX_SAVED_NETWORKS) {
            g_wifi_count++;
            ESP_LOGI(TAG, "Saved new network '%s'. Profile usage: %d/%d", ssid, g_wifi_count,
                     MAX_SAVED_NETWORKS);
        } else {
            ESP_LOGW(TAG, "Reached cap of %d networks. Dropped oldest profile from flash memory.",
                     MAX_SAVED_NETWORKS);
        }
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        size_t blob_size = sizeof(g_wifi_list);
        nvs_set_blob(h, "wifi_list", g_wifi_list, blob_size);
        nvs_commit(h);
        nvs_close(h);
    }
}
