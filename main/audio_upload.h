#ifndef AUDIO_UPLOAD_H
#define AUDIO_UPLOAD_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "location_service.h"

// WAV header生成
void create_wav_header(uint8_t* header, uint32_t data_size, uint32_t sample_rate);

// 上傳音頻到服務器（JSON格式，base64編碼）
// response_buffer: 用於接收伺服器回應的緩衝區（可選，傳 NULL 則不接收）
// response_size: 緩衝區大小
esp_err_t upload_audio_json(const char* url, 
                            const char* api_key,
                            const int16_t* audio_data, 
                            size_t audio_len,
                            uint32_t sample_rate,
                            char* response_buffer,
                            size_t response_size);

// 上傳音頻和位置資訊到服務器（位置資訊放在 HTTP Header "x-esp32-loc" 中）
// location: 位置資訊（可選，傳 NULL 則不包含位置）
esp_err_t upload_audio_with_location(const char* url, 
                                     const char* api_key,
                                     const int16_t* audio_data, 
                                     size_t audio_len,
                                     uint32_t sample_rate,
                                     const location_info_t* location,
                                     char* response_buffer,
                                     size_t response_size);

#endif // AUDIO_UPLOAD_H
