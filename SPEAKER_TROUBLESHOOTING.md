# MAX98357A 喇叭故障排除

## 問題描述

系統顯示 TTS 播放完成，但喇叭沒有聲音。

## 硬體檢查清單

### 1. MAX98357A 接線確認

根據你的配置：
```
MAX98357A → ESP32-S3
-----------------------
VIN  → 5V (或 3.3V)
GND  → GND
BCLK → GPIO 15
LRC  → GPIO 7
DIN  → GPIO 16
SD   → GPIO 17 (Shutdown，高電平啟用)
GAIN → 懸空或接地（15dB 增益）
```

**重要**：
- ✅ VIN 必須接 5V（如果用 3.3V 音量會很小）
- ✅ SD 引腳必須接 GPIO 17（程式會自動拉高啟用）
- ✅ GAIN 引腳可以懸空（預設 15dB）或接地（9dB）

### 2. 喇叭連接

- ✅ 確認喇叭正負極接對
- ✅ 確認喇叭阻抗為 4Ω 或 8Ω
- ✅ 確認喇叭功率至少 3W

### 3. 電源供應

MAX98357A 需要足夠的電流：
- ✅ 如果用 USB 供電，確認 USB 線材質量好
- ✅ 如果音量大時重啟，可能是電流不足
- ✅ 建議使用 5V 2A 以上的電源

## 軟體檢查

### 測試 1: 播放測試音

在主程式啟動後，加入測試音：

```c
// 在 app_main() 的音頻初始化後加入
ESP_LOGI(TAG, "🔊 播放測試音...");
audio_play_beep(1000, 500);  // 1kHz, 500ms
vTaskDelay(pdMS_TO_TICKS(1000));
```

### 測試 2: 檢查 SD 引腳狀態

```c
// 檢查 SD 引腳是否正確拉高
#include "driver/gpio.h"

int sd_level = gpio_get_level(GPIO_NUM_17);
ESP_LOGI(TAG, "SD 引腳狀態: %d (應該是 1)", sd_level);
```

### 測試 3: 增加音量

修改 `hi_esp_audio.c` 中的音量：

```c
// 在 audio_play_wav_buffer 中，播放前放大音量
for (size_t i = 0; i < num_samples; i++) {
    int32_t amplified = (int32_t)audio_data[i] * 4;  // 放大 4 倍
    if (amplified > 32767) amplified = 32767;
    if (amplified < -32768) amplified = -32768;
    audio_data[i] = (int16_t)amplified;
}
```

## 常見問題

### Q1: 日誌顯示播放完成，但沒聲音

**可能原因**：
1. SD 引腳沒有拉高（放大器關閉）
2. 喇叭接線錯誤
3. 電源電壓不足
4. 音量太小

**解決方法**：
```c
// 在播放前手動拉高 SD 引腳
gpio_set_level(GPIO_NUM_17, 1);
vTaskDelay(pdMS_TO_TICKS(10));  // 等待放大器啟動
```

### Q2: 有雜音但沒有清晰聲音

**可能原因**：
1. I2S 時鐘配置錯誤
2. 採樣率不匹配
3. 數據格式錯誤

**解決方法**：
檢查 WAV 文件格式：
- 採樣率：16000 Hz
- 位深度：16-bit
- 聲道：單聲道或立體聲

### Q3: 音量太小

**解決方法**：
1. 確認 VIN 接 5V（不是 3.3V）
2. GAIN 引腳懸空（15dB 增益）
3. 軟體放大音量（見測試 3）

## 快速測試程式

創建一個簡單的測試程式：

```c
void test_speaker(void) {
    ESP_LOGI("TEST", "🔊 開始喇叭測試...");
    
    // 測試 1: SD 引腳
    gpio_set_level(GPIO_NUM_17, 1);
    ESP_LOGI("TEST", "✅ SD 引腳已拉高");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 測試 2: 播放 1kHz 測試音 1 秒
    ESP_LOGI("TEST", "🎵 播放 1kHz 測試音...");
    audio_play_beep(1000, 1000);
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // 測試 3: 播放 500Hz 測試音 1 秒
    ESP_LOGI("TEST", "🎵 播放 500Hz 測試音...");
    audio_play_beep(500, 1000);
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    ESP_LOGI("TEST", "✅ 測試完成");
}
```

在 `app_main()` 中調用：
```c
// 在音頻初始化後
audio_init();
test_speaker();  // 加入這行
```

## 硬體測試

### 用萬用表測試

1. **測試 SD 引腳**：
   - 測量 GPIO 17 電壓
   - 應該是 3.3V（高電平）

2. **測試電源**：
   - 測量 MAX98357A 的 VIN 引腳
   - 應該是 5V

3. **測試喇叭**：
   - 測量喇叭阻抗
   - 應該是 4Ω 或 8Ω

### 用示波器測試（如果有）

1. **測試 BCLK**：應該看到時鐘信號
2. **測試 LRC**：應該看到左右聲道切換信號
3. **測試 DIN**：應該看到音頻數據

## 建議的修正步驟

1. **先測試硬體**：
   - 確認接線正確
   - 確認電源充足
   - 用萬用表測試 SD 引腳

2. **再測試軟體**：
   - 播放測試音
   - 檢查日誌
   - 增加音量

3. **如果還是沒聲音**：
   - 嘗試更換喇叭
   - 嘗試更換 MAX98357A 模組
   - 檢查 ESP32-S3 的 GPIO 是否損壞

## 參考資料

- [MAX98357A 數據手冊](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX98357A-MAX98357B.pdf)
- [ESP32-S3 I2S 文檔](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
