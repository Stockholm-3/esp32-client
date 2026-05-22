#include "settings_manager.h"

void settings_manager_init(void) {}
const char* settings_manager_get_location(void) { return ""; }
int settings_manager_get_price_zone(void) { return 0; }
int settings_manager_get_timeout(void) { return 0; }
const char* settings_manager_get_ssid(void) { return ""; }
const char* settings_manager_get_password(void) { return ""; }
void settings_manager_save_wifi(const char* ssid, const char* password) {
    (void)ssid;
    (void)password;
}
uint8_t settings_manager_get_all_networks(SavedWifiNetwork* out_list, uint8_t max_count) {
    return 0;
}
