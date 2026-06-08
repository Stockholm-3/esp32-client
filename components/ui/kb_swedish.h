#pragma once
#include "lvgl.h"

/* Apply Swedish character support to an lv_keyboard object.
 * Adds an "åäö" button to the standard lower/upper layouts that switches
 * to a compact Swedish pad (USER_1) with å ä ö Å Ä Ö.
 * Must be called after lv_keyboard_create(). */
void kb_apply_swedish(lv_obj_t* kb);
