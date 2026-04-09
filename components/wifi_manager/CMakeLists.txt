if(IDF_TARGET STREQUAL "linux")
    set(WIFI_SRC "wifi_manager_linux_stub.c")
    set(WIFI_REQS log)
else()
    set(WIFI_SRC "wifi_manager_esp.c")
    set(WIFI_REQS log freertos esp_wifi nvs_flash esp_timer)
endif()

idf_component_register(
    SRCS ${WIFI_SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${WIFI_REQS}
)
