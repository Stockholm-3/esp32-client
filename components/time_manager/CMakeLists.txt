# time_manager/CMakeLists.txt

if(IDF_TARGET STREQUAL "linux")
    # Linux/Simulator version - no network dependencies
    set(TIME_SRC "time_manager_linux_stub.c")
    set(TIME_REQS log)
else()
    # ESP32 version - uses SNTP
    set(TIME_SRC "time_manager_esp.c")
    set(TIME_REQS log freertos lwip esp_timer)
endif()

idf_component_register(
    SRCS ${TIME_SRC}
    INCLUDE_DIRS "include"
    REQUIRES ${TIME_REQS}
)