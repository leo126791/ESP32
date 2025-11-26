# ✅ SD 卡功能已重新啟用

## 更新內容

SD 卡初始化功能已重新啟用，並使用新的 GPIO 配置。

---

## 🔄 變更說明

### 代碼變更
- ✅ 取消註解 `sd_card_init()` 調用
- ✅ 更新 SD 卡 GPIO 配置為新引腳

### 新的 SD 卡 GPIO 配置
```
CS   → GPIO 1  (原 GPIO 10)
MOSI → GPIO 2  (原 GPIO 11)
CLK  → GPIO 3  (原 GPIO 12)
MISO → GPIO 8  (原 GPIO 13)
```

---

## 🔌 接線要求

### SD 卡模組接線
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

⚠️ **注意**：MISO 和 MOSI 最容易接反，請仔細檢查！

---

## 📋 SD 卡準備

1. **格式化 SD 卡**
   - 使用電腦格式化為 **FAT32**
   - 建議容量：≤ 32GB

2. **插入 SD 卡**
   - 確保 SD 卡插緊
   - 金屬觸點朝下

3. **檢查接線**
   - 所有 GND 連接在一起
   - VCC 接 3.3V（不是 5V！）
   - 信號線連接正確

---

## 🚀 測試步驟

### 1. 編譯項目
```bash
idf.py build
```

### 2. 燒錄到設備
```bash
idf.py -p COM3 flash monitor
```

### 3. 檢查輸出
應該看到：
```
I (xxx) SD_CARD: 初始化 SD 卡 (SPI 模式)...
I (xxx) SD_CARD: ✅ SD 卡掛載成功: /sdcard
I (xxx) SD_CARD: SD 卡容量: XXX MB
I (xxx) HI_ESP_KEYWORD: ✅ SD 卡初始化成功
```

---

## 🎯 SD 卡功能

啟用後，系統將能夠：

1. **下載 TTS 音檔**
   - 從服務器下載 AI 回覆的語音文件
   - 保存為 `/sdcard/voice.wav`

2. **保存錄音**
   - 可選：保存用戶的錄音到 SD 卡
   - 格式：16kHz, 16-bit, 單聲道 WAV

3. **播放音檔**
   - 從 SD 卡讀取 WAV 文件
   - 通過 MAX98357A 播放

---

## ⚠️ 故障排除

### 如果 SD 卡初始化失敗

1. **檢查接線**
   - 特別注意 MISO (GPIO 8) 和 MOSI (GPIO 2)
   - 確認 CS, CLK 連接正確

2. **檢查 SD 卡**
   - 確認已格式化為 FAT32
   - 嘗試更換 SD 卡

3. **檢查電源**
   - SD 卡 VCC 必須是 3.3V
   - 確保電源穩定

4. **查看詳細日誌**
   - 觀察串口輸出的錯誤信息
   - 參考 `SD_CARD_TROUBLESHOOTING.md`

---

## 📚 相關文檔

- `CURRENT_WIRING.md` - 完整接線指南
- `SD_CARD_TROUBLESHOOTING.md` - SD 卡故障排除
- `GPIO_UPDATE_SUMMARY.md` - GPIO 配置總結

---

**SD 卡功能已啟用，請重新接線並測試！** 🎉
