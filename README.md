# Hi Lemon 語音助理

基於 ESP32-S3 的智能語音助理，使用 Edge Impulse 機器學習模型進行 "Hi Lemon" 喚醒詞檢測。

## ✨ 特色功能

- 🎤 **Edge Impulse 喚醒詞檢測**: 使用機器學習模型準確識別 "Hi Lemon"
- ⏱️ **自動錄音時長**: 智能檢測語音結束，自動停止錄音（1-10 秒）
- 🎵 **24-bit 音頻**: INMP441 原生 24-bit 模式，提供卓越音質
- �️ **語音識別**:: 自動將語音轉換為文字
- 🤖 **AI 對話**: 整合 AI 服務進行智能回覆
- � **TTS 播放***: 自動下載並播放 AI 語音回覆
- 📍 **位置服務**: 自動獲取並上傳設備位置

## 🎯 快速開始

### 1. 硬體需求

- **ESP32-S3** (8MB PSRAM, 16MB Flash)
- **INMP441** 數位麥克風
- **MAX98357A** I2S 音頻放大器
- **喇叭** (3-8Ω)

### 2. 接線圖

#### INMP441 麥克風
```
INMP441    →    ESP32-S3
VDD        →    3.3V
GND        →    GND
L/R        →    GND (左聲道)
WS         →    GPIO 4
SCK        →    GPIO 5
SD         →    GPIO 6
```

#### MAX98357A 音頻輸出
```
MAX98357A  →    ESP32-S3
VIN        →    5V (必須 5V，不可用 3.3V)
GND        →    GND
BCLK       →    GPIO 8
LRC        →    GPIO 9
DIN        →    GPIO 10
SD         →    GPIO 7 (開關控制)
GAIN       →    GND (12dB 最大音量)
```

**重要提醒**：
- MAX98357A 必須使用 **5V** 供電，不可用 3.3V
- GAIN 引腳接 GND 可獲得最大音量（12dB 增益）
- 喇叭阻抗建議使用 4Ω 或 8Ω
- 確保所有 GND 共地

### 3. 配置與編譯

#### 步驟 1: 配置 WiFi

編輯 `main/hi_lemon_keyword.c`：
```c
// WiFi 配置
#define WIFI_SSID       "your_wifi_ssid"
#define WIFI_PASSWORD   "your_wifi_password"
```

#### 步驟 2: 配置服務器

```c
#define SERVER_URL      "https://your-server.com/esp32/audio"
#define LOCATION_URL    "https://your-server.com/esp32/location"
#define API_KEY         "your_api_key"
```

#### 步驟 3: 配置分區表

使用 menuconfig 配置自定義分區表（詳見 [PARTITION_TABLE_SETUP.md](PARTITION_TABLE_SETUP.md)）：

```bash
idf.py menuconfig
# 導航到: Partition Table → Custom partition table CSV
# 設置文件名: partitions_16mb.csv
# 保存並退出
```

#### 步驟 4: 編譯與燒錄

```bash
# 使用批次檔編譯
build_lemon.bat

# 或手動編譯
idf.py build

# 燒錄（替換 COM6 為你的端口）
idf.py -p COM6 flash monitor
```

## 🎙️ 使用方法

1. 上電後，系統會自動連接 WiFi
2. 清楚地說 **"Hi Lemon"** 來喚醒設備
3. 聽到提示音後，說出你的問題
4. 系統會自動檢測語音結束（靜音 1.5 秒）
5. 音頻上傳並播放 AI 回覆

### 自動錄音特性

- **最短時長**: 1 秒
- **最長時長**: 10 秒
- **自動停止**: 檢測到 1.5 秒靜音後自動停止
- **實時反饋**: 顯示錄音狀態（🗣️ 說話 / 🤫 靜音）

## 📁 專案結構

