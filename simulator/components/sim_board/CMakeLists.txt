idf_component_register(
    SRCS "sim_board.c"
    INCLUDE_DIRS "include"
    REQUIRES
        espressif__esp_lvgl_port
        lvgl__lvgl
        log
        freertos
)
