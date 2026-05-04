if(IDF_TARGET STREQUAL "linux")
    idf_component_register(INCLUDE_DIRS "include")
else()
    idf_component_register(
        SRCS "ws7b_board.c"
        INCLUDE_DIRS "include"
        REQUIRES ws7b_config esp_lcd esp_common log freertos driver
        PRIV_REQUIRES espressif__esp_lcd_touch_gt911
    )
endif()
