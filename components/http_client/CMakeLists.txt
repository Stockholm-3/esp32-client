if(IDF_TARGET STREQUAL "linux")
    set(SRC "http_client_linux.c")
    set(REQS log)
else()
    set(SRC "http_client_esp.c")
    set(REQS log esp_http_client)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${REQS} 
)

if(IDF_TARGET STREQUAL "linux")
    find_package(CURL REQUIRED)

    target_link_libraries(${COMPONENT_LIB} PRIVATE CURL::libcurl)
endif()
