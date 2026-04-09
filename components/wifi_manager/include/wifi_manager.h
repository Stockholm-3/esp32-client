#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>

/* Wi-Fi state enum */
typedef enum {
    WIFI_MANAGER_STATE_IDLE = 0,
    WIFI_MANAGER_STATE_CONNECTING,
    WIFI_MANAGER_STATE_CONNECTED,
    WIFI_MANAGER_STATE_DISCONNECTED,
    WIFI_MANAGER_STATE_FAILED
} wifi_manager_state_t;

/* Callback type for state changes */
typedef void (*wifi_manager_event_cb_t)(wifi_manager_state_t state);

/* Config struct for retry/backoff */
typedef struct {
    int max_retries;   // maximum retries before FAILED
    int base_retry_ms; // initial retry delay in ms
    int max_retry_ms;  // max retry delay in ms
} wifi_manager_config_t;

/**
 * @brief Start Wi-Fi STA mode
 * @param ssid Wi-Fi SSID
 * @param password Wi-Fi password
 * @param config Optional configuration (NULL = defaults)
 * @return int 0 = success, -1 = already initialized
 */
int wifi_manager_start(const char *ssid, const char *password,
                       const wifi_manager_config_t *config);

/**
 * @brief Stop Wi-Fi and reset state
 */
void wifi_manager_stop(void);

/**
 * @brief Poll function (must be called periodically)
 */
void wifi_manager_process(void);

/**
 * @brief Get current Wi-Fi state
 */
wifi_manager_state_t wifi_manager_get_state(void);

/**
 * @brief Register callback for Wi-Fi state changes
 */
void wifi_manager_register_callback(wifi_manager_event_cb_t cb);

#endif // WIFI_MANAGER_H
