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

# Always include the top-level components dir so shared deps (e.g. lib) are found
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../components")

# Include managed_components from the root project (e.g. littlefs, lvgl, etc.)
list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components")

# Use the root project's sdkconfig.defaults so settings like fonts are enabled,
# then apply test-specific overrides (e.g. partition table path).
set(SDKCONFIG_DEFAULTS
    "${CMAKE_CURRENT_SOURCE_DIR}/../sdkconfig.defaults"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdkconfig.defaults")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app)

