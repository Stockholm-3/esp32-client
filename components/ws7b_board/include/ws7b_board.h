#ifndef WS7B_BOARD_H
#define WS7B_BOARD_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise all hardware on the Waveshare 7" ESP32-S3 board.
 *
 * Performs the following steps in order:
 *   1. I2C master bus
 *   2. CH32V003 IO expander — configures outputs, resets LCD and touch
 *   3. GT911 touch controller — probes both known I2C addresses
 *   4. RGB LCD panel — double PSRAM framebuffers, DMA bounce buffer
 *
 * Must be called once before ws7b_board_get_panel() or
 * ws7b_board_get_touch() are used.
 *
 * @return ESP_OK on success, propagated ESP error on any failure.
 */
esp_err_t ws7b_board_init(void);

/**
 * @brief Return the RGB panel handle obtained during ws7b_board_init().
 *
 * @return Valid handle after ws7b_board_init(), NULL otherwise.
 */
esp_lcd_panel_handle_t ws7b_board_get_panel(void);

/**
 * @brief Return the GT911 touch handle obtained during ws7b_board_init().
 *
 * @return Valid handle if a GT911 was found, NULL otherwise.
 */
esp_lcd_touch_handle_t ws7b_board_get_touch(void);

/**
 * @brief Set the LCD backlight brightness via the IO expander PWM register.
 *
 * @param brightness  0 = off, 255 = full brightness.
 */
void ws7b_board_set_backlight(uint8_t brightness);

#ifdef __cplusplus
}
#endif

#endif // WS7B_BOARD_H
