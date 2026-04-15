#define _XOPEN_SOURCE 500

#include "fs.h"

#include <dirent.h>
#include <errno.h>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static bool g_mounted = false;

static void logf_msg(const char* level, const char* fmt, ...) {
    printf("[%s] fs: ", level);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

#define LOGI(...) logf_msg("I", __VA_ARGS__)
#define LOGW(...) logf_msg("W", __VA_ARGS__)
#define LOGE(...) logf_msg("E", __VA_ARGS__)

static esp_err_t make_dir(const char* path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        LOGE("mkdir('%s'): %s", path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fs_init(bool format_if_failed) {
    (void)format_if_failed;

    if (g_mounted) {
        LOGW("Already mounted");
        return ESP_OK;
    }

    if (make_dir(FS_MOUNT_POINT) != ESP_OK) {
        return ESP_FAIL;
    }

    g_mounted = true;
    LOGI("Mounted '%s' on %s", FS_PARTITION_LABEL, FS_MOUNT_POINT);
    fs_log_info();
    return ESP_OK;
}

esp_err_t fs_deinit(void) {
    if (!g_mounted) {
        return ESP_OK;
    }
    g_mounted = false;
    LOGI("Unmounted");
    return ESP_OK;
}

bool fs_is_mounted(void) { return g_mounted; }

esp_err_t fs_info(size_t* total_bytes, size_t* used_bytes) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    struct statvfs vfs;
    if (statvfs(FS_MOUNT_POINT, &vfs) != 0) {
        LOGE("statvfs: %s", strerror(errno));
        return ESP_FAIL;
    }

    if (total_bytes) {
        *total_bytes = (size_t)(vfs.f_blocks * vfs.f_frsize);
    }
    if (used_bytes) {
        *used_bytes = (size_t)((vfs.f_blocks - vfs.f_bfree) * vfs.f_frsize);
    }
    return ESP_OK;
}

void fs_log_info(void) {
    size_t total = 0;
    size_t used  = 0;

    if (fs_info(&total, &used) == ESP_OK) {
        LOGI("%zu KB total  %zu KB used  %zu KB free", total / 1024, used / 1024,
             (total - used) / 1024);
    }
}

static int list_entry(const char* path, const struct stat* st, int type_flag,
                      struct FTW* ftw_info) {
    (void)ftw_info;

    if (type_flag == FTW_F) {
        printf("  %-48s  %ld B\n", path, (long)st->st_size);
    } else if (type_flag == FTW_D && strcmp(path, FS_MOUNT_POINT) != 0) {
        printf("  %s/\n", path);
    }
    return 0;
}

esp_err_t fs_list(const char* dir_path) {
    if (!g_mounted) {
        return ESP_ERR_INVALID_STATE;
    }

    printf("[I] fs: %s\n", dir_path);

    if (nftw(dir_path, list_entry, 8, FTW_PHYS) != 0) {
        LOGE("nftw('%s'): %s", dir_path, strerror(errno));
        return ESP_FAIL;
    }
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
        LOGE("remove('%s'): %s", path, strerror(errno));
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
        LOGE("fopen('%s'): %s", path, strerror(errno));
        return ESP_FAIL;
    }

    const size_t written = fwrite(data, 1, len, f);
    fclose(f);

    if (written != len) {
        LOGE("fwrite: wrote %zu of %zu bytes", written, len);
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
        LOGE("fopen('%s'): %s", path, strerror(errno));
        return ESP_ERR_NOT_FOUND;
    }

    const size_t n = fread(buf, 1, buf_len, f);
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
    const long size = fs_size(path);
    if (size < 0) {
        return NULL;
    }

    char* buf = malloc((size_t)size + 1);
    if (!buf) {
        LOGE("malloc(%ld) failed", size + 1);
        return NULL;
    }

    size_t n = 0;
    if (fs_read(path, buf, (size_t)size, &n) != ESP_OK) {
        free(buf);
        return NULL;
    }

    buf[n] = '\0';
    return buf;
}

void fs_path(char* out, size_t out_len, const char* relative) {
    snprintf(out, out_len, "%s/%s", FS_MOUNT_POINT, relative);
}
