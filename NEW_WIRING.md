# 🎯 新的 GPIO 配置 - 方案 A（已確認）

## ✅ 所有代碼已更新並確認

所有 GPIO 配置已修改完成，請按照新的接線圖重新連接硬體。

---

## 📌 新的接線圖

### 1️⃣ INMP441 數位麥克風

```
INMP441 引腳    →    ESP32-S3 引腳
─────────────────────────────────
VDD             →    3.3V
GND             →    GND
L/R             →    GND (左聲道)
WS (LRCLK)      →    GPIO 4  ⭐ 新
SCK (BCLK)      →    GPIO 5  ⭐ 新
SD (DOUT)       →    GPIO 6  ⭐ 新
```

**變更**：
- GPIO 38 → GPIO 4
- GPIO 39 → GPIO 5
- GPIO 37 → GPIO 6

---

### 2️⃣ MAX98357A 音頻功放

```
MAX98357A 引腳  →    ESP32-S3 引腳
─────────────────────────────────
VIN             →    5V
GND             →    GND
LRC             →    GPIO 7  ⭐ 新
BCLK            →    GPIO 15 ⭐ 新
DIN             →    GPIO 16 ⭐ 新
SD              →    GPIO 17 ⭐ 新
GAIN            →    懸空 (預設 9dB)
```

**揚聲器連接**：
```
MAX98357A       →    揚聲器
─────────────────────────────
+               →    揚聲器 +
-               →    揚聲器 -
```

**變更**：
- GPIO 17 → GPIO 7
- GPIO 16 → GPIO 15
- GPIO 18 → GPIO 16
- GPIO 19 → GPIO 17

---

### 3️⃣ SD 卡模組

```
SD Card 引腳    →    ESP32-S3 引腳
─────────────────────────────────
VCC             →    3.3V
GND             →    GND
MISO            →    GPIO 8  ⭐ 新
MOSI            →    GPIO 2  ⭐ 新
SCK (CLK)       →    GPIO 3  ⭐ 新
CS              →    GPIO 1  ⭐ 新
```

**說明**：SD 卡使用自訂 GPIO 配置。

---

## 📊 GPIO 使用總覽（新配置）

| GPIO | 功能 | 模組 | 方向 | 變更 |
|------|------|------|------|------|
| 4 | I2S0 WS | INMP441 | 輸出 | ⭐ 新 |
| 5 | I2S0 BCK | INMP441 | 輸出 | ⭐ 新 |
| 6 | I2S0 SD | INMP441 | 輸入 | ⭐ 新 |
| 1 | SPI CS | SD Card | 輸出 | ⭐ 新 |
| 2 | SPI MOSI | SD Card | 輸出 | ⭐ 新 |
| 3 | SPI CLK | SD Card | 輸出 | ⭐ 新 |
| 7 | I2S1 WS | MAX98357A | 輸出 | ⭐ 新 |
| 8 | SPI MISO | SD Card | 輸入 | ⭐ 新 |
| 15 | I2S1 BCK | MAX98357A | 輸出 | ⭐ 新 |
| 16 | I2S1 DIN | MAX98357A | 輸出 | ⭐ 新 |
| 17 | SD Control | MAX98357A | 輸出 | ⭐ 新 |

---

## 🔌 接線圖示（新配置）

```
                    ESP32-S3-N16R8
                    ┌─────────────┐
         USB/5V ────┤ 5V          │
                    │ 3.3V        │
                    │ GND         │
                    │             │
    INMP441         │             │         MAX98357A
    ┌─────┐         │             │         ┌─────────┐
    │ VDD ├─────────┤ 3.3V        │    ┌────┤ VIN     │
    │ GND ├─────────┤ GND         │    │    │ GND ────┼──┐
    │ L/R ├─────────┤ GND         │    │    │ LRC ────┼──┤
    │ WS  ├─────────┤ GPIO 4  ⭐  │    │    │ BCLK ───┼──┤
    │ SCK ├─────────┤ GPIO 5  ⭐  │    │    │ DIN ────┼──┤
    │ SD  ├─────────┤ GPIO 6  ⭐  │    │    │ SD   ───┼──┤
    └─────┘         │             │    │    └─────────┘  │
                    │             │    │                 │
    SD Card         │             │    │    揚聲器       │
    ┌─────┐         │             │    │    ┌────┐      │
    │ VCC ├─────────┤ 3.3V        │    │    │ +  │      │
    │ GND ├─────────┤ GND         │    │    │ -  │      │
    │MISO ├─────────┤ GPIO 13 ✓   │    │    └────┘      │
    │MOSI ├─────────┤ GPIO 11 ✓   │    │                 │
    │ CLK ├─────────┤ GPIO 12 ✓   │    │                 │
    │ CS  ├─────────┤ GPIO 10 ✓   │    │                 │
    └─────┘         │             │    │                 │
                    │ GPIO 7  ⭐──┼────┘                 │
                    │ GPIO 15 ⭐──┼────┐                 │
                    │ GPIO 16 ⭐──┼────┤                 │
                    │ GPIO 17 ⭐──┼────┤                 │
                    │             │    │                 │
                    │ 5V ─────────┼────┴─────────────────┘
                    │ GND ────────┼────── GND (共地)
                    └─────────────┘
```

---

## 🚀 下一步

### 1. 重新接線
按照上面的新接線圖重新連接所有模組。

### 2. 重新編譯
```bash
idf.py build
```

### 3. 燒錄測試
```bash
idf.py -p COM3 flash monitor
```

---

## 📋 接線檢查清單（新配置）

### INMP441 麥克風
- [ ] VDD → 3.3V
- [ ] GND → GND
- [ ] L/R → GND
- [ ] WS → **GPIO 4** ⭐
- [ ] SCK → **GPIO 5** ⭐
- [ ] SD → **GPIO 6** ⭐

### MAX98357A 功放
- [ ] VIN → 5V
- [ ] GND → GND
- [ ] LRC → **GPIO 7** ⭐
- [ ] BCLK → **GPIO 15** ⭐
- [ ] DIN → **GPIO 16** ⭐
- [ ] SD → **GPIO 17** ⭐
- [ ] 揚聲器已連接

### SD 卡
- [ ] VCC → 3.3V
- [ ] GND → GND
- [ ] MISO → **GPIO 8** ⭐
- [ ] MOSI → **GPIO 2** ⭐
- [ ] CLK → **GPIO 3** ⭐
- [ ] CS → **GPIO 1** ⭐

---

**代碼已更新完成！請重新接線並編譯測試。** 🎉
