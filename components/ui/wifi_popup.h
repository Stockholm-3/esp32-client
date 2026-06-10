/**
 * @file wifi_popup.h
 * @ingroup ui
 * @brief Modal WiFi popup for network selection and password entry.
 *
 * Implements a modal dialog rendered on top of the main screen that lets
 * the user browse scan results, enter a password and receive connection
 * feedback. Only one callback slot is provided for each event.
 */
#pragma once
#include "lvgl.h"
#include "wifi_manager.h"

/**
 * @brief Callback invoked when the user taps the Connect button.
 *
 * @param ssid      Name of the selected access point (null-terminated).
 * @param password  Password entered by the user (null-terminated).
 */
typedef void (*WifiPopupConnectCbT)(const char* ssid, const char* password);

/**
 * @brief Result of a WiFi connection attempt.
 */
typedef enum {
    WIFI_POPUP_RESULT_CONNECTED,      /**< Connection established successfully. */
    WIFI_POPUP_RESULT_WRONG_PASSWORD, /**< Authentication failed (wrong password). */
    WIFI_POPUP_RESULT_NO_AP,          /**< Access point not found. */
    WIFI_POPUP_RESULT_FAILED,         /**< Connection failed for another reason. */
} WifiPopupConnectResult;

/**
 * @brief Create and initialise the WiFi popup.
 *
 * Builds the modal backdrop and dialog as children of @p parent.
 * The popup is hidden on creation and is shown when the WiFi button
 * in the Settings tab is pressed.
 *
 * @param parent  Parent LVGL object (typically the root screen).
 */
void wifi_popup_init(lv_obj_t* parent);

/**
 * @brief Refresh the access-point list displayed in the popup.
 *
 * Rebuilds the list widget from the provided scan results. Safe to call
 * from any task as long as the LVGL mutex is held by the caller.
 *
 * @param aps    Array of access-point descriptors from a WiFi scan.
 * @param count  Number of entries in @p aps.
 */
void wifi_popup_update_networks(const WifiManagerApInfo* aps, uint16_t count);

/**
 * @brief Register a callback for the Connect button event.
 *
 * The callback receives the selected SSID and the password entered by
 * the user. Only one callback can be registered at a time.
 *
 * @param cb  Handler function of type @ref WifiPopupConnectCbT.
 */
void wifi_popup_on_connect(WifiPopupConnectCbT cb);

/**
 * @brief Notify the popup of a connection result.
 *
 * Updates the status label and indicator according to @p result.
 * On @ref WIFI_POPUP_RESULT_CONNECTED the popup closes automatically.
 *
 * @param result  Outcome of the connection attempt.
 */
void wifi_popup_notify_result(WifiPopupConnectResult result);

/**
 * @brief Display the currently connected network name in the popup.
 *
 * Updates the SSID label. Pass @c NULL to clear the field.
 *
 * @param ssid  Network name (null-terminated), or @c NULL.
 */
void wifi_popup_set_connected_ssid(const char* ssid);
