/**
 * @file main.c
 * @brief Application entry point for the ESP32 client.
 *
 * @details Initialises peripherals (display, BME280, NVS, network stack),
 *          registers Wi-Fi and sensor callbacks, and runs the main task loop.
 */

#include "bme280_sensor.h"
#include "cache.h"
#include "cache_fs.h"
#include "clock.h"
#include "console_cli.h"
#include "data_fetcher.h"
#include "display.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fs.h"
#include "http_client.h"
#include "loc_server.h"
#include "nvs_flash.h"
#include "screen_timeout.h"
#include "settings_manager.h"
#include "smw.h"
#include "squareline/screens/ui_scr_home.h"
#include "time_manager.h"
#include "ui.h"
#include "ui_binder.h"
#include "wifi_manager.h"
#include "wifi_popup.h"
#include "ws7b_board.h"

#include <stdio.h>
#include <string.h>

#ifndef CONFIG_IDF_TARGET_LINUX
#    include "mdns.h"
#endif

/** @brief Maximum number of tasks in the SMW scheduler queue. */
#define SMW_MAX_TASKS 100

/**
 * @brief Base URL for all API requests (no trailing slash).
 *
 * Switch between environments by changing this single define:
 *   - Production:  "https://just-dev.freeduck.dev"
 *   - Local:       "http://localhost:8080"
 */
#define API_BASE_URL "http://192.168.1.18:10680"

/** @brief Log tag for this module. */
static const char* g_tag = "main";

/** @brief SSID of the currently active Wi-Fi network (32 chars + NUL). */
static char g_current_ssid[33] = "";
/** @brief Most recent BME280 sensor reading. */
static Bme280Reading g_bme_reading = {0};
/** @brief Set to true when a new BME280 sample has arrived and not yet consumed. */
static volatile bool g_bme_updated = false;
/** @brief Tracks whether BME280 was present in the last check. */
static bool g_bme_was_present = false;
/**
 * @brief Set to true after the first successful time sync.
 *
 * Used to open the clock gate exactly once, which unblocks all fetch tasks
 * that are waiting to make TLS connections.
 */
static bool g_time_synced = false;

/** @brief SMW scheduler instance. */
static SmwWorker g_smw_worker;
/** @brief Task array for the SMW scheduler. */
static SmwTask g_smw_tasks[SMW_MAX_TASKS];

/**
 * @brief Returns the current system time in milliseconds.
 * @return Elapsed milliseconds since system start (uint32_t).
 */
uint32_t get_system_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

