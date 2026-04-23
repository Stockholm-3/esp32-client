# display — LVGL wiring layer.
#
# Selects display_esp.c on ESP targets (depends on ws7b_board for hardware)
# or display_linux.c on the Linux target (SDL2 + pthread, no hardware deps).

if(IDF_TARGET STREQUAL "linux")
    set(DISPLAY_SRCS "display_linux.c")
    set(DISPLAY_REQUIRES
        log
        esp_common
        lvgl__lvgl
    )
    set(DISPLAY_PRIV_REQUIRES "")
else()
    set(DISPLAY_SRCS "display_esp.c")
    set(DISPLAY_REQUIRES
        ws7b_board
        esp_lcd
        esp_timer
        freertos
        log
        esp_common
        lvgl__lvgl
    )
    set(DISPLAY_PRIV_REQUIRES
        espressif__esp_lcd_touch_gt911
    )
endif()

idf_component_register(
    SRCS         ${DISPLAY_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES     ${DISPLAY_REQUIRES}
    PRIV_REQUIRES ${DISPLAY_PRIV_REQUIRES}
)

if(IDF_TARGET STREQUAL "linux")
    target_link_libraries(${COMPONENT_LIB} PRIVATE SDL2 pthread)
endif()
