/**
 * @file bme280_sensor.cpp
 * @brief BME280 environmental sensor module — C++ implementation for ESP-IDF.
 *
 * Public symbols are exported with C linkage (declared in bme280_sensor.h
 * inside extern "C" guards) so existing C callers require no changes.
 *
 * Compensation formulas are taken verbatim from the BME280 datasheet
 * (BST-BME280-DS002, section 8.2).
 */

#include "bme280_sensor.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <array>
#include <cstring>

static constexpr const char* TAG = "bme280_sensor";

// Register addresses
static constexpr uint8_t REG_ID        = 0xD0U;
static constexpr uint8_t REG_RESET     = 0xE0U;
static constexpr uint8_t REG_CTRL_HUM  = 0xF2U;
static constexpr uint8_t REG_STATUS    = 0xF3U;
static constexpr uint8_t REG_CTRL_MEAS = 0xF4U;
static constexpr uint8_t REG_CONFIG    = 0xF5U;
static constexpr uint8_t REG_DATA      = 0xF7U;
static constexpr uint8_t REG_CALIB_00  = 0x88U;
static constexpr uint8_t REG_CALIB_26  = 0xE1U;

static constexpr uint8_t BME280_CHIP_ID  = 0x60U;
static constexpr uint8_t MODE_SLEEP      = 0x00U;
static constexpr uint8_t MODE_FORCED     = 0x01U;
static constexpr uint8_t MODE_NORMAL     = 0x03U;
static constexpr uint8_t OSRS_X1         = 0x01U;
static constexpr uint8_t CTRL_MEAS_BASE  = (OSRS_X1 << 5U) | (OSRS_X1 << 2U);
static constexpr uint32_t MEAS_WAIT_MS   = 15U;
static constexpr uint32_t I2C_TIMEOUT_MS = 100U;

// Calibration data
struct Bme280Calib {
    // Temperature
    uint16_t dig_t1{};
    int16_t dig_t2{};
    int16_t dig_t3{};
    // Pressure
    uint16_t dig_p1{};
    int16_t dig_p2{};
    int16_t dig_p3{};
    int16_t dig_p4{};
    int16_t dig_p5{};
    int16_t dig_p6{};
    int16_t dig_p7{};
    int16_t dig_p8{};
    int16_t dig_p9{};
    // Humidity
    uint8_t dig_h1{};
    int16_t dig_h2{};
    uint8_t dig_h3{};
    int16_t dig_h4{};
    int16_t dig_h5{};
    int8_t dig_h6{};
};

// Acquires the semaphore on construction; releases it on destruction.
// Makes every early-return path safe

class LockGuard {
  public:
    explicit LockGuard(SemaphoreHandle_t sem, uint32_t timeout_ms = I2C_TIMEOUT_MS)
        : m_sem(sem), m_locked(xSemaphoreTake(sem, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) {}

    ~LockGuard() {
        if (m_locked) {
            xSemaphoreGive(m_sem);
        }
    }

    [[nodiscard]] bool locked() const { return m_locked; }

    // Non-copyable, non-movable
    LockGuard(const LockGuard&)            = delete;
    LockGuard& operator=(const LockGuard&) = delete;

  private:
    SemaphoreHandle_t m_sem;
    bool m_locked;
};

// Module state
struct Bme280SensorCtx {
    i2c_master_bus_handle_t i2c_bus{nullptr};
    i2c_master_dev_handle_t i2c_dev{nullptr};

    Bme280SensorMode mode{BME280_SENSOR_MODE_NORMAL};
    Bme280Calib calib{};

    SemaphoreHandle_t mutex{nullptr};

    TaskHandle_t task_handle{nullptr};
    volatile bool task_stop_req{false};
    Bme280TaskCfg task_cfg{};

