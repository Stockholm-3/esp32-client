if(IDF_TARGET STREQUAL "linux")
    set(SRC "env_sensor_linux.c")
else()
    set(SRC "env_sensor_esp.c")
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES driver ws7b_board lvgl__lvgl
)
