#include "lvgl.h"
#include "sim_board.h"
#include <stdio.h>

void app_main(void) {
    lv_disp_t *disp;
    lv_indev_t *mouse;

    if (sim_board_init(&disp, &mouse) != ESP_OK) {
        printf("sim_board_init failed!\n");
        return;
    }

    // Main loop: handle SDL events + LVGL
    sim_board_loop(); // <- this must never return
}
