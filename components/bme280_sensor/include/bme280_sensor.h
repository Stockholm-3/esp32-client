/**
 * @file bme280_sensor.h
 * @brief BME280 environmental sensor module for ESP-IDF.
 *
 * Communicates directly with the BME280 over the new i2c_master API —
 * no external component dependency required.
 *
 * Design
 * ──────
 * This module is scheduling-agnostic.  It owns driver initialisation and
 * compensation, and exposes two usage models:
 *
 *   Quick-start (recommended for most projects)
 *   ─────────────────────────────────────────────
 *     bme280_sensor_init_with_task(i2c_bus, my_callback, NULL);
 *     // Done. Callback fires every 1 s with fresh data.
 *     // Call bme280_sensor_get_last() anywhere for zero-wait cached reads.
 *
 *   Model A – Caller-driven (use with a state machine, LVGL task, etc.)
 *   ───────────────────────────────────────────────────────────────────
 *     bme280_sensor_init(…);
 *     // in your loop / state handler:
 *     Bme280Reading r;
 *     bme280_sensor_read(&r);
 *
 *   Model B – Built-in FreeRTOS polling task (full control)
 *   ────────────────────────────────────────────────────────
 *     bme280_sensor_init(…);
 *     bme280_sensor_start_task(…, my_callback);
 *     // callback fires on every new sample; task runs indefinitely.
 *
 * Both models can coexist: the task calls bme280_sensor_read() internally,
 * and any caller may also call bme280_sensor_read() directly (the function
 * is re-entrant when guarded by the internal mutex).
 *
 * Thread safety
 * ─────────────
 * bme280_sensor_read() and bme280_sensor_deinit() are protected by an
 * internal FreeRTOS mutex.  It is safe to call bme280_sensor_read() from
 * multiple tasks simultaneously.
 */

#ifndef BME280_SENSOR_H
#define BME280_SENSOR_H

#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Compensated sensor reading.
 *
 * Temperature is in °C, pressure in hPa, humidity in %RH.
 * All three fields are valid when bme280_sensor_read() returns ESP_OK.
 */
typedef struct {
    float temperature_c; /**< Ambient temperature in degrees Celsius.    */
    float pressure_hpa;  /**< Barometric pressure in hectopascals (hPa). */
    float humidity_pct;  /**< Relative humidity in percent (%RH).        */
} Bme280Reading;

/**
 * @brief Callback signature for the built-in polling task.
 *
 * Called from the sensor task context after every successful read.
 * Keep the body short — do not block inside the callback.
 *
 * @param reading  Pointer to the latest compensated reading.
 * @param user_ctx User-supplied context pointer passed to
 *                 bme280_sensor_start_task().
 */
typedef void (*Bme280SampleCb)(const Bme280Reading* reading, void* user_ctx);

/**
 * @brief Sensor operating mode.
 *
 * NORMAL mode: sensor cycles autonomously at the configured standby time.
 * FORCED mode: sensor takes one measurement then returns to sleep; the
 *              driver triggers each measurement explicitly.  Lower power,
 *              higher latency.
 *
 * For most UI / dashboard applications, NORMAL mode is recommended.
 */
typedef enum {
    BME280_SENSOR_MODE_NORMAL = 0, /**< Continuous background sampling. */
    BME280_SENSOR_MODE_FORCED,     /**< One-shot, driver-triggered.      */
} Bme280SensorMode;

/**
 * @brief Initialisation configuration.
 */
typedef struct {
    i2c_master_bus_handle_t i2c_bus; /**< Shared I2C master bus handle.           */
    uint8_t i2c_addr;                /**< Device address: 0x76 (SDO low) or
                                          0x77 (SDO high).                         */
    uint32_t i2c_freq_hz;            /**< I2C clock speed (typically 400 000 Hz). */
    Bme280SensorMode mode;           /**< Sampling mode (NORMAL or FORCED).        */
} Bme280SensorCfg;

/**
 * @brief Polling task configuration, passed to bme280_sensor_start_task().
 */
typedef struct {
    uint32_t poll_interval_ms; /**< How often to read the sensor (ms).
                                    Minimum: 10 ms.                         */
    Bme280SampleCb callback;   /**< Called with each new reading.           */
    void* user_ctx;            /**< Forwarded verbatim to callback.         */
    uint32_t stack_size;       /**< Task stack in bytes (≥ 3072).           */
    UBaseType_t priority;      /**< FreeRTOS task priority.                 */
    BaseType_t core_id;        /**< Core affinity: 0, 1, or tskNO_AFFINITY. */
} Bme280TaskCfg;

