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
int settings_manager_get_timeout(void);
uint8_t settings_manager_get_all_networks(SavedWifiNetwork* out_list, uint8_t max_count);
