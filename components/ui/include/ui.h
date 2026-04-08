#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build the application UI on the given display.
 *        Must be called while holding the LVGL mutex.
 *
 * @param disp  Active LVGL display (pass NULL to use the default display)
 */
void ui_build(lv_disp_t *disp);

#ifdef __cplusplus
}
#endif
