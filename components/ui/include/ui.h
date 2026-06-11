/**
 * @file ui.h
 * @defgroup ui UI Component
 * @brief Top-level UI component entry point.
 *
 * Provides a single entry point to initialise and build the entire LVGL
 * user interface: home screen, tabs (HOME, WEATHER, ELPRIS, SETTINGS),
 * on-screen keyboard, WiFi popup and screen timeout handling.
 * @{
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise and build the user interface.
 *
 * Calls the SquareLine-generated @c ui_init(), then applies the Swedish
 * keyboard layout, sets keyboard visibility, wires the timeout dropdown
 * and initialises the WiFi popup. Must be called exactly once after LVGL
 * has been initialised and a display registered.
 *
 * @param disp  Pointer to the active LVGL display.
 */
void ui_build(lv_display_t* disp);

#ifdef __cplusplus
}
#endif

/** @} */
