/*
 * Hi Lemon 關鍵詞檢測系統（使用 Edge Impulse）
 * 使用 Edge Impulse 模型檢測 "Hi Lemon" 喚醒詞
 */

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "audio_upload.h"
#include "sd_card_manager.h"
#include "esp_heap_caps.h"
#include "hi_esp_audio.h"
#include "location_service.h"
#include "esp_http_client.h"
#include "ei_wrapper.h"

static const char *TAG = "HI_LEMON";

// WiFi 配置
#define WIFI_SSID       "dlink-6A08"
#define WIFI_PASSWORD   "0952976105"

// 服務器配置
#define SERVER_URL          "https://nonargentiferous-fattily-robbin.ngrok-free.dev/esp32/audio"
#define LOCATION_URL        "https://nonargentiferous-fattily-robbin.ngrok-free.dev/esp32/location"
#define API_KEY             "lemongai"

// INMP441 I2S 配置
#define I2S_NUM                 I2S_NUM_0
#define I2S_SAMPLE_RATE         16000
#define I2S_BCK_PIN             GPIO_NUM_5
#define I2S_WS_PIN              GPIO_NUM_4
#define I2S_DATA_PIN            GPIO_NUM_6

// 音頻配置
#define AUDIO_BUFFER_SIZE       1024
#define RECORD_TIME_MS          3000
#define TOTAL_SAMPLES           (I2S_SAMPLE_RATE * RECORD_TIME_MS / 1000)

// Edge Impulse 檢測配置
#define EI_WINDOW_SIZE          16000   // 1 秒窗口（Edge Impulse 模型需求）
#define EI_SLIDE_SIZE           8000    // 滑動 0.5 秒
#define ENERGY_THRESHOLD        100000  // 能量閾值（避免處理靜音）
#define DETECTION_CONFIDENCE    0.7     // 檢測信心閾值（70%）

// 初始化 INMP441（24-bit 原生模式）
static esp_err_t init_inmp441(void) {
    ESP_LOGI(TAG, "🎤 初始化 INMP441 麥克風（24-bit 原生模式）...");
    
    gpio_set_pull_mode(I2S_WS_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(I2S_BCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(I2S_DATA_PIN, GPIO_PULLUP_ONLY);
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,  // 使用 32-bit 容器存儲 24-bit 數據
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = I2S_DATA_PIN
    };

    esp_err_t ret = i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ I2S 驅動安裝失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_NUM, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ I2S 引腳配置失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    i2s_zero_dma_buffer(I2S_NUM);
    
    ESP_LOGI(TAG, "✅ INMP441 初始化成功");
    return ESP_OK;
}

// 將 32-bit I2S 數據（24-bit 有效位）轉換為 16-bit
// INMP441 輸出 24-bit 數據，存儲在 32-bit 容器的高 24 位
static void convert_32bit_to_16bit(int32_t *input, int16_t *output, size_t length) {
    for (size_t i = 0; i < length; i++) {
        // 右移 16 位，將 24-bit 數據轉換為 16-bit
        // 這樣可以保留最高有效位，獲得更好的動態範圍
        output[i] = (int16_t)(input[i] >> 16);
    }
}

// 計算音頻能量
static int64_t calculate_energy(int16_t *buffer, size_t length) {
    int64_t energy = 0;
    for (size_t i = 0; i < length; i++) {
        energy += (int64_t)buffer[i] * buffer[i];
    }
    return energy / length;
}

// 輕度降噪處理
static void apply_noise_reduction(int16_t *audio_data, size_t length) {
    // 高通濾波器（去除極低頻雜訊）
    const float alpha = 0.99;
    float prev_output = 0;
    int16_t prev_input = 0;
    
    for (size_t i = 0; i < length; i++) {
        float filtered = alpha * (prev_output + audio_data[i] - prev_input);
        prev_output = filtered;
        prev_input = audio_data[i];
        audio_data[i] = (int16_t)filtered;
    }
    
    // 噪音門限
    const int16_t threshold = 50;
    for (size_t i = 0; i < length; i++) {
        if (abs(audio_data[i]) < threshold) {
            audio_data[i] = 0;
        }
    }
}

// 自動增益控制
static void apply_auto_gain(int16_t *audio_data, size_t length) {
    float rms = 0.0f;
    for (size_t i = 0; i < length; i++) {
        rms += (float)(audio_data[i] * audio_data[i]);
    }
    rms = sqrtf(rms / length);
    
    const float target_rms = 8192.0f;
    float gain = 1.0f;
    
    if (rms > 100.0f) {
        gain = target_rms / rms;
        if (gain < 1.0f) gain = 1.0f;
        if (gain > 8.0f) gain = 8.0f;
    } else {
        gain = 4.0f;
    }
    
    ESP_LOGI(TAG, "🎚️  自動增益: %.2fx (RMS: %.0f → %.0f)", gain, rms, rms * gain);
    
    for (size_t i = 0; i < length; i++) {
        int32_t amplified = (int32_t)(audio_data[i] * gain);
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        audio_data[i] = (int16_t)amplified;
    }
}

// HTTP 事件處理器
static esp_err_t tts_http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK;
}

