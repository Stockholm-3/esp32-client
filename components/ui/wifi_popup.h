#pragma once
#include "lvgl.h"
#include "wifi_manager.h"

typedef void (*WifiPopupConnectCbT)(const char* ssid, const char* password);

typedef enum {
    WIFI_POPUP_RESULT_CONNECTED,
    WIFI_POPUP_RESULT_WRONG_PASSWORD,
    WIFI_POPUP_RESULT_NO_AP,
    WIFI_POPUP_RESULT_FAILED,
} WifiPopupConnectResult;

void wifi_popup_init(lv_obj_t* parent);
void wifi_popup_update_networks(const WifiManagerApInfo* aps, uint16_t count);
void wifi_popup_on_connect(WifiPopupConnectCbT cb);
void wifi_popup_notify_result(WifiPopupConnectResult result);
void wifi_popup_set_connected_ssid(const char* ssid);
