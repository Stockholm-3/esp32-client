/**
 * @file ui_binder.h
 * @defgroup ui_binder UI Binder
 * @brief Two-way data-binding bridge between LVGL UI widgets and application logic.
 *
 * The UI Binder decouples the display layer from the rest of the firmware:
 *
 * - **App → UI**: call the @c ui_binder_update_* and @c ui_binder_set_*
 *   functions to push new data (sensor readings, WiFi state, etc.) into
 *   the active LVGL widgets.
 * - **UI → App**: register callbacks with the @c ui_binder_on_* functions;
 *   they are invoked whenever the user changes a setting in the UI.
 *
 * On the ESP32 target every function acquires the LVGL display mutex before
 * touching widgets. On the Linux simulator the mutex calls are omitted.
 *
 * @{
 */
#pragma once
#include <time.h>
#include "bme280_sensor.h"
#include "wifi_manager.h"

/* --------------------------------------------------------------------------
 * Initialisation
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialise the UI binder and register LVGL event handlers.
 *
 * Attaches internal event callbacks to the Settings-tab widgets
 * (location textarea, price-zone dropdown, timeout dropdown, AP switch,
 * local-web-client switch and WiFi switch). Must be called once after
 * @ref ui_build().
 */
void ui_binder_init(void);

/* --------------------------------------------------------------------------
 * Time
 * -------------------------------------------------------------------------- */

/**
 * @brief Update the local-time label in the status bar.
 *
 * @param t  Broken-down local time produced by @c localtime() / @c gmtime().
 */
void ui_binder_update_localtime(const struct tm *t);

/* --------------------------------------------------------------------------
 * Settings — load saved values into widgets
 * -------------------------------------------------------------------------- */

/**
 * @brief Set the location text area to a saved city name.
 *
 * Called by @c settings_manager on startup to restore the last known value.
 *
 * @param city  City name string (null-terminated).
 */
void ui_binder_set_location(const char *city);

/**
 * @brief Set the price-zone dropdown to a saved index.
 *
 * @param index  Zone index: 0 = SE1, 1 = SE2, 2 = SE3, 3 = SE4.
 */
void ui_binder_set_price_zone(int index);

/**
 * @brief Set the screen-timeout dropdown to a saved index.
 *
 * @param index  Timeout index: 0 = 5 min, 1 = 10 min, 2 = 15 min,
 *               3 = 20 min, 4 = never.
 */
void ui_binder_set_timeout(int index);

/**
 * @brief Set the Access Point toggle to a saved state.
 *
 * @param enabled  @c true = AP mode on, @c false = off.
 */
void ui_binder_set_ap_enabled(bool enabled);

/**
 * @brief Set the local web client toggle to a saved state.
 *
 * @param enabled  @c true = local web client enabled, @c false = disabled.
 */
void ui_binder_set_local_web_client_enabled(bool enabled);

/* --------------------------------------------------------------------------
 * Callback types
 * -------------------------------------------------------------------------- */

/**
 * @brief Callback invoked when the location text area loses focus.
 *
 * @param city  Current contents of the location text area (null-terminated).
 */
typedef void (*ui_binder_location_cb_t)(const char *city);

/**
 * @brief Callback invoked when a dropdown value changes.
 *
 * @param index  New selected index of the dropdown.
 */
typedef void (*ui_binder_dropdown_cb_t)(int index);

/**
 * @brief Callback invoked when a boolean toggle (switch) changes.
 *
 * @param enabled  New state of the switch.
 */
typedef void (*ui_binder_bool_cb_t)(bool enabled);

/**
 * @brief Callback invoked when a button is pressed (no payload).
 */
typedef void (*ui_binder_button_cb_t)(void);

/* --------------------------------------------------------------------------
 * Settings — change callbacks (UI → App)
 *
 * Each setting provides two independent registration slots (_on_* and
 * _on_*2) so that two separate subsystems can both listen to the same
 * event without coordination.
 * -------------------------------------------------------------------------- */

/**
 * @brief Register the primary callback for location changes.
 * @param cb  Handler called when the location text area loses focus.
 */
void ui_binder_on_location_changed(ui_binder_location_cb_t cb);

/**
 * @brief Register the secondary callback for location changes.
 * @param cb  Second independent handler for the same event.
 */
void ui_binder_on_location_changed2(ui_binder_location_cb_t cb);

/**
 * @brief Register the primary callback for price-zone changes.
 * @param cb  Handler called when the price-zone dropdown selection changes.
 */
