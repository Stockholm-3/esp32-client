#pragma once
#include "esp_err.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── Resolution
// ────────────────────────────────────────────────────────────────
#define WS7B_LCD_H_RES 1024
#define WS7B_LCD_V_RES 600

// ── I2C
// ───────────────────────────────────────────────────────────────────────
#define WS7B_I2C_NUM I2C_NUM_0
#define WS7B_I2C_SDA 8
#define WS7B_I2C_SCL 9
#define WS7B_I2C_FREQ_HZ 400000

// ── IO expander (CH32V003)
// ────────────────────────────────────────────────────
#define WS7B_IOEXP_ADDR 0x24
#define WS7B_IOEXP_REG_MODE 0x02
#define WS7B_IOEXP_REG_OUT 0x03
#define WS7B_IOEXP_REG_IN 0x04
#define WS7B_IOEXP_REG_PWM 0x05
#define WS7B_IOEXP_REG_ADC 0x06
#define WS7B_IOEXP_TP_RST 1
#define WS7B_IOEXP_LCD_BL 2
#define WS7B_IOEXP_LCD_RST 3

// ── GT911 touch
// ───────────────────────────────────────────────────────────────
#define WS7B_TOUCH_ADDR_PRIMARY 0x5D
#define WS7B_TOUCH_ADDR_SECONDARY 0x14
#define WS7B_TOUCH_INT 4

// ── RGB GPIOs (from Waveshare schematic)
// ──────────────────────────────────────
#define WS7B_LCD_PCLK 7
#define WS7B_LCD_VSYNC 3
#define WS7B_LCD_HSYNC 46
#define WS7B_LCD_DE 5
#define WS7B_LCD_DATA0 14  // B3
#define WS7B_LCD_DATA1 38  // B4
#define WS7B_LCD_DATA2 18  // B5
#define WS7B_LCD_DATA3 17  // B6
#define WS7B_LCD_DATA4 10  // B7
#define WS7B_LCD_DATA5 39  // G2
#define WS7B_LCD_DATA6 0   // G3
#define WS7B_LCD_DATA7 45  // G4
#define WS7B_LCD_DATA8 48  // G5
#define WS7B_LCD_DATA9 47  // G6
#define WS7B_LCD_DATA10 21 // G7
#define WS7B_LCD_DATA11 1  // R3
#define WS7B_LCD_DATA12 2  // R4
#define WS7B_LCD_DATA13 42 // R5
#define WS7B_LCD_DATA14 41 // R6
#define WS7B_LCD_DATA15 40 // R7

// ── RGB timing (from Waveshare demo) ─────────────────────────────────────────
#define WS7B_PCLK_HZ 30850000
#define WS7B_HSYNC_PULSE_WIDTH 162
#define WS7B_HSYNC_BACK_PORCH 152
#define WS7B_HSYNC_FRONT_PORCH 48
#define WS7B_VSYNC_PULSE_WIDTH 45
#define WS7B_VSYNC_BACK_PORCH 13
#define WS7B_VSYNC_FRONT_PORCH 3
// Bounce buffer: a few lines of internal SRAM used by DMA, keeps PSRAM
// framebuffer fed without stalling the pixel clock.
#define WS7B_BOUNCE_BUF_LINES 10

// ── LVGL task / tick config
// ───────────────────────────────────────────────────
#define WS7B_LVGL_TICK_MS 2
#define WS7B_LVGL_TASK_STACK (12 * 1024)
#define WS7B_LVGL_TASK_PRIORITY 2
#define WS7B_LVGL_TASK_MAX_DELAY_MS 500
#define WS7B_LVGL_TASK_MIN_DELAY_MS 10

/**
 * @brief Initialise the board: I2C, IO expander, RGB panel, GT911, LVGL.
 *        Must be called once from app_main before any LVGL use.
 *
 * @param disp_out  receives the lv_disp_t* (may be NULL)
 * @param touch_out receives the lv_indev_t* (may be NULL)
 */
esp_err_t ws7b_board_init(lv_display_t** disp_out, lv_indev_t** touch_out);

/** Returns idle CPU percentage (0-100) for LVGL SYSMON. */
uint32_t ws7b_get_idle_percent(void);

/** @brief Set backlight brightness 0-255 via CH32V003 PWM */
void ws7b_set_backlight(uint8_t brightness);

/** @brief Take LVGL mutex before calling any lv_* functions */
bool ws7b_lvgl_lock(int timeout_ms);

/** @brief Release LVGL mutex */
void ws7b_lvgl_unlock(void);

#ifdef __cplusplus
}
#endif
