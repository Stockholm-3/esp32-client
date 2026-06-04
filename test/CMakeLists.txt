cmake_minimum_required(VERSION 3.16)

# Auto-discover components that have a test/ subdirectory
file(GLOB component_tests
     LIST_DIRECTORIES true
     "${CMAKE_CURRENT_SOURCE_DIR}/../components/*/test")

set(EXTRA_COMPONENT_DIRS "")
foreach(test_dir ${component_tests})
    get_filename_component(comp_dir "${test_dir}" DIRECTORY)
    list(APPEND EXTRA_COMPONENT_DIRS "${comp_dir}")
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app)

