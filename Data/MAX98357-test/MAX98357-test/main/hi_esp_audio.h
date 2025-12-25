#ifndef HI_ESP_AUDIO_H
#define HI_ESP_AUDIO_H

#include "esp_err.h"
#include <stdint.h>

// 初始化音頻
esp_err_t audio_init(void);

// 播放嗶聲 (頻率 Hz, 持續 ms)
esp_err_t audio_play_beep(uint32_t frequency, uint32_t duration_ms);

// 播放 WAV 緩衝區 (會自動轉為 32-bit 播放)
esp_err_t audio_play_wav_buffer(const uint8_t *wav_data, size_t wav_size);

// 停止與卸載
esp_err_t audio_stop(void);
esp_err_t audio_deinit(void);

#endif // HI_ESP_AUDIO_H
