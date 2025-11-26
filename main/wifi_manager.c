#include "wifi_manager.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "WIFI_MANAGER";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          3

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA 已啟動，開始連接...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi斷線，原因: %d", disconn->reason);
        
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "🔗 重試連接WiFi (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            ESP_LOGE(TAG, "❌ WiFi連接失敗，已達最大重試次數");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "✅ 獲得IP地址:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_init_sta(const char* ssid, const char* password)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // 配置WiFi連接（先配置再啟動）
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    // 設置WiFi為STA模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 關閉省電模式以提高穩定性
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    // 設置主機名
    ESP_ERROR_CHECK(esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), "esp32s3-mic"));
    
    // 啟動WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "🔗 開始連接WiFi: %s", ssid);

    // 等待連接結果（增加超時時間）
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(15000));  // 15秒超時

    if (bits & WIFI_CONNECTED_BIT) {
        // 獲取並顯示連接信息
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "✅ WiFi連接成功");
            ESP_LOGI(TAG, "   SSID: %s", ap_info.ssid);
            ESP_LOGI(TAG, "   RSSI: %d dBm", ap_info.rssi);
            ESP_LOGI(TAG, "   Channel: %d", ap_info.primary);
        }
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "❌ WiFi連接失敗（多次重試）");
        ESP_LOGE(TAG, "   可能原因: 密碼錯誤 / 信號太弱 / AP不可用");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "❌ WiFi連接超時");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_is_connected(void)
{
    // 檢查WiFi連接狀態
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        ESP_LOGD("WIFI_MANAGER", "WiFi已連接: %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
        return true;
    }
    
    ESP_LOGD("WIFI_MANAGER", "WiFi未連接，錯誤: %s", esp_err_to_name(ret));
    return false;
}

void wifi_disconnect(void)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
}