```
├── main/
│   ├── hi_lemon_keyword.c       # 主程式（Edge Impulse 整合）
│   ├── ei_wrapper.cpp            # Edge Impulse C++ 包裝器
│   ├── hi_esp_audio.c            # 音頻輸出控制
│   ├── audio_upload_optimized.c  # 音頻上傳
│   ├── wifi_manager.c            # WiFi 管理
│   ├── location_service.c        # 位置服務
│   └── sd_card_manager.c         # SD 卡管理（可選）
├── components/
│   └── lemong_wake/              # Edge Impulse 模型
│       ├── edge-impulse-sdk/     # Edge Impulse SDK
│       ├── model-parameters/     # 模型參數
│       └── tflite-model/         # TensorFlow Lite 模型
├── EDGE_IMPULSE_INTEGRATION.md   # Edge Impulse 整合指南
├── EDGE_IMPULSE_SETUP.md         # Edge Impulse 設置指南
├── AUDIO_CONFIG_CHECK.md         # 音頻配置檢查報告
├── INMP441_24BIT_MODE.md         # 24-bit 模式說明
├── PARTITION_TABLE_SETUP.md      # 分區表配置指南
├── GPIO_QUICK_REFERENCE.md       # GPIO 快速參考
└── README.md                     # 本文件
```

## 🔧 技術規格

### Edge Impulse 模型

- **輸入**: 16,000 個樣本（1 秒，16 kHz）
- **輸出**: 2 個分類（"hi lemon", "noise"）
- **信心閾值**: 70%
- **模型大小**: ~38 KB
- **推理時間**: ~100ms

### 音頻配置

- **採樣率**: 16,000 Hz
- **位元深度**: 24-bit（INMP441 原生）→ 16-bit（模型輸入）
- **聲道**: 單聲道
- **檢測窗口**: 1 秒（滑動 0.5 秒）

### 記憶體使用

- **Flash**: ~1.3 MB（包含模型）
- **SRAM**: ~200 KB
- **PSRAM**: 動態分配（TTS 緩衝區）
- **檢測緩衝區**: 32 KB
- **錄音緩衝區**: 最大 320 KB（10 秒）

### 分區配置

- **Bootloader**: 21 KB
- **NVS**: 24 KB
- **PHY Init**: 4 KB
- **Factory App**: 4 MB
- **Storage**: 12 MB（FAT 文件系統）

## ⚙️ 參數調整

### 喚醒詞檢測

在 `main/hi_lemon_keyword.c` 中調整：

```c
#define ENERGY_THRESHOLD        100000  // 降低 = 更靈敏
#define DETECTION_CONFIDENCE    0.7     // 降低 = 更容易觸發
```

### 自動錄音

```c
#define MIN_RECORD_TIME_MS      1000    // 最短錄音時間
#define MAX_RECORD_TIME_MS      10000   // 最長錄音時間
#define SILENCE_TIMEOUT_MS      1500    // 靜音超時
#define VOICE_ENERGY_THRESHOLD  80000   // 語音能量閾值
#define SILENCE_ENERGY_THRESHOLD 30000  // 靜音能量閾值
```

## 📊 效能指標

- **喚醒詞檢測延遲**: ~100ms
- **錄音時長**: 1-10 秒（自動）
- **上傳速度**: ~150 KB/s
- **TTS 播放延遲**: ~2 秒
- **CPU 使用率**: ~30%（檢測時）
- **識別準確度**: ~92%

## 🐛 故障排除

### 喚醒詞檢測不靈敏

1. 降低 `ENERGY_THRESHOLD`
2. 降低 `DETECTION_CONFIDENCE`
3. 檢查麥克風接線
4. 確保環境安靜

### 誤觸發

1. 提高 `ENERGY_THRESHOLD`
2. 提高 `DETECTION_CONFIDENCE`
3. 重新訓練模型（收集更多負樣本）

### 錄音時間不正確

1. 調整 `SILENCE_TIMEOUT_MS`（靜音超時）
2. 調整 `VOICE_ENERGY_THRESHOLD`（語音閾值）
3. 調整 `SILENCE_ENERGY_THRESHOLD`（靜音閾值）

### 喇叭沒有聲音

**檢查硬體**：
1. 確認 MAX98357A 的 VIN 接到 **5V**（不是 3.3V）
2. 確認 GAIN 引腳接到 **GND**（獲得最大音量）
3. 檢查所有接線是否牢固
4. 確認喇叭阻抗為 4Ω 或 8Ω
5. 用其他設備測試喇叭是否正常

**檢查軟體**：
1. 查看 Log 是否顯示 "✅ 音頻輸出初始化成功"
2. 查看 Log 是否顯示 "🎵 播放測試音..."
3. 檢查是否有錯誤訊息

**接線確認**：
```
MAX98357A  →  ESP32-S3
VIN   → 5V ⚡ (不是 3.3V！)
GND   → GND
BCLK  → GPIO 8
LRC   → GPIO 9
DIN   → GPIO 10
SD    → GPIO 7
GAIN  → GND ⚡ (必須接地！)
```

