#ifndef ENV_SENSOR_H
#define ENV_SENSOR_H

#include "esp_err.h"

typedef struct {
    float temperature;  // °C
    float humidity;     // %
    float pressure;     // hPa
} SensorData;

esp_err_t sensor_init(void);
esp_err_t sensor_read(SensorData *out);

#endif
