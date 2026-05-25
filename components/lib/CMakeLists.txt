idf_component_register(
    SRCS 
        "bingus-lib/src/smw.c"
        "bingus-lib/src/cache.c"
        # Add other source files as needed
    INCLUDE_DIRS 
        "bingus-lib/include"
    REQUIRES log
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-format -Wno-format-truncation)