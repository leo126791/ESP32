# MAX98357A 硬體接線指南

## 接線表 (ESP32-S3 N16R8)

| MAX98357A 腳位 | 連接目標 (ESP32 / 電源) | 備註 |
|---|---|---|
| Vin | 5V (DC-DC 輸出) | ⚠️ 絕對不要接 3.3V，推力會不足 |
| GND | GND | 務必與 ESP32 共地 |
| DIN | GPIO 6 | 音訊數據 (Data) |
| BCLK | GPIO 4 | 位元時鐘 (Bit Clock) |
| LRC | GPIO 5 | 左右聲道時鐘 (WS) |
| SD | GPIO 7 (或接 3.3V) | 若接 GPIO 7 可用程式控制開關；接 3.3V 則恆開 |
| GAIN | 接 GND (建議) | 設定為 12dB 增益 (較大聲)。若懸空為 9dB (標準) |

## 重要注意事項

1. **電源**: MAX98357A 需要 5V 供電才能有足夠推力
2. **共地**: 確保 ESP32 和 MAX98357A 共用同一個 GND
3. **GPIO 選擇**: 避開 PSRAM 使用的引腳 (GPIO 35-37)
4. **增益設定**: GAIN 接 GND 可獲得 12dB 增益，聲音更大

## 編譯與燒錄

```bash
# 編譯
idf.py build

# 燒錄 (替換 COMx 為實際端口)
idf.py -p COMx flash monitor

# 如果燒錄失敗，按住 Boot 鍵再按 Reset
```

## 功能特點

- 使用 32-bit I2S 模式，音質清晰無雜訊
- 分塊記憶體管理，避免記憶體不足
- 支援播放嗶聲和 WAV 音檔
- 44.1kHz 採樣率，標準音質
