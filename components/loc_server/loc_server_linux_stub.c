#include "esp_log.h"
#include "loc_server.h"

static const char* g_tag = "loc_server";

void loc_server_init(void) { ESP_LOGI(g_tag, "stub: init"); }
esp_err_t loc_server_start(void) {
    ESP_LOGI(g_tag, "stub: start");
    return ESP_OK;
}
void loc_server_stop(void) {}
void loc_server_push_settings(void) {}
void loc_server_notify_wifi_state(WifiManagerState state) { (void)state; }
