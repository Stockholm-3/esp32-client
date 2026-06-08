
#pragma once

#include "bme280_sensor.h"
#include "wifi_manager.h"

/**
 * @brief Initializes the trigger manager and creates the internal event queue.
 *        Must be called once before trigger_manager_process().
 */
void trigger_manager_init(void);

/**
 * @brief Callback for BME280 sensor readings. Stores the reading and posts
 *        an event to the queue. Register this with bme280_sensor_init_with_task().
 *
 * @param reading  Latest sensor reading from the BME280.
 * @param ctx      Unused context pointer.
 */
void trigger_manager_on_bme280(const Bme280Reading* reading, void* ctx);

/**
 * @brief Callback for WiFi state changes. Posts EVENT_WIFI_CONNECTED or
 *        EVENT_WIFI_LOST to the queue. Register this with wifi_manager_register_callback().
 *
 * @param state   Current WiFi manager state.
 * @param reason  Failure reason (unused).
 */
void trigger_manager_on_wifi_state(WifiManagerState state, WifiManagerFailReason reason);

/**
 * @brief Stores the WiFi network name so it can be shown in the UI when
 *        the connection event fires.
 *
 * @param ssid  Null-terminated network name (max 32 characters).
 */
void trigger_manager_set_wifi_name(const char* ssid);

/**
 * @brief Processes one pending event from the queue and updates the UI.
 *        Call this every iteration of the main loop.
 */
void trigger_manager_process(void);
