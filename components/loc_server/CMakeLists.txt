idf_component_register(
    SRCS "loc_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server settings_manager ui_binder wifi_manager display log freertos cjson
)

target_add_binary_data(${COMPONENT_LIB} "www/index.html" TEXT)
target_add_binary_data(${COMPONENT_LIB} "www/style.css"  TEXT)
target_add_binary_data(${COMPONENT_LIB} "www/app.js"     TEXT)
