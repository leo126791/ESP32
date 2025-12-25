#ifndef LOCATION_SERVICE_H
#define LOCATION_SERVICE_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 位置信息結構
typedef struct {
    char country[64];       // 國家
    char country_code[8];   // 國家代碼
    char region[64];        // 地區
    char region_name[64];   // 地區名稱
    char city[64];          // 城市
    char zip[16];           // 郵編
    float lat;              // 緯度
    float lon;              // 經度
    char timezone[64];      // 時區
    char isp[128];          // ISP
    char query[64];         // IP 地址
} location_info_t;

/**
 * @brief 從 ip-api.com 獲取位置信息
 * @param location 位置信息結構指針
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t location_get_info(location_info_t *location);

/**
 * @brief 將位置信息發送到服務器
 * @param server_url 服務器 URL
 * @param api_key API 密鑰
 * @param location 位置信息
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t location_send_to_server(const char *server_url, const char *api_key, const location_info_t *location);

#ifdef __cplusplus
}
#endif

#endif // LOCATION_SERVICE_H
