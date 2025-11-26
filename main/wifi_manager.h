#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi連接函數
esp_err_t wifi_init_sta(const char* ssid, const char* password);
bool wifi_is_connected(void);
void wifi_disconnect(void);

#endif // WIFI_MANAGER_H
