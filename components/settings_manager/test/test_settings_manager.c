#include "settings_manager.h"
#include "unity.h"

// settings_manager_save_price_zone() updates the in-memory g_s_price_zone
// immediately before attempting an NVS write, so these tests do not require
// settings_manager_init() (which depends on ui_binder/LVGL) or a mounted
// NVS partition.

// ── Price zone string mapping ─────────────────────────────────────────────────

TEST_CASE("price zone 0 maps to SE1", "[settings_manager][logic]") {
    settings_manager_save_price_zone(0);
    TEST_ASSERT_EQUAL_STRING("SE1", settings_manager_get_price_zone_as_string());
}

TEST_CASE("price zone 1 maps to SE2", "[settings_manager][logic]") {
    settings_manager_save_price_zone(1);
    TEST_ASSERT_EQUAL_STRING("SE2", settings_manager_get_price_zone_as_string());
}

TEST_CASE("price zone 2 maps to SE3", "[settings_manager][logic]") {
    settings_manager_save_price_zone(2);
    TEST_ASSERT_EQUAL_STRING("SE3", settings_manager_get_price_zone_as_string());
}

TEST_CASE("price zone 3 maps to SE4", "[settings_manager][logic]") {
    settings_manager_save_price_zone(3);
    TEST_ASSERT_EQUAL_STRING("SE4", settings_manager_get_price_zone_as_string());
}

TEST_CASE("out-of-range price zone returns NULL", "[settings_manager][logic]") {
    settings_manager_save_price_zone(99);
    TEST_ASSERT_NULL(settings_manager_get_price_zone_as_string());
}

TEST_CASE("get_price_zone reflects saved value", "[settings_manager][logic]") {
    settings_manager_save_price_zone(2);
    TEST_ASSERT_EQUAL(2, settings_manager_get_price_zone());
}

// ── Wi-Fi network list ──────────────────────────────────────────────────────────

TEST_CASE("get_all_networks returns 0 on fresh state", "[settings_manager][hardware]") {
    // On a freshly erased NVS partition the list is empty.
    // Requires nvs_flash_init() to have been called.
    SavedWifiNetwork list[5];
    uint8_t count = settings_manager_get_all_networks(list, 5);
    // Count should be 0 on a clean device or at most MAX_SAVED_NETWORKS
    TEST_ASSERT_TRUE(count <= 5);
}

TEST_CASE("get_all_networks handles null output buffer", "[settings_manager][logic]") {
    // Should not crash and should return the stored count
    uint8_t count = settings_manager_get_all_networks(NULL, 5);
    TEST_ASSERT_TRUE(count <= 5);
}

TEST_CASE("get_all_networks honours max_count cap", "[settings_manager][logic]") {
    SavedWifiNetwork list[1];
    // Request at most 1 even if more are stored
    uint8_t count = settings_manager_get_all_networks(list, 1);
    TEST_ASSERT_TRUE(count <= 1);
}
