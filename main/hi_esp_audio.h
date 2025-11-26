#ifndef HI_ESP_AUDIO_H
#define HI_ESP_AUDIO_H

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// MAX98357A I2S 引腳配置（方案 A：標準配置）
#define I2S_SPEAKER_NUM         I2S_NUM_1
#define I2S_SPEAKER_BCK_PIN     GPIO_NUM_15  // BCLK (原 16)
#define I2S_SPEAKER_WS_PIN      GPIO_NUM_7   // LRC (原 17)
#define I2S_SPEAKER_DATA_PIN    GPIO_NUM_16  // DIN (原 18)
#define I2S_SPEAKER_SD_PIN      GPIO_NUM_17  // SD (原 19)
#define I2S_SPEAKER_SAMPLE_RATE 16000

// SD 引腳控制選項
// 如果不想用 GPIO 控制，註釋掉下面這行，SD 引腳保持懸空
#define USE_SD_PIN_CONTROL

/**
 * @brief 初始化 MAX98357A 音頻輸出
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t audio_init(void);

/**
 * @brief 播放音頻數據
 * @param data 音頻數據指針
 * @param length 數據長度（字節）
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t audio_play(const int16_t *data, size_t length);

/**
 * @brief 播放簡單的提示音（嗶聲）
 * @param frequency 頻率 (Hz)
 * @param duration_ms 持續時間 (毫秒)
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t audio_play_beep(uint32_t frequency, uint32_t duration_ms);

/**
 * @brief 播放 WAV 文件
 * @param file_path WAV 文件路徑（SD 卡或 SPIFFS）
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t audio_play_wav_file(const char *file_path);

/**
 * @brief 播放內存中的 WAV 數據
 * @param wav_data WAV 數據指針
 * @param wav_size WAV 數據大小
 * @return ESP_OK 成功，其他值失敗
 */
esp_err_t audio_play_wav_buffer(const uint8_t *wav_data, size_t wav_size);

/**
 * @brief 停止音頻播放
 */
void audio_stop(void);

#ifdef __cplusplus
}
#endif

#endif // HI_ESP_AUDIO_H
