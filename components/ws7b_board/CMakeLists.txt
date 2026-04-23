# ws7b_board — board bring-up for the Waveshare 7" ESP32-S3-Touch-LCD-7B board.
#
# Owns: I2C bus, CH32V003 IO expander, GT911 touch, RGB LCD panel.
# Exposes: ws7b_board_init(), ws7b_board_get_panel/touch(), ws7b_board_set_backlight().
#
idf_component_register(
    SRCS         "ws7b_board.c"
    INCLUDE_DIRS "include"
    REQUIRES
        driver
        esp_lcd
        esp_common
        log
        freertos
    PRIV_REQUIRES
        espressif__esp_lcd_touch_gt911
)
