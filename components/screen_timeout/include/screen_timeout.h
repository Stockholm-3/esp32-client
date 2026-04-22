#pragma once
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the screen timeout module.
 *
 * Must be called after display_init() and lv_init(), while holding the LVGL
 * mutex. Creates the inactivity timer and the dim overlay object.
 *
 * @param default_timeout_seconds Initial timeout (default 5 * 60).
 */
void screen_timeout_init(uint32_t default_timeout_seconds);

/**
 * @brief Record user activity (touch). Hides the dim overlay and restores
 *        backlight if the screensaver was active. Resets the inactivity timer.
 */
void screen_timeout_record_activity(void);

/**
 * @brief Set the screensaver timeout.
 *
 * @param timeout_seconds Seconds of inactivity before screen turns off.
 *                        Pass 0 to disable.
 */
void screen_timeout_set_seconds(uint32_t timeout_seconds);

#ifdef __cplusplus
}
#endif
