#ifndef I2C_MASTER_STUB_H
#define I2C_MASTER_STUB_H

#include "esp_err.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mock handles as void pointers
typedef void* i2c_master_bus_handle_t;
typedef void* i2c_master_dev_handle_t;

typedef enum {
    I2C_ADDR_BIT_LEN_7 = 0,
    I2C_ADDR_BIT_LEN_10,
} i2c_addr_bit_len_t;

typedef struct {
    i2c_addr_bit_len_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
} i2c_device_config_t;

// Stub function prototypes to satisfy the linker
static inline esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                                  const i2c_device_config_t* dev_config,
                                                  i2c_master_dev_handle_t* ret_handle) {
    return ESP_OK;
}

static inline esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev_handle) {
    return ESP_OK;
}

static inline esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev_handle, const uint8_t* data,
                                            size_t size, int xfer_timeout_ms) {
    return ESP_OK;
}

static inline esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev_handle,
                                                    const uint8_t* write_buffer, size_t write_size,
                                                    uint8_t* read_buffer, size_t read_size,
                                                    int xfer_timeout_ms) {
    return ESP_OK;
}

static inline esp_err_t i2c_master_probe(i2c_master_bus_handle_t bus_handle, uint16_t address,
                                         int xfer_timeout_ms) {
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif

#endif // I2C_MASTER_STUB_H
