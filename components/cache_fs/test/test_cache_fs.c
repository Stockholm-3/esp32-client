#include "cache.h"
#include "cache_fs.h"
#include "unity.h"

// ── CACHE_IO_ESP32 — verify all function pointers are wired ──────────────────

TEST_CASE("CACHE_IO_ESP32 exists pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.exists);
}

TEST_CASE("CACHE_IO_ESP32 write pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.write);
}

TEST_CASE("CACHE_IO_ESP32 read pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.read);
}

TEST_CASE("CACHE_IO_ESP32 remove pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.remove);
}

TEST_CASE("CACHE_IO_ESP32 get_size pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.get_size);
}

TEST_CASE("CACHE_IO_ESP32 list_dir pointer is non-null", "[cache_fs][logic]") {
    TEST_ASSERT_NOT_NULL(CACHE_IO_ESP32.list_dir);
}

// ── cache_fs_config — struct population ────────────────────────────────────────
// Note: mkdir inside cache_fs_config may fail if the FS is not mounted;
// the struct is still populated regardless of mkdir's return value.

TEST_CASE("cache_fs_config stores root_path correctly", "[cache_fs][logic]") {
    const char* path = "/storage/cache";
    CacheConfig cfg  = cache_fs_config(path, 3600);
    TEST_ASSERT_EQUAL_PTR(path, cfg.root_path);
}

TEST_CASE("cache_fs_config stores default_ttl_sec correctly", "[cache_fs][logic]") {
    CacheConfig cfg = cache_fs_config("/storage/cache", 3600);
    TEST_ASSERT_EQUAL(3600, cfg.default_ttl_sec);
}

TEST_CASE("cache_fs_config stores zero TTL (infinite) correctly", "[cache_fs][logic]") {
    CacheConfig cfg = cache_fs_config("/storage/cache", CACHE_TTL_INFINITE);
    TEST_ASSERT_EQUAL(CACHE_TTL_INFINITE, cfg.default_ttl_sec);
}

TEST_CASE("cache_fs_config wires the ESP32 IO backend", "[cache_fs][logic]") {
    CacheConfig cfg = cache_fs_config("/storage/cache", 0);
    TEST_ASSERT_EQUAL_PTR(&CACHE_IO_ESP32, cfg.io);
}
