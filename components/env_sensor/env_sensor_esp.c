#include "driver/i2c_master.h"
#include "env_sensor.h"
#include "esp_log.h"
#include "ws7b_board.h"

#define TAG "env_sensor"
#define BME280_ADDR 0x77

static i2c_master_dev_handle_t g_bme280_dev = NULL;

// BME280 register addresses
#define BME280_REG_CALIB 0x88
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_DATA 0xF7

// Calibration data stored at startup
static uint16_t g_dig_T1;
static int16_t g_dig_T2, g_dig_T3;
static uint16_t g_dig_P1;
static int16_t g_dig_P2, g_dig_P3, g_dig_P4, g_dig_P5;
static int16_t g_dig_P6, g_dig_P7, g_dig_P8, g_dig_P9;
static uint8_t g_dig_H1;
static int16_t g_dig_H2;
static uint8_t g_dig_H3;
static int16_t g_dig_H4, g_dig_H5;
static int8_t g_dig_H6;

// Write one byte to a BME280 register
static esp_err_t bme280_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(g_bme280_dev, buf, 2, pdMS_TO_TICKS(100));
}

// Read multiple bytes starting from a register
static esp_err_t bme280_read(uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_master_transmit_receive(g_bme280_dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

esp_err_t sensor_init(void) {
    // Add BME280 to the shared I2C bus initialized by ws7b_board
    i2c_master_bus_handle_t bus = ws7b_get_i2c_bus();
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BME280_ADDR,
        .scl_speed_hz    = 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &g_bme280_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "BME280 not found at 0x76");
        return ret;
    }

    // Read factory calibration data from sensor (unique per chip)
    uint8_t calib[26];
    ret = bme280_read(BME280_REG_CALIB, calib, 26);
    if (ret != ESP_OK)
        return ret;

    g_dig_T1 = (calib[1] << 8) | calib[0];
    g_dig_T2 = (calib[3] << 8) | calib[2];
    g_dig_T3 = (calib[5] << 8) | calib[4];
    g_dig_P1 = (calib[7] << 8) | calib[6];
    g_dig_P2 = (calib[9] << 8) | calib[8];
    g_dig_P3 = (calib[11] << 8) | calib[10];
    g_dig_P4 = (calib[13] << 8) | calib[12];
    g_dig_P5 = (calib[15] << 8) | calib[14];
    g_dig_P6 = (calib[17] << 8) | calib[16];
    g_dig_P7 = (calib[19] << 8) | calib[18];
    g_dig_P8 = (calib[21] << 8) | calib[20];
    g_dig_P9 = (calib[23] << 8) | calib[22];
    g_dig_H1 = calib[25];

    uint8_t calib2[7];
    bme280_read(0xE1, calib2, 7);
    g_dig_H2 = (calib2[1] << 8) | calib2[0];
    g_dig_H3 = calib2[2];
    g_dig_H4 = (calib2[3] << 4) | (calib2[4] & 0x0F);
    g_dig_H5 = (calib2[5] << 4) | (calib2[4] >> 4);
    g_dig_H6 = calib2[6];

    // Set humidity oversampling x1
    bme280_write(BME280_REG_CTRL_HUM, 0x01);
    // Set temp + pressure oversampling x1, normal mode
    bme280_write(BME280_REG_CTRL_MEAS, 0x27);

    ESP_LOGI(TAG, "BME280 initialized");
    return ESP_OK;
}

esp_err_t sensor_read(SensorData* out) {
    // Read 8 bytes of raw sensor data
    uint8_t raw[8];
    esp_err_t ret = bme280_read(BME280_REG_DATA, raw, 8);
    if (ret != ESP_OK)
        return ret;
    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);
    int32_t adc_H = ((int32_t)raw[6] << 8) | raw[7];

    int32_t var1 = ((((adc_T >> 3) - ((int32_t)g_dig_T1 << 1))) * g_dig_T2) >> 11;
    int32_t var2 =
        (((((adc_T >> 4) - (int32_t)g_dig_T1) * ((adc_T >> 4) - (int32_t)g_dig_T1)) >> 12) *
         g_dig_T3) >>
        14;
    int32_t t_fine   = var1 + var2;
    out->temperature = (float)((t_fine * 5 + 128) >> 8) / 100.0f;

    int64_t p1 = (int64_t)t_fine - 128000;
    int64_t p2 = p1 * p1 * g_dig_P6;
    p2 += (p1 * g_dig_P5) << 17;
    p2 += (int64_t)g_dig_P4 << 35;
    p1            = ((p1 * p1 * g_dig_P3) >> 8) + ((p1 * g_dig_P2) << 12);
    p1            = (((int64_t)1 << 47) + p1) * g_dig_P1 >> 33;
    int64_t p     = 1048576 - adc_P;
    p             = (((p << 31) - p2) * 3125) / p1;
    p1            = ((int64_t)g_dig_P9 * (p >> 13) * (p >> 13)) >> 25;
    p2            = ((int64_t)g_dig_P8 * p) >> 19;
    out->pressure = (float)((p + p1 + p2) >> 8) / 25600.0f;

    int32_t h = t_fine - 76800;
    h         = (((adc_H << 14) - (g_dig_H4 << 20) - (g_dig_H5 * h)) + 16384) >> 15;
    h         = h * (((h * (int32_t)g_dig_H6) >> 10) * (((h * (int32_t)g_dig_H3) >> 11) + 32768));
    h         = (h >> 10) + 2097152;

    h             = (h + 8192) >> 14;
    out->humidity = (float)h / 1024.0f;

    return ESP_OK;
}
