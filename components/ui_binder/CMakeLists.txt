if(IDF_TARGET STREQUAL "linux")
    set(SRC "ui_binder_linux.c")
    set(REQS ui)
else()
    set(SRC "ui_binder_esp.c")
    set(REQS display ui)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    PRIV_REQUIRES ${REQS}
)
