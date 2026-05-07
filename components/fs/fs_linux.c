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

static const char* g_tag = "fs_stub";

// Helper for local logging
static void log_msg(const char* level, const char* fmt, ...) {
    printf("[%s] %s: ", level, g_tag);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

#define LOGI(...) log_msg("I", __VA_ARGS__)
#define LOGW(...) log_msg("W", __VA_ARGS__)
#define LOGE(...) log_msg("E", __VA_ARGS__)

static esp_err_t ensure_dir(const char* path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        LOGE("mkdir('%s') failed: %s", path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fs_mount_littlefs(const char* partition_label, const char* mount_point,
                            bool format_if_failed) {
    (void)partition_label;
    (void)format_if_failed;

    LOGI("Stubbing LittleFS mount at %s", mount_point);
    return ensure_dir(mount_point);
}

esp_err_t fs_mount_sdcard(const char* mount_point) {
    LOGI("Stubbing SD Card mount at %s", mount_point);
    return ensure_dir(mount_point);
}

esp_err_t fs_unmount(const char* mount_point) {
    LOGI("Stubbing unmount for %s", mount_point);
    return ESP_OK;
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

static int nftw_callback(const char* path, const struct stat* st, int type_flag,
                         struct FTW* ftw_info) {
    (void)ftw_info;
    if (type_flag == FTW_F) {
        printf("  %-48s  %ld B\n", path, (long)st->st_size);
    } else if (type_flag == FTW_D) {
        printf("  %s/\n", path);
    }
    return 0;
}

esp_err_t fs_list(const char* dir_path) {
    LOGI("Listing: %s", dir_path);
    if (nftw(dir_path, nftw_callback, 8, FTW_PHYS) != 0) {
        LOGE("nftw('%s') failed: %s", dir_path, strerror(errno));
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fs_write(const char* path, const void* data, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) {
        LOGE("fopen('%s') failed: %s", path, strerror(errno));
        return ESP_FAIL;
    }

    const size_t WRITTEN = fwrite(data, 1, len, f);
    fclose(f);

    if (WRITTEN != len) {
        LOGE("fwrite failed: wrote %zu of %zu", WRITTEN, len);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t fs_read(const char* path, void* buf, size_t buf_len, size_t* bytes_read) {
    FILE* f = fopen(path, "rb");
    if (!f) {
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
    const long SIZE = fs_get_size(path);
    if (SIZE < 0) {
        return NULL;
    }

    char* buf = malloc((size_t)SIZE + 1);
    if (!buf) {
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

    size_t m_len    = strlen(mount_point);
    const char* sep = (m_len > 0 && mount_point[m_len - 1] == '/') ? ""
                      : (relative[0] == '/')                       ? ""
                                                                   : "/";

    snprintf(out, out_len, "%s%s%s", mount_point, sep, relative);
}
