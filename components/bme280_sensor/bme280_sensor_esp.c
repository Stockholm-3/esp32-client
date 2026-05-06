/**
 * @file bme280_sensor.c
 * @brief BME280 environmental sensor module — implementation.
 *
 * Communicates directly with the BME280 via the ESP-IDF i2c_master API.
 * No external component dependency.  Compensation formulas are taken
 * verbatim from the BME280 datasheet (BST-BME280-DS002, section 8.2).
 */

#include "bme280_sensor.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <string.h>

static const char* g_tag = "bme280_sensor";

/* ── BME280 register addresses ───────────────────────────────────────────── */
#define REG_ID 0xD0U /**< Chip ID — always reads 0x60 on BME280. */
#define REG_RESET 0xE0U
#define REG_CTRL_HUM 0xF2U  /**< Humidity oversampling.                  */
#define REG_STATUS 0xF3U    /**< Bit 3: measuring; bit 0: im_update.     */
#define REG_CTRL_MEAS 0xF4U /**< Temp/pressure oversampling + mode.      */
#define REG_CONFIG 0xF5U    /**< Standby time, IIR filter, SPI mode.     */
#define REG_DATA 0xF7U      /**< Burst-read start: press/temp/hum (8 B). */
#define REG_CALIB_00 0x88U  /**< T/P calibration bytes start (26 bytes). */
#define REG_CALIB_26 0xE1U  /**< H calibration bytes start (7 bytes).   */

/* BME280 chip ID */
#define BME280_CHIP_ID 0x60U

/* ctrl_meas mode bits */
#define MODE_SLEEP 0x00U
#define MODE_FORCED 0x01U /**< Write 01 or 10 to trigger one measurement. */
#define MODE_NORMAL 0x03U

/* Oversampling ×1 for all channels — good balance of noise vs. power */
#define OSRS_X1 0x01U

/*
 * ctrl_meas value: osrs_t=×1 [7:5], osrs_p=×1 [4:2], mode set separately.
 * Bits [7:2] = 0b00100100 = 0x24, mode bits [1:0] ORed in at runtime.
 */
#define CTRL_MEAS_BASE ((OSRS_X1 << 5U) | (OSRS_X1 << 2U))

/* Worst-case measurement time with ×1 oversampling (datasheet Table 13). */
#define MEAS_WAIT_MS 15U
#define I2C_TIMEOUT_MS 100U

/* ── Calibration data (stored after init) ────────────────────────────────── */

typedef struct {
    /* Temperature */
    uint16_t dig_t1;
    int16_t dig_t2;
    int16_t dig_t3;
    /* Pressure */
    uint16_t dig_p1;
    int16_t dig_p2;
    int16_t dig_p3;
    int16_t dig_p4;
    int16_t dig_p5;
    int16_t dig_p6;
    int16_t dig_p7;
    int16_t dig_p8;
    int16_t dig_p9;
    /* Humidity */
    uint8_t dig_h1;
    int16_t dig_h2;
    uint8_t dig_h3;
    int16_t dig_h4;
    int16_t dig_h5;
    int8_t dig_h6;
} Bme280Calib;

/* ── Module state ─────────────────────────────────────────────────────────── */

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_dev_handle_t i2c_dev;

    Bme280SensorMode mode;
    Bme280Calib calib;

    SemaphoreHandle_t mutex;

    TaskHandle_t task_handle;
    volatile bool task_stop_req;
    Bme280TaskCfg task_cfg;

    Bme280Reading last_reading;
    bool last_valid;
    bool initialised;
} Bme280SensorCtx;

static Bme280SensorCtx g_ctx = {0};

/* ── Low-level I2C helpers ────────────────────────────────────────────────── */

static esp_err_t reg_write(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(g_ctx.i2c_dev, buf, sizeof(buf), (int)I2C_TIMEOUT_MS);
}

static esp_err_t reg_read(uint8_t reg, uint8_t* dst, size_t len) {
    return i2c_master_transmit_receive(g_ctx.i2c_dev, &reg, 1U, dst, len, (int)I2C_TIMEOUT_MS);
}

/* ── Calibration load ─────────────────────────────────────────────────────── */

