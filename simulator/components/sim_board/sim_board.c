/*
 * sim_board.c — LVGL + SDL initialisation for the Linux simulator.
 *
 * esp_lvgl_port (≥ 2.4) ships a native Linux/SDL backend.  When the
 * IDF target is "linux" the component automatically links against SDL2
 * and registers an SDL display + mouse input device.  All we have to
 * do here is call the port's init / add-display / add-mouse helpers
 * and expose the same mutex wrappers the real ws7b_board provides.
 */

#include "sim_board.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *TAG = "sim_board";

esp_err_t sim_board_init(lv_disp_t **disp_out, lv_indev_t **touch_out) {
    // ── 1. Initialise the port (calls lv_init internally) ────────────────────
    const lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    esp_err_t ret = lvgl_port_init(&port_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "lvgl_port_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // ── 2. Add an SDL display
    // ─────────────────────────────────────────────────
    const lvgl_port_display_linux_cfg_t disp_cfg = {
        .hres = SIM_LCD_H_RES,
        .vres = SIM_LCD_V_RES,
    };
    lv_disp_t *disp = lvgl_port_add_disp_linux(&disp_cfg);
    if (!disp) {
        ESP_LOGE(TAG, "lvgl_port_add_disp_linux failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "SDL display %dx%d ready", SIM_LCD_H_RES, SIM_LCD_V_RES);

    // ── 3. Add SDL mouse / touch input
    // ────────────────────────────────────────
    lv_indev_t *indev = NULL;
    const lvgl_port_indev_linux_cfg_t mouse_cfg = {
        .type = LVGL_PORT_INDEV_TYPE_POINTER,
        .disp = disp,
    };
    indev = lvgl_port_add_indev_linux(&mouse_cfg);
    if (!indev) {
        // Non-fatal — simulator is still usable without mouse input
        ESP_LOGW(TAG, "lvgl_port_add_indev_linux failed (mouse unavailable)");
    }

    if (disp_out)
        *disp_out = disp;
    if (touch_out)
        *touch_out = indev;

    ESP_LOGI(TAG, "sim_board initialised");
    return ESP_OK;
}

bool sim_board_lvgl_lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void sim_board_lvgl_unlock(void) { lvgl_port_unlock(); }
