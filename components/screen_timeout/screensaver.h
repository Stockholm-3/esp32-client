#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the screensaver module.
 *        Creates the bouncing image and dark background.
 *        Must be called after lv_init().
 */
void screensaver_init(uint16_t scale);

/**
 * @brief Show the screensaver (start bouncing animation).
 */
void screensaver_show(void);

/**
 * @brief Hide the screensaver and clean up.
 */
void screensaver_hide(void);

/**
 * @brief Check if screensaver is currently visible.
 *
 * @return true if screensaver is active, false otherwise.
 */
bool screensaver_is_active(void);

#ifdef __cplusplus
}
#endif
