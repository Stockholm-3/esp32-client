#include "esp_littlefs.h"
#include "esp_log.h"
#include "fs.h"
#include "nvs_flash.h"
#include "unity.h"

#define TEST_MOUNT "/test_storage"
#define TEST_PART "storage" // Matches your production partition label

// Helper to get a full path during tests
static void get_test_path(char* out, size_t out_len, const char* filename) {
    fs_build_path(out, out_len, TEST_MOUNT, filename);
}

// =============================================================================
// TIER 1: PURE LOGIC & BOUNDARY TESTS (No mounting required)
// =============================================================================

TEST_CASE("fs_build_path handles standard formatting", "[fs][logic]") {
    char out[64];
    fs_build_path(out, sizeof(out), "/storage", "cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);
}

TEST_CASE("fs_build_path avoids double slashes", "[fs][logic]") {
    char out[64];
    // Trailing slash on mount
    fs_build_path(out, sizeof(out), "/storage/", "cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);

    // Leading slash on relative path
    fs_build_path(out, sizeof(out), "/storage", "/cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);
}

TEST_CASE("fs_build_path handles null pointers gracefully", "[fs][logic]") {
    char out[64] = "unchanged";
    fs_build_path(NULL, sizeof(out), "/storage", "cache.json");
    fs_build_path(out, sizeof(out), NULL, "cache.json");
    fs_build_path(out, sizeof(out), "/storage", NULL);

    // Ensure the function protected itself from hitting a null pointer dereference
    TEST_ASSERT_EQUAL_STRING("unchanged", out);
}

TEST_CASE("fs_mount_littlefs validation hooks", "[fs][hardware]") {
    // Test parameter enforcement
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_ARG, fs_mount_littlefs(NULL, TEST_MOUNT, false));
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_ARG, fs_mount_littlefs(TEST_PART, NULL, false));
}
