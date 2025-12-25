# Hi Lemon ESP32-S3 完整腳位配置

## 📌 硬體清單

- **主控板**: ESP32-S3-DevKitC-1 (8MB PSRAM, 16MB Flash)
- **麥克風**: INMP441 數位 MEMS 麥克風
- **音頻放大器**: MAX98357A I2S 音頻放大器
- **喇叭**: 3-8Ω, 3W

---

## 🎤 INMP441 麥克風接線

### 腳位對應

| INMP441 引腳 | ESP32-S3 GPIO | 說明 |
|-------------|---------------|------|
| **VDD** | 3.3V | 電源正極 |
| **GND** | GND | 電源負極/接地 |
| **L/R** | GND | 聲道選擇（GND=左聲道, 3.3V=右聲道）|
| **WS** (LRCLK) | **GPIO 4** | 字選擇時鐘（左右聲道選擇）|
| **SCK** (BCLK) | **GPIO 5** | 位時鐘（串行時鐘）|
| **SD** (DOUT) | **GPIO 6** | 串行數據輸出 |

### 接線圖

```
INMP441                    ESP32-S3
┌─────────┐               ┌──────────┐
│   VDD   │──────────────→│   3.3V   │
│   GND   │──────────────→│   GND    │
│   L/R   │──────────────→│   GND    │ (左聲道)
│   WS    │──────────────→│  GPIO 4  │
│   SCK   │──────────────→│  GPIO 5  │
│   SD    │──────────────→│  GPIO 6  │
└─────────┘               └──────────┘
```

### 配置說明

- **I2S 模式**: Master RX（主機接收模式）
- **採樣率**: 16,000 Hz
- **位元深度**: 24-bit（使用 32-bit 容器）
- **聲道**: 單聲道（左聲道）
- **通訊格式**: I2S 標準格式

### 程式碼配置

```c
// main/hi_lemon_keyword.c
#define I2S_NUM                 I2S_NUM_0
#define I2S_SAMPLE_RATE         16000
#define I2S_BCK_PIN             GPIO_NUM_5
#define I2S_WS_PIN              GPIO_NUM_4
#define I2S_DATA_PIN            GPIO_NUM_6
```

---

## 🔊 MAX98357A 音頻放大器接線

### 腳位對應

| MAX98357A 引腳 | ESP32-S3 GPIO | 說明 |
|---------------|---------------|------|
| **VIN** | 5V | 電源輸入（5V，也可用 3.3V 但音量較小）|
| **GND** | GND | 電源負極/接地 |
| **BCLK** | **GPIO 15** | 位時鐘 |
| **LRC** (LRCLK) | **GPIO 7** | 左右聲道時鐘 |
| **DIN** | **GPIO 16** | 數據輸入 |
| **SD** (Shutdown) | **GPIO 17** | 關機控制（可選，HIGH=開啟, LOW=關閉）|
| **GAIN** | 懸空或接地 | 增益控制（見下表）|

### 喇叭連接

| MAX98357A 引腳 | 喇叭 |
|---------------|------|
| **+** | 喇叭正極 |
| **-** | 喇叭負極 |

### 接線圖

```
MAX98357A                  ESP32-S3
┌─────────┐               ┌──────────┐
│   VIN   │──────────────→│    5V    │
│   GND   │──────────────→│   GND    │
│  BCLK   │←──────────────│ GPIO 15  │
│   LRC   │←──────────────│  GPIO 7  │
│   DIN   │←──────────────│ GPIO 16  │
│   SD    │←──────────────│ GPIO 17  │ (可選)
└─────────┘               └──────────┘
     │ +
     │ -
     ↓
  ┌──────┐
  │ 喇叭  │ (3-8Ω)
  └──────┘
```

### 增益設定

通過 GAIN 引腳設置音量增益：

| GAIN 連接 | 增益 (dB) | 說明 |
|----------|----------|------|
| 懸空 | 9 dB | 預設（推薦）|
| GND | 6 dB | 較小音量 |
| VDD | 12 dB | 較大音量 |
| 100kΩ 到 GND | 15 dB | 最大音量 |

**推薦**: 保持 GAIN 懸空（9 dB）

### 配置說明

- **I2S 模式**: Master TX（主機發送模式）
- **採樣率**: 16,000 Hz
- **位元深度**: 16-bit
- **聲道**: 單聲道
- **SD 控制**: GPIO 17（可選，用於開關放大器）

### 程式碼配置

