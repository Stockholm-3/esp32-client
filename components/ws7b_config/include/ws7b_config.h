#ifndef WS7B_CONFIG_H
#define WS7B_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── Resolution ───────────────────────────────────────────────────────────── */
#define WS7B_LCD_H_RES 1024
#define WS7B_LCD_V_RES 600

/* ── I2C ──────────────────────────────────────────────────────────────────── */
#define WS7B_I2C_NUM I2C_NUM_0
#define WS7B_I2C_SDA 8
#define WS7B_I2C_SCL 9
#define WS7B_I2C_FREQ_HZ 400000

/* ── IO expander (CH32V003) ───────────────────────────────────────────────── */
#define WS7B_IOEXP_ADDR 0x24
#define WS7B_IOEXP_REG_MODE 0x02
#define WS7B_IOEXP_REG_OUT 0x03
#define WS7B_IOEXP_REG_IN 0x04
#define WS7B_IOEXP_REG_PWM 0x05
#define WS7B_IOEXP_REG_ADC 0x06
#define WS7B_IOEXP_TP_RST 1
#define WS7B_IOEXP_LCD_BL 2
#define WS7B_IOEXP_LCD_RST 3

/* ── GT911 touch ──────────────────────────────────────────────────────────── */
#define WS7B_TOUCH_ADDR_PRIMARY 0x5D
#define WS7B_TOUCH_ADDR_SECONDARY 0x14
#define WS7B_TOUCH_INT 4

/* ── RGB GPIOs (from Waveshare schematic) ────────────────────────────────── */
#define WS7B_LCD_PCLK 7
#define WS7B_LCD_VSYNC 3
#define WS7B_LCD_HSYNC 46
#define WS7B_LCD_DE 5
#define WS7B_LCD_DATA0 14  /* B3 */
#define WS7B_LCD_DATA1 38  /* B4 */
#define WS7B_LCD_DATA2 18  /* B5 */
#define WS7B_LCD_DATA3 17  /* B6 */
#define WS7B_LCD_DATA4 10  /* B7 */
#define WS7B_LCD_DATA5 39  /* G2 */
#define WS7B_LCD_DATA6 0   /* G3 */
#define WS7B_LCD_DATA7 45  /* G4 */
#define WS7B_LCD_DATA8 48  /* G5 */
#define WS7B_LCD_DATA9 47  /* G6 */
#define WS7B_LCD_DATA10 21 /* G7 */
#define WS7B_LCD_DATA11 1  /* R3 */
#define WS7B_LCD_DATA12 2  /* R4 */
#define WS7B_LCD_DATA13 42 /* R5 */
#define WS7B_LCD_DATA14 41 /* R6 */
#define WS7B_LCD_DATA15 40 /* R7 */

/* ── RGB timing (from Waveshare demo) ────────────────────────────────────── */
#define WS7B_PCLK_HZ 30850000
#define WS7B_HSYNC_PULSE_WIDTH 162
#define WS7B_HSYNC_BACK_PORCH 152
#define WS7B_HSYNC_FRONT_PORCH 48
#define WS7B_VSYNC_PULSE_WIDTH 45
#define WS7B_VSYNC_BACK_PORCH 13
#define WS7B_VSYNC_FRONT_PORCH 3

/*
 * Bounce buffer: a small block of internal SRAM used by the DMA engine to
 * feed the PSRAM framebuffer without stalling the pixel clock.
 */
#define WS7B_BOUNCE_BUF_LINES 10

/* ── LVGL task / tick ────────────────────────────────────────────────────── */
#define WS7B_LVGL_TICK_MS 2
#define WS7B_LVGL_TASK_STACK (12 * 1024)
#define WS7B_LVGL_TASK_PRIORITY 2
#define WS7B_LVGL_TASK_MAX_DELAY_MS 500
#define WS7B_LVGL_TASK_MIN_DELAY_MS 10

#define WS7B_LVGL_TASK_CORE 0

#ifdef __cplusplus
}
#endif

#endif // WS7B_CONFIG_H
