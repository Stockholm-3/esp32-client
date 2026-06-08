#include "trigger_manager.h"

#include "bme280_sensor.h"
#include "esp_log.h"
#include "freertos/queue.h"
#include "time_manager.h"
#include "ui_binder.h"

#include <string.h>

/** @brief Events that can be posted to the trigger queue. */
typedef enum {
    EVENT_WIFI_CONNECTED,
    EVENT_WIFI_LOST,
    EVENT_BME280,
} AppEvent;

static QueueHandle_t g_queue;
static char g_ssid[33];
static Bme280Reading g_bme_reading;

void trigger_manager_init() { g_queue = xQueueCreate(10, sizeof(AppEvent)); }

void trigger_manager_on_bme280(const Bme280Reading* reading, void* ctx) {
    (void)ctx;
    g_bme_reading = *reading;
    AppEvent evt  = EVENT_BME280;
    xQueueSend(g_queue, &evt, 0);
}

void trigger_manager_on_wifi_state(WifiManagerState state, WifiManagerFailReason reason) {
    (void)reason;
    AppEvent evt = (state == WIFI_MANAGER_STATE_CONNECTED) ? EVENT_WIFI_CONNECTED : EVENT_WIFI_LOST;
    xQueueSend(g_queue, &evt, 0);
}

void trigger_manager_set_wifi_name(const char* ssid) {
    strncpy(g_ssid, ssid, 32);
    g_ssid[32] = '\0';
}

void trigger_manager_process(void) {
    AppEvent evt;
    struct tm timeinfo;
    if (xQueueReceive(g_queue, &evt, 0)) {
        switch (evt) {
        case EVENT_WIFI_CONNECTED:
            ESP_LOGI("TRIGGER", "WiFi connected — ssid: %s", g_ssid);
            ui_binder_update_wifi_name(g_ssid);
            time_manager_init(NULL);
            break;
        case EVENT_WIFI_LOST:
            ESP_LOGI("TRIGGER", "WiFi lost");
            break;

        case EVENT_BME280:
            ESP_LOGI("TRIGGER", "BME280 — temp: %.2f", (double)g_bme_reading.temperature_c);
            ui_binder_update_bme280(&g_bme_reading);
            break;
        }
    }
    if (time_manager_get_time(&timeinfo)) {
        ui_binder_update_localtime(&timeinfo);
    }
}
