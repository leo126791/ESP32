# 🔊 TTS 音檔 PSRAM 播放功能

## ✅ 功能說明

現在系統會將 TTS 音檔直接下載到 PSRAM 並立即播放，不再依賴 SD 卡。

---

## 🎯 工作流程

```
1. 用戶說 "Hi ESP"
   ↓
2. 系統錄音 3 秒
   ↓
3. 上傳到服務器（STT 語音識別）
   ↓
4. 服務器返回 AI 回覆文字
   ↓
5. 下載 TTS 音檔到 PSRAM (voice.wav)
   ↓
6. 直接從 PSRAM 播放 TTS
   ↓
7. (可選) 同時保存到 SD 卡
```

---

## 💾 記憶體使用

### PSRAM 分配
- **TTS 音檔**: 最大 500KB
- **錄音緩衝**: 96KB (3秒 @ 16kHz)
- **總計**: ~600KB

### 優點
- ✅ 不依賴 SD 卡硬體
- ✅ 播放速度更快
- ✅ 減少 I/O 操作
- ✅ 簡化故障排除

---

## 🔧 實現細節

### 新增函數

#### `download_and_play_tts(const char* url)`
下載 TTS 音檔到 PSRAM 並播放。

**流程**：
1. 建立 HTTP 連線
2. 獲取檔案大小
3. 分配 PSRAM 緩衝區
4. 下載數據到 PSRAM
5. 調用 `audio_play_wav_buffer()` 播放
6. 釋放 PSRAM

**參數**：
- `url`: TTS 音檔的 URL

**返回**：
- `ESP_OK`: 成功
- `ESP_FAIL`: 失敗

---

## 📊 執行日誌示例

```
I (xxx) HI_ESP_KEYWORD: 🔊 檢測到 'Hi ESP'！
I (xxx) HI_ESP_KEYWORD: 🎙️  開始錄音...
I (xxx) HI_ESP_KEYWORD: ✅ 錄音完成: 48000 樣本 (3.0 秒)
I (xxx) HI_ESP_KEYWORD: 📤 上傳音頻到服務器...
I (xxx) HI_ESP_KEYWORD: ✅ 音頻上傳成功
I (xxx) HI_ESP_KEYWORD: 🎤 你說：你好
I (xxx) HI_ESP_KEYWORD: 🤖 AI 回覆：你好！有什麼可以幫助你的嗎？
I (xxx) HI_ESP_KEYWORD: 📥 開始下載 TTS 音檔到 PSRAM...
I (xxx) HI_ESP_KEYWORD: 📊 HTTP 狀態: 200, 檔案大小: 45678 bytes (44.6 KB)
I (xxx) HI_ESP_KEYWORD: 💾 分配 PSRAM: 45678 bytes
I (xxx) HI_ESP_KEYWORD: 📥 下載進度: 10% (4567/45678 bytes)
I (xxx) HI_ESP_KEYWORD: 📥 下載進度: 20% (9134/45678 bytes)
...
I (xxx) HI_ESP_KEYWORD: 📥 下載進度: 100% (45678/45678 bytes)
I (xxx) HI_ESP_KEYWORD: ✅ 下載完成: 45678 bytes
I (xxx) HI_ESP_KEYWORD: 🔊 開始播放 TTS...
I (xxx) AUDIO: 播放 WAV 數據: 22795 樣本
I (xxx) HI_ESP_KEYWORD: ✅ TTS 播放完成
I (xxx) HI_ESP_KEYWORD: 💾 同時保存到 SD 卡...
I (xxx) HI_ESP_KEYWORD: ✅ 已保存到 SD 卡: voice.wav
I (xxx) HI_ESP_KEYWORD: 🔄 繼續監聽...
```

---

## 🎛️ 配置選項

### 檔案大小限制
```c
if (content_length <= 0 || content_length > 500000) {  // 限制 500KB
    ESP_LOGE(TAG, "❌ 檔案大小異常: %d bytes", content_length);
    return ESP_FAIL;
}
```

