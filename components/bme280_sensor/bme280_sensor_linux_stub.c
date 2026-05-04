/**
 * @file bme280_sensor_linux_stub.c
 * @brief Linux simulator stub for BME280.
 */

#include "bme280_sensor.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char* g_tag = "bme280_sim";

/* --- Module state --- */
typedef struct {
    SemaphoreHandle_t mutex;
    TaskHandle_t task_handle;
    volatile bool task_stop_req;
    Bme280TaskCfg task_cfg;
    Bme280Reading last_reading;
    bool last_valid;
    bool initialised;
} Bme280SimCtx;

static Bme280SimCtx g_ctx = {0};

/* --- Internal Logic --- */

static void generate_simulated_data(Bme280Reading* out) {
    // Generate simple drift around standard indoor values
    // Temperature: 22.0 +/- 2.0
    // Pressure: 1013.25 +/- 5.0
    // Humidity: 45.0 +/- 10.0
    out->temperature_c = 22.0F + ((float)(rand() % 400) / 100.0F - 2.0F);
    out->pressure_hpa  = 1013.25F + ((float)(rand() % 1000) / 100.0F - 5.0F);
    out->humidity_pct  = 45.0F + ((float)(rand() % 2000) / 100.0F - 10.0F);
}

static void sensor_task(void* arg) {
    const Bme280TaskCfg* cfg = (const Bme280TaskCfg*)arg;
    ESP_LOGI(g_tag, "Simulated polling task started");

    while (!g_ctx.task_stop_req) {
        Bme280Reading reading = {0};
        if (bme280_sensor_read(&reading) == ESP_OK) {
            if (cfg->callback != NULL) {
                cfg->callback(&reading, cfg->user_ctx);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(cfg->poll_interval_ms));
    }

    g_ctx.task_handle = NULL;
    vTaskDelete(NULL);
}

/* --- Public API Stub Implementation --- */

esp_err_t bme280_sensor_init(const Bme280SensorCfg* cfg) {
    if (g_ctx.initialised) {
        return ESP_ERR_INVALID_STATE;
    }

    srand(time(NULL)); // Seed for random data

    g_ctx.mutex = xSemaphoreCreateMutex();
    if (g_ctx.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    g_ctx.initialised = true;
    g_ctx.last_valid  = false;

    ESP_LOGI(g_tag, "BME280 Simulator initialized (Linux Stub)");
    return ESP_OK;
}

esp_err_t bme280_sensor_read(Bme280Reading* out) {
    ESP_RETURN_ON_FALSE(out != NULL, ESP_ERR_INVALID_ARG, g_tag, "out is NULL");
    ESP_RETURN_ON_FALSE(g_ctx.initialised, ESP_ERR_INVALID_STATE, g_tag, "Not initialised");

    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    generate_simulated_data(out);
    g_ctx.last_reading = *out;
    g_ctx.last_valid   = true;

    xSemaphoreGive(g_ctx.mutex);
    return ESP_OK;
}

esp_err_t bme280_sensor_get_last(Bme280Reading* out) {
    if (!g_ctx.last_valid) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(g_ctx.mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        *out = g_ctx.last_reading;
        xSemaphoreGive(g_ctx.mutex);
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t bme280_sensor_start_task(const Bme280TaskCfg* task_cfg) {
    if (g_ctx.task_handle != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    g_ctx.task_cfg      = *task_cfg;
    g_ctx.task_stop_req = false;

    BaseType_t res = xTaskCreate(sensor_task, "bme280_sim_task", task_cfg->stack_size,
                                 &g_ctx.task_cfg, task_cfg->priority, &g_ctx.task_handle);

    return (res == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

esp_err_t bme280_sensor_stop_task(void) {
    if (g_ctx.task_handle == NULL) {
        return ESP_OK;
    }
    g_ctx.task_stop_req = true;

    // Simple wait for task exit
    int timeout = 0;
    while (g_ctx.task_handle != NULL && timeout++ < 100) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return ESP_OK;
}

bool bme280_sensor_task_running(void) { return g_ctx.task_handle != NULL; }

esp_err_t bme280_sensor_init_with_task(i2c_master_bus_handle_t i2c_bus, Bme280SampleCb callback,
                                       void* user_ctx) {
    Bme280SensorCfg scfg = {.i2c_bus = i2c_bus}; // i2c_bus ignored in sim
    ESP_ERROR_CHECK(bme280_sensor_init(&scfg));

    Bme280TaskCfg tcfg = {.poll_interval_ms = BME280_TASK_DEFAULT_INTERVAL_MS,
                          .callback         = callback,
                          .user_ctx         = user_ctx,
                          .stack_size       = BME280_TASK_DEFAULT_STACK_SIZE,
                          .priority         = BME280_TASK_DEFAULT_PRIORITY};
    return bme280_sensor_start_task(&tcfg);
}

esp_err_t bme280_sensor_deinit(void) {
    if (!g_ctx.initialised) {
        return ESP_OK;
    }
    bme280_sensor_stop_task();
    if (g_ctx.mutex) {
        vSemaphoreDelete(g_ctx.mutex);
    }
    memset(&g_ctx, 0, sizeof(g_ctx));
    return ESP_OK;
}
