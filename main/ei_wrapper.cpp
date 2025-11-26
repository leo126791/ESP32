#include "ei_wrapper.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "esp_log.h"

static const char *TAG = "EI_WRAPPER";

void ei_wrapper_init(void) {
    ESP_LOGI(TAG, "Edge Impulse 模型初始化完成");
}

int ei_wrapper_run_inference(int16_t *raw_data, size_t data_len) {
    if (data_len != EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
        ESP_LOGE(TAG, "輸入長度錯誤! 需要: %d, 收到: %d", 
                 EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, data_len);
        return -1;
    }

    // 建立訊號轉換結構 (將 int16 轉為 float)
    signal_t signal;
    signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    signal.get_data = [raw_data](size_t offset, size_t length, float *out_ptr) {
        for (size_t i = 0; i < length; i++) {
            out_ptr[i] = (float)raw_data[offset + i];
        }
        return 0;
    };

    ei_impulse_result_t result = { 0 };

    // 執行推理
    EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
    if (res != EI_IMPULSE_OK) {
        ESP_LOGE(TAG, "推理錯誤: %d", res);
        return -1;
    }

    // 找出最高分的分類
    int best_idx = -1;
    float best_score = 0.0f;
    for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        // 如果您想看每個分類的分數，取消下面這行的註釋
        // ESP_LOGI(TAG, "%s: %.2f", result.classification[i].label, result.classification[i].value);
        if (result.classification[i].value > best_score) {
            best_score = result.classification[i].value;
            best_idx = i;
        }
    }

    // 設定信心門檻 (0.8 = 80%)
    if (best_score > 0.8f) {
        return best_idx;
    }

    return -1; // 未偵測到或信心不足
}

const char* ei_wrapper_get_label(int label_index) {
    if (label_index >= 0 && label_index < EI_CLASSIFIER_LABEL_COUNT) {
        return ei_classifier_inferencing_categories[label_index];
    }
    return "Unknown";
}
