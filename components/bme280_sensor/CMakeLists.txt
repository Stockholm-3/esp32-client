if(IDF_TARGET STREQUAL "linux")
    set(SRC "bme280_sensor_linux_stub.c")
    set(REQS log freertos esp_driver_i2c)
else()
    set(SRC "bme280_sensor_esp.cpp")
    set(REQS log freertos esp_driver_i2c)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${REQS}
)
