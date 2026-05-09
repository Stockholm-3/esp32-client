/**
 * @file fs.h
 * @brief Generic File System Manager for ESP32 (LittleFS, FATFS/SD-Card)
 *
 * This module provides a unified wrapper around the ESP-IDF Virtual File System (VFS).
 * By using absolute paths (e.g., "/storage/file.txt" vs "/sdcard/file.txt"), the
 * same functions can operate on different physical storage media.
 */

#ifndef FS_H
#define FS_H

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize and mount a LittleFS partition.
 *
 * @param partition_label Label of the partition in the CSV table (e.g., "storage").
 * @param mount_point VFS path where the partition will be accessed (e.g., "/storage").
 * @param format_if_failed If true, formats the partition if it's corrupted or unformatted.
 * @return ESP_OK on success, or an error code from the underlying VFS driver.
 */
esp_err_t fs_mount_littlefs(const char* partition_label, const char* mount_point,
                            bool format_if_failed);

/**
 * @brief Initialize and mount an SD Card using FATFS via SDMMC.
 *
 * @note This uses the default SDMMC host and slot configuration.
 * @param mount_point VFS path where the SD card will be accessed (e.g., "/sdcard").
 * @return ESP_OK on success, or an error code from the FATFS/SDMMC driver.
 */
esp_err_t fs_mount_sdcard(const char* mount_point);

/**
 * @brief Unmount a file system.
 *
 * @param mount_point The VFS path to unmount.
 * @return ESP_OK on success.
 */
esp_err_t fs_unmount(const char* mount_point);

/**
 * @brief Check if a file or directory exists.
 *
 * @param path Full absolute path to the object.
 * @return true if it exists, false otherwise.
 */
bool fs_exists(const char* path);

/**
 * @brief Get the size of a file in bytes.
 *
 * @param path Full absolute path to the file.
 * @return File size in bytes, or -1 if the file does not exist.
 */
long fs_get_size(const char* path);

/**
 * @brief Delete a file from the file system.
 *
 * @param path Full absolute path to the file.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if missing.
 */
esp_err_t fs_remove(const char* path);

/**
 * @brief Log a list of all files in a directory to ESP_LOGI.
 *
 * @param dir_path Full absolute path to the directory.
 * @return ESP_OK on success.
 */
esp_err_t fs_list(const char* dir_path);

/**
 * @brief Write raw binary data to a file.
 *
 * Overwrites any existing content.
 *
 * @param path Full absolute path to the file.
 * @param data Pointer to the data buffer.
 * @param len Number of bytes to write.
 * @return ESP_OK on success.
 */
esp_err_t fs_write(const char* path, const void* data, size_t len);

/**
 * @brief Read raw binary data from a file.
 *
 * @param path Full absolute path to the file.
 * @param buf Pointer to the destination buffer.
 * @param buf_len Size of the destination buffer.
 * @param[out] bytes_read Optional pointer to store the actual number of bytes read.
 * @return ESP_OK on success.
 */
esp_err_t fs_read(const char* path, void* buf, size_t buf_len, size_t* bytes_read);

/**
 * @brief Write a null-terminated string to a file.
 *
 * @param path Full absolute path to the file.
 * @param str The string to write.
 * @return ESP_OK on success.
 */
esp_err_t fs_write_str(const char* path, const char* str);

/**
 * @brief Read a file into a newly allocated string.
 *
 * Memory is allocated on the heap. The string is automatically null-terminated.
 *
 * @warning The caller is responsible for calling free() on the returned pointer.
 * @param path Full absolute path to the file.
 * @return Pointer to the allocated string, or NULL on failure.
 */
char* fs_read_str(const char* path);

/**
 * @brief Safe helper to construct an absolute path from a mount point and relative path.
 *
 * @param out Buffer to store the resulting string.
 * @param out_len Size of the output buffer.
 * @param mount_point The base mount point (e.g., "/storage").
 * @param relative The relative path (e.g., "config.json").
 */
void fs_build_path(char* out, size_t out_len, const char* mount_point, const char* relative);

#ifdef __cplusplus
}
#endif

#endif // FS_H
