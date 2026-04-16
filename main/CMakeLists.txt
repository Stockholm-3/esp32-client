idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_event
        lwip
        log
        freertos
         wifi_manager
        time_manager
)

target_include_directories(__idf_main PRIVATE ${CMAKE_SOURCE_DIR})
