/**
 * @file console_cli.c
 * @brief Interactive CLI over UART0 (WCH/CP210x bridge, "UART" port).
 *
 * Normal workflow (USB port):
 *   idf.py flash          — flash firmware
 *   idf.py monitor        — view ESP_LOG output
 *   make fm               — flash + monitor
 *
 * CLI workflow (UART port, optional):
 *   make cli              — interactive shell, clean, no log noise
 *
 * sdkconfig required:
 *   CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y   — logs + flash on USB
 *   CONFIG_ESP_CONSOLE_SECONDARY_NONE=y
 */

#include "console_cli.h"

#include "bme280_sensor.h"
#include "display.h"
#include "driver/uart.h"
#include "esp_chip_info.h"
#include "esp_console.h"
#include "esp_flash.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "screen_timeout.h"
#include "ws7b_board.h"

#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static const char* g_tag = "console_cli";

#define CLI_UART UART_NUM_0
#define CLI_BAUD 115200
#define CLI_TX_PIN 43 /* ESP32-S3 default UART0 TX */
#define CLI_RX_PIN 44 /* ESP32-S3 default UART0 RX */
#define LINE_BUF_SIZE 256
#define PROMPT "esp32-s3> "

// write helpers for uart
//
static void con_write(const char* s) { uart_write_bytes(CLI_UART, s, strlen(s)); }

static void con_writef(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void con_writef(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) {
        uart_write_bytes(CLI_UART, buf, (size_t)n);
    }
}

//
// Commands
//
static int cmd_status_handler(int argc, char** argv) {
    uint32_t heap_free   = esp_get_free_heap_size();
    uint32_t heap_min    = esp_get_minimum_free_heap_size();
    uint32_t heap_total  = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    uint32_t heap_used   = heap_total - heap_free;
    uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_free  = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_used  = psram_total - psram_free;
    uint32_t flash_total = 0;
    esp_flash_get_size(NULL, &flash_total);

    int64_t uptime_ms = esp_timer_get_time() / 1000;
    uint32_t up_h     = (uint32_t)(uptime_ms / 3600000);
    uint32_t up_m     = (uint32_t)((uptime_ms % 3600000) / 60000);
    uint32_t up_s     = (uint32_t)((uptime_ms % 60000) / 1000);

    esp_reset_reason_t reset = esp_reset_reason();
    const char* reset_str;
    switch (reset) {
    case ESP_RST_POWERON:
        reset_str = "power-on";
        break;
    case ESP_RST_SW:
        reset_str = "software";
        break;
    case ESP_RST_PANIC:
        reset_str = "panic";
        break;
    case ESP_RST_INT_WDT:
        reset_str = "interrupt wdt";
        break;
    case ESP_RST_TASK_WDT:
        reset_str = "task wdt";
        break;
    case ESP_RST_WDT:
        reset_str = "other wdt";
        break;
    case ESP_RST_BROWNOUT:
        reset_str = "brownout";
        break;
    case ESP_RST_SDIO:
        reset_str = "SDIO";
        break;
    default:
        reset_str = "unknown";
        break;
    }

    esp_chip_info_t chip;
    esp_chip_info(&chip);

    con_write("\r\n"
              "╔══════════════════════════════════════════════╗\r\n"
              "║            ESP32 SYSTEM STATUS               ║\r\n"
              "╠══════════════════════════════════════════════╣\r\n");

    con_writef("║  Chip     : ESP32-S3  rev %" PRIu32 "  (%" PRIu32 " core%s)\r\n",
               (uint32_t)chip.revision, (uint32_t)chip.cores, chip.cores == 1 ? "" : "s");
    con_writef("║  IDF      : %s\r\n", esp_get_idf_version());
    con_writef("║  Uptime   : %" PRIu32 "h %02" PRIu32 "m %02" PRIu32 "s\r\n", up_h, up_m, up_s);
    con_writef("║  Reset    : %s\r\n", reset_str);

    con_write("╠══════════════════════════════════════════════╣\r\n"
              "║  HEAP (internal)\r\n");
    con_writef("║    Total  : %7" PRIu32 " bytes\r\n", heap_total);
    con_writef("║    Used   : %7" PRIu32 " bytes  (%" PRIu32 "%%)\r\n", heap_used,
               heap_total ? heap_used * 100 / heap_total : 0);
    con_writef("║    Free   : %7" PRIu32 " bytes\r\n", heap_free);
    con_writef("║    Min    : %7" PRIu32 " bytes  (lifetime low)\r\n", heap_min);

    con_write("╠══════════════════════════════════════════════╣\r\n"
              "║  PSRAM\r\n");
    con_writef("║    Total  : %7" PRIu32 " bytes\r\n", psram_total);
    con_writef("║    Used   : %7" PRIu32 " bytes  (%" PRIu32 "%%)\r\n", psram_used,
               psram_total ? psram_used * 100 / psram_total : 0);
    con_writef("║    Free   : %7" PRIu32 " bytes\r\n", psram_free);

    con_write("╠══════════════════════════════════════════════╣\r\n");
    con_writef("║  Flash    : %7" PRIu32 " bytes\r\n", flash_total);
    con_write("╚══════════════════════════════════════════════╝\r\n\r\n");

    return 0;
}

