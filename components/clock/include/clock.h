#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialise the clock module.
 *
 * Registers an LVGL timer that fires every second, reads the current time
 * from time_manager, and pushes it to ui_binder_update_localtime().
 *
 * Must be called once while the LVGL lock is held (e.g. from ui_build()).
 */
void clock_init(void);

/**
 * @brief Tear down the clock module and cancel the LVGL timer.
 *
 * Must be called while the LVGL lock is held.
 */
void clock_deinit(void);

#ifdef __cplusplus
}
#endif
