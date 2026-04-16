#include "env_sensor.h"

esp_err_t sensor_init(void) {
    return ESP_OK;
}

esp_err_t sensor_read(SensorData *out) {
    out->temperature = 22.5f;
    out->humidity    = 55.0f;
    out->pressure    = 1013.0f;
    return ESP_OK;
}
