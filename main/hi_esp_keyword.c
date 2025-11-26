/*
 * Hi ESP 關鍵詞檢測系統
 * 檢測到 "Hi ESP" 後錄音並上傳到服務器
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "wifi_manager.h"
#include "audio_upload.h"
#include "sd_card_manager.h"
#include "esp_heap_caps.h"
#include "hi_esp_audio.h"
#include "location_service.h"
#include "esp_http_client.h"

static const char *TAG = "HI_ESP_KEYWORD";

// WiFi 配置
#define WIFI_SSID       "dlink-6A08"
#define WIFI_PASSWORD   "0952976105"

// 服務器配置
#define SERVER_URL          "https://nonargentiferous-fattily-robbin.ngrok-free.dev/esp32/audio"
#define LOCATION_URL        "https://nonargentiferous-fattily-robbin.ngrok-free.dev/esp32/location"
#define API_KEY             "lemongai"

// INMP441 I2S 配置（方案 A：標準配置）
#define I2S_NUM                 I2S_NUM_0
#define I2S_SAMPLE_RATE         16000   // 16kHz（記憶體友好）
#define I2S_BCK_PIN             GPIO_NUM_5   // 原 39
#define I2S_WS_PIN              GPIO_NUM_4   // 原 38
#define I2S_DATA_PIN            GPIO_NUM_6   // 原 37

// 音頻配置
#define AUDIO_BUFFER_SIZE       1024    // 標準緩衝區
#define RECORD_TIME_MS          3000    // 3 秒（記憶體友好）
#define TOTAL_SAMPLES           (I2S_SAMPLE_RATE * RECORD_TIME_MS / 1000)

// 關鍵詞檢測配置（優化版 - 平衡靈敏度和準確度）
#define KEYWORD_WINDOW_MS       500     // 檢測窗口 500ms
#define KEYWORD_SAMPLES         (I2S_SAMPLE_RATE * KEYWORD_WINDOW_MS / 1000)
#define ENERGY_THRESHOLD        150000  // 降低能量閾值（更容易觸發）
#define SILENCE_THRESHOLD       50000   // 靜音閾值（用於檢測停頓）
#define MIN_VOICE_DURATION_MS   150     // 最短語音持續時間（避免誤觸發）
#define MAX_VOICE_DURATION_MS   2000    // 最長語音持續時間
#define MIN_HIGH_AMPLITUDE      800     // 最小高振幅樣本值（降低要求）

// "Hi ESP" 特徵檢測（放寬要求）
#define HI_DURATION_MIN         80      // "Hi" 最短持續時間 (ms) - 放寬
#define HI_DURATION_MAX         400     // "Hi" 最長持續時間 (ms) - 放寬
#define PAUSE_DURATION_MIN      30      // 停頓最短時間 (ms) - 放寬
#define PAUSE_DURATION_MAX      300     // 停頓最長時間 (ms) - 放寬
#define ESP_DURATION_MIN        100     // "ESP" 最短持續時間 (ms) - 放寬
#define ESP_DURATION_MAX        500     // "ESP" 最長持續時間 (ms) - 放寬

// 初始化 INMP441（使用舊 I2S API）
static esp_err_t init_inmp441(void) {
    ESP_LOGI(TAG, "初始化 INMP441 麥克風（記憶體優化：16kHz）...");
    
    // 配置 GPIO 上拉（穩定信號線，改善接地問題）
    gpio_set_pull_mode(I2S_WS_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(I2S_BCK_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(I2S_DATA_PIN, GPIO_PULLUP_ONLY);
    ESP_LOGI(TAG, "✓ GPIO 上拉已啟用（改善穩定性）");
    
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = I2S_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // 使用 16-bit（記憶體友好）
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,     // 標準 DMA 緩衝區
        .dma_buf_len = 256,     // 標準緩衝區長度
        .use_apll = false,      // 不使用 APLL（更穩定）
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
        ESP_LOGE(TAG, "I2S 驅動安裝失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = i2s_set_pin(I2S_NUM, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 引腳配置失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    i2s_zero_dma_buffer(I2S_NUM);
    
    ESP_LOGI(TAG, "✅ INMP441 初始化成功");
    return ESP_OK;
}

// 計算音頻能量
static float calculate_energy(int16_t *buffer, int length) {
    float energy = 0.0;
    for (int i = 0; i < length; i++) {
        energy += (float)(buffer[i] * buffer[i]);
    }
    return energy / length;
}

// 計算過零率 (Zero Crossing Rate)
static float calculate_zcr(int16_t *buffer, int length) {
    int zero_crossings = 0;
    for (int i = 1; i < length; i++) {
        if ((buffer[i] >= 0 && buffer[i-1] < 0) || 
            (buffer[i] < 0 && buffer[i-1] >= 0)) {
            zero_crossings++;
        }
    }
    return (float)zero_crossings / length;
}

// 改進的語音檢測（能量 + 持續時間 + 動態閾值）
static bool detect_voice(int16_t *buffer, int length) {
    float energy = calculate_energy(buffer, length);
    
    // 每 2 秒顯示一次檢測狀態
    static int log_counter = 0;
    if (log_counter++ % 20 == 0) {
        ESP_LOGI(TAG, "📊 能量: %.0f (閾值: %.0f)", energy, (float)ENERGY_THRESHOLD);
    }
    
    // 能量必須超過閾值
    if (energy > ENERGY_THRESHOLD) {
        // 額外檢查：確保不是單一脈衝（降低要求）
        int high_samples = 0;
        for (int i = 0; i < length; i++) {
            if (abs(buffer[i]) > MIN_HIGH_AMPLITUDE) {  // 使用可配置的閾值
                high_samples++;
            }
        }
        
        // 至少 15% 的樣本要有明顯振幅（降低要求）
        if (high_samples > length / 7) {
            return true;
        }
    }
    
    return false;
}

// 檢測 "Hi ESP" 模式（優化版 - 更靈活的檢測）
// 返回: true 如果檢測到 "Hi ESP" 模式
static bool detect_hi_esp_pattern(int16_t *buffer, int total_samples) {
    const int chunk_size = (I2S_SAMPLE_RATE * 40) / 1000;  // 40ms 塊（更細緻）
    int num_chunks = total_samples / chunk_size;
    
    if (num_chunks < 4) return false;  // 至少需要 160ms
    
    // 計算每個塊的能量
    float *energies = (float*)malloc(num_chunks * sizeof(float));
    if (!energies) return false;
    
    float max_energy = 0;
    for (int i = 0; i < num_chunks; i++) {
        energies[i] = calculate_energy(buffer + i * chunk_size, chunk_size);
        if (energies[i] > max_energy) max_energy = energies[i];
    }
    
    // 動態調整閾值（基於最大能量）
    float dynamic_threshold = max_energy * 0.3;  // 30% 的最大能量
    if (dynamic_threshold < ENERGY_THRESHOLD * 0.5) {
        dynamic_threshold = ENERGY_THRESHOLD * 0.5;  // 最低閾值
    }
    
    ESP_LOGI(TAG, "🔍 動態閾值: %.0f (最大能量: %.0f)", dynamic_threshold, max_energy);
    
    // 尋找模式: 高能量 -> 低能量 -> 高能量
    // 對應: "Hi" -> 停頓 -> "ESP"
    bool pattern_found = false;
    
    for (int i = 0; i < num_chunks - 3; i++) {
        // 檢查是否有 "Hi" (高能量段)
        bool has_hi = false;
        int hi_start = -1;
        for (int j = i; j < i + 3 && j < num_chunks; j++) {
            if (energies[j] > dynamic_threshold) {
                has_hi = true;
                if (hi_start == -1) hi_start = j;
            }
        }
        
        if (!has_hi || hi_start == -1) continue;
        
        // 檢查是否有停頓 (低能量段) - 放寬要求
        bool has_pause = false;
        int pause_end = -1;
        for (int j = hi_start + 1; j < hi_start + 5 && j < num_chunks; j++) {
            if (energies[j] < dynamic_threshold * 0.6) {  // 60% 的動態閾值（更寬鬆）
                has_pause = true;
                pause_end = j;
                break;
            }
        }
        
        // 如果沒有明顯停頓，也接受（連續說也可以）
        if (!has_pause) {
            pause_end = hi_start + 1;
        }
        
        // 檢查是否有 "ESP" (高能量段)
        bool has_esp = false;
        for (int j = pause_end + 1; j < pause_end + 4 && j < num_chunks; j++) {
            if (energies[j] > dynamic_threshold) {
                has_esp = true;
                break;
            }
        }
        
        if (has_esp || !has_pause) {  // 有第二個高能量段，或者是連續語音
            ESP_LOGI(TAG, "🎯 檢測到 'Hi ESP' 模式！(停頓: %s)", has_pause ? "是" : "否");
            pattern_found = true;
            break;
        }
    }
    
    free(energies);
    return pattern_found;
}

// 1. 輕度高通濾波器（只去除極低頻雜訊）
static void apply_highpass_filter(int16_t *audio_data, size_t length) {
    const float alpha = 0.99;  // 截止頻率約 20 Hz（更溫和）
    float prev_output = 0;
    int16_t prev_input = 0;
    
    for (size_t i = 0; i < length; i++) {
        float filtered = alpha * (prev_output + audio_data[i] - prev_input);
        prev_output = filtered;
        prev_input = audio_data[i];
        audio_data[i] = (int16_t)filtered;
    }
}

// 2. 溫和的噪音門限（只去除極低電平噪音）
static void apply_noise_gate(int16_t *audio_data, size_t length) {
    const int16_t threshold = 50;  // 降低門限（更溫和）
    int suppressed = 0;
    
    for (size_t i = 0; i < length; i++) {
        if (abs(audio_data[i]) < threshold) {
            audio_data[i] = 0;
            suppressed++;
        }
    }
    
    ESP_LOGI(TAG, "   噪音抑制: %d/%zu 樣本 (%.1f%%)", 
             suppressed, length, (float)suppressed * 100 / length);
}

// 3. 輕度降噪處理（保持音質）
static void apply_noise_reduction(int16_t *audio_data, size_t length) {
    ESP_LOGI(TAG, "🔧 輕度降噪:");
    
    // 只使用高通濾波（去除極低頻雜訊）
    apply_highpass_filter(audio_data, length);
    ESP_LOGI(TAG, "   ✓ 高通濾波（去除 < 20Hz）");
    
    // 溫和的噪音門限
    apply_noise_gate(audio_data, length);
    
    // 不使用移動平均，保持原始音質
}

// 5. 智能音量調整（自動增益控制）
static void apply_auto_gain(int16_t *audio_data, size_t length) {
    // 計算音訊的 RMS（均方根）能量
    float rms = 0.0f;
    for (size_t i = 0; i < length; i++) {
        rms += (float)(audio_data[i] * audio_data[i]);
    }
    rms = sqrtf(rms / length);
    
    // 目標 RMS（約 -12dB）
    const float target_rms = 8192.0f;  // 約 25% 的最大值
    
    // 計算增益
    float gain = 1.0f;
    if (rms > 100.0f) {  // 避免除以零
        gain = target_rms / rms;
        // 限制增益範圍（1x - 8x）
        if (gain < 1.0f) gain = 1.0f;
        if (gain > 8.0f) gain = 8.0f;
    } else {
        gain = 4.0f;  // 預設增益
    }
    
    ESP_LOGI(TAG, "🎚️  自動增益: %.2fx (RMS: %.0f → %.0f)", gain, rms, rms * gain);
    
    // 應用增益並防止削波
    for (size_t i = 0; i < length; i++) {
        int32_t amplified = (int32_t)(audio_data[i] * gain);
        if (amplified > 32767) amplified = 32767;
        if (amplified < -32768) amplified = -32768;
        audio_data[i] = (int16_t)amplified;
    }
}

// HTTP 事件處理器（用於下載 TTS）
static esp_err_t tts_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_ON_DATA:
            // 數據會在主函數中處理
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        default:
            break;
    }
    return ESP_OK;
}

// 下載 TTS 音檔到 PSRAM 並播放
static esp_err_t download_and_play_tts(const char* url) {
    ESP_LOGI(TAG, "📥 下載 TTS: %s", url);
    
    // 配置 HTTP 客戶端
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
    
    // 開始 HTTP GET 請求
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTP 連線失敗: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }
    
    // 獲取內容長度
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📊 HTTP 狀態: %d, 檔案大小: %d bytes (%.1f KB)", 
             status_code, content_length, (float)content_length / 1024);
    
    if (status_code != 200) {
        ESP_LOGE(TAG, "❌ HTTP 錯誤: %d", status_code);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    if (content_length <= 0 || content_length > 500000) {  // 限制 500KB
        ESP_LOGE(TAG, "❌ 檔案大小異常: %d bytes", content_length);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }
    
    // 分配 PSRAM 緩衝區存儲整個 WAV 文件
    ESP_LOGI(TAG, "💾 分配 PSRAM: %d bytes", content_length);
    uint8_t *wav_buffer = (uint8_t*)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM);
    if (wav_buffer == NULL) {
        ESP_LOGE(TAG, "❌ PSRAM 分配失敗");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    // 下載數據到 PSRAM
    int total_read = 0;
    int last_progress = -1;
    
    while (total_read < content_length) {
        int read_len = esp_http_client_read(client, 
                                            (char*)(wav_buffer + total_read), 
                                            content_length - total_read);
        if (read_len < 0) {
            ESP_LOGE(TAG, "❌ 讀取數據失敗");
            free(wav_buffer);
            esp_http_client_cleanup(client);
            return ESP_FAIL;
        }
        
        if (read_len == 0) {
            break;  // 下載完成
        }
        
        total_read += read_len;
        
        // 顯示進度
        int progress = (total_read * 100) / content_length;
        if (progress / 10 != last_progress / 10) {
            ESP_LOGI(TAG, "📥 下載進度: %d%% (%d/%d bytes)", 
                     progress, total_read, content_length);
            last_progress = progress;
        }
    }
    
    esp_http_client_cleanup(client);
    
    ESP_LOGI(TAG, "✅ 下載完成: %d bytes", total_read);
    
    // 播放 WAV 文件
    ESP_LOGI(TAG, "🔊 開始播放 TTS...");
    esp_err_t play_ret = audio_play_wav_buffer(wav_buffer, total_read);
    
    // 釋放 PSRAM
    free(wav_buffer);
    
    if (play_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ TTS 播放完成");
    } else {
        ESP_LOGE(TAG, "❌ TTS 播放失敗");
    }
    
    return play_ret;
}

// 錄音並上傳
static void record_and_upload(void) {
    ESP_LOGI(TAG, "🎙️  開始錄音 %d 秒（16kHz）...", RECORD_TIME_MS / 1000);
    
    // 分配音頻緩衝區（16kHz 3秒 = 96KB，記憶體友好）
    size_t buffer_size = TOTAL_SAMPLES * sizeof(int16_t);
    ESP_LOGI(TAG, "📊 需要分配 %zu KB 緩衝區...", buffer_size / 1024);
    ESP_LOGI(TAG, "💾 當前可用: %zu KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    
    int16_t *audio_data = (int16_t*)malloc(buffer_size);
    if (!audio_data) {
        ESP_LOGE(TAG, "❌ 無法分配音頻緩衝區（需要 %zu KB）", buffer_size / 1024);
        ESP_LOGE(TAG, "💡 可用 DRAM: %zu KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
        return;
    }
    ESP_LOGI(TAG, "✅ 緩衝區分配成功");
    
    int16_t *temp_buffer = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
    if (!temp_buffer) {
        free(audio_data);
        ESP_LOGE(TAG, "❌ 無法分配臨時緩衝區");
        return;
    }
    
    // 錄音
    size_t samples_recorded = 0;
    size_t bytes_read;
    
    while (samples_recorded < TOTAL_SAMPLES) {
        esp_err_t ret = i2s_read(I2S_NUM, temp_buffer, AUDIO_BUFFER_SIZE * sizeof(int16_t), 
                                 &bytes_read, pdMS_TO_TICKS(1000));
        
        if (ret == ESP_OK && bytes_read > 0) {
            size_t samples_read = bytes_read / sizeof(int16_t);
            size_t samples_to_copy = (samples_recorded + samples_read > TOTAL_SAMPLES) ?
                                     (TOTAL_SAMPLES - samples_recorded) : samples_read;
            
            // 直接複製（16-bit 到 16-bit）
            memcpy(audio_data + samples_recorded, temp_buffer, 
                   samples_to_copy * sizeof(int16_t));
            samples_recorded += samples_to_copy;
            
            // 顯示進度
            if (samples_recorded % (I2S_SAMPLE_RATE) == 0) {
                ESP_LOGI(TAG, "錄音進度: %d%% (%d 秒)", 
                        (samples_recorded * 100) / TOTAL_SAMPLES,
                        samples_recorded / I2S_SAMPLE_RATE);
            }
        }
    }
    
    free(temp_buffer);
    ESP_LOGI(TAG, "✅ 錄音完成: %zu 樣本 (%.1f 秒)", 
             samples_recorded, (float)samples_recorded / I2S_SAMPLE_RATE);
    
    // 計算原始音頻能量
    float energy = calculate_energy(audio_data, samples_recorded);
    ESP_LOGI(TAG, "原始音頻能量: %.0f", energy);
    
    // 應用輕度降噪處理（保持音質）
    apply_noise_reduction(audio_data, samples_recorded);
    
    // 應用智能音量調整
    apply_auto_gain(audio_data, samples_recorded);
    
    // 檢查 WiFi 連接
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "⚠️  WiFi 未連接，無法上傳");
        free(audio_data);
        return;
    }
    
    // 上傳到服務器並接收回應
    ESP_LOGI(TAG, "📤 上傳音頻到服務器...");
    
    // 分配緩衝區接收伺服器回應
    char *response = malloc(4096);
    if (!response) {
        ESP_LOGE(TAG, "❌ 無法分配響應緩衝區");
        free(audio_data);
        return;
    }
    
    esp_err_t ret = upload_audio_json(SERVER_URL, API_KEY, 
                                      audio_data, samples_recorded, 
                                      I2S_SAMPLE_RATE,
                                      response, 4096);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 音頻上傳成功");
        ESP_LOGI(TAG, "");
        
        // 簡單解析 JSON 回應（手動提取）
        // 尋找 "stt_text":"..."
        char *stt_start = strstr(response, "\"stt_text\":\"");
        if (stt_start) {
            stt_start += 12;  // 跳過 "stt_text":"
            char *stt_end = strchr(stt_start, '"');
            if (stt_end) {
                int stt_len = stt_end - stt_start;
                if (stt_len > 0 && stt_len < 512) {
                    char stt_text[512];
                    memcpy(stt_text, stt_start, stt_len);
                    stt_text[stt_len] = '\0';
                    ESP_LOGI(TAG, "🎤 你說：%s", stt_text);
                }
            }
        }
        
        // 尋找 "ai_reply":"..."
        char *ai_start = strstr(response, "\"ai_reply\":\"");
        if (ai_start) {
            ai_start += 12;  // 跳過 "ai_reply":"
            
            // 找到結束的引號（需要處理轉義）
            char *ai_end = ai_start;
            while (*ai_end && !(*ai_end == '"' && *(ai_end - 1) != '\\')) {
                ai_end++;
            }
            
            if (*ai_end == '"') {
                int ai_len = ai_end - ai_start;
                if (ai_len > 0 && ai_len < 2048) {
                    char ai_reply[2048];
                    char *src = ai_start;
                    char *dst = ai_reply;
                    
                    // 處理轉義字符
                    while (src < ai_end && (dst - ai_reply) < 2047) {
                        if (*src == '\\') {
                            src++;
                            if (src >= ai_end) break;
                            
                            if (*src == 'n') {
                                *dst++ = '\n';
                                src++;
                            } else if (*src == 'u' && (src + 4) < ai_end) {
                                // Unicode 轉義 \uXXXX - 直接跳過，保留原始 UTF-8
                                // 實際上 JSON 中的中文已經是 UTF-8，不需要解碼
                                src++;  // 跳過 'u'
                                // 跳過 4 個十六進制數字
                                src += 4;
                            } else {
                                *dst++ = *src++;
                            }
                        } else {
                            *dst++ = *src++;
                        }
                    }
                    *dst = '\0';
                    
                    ESP_LOGI(TAG, "🤖 AI 回覆：");
                    
                    // 分行顯示（避免單行過長）
                    char *line = ai_reply;
                    char *next_line;
                    while ((next_line = strchr(line, '\n')) != NULL) {
                        *next_line = '\0';
                        ESP_LOGI(TAG, "%s", line);
                        line = next_line + 1;
                    }
                    if (*line) {
                        ESP_LOGI(TAG, "%s", line);
                    }
                }
            }
        }
        
        ESP_LOGI(TAG, "");
        
        // 下載 TTS 音檔到 PSRAM 並播放
        ESP_LOGI(TAG, "📥 開始下載 TTS 音檔到 PSRAM...");
        
        const char* audio_url = "https://nonargentiferous-fattily-robbin.ngrok-free.dev/public/voice.wav";
        
        // 下載到 PSRAM 並播放
        esp_err_t play_ret = download_and_play_tts(audio_url);
        if (play_ret == ESP_OK) {
            ESP_LOGI(TAG, "✅ TTS 播放完成");
        } else {
            ESP_LOGW(TAG, "⚠️ TTS 下載或播放失敗");
        }
        
        // 可選：如果 SD 卡已掛載，也保存一份到 SD 卡
        if (sd_is_mounted()) {
            ESP_LOGI(TAG, "💾 同時保存到 SD 卡...");
            const char* save_filename = "voice.wav";
            esp_err_t download_ret = sd_download_wav(audio_url, save_filename);
            if (download_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ 已保存到 SD 卡: %s", save_filename);
            }
        }
        
    } else {
        ESP_LOGE(TAG, "❌ 音頻上傳失敗");
    }
    
    free(response);
    free(audio_data);
}

// "Hi ESP" 檢測任務
static void keyword_detection_task(void *pvParameters) {
    // 分配較大的緩衝區用於模式檢測 (1秒)
    const int pattern_buffer_size = I2S_SAMPLE_RATE * 1;  // 1秒
    
    int16_t *pattern_buffer = (int16_t*)malloc(pattern_buffer_size * sizeof(int16_t));
    int16_t *read_buffer = (int16_t*)malloc(KEYWORD_SAMPLES * sizeof(int16_t));
    
    if (!pattern_buffer || !read_buffer) {
        ESP_LOGE(TAG, "❌ 無法分配檢測緩衝區");
        if (pattern_buffer) free(pattern_buffer);
        if (read_buffer) free(read_buffer);
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "🎤 開始監聽 'Hi ESP'...");
    ESP_LOGI(TAG, "💡 請清楚地說 'Hi ESP' 來觸發錄音");
    
    size_t bytes_read;
    int cooldown = 0;           // 冷卻計數器
    int pattern_samples = 0;    // 模式緩衝區中的樣本數
    bool collecting = false;    // 是否正在收集語音
    
    while (1) {
        // 讀取音頻數據
        esp_err_t ret = i2s_read(I2S_NUM, read_buffer, 
                                 KEYWORD_SAMPLES * sizeof(int16_t), 
                                 &bytes_read, pdMS_TO_TICKS(100));
        
        if (ret == ESP_OK && bytes_read > 0) {
            int samples = bytes_read / sizeof(int16_t);
            
            // 冷卻期間不檢測
            if (cooldown > 0) {
                cooldown--;
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            
            // 檢測語音活動
            bool has_voice = detect_voice(read_buffer, samples);
            
            if (has_voice) {
                // 開始或繼續收集語音
                if (!collecting) {
                    ESP_LOGI(TAG, "🎯 檢測到語音，開始收集...");
                    collecting = true;
                    pattern_samples = 0;
                }
                
                // 將數據添加到模式緩衝區
                int samples_to_copy = (pattern_samples + samples > pattern_buffer_size) ?
                                     (pattern_buffer_size - pattern_samples) : samples;
                
                if (samples_to_copy > 0) {
                    memcpy(pattern_buffer + pattern_samples, read_buffer, 
                           samples_to_copy * sizeof(int16_t));
                    pattern_samples += samples_to_copy;
                }
                
            } else if (collecting) {
                // 語音結束，檢查是否為 "Hi ESP"
                float total_duration_ms = (float)pattern_samples * 1000 / I2S_SAMPLE_RATE;
                ESP_LOGI(TAG, "語音結束，檢查模式... (%d 樣本, %.0f ms)", pattern_samples, total_duration_ms);
                
                // 檢查語音持續時間是否合理
                if (total_duration_ms >= MIN_VOICE_DURATION_MS && total_duration_ms <= MAX_VOICE_DURATION_MS) {
                    if (detect_hi_esp_pattern(pattern_buffer, pattern_samples)) {
                        ESP_LOGI(TAG, "🔊 檢測到 'Hi ESP'！");
                        ESP_LOGI(TAG, "🎙️  開始錄音...");
                        
                        // 錄音並上傳
                        record_and_upload();
                        
                        // 設置冷卻時間（3秒）
                        cooldown = 30;
                        
                        ESP_LOGI(TAG, "🔄 繼續監聽...");
                    } else {
                        ESP_LOGI(TAG, "❌ 不是 'Hi ESP'，繼續監聽...");
                    }
                } else {
                    ESP_LOGI(TAG, "⏱️  語音時長不符 (%.0f ms)，繼續監聽...", total_duration_ms);
                }
                
                // 重置收集狀態
                collecting = false;
                pattern_samples = 0;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "🚀 Hi ESP 關鍵詞檢測系統（優化版）");
    ESP_LOGI(TAG, "========================================");
    
    // 顯示記憶體狀態
    ESP_LOGI(TAG, "💾 可用記憶體: %zu KB", heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024);
    
    // 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // 初始化 WiFi
    ESP_LOGI(TAG, "📡 初始化 WiFi...");
    ret = wifi_init_sta(WIFI_SSID, WIFI_PASSWORD);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ WiFi 初始化失敗");
        return;
    }
    
    // WiFi 連接成功後，立即獲取並發送位置信息
    ESP_LOGI(TAG, "📍 獲取位置信息...");
    location_info_t location;
    if (location_get_info(&location) == ESP_OK) {
        // 發送位置到服務器
        if (location_send_to_server(LOCATION_URL, API_KEY, &location) == ESP_OK) {
            ESP_LOGI(TAG, "✅ 位置信息已發送到服務器");
        } else {
            ESP_LOGW(TAG, "⚠️ 位置信息發送失敗");
        }
    } else {
        ESP_LOGW(TAG, "⚠️ 無法獲取位置信息");
    }
    
    // 初始化 INMP441
    ESP_LOGI(TAG, "🎤 初始化 INMP441 麥克風...");
    ret = init_inmp441();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ INMP441 初始化失敗");
        return;
    }
    
    // 初始化 SD 卡（新 GPIO 配置）
    ESP_LOGI(TAG, "💾 初始化 SD 卡...");
    ret = sd_card_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ SD 卡初始化失敗（將無法保存音檔）");
    } else {
        ESP_LOGI(TAG, "✅ SD 卡初始化成功");
        sd_list_files();
    }
    
    // 初始化音頻輸出
    ESP_LOGI(TAG, "🔊 初始化音頻輸出...");
    ret = audio_init();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "⚠️ 音頻輸出初始化失敗（將無法播放 TTS）");
        // 不返回，繼續運行（音頻輸出是可選的）
    } else {
        ESP_LOGI(TAG, "✅ 音頻輸出初始化成功");
    }
    
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
    ESP_LOGI(TAG, "🗣️  請清楚地說 'Hi ESP' 來觸發錄音");
    ESP_LOGI(TAG, "📤 檢測到關鍵詞後會錄音 %d 秒並上傳", RECORD_TIME_MS / 1000);
    ESP_LOGI(TAG, "🎵 音訊品質: %d kHz, 16-bit, 單聲道", I2S_SAMPLE_RATE / 1000);
    ESP_LOGI(TAG, "💡 能量閾值: %.0f", (float)ENERGY_THRESHOLD);
    ESP_LOGI(TAG, "💡 說話方式: 'Hi' (停頓) 'ESP'");
    ESP_LOGI(TAG, "💾 記憶體需求: 錄音緩衝 %zu KB", (TOTAL_SAMPLES * sizeof(int16_t)) / 1024);
    
    // 創建關鍵詞檢測任務
    xTaskCreate(keyword_detection_task, "keyword_detect", 12288, NULL, 5, NULL);
}
