/**
 * @file cache_fs.c
 * @brief ESP32 cache_io_t adapter — implementation
 *
 * Each wrapper converts between the conventions used by fs.h (esp_err_t,
 * size_t* out-param for bytes read) and the conventions expected by cache.h
 * (0 = success / negative = error; read returns byte count as int).
 */

#include "cache_fs.h"

#include "cache.h"
#include "esp_log.h"
#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* -------------------------------------------------------------------------
 * Adapter functions
 * ---------------------------------------------------------------------- */

static bool adapter_exists(const char* path) { return fs_exists(path); }

static int adapter_write(const char* path, const void* data, size_t len) {
    return (fs_write(path, data, len) == ESP_OK) ? 0 : -1;
}

static int adapter_read(const char* path, void* buf, size_t len) {
    size_t bytes_read = 0;
    esp_err_t err     = fs_read(path, buf, len, &bytes_read);
    if (err != ESP_OK) {
        return -1;
    }
    return (int)bytes_read;
}

static int adapter_remove(const char* path) {
    esp_err_t err = fs_remove(path);
    return (err == ESP_OK) ? 0 : -1;
}

static long adapter_get_size(const char* path) {
    return fs_get_size(path); /* already returns -1 on error */
}

/* ---- list_dir adapter -------------------------------------------------- *
 *
 * fs.h only provides fs_list(), which logs to ESP_LOGI and has no callback.
 * We need an enumerating version, so we implement it here directly using
 * the POSIX dirent API that is available through ESP-IDF's VFS layer.
 * This keeps fs.h unchanged and the adapter self-contained.
 */
static int adapter_list_dir(const char* dir_path, void (*cb)(const char* filename, void* user_ctx),
                            void* user_ctx) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return -1;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip navigation entries */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        cb(entry->d_name, user_ctx);
    }

    closedir(dir);
    return 0;
}

/* -------------------------------------------------------------------------
 * Public constant
 * ---------------------------------------------------------------------- */

const CacheIo CACHE_IO_ESP32 = {
    .exists   = adapter_exists,
    .write    = adapter_write,
    .read     = adapter_read,
    .remove   = adapter_remove,
    .get_size = adapter_get_size,
    .list_dir = adapter_list_dir,
};

/* -------------------------------------------------------------------------
 * Config builder
 * ---------------------------------------------------------------------- */

/**
 * @brief Builds a @ref CacheConfig backed by LittleFS and ensures the cache
 *        root directory exists.
 *
 * @c mkdir is called unconditionally; EEXIST is silently ignored so the call
 * is idempotent across reboots.  Any other error (e.g. the mount point does
 * not exist yet) is a hard fault — the caller must mount the filesystem
 * before calling this function.
 *
 * @param root_path       Absolute VFS path for the cache directory
 *                        (e.g. "/storage/cache").
 * @param default_ttl_sec Default TTL in seconds applied to entries that do
 *                        not specify one explicitly.
 * @return Populated @ref CacheConfig ready to pass to @ref cache_init.
 */
CacheConfig cache_fs_config(const char* root_path, uint32_t default_ttl_sec) {
    /* Create the directory if it does not already exist.
     * EEXIST is not an error — every boot after the first will hit it. */
    if (mkdir(root_path, 0775) != 0 && errno != EEXIST) {
        ESP_LOGE("cache_fs", "mkdir('%s') failed: %s", root_path, strerror(errno));
        /* Non-fatal: cache_put will fail loudly if the directory is truly
         * absent; we do not abort the whole boot sequence here. */
    }

    CacheConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.root_path       = root_path;
    cfg.io              = &CACHE_IO_ESP32;
    cfg.default_ttl_sec = default_ttl_sec;
    /* cfg.alloc left zeroed → stdlib malloc/free */
    return cfg;
}
