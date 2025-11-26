# Edge Impulse "Hi Lemon" 整合指南

## 概述

本專案已整合 Edge Impulse 機器學習模型來檢測 "Hi Lemon" 喚醒詞。

## 架構

### 1. Edge Impulse 模型
- **位置**: `components/lemong_wake/`
- **模型類型**: 音頻分類（Audio Classification）
- **輸入**: 16000 個樣本（1 秒，16kHz）
- **輸出**: 2 個分類
  - `hi lemon` (索引 0)
  - `noise` (索引 1)
- **信心閾值**: 70%

### 2. 程式架構

```
main/
├── hi_lemon_keyword.c    # 主程式（使用 Edge Impulse）
├── ei_wrapper.cpp         # Edge Impulse C++ 包裝器
├── ei_wrapper.h           # 包裝器標頭檔
├── hi_esp_audio.c         # 音頻輸出（MAX98357A）
├── audio_upload_optimized.c  # 音頻上傳
├── wifi_manager.c         # WiFi 管理
├── location_service.c     # 位置服務
└── sd_card_manager.c      # SD 卡管理

components/
└── lemong_wake/           # Edge Impulse 模型組件
    ├── edge-impulse-sdk/  # Edge Impulse SDK
    ├── model-parameters/  # 模型參數
    ├── tflite-model/      # TensorFlow Lite 模型
    └── CMakeLists.txt     # 組件編譯配置
```

### 3. 檢測流程

1. **連續監聽**: 使用滑動窗口（1 秒窗口，0.5 秒滑動）
2. **能量檢測**: 先檢查音頻能量，避免處理靜音
3. **Edge Impulse 推理**: 對有效音頻執行模型推理
4. **結果判斷**: 如果檢測到 "hi lemon" 且信心 > 70%
5. **錄音上傳**: 錄製 3 秒音頻並上傳到服務器
6. **TTS 播放**: 下載並播放 AI 回覆

## 編譯與燒錄

### 方法 1: 使用批次檔
```bash
build_lemon.bat
```

### 方法 2: 手動編譯
```bash
# 設定環境
call C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat

# 編譯
idf.py build

# 燒錄
idf.py -p COM3 flash monitor
```

## 配置

### WiFi 設定
在 `main/hi_lemon_keyword.c` 中修改：
```c
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
```

### 服務器設定
```c
#define SERVER_URL      "https://your-server.com/esp32/audio"
#define LOCATION_URL    "https://your-server.com/esp32/location"
#define API_KEY         "your_api_key"
```

### 檢測參數調整
```c
#define ENERGY_THRESHOLD        100000  // 能量閾值
#define DETECTION_CONFIDENCE    0.7     // 檢測信心閾值（70%）
```

## 硬體接線

### INMP441 麥克風
- VDD → 3.3V
- GND → GND
- L/R → GND
- WS  → GPIO 4
- SCK → GPIO 5
- SD  → GPIO 6

### MAX98357A 音頻輸出
- VIN → 5V
- GND → GND
- BCLK → GPIO 15
- LRC  → GPIO 7
- DIN  → GPIO 16
- SD   → GPIO 17 (可選，用於控制開關)

## 記憶體使用

- **Edge Impulse 模型**: ~38 KB (TensorFlow Lite Arena)
- **檢測緩衝區**: 32 KB (16000 樣本 × 2 bytes)
- **錄音緩衝區**: 96 KB (48000 樣本 × 2 bytes)
- **TTS 緩衝區**: 動態分配於 PSRAM

## 效能

- **檢測延遲**: ~100ms（包含 DSP 和推理）
- **CPU 使用率**: ~30%（檢測時）
- **記憶體**: ~200 KB 可用（ESP32-S3 8MB PSRAM）

## 除錯

### 查看檢測日誌
```
I (xxxxx) HI_LEMON: 📊 檢測語音能量: 150000
I (xxxxx) HI_LEMON: 🎯 檢測到: hi lemon
I (xxxxx) HI_LEMON: 🔊 檢測到 'Hi Lemon'！
```

### 常見問題

1. **檢測不靈敏**
   - 降低 `ENERGY_THRESHOLD`
   - 降低 `DETECTION_CONFIDENCE`
   - 檢查麥克風接線

2. **誤觸發**
   - 提高 `ENERGY_THRESHOLD`
   - 提高 `DETECTION_CONFIDENCE`

3. **編譯錯誤**
   - 確保 `esp-dsp` 組件已安裝
   - 檢查 ESP-IDF 版本（需要 v5.5+）

## 與舊版本的差異

### 舊版本 (hi_esp_keyword.c)
- 使用能量檢測和模式匹配
- 檢測 "Hi ESP"
- 準確度較低，容易誤觸發

### 新版本 (hi_lemon_keyword.c)
- 使用 Edge Impulse 機器學習模型
- 檢測 "Hi Lemon"
- 準確度高，誤觸發率低
- 需要更多記憶體和 CPU

## 下一步

1. **訓練自己的模型**: 在 Edge Impulse Studio 收集更多 "Hi Lemon" 樣本
2. **優化模型**: 減少模型大小和推理時間
3. **多語言支援**: 訓練支援多種語言的喚醒詞
4. **離線 TTS**: 整合本地 TTS 引擎

## 參考資料

- [Edge Impulse 文檔](https://docs.edgeimpulse.com/)
- [ESP-IDF 文檔](https://docs.espressif.com/projects/esp-idf/)
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers)
