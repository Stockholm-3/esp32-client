idf_component_register(
    SRCS "screen_timeout.c" "screensaver.c"
    INCLUDE_DIRS "include"
    REQUIRES display lvgl__lvgl
)
