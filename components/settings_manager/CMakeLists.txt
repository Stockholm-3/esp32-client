if(IDF_TARGET STREQUAL "linux")
    set(SRC "settings_manager_linux_stub.c")
    set(REQS ui_binder)
else()
    set(SRC "settings_manager_esp.c")
    set(REQS ui_binder nvs_flash)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${REQS}
)