```c
// main/hi_esp_audio.h
#define I2S_SPEAKER_NUM         I2S_NUM_1
#define I2S_SPEAKER_BCK_PIN     GPIO_NUM_15
#define I2S_SPEAKER_WS_PIN      GPIO_NUM_7
#define I2S_SPEAKER_DATA_PIN    GPIO_NUM_16
#define I2S_SPEAKER_SD_PIN      GPIO_NUM_17
#define I2S_SPEAKER_SAMPLE_RATE 16000
```

---

## 📊 完整腳位總覽

### 使用中的 GPIO

| GPIO | 功能 | 方向 | 說明 |
|------|------|------|------|
| **GPIO 4** | INMP441 WS | 輸出 | 麥克風字選擇時鐘 |
| **GPIO 5** | INMP441 SCK | 輸出 | 麥克風位時鐘 |
| **GPIO 6** | INMP441 SD | 輸入 | 麥克風數據輸入 |
| **GPIO 7** | MAX98357A LRC | 輸出 | 音頻左右聲道時鐘 |
| **GPIO 15** | MAX98357A BCLK | 輸出 | 音頻位時鐘 |
| **GPIO 16** | MAX98357A DIN | 輸出 | 音頻數據輸出 |
| **GPIO 17** | MAX98357A SD | 輸出 | 音頻放大器開關（可選）|

### 可用的 GPIO（未使用）

以下 GPIO 可用於擴展功能：

| GPIO | 說明 | 建議用途 |
|------|------|---------|
| GPIO 1-3 | 可用 | LED 指示燈、按鈕 |
| GPIO 8-14 | 可用 | SD 卡、SPI 設備 |
| GPIO 18-21 | 可用 | I2C 設備、傳感器 |
| GPIO 35-48 | 可用 | 其他外設 |

### 保留/特殊 GPIO

| GPIO | 說明 | 注意事項 |
|------|------|---------|
| GPIO 0 | Boot 按鈕 | 啟動時需為 HIGH |
| GPIO 19, 20 | USB D-, D+ | 用於 USB 通訊 |
| GPIO 43, 44 | UART0 TX, RX | 用於程式下載和監控 |
| GPIO 45 | Strapping Pin | 啟動配置 |
| GPIO 46 | Strapping Pin | 啟動配置 |

---

## 🔌 電源配置

### 電源需求

| 組件 | 電壓 | 電流 | 說明 |
|------|------|------|------|
| **ESP32-S3** | 3.3V | ~500mA | 主控板（含 WiFi）|
| **INMP441** | 3.3V | ~1.4mA | 麥克風（低功耗）|
| **MAX98357A** | 5V | ~1.5A | 音頻放大器（最大）|
| **總計** | - | ~2A | 建議使用 5V/2A 電源 |

### 電源連接

```
USB 5V ──┬──→ ESP32-S3 (5V 輸入)
         │      ↓
         │   3.3V 穩壓器
         │      ↓
         │   ├──→ INMP441 (3.3V)
         │   └──→ 其他 3.3V 設備
         │
         └──→ MAX98357A (5V)
```

---

## 🛠️ 接線建議

### 1. 線材選擇

- **電源線**: 使用較粗的線（20-22 AWG）
- **信號線**: 使用細線（26-28 AWG）
- **長度**: 盡量短（< 15cm）以減少干擾

### 2. 接地

- 所有 GND 必須連接在一起
- 使用星形接地（所有 GND 連到一個點）
- 避免接地迴路

### 3. 電源去耦

在每個 IC 的電源引腳附近放置去耦電容：
- **ESP32-S3**: 100nF + 10µF
- **INMP441**: 100nF
- **MAX98357A**: 100nF + 10µF

### 4. 信號完整性

- I2S 信號線遠離電源線
- 使用雙絞線或屏蔽線
- 避免與 WiFi 天線區域重疊

---

## 🧪 測試步驟

### 1. 麥克風測試

```c
// 檢查麥克風是否正常工作
ESP_LOGI(TAG, "測試麥克風...");
int16_t buffer[1024];
size_t bytes_read;
i2s_read(I2S_NUM_0, buffer, sizeof(buffer), &bytes_read, 1000);
ESP_LOGI(TAG, "讀取 %d 字節", bytes_read);
```

### 2. 音頻輸出測試

```c
// 播放測試音（440Hz 正弦波）
audio_play_beep(440, 1000);  // 1 秒
```

### 3. 完整系統測試

