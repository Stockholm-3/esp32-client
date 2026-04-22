#pragma once
#include <stdbool.h>

void settings_manager_init(void);

const char* settings_manager_get_location(void);
int         settings_manager_get_price_zone(void);
int         settings_manager_get_timeout(void);
