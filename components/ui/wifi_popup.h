#pragma once
#include "lvgl.h"
#include "wifi_manager.h"

typedef void (*WifiPopupConnectCbT)(const char* ssid, const char* password);

void wifi_popup_init(lv_obj_t* parent);
void wifi_popup_update_networks(const WifiManagerApInfo* aps, uint16_t count);
void wifi_popup_on_connect(WifiPopupConnectCbT cb);
