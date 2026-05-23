#include "nvs.h"
#include "nvs_flash.h"
#include "settings_manager.h"
#include "ui_binder.h"

#include <string.h>

#define NVS_NAMESPACE "settings"

static char g_s_ssid[33]      = "";
static char g_s_password[65]  = "";
static char g_s_location[128] = "";
static int g_s_price_zone     = 0;
static int g_s_timeout        = 0;
static int g_s_brightness     = 100;

static bool g_s_local_web_client_enabled = false;
static char g_s_sta_static_ip[16]        = "";
static char g_s_sta_gateway[16]          = "";
static char g_s_sta_netmask[16]          = "255.255.255.0";
static char g_s_mdns_hostname[33]        = "esp32-client";
static bool g_s_ap_enabled               = false;

void settings_manager_save_location(const char* city) {
    strncpy(g_s_location, city, sizeof(g_s_location) - 1);
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "location", city);
        nvs_commit(h);
        nvs_close(h);
    }
}
void settings_manager_save_price_zone(int index) {
    g_s_price_zone = index;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "price_zone", (uint8_t)index);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_timeout(int index) {
    g_s_timeout = index;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "timeout", (uint8_t)index);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_brightness(int value) {
    if (value < 1)
        value = 1;
    if (value > 100)
        value = 100;
    g_s_brightness = value;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "brightness", (uint8_t)value);
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
        nvs_get_str(h, "location", g_s_location, &len);

        len = sizeof(g_s_ssid);
        nvs_get_str(h, "ssid", g_s_ssid, &len);
        len = sizeof(g_s_password);
        nvs_get_str(h, "password", g_s_password, &len);

        uint8_t val = 0;
        if (nvs_get_u8(h, "price_zone", &val) == ESP_OK) {
            g_s_price_zone = val;
        }
        if (nvs_get_u8(h, "timeout", &val) == ESP_OK) {
            g_s_timeout = val;
        }
        if (nvs_get_u8(h, "brightness", &val) == ESP_OK) {
            g_s_brightness = (int)val;
            if (g_s_brightness < 1)
                g_s_brightness = 1;
            if (g_s_brightness > 100)
                g_s_brightness = 100;
        }
        if (nvs_get_u8(h, "lwc_enabled", &val) == ESP_OK) {
            g_s_local_web_client_enabled = (bool)val;
        }
        if (nvs_get_u8(h, "ap_enabled", &val) == ESP_OK) {
            g_s_ap_enabled = (bool)val;
        }
        len = sizeof(g_s_sta_static_ip);
        nvs_get_str(h, "sta_static_ip", g_s_sta_static_ip, &len);
        len = sizeof(g_s_sta_gateway);
        nvs_get_str(h, "sta_gateway", g_s_sta_gateway, &len);
        len = sizeof(g_s_sta_netmask);
        if (nvs_get_str(h, "sta_netmask", g_s_sta_netmask, &len) != ESP_OK) {
            strncpy(g_s_sta_netmask, "255.255.255.0", sizeof(g_s_sta_netmask) - 1);
        }
        len = sizeof(g_s_mdns_hostname);
        if (nvs_get_str(h, "mdns_hostname", g_s_mdns_hostname, &len) != ESP_OK) {
            strncpy(g_s_mdns_hostname, "esp32-client", sizeof(g_s_mdns_hostname) - 1);
        }
        nvs_close(h);
    }

    ui_binder_set_location(g_s_location);
    ui_binder_set_price_zone(g_s_price_zone);
    ui_binder_set_timeout(g_s_timeout);
    ui_binder_set_ap_enabled(g_s_ap_enabled);
    ui_binder_set_local_web_client_enabled(g_s_local_web_client_enabled);
    ui_binder_set_brightness(g_s_brightness);

    ui_binder_on_location_changed(settings_manager_save_location);
    ui_binder_on_price_changed(settings_manager_save_price_zone);
    ui_binder_on_timeout_changed(settings_manager_save_timeout);
    ui_binder_on_ap_enabled_changed(settings_manager_save_ap_enabled);
    ui_binder_on_local_web_client_changed(settings_manager_save_local_web_client_enabled);
    ui_binder_on_brightness_changed(settings_manager_save_brightness);
}
const char* settings_manager_get_location(void) { return g_s_location; }
int settings_manager_get_price_zone(void) { return g_s_price_zone; }
int settings_manager_get_timeout(void) { return g_s_timeout; }
int settings_manager_get_brightness(void) { return g_s_brightness; }
const char* settings_manager_get_ssid(void) { return g_s_ssid; }
const char* settings_manager_get_password(void) { return g_s_password; }

void settings_manager_save_wifi(const char* ssid, const char* password) {
    strncpy(g_s_ssid, ssid, 32);
    g_s_ssid[32] = '\0';
    strncpy(g_s_password, password, 64);
    g_s_password[64] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", g_s_ssid);
        nvs_set_str(h, "password", g_s_password);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool settings_manager_get_local_web_client_enabled(void) { return g_s_local_web_client_enabled; }
const char* settings_manager_get_sta_static_ip(void) { return g_s_sta_static_ip; }
const char* settings_manager_get_sta_gateway(void) { return g_s_sta_gateway; }
const char* settings_manager_get_sta_netmask(void) { return g_s_sta_netmask; }
const char* settings_manager_get_mdns_hostname(void) { return g_s_mdns_hostname; }
bool settings_manager_get_ap_enabled(void) { return g_s_ap_enabled; }

void settings_manager_save_local_web_client_enabled(bool enabled) {
    g_s_local_web_client_enabled = enabled;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "lwc_enabled", (uint8_t)enabled);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_sta_static_ip(const char* ip) {
    strncpy(g_s_sta_static_ip, ip, sizeof(g_s_sta_static_ip) - 1);
    g_s_sta_static_ip[sizeof(g_s_sta_static_ip) - 1] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "sta_static_ip", g_s_sta_static_ip);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_sta_gateway(const char* gateway) {
    strncpy(g_s_sta_gateway, gateway, sizeof(g_s_sta_gateway) - 1);
    g_s_sta_gateway[sizeof(g_s_sta_gateway) - 1] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "sta_gateway", g_s_sta_gateway);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_sta_netmask(const char* netmask) {
    strncpy(g_s_sta_netmask, netmask, sizeof(g_s_sta_netmask) - 1);
    g_s_sta_netmask[sizeof(g_s_sta_netmask) - 1] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "sta_netmask", g_s_sta_netmask);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_mdns_hostname(const char* hostname) {
    strncpy(g_s_mdns_hostname, hostname, sizeof(g_s_mdns_hostname) - 1);
    g_s_mdns_hostname[sizeof(g_s_mdns_hostname) - 1] = '\0';
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "mdns_hostname", g_s_mdns_hostname);
        nvs_commit(h);
        nvs_close(h);
    }
}

void settings_manager_save_ap_enabled(bool enabled) {
    g_s_ap_enabled = enabled;
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "ap_enabled", (uint8_t)enabled);
        nvs_commit(h);
        nvs_close(h);
    }
}
