# Edge Impulse 整合完成

## 已完成的設定

### 1. 組件結構 ✓
已建立 `components/lemong_wake/` 組件，包含：
- Edge Impulse SDK
- 模型參數
- TensorFlow Lite 模型
- ESP-IDF 相容的 CMakeLists.txt

### 2. C++ 橋接器 ✓
已建立 `main/ei_wrapper.h` 和 `main/ei_wrapper.cpp`，提供 C 語言介面：
- `ei_wrapper_init()` - 初始化模型
- `ei_wrapper_run_inference()` - 執行推理
- `ei_wrapper_get_label()` - 取得分類名稱

### 3. 編譯設定 ✓
已更新 `main/CMakeLists.txt`：
- 加入 `ei_wrapper.cpp` 到源檔案
- 加入 `lemong_wake` 到依賴項

## 使用範例

```c
#include "ei_wrapper.h"

void app_main(void) {
    // 初始化模型
    ei_wrapper_init();
    
    // 準備音訊數據 (16-bit PCM, 16kHz)
    int16_t audio_buffer[16000];  // 1 秒音訊
    // ... 從麥克風讀取音訊數據 ...
    
    // 執行推理
    int result = ei_wrapper_run_inference(audio_buffer, 16000);
    
    if (result >= 0) {
        const char* label = ei_wrapper_get_label(result);
        printf("偵測到關鍵字: %s\n", label);
        
        if (strcmp(label, "hi_lemon") == 0) {
            // 處理 "hi_lemon" 喚醒詞
        }
    }
}
```

## 編譯專案

```bash
idf.py build
```

## 注意事項

1. **音訊格式**: 確保輸入的音訊數據是 16-bit PCM 格式
2. **採樣率**: 根據您的 Edge Impulse 模型設定（通常是 16kHz）
3. **數據長度**: `data_len` 參數是樣本數，不是字節數
4. **信心門檻**: 預設設定為 0.8 (80%)，可在 `ei_wrapper.cpp` 中調整

## 整合到現有程式

在您的 `hi_esp_keyword.c` 中：

```c
#include "ei_wrapper.h"

// 在初始化時
ei_wrapper_init();

// 在音訊處理迴圈中
int result = ei_wrapper_run_inference(audio_samples, sample_count);
if (result >= 0) {
    const char* keyword = ei_wrapper_get_label(result);
    // 處理偵測到的關鍵字
}
```

## 檔案清單

- `components/lemong_wake/CMakeLists.txt` - 組件編譯設定
- `components/lemong_wake/INTEGRATION.md` - 整合說明
- `main/ei_wrapper.h` - C 語言標頭檔
- `main/ei_wrapper.cpp` - C++ 實作檔
- `main/CMakeLists.txt` - 已更新，包含 lemong_wake 依賴

## 下一步

1. 編譯專案確認無錯誤
2. 整合到您的音訊處理流程
3. 調整信心門檻以獲得最佳偵測效果
4. 測試不同的喚醒詞
