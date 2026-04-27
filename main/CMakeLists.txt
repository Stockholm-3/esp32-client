idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        display
        screen_timeout
        ui
)

target_include_directories(__idf_main PRIVATE ${CMAKE_SOURCE_DIR})
