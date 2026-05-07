if(IDF_TARGET STREQUAL "linux")
    set(SRC "fs_linux.c")
    set(REQS log)
else()
    set(SRC "fs_esp.c")
    set(REQS log littlefs fatfs)
endif()

idf_component_register(
    SRCS ${SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${REQS}
)