1. 上電後檢查串口輸出
2. 說 "Hi Lemon" 測試喚醒
3. 說話測試錄音
4. 檢查 TTS 播放

---

## 📋 故障排除

### 麥克風無聲音

1. ✅ 檢查 VDD 是否為 3.3V
2. ✅ 檢查 L/R 是否接 GND
3. ✅ 檢查 WS, SCK, SD 接線
4. ✅ 用示波器檢查時鐘信號

### 音頻無輸出

1. ✅ 檢查 MAX98357A 電源（5V）
2. ✅ 檢查 SD 引腳是否為 HIGH
3. ✅ 檢查喇叭連接
4. ✅ 檢查 BCLK, LRC, DIN 信號

### 音質差

1. ✅ 檢查電源穩定性
2. ✅ 添加去耦電容
3. ✅ 縮短信號線長度
4. ✅ 遠離干擾源

### WiFi 連接問題

1. ✅ 檢查天線區域無遮擋
2. ✅ 確保電源充足
3. ✅ 檢查 WiFi 配置

---

## 🎨 PCB 設計建議

如果你要設計 PCB：

### 1. 佈局

```
┌─────────────────────────────┐
│  [天線區域]                  │
│                              │
│  [ESP32-S3]                  │
│                              │
│  [INMP441]    [MAX98357A]   │
│                              │
│  [電源]       [喇叭接口]     │
└─────────────────────────────┘
```

### 2. 分層

- **頂層**: 信號線
- **內層 1**: GND 平面
- **內層 2**: 電源平面（3.3V, 5V）
- **底層**: 信號線

### 3. 注意事項

- WiFi 天線區域下方不要有銅箔
- I2S 信號線使用差分走線
- 電源線加粗（至少 0.5mm）
- 添加測試點方便調試

---

## 📸 實物接線參考

### 麵包板接線順序

1. **先接電源**
   - ESP32-S3 的 3.3V 和 GND
   - MAX98357A 的 5V 和 GND

2. **再接麥克風**
   - INMP441 的 6 根線

3. **最後接音頻**
   - MAX98357A 的 I2S 信號
   - 喇叭

4. **檢查**
   - 用萬用表檢查電源電壓
   - 檢查是否有短路

---

## 🔧 程式碼快速參考

### 查看當前配置

```c
// 在 app_main() 中添加
ESP_LOGI(TAG, "📌 腳位配置:");
ESP_LOGI(TAG, "麥克風 - WS: GPIO %d, SCK: GPIO %d, SD: GPIO %d", 
         I2S_WS_PIN, I2S_BCK_PIN, I2S_DATA_PIN);
ESP_LOGI(TAG, "音頻 - BCLK: GPIO %d, LRC: GPIO %d, DIN: GPIO %d, SD: GPIO %d",
         I2S_SPEAKER_BCK_PIN, I2S_SPEAKER_WS_PIN, 
         I2S_SPEAKER_DATA_PIN, I2S_SPEAKER_SD_PIN);
```

### 修改腳位

如果需要更改腳位，修改這兩個文件：

1. **main/hi_lemon_keyword.c** (麥克風)
```c
#define I2S_BCK_PIN             GPIO_NUM_5   // 改這裡
#define I2S_WS_PIN              GPIO_NUM_4   // 改這裡
#define I2S_DATA_PIN            GPIO_NUM_6   // 改這裡
```

2. **main/hi_esp_audio.h** (音頻輸出)
```c
#define I2S_SPEAKER_BCK_PIN     GPIO_NUM_15  // 改這裡
#define I2S_SPEAKER_WS_PIN      GPIO_NUM_7   // 改這裡
#define I2S_SPEAKER_DATA_PIN    GPIO_NUM_16  // 改這裡
#define I2S_SPEAKER_SD_PIN      GPIO_NUM_17  // 改這裡
```

---

## ✅ 檢查清單

在開始之前，確認：

- [ ] ESP32-S3 開發板（8MB PSRAM, 16MB Flash）
- [ ] INMP441 麥克風模組
- [ ] MAX98357A 音頻放大器模組
- [ ] 3-8Ω 喇叭
- [ ] 杜邦線（公對母、母對母）
- [ ] USB-C 數據線
- [ ] 5V/2A 電源供應器
- [ ] 麵包板（可選）
- [ ] 萬用表（用於測試）

---

**準備好了嗎？開始組裝你的 Hi Lemon 語音助理吧！** 🍋🎤
