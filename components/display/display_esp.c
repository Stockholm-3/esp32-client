/*
 * display_esp.c — vsync-locked flush, no flicker
 */

#include "display.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char* g_tag = "ws7b";

// ── Hardware handles
// ──────────────────────────────────────────────────────────
static i2c_master_bus_handle_t g_s_i2c_bus   = NULL;
static i2c_master_dev_handle_t g_s_ioexp_dev = NULL;
static esp_lcd_panel_handle_t g_s_panel      = NULL;
static esp_lcd_touch_handle_t g_s_touch      = NULL;

// ── LVGL state
// ────────────────────────────────────────────────────────────────
static SemaphoreHandle_t g_s_lvgl_mux  = NULL;
static SemaphoreHandle_t g_s_vsync_sem = NULL;

// ── Inactivity / screensaver state
// ─────────────────────────────────────────────────────────
static void (*g_s_activity_cb)(void) = NULL;

void display_set_activity_callback(void (*cb)(void)) { g_s_activity_cb = cb; }

// ── IO expander helpers
// ───────────────────────────────────────────────────────
static uint8_t g_s_io_state = 0xFF;

static esp_err_t ioexp_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(g_s_ioexp_dev, buf, 2, pdMS_TO_TICKS(100));
}

static void ioexp_set_pin(uint8_t pin, uint8_t level) {
    if (level) {
        g_s_io_state |= (1U << pin);
    } else {
        g_s_io_state &= ~(1U << pin);
    }
    ioexp_write(WS7B_IOEXP_REG_OUT, g_s_io_state);
}

void display_set_backlight(uint8_t brightness) {
    ioexp_write(WS7B_IOEXP_REG_PWM, brightness);
    ioexp_set_pin(WS7B_IOEXP_LCD_BL, brightness > 0 ? 1 : 0);
}

// ── Frame-complete ISR callback
// ───────────────────────────────────────────────
static IRAM_ATTR bool on_frame_buf_complete(esp_lcd_panel_handle_t panel,
                                            const esp_lcd_rgb_panel_event_data_t* edata,
                                            void* user_ctx) {
    BaseType_t need_yield = pdFALSE;
    xSemaphoreGiveFromISR(g_s_vsync_sem, &need_yield);
    return need_yield == pdTRUE;
}

// ── LVGL flush callback (double-buffer vsync swap)
// ───────────────────────────────────────────────────────
// RENDER_MODE_DIRECT calls flush_cb once per dirty region per frame.
// Only the final call (lv_display_flush_is_last) should trigger the
// full-screen zero-copy buffer swap and vsync wait; earlier calls just
// confirm the region is done so LVGL can continue rendering.
static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    (void)area;
    if (lv_display_flush_is_last(disp)) {
        esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);
        esp_lcd_panel_draw_bitmap(panel, 0, 0, WS7B_LCD_H_RES, WS7B_LCD_V_RES, px_map);
        xSemaphoreTake(g_s_vsync_sem, 0);
        xSemaphoreTake(g_s_vsync_sem, portMAX_DELAY);
    }
    lv_display_flush_ready(disp);
}

#if CONFIG_WS7B_QEMU_SIM
static void lvgl_flush_cb_qemu(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}
#endif

