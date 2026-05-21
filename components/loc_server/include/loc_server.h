#pragma once
#include "esp_err.h"
#include "wifi_manager.h"

void      loc_server_init(void);
esp_err_t loc_server_start(void);
void      loc_server_stop(void);
void      loc_server_push_settings(void);
void      loc_server_notify_wifi_state(WifiManagerState state);
