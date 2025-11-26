#ifndef SD_CARD_MANAGER_H
#define SD_CARD_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// 初始化 SD 卡
esp_err_t sd_card_init(void);

// 卸載 SD 卡
void sd_card_deinit(void);

// 保存音頻到 SD 卡（WAV 格式）
esp_err_t sd_save_audio_wav(const char* filename, 
                            const int16_t* audio_data, 
                            size_t audio_len,
                            uint32_t sample_rate);

// 檢查 SD 卡是否已掛載
bool sd_is_mounted(void);

// 列出 SD 卡上的文件
void sd_list_files(void);

// 刪除文件
esp_err_t sd_delete_file(const char* filename);

// 從 URL 下載 WAV 檔案到 SD 卡
esp_err_t sd_download_wav(const char* url, const char* filename);

// 格式化 SD 卡為 FAT32
esp_err_t sd_format_card(void);

#endif // SD_CARD_MANAGER_H
