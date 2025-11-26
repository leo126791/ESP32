#ifndef EI_WRAPPER_H
#define EI_WRAPPER_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 初始化模型
void ei_wrapper_init(void);

// 執行推理
// raw_data: 16-bit PCM 音訊數據
// data_len: 樣本數 (不是字節數)
// 返回值: 識別到的分類 ID (-1 表示未知/噪音)
int ei_wrapper_run_inference(int16_t *raw_data, size_t data_len);

// 取得分類名稱 (例如 "hi_lemon")
const char* ei_wrapper_get_label(int label_index);

#ifdef __cplusplus
}
#endif

#endif