    Bme280Reading last_reading{};
    bool last_valid{false};
    bool initialised{false};
};

static Bme280SensorCtx g_ctx{};

// i2c helpers
static esp_err_t reg_write(uint8_t reg, uint8_t value) {
    const std::array<uint8_t, 2> BUF{reg, value};
    return i2c_master_transmit(g_ctx.i2c_dev, BUF.data(), BUF.size(),
                               static_cast<int>(I2C_TIMEOUT_MS));
}

static esp_err_t reg_read(uint8_t reg, uint8_t* dst, size_t len) {
    return i2c_master_transmit_receive(g_ctx.i2c_dev, &reg, 1U, dst, len,
                                       static_cast<int>(I2C_TIMEOUT_MS));
}

static esp_err_t load_calibration() {
    std::array<uint8_t, 26> c{};
    ESP_RETURN_ON_ERROR(reg_read(REG_CALIB_00, c.data(), c.size()), TAG,
                        "Failed to read T/P calibration");

    auto u16 = [&](int lo, int hi) -> uint16_t {
        return static_cast<uint16_t>((c[hi] << 8U) | c[lo]);
    };
    auto s16 = [&](int lo, int hi) -> int16_t {
        return static_cast<int16_t>((c[hi] << 8U) | c[lo]);
    };

    g_ctx.calib.dig_t1 = u16(0, 1);
    g_ctx.calib.dig_t2 = s16(2, 3);
    g_ctx.calib.dig_t3 = s16(4, 5);

    g_ctx.calib.dig_p1 = u16(6, 7);
    g_ctx.calib.dig_p2 = s16(8, 9);
    g_ctx.calib.dig_p3 = s16(10, 11);
    g_ctx.calib.dig_p4 = s16(12, 13);
    g_ctx.calib.dig_p5 = s16(14, 15);
    g_ctx.calib.dig_p6 = s16(16, 17);
    g_ctx.calib.dig_p7 = s16(18, 19);
    g_ctx.calib.dig_p8 = s16(20, 21);
    g_ctx.calib.dig_p9 = s16(22, 23);
    g_ctx.calib.dig_h1 = c[25]; // byte at 0xA1

    std::array<uint8_t, 7> h{};
    ESP_RETURN_ON_ERROR(reg_read(REG_CALIB_26, h.data(), h.size()), TAG,
                        "Failed to read H calibration");

    g_ctx.calib.dig_h2 = static_cast<int16_t>((h[1] << 8U) | h[0]);
    g_ctx.calib.dig_h3 = h[2];
    g_ctx.calib.dig_h4 = static_cast<int16_t>((static_cast<int16_t>(h[3]) << 4U) | (h[4] & 0x0FU));
    g_ctx.calib.dig_h5 = static_cast<int16_t>((static_cast<int16_t>(h[5]) << 4U) | (h[4] >> 4U));
    g_ctx.calib.dig_h6 = static_cast<int8_t>(h[6]);

    return ESP_OK;
}

// Datasheet compensation formulas (double precision, section 8.2)
static float compensate_temperature(int32_t adc_t, int32_t& t_fine) {
    const Bme280Calib& cal = g_ctx.calib;
    const double VAR1 =
        (((double)adc_t / 16384.0) - ((double)cal.dig_t1 / 1024.0)) * (double)cal.dig_t2;
    const double VAR2 = (((double)adc_t / 131072.0) - ((double)cal.dig_t1 / 8192.0)) *
                        (((double)adc_t / 131072.0) - ((double)cal.dig_t1 / 8192.0)) *
                        (double)cal.dig_t3;
    t_fine = static_cast<int32_t>(VAR1 + VAR2);
    return static_cast<float>((VAR1 + VAR2) / 5120.0);
}

static float compensate_pressure(int32_t adc_p, int32_t t_fine) {
    const Bme280Calib& cal = g_ctx.calib;
    double var1            = ((double)t_fine / 2.0) - 64000.0;
    double var2            = var1 * var1 * (double)cal.dig_p6 / 32768.0;
    var2                   = var2 + (var1 * (double)cal.dig_p5 * 2.0);
    var2                   = (var2 / 4.0) + ((double)cal.dig_p4 * 65536.0);
    var1 = (((double)cal.dig_p3 * var1 * var1 / 524288.0) + ((double)cal.dig_p2 * var1)) / 524288.0;
    var1 = (1.0 + (var1 / 32768.0)) * (double)cal.dig_p1;

    if (var1 == 0.0) {
        return 0.0F; // Avoid division by zero
    }

    double pressure = 1048576.0 - (double)adc_p;
    pressure        = (pressure - (var2 / 4096.0)) * 6250.0 / var1;
    var1            = (double)cal.dig_p9 * pressure * pressure / 2147483648.0;
    var2            = pressure * (double)cal.dig_p8 / 32768.0;
    pressure        = pressure + ((var1 + var2 + (double)cal.dig_p7) / 16.0);
    return static_cast<float>(pressure / 100.0); // Pa → hPa
}

static float compensate_humidity(int32_t adc_h, int32_t t_fine) {
    const Bme280Calib& cal = g_ctx.calib;
    double var_h           = (double)t_fine - 76800.0;
    var_h =
        ((double)adc_h - (((double)cal.dig_h4 * 64.0) + ((double)cal.dig_h5 / 16384.0 * var_h))) *
        ((double)cal.dig_h2 / 65536.0 *
         (1.0 + ((double)cal.dig_h6 / 67108864.0 * var_h *
                 (1.0 + ((double)cal.dig_h3 / 67108864.0 * var_h)))));
    var_h = var_h * (1.0 - ((double)cal.dig_h1 * var_h / 524288.0));

    if (var_h > 100.0) {
        var_h = 100.0;
    } else if (var_h < 0.0) {
        var_h = 0.0;
    }

    return static_cast<float>(var_h);
}

static esp_err_t forced_measure_and_wait() {
    ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_MEAS, CTRL_MEAS_BASE | MODE_FORCED), TAG,
                        "Failed to trigger forced measurement");

    const TickType_t DEADLINE = xTaskGetTickCount() + pdMS_TO_TICKS(MEAS_WAIT_MS * 4U);

    while (true) {
        if (xTaskGetTickCount() >= DEADLINE) {
            ESP_LOGE(TAG, "Forced measurement timeout");
            return ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
        uint8_t status = 0U;
        ESP_RETURN_ON_ERROR(reg_read(REG_STATUS, &status, 1U), TAG, "Status read failed");
        if ((status & 0x08U) == 0U) {
            break; // measuring bit clear — data ready
        }
    }
    return ESP_OK;
}