// 下載並播放 TTS
static esp_err_t download_and_play_tts(const char* url) {
    ESP_LOGI(TAG, "📥 下載 TTS: %s", url);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = tts_http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .skip_cert_common_name_check = true,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ HTTP 客戶端初始化失敗");
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTP 連線失敗: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📊 HTTP 狀態: %d, 檔案大小: %d bytes (%.1f KB)", 
             status_code, content_length, (float)content_length / 1024);
    
    if (status_code != 200 || content_length <= 0 || content_length > 500000) {
        ESP_LOGE(TAG, "❌ 下載失敗");
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "💾 分配 PSRAM: %d bytes", content_length);
    uint8_t *wav_buffer = (uint8_t*)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM);
    if (wav_buffer == NULL) {
        ESP_LOGE(TAG, "❌ PSRAM 分配失敗");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    int total_read = 0;
    while (total_read < content_length) {
        int read_len = esp_http_client_read(client, (char*)(wav_buffer + total_read), 
                                            content_length - total_read);
        if (read_len <= 0) break;
        total_read += read_len;
        
        if (total_read % 10000 == 0 || total_read == content_length) {
            ESP_LOGI(TAG, "📥 下載進度: %d%% (%d/%d bytes)", 
                     (total_read * 100) / content_length, total_read, content_length);
        }
    }
    
    esp_http_client_cleanup(client);
    
    if (total_read != content_length) {
        ESP_LOGE(TAG, "❌ 下載不完整: %d/%d bytes", total_read, content_length);
        heap_caps_free(wav_buffer);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "✅ 下載完成: %d bytes", total_read);
    ESP_LOGI(TAG, "🔊 開始播放 TTS...");
    
    audio_play_wav_buffer(wav_buffer, content_length);
    
    heap_caps_free(wav_buffer);
    ESP_LOGI(TAG, "✅ TTS 播放完成");
    
    return ESP_OK;
}

// 從 JSON 響應中提取 TTS URL
static bool extract_tts_url(const char* json_response, char* url_buffer, size_t buffer_size) {
    const char* tts_key = "\"tts_saved\":true";
    if (strstr(json_response, tts_key) == NULL) {
        return false;
    }
    
    // 假設 TTS 文件在固定位置
    snprintf(url_buffer, buffer_size, "%s/public/voice.wav", 
             "https://nonargentiferous-fattily-robbin.ngrok-free.dev");
    return true;
}

