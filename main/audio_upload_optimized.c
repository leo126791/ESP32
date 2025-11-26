#include "audio_upload.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "mbedtls/base64.h"
#include "cJSON.h"

static const char *TAG = "AUDIO_UPLOAD_OPT";

// 創建 WAV 頭
void create_wav_header(uint8_t* header, uint32_t data_size, uint32_t sample_rate)
{
    uint32_t chunk_size = 36 + data_size;
    uint32_t byte_rate = sample_rate * 1 * 2;
    uint16_t block_align = 1 * 2;

    memcpy(&header[0], "RIFF", 4);
    *(uint32_t*)&header[4] = chunk_size;
    memcpy(&header[8], "WAVE", 4);
    memcpy(&header[12], "fmt ", 4);
    *(uint32_t*)&header[16] = 16;
    *(uint16_t*)&header[20] = 1;
    *(uint16_t*)&header[22] = 1;
    *(uint32_t*)&header[24] = sample_rate;
    *(uint32_t*)&header[28] = byte_rate;
    *(uint16_t*)&header[32] = block_align;
    *(uint16_t*)&header[34] = 16;
    memcpy(&header[36], "data", 4);
    *(uint32_t*)&header[40] = data_size;
}

// 優化版本：直接發送二進位 WAV（無 Base64 開銷）
esp_err_t upload_audio_json(const char* url, 
                            const char* api_key,
                            const int16_t* audio_data, 
                            size_t audio_len,
                            uint32_t sample_rate,
                            char* response_buffer,
                            size_t response_size)
{
    ESP_LOGI(TAG, "🌐 直接上傳 WAV: %zu 樣本 (%.1f 秒)", 
             audio_len, (float)audio_len / sample_rate);
    
    size_t pcm_bytes = audio_len * sizeof(int16_t);
    size_t wav_total = 44 + pcm_bytes;
    
    ESP_LOGI(TAG, "WAV 總大小: %zu bytes (%.1f KB)", wav_total, (float)wav_total / 1024);
    
    // HTTP 客戶端配置（優化 TCP 設定）
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 120000,  // 增加到 120 秒（Whisper + ChatGPT + TTS 需要時間）
        .skip_cert_common_name_check = true,
        .cert_pem = NULL,
        .use_global_ca_store = false,
        .buffer_size = 8192,      // 增加接收緩衝
        .buffer_size_tx = 4096,
        .keep_alive_enable = true,
        .keep_alive_idle = 10,
        .keep_alive_interval = 10,
        .keep_alive_count = 3,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ HTTP 客戶端初始化失敗");
        return ESP_FAIL;
    }
    
    // 設置 HTTP 頭 - 直接發送二進位 WAV
    esp_http_client_set_header(client, "Content-Type", "audio/wav");
    esp_http_client_set_header(client, "X-API-KEY", api_key);
    
    // 設置 POST 欄位（讓 ESP-IDF 自動處理 Content-Length）
    char content_len_str[32];
    snprintf(content_len_str, sizeof(content_len_str), "%zu", wav_total);
    esp_http_client_set_header(client, "Content-Length", content_len_str);
    
    // 開啟連線
    ESP_LOGI(TAG, "🔌 開啟 HTTP 連線 (Content-Length: %zu)...", wav_total);
    esp_err_t err = esp_http_client_open(client, wav_total);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "❌ 無法開啟 HTTP 連線: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "✅ 連線已建立，開始發送 WAV...");
    
    // 1. 發送 WAV 頭（44 bytes）
    uint8_t wav_header[44];
    create_wav_header(wav_header, pcm_bytes, sample_rate);
    
    int written = esp_http_client_write(client, (char*)wav_header, 44);
    if (written != 44) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "❌ WAV 頭寫入失敗: written=%d", written);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "📤 已發送 WAV 頭: 44 bytes");
    
    // 2. 分塊發送 PCM 數據（速度優化版）
    size_t total_sent = 0;
    size_t chunk_size = 2048;  // 增加到 2KB 加快速度
    int consecutive_failures = 0;
    
    ESP_LOGI(TAG, "開始發送 PCM 數據 (%zu bytes)...", pcm_bytes);
    
    while (total_sent < pcm_bytes) {
        size_t remaining = pcm_bytes - total_sent;
        size_t to_write = (remaining > chunk_size) ? chunk_size : remaining;
        
        // 嘗試寫入
        written = esp_http_client_write(client, (const char*)audio_data + total_sent, to_write);
        
        if (written <= 0) {
            consecutive_failures++;
            ESP_LOGW(TAG, "⚠️ 寫入失敗: %d (連續失敗: %d)", written, consecutive_failures);
            
            if (consecutive_failures >= 5) {
                esp_http_client_cleanup(client);
                ESP_LOGE(TAG, "❌ 連續失敗 5 次，放棄 (已發送 %zu/%zu bytes)", total_sent, pcm_bytes);
                return ESP_FAIL;
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        // 寫入成功
        consecutive_failures = 0;
        total_sent += written;
        
        // 每 8KB 才 log 一次（減少 log 開銷）
        if (total_sent % 8192 == 0 || total_sent == pcm_bytes) {
            ESP_LOGI(TAG, "📤 進度: %zu/%zu bytes (%.1f%%)", 
                     total_sent, pcm_bytes, (float)total_sent * 100 / pcm_bytes);
        }
        
        // 最小延遲，加快發送
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    ESP_LOGI(TAG, "✅ 已發送完整 WAV (%zu bytes)", wav_total);
    
    // 獲取響應
    ESP_LOGI(TAG, "⏳ 等待伺服器響應（可能需要 30-60 秒處理 AI...）");
    
    // 添加進度指示
    int wait_count = 0;
    int content_length = -1;
    int status_code = -1;
    
    while (wait_count < 120) {  // 最多等待 120 秒
        content_length = esp_http_client_fetch_headers(client);
        status_code = esp_http_client_get_status_code(client);
        
        if (status_code > 0) {
            break;  // 收到響應
        }
        
        if (wait_count % 10 == 0) {
            ESP_LOGI(TAG, "⏳ 等待中... (%d 秒)", wait_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_count++;
    }
    
    ESP_LOGI(TAG, "📊 HTTP 狀態碼: %d, Content-Length: %d", status_code, content_length);
    
    if (status_code == -1 || status_code == 0) {
        ESP_LOGE(TAG, "❌ 連線超時或網絡錯誤（等待了 %d 秒）", wait_count);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // 讀取響應內容
    if (content_length > 0) {
        int buffer_size = (content_length < 4096) ? content_length + 1 : 4096;
        char *temp_buffer = malloc(buffer_size);
        if (temp_buffer) {
            int read_len = esp_http_client_read(client, temp_buffer, buffer_size - 1);
            if (read_len > 0) {
                temp_buffer[read_len] = '\0';
                ESP_LOGI(TAG, "📨 伺服器響應: %s", temp_buffer);
                
                if (response_buffer && response_size > 0) {
                    size_t copy_len = (read_len < response_size - 1) ? read_len : response_size - 1;
                    memcpy(response_buffer, temp_buffer, copy_len);
                    response_buffer[copy_len] = '\0';
                }
            }
            free(temp_buffer);
        }
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (status_code == 200 || status_code == 201) {
        ESP_LOGI(TAG, "✅ 上傳成功");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ 上傳失敗: status=%d", status_code);
        return ESP_FAIL;
    }
}

// 上傳音頻和位置資訊（位置放在 HTTP Header "x-esp32-loc" 中）
esp_err_t upload_audio_with_location(const char* url, 
                                     const char* api_key,
                                     const int16_t* audio_data, 
                                     size_t audio_len,
                                     uint32_t sample_rate,
                                     const location_info_t* location,
                                     char* response_buffer,
                                     size_t response_size)
{
    ESP_LOGI(TAG, "🌐 上傳 WAV + 位置資訊: %zu 樣本 (%.1f 秒)", 
             audio_len, (float)audio_len / sample_rate);
    
    size_t pcm_bytes = audio_len * sizeof(int16_t);
    size_t wav_total = 44 + pcm_bytes;
    
    ESP_LOGI(TAG, "WAV 總大小: %zu bytes (%.1f KB)", wav_total, (float)wav_total / 1024);
    
    // 創建位置 JSON（如果提供）
    char* location_json = NULL;
    if (location) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "ip", location->query);
        cJSON_AddStringToObject(root, "country", location->country);
        cJSON_AddStringToObject(root, "country_code", location->country_code);
        cJSON_AddStringToObject(root, "city", location->city);
        cJSON_AddStringToObject(root, "region", location->region_name);
        cJSON_AddNumberToObject(root, "latitude", location->lat);
        cJSON_AddNumberToObject(root, "longitude", location->lon);
        cJSON_AddStringToObject(root, "timezone", location->timezone);
        cJSON_AddStringToObject(root, "isp", location->isp);
        
        location_json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        
        if (location_json) {
            ESP_LOGI(TAG, "📍 位置資訊: %s", location_json);
        }
    }
    
    // HTTP 客戶端配置
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 120000,
        .skip_cert_common_name_check = true,
        .buffer_size = 8192,
        .buffer_size_tx = 4096,
        .keep_alive_enable = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ HTTP 客戶端初始化失敗");
        if (location_json) free(location_json);
        return ESP_FAIL;
    }
    
    // 設置 HTTP 頭
    esp_http_client_set_header(client, "Content-Type", "audio/wav");
    esp_http_client_set_header(client, "X-API-KEY", api_key);
    
    // 將位置資訊放在 HTTP Header "x-esp32-loc" 中
    if (location_json) {
        esp_http_client_set_header(client, "x-esp32-loc", location_json);
        ESP_LOGI(TAG, "✅ 位置資訊已加入 HTTP Header");
    }
    
    char content_len_str[32];
    snprintf(content_len_str, sizeof(content_len_str), "%zu", wav_total);
    esp_http_client_set_header(client, "Content-Length", content_len_str);
    
    // 開啟連線
    ESP_LOGI(TAG, "🔌 開啟 HTTP 連線 (Content-Length: %zu)...", wav_total);
    esp_err_t err = esp_http_client_open(client, wav_total);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        if (location_json) free(location_json);
        ESP_LOGE(TAG, "❌ 無法開啟 HTTP 連線: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "✅ 連線已建立，開始發送 WAV...");
    
    // 1. 發送 WAV 頭（44 bytes）
    uint8_t wav_header[44];
    create_wav_header(wav_header, pcm_bytes, sample_rate);
    
    int written = esp_http_client_write(client, (char*)wav_header, 44);
    if (written != 44) {
        esp_http_client_cleanup(client);
        if (location_json) free(location_json);
        ESP_LOGE(TAG, "❌ WAV 頭寫入失敗: written=%d", written);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "📤 已發送 WAV 頭: 44 bytes");
    
    // 2. 分塊發送 PCM 數據
    size_t total_sent = 0;
    size_t chunk_size = 2048;
    int consecutive_failures = 0;
    
    ESP_LOGI(TAG, "開始發送 PCM 數據 (%zu bytes)...", pcm_bytes);
    
    while (total_sent < pcm_bytes) {
        size_t remaining = pcm_bytes - total_sent;
        size_t to_write = (remaining > chunk_size) ? chunk_size : remaining;
        
        written = esp_http_client_write(client, (const char*)audio_data + total_sent, to_write);
        
        if (written <= 0) {
            consecutive_failures++;
            ESP_LOGW(TAG, "⚠️ 寫入失敗: %d (連續失敗: %d)", written, consecutive_failures);
            
            if (consecutive_failures >= 5) {
                esp_http_client_cleanup(client);
                if (location_json) free(location_json);
                ESP_LOGE(TAG, "❌ 連續失敗 5 次，放棄 (已發送 %zu/%zu bytes)", total_sent, pcm_bytes);
                return ESP_FAIL;
            }
            
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        
        consecutive_failures = 0;
        total_sent += written;
        
        if (total_sent % 8192 == 0 || total_sent == pcm_bytes) {
            ESP_LOGI(TAG, "📤 進度: %zu/%zu bytes (%.1f%%)", 
                     total_sent, pcm_bytes, (float)total_sent * 100 / pcm_bytes);
        }
        
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    
    if (location_json) free(location_json);
    
    ESP_LOGI(TAG, "✅ 已發送完整 WAV (%zu bytes)", wav_total);
    
    // 獲取響應
    ESP_LOGI(TAG, "⏳ 等待伺服器響應（可能需要 30-60 秒處理 AI...）");
    
    int wait_count = 0;
    int content_length = -1;
    int status_code = -1;
    
    while (wait_count < 120) {
        content_length = esp_http_client_fetch_headers(client);
        status_code = esp_http_client_get_status_code(client);
        
        if (status_code > 0) {
            break;
        }
        
        if (wait_count % 10 == 0) {
            ESP_LOGI(TAG, "⏳ 等待中... (%d 秒)", wait_count);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        wait_count++;
    }
    
    ESP_LOGI(TAG, "📊 HTTP 狀態碼: %d, Content-Length: %d", status_code, content_length);
    
    if (status_code == -1 || status_code == 0) {
        ESP_LOGE(TAG, "❌ 連線超時或網絡錯誤（等待了 %d 秒）", wait_count);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // 讀取響應內容
    if (content_length > 0) {
        int buffer_size = (content_length < 4096) ? content_length + 1 : 4096;
        char *temp_buffer = malloc(buffer_size);
        if (temp_buffer) {
            int read_len = esp_http_client_read(client, temp_buffer, buffer_size - 1);
            if (read_len > 0) {
                temp_buffer[read_len] = '\0';
                ESP_LOGI(TAG, "📨 伺服器響應: %s", temp_buffer);
                
                if (response_buffer && response_size > 0) {
                    size_t copy_len = (read_len < response_size - 1) ? read_len : response_size - 1;
                    memcpy(response_buffer, temp_buffer, copy_len);
                    response_buffer[copy_len] = '\0';
                }
            }
            free(temp_buffer);
        }
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (status_code == 200 || status_code == 201) {
        ESP_LOGI(TAG, "✅ 上傳成功");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ 上傳失敗: status=%d", status_code);
        return ESP_FAIL;
    }
}