static esp_err_t load_calibration(void) {
    uint8_t c[26] = {0};
    ESP_RETURN_ON_ERROR(reg_read(REG_CALIB_00, c, sizeof(c)), g_tag,
                        "Failed to read T/P calibration");

    g_ctx.calib.dig_t1 = (uint16_t)(c[1] << 8U) | c[0];
    g_ctx.calib.dig_t2 = (int16_t)((c[3] << 8U) | c[2]);
    g_ctx.calib.dig_t3 = (int16_t)((c[5] << 8U) | c[4]);

    g_ctx.calib.dig_p1 = (uint16_t)((c[7] << 8U) | c[6]);
    g_ctx.calib.dig_p2 = (int16_t)((c[9] << 8U) | c[8]);
    g_ctx.calib.dig_p3 = (int16_t)((c[11] << 8U) | c[10]);
    g_ctx.calib.dig_p4 = (int16_t)((c[13] << 8U) | c[12]);
    g_ctx.calib.dig_p5 = (int16_t)((c[15] << 8U) | c[14]);
    g_ctx.calib.dig_p6 = (int16_t)((c[17] << 8U) | c[16]);
    g_ctx.calib.dig_p7 = (int16_t)((c[19] << 8U) | c[18]);
    g_ctx.calib.dig_p8 = (int16_t)((c[21] << 8U) | c[20]);
    g_ctx.calib.dig_p9 = (int16_t)((c[23] << 8U) | c[22]);
    /* c[24] = 0xA0 (reserved), c[25] = dig_H1 at 0xA1 */
    g_ctx.calib.dig_h1 = c[25];

    uint8_t h[7] = {0};
    ESP_RETURN_ON_ERROR(reg_read(REG_CALIB_26, h, sizeof(h)), g_tag,
                        "Failed to read H calibration");

    g_ctx.calib.dig_h2 = (int16_t)((h[1] << 8U) | h[0]);
    g_ctx.calib.dig_h3 = h[2];
    g_ctx.calib.dig_h4 = (int16_t)(((int16_t)h[3] << 4U) | (h[4] & 0x0FU));
    g_ctx.calib.dig_h5 = (int16_t)(((int16_t)h[5] << 4U) | (h[4] >> 4U));
    g_ctx.calib.dig_h6 = (int8_t)h[6];

    return ESP_OK;
}

/* ── Datasheet compensation formulas (double precision, section 8.2) ─────── */

static float compensate_temperature(int32_t adc_t, int32_t* t_fine) {
    const Bme280Calib* cal = &g_ctx.calib;
    double var1 = ((double)adc_t / 16384.0 - (double)cal->dig_t1 / 1024.0) * (double)cal->dig_t2;
    double var2 = ((double)adc_t / 131072.0 - (double)cal->dig_t1 / 8192.0) *
                  ((double)adc_t / 131072.0 - (double)cal->dig_t1 / 8192.0) * (double)cal->dig_t3;
    *t_fine     = (int32_t)(var1 + var2);
    return (float)((var1 + var2) / 5120.0);
}

static float compensate_pressure(int32_t adc_p, int32_t t_fine) {
    const Bme280Calib* cal = &g_ctx.calib;
    double var1            = ((double)t_fine / 2.0) - 64000.0;
    double var2            = var1 * var1 * (double)cal->dig_p6 / 32768.0;
    var2                   = var2 + (var1 * (double)cal->dig_p5 * 2.0);
    var2                   = (var2 / 4.0) + ((double)cal->dig_p4 * 65536.0);
    var1 = ((double)cal->dig_p3 * var1 * var1 / 524288.0 + (double)cal->dig_p2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)cal->dig_p1;
    if (var1 == 0.0) {
        return 0.0F; /* Avoid division by zero */
    }
    double pressure = 1048576.0 - (double)adc_p;
    pressure        = (pressure - var2 / 4096.0) * 6250.0 / var1;
    var1            = (double)cal->dig_p9 * pressure * pressure / 2147483648.0;
    var2            = pressure * (double)cal->dig_p8 / 32768.0;
    pressure        = pressure + ((var1 + var2 + (double)cal->dig_p7) / 16.0);
    return (float)(pressure / 100.0); /* Pa → hPa */
}

static float compensate_humidity(int32_t adc_h, int32_t t_fine) {
    const Bme280Calib* cal = &g_ctx.calib;
    double var_h           = (double)t_fine - 76800.0;
    var_h = ((double)adc_h - ((double)cal->dig_h4 * 64.0 + (double)cal->dig_h5 / 16384.0 * var_h)) *
            ((double)cal->dig_h2 / 65536.0 *
             (1.0 + (double)cal->dig_h6 / 67108864.0 * var_h *
                        (1.0 + (double)cal->dig_h3 / 67108864.0 * var_h)));
    var_h = var_h * (1.0 - (double)cal->dig_h1 * var_h / 524288.0);
    if (var_h > 100.0) {
        var_h = 100.0;
    } else if (var_h < 0.0) {
        var_h = 0.0;
    }
    return (float)var_h;
}

