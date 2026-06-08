cmake_minimum_required(VERSION 3.5)

set(IDF_TARGET "esp32s3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(app-template)

# LVGL's stress demo has a %d/%lu format mismatch in its own source.
# It's in managed_components so we can't edit it — suppress for that target only.
target_compile_options(__idf_lvgl__lvgl PRIVATE -Wno-format)

# Force-include supplement font symbols so the linker resolves montserrat_12/14
# referenced from managed liblvgl__lvgl.a after font patching.
target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
    -Wl,-u,montserrat_12
    -Wl,-u,montserrat_14
)

littlefs_create_partition_image(storage spiffs_image FLASH_IN_PROJECT)

# Patch managed LVGL font files to add .fallback → Swedish supplement fonts.
# Runs AFTER project() so managed_components are already downloaded by IDF.
find_program(PYTHON3 python3)
if(PYTHON3)
    execute_process(
        COMMAND ${PYTHON3} ${CMAKE_SOURCE_DIR}/scripts/patch_lvgl_fonts.py
        RESULT_VARIABLE _patch_rc
    )
    if(NOT _patch_rc EQUAL 0)
        message(WARNING "patch_lvgl_fonts.py failed — Swedish chars may not render")
    endif()
else()
    message(WARNING "python3 not found — Swedish font patch skipped")
endif()
