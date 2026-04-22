#include "nvs.h"
#include "nvs_flash.h"
#include "settings_manager.h"
#include "ui_binder.h"

#include <string.h>

#define NVS_NAMESPACE "settings"

static char g_s_location[128] = "";
static int g_s_price_zone     = 0;
static int g_s_timeout        = 0;

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

void(settings_manager_init(void)) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        size_t len = sizeof(g_s_location);
        // if key doesn't exist nvs_get return error and s_locatin vill stay ""
        nvs_get_str(h, "location", g_s_location, &len);

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
