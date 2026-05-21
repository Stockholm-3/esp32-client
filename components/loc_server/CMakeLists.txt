if(IDF_TARGET STREQUAL "linux")
    set(LOC_SRC "loc_server_linux_stub.c")
    set(LOC_REQS log wifi_manager)
else()
    set(LOC_SRC "loc_server.c")
    set(LOC_REQS esp_http_server esp_netif settings_manager ui_binder wifi_manager display log freertos espressif__cjson)
endif()

idf_component_register(
    SRCS ${LOC_SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${LOC_REQS}
)

if(NOT IDF_TARGET STREQUAL "linux")
    target_add_binary_data(${COMPONENT_LIB} "www/index.html" TEXT)
    target_add_binary_data(${COMPONENT_LIB} "www/style.css"  TEXT)
    target_add_binary_data(${COMPONENT_LIB} "www/app.js"     TEXT)
endif()
