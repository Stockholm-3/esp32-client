idf_component_register(
    SRCS "ws7b_board.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_lcd
        esp_lcd_touch
        driver
        freertos
        log
        esp_common
        esp_timer
        esp_driver_gpio
    PRIV_REQUIRES
        espressif__esp_lcd_touch_gt911
        lvgl__lvgl
)