/* ── Forced-mode trigger + wait ───────────────────────────────────────────── */

static esp_err_t forced_measure_and_wait(void) {
    /* Trigger: write MODE_FORCED into ctrl_meas (overwrites sleep mode) */
    ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_MEAS, CTRL_MEAS_BASE | MODE_FORCED), g_tag,
                        "Failed to trigger forced measurement");

    /* Poll status register until measuring bit clears */
    const TickType_t DEADLINE = xTaskGetTickCount() + pdMS_TO_TICKS(MEAS_WAIT_MS * 4U);
    while (true) {
        if (xTaskGetTickCount() >= DEADLINE) {
            ESP_LOGE(g_tag, "Forced measurement timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        uint8_t status = 0U;
        ESP_RETURN_ON_ERROR(reg_read(REG_STATUS, &status, 1U), g_tag, "Status read failed");
        if ((status & 0x08U) == 0U) {
            break; /* measuring bit clear — data is ready */
        }
    }
    return ESP_OK;
}

/* ── Polling task ─────────────────────────────────────────────────────────── */

static void sensor_task(void* arg) {
    const Bme280TaskCfg* cfg = (const Bme280TaskCfg*)arg;
    ESP_LOGI(g_tag, "Polling task started (interval=%lu ms)", (unsigned long)cfg->poll_interval_ms);

    while (!g_ctx.task_stop_req) {
        Bme280Reading reading = {0};
        esp_err_t ret         = bme280_sensor_read(&reading);
        if (ret == ESP_OK) {
            if (cfg->callback != NULL) {
                cfg->callback(&reading, cfg->user_ctx);
            }
        } else {
            ESP_LOGW(g_tag, "Polling task read error: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(cfg->poll_interval_ms));
    }

    ESP_LOGI(g_tag, "Polling task exiting");
    g_ctx.task_handle = NULL;
    vTaskDelete(NULL);
}

/* ── Public API ───────────────────────────────────────────────────────────── */

esp_err_t bme280_sensor_init(const Bme280SensorCfg* cfg) {
    ESP_RETURN_ON_FALSE(cfg != NULL, ESP_ERR_INVALID_ARG, g_tag, "cfg is NULL");
    ESP_RETURN_ON_FALSE(cfg->i2c_bus != NULL, ESP_ERR_INVALID_ARG, g_tag, "i2c_bus is NULL");
    ESP_RETURN_ON_FALSE(!g_ctx.initialised, ESP_ERR_INVALID_STATE, g_tag,
                        "Already initialised — call bme280_sensor_deinit() first");

    memset(&g_ctx, 0, sizeof(g_ctx));

    /* ── 1. Create mutex ──────────────────────────────────────────────────── */
    g_ctx.mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(g_ctx.mutex != NULL, ESP_ERR_NO_MEM, g_tag, "Mutex create failed");

    g_ctx.i2c_bus = cfg->i2c_bus;
    g_ctx.mode    = cfg->mode;

    /* ── 2. Attach device to shared bus ───────────────────────────────────── */
    const i2c_device_config_t DEV_CFG = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = cfg->i2c_addr,
        .scl_speed_hz    = cfg->i2c_freq_hz,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(cfg->i2c_bus, &DEV_CFG, &g_ctx.i2c_dev), g_tag,
                        "Failed to add BME280 to I2C bus");

    /* ── 3. Verify chip ID ────────────────────────────────────────────────── */
    uint8_t chip_id = 0U;
    ESP_RETURN_ON_ERROR(reg_read(REG_ID, &chip_id, 1U), g_tag, "Chip ID read failed");
    ESP_RETURN_ON_FALSE(chip_id == BME280_CHIP_ID, ESP_ERR_NOT_FOUND, g_tag,
                        "Unexpected chip ID 0x%02X (expected 0x60) — check wiring/address",
                        chip_id);

    /* ── 4. Soft-reset then load calibration ──────────────────────────────── */
    ESP_RETURN_ON_ERROR(reg_write(REG_RESET, 0xB6U), g_tag, "Soft reset failed");
    vTaskDelay(pdMS_TO_TICKS(5)); /* Wait for NVM copy to complete */
    ESP_RETURN_ON_ERROR(load_calibration(), g_tag, "Calibration load failed");

    /* ── 5. Configure humidity oversampling (must be set before ctrl_meas) ── */
    ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_HUM, OSRS_X1), g_tag, "ctrl_hum write failed");

    /* ── 6. Set operating mode ────────────────────────────────────────────── */
    if (cfg->mode == BME280_SENSOR_MODE_NORMAL) {
        ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_MEAS, CTRL_MEAS_BASE | MODE_NORMAL), g_tag,
                            "Failed to set normal mode");
        vTaskDelay(pdMS_TO_TICKS(MEAS_WAIT_MS)); /* Wait one cycle for first reading */
    }
    /* FORCED mode: measurements are triggered on demand in bme280_sensor_read() */

    g_ctx.initialised = true;
    ESP_LOGI(g_tag, "BME280 ready (addr=0x%02X, mode=%s)", cfg->i2c_addr,
             cfg->mode == BME280_SENSOR_MODE_NORMAL ? "NORMAL" : "FORCED");
    return ESP_OK;
}

