#ifndef FS_H
#define FS_H

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FS_MOUNT_POINT "/storage"
#define FS_PARTITION_LABEL "storage"
#define FS_MAX_PATH_LEN 128

/**
 * Mount the LittleFS partition. Call once at startup before any file
 * operations. Pass format_if_failed = true on first boot so the partition
 * is formatted automatically if blank.
 */
esp_err_t fs_init(bool format_if_failed);

/**
 * Unmount the filesystem. Call before deep sleep or shutdown to flush
 * any pending writes.
 */
esp_err_t fs_deinit(void);

bool fs_is_mounted(void);

/**
 * Fill out with total and used byte counts. Either pointer may be NULL.
 */
esp_err_t fs_info(size_t* total_bytes, size_t* used_bytes);

/**
 * Log storage usage to ESP_LOGI.
 */
void fs_log_info(void);

/**
 * Log every file under dir_path to ESP_LOGI.
 */
esp_err_t fs_list(const char* dir_path);

bool fs_exists(const char* path);

/**
 * Delete a file. Returns ESP_ERR_NOT_FOUND if the file does not exist.
 */
esp_err_t fs_remove(const char* path);

long fs_size(const char* path);

/**
 * Write len bytes from data to path, replacing any existing content.
 */
esp_err_t fs_write(const char* path, const void* data, size_t len);

/**
 * Read up to buf_len bytes from path into buf.
 * If bytes_read is non-NULL it is set to the number of bytes actually read.
 */
esp_err_t fs_read(const char* path, void* buf, size_t buf_len, size_t* bytes_read);

/**
 * Write a null-terminated string to path.
 */
esp_err_t fs_write_str(const char* path, const char* str);

/**
 * Read a file into a heap-allocated, null-terminated string.
 * The caller must free() the returned pointer. Returns NULL on error.
 */
char* fs_read_str(const char* path);

/**
 * Build an absolute path from a relative one.
 *
 *   char path[FS_MAX_PATH_LEN];
 *   fs_path(path, sizeof(path), "config/wifi.json");
 *   // path == "/storage/config/wifi.json"
 */
void fs_path(char* out, size_t out_len, const char* relative);

#ifdef __cplusplus
}
#endif

#endif // FS_H
