#pragma once

/*
 * sim_board.h — Linux/SDL simulator shim.
 *
 * Exposes the same lock/unlock API as ws7b_board so that code written
 * against the real board compiles unchanged in the simulator.
 *
 * Resolution is kept at 1024×600 to match the real WS7B panel.
 */

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SIM_LCD_H_RES 1024
#define SIM_LCD_V_RES 600

/**
 * @brief Initialise LVGL + SDL window via esp_lvgl_port.
 *        Must be called once before any lv_* calls.
 *
 * @param disp_out   receives the lv_disp_t* (may be NULL)
 * @param touch_out  receives the lv_indev_t* (may be NULL)
 */
esp_err_t sim_board_init(lv_disp_t **disp_out, lv_indev_t **touch_out);

/** @brief Acquire LVGL mutex before calling any lv_* functions. */
bool sim_board_lvgl_lock(int timeout_ms);

/** @brief Release LVGL mutex. */
void sim_board_lvgl_unlock(void);

/*
 * Compatibility aliases — lets simulator/main/main.c use the same
 * ws7b_* names as the real main without any #ifdefs.
 */
#define ws7b_board_init sim_board_init
#define ws7b_lvgl_lock sim_board_lvgl_lock
#define ws7b_lvgl_unlock sim_board_lvgl_unlock

#ifdef __cplusplus
}
#endif