static void smw_worker_task(void* ctx) {
    (void)ctx;
    ESP_LOGI("SMW_TASK", "State machine worker task started.");
    while (1) {
        smw_process(&g_smw_worker, get_system_ms());
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @brief Probes the I2C bus for BME280 sensor presence.
 *
 * @return true if BME280 is detected at either 0x76 or 0x77, false otherwise.
 */
static bool bme280_probe_i2c(void) {
    i2c_master_bus_handle_t bus = ws7b_board_get_i2c_bus();
    const uint8_t ADDRS[]       = {BME280_I2C_ADDR_PRIMARY, BME280_I2C_ADDR_SECONDARY};

    for (size_t i = 0; i < sizeof(ADDRS) / sizeof(ADDRS[0]); i++) {
        if (i2c_master_probe(bus, ADDRS[i], 100) == ESP_OK) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Callback invoked when a new BME280 sample is ready.
 */
static void on_bme280_sample(const Bme280Reading* reading, void* user_ctx) {
    (void)user_ctx;
    ESP_LOGI(g_tag, "BME280 — temp: %.2f °C  pressure: %.2f hPa  humidity: %.2f %%RH",
             (double)reading->temperature_c, (double)reading->pressure_hpa,
             (double)reading->humidity_pct);
    g_bme_reading = *reading;
    g_bme_updated = true;
}

/**
 * @brief Callback invoked when the user submits Wi-Fi credentials via the popup.
 */
static void on_wifi_connect(const char* ssid, const char* password) {
    strncpy(g_current_ssid, ssid, sizeof(g_current_ssid) - 1);
    g_current_ssid[sizeof(g_current_ssid) - 1] = '\0';
    if (wifi_manager_change_network(ssid, password) != 0) {
        wifi_manager_start(NULL);
        wifi_manager_change_network(ssid, password);
    }
    settings_manager_save_wifi(ssid, password);
}

/**
 * @brief Callback invoked on Wi-Fi manager state changes.
 */
static void on_wifi_state(WifiManagerState state, WifiManagerFailReason reason) {
    ui_binder_update_wifi_status(state);
    data_fetcher_notify_wifi_state(state);

    if (state == WIFI_MANAGER_STATE_CONNECTED) {
        wifi_popup_set_connected_ssid(g_current_ssid);
        wifi_popup_notify_result(WIFI_POPUP_RESULT_CONNECTED);
        ui_binder_update_wifi_name(g_current_ssid);
        time_manager_init(NULL);
    }

    if (state == WIFI_MANAGER_STATE_CONNECTED_WITH_DNS) {
        if (settings_manager_get_local_web_client_enabled()) {
            loc_server_start();
        }

        char ip_str[16] = "---";
#ifndef CONFIG_IDF_TARGET_LINUX
        esp_netif_t* sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(sta, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
            }
        }
#endif
        ui_binder_update_local_ip(ip_str);
    }

    if (state == WIFI_MANAGER_STATE_FAILED) {
        wifi_popup_set_connected_ssid("");
        WifiPopupConnectResult r;
        if (reason == WIFI_MANAGER_FAIL_REASON_AUTH) {
            r = WIFI_POPUP_RESULT_WRONG_PASSWORD;
        } else if (reason == WIFI_MANAGER_FAIL_REASON_NO_AP) {
            r = WIFI_POPUP_RESULT_NO_AP;
        } else {
            r = WIFI_POPUP_RESULT_FAILED;
        }
        wifi_popup_notify_result(r);
    } else if (state == WIFI_MANAGER_STATE_DISCONNECTED) {
        wifi_popup_set_connected_ssid("");
    }

    loc_server_notify_wifi_state(state);
}

static void request_weather_now(void) {
    data_fetcher_request_now("weather_min");
    data_fetcher_request_now("weather_hr");
}

static void on_ap_toggled(bool enabled) {
    wifi_manager_set_ap_enabled(enabled);
    if (enabled) {
        loc_server_start();
    }
}

static void on_data_cached(const FetchDescriptor* desc, void* user_ctx) {
    (void)user_ctx;

    void* data = NULL;
    size_t len = 0;
    if (cache_get_alloc(desc->cache_key, &data, &len) != CACHE_OK) {
        return;
    }

    ESP_LOGI(g_tag, "[%s] Fresh data ready — %zu bytes", desc->id, len);
    ESP_LOGI(g_tag, "[%s] Preview: %.200s%s", desc->id, (const char*)data, len > 200U ? "…" : "");

    if (strcmp(desc->id, "weather_min") == 0) {
        ui_binder_update_weather_min((const char*)data, len);
    } else if (strcmp(desc->id, "weather_hr") == 0) {
        ui_binder_update_weather_hr((const char*)data, len);
    } else if (strcmp(desc->id, "elpris") == 0) {
        ui_binder_update_elpris((const char*)data, len);
    }

    cache_free(data);
}

/* ---------------------------------------------------------------------------
 * Dynamic URL + cache-key builders
 *
 * All four builders read city and price_zone directly from settings_manager
 * on every invocation. This means a settings change takes effect on the very
 * next fetch — force_now or the normal schedule — without any restart.
 *
 * weather_min additionally reads the live clock for past_hours and returns
 * NULL until the clock is synced (year < 2020), deferring the first fetch
 * rather than fetching with a wrong value.
 *
 * The ctx argument is unused; settings are read via their own API.
 * ------------------------------------------------------------------------- */

static char* build_elpris_url(const FetchDescriptor* desc, char* url_buf, size_t url_buf_size,
                              char* key_buf, size_t key_buf_size, void* ctx) {
    (void)desc;
    (void)ctx;
    const char* zone = settings_manager_get_price_zone_as_string();
    snprintf(url_buf, url_buf_size, API_BASE_URL "/v1/elpris?price=%s", zone);
    snprintf(key_buf, key_buf_size, "fetch:elpris:%s", zone);
    return url_buf;
}

static char* build_weather_min_url(const FetchDescriptor* desc, char* url_buf, size_t url_buf_size,
                                   char* key_buf, size_t key_buf_size, void* ctx) {
    (void)desc;
    (void)ctx;

    /* Treat anything before 2020 as "clock not synced yet" — defer. */
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    if (tm.tm_year + 1900 < 2020) {
        return NULL;
    }

    const char* city = settings_manager_get_location();
    snprintf(url_buf, url_buf_size,
             API_BASE_URL "/v1/minutely?city=%s&hours=24&past_hours=%d", city,
             tm.tm_hour + 1);
    snprintf(key_buf, key_buf_size, "fetch:weather_min:%s", city);
    return url_buf;
}

static char* build_weather_hr_url(const FetchDescriptor* desc, char* url_buf, size_t url_buf_size,
                                  char* key_buf, size_t key_buf_size, void* ctx) {
    (void)desc;
    (void)ctx;
    const char* city = settings_manager_get_location();
    snprintf(url_buf, url_buf_size, API_BASE_URL "/v1/hourly?city=%s&hours=168", city);
    snprintf(key_buf, key_buf_size, "fetch:weather_hr:%s", city);
    return url_buf;
}

static char* build_energy_plan_url(const FetchDescriptor* desc, char* url_buf, size_t url_buf_size,
                                   char* key_buf, size_t key_buf_size, void* ctx) {
    (void)desc;
    (void)ctx;
    const char* zone = settings_manager_get_price_zone_as_string();
    const char* city = settings_manager_get_location();
    snprintf(url_buf, url_buf_size, API_BASE_URL "/v1/get_plan?price=%s&city=%s", zone, city);
    snprintf(key_buf, key_buf_size, "fetch:energy_plan:%s:%s", zone, city);
    return url_buf;
}

/**
 * @brief Application main function (FreeRTOS entry point).
 */
void app_main(void) { // NOLINT(readability-function-size,readability-function-cognitive-complexity)
    /* ---- NVS ---- */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ---- Filesystem / cache ---- */
    ESP_ERROR_CHECK(fs_mount_littlefs("storage", "/storage", true));
    CacheConfig cache_cfg = cache_fs_config("/storage/cache", 3600);
    cache_init(&cache_cfg);
    int cleaned = cache_cleanup();
    if (cleaned > 0) {
        ESP_LOGI(g_tag, "Cache: removed %d expired entry/entries", cleaned);
    }

    /* ---- Network stack ---- */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

#ifndef CONFIG_IDF_TARGET_LINUX
    mdns_init();
    mdns_instance_name_set("ESP32 Settings");
#endif

    /* ---- HTTP client (must be up before data_fetcher_init) ---- */
    HttpClientConfig http_cfg = {0};
    ESP_ERROR_CHECK_WITHOUT_ABORT(http_client_init(&http_cfg));

    /* ---- Display ---- */
    lv_display_t* disp = NULL;
    lv_indev_t* touch  = NULL;
    ESP_ERROR_CHECK(display_init(&disp, &touch));
    ESP_LOGI(g_tag, "Display initialized");

    /* ---- BME280 (non-fatal) ---- */
    esp_err_t bme_err =
        bme280_sensor_init_with_task(ws7b_board_get_i2c_bus(), on_bme280_sample, NULL);
    if (bme_err != ESP_OK) {
        ESP_LOGW(g_tag, "BME280 not found, skipping (%s)", esp_err_to_name(bme_err));
    } else {
        g_bme_was_present = true;
        ESP_LOGI(g_tag, "BME280 initialized");
    }

    /* ---- UI ---- */
    if (!display_lvgl_lock(-1)) {
        ESP_LOGE(g_tag, "Failed to acquire LVGL lock");
        return;
    }
    ui_build(disp);
    ScreenTimeoutConfig timeout_cfg = {
        .dim_timeout_seconds           = (5 * 60 * 50U) / 100U,
        .screensaver_timeout_seconds   = (5 * 60 * 75U) / 100U,
        .backlight_off_timeout_seconds = 5 * 60,
    };
    screen_timeout_init(&timeout_cfg);
    display_set_activity_callback(screen_timeout_record_activity);
    setenv("TZ", "CET-1CEST-2,M3.5.0/2,M10.5.0/3", 1);
    tzset();
    ui_binder_init();
    ui_binder_on_weather_refresh(request_weather_now);
    clock_init();
    display_lvgl_unlock();

    /* ---- Settings / location server ---- */
    settings_manager_init();
    loc_server_init();

    /* ---- Wi-Fi ---- */
    wifi_popup_on_connect(on_wifi_connect);
    wifi_manager_register_callback(on_wifi_state);
    ui_binder_on_ap_enabled_changed2(on_ap_toggled);

#ifndef CONFIG_IDF_TARGET_LINUX
    const char* mdns_host = settings_manager_get_mdns_hostname();
    mdns_hostname_set(mdns_host[0] != '\0' ? mdns_host : "esp32-client");

    if (settings_manager_get_ap_enabled()) {
        wifi_manager_set_ap_enabled(true);
        loc_server_start();
    }
#endif

    bool lwc_enabled           = settings_manager_get_local_web_client_enabled();
    WifiManagerConfig wifi_cfg = {0};
    if (lwc_enabled) {
        wifi_cfg.sta_static_ip_enabled = true;
        strncpy(wifi_cfg.sta_ip, settings_manager_get_sta_static_ip(), sizeof(wifi_cfg.sta_ip) - 1);
        strncpy(wifi_cfg.sta_gateway, settings_manager_get_sta_gateway(),
                sizeof(wifi_cfg.sta_gateway) - 1);
        strncpy(wifi_cfg.sta_netmask, settings_manager_get_sta_netmask(),
                sizeof(wifi_cfg.sta_netmask) - 1);
    }
    wifi_manager_start((int)lwc_enabled ? &wifi_cfg : NULL);
    wifi_manager_connect_to_saved_wifi();

    /* ---- SMW scheduler ---- */
    smw_init(&g_smw_worker, g_smw_tasks, SMW_MAX_TASKS);
    BaseType_t task_ret = xTaskCreate(smw_worker_task, "smw_worker_task", 4096, NULL, 5, NULL);
    if (task_ret != pdPASS) {
        ESP_LOGE(g_tag, "Failed to create SMW background task!");
    }

    console_cli_start();

    /* ---- Data fetcher descriptors ----
     *
     * All four descriptors use build_url callbacks that read city and
     * price_zone directly from settings_manager on every invocation.
     * This means a settings change (city, price zone) takes effect on the
     * very next fetch — scheduled or force_now — without any restart.
     *
     * The URL and cache-key buffers live on the stack of app_main (which
     * never returns), so pointers into them remain valid for the lifetime
     * of the fetcher.
     *
     * weather_min also defers (returns NULL) until the clock is synced so
     * past_hours is always correct on the first fetch.
     */
    char s_elpris_url[256];
    char s_elpris_key[128];
    char s_weather_min_url[256];
    char s_weather_min_key[128];
    char s_weather_hr_url[256];
    char s_weather_hr_key[128];
    char s_energy_plan_url[256];
    char s_energy_plan_key[128];

    FetchDescriptor s_fetch_descs[4];

    s_fetch_descs[0] = (FetchDescriptor){
        .id                  = "elpris",
        .cache_ttl_sec       = 24U * 3600U,
        .schedule.type       = FETCH_SCHEDULE_DAILY,
        .schedule.daily_hour = 14,
        .fetch_on_startup    = true,
        .build_url           = build_elpris_url,
        .url_buf             = s_elpris_url,
        .url_buf_size        = sizeof(s_elpris_url),
        .cache_key_buf       = s_elpris_key,
        .cache_key_buf_size  = sizeof(s_elpris_key),
    };
    s_fetch_descs[1] = (FetchDescriptor){
        .id                    = "weather_min",
        .cache_ttl_sec         = 24U * 3600U,
        .schedule.type         = FETCH_SCHEDULE_INTERVAL,
        .schedule.interval_sec = 15U * 60U,
        .fetch_on_startup      = true,
        .build_url             = build_weather_min_url,
        .url_buf               = s_weather_min_url,
        .url_buf_size          = sizeof(s_weather_min_url),
        .cache_key_buf         = s_weather_min_key,
        .cache_key_buf_size    = sizeof(s_weather_min_key),
    };
    s_fetch_descs[2] = (FetchDescriptor){
        .id                    = "weather_hr",
        .cache_ttl_sec         = 24U * 3600U,
        .schedule.type         = FETCH_SCHEDULE_INTERVAL,
        .schedule.interval_sec = 15U * 60U,
        .fetch_on_startup      = true,
        .build_url             = build_weather_hr_url,
        .url_buf               = s_weather_hr_url,
        .url_buf_size          = sizeof(s_weather_hr_url),
        .cache_key_buf         = s_weather_hr_key,
        .cache_key_buf_size    = sizeof(s_weather_hr_key),
    };
    s_fetch_descs[3] = (FetchDescriptor){
        .id                    = "energy_plan",
        .cache_ttl_sec         = 24U * 3600U,
        .schedule.type         = FETCH_SCHEDULE_INTERVAL,
        .schedule.interval_sec = 15U * 60U,
        .fetch_on_startup      = true,
        .build_url             = build_energy_plan_url,
        .url_buf               = s_energy_plan_url,
        .url_buf_size          = sizeof(s_energy_plan_url),
        .cache_key_buf         = s_energy_plan_key,
        .cache_key_buf_size    = sizeof(s_energy_plan_key),
    };

    DataFetcherConfig df_cfg = data_fetcher_default_config();
    df_cfg.descriptors       = s_fetch_descs;
    df_cfg.descriptor_count  = 4;
    df_cfg.on_cached         = on_data_cached;

#ifndef CONFIG_IDF_TARGET_LINUX
    data_fetcher_init(&df_cfg);
#endif

    /* ---- Main loop (1 s tick) ---- */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Clock sync: open the data_fetcher clock gate exactly once.
         * No URL rewriting or manual request_now needed — the weather_min
         * build_url callback reads the live clock on every fetch attempt. */
        struct tm timeinfo;
        if (time_manager_get_time(&timeinfo)) {
            if (!g_time_synced) {
                g_time_synced = true;
                ESP_LOGI(g_tag, "Time synced (local hour=%d)", timeinfo.tm_hour);
                data_fetcher_notify_time_sync();
            }
            if (display_lvgl_lock(0)) {
                ui_binder_update_localtime(&timeinfo);
                ui_tab_elpris_update_now();
                display_lvgl_unlock();
            }
        }

        /* BME280 hot-plug detection */
        bool bme_present = bme280_probe_i2c();
        if ((int)bme_present && !g_bme_was_present) {
            ESP_LOGI(g_tag, "BME280 detected — reinitializing");
            bme280_sensor_deinit();
            if (bme280_sensor_init_with_task(ws7b_board_get_i2c_bus(), on_bme280_sample, NULL) ==
                ESP_OK) {
                g_bme_was_present = true;
            } else {
                ESP_LOGW(g_tag, "BME280 reinit failed");
            }
        } else if (!bme_present && (int)g_bme_was_present) {
            ESP_LOGI(g_tag, "BME280 disconnected");
            bme280_sensor_deinit();
            g_bme_was_present = false;
        }

        /* Dispatch pending BME280 reading to UI */
        if (g_bme_updated) {
            g_bme_updated = false;
            ui_binder_update_bme280(&g_bme_reading);
        }
    }
}
