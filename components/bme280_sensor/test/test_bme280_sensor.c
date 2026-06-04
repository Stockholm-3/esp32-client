#include "bme280_sensor.h"
#include "unity.h"

// ── Pre-init guards ──────────────────────────────────────────────────────────
// These tests verify the public API's defensive behaviour before
// bme280_sensor_init() has been called successfully.

TEST_CASE("bme280_sensor_init rejects null config", "[bme280][logic]") {
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_ARG, bme280_sensor_init(NULL));
}

TEST_CASE("bme280_sensor_read returns INVALID_STATE before init", "[bme280][logic]") {
    Bme280Reading r = {0};
    esp_err_t err   = bme280_sensor_read(&r);
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_STATE, err);
}

TEST_CASE("bme280_sensor_get_last returns INVALID_STATE before init", "[bme280][logic]") {
    Bme280Reading r = {0};
    esp_err_t err   = bme280_sensor_get_last(&r);
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_STATE, err);
}

TEST_CASE("bme280_sensor_read null pointer returns error", "[bme280][logic]") {
    esp_err_t err = bme280_sensor_read(NULL);
    // Must return an error code, not crash
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
}

TEST_CASE("bme280_sensor_get_last null pointer returns error", "[bme280][logic]") {
    esp_err_t err = bme280_sensor_get_last(NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, err);
}

TEST_CASE("bme280_sensor_task_running returns false before init", "[bme280][logic]") {
    TEST_ASSERT_FALSE(bme280_sensor_task_running());
}

TEST_CASE("bme280_sensor_stop_task is safe before init", "[bme280][logic]") {
    // Must not crash; documented to return ESP_OK immediately if no task running
    esp_err_t err = bme280_sensor_stop_task();
    TEST_ASSERT_EQUAL_HEX(ESP_OK, err);
}

// ── Struct layout ────────────────────────────────────────────────────────────

TEST_CASE("Bme280Reading fields are zero-initialised by value-init", "[bme280][logic]") {
    Bme280Reading r = {0};
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.temperature_c);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.pressure_hpa);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, r.humidity_pct);
}
