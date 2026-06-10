#pragma once
#include "lvgl.h"

/**
 * @brief Apply Swedish character support to an LVGL keyboard object.
 *
 * Adds an "åäö" button to the standard lower- and upper-case layouts.
 * Pressing that button switches to a compact custom pad (USER_1) that
 * contains å, ä, ö, Å, Ä and Ö. Must be called after
 * @c lv_keyboard_create().
 *
 * @param kb  Pointer to the LVGL keyboard object to patch.
 */
void kb_apply_swedish(lv_obj_t *kb);
