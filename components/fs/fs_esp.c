#include "esp_littlefs.h"
#include "esp_log.h"
#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char* g_tag = "fs";

static bool g_mounted = false;

esp_err_t fs_init(bool format_if_failed) {
    if (g_mounted) {
        ESP_LOGW(g_tag, "Already mounted");
        return ESP_OK;
    }

    const esp_vfs_littlefs_conf_t CONF = {
        .base_path              = FS_MOUNT_POINT,
        .partition_label        = FS_PARTITION_LABEL,
        .format_if_mount_failed = (uint8_t)format_if_failed,
        .dont_mount             = 0U,
    };

    esp_err_t err = esp_vfs_littlefs_register(&CONF);
    if (err != ESP_OK) {
        ESP_LOGE(g_tag, "Mount failed (%s) — partition label: '%s'", esp_err_to_name(err),
                 FS_PARTITION_LABEL);
        return err;
    }

    g_mounted = true;
    ESP_LOGI(g_tag, "Mounted '%s' on %s", FS_PARTITION_LABEL, FS_MOUNT_POINT);
    fs_log_info();
    return ESP_OK;
}

esp_err_t fs_deinit(void) {
    if (!g_mounted) {
        return ESP_OK;
    }

    esp_err_t err = esp_vfs_littlefs_unregister(FS_PARTITION_LABEL);
    if (err == ESP_OK) {
        g_mounted = false;
        ESP_LOGI(g_tag, "Unmounted");
    } else {
        ESP_LOGE(g_tag, "Unmount failed: %s", esp_err_to_name(err));
    }
    return err;
}

bool fs_is_mounted(void) { return g_mounted; }

esp_err_t fs_info(size_t* total_bytes, size_t* used_bytes) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_littlefs_info(FS_PARTITION_LABEL, total_bytes, used_bytes);
}

void fs_log_info(void) {
    size_t total = 0;
    size_t used  = 0;

    if (fs_info(&total, &used) == ESP_OK) {
        ESP_LOGI(g_tag, "%zu KB total  %zu KB used  %zu KB free", total / 1024, used / 1024,
                 (total - used) / 1024);
    }
}

esp_err_t fs_list(const char* dir_path) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    DIR* dir = opendir(dir_path);
    if (!dir) {
        ESP_LOGE(g_tag, "opendir('%s'): %s", dir_path, strerror(errno));
        return ESP_FAIL;
    }

    ESP_LOGI(g_tag, "Listing directory: %s", dir_path);

    const struct dirent* entry;
    char full_path[512];

    while ((entry = readdir(dir)) != NULL) {
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

        if (written >= sizeof(full_path)) {
            ESP_LOGW(g_tag, "Path too long, skipping: %s", entry->d_name);
            continue;
        }

        ESP_LOGI(g_tag, "  %-40s  %ld B", entry->d_name, fs_size(full_path));
    }

    closedir(dir);
    return ESP_OK;
}

bool fs_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0;
}

esp_err_t fs_remove(const char* path) {
    if (!fs_exists(path)) {
        return ESP_ERR_NOT_FOUND;
    }
    if (remove(path) != 0) {
        ESP_LOGE(g_tag, "remove('%s'): %s", path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

long fs_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    return (long)st.st_size;
}

esp_err_t fs_write(const char* path, const void* data, size_t len) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE* f = fopen(path, "wb");
    if (!f) {
        ESP_LOGE(g_tag, "fopen('%s'): %s", path, strerror(errno));
        return ESP_FAIL;
    }

    const size_t WRITTEN = fwrite(data, 1, len, f);
    fclose(f);

    if (WRITTEN != len) {
        ESP_LOGE(g_tag, "fwrite: wrote %zu of %zu bytes", WRITTEN, len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fs_read(const char* path, void* buf, size_t buf_len, size_t* bytes_read) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    FILE* f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(g_tag, "fopen('%s'): %s", path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }

    const size_t N = fread(buf, 1, buf_len, f);
    fclose(f);

    if (bytes_read) {
        *bytes_read = N;
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
    const long SIZE = fs_size(path);
    if (SIZE < 0) {
        return NULL;
    }

    char* buf = malloc((size_t)SIZE + 1);
    if (!buf) {
        ESP_LOGE(g_tag, "malloc(%ld) failed", SIZE + 1);
        return NULL;
    }

    size_t n = 0;
    if (fs_read(path, buf, (size_t)SIZE, &n) != ESP_OK) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    return buf;
}

void fs_path(char* out, size_t out_len, const char* relative) {
    snprintf(out, out_len, "%s/%s", FS_MOUNT_POINT, relative);
}
