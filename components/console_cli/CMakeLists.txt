if(IDF_TARGET STREQUAL "linux")
    set(SRC "console_cli_stub.c")
    set(REQS log)
else()
    set(SRC "console_cli.c")
    set(REQS console bme280_sensor esp_driver_uart vfs esp_timer esp_hw_support spi_flash)

endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${REQS}
)
