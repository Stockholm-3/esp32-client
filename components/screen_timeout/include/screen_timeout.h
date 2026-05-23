#pragma once

#include "lvgl.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Timeout configuration structure.
 *
 * All timeouts in seconds. Use 0 to disable a stage.
 * Stages must be in increasing order; validation happens at init.
 *
 * Example: dim at 5 min, screensaver at 10 min, backlight off at 30 min:
 *   screen_timeout_config_t cfg = {
 *       .dim_timeout_seconds = 5 * 60,
 *       .screensaver_timeout_seconds = 10 * 60,
 *       .backlight_off_timeout_seconds = 30 * 60,
 *   };
 */
typedef struct {
    uint32_t dim_timeout_seconds;           // Stage 1: dim overlay appears
    uint32_t screensaver_timeout_seconds;   // Stage 2: bouncing screensaver
    uint32_t backlight_off_timeout_seconds; // Stage 3: backlight turns off
} ScreenTimeoutConfig;

/**
 * @brief Initialize the screen timeout module with configurable stages.
 *
 * Must be called after display_init() and lv_init(), while holding the LVGL
 * mutex.
 *
 * Validation:
 * - Negative values are treated as 0 (disabled).
 * - Stages with 0 are skipped.
 * - If a later stage has a lower timeout than an earlier stage,
 *   they are auto-corrected to maintain logical order.
 * - Example: dim=300, screensaver=250 → screensaver becomes 300 (same as dim).
 *
 * @param config Configuration with three optional timeout stages.
 */
void screen_timeout_init(const ScreenTimeoutConfig* config);

/**
 * @brief Record user activity (e.g., touch event).
 *
 * Immediately:
 * - Hides all overlays (dim and screensaver)
 * - Restores backlight to full brightness
 * - Resets the inactivity timer
 */
void screen_timeout_record_activity(void);

/**
 * @brief Reconfigure timeout values at runtime.
 *
 * Same validation rules as screen_timeout_init().
 * If currently in a timeout state, this may trigger state changes.
 *
 * @param config New configuration.
 */
void screen_timeout_set_config(const ScreenTimeoutConfig* config);

/**
 * @brief Get the current timeout configuration.
 *
 * @param config Pointer to structure to fill.
 */
void screen_timeout_get_config(ScreenTimeoutConfig* config);

/**
 * @brief Set the backlight level used when the screen is active.
 *
 * Call this whenever the user changes the brightness slider.
 * The value is immediately applied if the screen is currently active,
 * and is restored whenever the screen wakes from dim/off.
 *
 * @param brightness  Hardware brightness value (0–255).
 */
void screen_timeout_set_active_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif
