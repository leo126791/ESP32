#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "hi_esp_audio.h"

static const char *TAG = "SPEAKER_TEST";

void app_main(void)
{
    ESP_LOGI(TAG, "🚀 喇叭測試啟動 (32-bit 模式)");
    
    // 初始化
    if (audio_init() != ESP_OK) {
        ESP_LOGE(TAG, "❌ 初始化失敗！請檢查接線");
        return;
    }
    ESP_LOGI(TAG, "✅ 初始化成功");
    
    // 播放音階
    struct Note { uint32_t f; uint32_t d; } melody[] = {
        {261, 500}, {293, 500}, {329, 500}, // Do Re Mi
        {349, 500}, {392, 800}              // Fa Sol
    };
    
    while (1) {
        for (int i = 0; i < 5; i++) {
            ESP_LOGI(TAG, "🎵 頻率: %ld Hz", melody[i].f);
            audio_play_beep(melody[i].f, melody[i].d);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
