#ifndef WIFI_H
#define WIFI_H

// Initialize Wi-Fi in STA mode with given SSID and password
void wifi_init(const char *ssid, const char *password);
bool wifi_is_connected(void);

#endif // WIFI_H
