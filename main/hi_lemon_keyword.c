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
#include "esp_heap_caps.h"
#include "hi_esp_audio.h"
#include "location_service.h"
#include "esp_http_client.h"
#include "ei_wrapper.h"

static const char *TAG = "HI_LEMON";

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

// LED 指示燈配置
#define LED_GPIO                GPIO_NUM_11  // 外接 LED（錄音指示燈）



// 音頻配置
#define AUDIO_BUFFER_SIZE       1024
// 注意：已改用動態增益（AGC），不再使用固定增益

// 自動錄音配置
#define MIN_RECORD_TIME_MS      1000    // 最短錄音時間（1 秒）
#define MAX_RECORD_TIME_MS      10000   // 最長錄音時間（10 秒）
#define SILENCE_TIMEOUT_MS      1500    // 靜音超時（1.5 秒後停止錄音）
#define VOICE_ENERGY_THRESHOLD  100000   // 🔥 語音能量閾值降低（原 200000 → 10000）
#define SILENCE_ENERGY_THRESHOLD 30000   // 🔥 靜音能量閾值降低（原 100000 → 3000）

// Edge Impulse 檢測配置
#define EI_WINDOW_SIZE          16000   // 1 秒窗口（Edge Impulse 模型需求）
#define EI_SLIDE_SIZE           4000    // 滑動 0.25 秒（更頻繁的檢測）
#define ENERGY_THRESHOLD        5000    // 🔥 能量閾值大幅降低（原 100000 → 5000）
#define DETECTION_CONFIDENCE    0.6     // 檢測信心閾值（60% - 降低以提高觸發率）

// 初始化 LED 指示燈
static esp_err_t init_led(void) {
    ESP_LOGI(TAG, "💡 初始化 LED 指示燈 (GPIO %d)...", LED_GPIO);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ LED 初始化失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始狀態：關閉
    gpio_set_level(LED_GPIO, 0);
    ESP_LOGI(TAG, "✅ LED 初始化成功");
    return ESP_OK;
}

// LED 控制函數
static void led_on(void) {
    gpio_set_level(LED_GPIO, 1);
}

static void led_off(void) {
    gpio_set_level(LED_GPIO, 0);
}

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
        // 🔥 關鍵優化：加大緩衝區到 2 秒
        // AI 推理實測需要 1.1 秒，所以緩衝區必須 > 1.1 秒
        // 32 個緩衝區 × 1024 樣本 = 32768 樣本 ≈ 2.0 秒緩衝
        // 這樣即使 AI 推理 1.1s，還有 0.9s 的安全餘裕
        .dma_buf_count = 32,
        .dma_buf_len = 1024,
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

// 檢測是否有語音活動
static bool is_voice_active(int16_t *buffer, size_t length) {
    int64_t energy = calculate_energy(buffer, length);
    return energy > VOICE_ENERGY_THRESHOLD;
}

