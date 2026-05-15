#pragma once
#include <stdbool.h>

void settings_manager_init(void);

const char* settings_manager_get_ssid(void);
const char* settings_manager_get_password(void);
void        settings_manager_save_wifi(const char *ssid, const char *password);
const char* settings_manager_get_location(void);
int         settings_manager_get_price_zone(void);
int         settings_manager_get_timeout(void);
void settings_manager_save_location(const char* city);
void settings_manager_save_price_zone(int index);
void settings_manager_save_timeout(int index);

bool        settings_manager_get_local_web_client_enabled(void);
const char* settings_manager_get_sta_static_ip(void);
const char* settings_manager_get_sta_gateway(void);
const char* settings_manager_get_sta_netmask(void);
const char* settings_manager_get_mdns_hostname(void);
bool        settings_manager_get_ap_enabled(void);

void settings_manager_save_local_web_client_enabled(bool enabled);
void settings_manager_save_sta_static_ip(const char* ip);
void settings_manager_save_sta_gateway(const char* gateway);
void settings_manager_save_sta_netmask(const char* netmask);
void settings_manager_save_mdns_hostname(const char* hostname);
void settings_manager_save_ap_enabled(bool enabled);