// ── Touch input callback
// ──────────────────────────────────────────────────────
static void lvgl_touch_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    esp_lcd_touch_handle_t tp            = (esp_lcd_touch_handle_t)lv_indev_get_user_data(indev);
    uint8_t cnt                          = 0;
    esp_lcd_touch_point_data_t points[1] = {0};

    esp_lcd_touch_read_data(tp);
    esp_lcd_touch_get_data(tp, points, &cnt, 1);
    if (cnt > 0) {
        if (g_s_activity_cb) {
            g_s_activity_cb();
        }
        data->point.x = (int32_t)points[0].x;
        data->point.y = (int32_t)points[0].y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ── LVGL tick timer
// ───────────────────────────────────────────────────────────
static void lvgl_tick_cb(void* arg) { lv_tick_inc(WS7B_LVGL_TICK_MS); }

// ── LVGL task
// ─────────────────────────────────────────────────────────────────
static void lvgl_task(void* arg) {
    ESP_LOGI(g_tag, "LVGL task started");
    uint32_t delay_ms = WS7B_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        if (display_lvgl_lock(-1)) {
            delay_ms = lv_timer_handler();
            display_lvgl_unlock();
        }
        if (delay_ms > WS7B_LVGL_TASK_MAX_DELAY_MS) {
            delay_ms = WS7B_LVGL_TASK_MAX_DELAY_MS;
        }
        if (delay_ms < WS7B_LVGL_TASK_MIN_DELAY_MS) {
            delay_ms = WS7B_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

// ── I2C + IO expander init
// ────────────────────────────────────────────────────
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static esp_err_t init_i2c_and_ioexp(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port                     = WS7B_I2C_NUM,
        .sda_io_num                   = WS7B_I2C_SDA,
        .scl_io_num                   = WS7B_I2C_SCL,
        .clk_source                   = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = 1U,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &g_s_i2c_bus), g_tag, "I2C bus failed");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = WS7B_IOEXP_ADDR,
        .scl_speed_hz    = WS7B_I2C_FREQ_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(g_s_i2c_bus, &dev_cfg, &g_s_ioexp_dev), g_tag,
                        "ioexp add failed");

    ESP_RETURN_ON_ERROR(ioexp_write(WS7B_IOEXP_REG_MODE, 0xFF), g_tag, "ioexp mode failed");

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
    gpio_config(&io_cfg);
    gpio_set_level(WS7B_TOUCH_INT, 0);

    ioexp_set_pin(WS7B_IOEXP_TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    ioexp_set_pin(WS7B_IOEXP_TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    io_cfg.mode       = GPIO_MODE_INPUT;
    io_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_cfg);

    ESP_LOGI(g_tag, "IO expander initialised");
    return ESP_OK;
}

// ── Touch init
// ────────────────────────────────────────────────────────────────
static esp_err_t init_touch(void) {
    uint8_t addrs[] = {WS7B_TOUCH_ADDR_PRIMARY, WS7B_TOUCH_ADDR_SECONDARY};
    for (int i = 0; i < 2; i++) {
        ESP_LOGI(g_tag, "Trying GT911 at 0x%02X", addrs[i]);

        esp_lcd_panel_io_handle_t tp_io      = NULL;
        esp_lcd_panel_io_i2c_config_t io_cfg = {
            .dev_addr                    = addrs[i],
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
            ESP_LOGI(g_tag, "GT911 found at 0x%02X", addrs[i]);
            return ESP_OK;
        }
        esp_lcd_panel_io_del(tp_io);
    }
    ESP_LOGE(g_tag, "GT911 not found on I2C bus");
    return ESP_ERR_NOT_FOUND;
}

// ── RGB panel init
// ────────────────────────────────────────────────────────────
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
static esp_err_t init_rgb_panel(void) {
    // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization) -- ESP-IDF struct, zero-init is
    // correct
    esp_lcd_rgb_panel_config_t cfg = {
        .clk_src    = LCD_CLK_SRC_DEFAULT,
        .data_width = 16,
        .num_fbs    = 2,
        // Cast to size_t before multiplying to avoid implicit widening from int.
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

    ESP_RETURN_ON_ERROR(esp_lcd_new_rgb_panel(&cfg, &g_s_panel), g_tag, "panel create failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(g_s_panel), g_tag, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(g_s_panel), g_tag, "panel init failed");
    ESP_LOGI(g_tag, "RGB panel %dx%d ready", WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    return ESP_OK;
}

// ── LVGL display init
// ─────────────────────────────────────────────────────────
static lv_display_t* lvgl_display_init(void) {
    lv_display_t* disp = lv_display_create(WS7B_LCD_H_RES, WS7B_LCD_V_RES);
    assert(disp);

#if CONFIG_WS7B_QEMU_SIM
    static uint8_t qemu_buf[WS7B_LCD_H_RES * 10 * sizeof(lv_color_t)];
    lv_display_set_buffers(disp, qemu_buf, NULL, sizeof(qemu_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb_qemu);
#else
    // Two full-size framebuffers owned by the RGB panel driver (in PSRAM).
    // DIRECT mode lets LVGL render straight into these buffers; flush_cb
    // triggers a zero-copy vsync-locked swap instead of a memcpy.
    void* buf1 = NULL;
    void* buf2 = NULL;
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(g_s_panel, 2, &buf1, &buf2));
    size_t fb_bytes = (size_t)WS7B_LCD_H_RES * WS7B_LCD_V_RES * sizeof(lv_color_t);
    lv_display_set_buffers(disp, buf1, buf2, fb_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_user_data(disp, g_s_panel);
#endif

    return disp;
}

// ── Touch indev init
// ──────────────────────────────────────────────────────────
static lv_indev_t* lvgl_touch_init(void) {
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);
    lv_indev_set_user_data(indev, g_s_touch);
    return indev;
}

// ── Public init
// ───────────────────────────────────────────────────────────────
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
esp_err_t display_init(lv_display_t** disp_out, lv_indev_t** touch_out) {
#if CONFIG_WS7B_QEMU_SIM
    ESP_LOGW(TAG, "QEMU simulation mode — hardware init skipped");
#else
    ESP_RETURN_ON_ERROR(init_i2c_and_ioexp(), g_tag, "ioexp init failed");
    ESP_RETURN_ON_ERROR(init_touch(), g_tag, "touch init failed");
    ESP_RETURN_ON_ERROR(init_rgb_panel(), g_tag, "panel init failed");

    ESP_LOGI(g_tag, "creating vsync semaphore");
    g_s_vsync_sem = xSemaphoreCreateBinary();
    assert(g_s_vsync_sem);

    ESP_LOGI(g_tag, "registering panel callbacks");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_frame_buf_complete = on_frame_buf_complete,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(g_s_panel, &cbs, NULL));
#endif

    ESP_LOGI(g_tag, "calling lv_init");
    lv_init();
    ESP_LOGI(g_tag, "lv_init done");

    const esp_timer_create_args_t TICK_ARGS = {
        .callback = lvgl_tick_cb,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&TICK_ARGS, &tick_timer));
    // Cast to uint64_t before multiplying to avoid implicit widening from int.
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, (uint64_t)WS7B_LVGL_TICK_MS * 1000U));
    ESP_LOGI(g_tag, "tick timer started");

    g_s_lvgl_mux = xSemaphoreCreateRecursiveMutex();
    assert(g_s_lvgl_mux);
    ESP_LOGI(g_tag, "lvgl mutex created");

    lv_display_t* disp = lvgl_display_init();
    assert(disp);
    ESP_LOGI(g_tag, "display driver registered");

    lv_indev_t* indev = NULL;
    if (g_s_touch) {
        indev = lvgl_touch_init();
        ESP_LOGI(g_tag, "touch driver registered");
    }

    ESP_LOGI(g_tag, "creating LVGL task");
    if (xTaskCreatePinnedToCore(lvgl_task, "lvgl", WS7B_LVGL_TASK_STACK, NULL,
                                WS7B_LVGL_TASK_PRIORITY, NULL, tskNO_AFFINITY) != pdPASS) {
        ESP_LOGE(g_tag, "LVGL task create failed");
        return ESP_FAIL;
    }

#if !CONFIG_WS7B_QEMU_SIM
    display_set_backlight(255);
    ESP_LOGI(g_tag, "backlight on");
#endif

    if (disp_out) {
        *disp_out = disp;
    }
    if (touch_out) {
        *touch_out = indev;
    }

    ESP_LOGI(g_tag, "Board fully initialised");
    return ESP_OK;
}

// ── Mutex helpers
// ─────────────────────────────────────────────────────────────
bool display_lvgl_lock(int timeout_ms) {
    assert(g_s_lvgl_mux);
    TickType_t ticks = timeout_ms < 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(g_s_lvgl_mux, ticks) == pdTRUE;
}

void display_lvgl_unlock(void) {
    assert(g_s_lvgl_mux);
    xSemaphoreGiveRecursive(g_s_lvgl_mux);
}
