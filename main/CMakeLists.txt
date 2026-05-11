idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        display
        screen_timeout
        ui
        bme280_sensor
        clock
        ui_binder
        time_manager
        settings_manager
        wifi_manager
        lib
        loc_server
        mdns
        fs
        cache_fs
        smw_http
        http_client
        data_fetcher
        trigger_manager
)

target_include_directories(__idf_main PRIVATE ${CMAKE_SOURCE_DIR})
