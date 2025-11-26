# 🔄 GPIO 腳位重新規劃

## 📊 ESP32-S3 GPIO 使用建議

### 可用 GPIO 分類

#### ✅ 推薦使用（安全）
- GPIO 1-14, 21, 47, 48

#### ⚠️ 需注意
- GPIO 0: 啟動模式選擇（有上拉電阻）
- GPIO 19, 20: USB（如果使用 USB-JTAG）
- GPIO 43, 44: UART0（USB Serial）
- GPIO 45, 46: Strapping pins

#### ❌ 避免使用
- GPIO 26-32: SPI Flash/PSRAM（已佔用）
- GPIO 33-37: Octal PSRAM（已佔用）

---

## 🎯 新的 GPIO 配置方案

### 方案 A：標準配置（推薦）

```
┌─────────────────────────────────────────────────┐
│ 模組          功能      新 GPIO    原 GPIO      │
├─────────────────────────────────────────────────┤
│ INMP441      WS        GPIO 4     GPIO 38      │
│ (麥克風)     BCK       GPIO 5     GPIO 39      │
│              SD        GPIO 6     GPIO 37      │
├─────────────────────────────────────────────────┤
│ MAX98357A    LRC       GPIO 7     GPIO 17      │
│ (功放)       BCLK      GPIO 15    GPIO 16      │
│              DIN       GPIO 16    GPIO 18      │
│              SD        GPIO 17    GPIO 19      │
├─────────────────────────────────────────────────┤
│ SD Card      CS        GPIO 10    GPIO 10 ✓   │
│ (SPI)        MOSI      GPIO 11    GPIO 11 ✓   │
│              CLK       GPIO 12    GPIO 12 ✓   │
│              MISO      GPIO 13    GPIO 13 ✓   │
└─────────────────────────────────────────────────┘
```

**優點**：
- GPIO 4-7, 15-17 連續，方便接線
- SD 卡保持原配置（SPI2 預設引腳）
- 避開 Strapping pins

---

### 方案 B：緊湊配置

```
┌─────────────────────────────────────────────────┐
│ 模組          功能      新 GPIO    原 GPIO      │
├─────────────────────────────────────────────────┤
│ INMP441      WS        GPIO 1     GPIO 38      │
│ (麥克風)     BCK       GPIO 2     GPIO 39      │
│              SD        GPIO 3     GPIO 37      │
├─────────────────────────────────────────────────┤
│ MAX98357A    LRC       GPIO 8     GPIO 17      │
│ (功放)       BCLK      GPIO 9     GPIO 16      │
│              DIN       GPIO 18    GPIO 18 ✓   │
│              SD        GPIO 19    GPIO 19 ✓   │
├─────────────────────────────────────────────────┤
│ SD Card      CS        GPIO 10    GPIO 10 ✓   │
│ (SPI)        MOSI      GPIO 11    GPIO 11 ✓   │
│              CLK       GPIO 12    GPIO 12 ✓   │
│              MISO      GPIO 13    GPIO 13 ✓   │
└─────────────────────────────────────────────────┘
```

**優點**：
- GPIO 1-3 在板子一側
- GPIO 8-13, 18-19 在另一側
- 物理布局更清晰

---

### 方案 C：保守配置（最安全）

```
┌─────────────────────────────────────────────────┐
│ 模組          功能      新 GPIO    原 GPIO      │
├─────────────────────────────────────────────────┤
│ INMP441      WS        GPIO 4     GPIO 38      │
│ (麥克風)     BCK       GPIO 5     GPIO 39      │
│              SD        GPIO 6     GPIO 37      │
├─────────────────────────────────────────────────┤
│ MAX98357A    LRC       GPIO 8     GPIO 17      │
│ (功放)       BCLK      GPIO 9     GPIO 16      │
│              DIN       GPIO 21    GPIO 18      │
│              SD        GPIO 47    GPIO 19      │
├─────────────────────────────────────────────────┤
│ SD Card      CS        GPIO 10    GPIO 10 ✓   │
│ (SPI)        MOSI      GPIO 11    GPIO 11 ✓   │
│              CLK       GPIO 12    GPIO 12 ✓   │
│              MISO      GPIO 13    GPIO 13 ✓   │
└─────────────────────────────────────────────────┘
```

**優點**：
- 完全避開所有特殊功能引腳
- 最穩定的配置

---

## 💡 我的建議

### 推薦：方案 A（標準配置）

**理由**：
1. GPIO 連續，接線方便
2. SD 卡保持原配置（已知可用）
3. 避開大部分特殊引腳
4. 適合麵包板布局

---

## 🔧 實施步驟

### 如果選擇方案 A：

#### 1. 修改 INMP441 配置
```c
// main/hi_esp_keyword.c
#define I2S_BCK_PIN             GPIO_NUM_5   // 原 39
#define I2S_WS_PIN              GPIO_NUM_4   // 原 38
#define I2S_DATA_PIN            GPIO_NUM_6   // 原 37
```

#### 2. 修改 MAX98357A 配置
```c
// main/hi_esp_audio.h
#define I2S_SPEAKER_BCK_PIN     GPIO_NUM_15  // 原 16
#define I2S_SPEAKER_WS_PIN      GPIO_NUM_7   // 原 17
#define I2S_SPEAKER_DATA_PIN    GPIO_NUM_16  // 原 18
#define I2S_SPEAKER_SD_PIN      GPIO_NUM_17  // 原 19
```

#### 3. SD 卡保持不變
```c
// main/sd_card_manager.c
#define PIN_NUM_MISO  13  // 不變
#define PIN_NUM_MOSI  11  // 不變
#define PIN_NUM_CLK   12  // 不變
#define PIN_NUM_CS    10  // 不變
```

---

## 📋 新接線表（方案 A）

### INMP441 麥克風
```
INMP441    →    ESP32-S3
─────────────────────────
VDD        →    3.3V
GND        →    GND
L/R        →    GND
WS         →    GPIO 4  ⭐ 新
SCK        →    GPIO 5  ⭐ 新
SD         →    GPIO 6  ⭐ 新
```

### MAX98357A 功放
```
MAX98357A  →    ESP32-S3
─────────────────────────
VIN        →    5V
GND        →    GND
LRC        →    GPIO 7  ⭐ 新
BCLK       →    GPIO 15 ⭐ 新
DIN        →    GPIO 16 ⭐ 新
SD         →    GPIO 17 ⭐ 新
```

### SD 卡（不變）
```
SD Card    →    ESP32-S3
─────────────────────────
VCC        →    3.3V
GND        →    GND
MISO       →    GPIO 13 ✓
MOSI       →    GPIO 11 ✓
CLK        →    GPIO 12 ✓
CS         →    GPIO 10 ✓
```

---

## 🎯 你想要哪個方案？

請告訴我你選擇：
- **方案 A**：標準配置（推薦）
- **方案 B**：緊湊配置
- **方案 C**：保守配置
- **自訂**：告訴我你的需求

我會幫你修改所有相關的代碼文件！