void ui_binder_on_price_changed(ui_binder_dropdown_cb_t cb);

/**
 * @brief Register the secondary callback for price-zone changes.
 * @param cb  Second independent handler for the same event.
 */
void ui_binder_on_price_changed2(ui_binder_dropdown_cb_t cb);

/**
 * @brief Register the primary callback for screen-timeout changes.
 * @param cb  Handler called when the timeout dropdown selection changes.
 */
void ui_binder_on_timeout_changed(ui_binder_dropdown_cb_t cb);

/**
 * @brief Register the secondary callback for screen-timeout changes.
 * @param cb  Second independent handler for the same event.
 */
void ui_binder_on_timeout_changed2(ui_binder_dropdown_cb_t cb);

/**
 * @brief Register the primary callback for AP-mode toggle changes.
 * @param cb  Handler called when the AP switch is toggled.
 */
void ui_binder_on_ap_enabled_changed(ui_binder_bool_cb_t cb);

/**
 * @brief Register the secondary callback for AP-mode toggle changes.
 * @param cb  Second independent handler for the same event.
 */
void ui_binder_on_ap_enabled_changed2(ui_binder_bool_cb_t cb);

/**
 * @brief Register a callback for local web client toggle changes.
 * @param cb  Handler called when the local-web-client switch is toggled.
 */
void ui_binder_on_local_web_client_changed(ui_binder_bool_cb_t cb);

/**
 * @brief Register a callback for the Weather tab Refresh button.
 * @param cb  Handler called when the user taps Refresh on the weather tab.
 */
void ui_binder_on_weather_refresh(ui_binder_button_cb_t cb);

/* --------------------------------------------------------------------------
 * WiFi status
 * -------------------------------------------------------------------------- */

/**
 * @brief Update the WiFi status indicator in the status bar.
 *
 * Maps @ref WifiManagerState values to a human-readable status string and
 * colour shown in the top status bar.
 *
 * @param state  Current WiFi manager connection state.
 */
void ui_binder_update_wifi_status(WifiManagerState state);

/**
 * @brief Update the connected network name in the status bar and Settings tab.
 *
 * @param ssid  Name of the connected access point (null-terminated).
 *              Pass @c NULL or an empty string to clear the label.
 */
void ui_binder_update_wifi_name(const char *ssid);

/**
 * @brief Update the local IP address label in the Settings tab.
 *
 * @param ip  IP address string in dotted-decimal notation (null-terminated).
 */
void ui_binder_update_local_ip(const char* ip);

/* --------------------------------------------------------------------------
 * Sensor & weather data
 * -------------------------------------------------------------------------- */

/**
 * @brief Push a new BME280 reading to the indoor-sensor gauges.
 *
 * Updates the temperature, pressure and humidity arcs and their value
 * labels on the Home tab.
 *
 * @param reading  Pointer to a populated @ref Bme280Reading structure.
 */
void ui_binder_update_bme280(const Bme280Reading *reading);

/**
 * @brief Pass a raw weather API JSON response to the Weather tab.
 *
 * The JSON is parsed inside @c ui_tab_weather_handle_server_response().
 * The buffer does not need to remain valid after this call returns.
 *
 * @param json  Pointer to the JSON payload.
 * @param len   Length of the JSON buffer in bytes.
 */
void ui_binder_update_weather(const char* json, size_t len);

/**
 * @brief Pass a raw electricity price JSON response to the Elpris tab.
 *
 * @param json  Pointer to the JSON payload.
 * @param len   Length of the JSON buffer in bytes.
 */
void ui_binder_update_elpris(const char* json, size_t len);

/**
 * @brief Pass minute-resolution weather JSON to the Weather tab.
 *
 * @param json  Pointer to the JSON payload.
 * @param len   Length of the JSON buffer in bytes.
 */
void ui_binder_update_weather_min(const char* json, size_t len);

/**
 * @brief Pass hourly weather JSON to the Weather tab.
 *
 * @param json  Pointer to the JSON payload.
 * @param len   Length of the JSON buffer in bytes.
 */
void ui_binder_update_weather_hr(const char* json, size_t len);

/**
 * @brief Programmatically trigger the weather-refresh callback.
 *
 * Behaves identically to the user pressing the Refresh button on the
 * Weather tab. Used by the simulator to force an initial data load.
 */
void ui_binder_trigger_weather_refresh(void);

/** @} */
