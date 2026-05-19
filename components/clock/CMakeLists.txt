idf_component_register(
    SRCS "clock.c"
    INCLUDE_DIRS "include"
    REQUIRES
        lvgl__lvgl
        time_manager
        ui_binder
)
