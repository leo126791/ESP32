#include "hi_esp_audio.h"
#include "driver/i2s.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>
#include <string.h>

static const char *TAG = "AUDIO";

esp_err_t audio_init(void)
{
    ESP_LOGI(TAG, "初始化 MAX98357A 音頻輸出...");
    
#ifdef USE_SD_PIN_CONTROL
    // 配置 SD 引腳（控制開關）
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << I2S_SPEAKER_SD_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(I2S_SPEAKER_SD_PIN, 1);  // 1=開啟放大器
    ESP_LOGI(TAG, "SD 引腳已配置 (GPIO %d): 放大器已開啟", I2S_SPEAKER_SD_PIN);
#else
    ESP_LOGI(TAG, "SD 引腳保持懸空（預設工作模式）");
#endif
    
    // 使用舊版 I2S API 配置
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = I2S_SPEAKER_SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_SPEAKER_BCK_PIN,
        .ws_io_num = I2S_SPEAKER_WS_PIN,
        .data_out_num = I2S_SPEAKER_DATA_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t ret = i2s_driver_install(I2S_SPEAKER_NUM, &i2s_config, 0, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 驅動安裝失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2s_set_pin(I2S_SPEAKER_NUM, &pin_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S 設置引腳失敗: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_zero_dma_buffer(I2S_SPEAKER_NUM);
    
    ESP_LOGI(TAG, "✅ MAX98357A 初始化成功");
    ESP_LOGI(TAG, "📌 引腳: BCLK=%d, LRC=%d, DIN=%d", 
             I2S_SPEAKER_BCK_PIN, I2S_SPEAKER_WS_PIN, I2S_SPEAKER_DATA_PIN);
#ifdef USE_SD_PIN_CONTROL
    ESP_LOGI(TAG, "📌 SD 引腳: GPIO %d (可控制開關)", I2S_SPEAKER_SD_PIN);
#else
    ESP_LOGI(TAG, "📌 SD 引腳: 懸空（預設工作模式）");
#endif
    
    return ESP_OK;
}

esp_err_t audio_play(const int16_t *data, size_t length)
{
    size_t bytes_written = 0;
    esp_err_t ret = i2s_write(I2S_SPEAKER_NUM, data, length * sizeof(int16_t), 
                              &bytes_written, portMAX_DELAY);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "寫入音頻數據失敗: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGD(TAG, "播放 %d 字節音頻數據", bytes_written);
    return ESP_OK;
}

esp_err_t audio_play_beep(uint32_t frequency, uint32_t duration_ms)
{
    // 計算樣本數
    uint32_t num_samples = (I2S_SPEAKER_SAMPLE_RATE * duration_ms) / 1000;
    int16_t *beep_buffer = (int16_t *)malloc(num_samples * sizeof(int16_t));
    
    if (beep_buffer == NULL) {
        ESP_LOGE(TAG, "分配提示音緩衝區失敗");
        return ESP_ERR_NO_MEM;
    }
    
    // 生成正弦波
    const float amplitude = 8000.0f;  // 音量（最大 32767）
    for (uint32_t i = 0; i < num_samples; i++) {
        float t = (float)i / I2S_SPEAKER_SAMPLE_RATE;
        beep_buffer[i] = (int16_t)(amplitude * sinf(2.0f * M_PI * frequency * t));
    }
    
    // 播放
    esp_err_t ret = audio_play(beep_buffer, num_samples);
    
    free(beep_buffer);
    return ret;
}

esp_err_t audio_play_wav_file(const char *file_path)
{
    ESP_LOGI(TAG, "播放 WAV 文件: %s", file_path);
    
    FILE *fp = fopen(file_path, "rb");
    if (fp == NULL) {
        ESP_LOGE(TAG, "無法打開文件: %s", file_path);
        return ESP_ERR_NOT_FOUND;
    }
    
    // 讀取 WAV 文件頭（44 字節）
    uint8_t wav_header[44];
    if (fread(wav_header, 1, 44, fp) != 44) {
        ESP_LOGE(TAG, "讀取 WAV 文件頭失敗");
        fclose(fp);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 驗證 WAV 格式
    if (memcmp(wav_header, "RIFF", 4) != 0 || memcmp(wav_header + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "不是有效的 WAV 文件");
        fclose(fp);
        return ESP_ERR_INVALID_ARG;
    }
    
    // 讀取並播放音頻數據
    const size_t buffer_size = 1024;
    int16_t *audio_buffer = (int16_t *)malloc(buffer_size * sizeof(int16_t));
    if (audio_buffer == NULL) {
        ESP_LOGE(TAG, "分配音頻緩衝區失敗");
        fclose(fp);
        return ESP_ERR_NO_MEM;
    }
    
    size_t total_bytes = 0;
    size_t bytes_read;
    
    while ((bytes_read = fread(audio_buffer, 1, buffer_size * sizeof(int16_t), fp)) > 0) {
        size_t samples = bytes_read / sizeof(int16_t);
        esp_err_t ret = audio_play(audio_buffer, samples);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "播放音頻數據失敗");
            free(audio_buffer);
            fclose(fp);
            return ret;
        }
        total_bytes += bytes_read;
    }
    
    free(audio_buffer);
    fclose(fp);
    
    ESP_LOGI(TAG, "✅ 播放完成，共 %d 字節", total_bytes);
    return ESP_OK;
}

esp_err_t audio_play_wav_buffer(const uint8_t *wav_data, size_t wav_size)
{
    if (wav_size < 44) {
        ESP_LOGE(TAG, "WAV 數據太小");
        return ESP_ERR_INVALID_SIZE;
    }
    
    // 驗證 WAV 格式
    if (memcmp(wav_data, "RIFF", 4) != 0 || memcmp(wav_data + 8, "WAVE", 4) != 0) {
        ESP_LOGE(TAG, "不是有效的 WAV 數據");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 跳過 WAV 文件頭，播放音頻數據
    const int16_t *audio_data = (const int16_t *)(wav_data + 44);
    size_t audio_samples = (wav_size - 44) / sizeof(int16_t);
    
    ESP_LOGI(TAG, "播放 WAV 數據: %d 樣本", audio_samples);
    
    return audio_play(audio_data, audio_samples);
}

void audio_stop(void)
{
    i2s_driver_uninstall(I2S_SPEAKER_NUM);
    
#ifdef USE_SD_PIN_CONTROL
    // 關閉放大器以節省電力
    gpio_set_level(I2S_SPEAKER_SD_PIN, 0);
    ESP_LOGI(TAG, "音頻輸出已停止，放大器已關閉");
#else
    ESP_LOGI(TAG, "音頻輸出已停止");
#endif
}
