if(IDF_TARGET STREQUAL "linux")
    set(SRC "time_manager_linux_stub.c")
    set(REQS log)
else()
    set(SRC "time_manager_esp.c")
    set(REQS log lwip esp_timer)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${REQS}
)
