# Hi Lemon 語音助理

基於 ESP32-S3 的智能語音助理，使用 Edge Impulse 機器學習模型進行 "Hi Lemon" 喚醒詞檢測。

## 特色功能

- 🎤 **Edge Impulse 喚醒詞檢測**: 使用機器學習模型準確識別 "Hi Lemon"
- 🗣️ **語音識別**: 自動將語音轉換為文字
- 🤖 **AI 對話**: 整合 AI 服務進行智能回覆
- 🔊 **TTS 播放**: 自動下載並播放 AI 語音回覆
- 📍 **位置服務**: 自動獲取並上傳設備位置
- 💾 **SD 卡支援**: 可選的本地音頻存儲

## 硬體需求

### 主控板
- ESP32-S3 (8MB PSRAM)

### 麥克風
- INMP441 數位麥克風

### 音頻輸出
- MAX98357A I2S 音頻放大器

### 可選
- SD 卡模組（用於本地存儲）

## 接線圖

### INMP441 麥克風
```
INMP441    →    ESP32-S3
VDD        →    3.3V
GND        →    GND
L/R        →    GND (左聲道)
WS         →    GPIO 4
SCK        →    GPIO 5
SD         →    GPIO 6
```

### MAX98357A 音頻輸出
```
MAX98357A  →    ESP32-S3
VIN        →    5V
GND        →    GND
BCLK       →    GPIO 15
LRC        →    GPIO 7
DIN        →    GPIO 16
SD         →    GPIO 17 (可選，用於控制開關)
```

### SD 卡模組（可選）
```
SD Card    →    ESP32-S3
CS         →    GPIO 1
MOSI       →    GPIO 2
MISO       →    GPIO 8
CLK        →    GPIO 3
VCC        →    3.3V
GND        →    GND
```

## 快速開始

### 1. 環境設置

確保已安裝 ESP-IDF v5.5+：
```bash
# Windows
C:\Espressif\frameworks\esp-idf-v5.5.1\export.bat
```

### 2. 配置 WiFi

編輯 `main/hi_lemon_keyword.c`：
```c
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
```

### 3. 配置服務器

編輯 `main/hi_lemon_keyword.c`：
```c
#define SERVER_URL      "https://your-server.com/esp32/audio"
#define LOCATION_URL    "https://your-server.com/esp32/location"
#define API_KEY         "your_api_key"
```

### 4. 編譯與燒錄

```bash
# 編譯
build_lemon.bat

# 或手動編譯
idf.py build

# 燒錄（替換 COM3 為你的端口）
idf.py -p COM3 flash monitor
```

## 使用方法

1. 上電後，系統會自動連接 WiFi
2. 清楚地說 **"Hi Lemon"** 來喚醒設備
3. 聽到提示音後，說出你的問題（3 秒內）
4. 系統會自動上傳音頻並播放 AI 回覆

## 專案結構

```
├── main/
│   ├── hi_lemon_keyword.c      # 主程式（Edge Impulse 整合）
│   ├── ei_wrapper.cpp           # Edge Impulse C++ 包裝器
│   ├── hi_esp_audio.c           # 音頻輸出控制
│   ├── audio_upload_optimized.c # 音頻上傳
│   ├── wifi_manager.c           # WiFi 管理
│   ├── location_service.c       # 位置服務
│   └── sd_card_manager.c        # SD 卡管理
├── components/
│   └── lemong_wake/             # Edge Impulse 模型
│       ├── edge-impulse-sdk/    # Edge Impulse SDK
│       ├── model-parameters/    # 模型參數
│       └── tflite-model/        # TensorFlow Lite 模型
├── EDGE_IMPULSE_INTEGRATION.md  # Edge Impulse 整合指南
├── EDGE_IMPULSE_SETUP.md        # Edge Impulse 設置指南
├── GPIO_QUICK_REFERENCE.md      # GPIO 快速參考
├── SD_CARD_TROUBLESHOOTING.md   # SD 卡故障排除
└── TTS_PSRAM_PLAYBACK.md        # TTS PSRAM 播放說明
```

## Edge Impulse 模型

本專案使用 Edge Impulse 訓練的音頻分類模型：

- **輸入**: 16000 個樣本（1 秒，16kHz）
- **輸出**: 2 個分類
  - `hi lemon` - 喚醒詞
  - `noise` - 背景噪音
- **信心閾值**: 70%
- **模型大小**: ~38 KB

詳細資訊請參考 [EDGE_IMPULSE_INTEGRATION.md](EDGE_IMPULSE_INTEGRATION.md)

## 記憶體使用

- **Flash**: ~1 MB（包含模型）
- **SRAM**: ~200 KB
- **PSRAM**: 動態分配（TTS 緩衝區）
- **模型 Arena**: 38 KB

## 效能指標

- **喚醒詞檢測延遲**: ~100ms
- **錄音時長**: 3 秒
- **上傳速度**: ~150 KB/s
- **TTS 播放延遲**: ~2 秒

## 調整參數

### 檢測靈敏度

在 `main/hi_lemon_keyword.c` 中調整：

```c
#define ENERGY_THRESHOLD        100000  // 降低 = 更靈敏
#define DETECTION_CONFIDENCE    0.7     // 降低 = 更容易觸發
```

### 錄音時長

```c
#define RECORD_TIME_MS          3000    // 毫秒
```

## 故障排除

### 喚醒詞檢測不靈敏
1. 降低 `ENERGY_THRESHOLD`
2. 降低 `DETECTION_CONFIDENCE`
3. 檢查麥克風接線
4. 確保環境安靜

### 誤觸發
1. 提高 `ENERGY_THRESHOLD`
2. 提高 `DETECTION_CONFIDENCE`

### SD 卡無法讀取
參考 [SD_CARD_TROUBLESHOOTING.md](SD_CARD_TROUBLESHOOTING.md)

### 編譯錯誤
1. 確保 ESP-IDF 版本 >= v5.5
2. 確保 `esp-dsp` 組件已安裝
3. 清理並重新編譯：`idf.py fullclean && idf.py build`

## 開發指南

### 訓練自己的模型

1. 前往 [Edge Impulse Studio](https://studio.edgeimpulse.com/)
2. 收集 "Hi Lemon" 音頻樣本
3. 訓練模型並導出為 ESP32 格式
4. 替換 `components/lemong_wake/` 目錄

詳細步驟請參考 [EDGE_IMPULSE_SETUP.md](EDGE_IMPULSE_SETUP.md)

### 添加新功能

1. 在 `main/` 目錄創建新的 `.c` 或 `.cpp` 文件
2. 在 `main/CMakeLists.txt` 中添加源文件
3. 重新編譯

## 授權

本專案採用 MIT 授權。詳見 [LICENSE](LICENSE) 文件。

## 致謝

- [Edge Impulse](https://edgeimpulse.com/) - 機器學習平台
- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif IoT 開發框架
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) - 嵌入式機器學習

## 聯絡方式

如有問題或建議，歡迎提交 Issue 或 Pull Request。

---

**注意**: 本專案需要有效的 Edge Impulse 授權才能使用模型。請確保遵守 Edge Impulse 的使用條款。
