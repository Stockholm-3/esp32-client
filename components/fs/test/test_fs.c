#include "fs.h"
#include "unity.h"

// ── fs_build_path — pure string logic ──────────────────────────────────────

TEST_CASE("fs_build_path produces standard path", "[fs][logic]") {
    char out[64];
    fs_build_path(out, sizeof(out), "/storage", "cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);
}

TEST_CASE("fs_build_path strips trailing slash from mount point", "[fs][logic]") {
    char out[64];
    fs_build_path(out, sizeof(out), "/storage/", "cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);
}

TEST_CASE("fs_build_path strips leading slash from relative path", "[fs][logic]") {
    char out[64];
    fs_build_path(out, sizeof(out), "/storage", "/cache.json");
    TEST_ASSERT_EQUAL_STRING("/storage/cache.json", out);
}

TEST_CASE("fs_build_path handles nested relative path", "[fs][logic]") {
    char out[64];
    fs_build_path(out, sizeof(out), "/storage", "sub/dir/file.json");
    TEST_ASSERT_EQUAL_STRING("/storage/sub/dir/file.json", out);
}

TEST_CASE("fs_build_path null out buffer does not crash", "[fs][logic]") {
    // Must not crash — function should guard against NULL out
    fs_build_path(NULL, 64, "/storage", "cache.json");
}

TEST_CASE("fs_build_path null mount point does not corrupt output", "[fs][logic]") {
    char out[64];
    memset(out, 0xAB, sizeof(out));
    fs_build_path(out, sizeof(out), NULL, "cache.json");
    // Output must not be written to with garbage
    TEST_ASSERT_NOT_EQUAL(0xABABABAB, *(uint32_t*)out);
}

TEST_CASE("fs_build_path null relative path does not corrupt output", "[fs][logic]") {
    char out[64];
    memset(out, 0xAB, sizeof(out));
    fs_build_path(out, sizeof(out), "/storage", NULL);
    TEST_ASSERT_NOT_EQUAL(0xABABABAB, *(uint32_t*)out);
}

// ── fs_mount_littlefs — null argument validation ────────────────────────────

TEST_CASE("fs_mount_littlefs rejects null partition label", "[fs][hardware]") {
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_ARG, fs_mount_littlefs(NULL, "/test", false));
}

TEST_CASE("fs_mount_littlefs rejects null mount point", "[fs][hardware]") {
    TEST_ASSERT_EQUAL_HEX(ESP_ERR_INVALID_ARG, fs_mount_littlefs("storage", NULL, false));
}
