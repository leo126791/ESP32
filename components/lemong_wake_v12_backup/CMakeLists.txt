# Edge Impulse component for ESP-IDF

file(GLOB_RECURSE EDGE_IMPULSE_SOURCES
    "edge-impulse-sdk/classifier/*.cpp"
    "edge-impulse-sdk/dsp/*.cpp"
    "edge-impulse-sdk/porting/*.cpp"
    "edge-impulse-sdk/tensorflow/lite/micro/*.cpp"
    "edge-impulse-sdk/tensorflow/lite/micro/*.cc"
    "edge-impulse-sdk/tensorflow/lite/micro/kernels/*.cpp"
    "edge-impulse-sdk/tensorflow/lite/micro/kernels/*.cc"
    "edge-impulse-sdk/tensorflow/lite/micro/memory_planner/*.cc"
    "edge-impulse-sdk/tensorflow/lite/kernels/*.cc"
    "edge-impulse-sdk/tensorflow/lite/kernels/internal/*.cc"
    "edge-impulse-sdk/tensorflow/lite/c/*.c"
    "edge-impulse-sdk/tensorflow/lite/core/*.cc"
    "edge-impulse-sdk/tensorflow/lite/core/api/*.cc"
    "tflite-model/*.cpp"
    "model-parameters/*.cpp"
)

# Exclude problematic files
list(FILTER EDGE_IMPULSE_SOURCES EXCLUDE REGEX ".*debug_log.cpp$")
list(FILTER EDGE_IMPULSE_SOURCES EXCLUDE REGEX ".*micro_time.cpp$")
list(FILTER EDGE_IMPULSE_SOURCES EXCLUDE REGEX ".*_test\\.cc$")
list(FILTER EDGE_IMPULSE_SOURCES EXCLUDE REGEX ".*mock.*")
list(FILTER EDGE_IMPULSE_SOURCES EXCLUDE REGEX ".*fake_micro_context.*")

idf_component_register(
    SRCS ${EDGE_IMPULSE_SOURCES}
    INCLUDE_DIRS 
        "."
        "edge-impulse-sdk"
        "edge-impulse-sdk/third_party/flatbuffers/include"
        "edge-impulse-sdk/third_party/gemmlowp"
        "edge-impulse-sdk/third_party/ruy"
        "edge-impulse-sdk/CMSIS/Core/Include"
        "edge-impulse-sdk/CMSIS/DSP/Include"
        "model-parameters"
        "tflite-model"
    REQUIRES 
        esp_timer
)

# Add compiler flags to handle TensorFlow Lite warnings
target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wno-maybe-uninitialized
    -Wno-missing-field-initializers
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-deprecated-declarations
)
