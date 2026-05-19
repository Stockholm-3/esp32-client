#include "driver/sdmmc_host.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "fs.h"
#include "sdmmc_cmd.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* g_tag = "fs_mgr";

esp_err_t fs_mount_littlefs(const char* partition_label, const char* mount_point,
                            bool format_if_failed) {
    if (!partition_label || !mount_point) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_vfs_littlefs_conf_t conf = {.base_path              = mount_point,
                                    .partition_label        = partition_label,
                                    .format_if_mount_failed = (uint8_t)format_if_failed,
                                    .dont_mount             = 0U};

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(g_tag, "Failed to mount LittleFS %s at %s (%s)", partition_label, mount_point,
                 esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(g_tag, "Mounted LittleFS [%s] -> %s", partition_label, mount_point);
    return ESP_OK;
}

esp_err_t fs_mount_sdcard(const char* mount_point) {
    if (!mount_point) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024};

    sdmmc_host_t host               = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    sdmmc_card_t* card;

    esp_err_t err = esp_vfs_fat_sdmmc_mount(mount_point, &host, &slot_config, &mount_config, &card);
    if (err != ESP_OK) {
        ESP_LOGE(g_tag, "Failed to mount SD Card at %s (%s)", mount_point, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(g_tag, "Mounted SD Card -> %s", mount_point);
    return ESP_OK;
}

esp_err_t fs_unmount(const char* mount_point) {
    // Note: unregistering is handled by specific drivers.
    // We attempt LittleFS first, then FAT.
    esp_err_t err = esp_vfs_littlefs_unregister(mount_point);
    if (err != ESP_OK) {
        err = esp_vfs_fat_sdcard_unmount(mount_point,
                                         NULL); // NULL because card pointer isn't stored locally
    }

    if (err == ESP_OK) {
        ESP_LOGI(g_tag, "Unmounted %s", mount_point);
    }
    return err;
}

bool fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

long fs_get_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

esp_err_t fs_list(const char* dir_path) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGE(g_tag, "opendir('%s') failed: %s", dir_path, strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGI(g_tag, "Listing: %s", dir_path);
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        char full_path[256];
        fs_build_path(full_path, sizeof(full_path), dir_path, entry->d_name);
        ESP_LOGI(g_tag, "  %-32s | %ld bytes", entry->d_name, fs_get_size(full_path));
    }

    closedir(dir);
    return ESP_OK;
}

esp_err_t fs_write(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(g_tag, "fopen('%s') for write failed: %s", path, strerror(errno));
        return ESP_FAIL;
    }

    size_t written = fwrite(data, 1, len, f);
    fclose(f);

    return (written == len) ? ESP_OK : ESP_FAIL;
}

esp_err_t fs_read(const char* path, void* buf, size_t buf_len, size_t* bytes_read) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    size_t n = fread(buf, 1, buf_len, f);
    fclose(f);

    if (bytes_read) {
        *bytes_read = n;
    }
    return ESP_OK;
}

esp_err_t fs_write_str(const char* path, const char* str) {
    if (!str) {
        return ESP_ERR_INVALID_ARG;
    }
    return fs_write(path, str, strlen(str));
}

char* fs_read_str(const char* path) {
    long size = fs_get_size(path);
    if (size < 0) {
        return NULL;
    }

    char* buf = malloc(size + 1);
    if (!buf) {
        return NULL;
    }

    size_t n = 0;
    if (fs_read(path, buf, size, &n) != ESP_OK) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    return buf;
}

esp_err_t fs_remove(const char* path) {
    if (remove(path) != 0) {
        return (errno == ENOENT) ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    }
    return ESP_OK;
}

void fs_build_path(char* out, size_t out_len, const char* mount_point, const char* relative) {
    if (!out || !mount_point || !relative) {
        return;
    }

    // Ensure no double slashes between mount_point and relative path
    size_t m_len    = strlen(mount_point);
    const char* sep = "/";

    if ((m_len > 0 && mount_point[m_len - 1] == '/') || relative[0] == '/') {
        sep = "";
    }

    snprintf(out, out_len, "%s%s%s", mount_point, sep, relative);
}