static int cmd_sensor_reading(int argc, char** argv) {
    Bme280Reading out;
    esp_err_t err = bme280_sensor_get_last(&out);

    if (err == ESP_ERR_INVALID_STATE) {
        con_write("No reading available yet — sensor not connected or not polled.\r\n");
        return 1;
    }
    if (err != ESP_OK) {
        con_writef("Sensor error: %s\r\n", esp_err_to_name(err));
        return 1;
    }

    con_write("\r\n"
              "╔══════════════════════════════════════════════╗\r\n"
              "║           BME280 SENSOR READING              ║\r\n"
              "╠══════════════════════════════════════════════╣\r\n");
    con_writef("║  Temperature : %.2f °C\r\n", (double)out.temperature_c);
    con_writef("║  Humidity    : %.2f %%RH\r\n", (double)out.humidity_pct);
    con_writef("║  Pressure    : %.2f hPa\r\n", (double)out.pressure_hpa);
    con_write("╚══════════════════════════════════════════════╝\r\n\r\n");

    return 0;
}

static int cmd_help_handler(int argc, char** argv) {
    con_write("\r\n"
              "╔══════════════════════════════════════════════╗\r\n"
              "║               AVAILABLE COMMANDS             ║\r\n"
              "╠══════════════════════════════════════════════╣\r\n"
              "║  help            — show this message         ║\r\n"
              "║  status          — system telemetry          ║\r\n"
              "║  sensor          — BME280 reading            ║\r\n"
              "║  backlight       — set backlight 0-255       ║\r\n"
              "║  simulate_touch  — simulate touch event      ║\r\n"
              "║  test_backlight  — run backlight test (~5s)  ║\r\n"
              "║  restart         — software reset            ║\r\n"
              "╚══════════════════════════════════════════════╝\r\n\r\n");
    return 0;
}

static int cmd_test_backlight_handler(int argc, char** argv) {
    int iterations = 10;
    if (argc >= 2) {
        iterations = atoi(argv[1]);
        if (iterations <= 0) {
            con_write("Usage: test_backlight [count]  (default 10)\r\n");
            return 1;
        }
    }

    int passed = 0;
    int total  = 0;

    // TC-1: backlight on/off returns ESP_OK
    total++;
    {
        esp_err_t r1 = ws7b_board_set_backlight(0);
        esp_err_t r2 = ws7b_board_set_backlight(255);
        esp_err_t r3 = ws7b_board_set_backlight(0);
        ws7b_board_set_backlight(255);
        bool ok = (r1 == ESP_OK && r2 == ESP_OK && r3 == ESP_OK) != 0;
        con_writef("[TC-1] backlight on/off ... %s\r\n", (int)ok ? "PASS" : "FAIL");
        if (ok) {
            passed++;
        }
    }

    // TC-2: simulate_touch when screen active → woke=0
    total++;
    {
        bool woke = false;
        if (display_lvgl_lock(200)) {
            woke = screen_timeout_record_activity();
            display_lvgl_unlock();
        }
        bool ok = (!woke) != 0;
        con_writef("[TC-2] simulate_touch (active) ... %s\r\n", (int)ok ? "PASS" : "FAIL");
        if (ok) {
            passed++;
        }
    }

    // TC-3: timeout wake cycle, repeated `iterations` times
    ScreenTimeoutConfig saved;
    ScreenTimeoutConfig test_cfg = {
        .dim_timeout_seconds           = 1,
        .screensaver_timeout_seconds   = 2,
        .backlight_off_timeout_seconds = 3,
    };
    if (display_lvgl_lock(200)) {
        screen_timeout_get_config(&saved);
        screen_timeout_set_config(&test_cfg);
        display_lvgl_unlock();
    }

    for (int i = 0; i < iterations; i++) {
        total++;
        con_writef("[TC-3 iter %d/%d] waiting 5s ...", i + 1, iterations);
        vTaskDelay(pdMS_TO_TICKS(5000));

        bool woke1 = false;
        bool woke2 = true;
        if (display_lvgl_lock(200)) {
            woke1 = screen_timeout_record_activity();
            display_lvgl_unlock();
        }
        if (display_lvgl_lock(200)) {
            woke2 = screen_timeout_record_activity();
            display_lvgl_unlock();
        }

        bool ok = ((int)woke1 && !woke2) != 0;
        con_writef(" %s (woke1=%d woke2=%d)\r\n", (int)ok ? "PASS" : "FAIL", (int)woke1,
                   (int)woke2);
        if (ok) {
            passed++;
        }
    }

    if (display_lvgl_lock(200)) {
        screen_timeout_set_config(&saved);
        display_lvgl_unlock();
    }

    con_writef("\r\nResult: %d/%d passed\r\n", passed, total);
    return (passed == total) ? 0 : 1;
}

