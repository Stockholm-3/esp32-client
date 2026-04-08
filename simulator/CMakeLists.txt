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
    "${CMAKE_CURRENT_SOURCE_DIR}/../components"   # shared: ui
    "${CMAKE_CURRENT_SOURCE_DIR}/components"      # simulator-only: sim_board
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(simulator)
