#pragma once
#include <stdbool.h>

void settings_manager_init(void);

const char* settings_manager_get_ssid(void);
const char* settings_manager_get_password(void);
void        settings_manager_save_wifi(const char *ssid, const char *password);
const char* settings_manager_get_location(void);
int         settings_manager_get_price_zone(void);
int         settings_manager_get_timeout(void);
