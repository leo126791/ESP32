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
VIN        →    5V (必須 5V)
GND        →    GND
BCLK       →    GPIO 8
LRC        →    GPIO 9
DIN        →    GPIO 10
SD         →    GPIO 7
GAIN       →    GND (12dB 增益)
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

### LED 指示燈（錄音狀態）
```
LED        →    ESP32-S3
─────────────────────────
正極 (+)   →    GPIO 11 (透過 220Ω 電阻)
負極 (-)   →    GND
```
**注意**：LED 需要串聯 220Ω 限流電阻

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
| **7** | MAX98357A | SD | 輸出 |
| **8** | MAX98357A | BCLK | 輸出 |
| **9** | MAX98357A | LRC | 輸出 |
| **10** | MAX98357A | DIN | 輸出 |
| **11** | LED | 錄音指示燈 | 輸出 |

---

## 🔌 電源連接

```
5V 電源    →    ESP32-S3 (5V)
           →    MAX98357A (VIN) ⚡ 必須 5V

3.3V       →    INMP441 (VDD)

GND        →    所有模組的 GND (共地)
           →    MAX98357A GAIN (12dB 增益)
```

**重要提醒**：
- MAX98357A 必須使用 5V 供電，不可用 3.3V
- GAIN 引腳接 GND 可獲得最大音量（12dB）
- 所有 GND 必須連接在一起（共地）

---

## ⚡ 快速檢查清單

### INMP441 (麥克風)
- [ ] GPIO 4 → WS
- [ ] GPIO 5 → SCK
- [ ] GPIO 6 → SD
- [ ] 3.3V 電源
- [ ] L/R 接 GND

### MAX98357A (揚聲器)
- [ ] GPIO 7 → SD
- [ ] GPIO 8 → BCLK
- [ ] GPIO 9 → LRC
- [ ] GPIO 10 → DIN
- [ ] 5V 電源（不是 3.3V）
- [ ] GAIN → GND（最大音量）
- [ ] 揚聲器已連接（4-8Ω）

---

## 💡 記憶口訣

### INMP441: **4-5-6**
- WS=4, BCK=5, SD=6

### MAX98357A: **7-8-9-10**
- SD=7, BCLK=8, LRC=9, DIN=10

### LED: **11**
- 錄音指示燈 (需要 220Ω 電阻)

---

## 🎨 接線圖（簡化版）

```
        ESP32-S3
        ┌──────┐
        │  4   │ ← MIC WS
        │  5   │ ← MIC BCK
        │  6   │ ← MIC SD
        │  7   │ ← SPK SD
        │  8   │ ← SPK BCLK
        │  9   │ ← SPK LRC
        │  10  │ ← SPK DIN
        └──────┘
```

---

## 📱 保存到手機

拍照或截圖保存這個頁面，方便接線時查看！

---

## 🔧 故障排除

### 喇叭沒有聲音？

1. 確認 MAX98357A 的 VIN 接到 **5V**（不是 3.3V）
2. 確認 GAIN 引腳接到 **GND**
3. 檢查所有接線是否牢固
4. 用其他設備測試喇叭是否正常

詳細排查請參考 **[SPEAKER_TROUBLESHOOTING.md](SPEAKER_TROUBLESHOOTING.md)** 📖
