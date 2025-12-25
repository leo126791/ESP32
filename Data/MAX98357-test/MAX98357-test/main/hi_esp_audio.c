#include "hi_esp_audio.h"
#include <math.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "HI_ESP_AUDIO";

// 引腳配置
#define I2S_BCLK_IO      GPIO_NUM_4
#define I2S_WS_IO        GPIO_NUM_5
#define I2S_DOUT_IO      GPIO_NUM_6
#define I2S_SD_IO        GPIO_NUM_7

// 音頻參數 (使用 32-bit 解決雜訊問題)
#define SAMPLE_RATE      44100
#define PI               3.14159265358979323846

// 32-bit 音量 (最大 21億，設 5億約 25% 音量)
#define AMPLITUDE_32     500000000

static i2s_chan_handle_t tx_handle = NULL;
static bool audio_initialized = false;

esp_err_t audio_init(void) {
    if (audio_initialized) return ESP_OK;
    
    ESP_LOGI(TAG, "初始化 MAX98357A (32-bit 模式)...");
    
    // 1. 建立通道
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; // 自動清除 DMA 避免雜訊
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));
    
    // 2. 設定標準模式 (32-bit Philips)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        // 🔥 關鍵：使用 32-bit 位寬
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = I2S_BCLK_IO,
            .ws = I2S_WS_IO,
            .dout = I2S_DOUT_IO,
            .din = I2S_GPIO_UNUSED,
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    
    // 3. 啟用通道
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    
    // 4. 硬體開啟放大器 (SD Pin)
    if (I2S_SD_IO != GPIO_NUM_NC) {
        gpio_config_t sd_config = {
            .pin_bit_mask = (1ULL << I2S_SD_IO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        gpio_config(&sd_config);
        gpio_set_level(I2S_SD_IO, 1); // 拉高開啟
        ESP_LOGI(TAG, "放大器已啟用 (GPIO %d)", I2S_SD_IO);
    }
    
    audio_initialized = true;
    return ESP_OK;
}

esp_err_t audio_play_beep(uint32_t frequency, uint32_t duration_ms) {
    if (!audio_initialized) return ESP_ERR_INVALID_STATE;
    
    // 計算總樣本數
    uint32_t total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    uint32_t samples_generated = 0;
    
    // 🔥 分塊處理：每次只算 1024 個樣本 (節省記憶體)
    const size_t CHUNK_SIZE = 1024;
    // 32-bit 立體聲 = 8 bytes/sample
    int32_t *chunk_buffer = (int32_t*)malloc(CHUNK_SIZE * 2 * sizeof(int32_t));
    if (!chunk_buffer) {
        ESP_LOGE(TAG, "記憶體分配失敗");
        return ESP_ERR_NO_MEM;
    }
    
    size_t bytes_written;
    while (samples_generated < total_samples) {
        size_t current_chunk = total_samples - samples_generated;
        if (current_chunk > CHUNK_SIZE) current_chunk = CHUNK_SIZE;
        
        for (uint32_t i = 0; i < current_chunk; i++) {
            float t = (float)(samples_generated + i) / SAMPLE_RATE;
            // 使用 32-bit 幅度生成正弦波
            int32_t sample = (int32_t)(AMPLITUDE_32 * sin(2 * PI * frequency * t));
            chunk_buffer[i * 2]     = sample; // 左聲道
            chunk_buffer[i * 2 + 1] = sample; // 右聲道
        }
        
        i2s_channel_write(tx_handle, chunk_buffer, 
                         current_chunk * 2 * sizeof(int32_t),
                         &bytes_written, portMAX_DELAY);
        samples_generated += current_chunk;
    }
    
    free(chunk_buffer);
    return ESP_OK;
}

// 播放 16-bit WAV (自動轉 32-bit)
esp_err_t audio_play_wav_buffer(const uint8_t *wav_data, size_t wav_size) {
    if (!audio_initialized || wav_size < 44) return ESP_ERR_INVALID_STATE;
    
    const int16_t *src_data = (const int16_t *)(wav_data + 44); // 跳過檔頭
    size_t total_samples = (wav_size - 44) / sizeof(int16_t);
    size_t samples_processed = 0;
    
    const size_t CHUNK_SIZE = 512; // 分塊大小
    int32_t *chunk_buffer = (int32_t*)malloc(CHUNK_SIZE * 2 * sizeof(int32_t));
    if (!chunk_buffer) return ESP_ERR_NO_MEM;
    
    size_t bytes_written;
    while (samples_processed < total_samples) {
        size_t current_chunk = total_samples - samples_processed;
        if (current_chunk > CHUNK_SIZE) current_chunk = CHUNK_SIZE;
        
        for (size_t i = 0; i < current_chunk; i++) {
            // 🔥 關鍵：16-bit 左移 16 位變 32-bit
            int32_t sample32 = ((int32_t)src_data[samples_processed + i]) << 16;
            chunk_buffer[i * 2]     = sample32;
            chunk_buffer[i * 2 + 1] = sample32;
        }
        
        i2s_channel_write(tx_handle, chunk_buffer,
                         current_chunk * 2 * sizeof(int32_t),
                         &bytes_written, portMAX_DELAY);
        samples_processed += current_chunk;
    }
    
    free(chunk_buffer);
    return ESP_OK;
}

esp_err_t audio_stop(void) {
    if (tx_handle) {
        i2s_channel_disable(tx_handle);
        i2s_channel_enable(tx_handle); // 重置
    }
    return ESP_OK;
}

esp_err_t audio_deinit(void) {
    if (audio_initialized) {
        if (I2S_SD_IO != GPIO_NUM_NC) gpio_set_level(I2S_SD_IO, 0);
        i2s_channel_disable(tx_handle);
        i2s_del_channel(tx_handle);
        audio_initialized = false;
    }
    return ESP_OK;
}