// 錄音並上傳
static esp_err_t record_and_upload(void) {
    ESP_LOGI(TAG, "🎙️  開始錄音 %d 秒...", RECORD_TIME_MS / 1000);
    
    size_t buffer_size = TOTAL_SAMPLES * sizeof(int16_t);
    ESP_LOGI(TAG, "📊 需要分配 %zu KB 緩衝區...", buffer_size / 1024);
    ESP_LOGI(TAG, "💾 當前可用: %zu KB", heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);
    
    int16_t *audio_buffer = (int16_t*)malloc(buffer_size);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 記憶體分配失敗");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "✅ 緩衝區分配成功");
    
    size_t bytes_read = 0;
    size_t total_samples = 0;
    int32_t temp_buffer_32[AUDIO_BUFFER_SIZE];
    int16_t temp_buffer_16[AUDIO_BUFFER_SIZE];
    
    while (total_samples < TOTAL_SAMPLES) {
        i2s_read(I2S_NUM, temp_buffer_32, sizeof(temp_buffer_32), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);
        
        // 轉換 32-bit 到 16-bit
        convert_32bit_to_16bit(temp_buffer_32, temp_buffer_16, samples_read);
        
        size_t samples_to_copy = (total_samples + samples_read <= TOTAL_SAMPLES) ? 
                                 samples_read : (TOTAL_SAMPLES - total_samples);
        
        memcpy(audio_buffer + total_samples, temp_buffer_16, 
               samples_to_copy * sizeof(int16_t));
        total_samples += samples_to_copy;
        
        int progress = (total_samples * 100) / TOTAL_SAMPLES;
        if (progress % 20 == 0 || total_samples >= TOTAL_SAMPLES) {
            ESP_LOGI(TAG, "錄音進度: %d%% (%d 秒)", progress, 
                     (int)total_samples / I2S_SAMPLE_RATE);
        }
    }
    
    ESP_LOGI(TAG, "✅ 錄音完成: %zu 樣本 (%.1f 秒)", 
             total_samples, 
             (float)total_samples / I2S_SAMPLE_RATE);
    
    int64_t energy = calculate_energy(audio_buffer, TOTAL_SAMPLES);
    ESP_LOGI(TAG, "原始音頻能量: %lld", energy);
    
    ESP_LOGI(TAG, "🔧 輕度降噪...");
    apply_noise_reduction(audio_buffer, TOTAL_SAMPLES);
    apply_auto_gain(audio_buffer, TOTAL_SAMPLES);
    
    ESP_LOGI(TAG, "📤 上傳音頻到服務器...");
    
    // 分配響應緩衝區
    char* response_buffer = (char*)malloc(2048);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配響應緩衝區");
        free(audio_buffer);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = upload_audio_json(SERVER_URL, API_KEY, audio_buffer, TOTAL_SAMPLES, 
                                      I2S_SAMPLE_RATE, response_buffer, 2048);
    
    free(audio_buffer);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 音頻上傳成功");
        ESP_LOGI(TAG, "");
        
        // 提取並播放 TTS
        char tts_url[256];
        if (extract_tts_url(response_buffer, tts_url, sizeof(tts_url))) {
            download_and_play_tts(tts_url);
        }
    } else {
        ESP_LOGE(TAG, "❌ 音頻上傳失敗");
    }
    
    free(response_buffer);
    return ret;
}