esp_err_t bme280_sensor_read(Bme280Reading* out) {
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, g_tag, "out is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, g_tag, "Not initialised");

    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        ESP_LOGE(g_tag, "Mutex timeout in bme280_sensor_read()");
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;

    if (g_ctx.mode == BME280_SENSOR_MODE_FORCED) {
        ret = forced_measure_and_wait();
        if (ret != ESP_OK) {
            goto release;
        }
    }

    /*
     * Burst-read 8 bytes from 0xF7:
     *   [0..2] press_msb / press_lsb / press_xlsb
     *   [3..5] temp_msb  / temp_lsb  / temp_xlsb
     *   [6..7] hum_msb   / hum_lsb
     */
    uint8_t raw[8] = {0};
    ret            = reg_read(REG_DATA, raw, sizeof(raw));
    if (ret != ESP_OK) {
        ESP_LOGE(g_tag, "Data burst read failed: %s", esp_err_to_name(ret));
        goto release;
    }

    int32_t adc_p =
        (int32_t)(((uint32_t)raw[0] << 12U) | ((uint32_t)raw[1] << 4U) | ((uint32_t)raw[2] >> 4U));
    int32_t adc_t =
        (int32_t)(((uint32_t)raw[3] << 12U) | ((uint32_t)raw[4] << 4U) | ((uint32_t)raw[5] >> 4U));
    int32_t adc_h = (int32_t)(((uint32_t)raw[6] << 8U) | (uint32_t)raw[7]);

    int32_t t_fine     = 0;
    out->temperature_c = compensate_temperature(adc_t, &t_fine);
    out->pressure_hpa  = compensate_pressure(adc_p, t_fine);
    out->humidity_pct  = compensate_humidity(adc_h, t_fine);
    g_ctx.last_reading = *out;
    g_ctx.last_valid   = true;

release:
    xSemaphoreGive(g_ctx.mutex);
    return ret;
}

