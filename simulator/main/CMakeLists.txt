idf_component_register(
    SRCS "../../main/main.c"
    INCLUDE_DIRS "../../main"
    REQUIRES display screen_timeout bme280_sensor ui log ws7b_board time_manager ui_binder settings_manager wifi_manager lib esp_idf_stubs loc_server
)