// polling task
static void sensor_task(void* arg) {
    const auto* cfg = static_cast<const Bme280TaskCfg*>(arg);
    ESP_LOGI(TAG, "Polling task started (interval=%" PRIu32 " ms)", cfg->poll_interval_ms);

    while (!g_ctx.task_stop_req) {
        Bme280Reading reading{};
        const esp_err_t RET = bme280_sensor_read(&reading);
        if (RET == ESP_OK) {
            if (cfg->callback != nullptr) {
                cfg->callback(&reading, cfg->user_ctx);
            }
        } else {
            ESP_LOGW(TAG, "Polling task read error: %s", esp_err_to_name(RET));
        }
        vTaskDelay(pdMS_TO_TICKS(cfg->poll_interval_ms));
    }

    ESP_LOGI(TAG, "Polling task exiting");
    g_ctx.task_handle = nullptr;
    vTaskDelete(nullptr);
}

// Public api
esp_err_t bme280_sensor_init(const Bme280SensorCfg* cfg) {
    ESP_RETURN_ON_FALSE(cfg != nullptr, ESP_ERR_INVALID_ARG, TAG, "cfg is NULL");
    ESP_RETURN_ON_FALSE(cfg->i2c_bus != nullptr, ESP_ERR_INVALID_ARG, TAG, "i2c_bus is NULL");
    ESP_RETURN_ON_FALSE(!g_ctx.initialised, ESP_ERR_INVALID_STATE, TAG,
                        "Already initialised — call bme280_sensor_deinit() first");

    g_ctx = Bme280SensorCtx{}; // Reset to value-initialised state

    // Create mutex
    g_ctx.mutex = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(g_ctx.mutex != nullptr, ESP_ERR_NO_MEM, TAG, "Mutex create failed");

    g_ctx.i2c_bus = cfg->i2c_bus;
    g_ctx.mode    = cfg->mode;

    // Attach device to shared bus
    const i2c_device_config_t DEV_CFG{
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = cfg->i2c_addr,
        .scl_speed_hz    = cfg->i2c_freq_hz,
        .scl_wait_us     = 0,
        .flags           = {0},
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(cfg->i2c_bus, &DEV_CFG, &g_ctx.i2c_dev), TAG,
                        "Failed to add BME280 to I2C bus");

    // Wait for NVM copy to complete, then verify chip ID
    {
        // im_update (bit 0 of REG_STATUS) is set while NVM is being copied on power-on.
        // Datasheet says this takes up to 2 ms; we poll with a short timeout.
        const TickType_t DEADLINE = xTaskGetTickCount() + pdMS_TO_TICKS(10);
        while (xTaskGetTickCount() < DEADLINE) {
            uint8_t status = 0U;
            // If I2C isn't ready yet, just wait and retry
            if (reg_read(REG_STATUS, &status, 1U) == ESP_OK && (status & 0x01U) == 0U) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    uint8_t chip_id = 0U;
    ESP_RETURN_ON_ERROR(reg_read(REG_ID, &chip_id, 1U), TAG, "Chip ID read failed");
    ESP_RETURN_ON_FALSE(chip_id == BME280_CHIP_ID, ESP_ERR_NOT_FOUND, TAG,
                        "Unexpected chip ID 0x%02X (expected 0x60) — check wiring/address",
                        chip_id);

    // Soft-reset, then load calibration
    ESP_RETURN_ON_ERROR(reg_write(REG_RESET, 0xB6U), TAG, "Soft reset failed");
    vTaskDelay(pdMS_TO_TICKS(5)); // Wait for NVM copy to complete
    ESP_RETURN_ON_ERROR(load_calibration(), TAG, "Calibration load failed");

    // Humidity oversampling (must be written before ctrl_meas)
    ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_HUM, OSRS_X1), TAG, "ctrl_hum write failed");

    // Set operating mode
    if (cfg->mode == BME280_SENSOR_MODE_NORMAL) {
        ESP_RETURN_ON_ERROR(reg_write(REG_CTRL_MEAS, CTRL_MEAS_BASE | MODE_NORMAL), TAG,
                            "Failed to set normal mode");
        vTaskDelay(pdMS_TO_TICKS(MEAS_WAIT_MS)); // Wait one cycle for first reading
    }
    // FORCED: measurements triggered on demand in bme280_sensor_read()

    g_ctx.initialised = true;
    ESP_LOGI(TAG, "BME280 ready (addr=0x%02X, mode=%s)", cfg->i2c_addr,
             cfg->mode == BME280_SENSOR_MODE_NORMAL ? "NORMAL" : "FORCED");
    return ESP_OK;
}

