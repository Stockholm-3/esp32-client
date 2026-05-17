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
void settings_manager_save_location(const char* city) { (void)city; }
void settings_manager_save_price_zone(int index) { (void)index; }
void settings_manager_save_timeout(int index) { (void)index; }
bool settings_manager_get_local_web_client_enabled(void) { return false; }
const char* settings_manager_get_sta_static_ip(void) { return ""; }
const char* settings_manager_get_sta_gateway(void) { return ""; }
const char* settings_manager_get_sta_netmask(void) { return ""; }
const char* settings_manager_get_mdns_hostname(void) { return "esp32-client"; }
bool settings_manager_get_ap_enabled(void) { return false; }
void settings_manager_save_local_web_client_enabled(bool e) { (void)e; }
void settings_manager_save_sta_static_ip(const char* ip) { (void)ip; }
void settings_manager_save_sta_gateway(const char* gw) { (void)gw; }
void settings_manager_save_sta_netmask(const char* nm) { (void)nm; }
void settings_manager_save_mdns_hostname(const char* h) { (void)h; }
void settings_manager_save_ap_enabled(bool e) { (void)e; }
