#pragma once

#include <android/log.h>
#include <cstdint>
#include <string>

#define LOG_TAG "AnxdoidNative"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace anxdoid {

struct RuntimeConfig {
    std::string base_dir;
    std::string rootfs_dir;
    std::string system_dir;
    std::string data_dir;
    bool is_initialized = false;
};

} // namespace anxdoid
