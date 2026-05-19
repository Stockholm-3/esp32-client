/*
 * clock.c — Periodic clock display using an LVGL timer.
 *
 * Registers a 1-second LVGL timer that reads the current time from
 * time_manager and forwards it to ui_binder_update_localtime().
 *
 * Running inside the LVGL timer callback means the LVGL task already holds
 * its (recursive) mutex, so ui_binder_update_localtime() can safely re-take
 * it on the ESP target, and the Linux stub skips locking entirely.
 */

#include "clock.h"

#include "lvgl.h"
#include "time_manager.h"
#include "ui_binder.h"

#define CLOCK_UPDATE_INTERVAL_MS 1000U

static lv_timer_t* g_timer = NULL;

static void clock_tick(lv_timer_t* t) {
    (void)t;
    struct tm tm_info;
    if (time_manager_get_time(&tm_info)) {
        ui_binder_update_localtime(&tm_info);
    }
}

void clock_init(void) {
    if (g_timer == NULL) {
        g_timer = lv_timer_create(clock_tick, CLOCK_UPDATE_INTERVAL_MS, NULL);
    }
}

void clock_deinit(void) {
    if (g_timer != NULL) {
        lv_timer_delete(g_timer);
        g_timer = NULL;
    }
}