static int cmd_simulate_touch_handler(int argc, char** argv) {
    (void)argc;
    (void)argv;
    bool woke = false;
    if (display_lvgl_lock(100)) {
        woke = screen_timeout_record_activity();
        display_lvgl_unlock();
    }
    con_writef("simulate_touch: woke=%d\r\n", (int)woke ? 1 : 0);
    return 0;
}

static int cmd_backlight_handler(int argc, char** argv) {
    if (argc != 2) {
        con_write("Usage: backlight <0-255>\r\n");
        return 1;
    }
    int val = atoi(argv[1]);
    if (val < 0 || val > 255) {
        con_write("Error: value must be 0-255\r\n");
        return 1;
    }
    esp_err_t err = ws7b_board_set_backlight((uint8_t)val);
    if (err == ESP_OK) {
        con_writef("backlight(%d): OK\r\n", val);
    } else {
        con_writef("backlight(%d): FAILED err=%d (%s)\r\n", val, err, esp_err_to_name(err));
    }
    return 0;
}

static int cmd_restart_handler(int argc, char** argv) {
    con_write("Rebooting...\r\n");
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
    return 0;
}

static void console_cli_task(void* arg) {
    con_write("\r\n\r\n"
              "╔══════════════════════════════════════╗\r\n"
              "║   ESP32-S3 Interactive Console       ║\r\n"
              "║   Type 'help' for available commands ║\r\n"
              "╚══════════════════════════════════════╝\r\n\r\n" PROMPT);

    char line[LINE_BUF_SIZE];
    int idx = 0;
    uint8_t c;

    while (1) {
        int n = uart_read_bytes(CLI_UART, &c, 1, pdMS_TO_TICKS(10));
        if (n <= 0) {
            continue;
        }

        if (c >= 32 && c <= 126) {
            uart_write_bytes(CLI_UART, &c, 1);
            if (idx < LINE_BUF_SIZE - 1) {
                line[idx++] = (char)c;
            }

        } else if ((c == '\b' || c == 127) && idx > 0) {
            con_write("\b \b");
            idx--;

        } else if (c == '\r' || c == '\n') {
            con_write("\r\n");
            line[idx] = '\0';

            if (idx > 0) {
                int ret       = 0;
                esp_err_t err = esp_console_run(line, &ret);
                if (err == ESP_ERR_NOT_FOUND) {
                    con_writef("Unknown command: '%s'  (try 'help')\r\n", line);
                } else if (err == ESP_ERR_INVALID_ARG) {
                    con_write("Invalid arguments.\r\n");
                } else if (err != ESP_OK) {
                    con_writef("Error: %s\r\n", esp_err_to_name(err));
                } else if (ret != 0) {
                    con_writef("Command returned non-zero: %d\r\n", ret);
                }
            }

            idx = 0;
            con_write(PROMPT);

        } else if (c == 3) {
            con_write("^C\r\n");
            idx = 0;
            con_write(PROMPT);
        }
    }
}

void console_cli_start(void) {
    // Configure UART0 for CLI use
    const uart_config_t UART_CFG = {
        .baud_rate  = CLI_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(CLI_UART, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CLI_UART, &UART_CFG));
    ESP_ERROR_CHECK(
        uart_set_pin(CLI_UART, CLI_TX_PIN, CLI_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Command dispatcher
    esp_console_config_t cfg = ESP_CONSOLE_CONFIG_DEFAULT();
    cfg.max_cmdline_args     = 8;
    cfg.max_cmdline_length   = LINE_BUF_SIZE;
    ESP_ERROR_CHECK(esp_console_init(&cfg));
    esp_console_register_help_command();

    const esp_console_cmd_t CMDS[] = {
        {
            .command = "help",
            .help    = "List available commands",
            .hint    = NULL,
            .func    = cmd_help_handler,
        },
        {
            .command = "status",
            .help    = "Show real-time system telemetry",
            .hint    = NULL,
            .func    = cmd_status_handler,
        },
        {
            .command = "sensor",
            .help    = "Bme280 sensor reading",
            .hint    = NULL,
            .func    = cmd_sensor_reading,
        },
        {
            .command = "restart",
            .help    = "Software-reset the microcontroller",
            .hint    = NULL,
            .func    = cmd_restart_handler,
        },
        {
            .command = "backlight",
            .help    = "Set backlight brightness (0-255)",
            .hint    = "<0-255>",
            .func    = cmd_backlight_handler,
        },
        {
            .command = "simulate_touch",
            .help    = "Simulate a touch event (for automated testing)",
            .hint    = NULL,
            .func    = cmd_simulate_touch_handler,
        },
        {
            .command = "test_backlight",
            .help    = "Run backlight test suite (default 10 cycles)",
            .hint    = "[count]",
            .func    = cmd_test_backlight_handler,
        },
    };
    for (size_t i = 0; i < sizeof(CMDS) / sizeof(CMDS[0]); i++) {
        ESP_ERROR_CHECK(esp_console_cmd_register(&CMDS[i]));
    }

    xTaskCreate(console_cli_task, "console_cli", 4096, NULL, 3, NULL);
    ESP_LOGI(g_tag, "CLI ready on UART port | logs on USB port");
}