// 主監聽循環（使用 Edge Impulse）
static void listen_for_hi_lemon(void) {
    ESP_LOGI(TAG, "🎤 開始監聽 'Hi Lemon'...");
    ESP_LOGI(TAG, "💡 使用 Edge Impulse 模型進行檢測（24-bit 音質）");
    
    int16_t *window_buffer = (int16_t*)malloc(EI_WINDOW_SIZE * sizeof(int16_t));
    if (window_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配檢測緩衝區");
        return;
    }
    
    // 32-bit 緩衝區用於接收 I2S 數據
    int32_t temp_buffer_32[AUDIO_BUFFER_SIZE];
    int16_t temp_buffer_16[AUDIO_BUFFER_SIZE];
    size_t window_pos = 0;
    
    while (1) {
        size_t bytes_read = 0;
        i2s_read(I2S_NUM, temp_buffer_32, sizeof(temp_buffer_32), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);
        
        // 轉換 32-bit 到 16-bit
        convert_32bit_to_16bit(temp_buffer_32, temp_buffer_16, samples_read);
        
        // 填充滑動窗口
        for (size_t i = 0; i < samples_read; i++) {
            window_buffer[window_pos] = temp_buffer_16[i];
            window_pos++;
            
            // 當窗口填滿時，執行檢測
            if (window_pos >= EI_WINDOW_SIZE) {
                // 檢查能量（避免處理靜音）
                int64_t energy = calculate_energy(window_buffer, EI_WINDOW_SIZE);
                
                if (energy > ENERGY_THRESHOLD) {
                    ESP_LOGI(TAG, "📊 檢測語音能量: %lld", energy);
                    
                    // 執行 Edge Impulse 推理
                    int label_idx = ei_wrapper_run_inference(window_buffer, EI_WINDOW_SIZE);
                    
                    if (label_idx >= 0) {
                        const char* label = ei_wrapper_get_label(label_idx);
                        ESP_LOGI(TAG, "🎯 檢測到: %s", label);
                        
                        // 檢查是否為 "hi lemon" (索引 0)
                        if (label_idx == 0 || strstr(label, "hi lemon") != NULL) {
                            ESP_LOGI(TAG, "🔊 檢測到 'Hi Lemon'！");
                            
                            // 錄音並上傳
                            record_and_upload();
                            
                            // 清空緩衝區，避免重複觸發
                            window_pos = 0;
                            memset(window_buffer, 0, EI_WINDOW_SIZE * sizeof(int16_t));
                            
                            ESP_LOGI(TAG, "🔄 繼續監聽...");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            continue;
                        }
                    }
                }
                
                // 滑動窗口
                memmove(window_buffer, window_buffer + EI_SLIDE_SIZE, 
                       (EI_WINDOW_SIZE - EI_SLIDE_SIZE) * sizeof(int16_t));
                window_pos = EI_WINDOW_SIZE - EI_SLIDE_SIZE;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    free(window_buffer);
}

void app_main(void) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   Hi Lemon 語音助理啟動中...");
    ESP_LOGI(TAG, "   使用 Edge Impulse 喚醒詞檢測");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");
    
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化 Edge Impulse
    ESP_LOGI(TAG, "🤖 初始化 Edge Impulse 模型...");
    ei_wrapper_init();
    
    // 連接 WiFi
    ESP_LOGI(TAG, "📡 連接 WiFi...");
    wifi_init_sta(WIFI_SSID, WIFI_PASSWORD);
    
    // 發送位置信息
    ESP_LOGI(TAG, "📍 發送位置信息...");
    location_info_t location;
    if (location_get_info(&location) == ESP_OK) {
        location_send_to_server(LOCATION_URL, API_KEY, &location);
    }
    
    // 初始化麥克風
    ESP_LOGI(TAG, "🎤 初始化 INMP441 麥克風...");
    if (init_inmp441() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 麥克風初始化失敗");
        return;
    }
    ESP_LOGI(TAG, "✅ INMP441 初始化成功（24-bit 原生模式）");
    
    // 初始化 SD 卡（可選）
    ESP_LOGI(TAG, "💾 初始化 SD 卡...");
    if (sd_card_init() != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ SD 卡初始化失敗（將無法保存音檔）");
    }
    
    // 初始化音頻輸出
    ESP_LOGI(TAG, "🔊 初始化音頻輸出...");
    if (audio_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 音頻輸出初始化失敗");
        return;
    }
    ESP_LOGI(TAG, "✅ 音頻輸出初始化成功");
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "✅ 系統初始化完成");
    ESP_LOGI(TAG, "📌 INMP441 接線:");
    ESP_LOGI(TAG, "   VDD → 3.3V");
    ESP_LOGI(TAG, "   GND → GND");
    ESP_LOGI(TAG, "   L/R → GND");
    ESP_LOGI(TAG, "   WS  → GPIO %d", I2S_WS_PIN);
    ESP_LOGI(TAG, "   SCK → GPIO %d", I2S_BCK_PIN);
    ESP_LOGI(TAG, "   SD  → GPIO %d", I2S_DATA_PIN);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "🗣️  請清楚地說 'Hi Lemon' 來觸發錄音");
    ESP_LOGI(TAG, "📤 檢測到關鍵詞後會錄音 %d 秒並上傳", RECORD_TIME_MS / 1000);
    ESP_LOGI(TAG, "🎵 音訊品質: %d kHz, 24-bit (INMP441 原生), 單聲道", I2S_SAMPLE_RATE / 1000);
    ESP_LOGI(TAG, "🤖 使用 Edge Impulse 模型檢測");
    ESP_LOGI(TAG, "💡 24-bit 模式提供更好的動態範圍和音質");
    ESP_LOGI(TAG, "");
    
    // 開始監聽
    listen_for_hi_lemon();
}