esp_err_t bme280_sensor_read(Bme280Reading* out) {
    ESP_RETURN_ON_FALSE(out != nullptr, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, TAG, "Not initialised");

    LockGuard lock{g_ctx.mutex};
    if (!lock.locked()) {
        ESP_LOGE(TAG, "Mutex timeout in bme280_sensor_read()");
        return ESP_ERR_TIMEOUT;
    }

    if (g_ctx.mode == BME280_SENSOR_MODE_FORCED) {
        const esp_err_t RET = forced_measure_and_wait();
        if (RET != ESP_OK) {
            return RET;
        }
    }

    // Burst-read 8 bytes from 0xF7:
    //   [0..2] press_msb / press_lsb / press_xlsb
    //   [3..5] temp_msb  / temp_lsb  / temp_xlsb
    //   [6..7] hum_msb   / hum_lsb
    std::array<uint8_t, 8> raw{};
    const esp_err_t RET = reg_read(REG_DATA, raw.data(), raw.size());
    if (RET != ESP_OK) {
        ESP_LOGE(TAG, "Data burst read failed: %s", esp_err_to_name(RET));
        return RET;
    }

    const int32_t ADC_P = static_cast<int32_t>(((uint32_t)raw[0] << 12U) |
                                               ((uint32_t)raw[1] << 4U) | ((uint32_t)raw[2] >> 4U));
    const int32_t ADC_T = static_cast<int32_t>(((uint32_t)raw[3] << 12U) |
                                               ((uint32_t)raw[4] << 4U) | ((uint32_t)raw[5] >> 4U));
    const int32_t ADC_H = static_cast<int32_t>(((uint32_t)raw[6] << 8U) | (uint32_t)raw[7]);

    int32_t t_fine     = 0;
    out->temperature_c = compensate_temperature(ADC_T, t_fine);
    out->pressure_hpa  = compensate_pressure(ADC_P, t_fine);
    out->humidity_pct  = compensate_humidity(ADC_H, t_fine);

    g_ctx.last_reading = *out;
    g_ctx.last_valid   = true;

    return ESP_OK; // LockGuard releases mutex here automatically
}

