cmake_minimum_required(VERSION 3.16)

# 1. Look one directory up into the root components folder for 'test' directories
file(GLOB component_tests RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../components" "${CMAKE_CURRENT_SOURCE_DIR}/../components/*/test")

set(TEST_COMPONENTS "")
set(COMPONENTS_WITH_TESTS "")

foreach(test_path ${component_tests})
    string(REPLACE "/test" "" component_name ${test_path})
    list(APPEND TEST_COMPONENTS ${component_name})
    
    # Construct the absolute path to this specific component
    list(APPEND COMPONENTS_WITH_TESTS "${CMAKE_CURRENT_SOURCE_DIR}/../components/${component_name}")
endforeach()

# 2. Force the build system to ONLY look at components that actually have a test folder
set(EXTRA_COMPONENT_DIRS ${COMPONENTS_WITH_TESTS})

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(test_app)
