#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "bme280.h"

static const char *TAG = "BME280_TEST";

// I2C configuration - CORRECTED PINS FOR WAVESHARE ESP32-S3
#define I2C_MASTER_NUM           I2C_NUM_0
#define I2C_MASTER_SDA_IO        8      // Fixed: GPIO8 for SDA
#define I2C_MASTER_SCL_IO        9      // Fixed: GPIO9 for SCL
#define I2C_MASTER_FREQ_HZ       100000
#define BME280_I2C_ADDRESS       0x77   // Try 0x77 if this fails

// I2C read function for BME280 API
int8_t i2c_read(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    int ret = i2c_master_write_read_device(I2C_MASTER_NUM, BME280_I2C_ADDRESS,
                                           &reg_addr, 1, reg_data, len,
                                           pdMS_TO_TICKS(1000));
    return (ret == ESP_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}

// I2C write function for BME280 API
int8_t i2c_write(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr)
{
    uint8_t *buffer = malloc(len + 1);
    if (!buffer) return BME280_E_COMM_FAIL;
    
    buffer[0] = reg_addr;
    memcpy(buffer + 1, reg_data, len);
    
    int ret = i2c_master_write_to_device(I2C_MASTER_NUM, BME280_I2C_ADDRESS,
                                         buffer, len + 1, pdMS_TO_TICKS(1000));
    free(buffer);
    
    return (ret == ESP_OK) ? BME280_OK : BME280_E_COMM_FAIL;
}

// Delay function for BME280 API
void delay_us(uint32_t period, void *intf_ptr)
{
    esp_rom_delay_us(period);
}

// Initialize I2C bus
void init_i2c(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0));
    ESP_LOGI(TAG, "I2C initialized on SDA=GPIO8, SCL=GPIO9");
}

void app_main(void)
{
    int8_t rslt;
    struct bme280_dev dev;
    struct bme280_settings settings;
    struct bme280_data data;
    
    ESP_LOGI(TAG, "Starting BME280 Test...");
    
    // Initialize I2C
    init_i2c();
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // Setup BME280 device structure
    dev.intf = BME280_I2C_INTF;
    dev.read = i2c_read;
    dev.write = i2c_write;
    dev.delay_us = delay_us;
    dev.intf_ptr = NULL;
    
    // Initialize sensor
    rslt = bme280_init(&dev);
    if (rslt != BME280_OK) {
        ESP_LOGE(TAG, "BME280 init failed! Error: %d", rslt);
        if (rslt == BME280_E_DEV_NOT_FOUND) {
            ESP_LOGE(TAG, "Device not found - check wiring and I2C address");
            ESP_LOGE(TAG, "Make sure BME280 is connected to GPIO8 (SDA) and GPIO9 (SCL)");
        }
        return;
    }
    
    ESP_LOGI(TAG, "✓ BME280 found! Chip ID: 0x%02X", dev.chip_id);
    
    // Get default settings
    bme280_get_sensor_settings(&settings, &dev);
    
    // Configure for normal operation
    settings.filter = BME280_FILTER_COEFF_OFF;
    settings.osr_h = BME280_OVERSAMPLING_1X;
    settings.osr_p = BME280_OVERSAMPLING_1X;
    settings.osr_t = BME280_OVERSAMPLING_1X;
    settings.standby_time = BME280_STANDBY_TIME_1000_MS;
    
    bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &settings, &dev);
    bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &dev);
    
    ESP_LOGI(TAG, "✓ Sensor configured and in NORMAL mode");
    
    // Read data every 2 seconds
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        
        rslt = bme280_get_sensor_data(BME280_ALL, &data, &dev);
        
        if (rslt == BME280_OK) {
            ESP_LOGI(TAG, "\n=== Sensor Readings ===");
            ESP_LOGI(TAG, "🌡️  Temperature: %.2f °C", data.temperature / 100.0f);
            ESP_LOGI(TAG, "💨 Pressure:    %.2f hPa", data.pressure / 100.0f);
            ESP_LOGI(TAG, "💧 Humidity:    %.2f %%", data.humidity / 1024.0f);
        } else {
            ESP_LOGE(TAG, "Failed to read data! Error: %d", rslt);
        }
    }
}