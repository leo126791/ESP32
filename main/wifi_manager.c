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

// Include provisioning manager
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

static const char *TAG = "WIFI_MANAGER";

// BLE Provisioning Configuration
#define PROV_DEVICE_NAME_PREFIX "PROV_Lemon_"
#define PROV_SECURITY_VER       WIFI_PROV_SECURITY_1
#define PROV_POP                "lemon123"  // Provisioning password

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          3

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                         int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "WiFi STA started, connecting...");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "WiFi disconnected, reason: %d", disconn->reason);
        
        if (s_retry_num < MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retry WiFi connection (%d/%d)", s_retry_num, MAX_RETRY);
        } else {
            ESP_LOGE(TAG, "WiFi connection failed, max retry reached");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
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

    // Configure WiFi connection
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

    // Set WiFi to STA mode
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // Disable power save mode for better stability
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    
    // Set hostname
    ESP_ERROR_CHECK(esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), "esp32s3-mic"));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    // Wait for connection result (15 seconds timeout)
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        // Get and display connection info
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected successfully");
            ESP_LOGI(TAG, "   SSID: %s", ap_info.ssid);
            ESP_LOGI(TAG, "   RSSI: %d dBm", ap_info.rssi);
            ESP_LOGI(TAG, "   Channel: %d", ap_info.primary);
        }
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "WiFi connection failed (max retry)");
        ESP_LOGE(TAG, "   Possible reasons: wrong password / weak signal / AP unavailable");
        return ESP_FAIL;
    } else {
        ESP_LOGE(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_is_connected(void)
{
    // Check WiFi connection status
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    
    if (ret == ESP_OK) {
        ESP_LOGD("WIFI_MANAGER", "WiFi connected: %s, RSSI: %d", ap_info.ssid, ap_info.rssi);
        return true;
    }
    
    ESP_LOGD("WIFI_MANAGER", "WiFi not connected, error: %s", esp_err_to_name(ret));
    return false;
}

void wifi_disconnect(void)
{
    esp_wifi_disconnect();
    esp_wifi_stop();
}

/* ========== BLE Provisioning Functions ========== */

// Provisioning event handler
static void prov_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_PROV_EVENT) {
        switch (event_id) {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started, waiting for phone connection...");
                break;
            case WIFI_PROV_CRED_RECV: {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG, "Received Wi-Fi credentials");
                ESP_LOGI(TAG, "   SSID: %s", (const char *) wifi_sta_cfg->ssid);
                ESP_LOGI(TAG, "   Password: %s", (const char *) wifi_sta_cfg->password);
                break;
            }
            case WIFI_PROV_CRED_FAIL: {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG, "Provisioning failed, reason: %s",
                        (*reason == WIFI_PROV_STA_AUTH_ERROR) ?
                        "Wi-Fi auth failed (wrong password)" : "Wi-Fi AP not found");
                break;
            }
            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful! Connecting to Wi-Fi...");
                break;
            case WIFI_PROV_END:
                ESP_LOGI(TAG, "Provisioning ended, releasing BT memory");
                wifi_prov_mgr_deinit();
                break;
            default:
                break;
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected to Wi-Fi, IP: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected, reconnecting...");
        esp_wifi_connect();
    }
}

// Get device BLE service name (with MAC address suffix)
static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = PROV_DEVICE_NAME_PREFIX;
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X",
             ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

// Start Wi-Fi provisioning (auto-check if already provisioned)
void wifi_init_with_provisioning(void)
{
    // 1. Initialize TCP/IP and Event Loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Initialize Wi-Fi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. Register events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, 
                                               &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                               &prov_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, 
                                               &prov_event_handler, NULL));

    // 4. Initialize provisioning manager
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM  // Free BT memory after provisioning
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    // 5. Check if already provisioned
    bool provisioned = false;
    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    if (provisioned) {
        ESP_LOGI(TAG, "Device already provisioned, starting Wi-Fi");
        wifi_prov_mgr_deinit();
        
        // Disable power save mode for better stability
        ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
        
        esp_wifi_set_mode(WIFI_MODE_STA);
        esp_wifi_start();
    } else {
        ESP_LOGI(TAG, "Device not provisioned, starting BLE provisioning mode...");
        
        // Set BLE service name
        char service_name[32];
        get_device_service_name(service_name, sizeof(service_name));
        
        // Set security parameters
        wifi_prov_security_t security = PROV_SECURITY_VER;
        const char *pop = PROV_POP;
        
        // Start provisioning
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, pop, service_name, NULL));
        
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Please use phone APP to connect BLE device:");
        ESP_LOGI(TAG, "   Device name: %s", service_name);
        ESP_LOGI(TAG, "   Provisioning password: %s", pop);
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Download APP:");
        ESP_LOGI(TAG, "   iOS: ESP BLE Provisioning");
        ESP_LOGI(TAG, "   Android: ESP BLE Provisioning");
        ESP_LOGI(TAG, "========================================");
    }
}

// Reset provisioning info (for testing or re-provisioning)
esp_err_t wifi_reset_provisioning(void)
{
    ESP_LOGI(TAG, "Resetting provisioning info...");
    
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
    };
    
    esp_err_t ret = wifi_prov_mgr_init(config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Provisioning manager init failed");
        return ret;
    }
    
    wifi_prov_mgr_reset_provisioning();
    wifi_prov_mgr_deinit();
    
    ESP_LOGI(TAG, "Provisioning info reset, please restart device");
    return ESP_OK;
}
