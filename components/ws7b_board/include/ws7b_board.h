#ifndef WS7B_BOARD_H
#define WS7B_BOARD_H

#include "esp_err.h"
#include "sdkconfig.h"

/*
 * Note: We guard hardware-specific includes and types to ensure
 * the header remains compilable on Linux/Simulator targets.
 */
#ifndef CONFIG_IDF_TARGET_LINUX
#    include "driver/i2c_master.h"
#    include "esp_lcd_panel_ops.h"
#    include "esp_lcd_touch.h"
#else
/**
 * @brief Mock handles for Linux/Simulator build.
 */
typedef void* esp_lcd_panel_handle_t;
typedef void* esp_lcd_touch_handle_t;
typedef void* i2c_master_bus_handle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise all hardware on the Waveshare 7" ESP32-S3 board.
 *
 * @return ESP_OK on success, propagated ESP error on any failure.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline esp_err_t ws7b_board_init(void) { return ESP_OK; }
#else
esp_err_t ws7b_board_init(void);
#endif

/**
 * @brief Return the RGB panel handle obtained during ws7b_board_init().
 *
 * @return Valid handle after ws7b_board_init(), NULL otherwise.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline esp_lcd_panel_handle_t ws7b_board_get_panel(void) { return (void*)0; }
#else
esp_lcd_panel_handle_t ws7b_board_get_panel(void);
#endif

/**
 * @brief Return the GT911 touch handle obtained during ws7b_board_init().
 *
 * @return Valid handle if a GT911 was found, NULL otherwise.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline esp_lcd_touch_handle_t ws7b_board_get_touch(void) { return (void*)0; }
#else
esp_lcd_touch_handle_t ws7b_board_get_touch(void);
#endif

/**
 * @brief Set the LCD backlight brightness via the IO expander PWM register.
 *
 * @param brightness  0 = off, 255 = full brightness.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline esp_err_t ws7b_board_set_backlight(uint8_t brightness) {
    (void)brightness;
    return ESP_OK;
}
#else
esp_err_t ws7b_board_set_backlight(uint8_t brightness);
#endif

/**
 * @brief Return the shared I2C master bus handle.
 *
 * @return Valid bus handle after ws7b_board_init(), NULL otherwise.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline i2c_master_bus_handle_t ws7b_board_get_i2c_bus(void) { return (void*)0; }
#else
i2c_master_bus_handle_t ws7b_board_get_i2c_bus(void);
#endif

/**
 * @brief Attempt to recover the I2C bus after a GT911 communication failure.
 *
 * Sends 9 clock pulses to release a stuck SDA line, then a STOP condition.
 * Call this when esp_lcd_touch_read_data() returns an error.
 *
 * @return ESP_OK on success, or an I2C error code.
 */
#ifdef CONFIG_IDF_TARGET_LINUX
static inline esp_err_t ws7b_board_recover_touch(void) { return ESP_OK; }
#else
esp_err_t ws7b_board_recover_touch(void);
#endif

#ifdef __cplusplus
}
#endif

#endif // WS7B_BOARD_H
