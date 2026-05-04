#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise LVGL and wire it to the display hardware.
 *
 * ESP target  — calls ws7b_board_init(), registers vsync-locked double-buffer
 *               flush, touch indev, tick timer, and LVGL task.
 * Linux target — opens an SDL2 window and wires LVGL to it.
 *
 * Must be called once from app_main before any lv_* calls are made.
 *
 * @param disp_out   Receives the lv_display_t* handle (may be NULL).
 * @param touch_out  Receives the lv_indev_t*   handle (may be NULL).
 * @return ESP_OK on success, propagated ESP error on any failure.
 */
esp_err_t display_init(lv_display_t** disp_out, lv_indev_t** touch_out);

/**
 * @brief Acquire the LVGL mutex before calling any lv_* function.
 *
 * Re-entrant: may be called multiple times from the same task provided each
 * call is paired with a corresponding display_lvgl_unlock().
 *
 * @param timeout_ms  Maximum wait in milliseconds; -1 waits forever.
 * @return true  if the mutex was acquired.
 * @return false if the timeout expired.
 */
bool display_lvgl_lock(int timeout_ms);

/**
 * @brief Release the LVGL mutex acquired by display_lvgl_lock().
 */
void display_lvgl_unlock(void);

/**
 * @brief Register a callback invoked on every touch event.
 *        Use this to notify screen_timeout of user activity.
 */
void display_set_activity_callback(void (*cb)(void));

/**
 * @brief Set the LCD backlight brightness.
 *
 * Delegates to ws7b_board_set_backlight() on the ESP target; no-op on Linux.
 *
 * @param brightness  0 = off, 255 = full brightness.
 */
void display_set_backlight(uint8_t brightness);

/**
 * @brief Return idle CPU percentage (0-100) for use with LVGL SYSMON.
 *
 * Calculated from FreeRTOS run-time stats on the ESP target.
 * Always returns 0 on the Linux simulator.
 */
uint32_t display_get_idle_percent(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H