// 檢測是否為靜音
__attribute__((unused)) static bool is_silence(int16_t *buffer, size_t length) {
    int64_t energy = calculate_energy(buffer, length);
    return energy < SILENCE_ENERGY_THRESHOLD;
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

// 動態增益控制（AGC - Automatic Gain Control）
static float calculate_dynamic_gain(int16_t *audio_data, size_t length) {
    // 計算平均振幅
    int32_t sum = 0;
    for (size_t i = 0; i < length; i++) {
        sum += abs(audio_data[i]);
    }
    float avg_amplitude = (float)sum / length;
    
    // 根據音量動態調整增益
    float dynamic_gain;
    if (avg_amplitude < 100) {
        dynamic_gain = 50.0f;  // 極小聲時，放大 50 倍
    } else if (avg_amplitude < 500) {
        dynamic_gain = 20.0f;  // 普通小聲，放大 20 倍
    } else if (avg_amplitude < 1000) {
        dynamic_gain = 10.0f;  // 中等音量，放大 10 倍
    } else {
        dynamic_gain = 5.0f;   // 已經夠大聲了，只放大 5 倍（避免破音）
    }
    
    return dynamic_gain;
}

// 自動增益控制（保留供未來使用）
__attribute__((unused)) static void apply_auto_gain(int16_t *audio_data, size_t length) {
    // 計算原始 RMS
    float rms = 0.0f;
    for (size_t i = 0; i < length; i++) {
        rms += (float)(audio_data[i] * audio_data[i]);
    }
    rms = sqrtf(rms / length);
    
    // 使用動態增益
    float gain = calculate_dynamic_gain(audio_data, length);
    
    ESP_LOGI(TAG, "🎚️  音頻增益: %.1fx (RMS: %.0f → %.0f)", gain, rms, rms * gain);
    
    // 應用增益並防止削波
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
    
    // ESP32-S3 有 8MB PSRAM，可以處理更大的 TTS 檔案
    if (status_code != 200 || content_length <= 0 || content_length > 1000000) {
        ESP_LOGE(TAG, "❌ 下載失敗 (狀態: %d, 大小: %d bytes)", status_code, content_length);
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
    
    // 顯示 PSRAM 使用情況
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_used = psram_total - psram_free;
    ESP_LOGI(TAG, "💾 PSRAM 使用: %d KB / %d KB (剩餘: %d KB, %.1f%%)", 
             psram_used / 1024, psram_total / 1024, psram_free / 1024,
             (float)psram_free * 100 / psram_total);
    
    ESP_LOGI(TAG, "🔊 開始播放 TTS...");
    
    audio_play_wav_buffer(wav_buffer, content_length);
    
    heap_caps_free(wav_buffer);
    ESP_LOGI(TAG, "✅ TTS 播放完成");
    
    // 顯示釋放後的 PSRAM 使用情況
    psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    psram_used = psram_total - psram_free;
    ESP_LOGI(TAG, "💾 PSRAM 釋放後: %d KB / %d KB (剩餘: %d KB, %.1f%%)", 
             psram_used / 1024, psram_total / 1024, psram_free / 1024,
             (float)psram_free * 100 / psram_total);
    
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

// 自動錄音並上傳（根據語音活動動態調整時長）
static esp_err_t record_and_upload(void) {
    ESP_LOGI(TAG, "🎙️  開始自動錄音（最短 %d 秒，最長 %d 秒）...", 
             MIN_RECORD_TIME_MS / 1000, MAX_RECORD_TIME_MS / 1000);
    
    // 分配最大緩衝區
    size_t max_samples = (I2S_SAMPLE_RATE * MAX_RECORD_TIME_MS) / 1000;
    size_t buffer_size = max_samples * sizeof(int16_t);
    
    ESP_LOGI(TAG, "📊 分配緩衝區: %zu KB", buffer_size / 1024);
    ESP_LOGI(TAG, "💾 當前可用: %zu KB", heap_caps_get_free_size(MALLOC_CAP_8BIT) / 1024);
    
    int16_t *audio_buffer = (int16_t*)malloc(buffer_size);
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 記憶體分配失敗");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "✅ 緩衝區分配成功");
    
    // 使用動態分配避免堆疊溢出
    int32_t *temp_buffer_32 = (int32_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int32_t));
    int16_t *temp_buffer_16 = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
    
    if (temp_buffer_32 == NULL || temp_buffer_16 == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配臨時緩衝區");
        led_off();  // 確保 LED 關閉
        free(audio_buffer);
        if (temp_buffer_32) free(temp_buffer_32);
        if (temp_buffer_16) free(temp_buffer_16);
        return ESP_ERR_NO_MEM;
    }
    
    size_t bytes_read = 0;
    size_t total_samples = 0;
    
    // 語音活動檢測變數
    size_t min_samples = (I2S_SAMPLE_RATE * MIN_RECORD_TIME_MS) / 1000;
    size_t silence_samples = (I2S_SAMPLE_RATE * SILENCE_TIMEOUT_MS) / 1000;
    size_t silence_counter = 0;
    bool voice_detected = false;
    int last_progress = -1;
    
    // 🔴 開啟 LED 指示燈
    led_on();
    ESP_LOGI(TAG, "🎤 開始錄音，等待語音結束... (LED 已開啟)");
    
    while (total_samples < max_samples) {
        i2s_read(I2S_NUM, temp_buffer_32, AUDIO_BUFFER_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);
        
        // 轉換 32-bit 到 16-bit
        convert_32bit_to_16bit(temp_buffer_32, temp_buffer_16, samples_read);
        
        // 🔥 應用動態增益（AGC）- 根據音量自動調整
        float dynamic_gain = calculate_dynamic_gain(temp_buffer_16, samples_read);
        for (size_t i = 0; i < samples_read; i++) {
            int32_t amplified = (int32_t)(temp_buffer_16[i] * dynamic_gain);
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            temp_buffer_16[i] = (int16_t)amplified;
        }
        
        // 檢測語音活動
        int64_t current_energy = calculate_energy(temp_buffer_16, samples_read);
        bool has_voice = current_energy > VOICE_ENERGY_THRESHOLD;
        bool is_silent = current_energy < SILENCE_ENERGY_THRESHOLD;
        
        // 調試：顯示當前能量值（每秒一次）
        static int energy_log_counter = 0;
        if (++energy_log_counter % 16 == 0) {  // 16 * 1024 / 16000 ≈ 1 秒
            ESP_LOGI(TAG, "🔊 能量: %lld (語音閾值: %d, 靜音閾值: %d)", 
                     current_energy, VOICE_ENERGY_THRESHOLD, SILENCE_ENERGY_THRESHOLD);
        }
        
        // 🔥 修正：使用更智能的判斷
        // 如果能量 > 語音閾值，明確是說話
        // 如果能量 < 靜音閾值，明確是靜音
        // 中間地帶（3000-10000）：如果已經檢測到語音，就當作靜音的開始
        if (has_voice) {
            voice_detected = true;
            silence_counter = 0;
        } else if (is_silent) {
            // 明確的靜音
            if (voice_detected) {
                silence_counter += samples_read;
            }
        } else {
            // 中間地帶（可能是說話結束後的餘音或環境噪音）
            // 如果已經檢測到語音，就當作靜音累加
            if (voice_detected) {
                silence_counter += samples_read;
            }
        }
        
        // 複製數據到緩衝區
        size_t samples_to_copy = (total_samples + samples_read <= max_samples) ? 
                                 samples_read : (max_samples - total_samples);
        
        memcpy(audio_buffer + total_samples, temp_buffer_16, 
               samples_to_copy * sizeof(int16_t));
        total_samples += samples_to_copy;
        
        // 顯示進度（每 0.5 秒）
        float duration = (float)total_samples / I2S_SAMPLE_RATE;
        int current_progress = (int)(duration * 2);  // 每 0.5 秒一次
        if (current_progress != last_progress) {
            const char* status = has_voice ? "🗣️ 說話中" : (is_silent ? "🤫 靜音" : "⏸️  等待");
            float silence_duration = (float)silence_counter / I2S_SAMPLE_RATE;
            ESP_LOGI(TAG, "⏱️  錄音: %.1f 秒 | %s | 靜音: %.1f 秒", 
                     duration, status, silence_duration);
            last_progress = current_progress;
        }
        
        // 檢查是否應該停止錄音
        if (total_samples >= min_samples) {
            // 達到最短時間後，如果檢測到足夠長的靜音，停止錄音
            if (silence_counter >= silence_samples) {
                ESP_LOGI(TAG, "🛑 檢測到語音結束（靜音 %.1f 秒）", 
                         (float)silence_counter / I2S_SAMPLE_RATE);
                break;
            }
        }
    }
    
    // 如果達到最大時長
    if (total_samples >= max_samples) {
        ESP_LOGW(TAG, "⚠️  達到最大錄音時長 %d 秒", MAX_RECORD_TIME_MS / 1000);
    }
    
    // ⚫ 關閉 LED 指示燈
    led_off();
    ESP_LOGI(TAG, "✅ 錄音完成: %zu 樣本 (%.1f 秒) (LED 已關閉)", 
             total_samples, 
             (float)total_samples / I2S_SAMPLE_RATE);
    
    int64_t energy = calculate_energy(audio_buffer, total_samples);
    ESP_LOGI(TAG, "✅ 錄音音頻能量: %lld (已應用動態增益)", energy);
    
    // 注意：動態增益已在錄音時應用，這裡只做輕度降噪
    ESP_LOGI(TAG, "🔧 輕度降噪...");
    apply_noise_reduction(audio_buffer, total_samples);
    
    ESP_LOGI(TAG, "📤 上傳音頻到服務器...");
    
    // 分配響應緩衝區
    char* response_buffer = (char*)malloc(2048);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配響應緩衝區");
        led_off();  // 確保 LED 關閉
        free(audio_buffer);
        free(temp_buffer_32);
        free(temp_buffer_16);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t ret = upload_audio_json(SERVER_URL, API_KEY, audio_buffer, total_samples, 
                                      I2S_SAMPLE_RATE, response_buffer, 2048);
    
    // 釋放所有緩衝區
    free(audio_buffer);
    free(temp_buffer_32);
    free(temp_buffer_16);
    
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

// 主監聽循環（最終增強版 - 修復靈敏度問題）
static void listen_for_hi_lemon(void) {
    ESP_LOGI(TAG, "🎤 啟動高靈敏度監聽 (先濾波 -> 再AGC -> 移除切片過濾)...");
    ESP_LOGI(TAG, "💡 使用 Edge Impulse 模型進行檢測（24-bit 音質）");
    
    // 使用動態分配避免堆疊溢出
    int16_t *window_buffer = (int16_t*)malloc(EI_WINDOW_SIZE * sizeof(int16_t));
    if (window_buffer == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配檢測緩衝區");
        return;
    }
    
    // 32-bit 緩衝區用於接收 I2S 數據（使用動態分配）
    int32_t *temp_buffer_32 = (int32_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int32_t));
    int16_t *temp_buffer_16 = (int16_t*)malloc(AUDIO_BUFFER_SIZE * sizeof(int16_t));
    
    if (temp_buffer_32 == NULL || temp_buffer_16 == NULL) {
        ESP_LOGE(TAG, "❌ 無法分配音頻緩衝區");
        free(window_buffer);
        if (temp_buffer_32) free(temp_buffer_32);
        if (temp_buffer_16) free(temp_buffer_16);
        return;
    }
    
    ESP_LOGI(TAG, "✅ 緩衝區分配成功");
    ESP_LOGI(TAG, "   Window buffer: %d bytes", EI_WINDOW_SIZE * sizeof(int16_t));
    ESP_LOGI(TAG, "   Temp buffers: %d bytes each", AUDIO_BUFFER_SIZE * sizeof(int32_t));
    
    memset(window_buffer, 0, EI_WINDOW_SIZE * sizeof(int16_t));
    size_t window_pos = 0;
    
    while (1) {
        size_t bytes_read = 0;
        // 1. 讀取 I2S 數據
        i2s_read(I2S_NUM, temp_buffer_32, AUDIO_BUFFER_SIZE * sizeof(int32_t), &bytes_read, portMAX_DELAY);
        size_t samples_read = bytes_read / sizeof(int32_t);
        
        // 2. 轉換 32-bit 到 16-bit
        convert_32bit_to_16bit(temp_buffer_32, temp_buffer_16, samples_read);
        
        // 3. 🔥 關鍵修正：先執行降噪/濾波 (去除 DC 偏差)
        // 這樣後面的 AGC 才能正確判斷音量，不會因為底噪誤判而降低增益
        apply_noise_reduction(temp_buffer_16, samples_read);
        
        // 4. 計算動態增益 (現在訊號乾淨了，計算會更準)
        float dynamic_gain = calculate_dynamic_gain(temp_buffer_16, samples_read);
        
        // 5. 應用增益並防止削波
        for (size_t i = 0; i < samples_read; i++) {
            int32_t amplified = (int32_t)(temp_buffer_16[i] * dynamic_gain);
            if (amplified > 32767) amplified = 32767;
            if (amplified < -32768) amplified = -32768;
            temp_buffer_16[i] = (int16_t)amplified;
        }
        
        // 6. 填充滑動窗口
        for (size_t i = 0; i < samples_read; i++) {
            window_buffer[window_pos] = temp_buffer_16[i];
            window_pos++;
            
            // 當窗口填滿時，執行檢測
            if (window_pos >= EI_WINDOW_SIZE) {
                // 計算整段窗口的能量
                int64_t window_energy = calculate_energy(window_buffer, EI_WINDOW_SIZE);
                
                // 🔥 關鍵修正：移除 slice_energy 檢查，只檢查整體能量
                // 只要整段聲音夠大聲，就跑 AI，避免漏掉結尾
                if (window_energy > ENERGY_THRESHOLD) {
                    ESP_LOGI(TAG, "📊 檢測語音能量: %lld (增益: %.1fx)", window_energy, dynamic_gain);
                    
                    // 執行 Edge Impulse 推理
                    int label_idx = ei_wrapper_run_inference(window_buffer, EI_WINDOW_SIZE);
                    
                    if (label_idx >= 0) {
                        const char* label = ei_wrapper_get_label(label_idx);
                        ESP_LOGI(TAG, "🎯 檢測到: %s", label);
                        
                        // 檢查是否為 "hi lemon" (索引 0)
                        if (label_idx == 0 || strstr(label, "hi lemon") != NULL) {
                            ESP_LOGI(TAG, "🔊 抓到了！Hi Lemon (能量: %lld, 增益: %.1fx)", 
                                     window_energy, dynamic_gain);
                            
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
                
                // 滑動窗口 (丟掉舊的，保留新的)
                memmove(window_buffer, window_buffer + EI_SLIDE_SIZE, 
                       (EI_WINDOW_SIZE - EI_SLIDE_SIZE) * sizeof(int16_t));
                window_pos = EI_WINDOW_SIZE - EI_SLIDE_SIZE;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    // 釋放所有緩衝區
    free(window_buffer);
    free(temp_buffer_32);
    free(temp_buffer_16);
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
    
    // 啟動 Wi-Fi 配網（自動檢測是否已配網）
    ESP_LOGI(TAG, "📡 啟動 Wi-Fi 配網...");
    wifi_init_with_provisioning();
    
    // 等待 Wi-Fi 連接成功
    ESP_LOGI(TAG, "⏳ 等待 Wi-Fi 連接...");
    int retry_count = 0;
    while (!wifi_is_connected() && retry_count < 60) {
        if (retry_count % 10 == 0) {
            ESP_LOGI(TAG, "   等待中... (%d/60 秒)", retry_count);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
        retry_count++;
    }
    
    if (!wifi_is_connected()) {
        ESP_LOGE(TAG, "❌ Wi-Fi 連接失敗");
        ESP_LOGI(TAG, "💡 提示：如需重新配網，請調用 wifi_reset_provisioning() 並重啟");
        return;
    }
    
    ESP_LOGI(TAG, "✅ Wi-Fi 已連接");
    
    // 等待 DNS 和網路穩定（避免競態條件）
    ESP_LOGI(TAG, "⏳ 等待網路穩定（DNS 初始化）...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 發送位置信息
    ESP_LOGI(TAG, "📍 發送位置信息...");
    location_info_t location;
    if (location_get_info(&location) == ESP_OK) {
        location_send_to_server(LOCATION_URL, API_KEY, &location);
    }
    
    // 初始化 LED 指示燈
    ESP_LOGI(TAG, "💡 初始化 LED 指示燈...");
    if (init_led() != ESP_OK) {
        ESP_LOGE(TAG, "❌ LED 初始化失敗");
        return;
    }
    
    // 初始化麥克風
    ESP_LOGI(TAG, "🎤 初始化 INMP441 麥克風...");
    if (init_inmp441() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 麥克風初始化失敗");
        return;
    }
    ESP_LOGI(TAG, "✅ INMP441 初始化成功（24-bit 原生模式）");
    
    // 初始化音頻輸出
    ESP_LOGI(TAG, "🔊 初始化音頻輸出...");
    if (audio_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 音頻輸出初始化失敗");
        return;
    }
    ESP_LOGI(TAG, "✅ 音頻輸出初始化成功");
    
    // 顯示記憶體資訊
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t sram_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t sram_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "💾 記憶體資訊:");
    ESP_LOGI(TAG, "   PSRAM: %d KB 總容量, %d KB 可用 (%.1f%%)", 
             psram_total / 1024, psram_free / 1024, 
             (float)psram_free * 100 / psram_total);
    ESP_LOGI(TAG, "   SRAM:  %d KB 總容量, %d KB 可用 (%.1f%%)", 
             sram_total / 1024, sram_free / 1024,
             (float)sram_free * 100 / sram_total);
    ESP_LOGI(TAG, "");
    
    // 播放測試音（診斷用）
    ESP_LOGI(TAG, "🎵 播放測試音...");
    audio_play_beep(1000, 500);  // 1kHz, 500ms
    vTaskDelay(pdMS_TO_TICKS(600));
    audio_play_beep(1500, 500);  // 1.5kHz, 500ms
    ESP_LOGI(TAG, "✅ 測試音播放完成");
    
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
    ESP_LOGI(TAG, "📤 檢測到關鍵詞後會自動錄音並上傳");
    ESP_LOGI(TAG, "⏱️  錄音時長: %d-%d 秒（自動檢測語音結束）", 
             MIN_RECORD_TIME_MS / 1000, MAX_RECORD_TIME_MS / 1000);
    ESP_LOGI(TAG, "🎵 音訊品質: %d kHz, 24-bit (INMP441 原生), 單聲道", I2S_SAMPLE_RATE / 1000);
    ESP_LOGI(TAG, "🤖 使用 Edge Impulse 模型檢測");
    ESP_LOGI(TAG, "💡 24-bit 模式提供更好的動態範圍和音質");
    ESP_LOGI(TAG, "🎙️  自動語音活動檢測（VAD）已啟用");
    ESP_LOGI(TAG, "");
    
    // 開始監聽
    listen_for_hi_lemon();
}
