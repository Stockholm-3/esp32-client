idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "include"
    REQUIRES log freertos esp_wifi nvs_flash 
)