詳細排查步驟請參考 [SPEAKER_TROUBLESHOOTING.md](SPEAKER_TROUBLESHOOTING.md)

### 編譯錯誤

**應用程式太大**:
- 使用 menuconfig 配置自定義分區表
- 參考 [PARTITION_TABLE_SETUP.md](PARTITION_TABLE_SETUP.md)

**ESP-DSP 錯誤**:
- 確保 ESP-IDF 版本 >= v5.5
- 檢查 `esp-dsp` 組件已安裝

## 📚 文檔指南

- **[EDGE_IMPULSE_INTEGRATION.md](EDGE_IMPULSE_INTEGRATION.md)**: Edge Impulse 整合詳細說明
- **[EDGE_IMPULSE_SETUP.md](EDGE_IMPULSE_SETUP.md)**: 如何訓練自己的模型
- **[AUDIO_CONFIG_CHECK.md](AUDIO_CONFIG_CHECK.md)**: 音頻配置檢查報告
- **[INMP441_24BIT_MODE.md](INMP441_24BIT_MODE.md)**: 24-bit 模式技術說明
- **[PARTITION_TABLE_SETUP.md](PARTITION_TABLE_SETUP.md)**: 分區表配置步驟
- **[GPIO_QUICK_REFERENCE.md](GPIO_QUICK_REFERENCE.md)**: GPIO 引腳參考
- **[SPEAKER_TROUBLESHOOTING.md](SPEAKER_TROUBLESHOOTING.md)**: 喇叭故障排除指南

## 🔄 開發指南

### 訓練自己的模型

1. 前往 [Edge Impulse Studio](https://studio.edgeimpulse.com/)
2. 收集 "Hi Lemon" 音頻樣本（建議 100+ 樣本）
3. 收集背景噪音樣本
4. 訓練模型並導出為 ESP32 格式
5. 替換 `components/lemong_wake/` 目錄

詳細步驟請參考 [EDGE_IMPULSE_SETUP.md](EDGE_IMPULSE_SETUP.md)

### 添加新功能

1. 在 `main/` 目錄創建新的 `.c` 或 `.cpp` 文件
2. 在 `main/CMakeLists.txt` 中添加源文件
3. 重新編譯

### 調試技巧

```bash
# 查看完整日誌
idf.py -p COM6 monitor

# 只編譯不燒錄
idf.py build

# 清理並重新編譯
idf.py fullclean
idf.py build
```

## 🌟 特色亮點

### 1. 24-bit 音頻處理

- INMP441 原生 24-bit 輸出
- 動態範圍：144 dB（vs 16-bit 的 96 dB）
- 更低的噪聲底限
- 更好的語音識別準確度

### 2. 自動錄音時長

- 智能語音活動檢測（VAD）
- 自動檢測語音結束
- 節省網絡帶寬和處理時間
- 更自然的對話體驗

### 3. Edge Impulse 機器學習

- 高準確度喚醒詞檢測
- 低誤觸發率
- 快速響應（~100ms）
- 可自定義訓練

## 📝 授權

本專案採用 MIT 授權。詳見 [LICENSE](LICENSE) 文件。

## 🙏 致謝

- [Edge Impulse](https://edgeimpulse.com/) - 機器學習平台
- [ESP-IDF](https://github.com/espressif/esp-idf) - Espressif IoT 開發框架
- [TensorFlow Lite Micro](https://www.tensorflow.org/lite/microcontrollers) - 嵌入式機器學習
- [ESP-DSP](https://github.com/espressif/esp-dsp) - 數位信號處理庫

## 📧 聯絡方式

如有問題或建議，歡迎提交 Issue 或 Pull Request。

---

**注意**: 本專案需要有效的 Edge Impulse 授權才能使用模型。請確保遵守 Edge Impulse 的使用條款。

## 🚀 版本歷史

### v2.0.0 (當前版本)
- ✅ 整合 Edge Impulse 喚醒詞檢測
- ✅ 實現自動錄音時長（VAD）
- ✅ 升級到 24-bit 音頻模式
- ✅ 優化記憶體使用
- ✅ 改進語音識別準確度

### v1.0.0
- 基於能量檢測的 "Hi ESP" 喚醒詞
- 固定 3 秒錄音時長
- 16-bit 音頻模式

---

**享受你的 Hi Lemon 語音助理！** 🍋🎤
