#include "unity.h"
#include "wifi_manager.h"

// ── is_dns_ready — before wifi_manager_start() ──────────────────────────────
// esp_netif_get_handle_from_ifkey returns NULL if the WIFI_STA_DEF
// interface has not been created yet, so is_dns_ready() safely returns false.

TEST_CASE("is_dns_ready returns false when wifi not started", "[wifi_manager][logic]") {
    TEST_ASSERT_FALSE(is_dns_ready());
}

// ── WifiManagerConfig defaults ──────────────────────────────────────────────
// Verifies that the documented defaults match the header constants.

TEST_CASE("MAX_SAVED_NETWORKS constant is at least 1", "[wifi_manager][logic]") {
    TEST_ASSERT_GREATER_THAN(0, MAX_SAVED_NETWORKS);
}

TEST_CASE("WifiManagerApInfo ssid field has room for 32-char SSID plus NUL",
          "[wifi_manager][logic]") {
    WifiManagerApInfo ap = {0};
    // ssid[33] must accommodate a 32-character SSID plus null terminator
    TEST_ASSERT_EQUAL(33, sizeof(ap.ssid));
}

TEST_CASE("SavedWifiNetwork password field is large enough for WPA2", "[wifi_manager][logic]") {
    SavedWifiNetwork nw = {0};
    // WPA2 max passphrase is 63 chars + NUL = 64
    TEST_ASSERT_GREATER_OR_EQUAL(64, sizeof(nw.password));
}

// ── wifi_manager_stop — safe before start ────────────────────────────────

TEST_CASE("wifi_manager_stop does not crash when called before start", "[wifi_manager][logic]") {
    wifi_manager_stop();
}
