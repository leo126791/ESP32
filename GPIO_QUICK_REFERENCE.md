# 📌 GPIO 腳位速查表

## 🎯 完整接線配置

### INMP441 麥克風
```
INMP441    →    ESP32-S3
─────────────────────────
VDD        →    3.3V
GND        →    GND
L/R        →    GND
WS         →    GPIO 4
SCK        →    GPIO 5
SD         →    GPIO 6
```

### MAX98357A 功放
```
MAX98357A  →    ESP32-S3
─────────────────────────
VIN        →    5V
GND        →    GND
LRC        →    GPIO 7
BCLK       →    GPIO 15
DIN        →    GPIO 16
SD         →    GPIO 17
```

### SD 卡模組
```
SD Card    →    ESP32-S3
─────────────────────────
VCC        →    3.3V
GND        →    GND
CS         →    GPIO 1
MOSI       →    GPIO 2
CLK        →    GPIO 3
MISO       →    GPIO 8
```

---

## 📊 GPIO 使用總覽

| GPIO | 模組 | 功能 | 方向 |
|------|------|------|------|
| **1** | SD Card | CS | 輸出 |
| **2** | SD Card | MOSI | 輸出 |
| **3** | SD Card | CLK | 輸出 |
| **4** | INMP441 | WS | 輸出 |
| **5** | INMP441 | BCK | 輸出 |
| **6** | INMP441 | SD | 輸入 |
| **7** | MAX98357A | LRC | 輸出 |
| **8** | SD Card | MISO | 輸入 |
| **15** | MAX98357A | BCLK | 輸出 |
| **16** | MAX98357A | DIN | 輸出 |
| **17** | MAX98357A | SD | 輸出 |

---

## 🔌 電源連接

```
5V 電源    →    ESP32-S3 (5V)
           →    MAX98357A (VIN)

3.3V       →    INMP441 (VDD)
           →    SD Card (VCC)

GND        →    所有模組的 GND (共地)
```

---

## ⚡ 快速檢查清單

### INMP441 (麥克風)
- [ ] GPIO 4 → WS
- [ ] GPIO 5 → SCK
- [ ] GPIO 6 → SD
- [ ] 3.3V 電源
- [ ] L/R 接 GND

### MAX98357A (揚聲器)
- [ ] GPIO 7 → LRC
- [ ] GPIO 15 → BCLK
- [ ] GPIO 16 → DIN
- [ ] GPIO 17 → SD
- [ ] 5V 電源
- [ ] 揚聲器已連接

### SD Card (可選)
- [ ] GPIO 1 → CS
- [ ] GPIO 2 → MOSI
- [ ] GPIO 3 → CLK
- [ ] GPIO 8 → MISO
- [ ] 3.3V 電源
- [ ] SD 卡已插入

---

## 💡 記憶口訣

### INMP441: **4-5-6**
- WS=4, BCK=5, SD=6

### MAX98357A: **7-15-16-17**
- LRC=7, BCLK=15, DIN=16, SD=17

### SD Card: **1-2-3-8**
- CS=1, MOSI=2, CLK=3, MISO=8

---

## 🎨 接線圖（簡化版）

```
        ESP32-S3
        ┌──────┐
        │  1   │ ← SD CS
        │  2   │ ← SD MOSI
        │  3   │ ← SD CLK
        │  4   │ ← MIC WS
        │  5   │ ← MIC BCK
        │  6   │ ← MIC SD
        │  7   │ ← SPK LRC
        │  8   │ ← SD MISO
        │ ...  │
        │  15  │ ← SPK BCLK
        │  16  │ ← SPK DIN
        │  17  │ ← SPK SD
        └──────┘
```

---

## 📱 保存到手機

拍照或截圖保存這個頁面，方便接線時查看！

---

**需要詳細說明？查看 `CURRENT_WIRING.md`** 📖
