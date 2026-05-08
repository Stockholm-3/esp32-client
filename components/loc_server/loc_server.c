#include "loc_server.h"

#include "cJSON.h"
#include "display.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "settings_manager.h"
#include "ui_binder.h"
#include "wifi_manager.h"

#include <string.h>

static const char* g_tag       = "loc_server";
static httpd_handle_t g_server = NULL;
static volatile int g_ws_fd    = -1;

extern const uint8_t INDEX_HTML_START[] asm("_binary_index_html_start");
extern const uint8_t INDEX_HTML_END[] asm("_binary_index_html_end");
extern const uint8_t STYLE_CSS_START[] asm("_binary_style_css_start");
extern const uint8_t STYLE_CSS_END[] asm("_binary_style_css_end");
extern const uint8_t APP_JS_START[] asm("_binary_app_js_start");
extern const uint8_t APP_JS_END[] asm("_binary_app_js_end");

typedef struct {
    const char* uri;
    const uint8_t* start;
    const uint8_t* end;
    const char* content_type;
} StaticFile;

static const StaticFile G_FILES[] = {
    {"/", INDEX_HTML_START, INDEX_HTML_END, "text/html"},
    {"/style.css", STYLE_CSS_START, STYLE_CSS_END, "text/css"},
    {"/app.js", APP_JS_START, APP_JS_END, "application/javascript"},
};

static esp_err_t handle_static(httpd_req_t* req) {
    for (int i = 0; i < (int)(sizeof(G_FILES) / sizeof(G_FILES[0])); i++) {
        if (strcmp(req->uri, G_FILES[i].uri) == 0) {
            size_t len = (size_t)(G_FILES[i].end - G_FILES[i].start);
            httpd_resp_set_type(req, G_FILES[i].content_type);
            httpd_resp_set_hdr(req, "Cache-Control", "no-store");
            return httpd_resp_send(req, (const char*)G_FILES[i].start, (ssize_t)len);
        }
    }
    return httpd_resp_send_404(req);
}

void loc_server_push_settings(void) {
    if (g_server == NULL || g_ws_fd < 0) {
        return;
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"type\":\"settings\","
             "\"ssid\":\"%s\","
             "\"location\":\"%s\","
             "\"price_zone\":%d,"
             "\"timeout\":%d}",
             settings_manager_get_ssid(), settings_manager_get_location(),
             settings_manager_get_price_zone(), settings_manager_get_timeout());

    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)buf,
        .len     = strlen(buf),
    };

    esp_err_t err = httpd_ws_send_frame_async(g_server, g_ws_fd, &frame);
    if (err != ESP_OK) {
        ESP_LOGW(g_tag, "WS send failed: %s", esp_err_to_name(err));
        g_ws_fd = -1;
    }
}

static void on_wifi_scan_done(const WifiManagerApInfo* aps, uint16_t count) {
    if (g_server == NULL || g_ws_fd < 0) {
        return;
    }

    // building JSON: {"type":"wifi_scan_result","aps":[{"ssid":"...","rssi":-45,"open":false},...]}
    char buf[1024];
    int pos = snprintf(buf, sizeof(buf), "{\"type\":\"wifi_scan_result\",\"aps\":[");
    for (int i = 0; i < count && pos < (int)sizeof(buf) - 64; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s{\"ssid\":\"%s\",\"rssi\":%d,\"open\":%s}",
                        i > 0 ? "," : "", aps[i].ssid, aps[i].rssi,
                        aps[i].authmode == 0 ? "true" : "false");
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)buf,
        .len     = (size_t)pos,
    };
    httpd_ws_send_frame_async(g_server, g_ws_fd, &frame);
}

