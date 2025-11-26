# 🎯 GPIO 配置更新總結

## ✅ 更新完成

所有 GPIO 配置已成功更新為新的方案 A（標準配置），包括 SD 卡自訂 GPIO。

**新增功能**：TTS 音檔現在直接下載到 PSRAM 並播放，不依賴 SD 卡！

---

## 📊 GPIO 變更對照表

### INMP441 麥克風
| 功能 | 原 GPIO | 新 GPIO | 狀態 |
|------|---------|---------|------|
| WS (LRCLK) | GPIO 38 | **GPIO 4** | ✅ 已更新 |
| BCK (BCLK) | GPIO 39 | **GPIO 5** | ✅ 已更新 |
| SD (DOUT) | GPIO 37 | **GPIO 6** | ✅ 已更新 |

### MAX98357A 功放
| 功能 | 原 GPIO | 新 GPIO | 狀態 |
|------|---------|---------|------|
| LRC | GPIO 17 | **GPIO 7** | ✅ 已更新 |
| BCLK | GPIO 16 | **GPIO 15** | ✅ 已更新 |
| DIN | GPIO 18 | **GPIO 16** | ✅ 已更新 |
| SD | GPIO 19 | **GPIO 17** | ✅ 已更新 |

### SD 卡（SPI）
| 功能 | 原 GPIO | 新 GPIO | 狀態 |
|------|---------|---------|------|
| CS | GPIO 10 | **GPIO 1** | ✅ 已更新 |
| MOSI | GPIO 11 | **GPIO 2** | ✅ 已更新 |
| CLK | GPIO 12 | **GPIO 3** | ✅ 已更新 |
| MISO | GPIO 13 | **GPIO 8** | ✅ 已更新 |

---

## 📝 已更新的文件

### 1. 代碼文件
- ✅ `main/hi_esp_audio.h` - MAX98357A GPIO 定義
- ✅ `main/hi_esp_keyword.c` - INMP441 GPIO 定義
- ✅ `main/sd_card_manager.c` - SD 卡 GPIO 定義

### 2. 文檔文件
- ✅ `CURRENT_WIRING.md` - 當前接線指南
- ✅ `NEW_WIRING.md` - 新接線方案說明
- ✅ `GPIO_UPDATE_SUMMARY.md` - 更新總結

---

## 🔌 新的接線配置

### INMP441 麥克風
```
INMP441    →    ESP32-S3
─────────────────────────
VDD        →    3.3V
GND        →    GND
L/R        →    GND
WS         →    GPIO 4  ⭐
SCK        →    GPIO 5  ⭐
SD         →    GPIO 6  ⭐
```

### MAX98357A 功放
```
MAX98357A  →    ESP32-S3
─────────────────────────
VIN        →    5V
GND        →    GND
LRC        →    GPIO 7  ⭐
BCLK       →    GPIO 15 ⭐
DIN        →    GPIO 16 ⭐
SD         →    GPIO 17 ⭐
```

### SD 卡
```
SD Card    →    ESP32-S3
─────────────────────────
VCC        →    3.3V
GND        →    GND
MISO       →    GPIO 8  ⭐
MOSI       →    GPIO 2  ⭐
CLK        →    GPIO 3  ⭐
CS         →    GPIO 1  ⭐
```

---

## 🚀 下一步操作

### 1. 重新接線
按照新的 GPIO 配置重新連接所有模組：
- INMP441: GPIO 4, 5, 6
- MAX98357A: GPIO 7, 15, 16, 17
- SD Card: 保持不變

### 2. 編譯項目
```bash
idf.py build
```

### 3. 燒錄到設備
```bash
idf.py -p COM3 flash monitor
```

### 4. 測試功能
- 麥克風錄音測試
- 揚聲器播放測試
- SD 卡讀寫測試
- 關鍵詞檢測測試

---

## 💡 為什麼要更改 GPIO？

### 原因
1. **避開特殊引腳**: GPIO 37-39 是 Octal PSRAM 相關引腳，可能導致衝突
2. **更好的布局**: 新的 GPIO 4-7, 15-17 更連續，方便接線
3. **提高穩定性**: 使用推薦的 GPIO 範圍，減少干擾

### 優點
- ✅ 避開 PSRAM 引腳衝突
- ✅ GPIO 連續，接線更方便
- ✅ 符合 ESP32-S3 最佳實踐
- ✅ SD 卡保持原配置（已知可用）

---

## ⚠️ 注意事項

1. **必須重新接線**: 舊的接線配置將不再工作
2. **檢查連接**: 確保所有 GPIO 連接正確
3. **共地連接**: 確保所有 GND 連接在一起
4. **電源電壓**: INMP441 和 SD Card 使用 3.3V，MAX98357A 使用 5V

---

## 📚 參考文檔

- `CURRENT_WIRING.md` - 完整接線指南（已更新）
- `NEW_WIRING.md` - 新接線方案詳細說明
- `NEW_GPIO_PLAN.md` - GPIO 規劃方案對比

---

**更新完成！請重新接線並測試。** 🎉
