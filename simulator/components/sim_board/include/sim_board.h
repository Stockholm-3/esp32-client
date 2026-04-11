#pragma once

#include "esp_err.h"
#include "lvgl.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ────────────── Simulator LCD resolution ────────────── */
#define SIM_LCD_H_RES 1024
#define SIM_LCD_V_RES 600

/* ────────────── Simulator API ────────────── */

/**
 * @brief Initialize the simulator board (SDL + LVGL)
 * @param disp_out Optional pointer to receive the LVGL display
 * @param touch_out Optional pointer to receive the LVGL input device (mouse)
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t sim_board_init(lv_disp_t** disp_out, lv_indev_t** touch_out);

/**
 * @brief Lock LVGL for thread-safe operations
 * @param timeout_ms Timeout in milliseconds (ignored, always blocks)
 * @return true if lock acquired, false otherwise
 */
bool sim_board_lvgl_lock(int timeout_ms);

/**
 * @brief Unlock LVGL after a previous lock
 */
void sim_board_lvgl_unlock(void);

/**
 * @brief Run the main simulator loop
 * This function never returns. It handles SDL events and LVGL timers.
 */
void sim_board_loop(void);

/* ────────────── Compatibility aliases ────────────── */
#define ws7b_board_init sim_board_init
#define ws7b_lvgl_lock sim_board_lvgl_lock
#define ws7b_lvgl_unlock sim_board_lvgl_unlock

#ifdef __cplusplus
}
#endif
