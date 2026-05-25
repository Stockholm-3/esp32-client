if(IDF_TARGET STREQUAL "linux")
    set(SRC "ui_binder_linux.c")
    set(PRIV_REQS ui)
else()
    set(SRC "ui_binder_esp.c")
    set(PRIV_REQS display ui fs)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    REQUIRES wifi_manager bme280_sensor
    PRIV_REQUIRES ${PRIV_REQS}
)