可以根據需要調整 `500000` (500KB) 的限制。

### HTTP 超時
```c
.timeout_ms = 30000,  // 30 秒
```

### 緩衝區大小
```c
.buffer_size = 4096,  // 4KB
```

---

## 🔄 與 SD 卡的關係

### 主要播放：PSRAM
- TTS 音檔下載到 PSRAM
- 直接從 PSRAM 播放
- 播放完成後釋放記憶體

### 備份保存：SD 卡（可選）
- 如果 SD 卡已掛載
- 同時保存一份到 SD 卡
- 用於離線播放或調試

### 代碼邏輯
```c
// 主要功能：下載到 PSRAM 並播放
esp_err_t play_ret = download_and_play_tts(audio_url);

// 可選功能：保存到 SD 卡
if (sd_is_mounted()) {
    sd_download_wav(audio_url, "voice.wav");
}
```

---

## ⚡ 性能優勢

### 播放延遲對比

| 方式 | 延遲 | 說明 |
|------|------|------|
| **PSRAM 播放** | ~2-3 秒 | 下載 + 播放 |
| SD 卡播放 | ~4-5 秒 | 下載 + 寫入 + 讀取 + 播放 |

### 記憶體使用

| 方式 | PSRAM | DRAM | SD 卡 |
|------|-------|------|-------|
| **PSRAM 播放** | ~500KB | 最小 | 不需要 |
| SD 卡播放 | 最小 | 最小 | 需要 |

---

## 🐛 故障排除

### 問題 1：PSRAM 分配失敗
**錯誤**：
```
E (xxx) HI_ESP_KEYWORD: ❌ PSRAM 分配失敗
```

**原因**：
- PSRAM 不足
- 檔案太大

**解決**：
1. 檢查 PSRAM 可用空間
2. 減小 TTS 音檔大小
3. 增加檔案大小限制

### 問題 2：HTTP 下載失敗
**錯誤**：
```
E (xxx) HI_ESP_KEYWORD: ❌ HTTP 連線失敗
```

**原因**：
- WiFi 未連接
- 服務器不可達
- URL 錯誤

**解決**：
1. 檢查 WiFi 連接
2. 測試服務器 URL
3. 檢查防火牆設置

### 問題 3：播放失敗
**錯誤**：
```
E (xxx) HI_ESP_KEYWORD: ❌ TTS 播放失敗
```

**原因**：
- WAV 格式錯誤
- 音頻輸出未初始化
- 揚聲器未連接

**解決**：
1. 檢查 WAV 文件格式
2. 確認 MAX98357A 已初始化
3. 檢查揚聲器接線

---

## 🧪 測試步驟

### 1. 編譯項目
```bash
idf.py build
```

### 2. 燒錄到設備
```bash
idf.py -p COM3 flash monitor
```

### 3. 測試流程
1. 等待系統初始化
2. 說 "Hi ESP"
3. 說一句話（例如："你好"）
4. 等待 AI 回覆
5. 聽到 TTS 語音播放

### 4. 檢查日誌
- ✅ 看到 "下載 TTS 音檔到 PSRAM"
- ✅ 看到下載進度
- ✅ 看到 "TTS 播放完成"

---

## 📚 相關文件

- `main/hi_esp_keyword.c` - 主要實現
- `main/hi_esp_audio.c` - 音頻播放
- `main/hi_esp_audio.h` - 音頻接口
- `CURRENT_WIRING.md` - 接線指南

---

## 🎯 未來改進

### 可能的優化
1. **串流播放**：邊下載邊播放，減少延遲
2. **緩存機制**：常用回覆緩存在 PSRAM
3. **壓縮格式**：支援 MP3/AAC 減少檔案大小
4. **音量控制**：動態調整播放音量

---

**TTS PSRAM 播放功能已啟用！** 🎉
