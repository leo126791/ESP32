# Hi ESP - 語音關鍵詞檢測系統

基於 ESP32-S3 + INMP441 麥克風的智能語音關鍵詞檢測系統，支持 "Hi ESP" 喚醒詞檢測、自動錄音和雲端上傳。

## 🎯 主要功能

- **持續監聽**：系統持續監聽環境聲音
- **關鍵詞檢測**：檢測到 "Hi ESP" 關鍵詞時觸發
- **自動錄音**：檢測到關鍵詞後自動錄音 3 秒
- **雲端上傳**：錄音完成後自動上傳到指定服務器
- **SD 卡備份**：支持本地 SD 卡存儲（可選）
- **機器學習**：支持 ML 模型訓練以提升準確度

## 🔧 硬件要求

### 必需組件
- ESP32-S3 開發板（建議 8MB PSRAM）
- INMP441 數字麥克風模組
- USB 數據線

### 可選組件
- Micro SD 卡（用於本地存儲）
- SD 卡模組

## 📦 快速開始

### 1. 硬件連接

| INMP441 | ESP32-S3 |
|---------|----------|
| VDD     | 3.3V     |
| GND     | GND      |
| L/R     | GND      |
| WS      | GPIO 38  |
| SCK     | GPIO 39  |
| SD      | GPIO 37  |

### 2. 配置 WiFi

編輯 `main/hi_esp_keyword.c`：

```c
#define WIFI_SSID       "你的WiFi名稱"
#define WIFI_PASSWORD   "你的WiFi密碼"
#define SERVER_URL      "http://your-server.com/api/audio"
```

### 3. 編譯和燒錄

```bash
# Windows
setup_idf.bat
build.bat
flash.bat

# Linux/Mac
. $HOME/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## 📚 文檔

- [硬件設置指南](docs/HARDWARE_SETUP.md) - INMP441、SD 卡、PSRAM 配置
- [開發指南](docs/DEVELOPMENT.md) - 編譯、燒錄、調試
- [問題排查](docs/TROUBLESHOOTING.md) - 常見問題解決方案
- [ML 訓練指南](docs/ML_TRAINING.md) - 機器學習模型訓練

## 🎵 當前配置

- **採樣率**: 16 kHz
- **錄音時長**: 3 秒
- **記憶體需求**: 96 KB
- **檢測準確度**: 70-80%（能量檢測）/ 90-98%（ML 模型）

## 🛠️ 開發工具

### 批次腳本（Windows）

```bash
setup_idf.bat          # 設置 ESP-IDF 環境
build.bat              # 編譯專案
flash.bat              # 燒錄到設備
monitor.bat            # 串口監控
switch_mode.bat        # 切換工作模式
```

### 模式切換

```bash
# 關鍵詞檢測模式（默認）
switch_mode.bat keyword

# 數據收集模式（用於 ML 訓練）
switch_mode.bat collect

# 測試模式
switch_mode.bat test
```

## 📊 性能指標

| 項目 | 能量檢測 | ML 模型 |
|------|----------|---------|
| 準確度 | 70-80% | 90-98% |
| 誤觸發率 | 中等 | 極低 |
| 記憶體使用 | ~100KB | ~150KB |
| 需要訓練 | 否 | 是 |

## 🔍 故障排除

### WiFi 連接失敗
- 確認 SSID 和密碼正確
- 確保使用 2.4GHz WiFi（ESP32 不支持 5GHz）

### 麥克風無聲音
- 檢查接線是否正確
- 確認 VDD 連接到 3.3V（不是 5V）
- 確認 L/R 接地（選擇左聲道）

### 檢測不準確
- 調整 `ENERGY_THRESHOLD` 參數
- 考慮訓練 ML 模型以提升準確度

詳細故障排除請參考 [TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md)

## 📁 專案結構

```
├── main/                    # 主程式碼
│   ├── core/               # 核心功能
│   ├── audio/              # 音頻處理
│   ├── keyword/            # 關鍵詞檢測
│   ├── storage/            # 存儲管理
│   └── network/            # 網絡功能
├── docs/                   # 文檔
├── tools/                  # 工具腳本
├── scripts/                # 批次腳本
└── tests/                  # 測試文件
```

## 🤝 貢獻

歡迎提交 Issue 和 Pull Request！

## 📄 授權

MIT License

## 🔗 相關資源

- [ESP-IDF 文檔](https://docs.espressif.com/projects/esp-idf/)
- [INMP441 數據手冊](https://invensense.tdk.com/products/digital/inmp441/)
- [ESP32-S3 技術規格](https://www.espressif.com/en/products/socs/esp32-s3)