esp_err_t bme280_sensor_get_last(Bme280Reading* out) {
    ESP_RETURN_ON_FALSE(out != nullptr, ESP_ERR_INVALID_ARG, TAG, "out is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, TAG, "Not initialised");

    if (!g_ctx.last_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    LockGuard lock{g_ctx.mutex};
    if (!lock.locked()) {
        return ESP_ERR_TIMEOUT;
    }

    *out = g_ctx.last_reading;
    return ESP_OK;
}

esp_err_t bme280_sensor_start_task(const Bme280TaskCfg* task_cfg) {
    ESP_RETURN_ON_FALSE(task_cfg != nullptr, ESP_ERR_INVALID_ARG, TAG, "task_cfg is NULL");
    ESP_RETURN_ON_FALSE(task_cfg->callback != nullptr, ESP_ERR_INVALID_ARG, TAG,
                        "callback is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, TAG, "Not initialised");
    ESP_RETURN_ON_FALSE(g_ctx.task_handle == nullptr, ESP_ERR_INVALID_STATE, TAG,
                        "Polling task already running");

    const uint32_t INTERVAL_MS = task_cfg->poll_interval_ms < 10U ? BME280_TASK_DEFAULT_INTERVAL_MS
                                                                  : task_cfg->poll_interval_ms;
    const uint32_t STACK =
        task_cfg->stack_size < 3072U ? BME280_TASK_DEFAULT_STACK_SIZE : task_cfg->stack_size;
    const UBaseType_t PRIORITY =
        task_cfg->priority == 0U ? BME280_TASK_DEFAULT_PRIORITY : task_cfg->priority;

    g_ctx.task_cfg                  = *task_cfg;
    g_ctx.task_cfg.poll_interval_ms = INTERVAL_MS;
    g_ctx.task_cfg.stack_size       = STACK;
    g_ctx.task_stop_req             = false;

    const BaseType_t CREATED =
        xTaskCreatePinnedToCore(sensor_task, "bme280_poll", STACK, &g_ctx.task_cfg, PRIORITY,
                                &g_ctx.task_handle, task_cfg->core_id);
    ESP_RETURN_ON_FALSE(CREATED == pdPASS, ESP_ERR_NO_MEM, TAG, "Task create failed");

    ESP_LOGI(TAG,
             "Polling task created (stack=%" PRIu32 ", prio=%u, core=%d, interval=%" PRIu32 " ms)",
             STACK, PRIORITY, static_cast<int>(task_cfg->core_id), INTERVAL_MS);
    return ESP_OK;
}

esp_err_t bme280_sensor_stop_task() {
    if (g_ctx.task_handle == nullptr) {
        return ESP_OK;
    }

    g_ctx.task_stop_req = true;

    const TickType_t TIMEOUT =
        xTaskGetTickCount() + pdMS_TO_TICKS((g_ctx.task_cfg.poll_interval_ms * 3U) + 500U);

    while (g_ctx.task_handle != nullptr) {
        if (xTaskGetTickCount() > TIMEOUT) {
            ESP_LOGW(TAG, "Timeout waiting for polling task — force-deleting");
            vTaskDelete(g_ctx.task_handle);
            g_ctx.task_handle = nullptr;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Polling task stopped");
    return ESP_OK;
}

bool bme280_sensor_task_running() { return g_ctx.task_handle != nullptr; }

esp_err_t bme280_sensor_init_with_task(i2c_master_bus_handle_t i2c_bus, Bme280SampleCb callback,
                                       void* user_ctx) {
    ESP_RETURN_ON_FALSE(i2c_bus != nullptr, ESP_ERR_INVALID_ARG, TAG, "i2c_bus is NULL");
    ESP_RETURN_ON_FALSE(callback != nullptr, ESP_ERR_INVALID_ARG, TAG, "callback is NULL");

    // Probe both known addresses — SDO low = 0x76, SDO high = 0x77
    static constexpr std::array<uint8_t, 2> ADDRS{BME280_I2C_ADDR_PRIMARY,
                                                  BME280_I2C_ADDR_SECONDARY};

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    for (const uint8_t ADDR : ADDRS) {
        if (i2c_master_probe(i2c_bus, ADDR, static_cast<int>(I2C_TIMEOUT_MS)) == ESP_OK) {
            const Bme280SensorCfg SENSOR_CFG{
                .i2c_bus     = i2c_bus,
                .i2c_addr    = ADDR,
                .i2c_freq_hz = 400000U,
                .mode        = BME280_SENSOR_MODE_NORMAL,
            };
            ret = bme280_sensor_init(&SENSOR_CFG);
            if (ret == ESP_OK) {
                break;
            }
        }
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "Quick-start init failed — BME280 not found at 0x76 or 0x77");

    const Bme280TaskCfg TASK_CFG{
        .poll_interval_ms = BME280_TASK_DEFAULT_INTERVAL_MS,
        .callback         = callback,
        .user_ctx         = user_ctx,
        .stack_size       = BME280_TASK_DEFAULT_STACK_SIZE,
        .priority         = BME280_TASK_DEFAULT_PRIORITY,
        .core_id          = BME280_TASK_DEFAULT_CORE,
    };
    ESP_RETURN_ON_ERROR(bme280_sensor_start_task(&TASK_CFG), TAG, "Quick-start task failed");

    return ESP_OK;
}

esp_err_t bme280_sensor_deinit() {
    if (!g_ctx.initialised) {
        return ESP_OK;
    }

    bme280_sensor_stop_task();

    if (g_ctx.i2c_dev != nullptr) {
        i2c_master_bus_rm_device(g_ctx.i2c_dev);
        g_ctx.i2c_dev = nullptr;
    }

    if (g_ctx.mutex != nullptr) {
        vSemaphoreDelete(g_ctx.mutex);
        g_ctx.mutex = nullptr;
    }

    g_ctx = Bme280SensorCtx{}; // Reset to clean state
    ESP_LOGI(TAG, "BME280 de-initialised");
    return ESP_OK;
}
