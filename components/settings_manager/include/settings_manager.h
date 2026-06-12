#pragma once
#include "wifi_manager.h"

#include <stdbool.h>
#include <stdint.h>

void settings_manager_init(void);

const char* settings_manager_get_ssid(void);
const char* settings_manager_get_password(void);
void settings_manager_save_wifi(const char* ssid, const char* password);
const char* settings_manager_get_location(void);
int settings_manager_get_price_zone(void);
char* settings_manager_get_price_zone_as_string();
int settings_manager_get_timeout(void);
uint8_t settings_manager_get_all_networks(SavedWifiNetwork* out_list, uint8_t max_count);
void settings_manager_remove_wifi(const char* ssid);

void settings_manager_save_location(const char* city);

typedef void (*settings_manager_location_cb_t)(const char* city);
void settings_manager_on_location_saved(settings_manager_location_cb_t cb);
void settings_manager_save_price_zone(int index);
void settings_manager_save_timeout(int index);

bool settings_manager_get_local_web_client_enabled(void);
const char* settings_manager_get_sta_static_ip(void);
const char* settings_manager_get_sta_gateway(void);
const char* settings_manager_get_sta_netmask(void);
const char* settings_manager_get_mdns_hostname(void);
bool settings_manager_get_ap_enabled(void);

void settings_manager_save_local_web_client_enabled(bool enabled);
void settings_manager_save_sta_static_ip(const char* ip);
void settings_manager_save_sta_gateway(const char* gateway);
void settings_manager_save_sta_netmask(const char* netmask);
void settings_manager_save_mdns_hostname(const char* hostname);
void settings_manager_save_ap_enabled(bool enabled);
