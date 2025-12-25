# 更新 Edge Impulse 模型指南

## 📋 概述

當你在 Edge Impulse Studio 訓練了新版本的模型後，需要將新模型部署到 ESP32-S3。本指南將教你如何替換模型。

## 🎯 何時需要更新模型

- ✅ 訓練了新的喚醒詞
- ✅ 增加了更多訓練樣本
- ✅ 改進了模型準確度
- ✅ 更改了採樣率或窗口大小
- ✅ 添加了新的分類標籤

## 📦 步驟 1: 從 Edge Impulse 導出模型

### 1.1 登入 Edge Impulse Studio

前往 [Edge Impulse Studio](https://studio.edgeimpulse.com/)

### 1.2 選擇你的專案

選擇 "Lemon-ESP" 或你的專案名稱

### 1.3 導出模型

1. 點擊左側菜單 **"Deployment"**
2. 選擇 **"C++ library"**
3. 選擇優化選項：
   - **EON Compiler**: ✅ 啟用（推薦，更快更小）
   - **Quantized (int8)**: ✅ 啟用（節省記憶體）
4. 點擊 **"Build"**
5. 下載 ZIP 檔案（例如：`ei-lemon-esp-arduino-1.0.2.zip`）

### 1.4 解壓縮檔案

```
下載的 ZIP 檔案結構:
ei-lemon-esp-arduino-1.0.2.zip
├── edge-impulse-sdk/          ← SDK 核心
├── model-parameters/           ← 模型參數
├── tflite-model/              ← TensorFlow Lite 模型
├── README.txt
└── INTEGRATION.md
```

## 🔄 步驟 2: 備份舊模型

在替換前，先備份現有模型：

```bash
# 在專案根目錄執行
cd components
cp -r lemong_wake lemong_wake_backup_$(date +%Y%m%d)

# Windows PowerShell
Copy-Item -Recurse lemong_wake lemong_wake_backup_20241202
```

## 📂 步驟 3: 替換模型檔案

### 方法 A: 完整替換（推薦）

```bash
# 1. 刪除舊模型（保留 CMakeLists.txt）
cd components/lemong_wake
rm -rf edge-impulse-sdk model-parameters tflite-model

# Windows
rmdir /s /q edge-impulse-sdk
rmdir /s /q model-parameters
rmdir /s /q tflite-model

# 2. 複製新模型
# 從解壓的 ZIP 檔案複製這三個資料夾到 components/lemong_wake/
```

### 方法 B: 手動替換（精確控制）

只替換必要的檔案：

```
components/lemong_wake/
├── edge-impulse-sdk/          ← 完整替換
├── model-parameters/          ← 完整替換
│   ├── model_metadata.h       ← 重要！包含模型配置
│   └── model_variables.h      ← 重要！包含分類標籤
├── tflite-model/              ← 完整替換
│   └── trained_model_compiled.cpp  ← 編譯後的模型
└── CMakeLists.txt             ← 保留不變！
```

## ⚙️ 步驟 4: 檢查模型配置

### 4.1 檢查採樣率

打開 `components/lemong_wake/model-parameters/model_metadata.h`：

```c
#define EI_CLASSIFIER_FREQUENCY                  16000  // 必須是 16000
#define EI_CLASSIFIER_RAW_SAMPLE_COUNT           16000  // 必須是 16000
```

**如果不是 16000**:
- 你的模型使用了不同的採樣率
- 需要修改 `main/hi_lemon_keyword.c` 中的 `I2S_SAMPLE_RATE`

### 4.2 檢查輸入大小

```c
#define EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE       16000  // 應該是 16000
```

**如果改變了**:
- 需要修改 `main/hi_lemon_keyword.c` 中的 `EI_WINDOW_SIZE`

### 4.3 檢查分類標籤

打開 `components/lemong_wake/model-parameters/model_variables.h`：

```c
const char* ei_classifier_inferencing_categories[] = { 
    "hi lemon",   // 索引 0
    "noise"       // 索引 1
};
```

**如果標籤改變了**:
- 需要修改 `main/ei_wrapper.cpp` 中的標籤檢查邏輯

## 🔧 步驟 5: 更新程式碼（如果需要）

### 5.1 如果採樣率改變

假設新模型使用 8000 Hz：

```c
// main/hi_lemon_keyword.c

// 原本
#define I2S_SAMPLE_RATE         16000
#define EI_WINDOW_SIZE          16000

// 改為
#define I2S_SAMPLE_RATE         8000
#define EI_WINDOW_SIZE          8000
```

### 5.2 如果窗口大小改變

假設新模型使用 2 秒窗口（32000 樣本）：

```c
// main/hi_lemon_keyword.c

// 原本
#define EI_WINDOW_SIZE          16000   // 1 秒

// 改為
#define EI_WINDOW_SIZE          32000   // 2 秒
#define EI_SLIDE_SIZE           16000   // 滑動 1 秒
```

### 5.3 如果分類標籤改變

假設新模型有 3 個分類：

```c
// model_variables.h 中
const char* ei_classifier_inferencing_categories[] = { 
    "hi lemon",      // 索引 0
    "hey lemon",     // 索引 1
    "noise"          // 索引 2
};
```

更新 `main/hi_lemon_keyword.c`：

```c
// 原本
if (label_idx == 0 || strstr(label, "hi lemon") != NULL) {
    ESP_LOGI(TAG, "🔊 檢測到 'Hi Lemon'！");
    record_and_upload();
}

// 改為
if (label_idx == 0 || label_idx == 1) {  // 接受兩種喚醒詞
    ESP_LOGI(TAG, "🔊 檢測到喚醒詞: %s", label);
    record_and_upload();
}
```

### 5.4 如果信心閾值需要調整

```c
// main/ei_wrapper.cpp

// 原本
if (best_score > 0.8f) {  // 80% 信心閾值
    return best_idx;
}

// 根據新模型表現調整
if (best_score > 0.7f) {  // 降低到 70%
    return best_idx;
}
```

## 🏗️ 步驟 6: 編譯測試

### 6.1 清理舊的編譯檔案

```bash
idf.py fullclean
```

### 6.2 重新編譯

```bash
idf.py build
```

### 6.3 檢查編譯輸出

注意這些資訊：

```
Components: ... lemong_wake ...
Building CXX object esp-idf/lemong_wake/...
Linking CXX static library liblemong_wake.a
```

**常見編譯錯誤**:

#### 錯誤 1: 找不到標頭檔
```
fatal error: model_metadata.h: No such file or directory
```
**解決**: 檢查 `model-parameters/` 資料夾是否正確複製

#### 錯誤 2: 記憶體不足
```
region `iram0_0_seg' overflowed
```
**解決**: 新模型太大，需要優化或使用更大的分區

#### 錯誤 3: 未定義的引用
```
undefined reference to `ei_classifier_inferencing_categories'
```
**解決**: 檢查 `model_variables.h` 是否正確

## 🧪 步驟 7: 測試新模型

### 7.1 燒錄到設備

```bash
idf.py -p COM6 flash monitor
```

### 7.2 檢查啟動日誌

應該看到：

```
I (xxxx) EI_WRAPPER: Edge Impulse 模型初始化完成
I (xxxx) HI_LEMON: 🤖 使用 Edge Impulse 模型檢測
```

### 7.3 測試喚醒詞

1. 說 "Hi Lemon"
2. 觀察日誌：
   ```
   I (xxxx) HI_LEMON: 📊 檢測語音能量: 150000
   I (xxxx) HI_LEMON: 🎯 檢測到: hi lemon
   I (xxxx) HI_LEMON: 🔊 檢測到 'Hi Lemon'！
   ```

### 7.4 測試準確度

記錄測試結果：

| 測試 | 說的話 | 是否觸發 | 正確？ |
|------|--------|---------|--------|
| 1 | "Hi Lemon" | ✅ | ✅ |
| 2 | "Hi Lemon" | ✅ | ✅ |
| 3 | "Hello" | ❌ | ✅ |
| 4 | 背景噪音 | ❌ | ✅ |
| 5 | "Hi Lemon" | ❌ | ❌ 漏檢 |

**準確度計算**:
```
準確度 = 正確次數 / 總測試次數 × 100%
```

## 📊 步驟 8: 性能對比

### 8.1 記憶體使用

```bash
# 編譯後查看
idf.py size

# 比較
舊模型: Flash 1.2 MB, SRAM 180 KB
新模型: Flash 1.3 MB, SRAM 190 KB
```

### 8.2 推理時間

在 `main/ei_wrapper.cpp` 中添加計時：

```cpp
int ei_wrapper_run_inference(int16_t *raw_data, size_t data_len) {
    // ... 現有代碼 ...
    
    // 開始計時
    int64_t start_time = esp_timer_get_time();
    
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    
    // 結束計時
    int64_t end_time = esp_timer_get_time();
    ESP_LOGI(TAG, "⏱️  推理時間: %lld ms", (end_time - start_time) / 1000);
    
    // ... 繼續 ...
}
```

### 8.3 準確度對比

| 指標 | 舊模型 | 新模型 | 改善 |
|------|--------|--------|------|
| 準確度 | 85% | 92% | +7% |
| 誤觸發率 | 5% | 2% | -3% |
| 推理時間 | 120ms | 100ms | -20ms |

## 🔄 步驟 9: 回滾（如果需要）

如果新模型表現不佳：

```bash
# 1. 刪除新模型
cd components
rm -rf lemong_wake

# 2. 恢復備份
cp -r lemong_wake_backup_20241202 lemong_wake

# 3. 重新編譯
cd ../..
idf.py fullclean
idf.py build
idf.py -p COM6 flash
```

## 📝 步驟 10: 更新文檔

更新 `EDGE_IMPULSE_INTEGRATION.md`：

```markdown
## 模型版本

- **當前版本**: v1.0.3
- **更新日期**: 2024-12-02
- **訓練樣本**: 150 個 "Hi Lemon" + 100 個噪音
- **準確度**: 92%
- **模型大小**: 38 KB

## 更新日誌

### v1.0.3 (2024-12-02)
- 增加 50 個新訓練樣本
- 改進噪音抑制
- 準確度從 85% 提升到 92%

### v1.0.2 (2024-11-15)
- 初始版本
```

## 🎯 最佳實踐

### 1. 版本控制

```bash
# 為每個模型版本打標籤
git add components/lemong_wake
git commit -m "Update Edge Impulse model to v1.0.3"
git tag -a model-v1.0.3 -m "Model version 1.0.3"
```

### 2. 保留多個版本

```
components/
├── lemong_wake/              ← 當前使用
├── lemong_wake_v1.0.2/       ← 備份
└── lemong_wake_v1.0.3/       ← 最新
```

### 3. 測試清單

- [ ] 編譯成功
- [ ] 燒錄成功
- [ ] 系統啟動正常
- [ ] 喚醒詞檢測正常
- [ ] 準確度測試（至少 20 次）
- [ ] 誤觸發測試（至少 10 次）
- [ ] 長時間運行測試（1 小時）
- [ ] 記憶體使用正常
- [ ] 無記憶體洩漏

## 🐛 常見問題

### Q1: 新模型編譯失敗

**A**: 檢查：
1. 是否完整複製了三個資料夾
2. CMakeLists.txt 是否正確
3. ESP-IDF 版本是否兼容

### Q2: 模型太大無法編譯

**A**: 
1. 在 Edge Impulse 中啟用 EON Compiler
2. 使用 int8 量化
3. 減少模型複雜度
4. 增加分區大小

### Q3: 準確度下降

**A**:
1. 檢查採樣率是否匹配
2. 調整信心閾值
3. 收集更多訓練樣本
4. 檢查麥克風配置

### Q4: 推理時間太長

**A**:
1. 啟用 EON Compiler
2. 使用 int8 量化
3. 減少模型層數
4. 降低採樣率（如果可接受）

## 📚 相關文檔

- [EDGE_IMPULSE_SETUP.md](EDGE_IMPULSE_SETUP.md) - 如何訓練模型
- [EDGE_IMPULSE_INTEGRATION.md](EDGE_IMPULSE_INTEGRATION.md) - 整合說明
- [AUDIO_CONFIG_CHECK.md](AUDIO_CONFIG_CHECK.md) - 音頻配置檢查

## ✅ 快速檢查清單

更新模型前：
- [ ] 備份舊模型
- [ ] 記錄舊模型性能
- [ ] 準備測試計劃

更新過程：
- [ ] 從 Edge Impulse 導出模型
- [ ] 替換三個資料夾
- [ ] 檢查配置參數
- [ ] 更新程式碼（如需要）

更新後：
- [ ] 編譯成功
- [ ] 測試準確度
- [ ] 記錄性能數據
- [ ] 更新文檔
- [ ] 提交版本控制

---

**記住**: 每次更新模型都要充分測試！🎯
