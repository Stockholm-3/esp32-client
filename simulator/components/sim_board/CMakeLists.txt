idf_component_register(
    SRCS "sim_board.c"
    INCLUDE_DIRS "include"
)

target_link_libraries(${COMPONENT_LIB} PRIVATE SDL2 pthread)
