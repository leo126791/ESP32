# components/lemong_wake/CMakeLists.txt

# 1. 設定來源檔案 (遞迴搜尋所有必要的 .c, .cpp, .cc, .S 檔案)
file(GLOB_RECURSE SOURCES
    "edge-impulse-sdk/dsp/*.c"
    "edge-impulse-sdk/dsp/*.cpp"
    "edge-impulse-sdk/dsp/*.cc"
    "edge-impulse-sdk/tensorflow/*.c"
    "edge-impulse-sdk/tensorflow/*.cpp"
    "edge-impulse-sdk/tensorflow/*.cc"
    "edge-impulse-sdk/CMSIS/*.c"
    "edge-impulse-sdk/CMSIS/*.cpp"
    "edge-impulse-sdk/CMSIS/*.cc"
    "edge-impulse-sdk/porting/*.c"
    "edge-impulse-sdk/porting/*.cpp"
    "edge-impulse-sdk/porting/*.cc"
    "edge-impulse-sdk/porting/*.S"      # 包含 ESP-NN 的組合語言優化檔
    "edge-impulse-sdk/classifier/*.c"
    "edge-impulse-sdk/classifier/*.cpp"
    "edge-impulse-sdk/classifier/*.cc"
    "model-parameters/*.c"
    "model-parameters/*.cpp"
    "model-parameters/*.cc"
    "tflite-model/*.c"
    "tflite-model/*.cpp"
    "tflite-model/*.cc"
)

# 注意：現在使用 esp-dsp 組件，不再排除相關檔案

# 2. 註冊組件
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/dsp"
        "edge-impulse-sdk/porting"
        "edge-impulse-sdk/classifier"
        "edge-impulse-sdk/tensorflow"
        "edge-impulse-sdk/third_party"
        "edge-impulse-sdk/third_party/flatbuffers/include"
        "edge-impulse-sdk/third_party/gemmlowp"
        "edge-impulse-sdk/third_party/ruy"
        "model-parameters"
        "tflite-model"
    REQUIRES nvs_flash esp_timer esp-dsp
)

# 定義編譯宏以啟用 ESP-DSP
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    EIDSP_USE_CMSIS_DSP=0
    EIDSP_USE_ESP_DSP=1
    EI_CLASSIFIER_TFLITE_ENABLE_ESP_NN=0
)

# 3. 設定編譯選項 (關閉警告，強制 C++17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-error=format
    -Wno-error=type-limits
    -Wno-error=sign-compare
    -Wno-error=maybe-uninitialized
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-unused-function
    -Wno-unused-variable
)
