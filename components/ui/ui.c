#include "ui.h"
#include "squareline/ui.h"

void ui_build(lv_disp_t* disp) {
    (void)disp;  // ui_init() uses lv_disp_get_default() internally
    ui_init();
}
