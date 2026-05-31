idf_component_register(SRCS "console_cli.c"
                       INCLUDE_DIRS "include"
                       REQUIRES console bme280_sensor esp_driver_uart vfs esp_timer esp_hw_support spi_flash)
