#include "location_service.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "LOCATION";

// ip-api.com 的 API endpoint
#define IP_API_URL "http://ip-api.com/json/?fields=status,message,country,countryCode,region,regionName,city,zip,lat,lon,timezone,isp,query"

// HTTP 響應緩衝區
static char response_buffer[2048];
static int response_len = 0;

// HTTP 事件處理器
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (response_len + evt->data_len < sizeof(response_buffer)) {
                memcpy(response_buffer + response_len, evt->data, evt->data_len);
                response_len += evt->data_len;
                response_buffer[response_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t location_get_info(location_info_t *location)
{
    if (location == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(location, 0, sizeof(location_info_t));
    response_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));

    ESP_LOGI(TAG, "📍 正在獲取位置信息...");

    // 配置 HTTP 客戶端
    esp_http_client_config_t config = {
        .url = IP_API_URL,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ 無法初始化 HTTP 客戶端");
        return ESP_FAIL;
    }

    // 執行 GET 請求
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET 狀態 = %d, 內容長度 = %d", status_code, response_len);

        if (status_code == 200 && response_len > 0) {
            // 解析 JSON 響應
            cJSON *root = cJSON_Parse(response_buffer);
            if (root == NULL) {
                ESP_LOGE(TAG, "❌ JSON 解析失敗");
                esp_http_client_cleanup(client);
                return ESP_FAIL;
            }

            // 檢查狀態
            cJSON *status = cJSON_GetObjectItem(root, "status");
            if (status && strcmp(status->valuestring, "success") == 0) {
                // 提取位置信息
                cJSON *item;
                
                if ((item = cJSON_GetObjectItem(root, "country")) != NULL && item->valuestring) {
                    strncpy(location->country, item->valuestring, sizeof(location->country) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "countryCode")) != NULL && item->valuestring) {
                    strncpy(location->country_code, item->valuestring, sizeof(location->country_code) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "region")) != NULL && item->valuestring) {
                    strncpy(location->region, item->valuestring, sizeof(location->region) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "regionName")) != NULL && item->valuestring) {
                    strncpy(location->region_name, item->valuestring, sizeof(location->region_name) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "city")) != NULL && item->valuestring) {
                    strncpy(location->city, item->valuestring, sizeof(location->city) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "zip")) != NULL && item->valuestring) {
                    strncpy(location->zip, item->valuestring, sizeof(location->zip) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "lat")) != NULL) {
                    location->lat = (float)item->valuedouble;
                }
                if ((item = cJSON_GetObjectItem(root, "lon")) != NULL) {
                    location->lon = (float)item->valuedouble;
                }
                if ((item = cJSON_GetObjectItem(root, "timezone")) != NULL && item->valuestring) {
                    strncpy(location->timezone, item->valuestring, sizeof(location->timezone) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "isp")) != NULL && item->valuestring) {
                    strncpy(location->isp, item->valuestring, sizeof(location->isp) - 1);
                }
                if ((item = cJSON_GetObjectItem(root, "query")) != NULL && item->valuestring) {
                    strncpy(location->query, item->valuestring, sizeof(location->query) - 1);
                }

                ESP_LOGI(TAG, "✅ 位置信息獲取成功:");
                ESP_LOGI(TAG, "   IP: %s", location->query);
                ESP_LOGI(TAG, "   國家: %s (%s)", location->country, location->country_code);
                ESP_LOGI(TAG, "   城市: %s, %s", location->city, location->region_name);
                ESP_LOGI(TAG, "   座標: %.4f, %.4f", location->lat, location->lon);
                ESP_LOGI(TAG, "   時區: %s", location->timezone);
                ESP_LOGI(TAG, "   ISP: %s", location->isp);

                cJSON_Delete(root);
                esp_http_client_cleanup(client);
                return ESP_OK;
            } else {
                cJSON *message = cJSON_GetObjectItem(root, "message");
                ESP_LOGE(TAG, "❌ API 返回錯誤: %s", message ? message->valuestring : "unknown");
                cJSON_Delete(root);
            }
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP GET 請求失敗: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return ESP_FAIL;
}

esp_err_t location_send_to_server(const char *server_url, const char *api_key, const location_info_t *location)
{
    if (server_url == NULL || api_key == NULL || location == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "📤 發送位置信息到服務器...");

    // 創建 JSON 數據
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "api_key", api_key);
    cJSON_AddStringToObject(root, "ip", location->query);
    cJSON_AddStringToObject(root, "country", location->country);
    cJSON_AddStringToObject(root, "country_code", location->country_code);
    cJSON_AddStringToObject(root, "city", location->city);
    cJSON_AddStringToObject(root, "region", location->region_name);
    cJSON_AddNumberToObject(root, "latitude", location->lat);
    cJSON_AddNumberToObject(root, "longitude", location->lon);
    cJSON_AddStringToObject(root, "timezone", location->timezone);
    cJSON_AddStringToObject(root, "isp", location->isp);

    char *json_data = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_data == NULL) {
        ESP_LOGE(TAG, "❌ JSON 創建失敗");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "發送數據: %s", json_data);

    // 配置 HTTP 客戶端
    response_len = 0;
    memset(response_buffer, 0, sizeof(response_buffer));

    esp_http_client_config_t config = {
        .url = server_url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ 無法初始化 HTTP 客戶端");
        free(json_data);
        return ESP_FAIL;
    }

    // 設置 POST 請求
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_data, strlen(json_data));

    // 執行請求
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP POST 狀態 = %d", status_code);
        
        if (status_code == 200) {
            ESP_LOGI(TAG, "✅ 位置信息發送成功");
            if (response_len > 0) {
                ESP_LOGI(TAG, "服務器響應: %s", response_buffer);
            }
            free(json_data);
            esp_http_client_cleanup(client);
            return ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "❌ HTTP POST 請求失敗: %s", esp_err_to_name(err));
    }

    free(json_data);
    esp_http_client_cleanup(client);
    return ESP_FAIL;
}
