# Edge Impulse 整合說明

## 組件結構

```
components/lemong_wake/
├── CMakeLists.txt              # ESP-IDF 組件編譯設定
├── edge-impulse-sdk/           # Edge Impulse SDK
├── model-parameters/           # 模型參數
├── tflite-model/              # TensorFlow Lite 模型
└── INTEGRATION.md             # 本文件
```

## 使用方式

### 1. 在您的 C 程式中引入標頭檔

```c
#include "ei_wrapper.h"
```

### 2. 初始化模型

```c
ei_wrapper_init();
```

### 3. 執行推理

```c
int16_t audio_buffer[16000];  // 假設 1 秒的 16kHz 音訊
// ... 填充音訊數據 ...

int result = ei_wrapper_run_inference(audio_buffer, 16000);
if (result >= 0) {
    const char* label = ei_wrapper_get_label(result);
    printf("偵測到: %s\n", label);
}
```

## API 說明

### `void ei_wrapper_init(void)`
初始化 Edge Impulse 模型。

### `int ei_wrapper_run_inference(int16_t *raw_data, size_t data_len)`
執行推理。
- **參數**:
  - `raw_data`: 16-bit PCM 音訊數據
  - `data_len`: 樣本數（不是字節數）
- **返回值**: 分類 ID（-1 表示未偵測到或信心不足）

### `const char* ei_wrapper_get_label(int label_index)`
取得分類名稱。
- **參數**: `label_index` - 分類 ID
- **返回值**: 分類名稱字串

## 編譯選項

組件已設定以下編譯選項以避免警告：
- `-Wno-error=format`
- `-Wno-error=type-limits`
- `-Wno-error=sign-compare`
- `-Wno-unused-parameter`
- `-Wno-missing-field-initializers`

## 依賴項

- `nvs_flash`: NVS 儲存
- `esp_timer`: 計時器
- `esp_dsp`: ESP DSP 加速庫
