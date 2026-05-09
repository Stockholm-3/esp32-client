/**
 * @file cache_fs.h
 * @brief ESP32 cache_io_t adapter — bridges cache.h and fs.h
 *
 * Provides a pre-filled cache_io_t and a ready-made cache_config_t builder
 * so that wiring up the cache module on ESP32 is a one-liner:
 *
 *   #include "cache_fs_esp32.h"
 *
 *   // In app_main, after fs_mount_littlefs():
 *   cache_config_t cfg = cache_fs_esp32_config("/storage/cache", 3600);
 *   cache_init(&cfg);
 *
 * The adapter translates between the cache module's return-value conventions
 * (0 = success, negative = error; read returns byte count) and the esp_err_t
 * conventions used by fs.h.
 */

#ifndef CACHE_FS_ESP32_H
#define CACHE_FS_ESP32_H

#include "cache.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A cache_io_t whose function pointers are wired to fs.h.
 *
 * This is a statically allocated constant — safe to take its address and
 * embed in a cache_config_t.
 */
extern const cache_io_t CACHE_IO_ESP32;

/**
 * @brief Build a ready-to-use cache_config_t backed by the ESP32 fs layer.
 *
 * @param root_path       Absolute VFS path for the cache directory
 *                        (e.g. "/storage/cache").  The directory does not
 *                        need to exist beforehand; cache entries are stored
 *                        as flat files directly inside it.
 * @param default_ttl_sec Default TTL in seconds.  Pass CACHE_TTL_INFINITE
 *                        to disable expiry by default.
 * @return A fully populated cache_config_t ready to pass to cache_init()
 *         or cache_create().
 */
cache_config_t cache_fs_config(const char* root_path, uint32_t default_ttl_sec);

#ifdef __cplusplus
}
#endif

#endif // CACHE_FS_H
