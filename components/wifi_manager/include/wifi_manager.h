#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    WIFI_MANAGER_STATE_IDLE = 0,
    WIFI_MANAGER_STATE_CONNECTING,
    WIFI_MANAGER_STATE_CONNECTED,
    WIFI_MANAGER_STATE_DISCONNECTED,
    WIFI_MANAGER_STATE_FAILED, /* unrecoverable — bad password, SSID not found, etc. */
    WIFI_MANAGER_STATE_SCANNING,
} WifiManagerState;

typedef enum {
    WIFI_MANAGER_FAIL_REASON_UNKNOWN = 0,
    WIFI_MANAGER_FAIL_REASON_AUTH,  /* wrong password or auth rejected */
    WIFI_MANAGER_FAIL_REASON_NO_AP, /* SSID not found */
} WifiManagerFailReason;

typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode; /* maps to wifi_auth_mode_t on ESP, 0 = open on stub */
} WifiManagerApInfo;

/*
 * Invoked on every state transition. When state is WIFI_MANAGER_STATE_FAILED,
 * reason indicates why — otherwise reason is WIFI_MANAGER_FAIL_REASON_UNKNOWN.
 */
typedef void (*WifiManagerEventCb)(WifiManagerState state, WifiManagerFailReason reason);

/*
 * Delivered once when a scan completes. The result buffer is only valid for
 * the duration of this call — copy any entries you need to retain.
 */
typedef void (*WifiManagerScanDoneCb)(const WifiManagerApInfo* results, uint16_t count);

typedef struct {
    int max_retries;
    int base_retry_ms;
    int max_retry_ms;
} WifiManagerConfig;

/**
 * @brief Start Wi-Fi STA mode.
 *
 * Assumes nvs_flash_init(), esp_netif_init(), and
 * esp_event_loop_create_default() have already been called by the application.
 * Retry backoff is handled internally via a FreeRTOS timer.
 *
 * Transient disconnections (e.g. router reboot) are retried indefinitely with
 * exponential backoff capped at max_retry_ms. Unrecoverable failures (wrong
 * password, SSID not found) stop immediately and transition to
 * WIFI_MANAGER_STATE_FAILED with an appropriate reason code.
 *
 * @param ssid     Target SSID.
 * @param password Target password.
 * @param config   Optional retry/backoff config. NULL uses defaults.
 * @return 0 on success, -1 on failure.
 */
int wifi_manager_start(const char* ssid, const char* password, const WifiManagerConfig* config);

/**
 * @brief Stop Wi-Fi, cancel any pending retry, and reset all state.
 */
void wifi_manager_stop(void);

/**
 * @brief Force an immediate reconnection attempt, cancelling any pending retry timer.
 *
 * Also clears a FAILED state, allowing recovery after supplying new credentials
 * via wifi_manager_change_network().
 */
void wifi_manager_reconnect(void);

/**
 * @brief Switch to a different network without reinitializing the Wi-Fi stack.
 *
 * Cancels any pending retry, applies the new credentials, and connects
 * immediately. Safe to call while connected, disconnected, or in FAILED state.
 *
 * @param ssid     New target SSID.
 * @param password New target password.
 * @return 0 on success, -1 if not initialized.
 */
int wifi_manager_change_network(const char* ssid, const char* password);

/**
 * @brief Trigger an asynchronous AP scan.
 *
 * Results are delivered via cb from the Wi-Fi event loop. The manager must
 * be initialized before calling this.
 *
 * @param cb Called once when the scan completes. Must not be NULL.
 * @return 0 on success, -1 if not initialized or a scan is already in progress.
 */
int wifi_manager_scan(WifiManagerScanDoneCb cb);

/**
 * @brief Get the current Wi-Fi state.
 */
WifiManagerState wifi_manager_get_state(void);

/**
 * @brief Register a callback invoked on every state transition.
 */
void wifi_manager_register_callback(WifiManagerEventCb cb);

#endif /* WIFI_MANAGER_H */
