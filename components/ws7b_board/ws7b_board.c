/*
 * ws7b_board.c — Hardware bring-up for the Waveshare 7" ESP32-S3 board.
 *
 * This component owns everything that is specific to the physical board:
 *   - Shared I2C master bus
 *   - CH32V003 IO expander (reset sequencing, backlight, GPIO control)
 *   - GT911 capacitive touch controller
 *   - ESP32-S3 RGB LCD panel (double PSRAM framebuffers + DMA bounce buffer)
 */

#include "ws7b_board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdint.h>
#include <string.h>

static const char* g_tag = "ws7b_board";

static i2c_master_bus_handle_t g_s_i2c_bus   = NULL;
static i2c_master_dev_handle_t g_s_ioexp_dev = NULL;
static esp_lcd_panel_handle_t g_s_panel      = NULL;
static esp_lcd_touch_handle_t g_s_touch      = NULL;

/*
 * Shadow register for the IO expander output latch.  All pins default high
 * so that peripherals are held in a known de-asserted state before we
 * explicitly drive any line.
 */
static uint8_t g_s_ioexp_out = 0xFF;

static esp_err_t ioexp_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(g_s_ioexp_dev, buf, sizeof(buf), pdMS_TO_TICKS(100));
}

static void ioexp_set_pin(uint8_t pin, uint8_t level) {
    if (level) {
        g_s_ioexp_out |= (uint8_t)(1U << pin);
    } else {
        g_s_ioexp_out &= (uint8_t)(~(1U << pin));
    }
    ioexp_write(WS7B_IOEXP_REG_OUT, g_s_ioexp_out);
}

/*
 * Initialise the CH32V003 IO expander, then perform the LCD and touch
 * controller reset sequences that are gated through it.
 *
 * Reset sequence details:
 *   LCD  — assert RST low for 20 ms, release, wait 120 ms for panel ready.
 *   GT911 — INT must be driven low before TP_RST is released so the
 *            controller latches I2C address 0x5D (primary address).
 *            After reset, INT is reconfigured as an input with pull-up.
 */
