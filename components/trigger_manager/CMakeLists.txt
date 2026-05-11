

idf_component_register (
    SRCS "trigger_manager.c"
    INCLUDE_DIRS "."
    REQUIRES ui_binder time_manager wifi_manager bme280_sensor 
    
)