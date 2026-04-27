idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        display
        ui
)

target_include_directories(__idf_main PRIVATE ${CMAKE_SOURCE_DIR})