static esp_err_t init_ioexp(void) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = WS7B_IOEXP_ADDR,
        .scl_speed_hz    = WS7B_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(g_s_i2c_bus, &dev_cfg, &g_s_ioexp_dev), g_tag,
                        "IO expander: add device failed");

    /* All pins → outputs */
    ESP_RETURN_ON_ERROR(ioexp_write(WS7B_IOEXP_REG_MODE, 0xFF), g_tag,
                        "IO expander: mode register write failed");

    ioexp_set_pin(WS7B_IOEXP_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    ioexp_set_pin(WS7B_IOEXP_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    gpio_config_t io_cfg = {
        .pin_bit_mask = 1ULL << WS7B_TOUCH_INT,
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), g_tag, "Touch INT gpio_config failed");
    gpio_set_level(WS7B_TOUCH_INT, 0);

    ioexp_set_pin(WS7B_IOEXP_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    ioexp_set_pin(WS7B_IOEXP_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    /* Reconfigure INT as input with pull-up for normal IRQ operation */
    io_cfg.mode       = GPIO_MODE_INPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_RETURN_ON_ERROR(gpio_config(&io_cfg), g_tag, "Touch INT input reconfigure failed");

    ESP_LOGI(g_tag, "IO expander ready (addr=0x%02X)", WS7B_IOEXP_ADDR);
    return ESP_OK;
}

/*
 * Probe both GT911 I2C addresses.  The active address depends on the state of
 * the INT pin during reset (handled above in init_ioexp); probing both makes
 * the driver robust to timing variations.
 */
static esp_err_t init_touch(void) {
    const uint8_t ADDRS[] = {WS7B_TOUCH_ADDR_PRIMARY, WS7B_TOUCH_ADDR_SECONDARY};

    for (int i = 0; i < (int)(sizeof(ADDRS) / sizeof(ADDRS[0])); i++) {
        ESP_LOGI(g_tag, "GT911: probing 0x%02X", ADDRS[i]);

        esp_lcd_panel_io_handle_t tp_io      = NULL;
        esp_lcd_panel_io_i2c_config_t io_cfg = {
            .dev_addr                    = ADDRS[i],
            .scl_speed_hz                = WS7B_I2C_FREQ_HZ,
            .control_phase_bytes         = 1,
            .dc_bit_offset               = 0,
            .lcd_cmd_bits                = 16,
            .lcd_param_bits              = 8,
            .flags.disable_control_phase = 1U,
        };

        if (esp_lcd_new_panel_io_i2c(g_s_i2c_bus, &io_cfg, &tp_io) != ESP_OK) {
            continue;
        }

        esp_lcd_touch_config_t tp_cfg = {
            .x_max            = WS7B_LCD_H_RES,
            .y_max            = WS7B_LCD_V_RES,
            .rst_gpio_num     = -1,
            .int_gpio_num     = -1,
            .levels.reset     = 0,
            .levels.interrupt = 0,
            .flags.swap_xy    = 0,
            .flags.mirror_x   = 0,
            .flags.mirror_y   = 0,
        };

        if (esp_lcd_touch_new_i2c_gt911(tp_io, &tp_cfg, &g_s_touch) == ESP_OK) {
            ESP_LOGI(g_tag, "GT911 found at 0x%02X", ADDRS[i]);
            return ESP_OK;
        }

        esp_lcd_panel_io_del(tp_io);
    }

    ESP_LOGE(g_tag, "GT911 not found on I2C bus");
    return ESP_ERR_NOT_FOUND;
}

/*
 * Allocate two full-screen PSRAM framebuffers (double-buffering) and a small
 * internal-SRAM DMA bounce buffer.  The bounce buffer keeps the pixel clock
 * fed during PSRAM access latency spikes.
 */
static esp_err_t init_rgb_panel(void) {
    /* NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization) — zero-init is correct here */
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src    = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .num_fbs    = 2,
        /* Cast to size_t before multiplying to avoid implicit int widening */
        .bounce_buffer_size_px = (size_t)WS7B_BOUNCE_BUF_LINES * WS7B_LCD_H_RES,
        .pclk_gpio_num         = WS7B_LCD_PCLK,
        .vsync_gpio_num        = WS7B_LCD_VSYNC,
        .hsync_gpio_num        = WS7B_LCD_HSYNC,
        .de_gpio_num           = WS7B_LCD_DE,
        .disp_gpio_num         = -1,
        .data_gpio_nums =
            {
                WS7B_LCD_DATA0,
                WS7B_LCD_DATA1,
                WS7B_LCD_DATA2,
                WS7B_LCD_DATA3,
                WS7B_LCD_DATA4,
                WS7B_LCD_DATA5,
                WS7B_LCD_DATA6,
                WS7B_LCD_DATA7,
                WS7B_LCD_DATA8,
                WS7B_LCD_DATA9,
                WS7B_LCD_DATA10,
                WS7B_LCD_DATA11,
                WS7B_LCD_DATA12,
                WS7B_LCD_DATA13,
                WS7B_LCD_DATA14,
                WS7B_LCD_DATA15,
            },
        .timings =
            {
                .pclk_hz               = WS7B_PCLK_HZ,
                .h_res                 = WS7B_LCD_H_RES,
                .v_res                 = WS7B_LCD_V_RES,
                .hsync_pulse_width     = WS7B_HSYNC_PULSE_WIDTH,
                .hsync_back_porch      = WS7B_HSYNC_BACK_PORCH,
                .hsync_front_porch     = WS7B_HSYNC_FRONT_PORCH,
                .vsync_pulse_width     = WS7B_VSYNC_PULSE_WIDTH,
                .vsync_back_porch      = WS7B_VSYNC_BACK_PORCH,
                .vsync_front_porch     = WS7B_VSYNC_FRONT_PORCH,
                .flags.pclk_active_neg = 1U,
            },
        .flags.fb_in_psram = 1U,
    };

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &g_s_panel), g_tag, "Panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(g_s_panel), g_tag, "Panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(g_s_panel), g_tag, "Panel init failed");

    ESP_LOGI(g_tag, "RGB panel ready (%dx%d, 2x PSRAM framebuffers)", WS7B_LCD_H_RES,
             WS7B_LCD_V_RES);
    return ESP_OK;
}

/* NOLINTNEXTLINE(readability-function-cognitive-complexity) */
esp_err_t ws7b_board_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port                     = WS7B_I2C_NUM,
        .sda_io_num                   = WS7B_I2C_SDA,
        .scl_io_num                   = WS7B_I2C_SCL,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = 1U,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &g_s_i2c_bus), g_tag, "I2C bus create failed");
    ESP_LOGI(g_tag, "I2C bus ready (SDA=%d SCL=%d @ %d Hz)", WS7B_I2C_SDA, WS7B_I2C_SCL,
             WS7B_I2C_FREQ_HZ);

    ESP_RETURN_ON_ERROR(init_ioexp(), g_tag, "IO expander init failed");

    ESP_RETURN_ON_ERROR(init_touch(), g_tag, "Touch init failed");

    ESP_RETURN_ON_ERROR(init_rgb_panel(), g_tag, "RGB panel init failed");

    ESP_LOGI(g_tag, "Board fully initialised");
    return ESP_OK;
}

esp_lcd_panel_handle_t ws7b_board_get_panel(void) { return g_s_panel; }

esp_lcd_touch_handle_t ws7b_board_get_touch(void) { return g_s_touch; }

void ws7b_board_set_backlight(uint8_t brightness) {
    ioexp_write(WS7B_IOEXP_REG_PWM, brightness);
    ioexp_set_pin(WS7B_IOEXP_LCD_BL, brightness > 0 ? 1 : 0);
}
