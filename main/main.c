#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ws7b_board.h"

void app_main(void) {
    lv_disp_t *disp = NULL;
    lv_indev_t *touch = NULL;

    ESP_ERROR_CHECK(ws7b_board_init(&disp, &touch));
    ESP_LOGI("main", "init done, heap free: %lu", esp_get_free_heap_size());

    // Use -1 (portMAX_DELAY) not 0 here — 0 means "give up immediately if
    // the mutex is taken", which can silently fail and leave LVGL unprotected.
    if (!ws7b_lvgl_lock(-1)) {
        ESP_LOGE("main", "Failed to acquire LVGL lock");
        return;
    }

    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    // Bright red box to confirm objects render correctly
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 400, 100);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(box);
    lv_label_set_text(label, "Hello 7B!");
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFF00), LV_PART_MAIN);
    lv_obj_center(label);

    ws7b_lvgl_unlock();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
