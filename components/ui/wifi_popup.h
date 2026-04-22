#pragma once
#include "lvgl.h"
#include "wifi_manager.h"

typedef void (*wifi_popup_connect_cb_t)(const char *ssid, const char *password);

void wifi_popup_init(lv_obj_t *parent);
void wifi_popup_update_networks(const WifiApInfo *aps, uint16_t count);
void wifi_popup_on_connect(wifi_popup_connect_cb_t cb);
