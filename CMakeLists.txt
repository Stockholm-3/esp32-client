cmake_minimum_required(VERSION 3.5)

set(IDF_TARGET "esp32s3")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(app-template)

# LVGL's stress demo has a %d/%lu format mismatch in its own source.
# It's in managed_components so we can't edit it — suppress for that target only.
target_compile_options(__idf_lvgl__lvgl PRIVATE -Wno-format)
