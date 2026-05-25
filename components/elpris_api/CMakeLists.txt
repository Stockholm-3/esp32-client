if(IDF_TARGET STREQUAL "linux")
    # Linux/simulator version - uses system time and libcurl
    set(SRC "elpris_api.c")
    set(REQS log)
    
    idf_component_register(
        SRCS ${SRC}
        INCLUDE_DIRS "."          
        REQUIRES ${REQS}
    )
    
    find_package(CURL REQUIRED)
    target_link_libraries(${COMPONENT_LIB} PRIVATE CURL::libcurl)
else()
    # ESP32 version - uses SNTP and esp_http_client
    set(SRC "elpris_api.c")
    set(REQS log http_client time_manager)
    
    idf_component_register(
        SRCS ${SRC}
        INCLUDE_DIRS "include"          
        REQUIRES ${REQS}
    )
endif()