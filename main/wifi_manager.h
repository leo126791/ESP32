#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdbool.h>
#include "esp_err.h"

// WiFi連接函數（手動配置）
esp_err_t wifi_init_sta(const char* ssid, const char* password);

// WiFi配網函數（自動檢查是否已配網，未配網則啟動BLE配網）
void wifi_init_with_provisioning(void);

// WiFi狀態查詢
bool wifi_is_connected(void);
void wifi_disconnect(void);

// 重置配網資訊（用於測試或重新配網）
esp_err_t wifi_reset_provisioning(void);

#endif // WIFI_MANAGER_H
