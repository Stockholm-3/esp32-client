if(IDF_TARGET STREQUAL "linux")
    set(DISPLAY_SRC "display_linux.c")
    set(DISPLAY_REQS 
        log 
        esp_common 
        esp_timer
    )
    set(DISPLAY_PRIV_REQS
        lvgl__lvgl
    )
else()
    set(DISPLAY_SRC "display_esp.c")
    set(DISPLAY_REQS 
        esp_lcd 
        esp_lcd_touch 
        driver 
        freertos 
        log 
        esp_common 
        esp_timer 
        esp_driver_gpio
    )
    set(DISPLAY_PRIV_REQS
        espressif__esp_lcd_touch_gt911
        lvgl__lvgl
    )
endif()

idf_component_register(
    SRCS "${DISPLAY_SRC}"
    INCLUDE_DIRS "include"
    REQUIRES ${DISPLAY_REQS} lvgl__lvgl
    PRIV_REQUIRES ${DISPLAY_PRIV_REQS}
)

if(IDF_TARGET STREQUAL "linux")
    # ${COMPONENT_LIB} is the internal variable for this component's library
    target_link_libraries(${COMPONENT_LIB} PRIVATE SDL2 pthread)
    
    # Optional: If you use LVGL as a managed component on Linux too, 
    # ensure the include path is available. If LVGL is provided via 
    # idf_component_register, this is usually handled, but if you have 
    # issues with SDL2 headers not being found, you can add:
    # target_compile_options(${COMPONENT_LIB} PRIVATE "-D_REENTRANT")
endif()