#define BME280_I2C_ADDR_PRIMARY 0x76U   /**< SDO pin pulled low.  */
#define BME280_I2C_ADDR_SECONDARY 0x77U /**< SDO pin pulled high. */

#define BME280_TASK_DEFAULT_STACK_SIZE 4096U
#define BME280_TASK_DEFAULT_PRIORITY 3U
#define BME280_TASK_DEFAULT_CORE tskNO_AFFINITY
#define BME280_TASK_DEFAULT_INTERVAL_MS 1000U

/**
 * @brief Initialise the BME280 sensor.
 *
 * Attaches the device to the provided I2C bus, reads and stores calibration
 * data, and starts the sensor in the requested mode.  Must be called before
 * any other function in this module.
 *
 * @param cfg  Pointer to a fully populated Bme280SensorCfg.
 * @return     ESP_OK on success, or a propagated error code.
 */
esp_err_t bme280_sensor_init(const Bme280SensorCfg* cfg);

/**
 * @brief Read the latest compensated data from the sensor.
 *
 * In NORMAL mode the sensor registers are read directly.
 * In FORCED mode the function triggers a measurement, waits for completion,
 * then reads and compensates the result.
 *
 * This function is thread-safe.
 *
 * @param[out] out  Destination for the compensated reading.
 * @return          ESP_OK on success, ESP_ERR_INVALID_STATE if the module
 *                  has not been initialised, or an I2C error code.
 */
esp_err_t bme280_sensor_read(Bme280Reading* out);

/**
 * @brief Start the built-in FreeRTOS polling task.
 *
 * The task calls bme280_sensor_read() at the configured interval and
 * forwards each result to the user callback.  Read errors are logged but
 * do not stop the task.
 *
 * Only one polling task may run at a time.  Calling this function while a
 * task is already running returns ESP_ERR_INVALID_STATE.
 *
 * bme280_sensor_init() must have been called successfully first.
 *
 * @param task_cfg  Polling task configuration (stack, priority, interval…).
 * @return          ESP_OK on success.
 */
esp_err_t bme280_sensor_start_task(const Bme280TaskCfg* task_cfg);

/**
 * @brief Stop the built-in polling task (if running).
 *
 * Blocks until the task has exited.  Safe to call even if the task is not
 * running (returns ESP_OK immediately).
 *
 * @return ESP_OK always.
 */
esp_err_t bme280_sensor_stop_task(void);

/**
 * @brief Return true if the polling task is currently running.
 */
bool bme280_sensor_task_running(void);

/**
 * @brief Return a snapshot of the last successful reading.
 *
 * Does not communicate with the sensor.  Returns ESP_ERR_INVALID_STATE if
 * no successful read has occurred yet.
 *
 * Useful for low-priority UI updates that do not need to block on I2C.
 *
 * @param[out] out  Destination for the cached reading.
 * @return          ESP_OK or ESP_ERR_INVALID_STATE.
 */
esp_err_t bme280_sensor_get_last(Bme280Reading* out);

/**
 * @brief Quick-start helper: init + Normal mode + polling task in one call.
 *
 * Equivalent to calling bme280_sensor_init() followed by
 * bme280_sensor_start_task() with all defaults:
 *
 *   - I2C address : auto-probed (0x76 tried first, then 0x77)
 *   - Mode        : NORMAL (continuous background sampling)
 *   - Interval    : BME280_TASK_DEFAULT_INTERVAL_MS (1 000 ms)
 *   - Stack       : BME280_TASK_DEFAULT_STACK_SIZE  (4 096 bytes)
 *   - Priority    : BME280_TASK_DEFAULT_PRIORITY    (3)
 *   - Core        : tskNO_AFFINITY
 *
 * After this returns ESP_OK, the polling task is running.  Call
 * bme280_sensor_get_last() from anywhere for a zero-wait cached read.
 *
 * @param i2c_bus   Shared I2C master bus handle.
 * @param callback  Called with each new reading — must not be NULL.
 * @param user_ctx  Forwarded verbatim to every callback invocation.
 * @return          ESP_OK on success, propagated error otherwise.
 */
esp_err_t bme280_sensor_init_with_task(i2c_master_bus_handle_t i2c_bus, Bme280SampleCb callback,
                                       void* user_ctx);

/**
 * @brief De-initialise the module and release all resources.
 *
 * Stops the polling task if running, releases the I2C device, and destroys
 * the internal mutex.  After this call, bme280_sensor_init() may be called
 * again.
 *
 * @return ESP_OK always.
 */
esp_err_t bme280_sensor_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* BME280_SENSOR_H */