esp_err_t bme280_sensor_get_last(Bme280Reading* out) {
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, g_tag, "out is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, g_tag, "Not initialised");

    if (!g_ctx.last_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(I2C_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    *out = g_ctx.last_reading;
    xSemaphoreGive(g_ctx.mutex);
    return ESP_OK;
}

esp_err_t bme280_sensor_start_task(const Bme280TaskCfg* task_cfg) {
    ESP_RETURN_ON_FALSE(task_cfg != NULL, ESP_ERR_INVALID_ARG, g_tag, "task_cfg is NULL");
    ESP_RETURN_ON_FALSE(task_cfg->callback != NULL, ESP_ERR_INVALID_ARG, g_tag, "callback is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, g_tag, "Not initialised");
    ESP_RETURN_ON_FALSE(g_ctx.task_handle == NULL, ESP_ERR_INVALID_STATE, g_tag,
                        "Polling task already running");

    uint32_t interval_ms = task_cfg->poll_interval_ms < 10U ? BME280_TASK_DEFAULT_INTERVAL_MS
                                                            : task_cfg->poll_interval_ms;
    uint32_t stack =
        task_cfg->stack_size < 3072U ? BME280_TASK_DEFAULT_STACK_SIZE : task_cfg->stack_size;
    UBaseType_t priority =
        task_cfg->priority == 0U ? BME280_TASK_DEFAULT_PRIORITY : task_cfg->priority;

    g_ctx.task_cfg                  = *task_cfg;
    g_ctx.task_cfg.poll_interval_ms = interval_ms;
    g_ctx.task_cfg.stack_size       = stack;
    g_ctx.task_stop_req             = false;

    BaseType_t created = xTaskCreatePinnedToCore(sensor_task, "bme280_poll", stack, &g_ctx.task_cfg,
                                                 priority, &g_ctx.task_handle, task_cfg->core_id);
    ESP_RETURN_ON_FALSE(created == pdPASS, ESP_ERR_NO_MEM, g_tag, "Task create failed");

    ESP_LOGI(g_tag, "Polling task created (stack=%lu, prio=%lu, core=%d, interval=%lu ms)",
             (unsigned long)stack, (unsigned long)priority, (int)task_cfg->core_id,
             (unsigned long)interval_ms);
    return ESP_OK;
}

esp_err_t bme280_sensor_stop_task(void) {
    if (g_ctx.task_handle == NULL) {
        return ESP_OK;
    }

    g_ctx.task_stop_req = true;

    const TickType_t TIMEOUT =
        xTaskGetTickCount() + pdMS_TO_TICKS(g_ctx.task_cfg.poll_interval_ms * 3U + 500U);
    while (g_ctx.task_handle != NULL) {
        if (xTaskGetTickCount() > TIMEOUT) {
            ESP_LOGW(g_tag, "Timeout waiting for polling task — force-deleting");
            vTaskDelete(g_ctx.task_handle);
            g_ctx.task_handle = NULL;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(g_tag, "Polling task stopped");
    return ESP_OK;
}

bool bme280_sensor_task_running(void) { return g_ctx.task_handle != NULL; }

esp_err_t bme280_sensor_init_with_task(i2c_master_bus_handle_t i2c_bus, Bme280SampleCb callback,
                                       void* user_ctx) {
    ESP_RETURN_ON_FALSE(i2c_bus != NULL, ESP_ERR_INVALID_ARG, g_tag, "i2c_bus is NULL");
    ESP_RETURN_ON_FALSE(callback != NULL, ESP_ERR_INVALID_ARG, g_tag, "callback is NULL");

    /* Probe both known addresses — SDO low = 0x76, SDO high = 0x77 */
    const uint8_t ADDRS[] = {BME280_I2C_ADDR_PRIMARY, BME280_I2C_ADDR_SECONDARY};
    esp_err_t ret         = ESP_ERR_NOT_FOUND;
    for (size_t i = 0; i < sizeof(ADDRS) / sizeof(ADDRS[0]); i++) {
        if (i2c_master_probe(i2c_bus, ADDRS[i], (int)I2C_TIMEOUT_MS) == ESP_OK) {
            const Bme280SensorCfg SENSOR_CFG = {
                .i2c_bus     = i2c_bus,
                .i2c_addr    = ADDRS[i],
                .i2c_freq_hz = 400000U,
                .mode        = BME280_SENSOR_MODE_NORMAL,
            };
            ret = bme280_sensor_init(&SENSOR_CFG);
            if (ret == ESP_OK) {
                break;
            }
        }
    }
    ESP_RETURN_ON_ERROR(ret, g_tag, "Quick-start init failed — BME280 not found at 0x76 or 0x77");

    const Bme280TaskCfg TASK_CFG = {
        .poll_interval_ms = BME280_TASK_DEFAULT_INTERVAL_MS,
        .callback         = callback,
        .user_ctx         = user_ctx,
        .stack_size       = BME280_TASK_DEFAULT_STACK_SIZE,
        .priority         = BME280_TASK_DEFAULT_PRIORITY,
        .core_id          = BME280_TASK_DEFAULT_CORE,
    };
    ESP_RETURN_ON_ERROR(bme280_sensor_start_task(&TASK_CFG), g_tag, "Quick-start task failed");

    return ESP_OK;
}

esp_err_t bme280_sensor_deinit(void) {
    if (!g_ctx.initialised) {
        return ESP_OK;
    }

    bme280_sensor_stop_task();

    if (g_ctx.i2c_dev != NULL) {
        i2c_master_bus_rm_device(g_ctx.i2c_dev);
        g_ctx.i2c_dev = NULL;
    }

    if (g_ctx.mutex != NULL) {
        vSemaphoreDelete(g_ctx.mutex);
        g_ctx.mutex = NULL;
    }

    g_ctx.initialised = false;
    ESP_LOGI(g_tag, "BME280 de-initialised");
    return ESP_OK;
}