static void handle_ws_message(const char* json_str) {
    cJSON* root = cJSON_Parse(json_str);
    if (!root) {
        return;
    }

    cJSON* type = cJSON_GetObjectItemCaseSensitive(root, "type");
    if (!cJSON_IsString(type) || strcmp(type->valuestring, "set_settings") != 0) {
        cJSON_Delete(root);
        return;
    }
    if (strcmp(type->valuestring, "scan_wifi") == 0) {
        wifi_manager_scan(on_wifi_scan_done);
        cJSON_Delete(root);
        return;
    }

    cJSON* loc = cJSON_GetObjectItemCaseSensitive(root, "location");
    if (cJSON_IsString(loc) && loc->valuestring[0]) {
        settings_manager_save_location(loc->valuestring);
        if (display_lvgl_lock(100)) {
            ui_binder_set_location(loc->valuestring);
            display_lvgl_unlock();
        }
    }

    cJSON* zone = cJSON_GetObjectItemCaseSensitive(root, "price_zone");
    if (cJSON_IsNumber(zone)) {
        int idx = (int)zone->valuedouble;
        settings_manager_save_price_zone(idx);
        if (display_lvgl_lock(100)) {
            ui_binder_set_price_zone(idx);
            display_lvgl_unlock();
        }
    }

    cJSON* tmo = cJSON_GetObjectItemCaseSensitive(root, "timeout");
    if (cJSON_IsNumber(tmo)) {
        int idx = (int)tmo->valuedouble;
        settings_manager_save_timeout(idx);
        if (display_lvgl_lock(100)) {
            ui_binder_set_timeout(idx);
            display_lvgl_unlock();
        }
    }

    cJSON* ssid = cJSON_GetObjectItemCaseSensitive(root, "ssid");
    cJSON* pass = cJSON_GetObjectItemCaseSensitive(root, "password");
    if (cJSON_IsString(ssid) && ssid->valuestring[0] && cJSON_IsString(pass) &&
        pass->valuestring[0]) {
        settings_manager_save_wifi(ssid->valuestring, pass->valuestring);
        wifi_manager_change_network(ssid->valuestring, pass->valuestring);
    }

    cJSON_Delete(root);
}

static esp_err_t handle_ws(httpd_req_t* req) {
    if (req->method == HTTP_GET) {
        g_ws_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(g_tag, "Browser connected, fd=%d", g_ws_fd);
        loc_server_push_settings();
        return ESP_OK;
    }

    httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT};
    esp_err_t ret          = httpd_ws_recv_frame(req, &frame, 0);
    if (ret != ESP_OK || frame.len == 0) {
        return ret;
    }

    uint8_t* buf = calloc(1, frame.len + 1);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }
    frame.payload = buf;
    ret           = httpd_ws_recv_frame(req, &frame, frame.len);
    if (ret == ESP_OK) {
        handle_ws_message((char*)buf);
    }
    free(buf);
    return ret;
}

void loc_server_notify_wifi_state(WifiManagerState state) {
    if (g_server == NULL || g_ws_fd < 0) {
        return;
    }

    const char* state_str = state == WIFI_MANAGER_STATE_CONNECTED    ? "connected"
                            : state == WIFI_MANAGER_STATE_CONNECTING ? "connecting"
                            : state == WIFI_MANAGER_STATE_FAILED     ? "failed"
                                                                     : "disconnected";
    char buf[64];
    snprintf(buf, sizeof(buf), "{\"type\":\"wifi_status\",\"state\":\"%s\"}", state_str);

    httpd_ws_frame_t frame = {
        .type    = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)buf,
        .len     = strlen(buf),
    };
    httpd_ws_send_frame_async(g_server, g_ws_fd, &frame);
}

esp_err_t loc_server_start(void) {
    if (g_server) {
        return ESP_OK;
    }

    httpd_config_t config   = HTTPD_DEFAULT_CONFIG();
    config.server_port      = 80;
    config.max_open_sockets = 4;
    config.stack_size       = 8192;

    ESP_RETURN_ON_ERROR(httpd_start(&g_server, &config), g_tag, "httpd_start failed");

    static const httpd_uri_t URI_ROOT = {
        .uri     = "/",
        .method  = HTTP_GET,
        .handler = handle_static,
    };
    static const httpd_uri_t URI_CSS = {
        .uri     = "/style.css",
        .method  = HTTP_GET,
        .handler = handle_static,
    };
    static const httpd_uri_t URI_JS = {
        .uri     = "/app.js",
        .method  = HTTP_GET,
        .handler = handle_static,
    };
    static const httpd_uri_t URI_WS = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = handle_ws,
        .is_websocket = true,
    };
    httpd_register_uri_handler(g_server, &URI_ROOT);
    httpd_register_uri_handler(g_server, &URI_CSS);
    httpd_register_uri_handler(g_server, &URI_JS);
    httpd_register_uri_handler(g_server, &URI_WS);

    ESP_LOGI(g_tag, "Server started on port 80");
    return ESP_OK;
}

void loc_server_stop(void) {
    if (g_server) {
        httpd_stop(g_server);
        g_server = NULL;
        g_ws_fd  = -1;
    }
}

static void on_settings_changed_from_lvgl(const char* unused) {
    (void)unused;
    loc_server_push_settings();
}

static void on_settings_changed_from_lvgl_int(int unused) {
    (void)unused;
    loc_server_push_settings();
}

void loc_server_init(void) {
    ui_binder_on_location_changed2(on_settings_changed_from_lvgl);
    ui_binder_on_price_changed2(on_settings_changed_from_lvgl_int);
    ui_binder_on_timeout_changed2(on_settings_changed_from_lvgl_int);
}
