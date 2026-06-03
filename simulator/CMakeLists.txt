cmake_minimum_required(VERSION 3.16)

# ── CRITICAL for linux target ─────────────────────────────────────────────────
# Without this, IDF auto-includes every component — including hardware-only ones
# like esp_driver_ana_cmpr → esp_hal_ana_cmpr that don't exist on linux.
# Setting COMPONENTS to "main" makes IDF only pull in what main explicitly
# requires (transitively: sim_board, ui, esp_lvgl_port, lvgl, freertos, log…)
set(COMPONENTS main)

# ── Tell IDF where to find our shared components ─────────────────────────────
# The ui component lives one level up, alongside ws7b_board.
# We add the parent's components/ directory so IDF can resolve it.
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../components"          # shared: ui
    "${CMAKE_CURRENT_SOURCE_DIR}/../managed_components"  # shared: cjson, etc.
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(simulator)

# Force-include supplement font symbols so the linker resolves montserrat_12/14
# referenced from managed liblvgl__lvgl.a after font patching.
target_link_options(${CMAKE_PROJECT_NAME}.elf PRIVATE
    -Wl,-u,montserrat_12
    -Wl,-u,montserrat_14
)

# Patch shared managed LVGL fonts (../managed_components) for Swedish support.
# Runs here too in case linux-build is invoked without a prior ESP32 build.
find_program(PYTHON3 python3)
if(PYTHON3)
    execute_process(
        COMMAND ${PYTHON3} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/patch_lvgl_fonts.py
        RESULT_VARIABLE _patch_rc
    )
    if(NOT _patch_rc EQUAL 0)
        message(WARNING "patch_lvgl_fonts.py failed — Swedish chars may not render")
    endif()
endif()
