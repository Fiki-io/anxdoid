#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/input.h>
#include <mutex>
#include <memory>
#include <string>

#include "anxdoid_common.h"
#include "../launcher/process_launcher.h"

namespace {
    std::mutex g_state_mutex;
    anxdoid::RuntimeConfig g_config;
    ANativeWindow* g_native_window = nullptr;
    pid_t g_guest_pid = -1;
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_initRuntime(
    JNIEnv* env,
    jobject /* thiz */,
    jstring rootDir,
    jstring rootfsDir) {
    
    std::lock_guard<std::mutex> lock(g_state_mutex);

    const char* c_root = env->GetStringUTFChars(rootDir, nullptr);
    const char* c_rootfs = env->GetStringUTFChars(rootfsDir, nullptr);

    if (!c_root || !c_rootfs) {
        LOGE("Failed to parse directory strings from JNI");
        if (c_root) env->ReleaseStringUTFChars(rootDir, c_root);
        if (c_rootfs) env->ReleaseStringUTFChars(rootfsDir, c_rootfs);
        return JNI_FALSE;
    }

    g_config.base_dir = c_root;
    g_config.rootfs_dir = c_rootfs;
    g_config.system_dir = g_config.rootfs_dir + "/system";
    g_config.data_dir = g_config.rootfs_dir + "/data";
    g_config.is_initialized = true;

    env->ReleaseStringUTFChars(rootDir, c_root);
    env->ReleaseStringUTFChars(rootfsDir, c_rootfs);

    LOGI("Anxdoid Native Runtime Initialized.");
    LOGI("  Base dir  : %s", g_config.base_dir.c_str());
    LOGI("  Rootfs dir: %s", g_config.rootfs_dir.c_str());
    LOGI("  System dir: %s", g_config.system_dir.c_str());

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_setSurface(
    JNIEnv* env,
    jobject /* thiz */,
    jobject surface) {
    
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_native_window != nullptr) {
        ANativeWindow_release(g_native_window);
        g_native_window = nullptr;
    }

    if (surface != nullptr) {
        g_native_window = ANativeWindow_fromSurface(env, surface);
        if (g_native_window != nullptr) {
            int32_t width = ANativeWindow_getWidth(g_native_window);
            int32_t height = ANativeWindow_getHeight(g_native_window);
            int32_t format = ANativeWindow_getFormat(g_native_window);
            LOGI("ANativeWindow acquired: %dx%d (format: %d)", width, height, format);
            return JNI_TRUE;
        } else {
            LOGE("Failed to obtain ANativeWindow from Surface");
            return JNI_FALSE;
        }
    }

    return JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_clearSurface(
    JNIEnv* /* env */,
    jobject /* thiz */) {
    
    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_native_window != nullptr) {
        ANativeWindow_release(g_native_window);
        g_native_window = nullptr;
        LOGI("ANativeWindow released");
    }
}

JNIEXPORT jboolean JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_sendTouchEvent(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint action,
    jfloat x,
    jfloat y,
    jfloat pressure) {

    // Serializes Android MotionEvent into standard Linux input_event
    // For Milestone 1, log high-level touch event; in Milestone 5 this connects to the input pipe
    LOGD("TouchEvent: action=%d, pos=(%.1f, %.1f), pressure=%.2f", action, x, y, pressure);
    return JNI_TRUE;
}

JNIEXPORT jint JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_startGuestRuntime(
    JNIEnv* /* env */,
    jobject /* thiz */) {

    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (!g_config.is_initialized) {
        LOGE("Cannot start guest runtime: not initialized!");
        return -1;
    }

    if (g_guest_pid > 0) {
        LOGW("Guest runtime is already running with PID: %d", g_guest_pid);
        return g_guest_pid;
    }

    LOGI("Preparing to start Userspace ARM64 Guest Runtime...");

    std::string rootfs_system = g_config.system_dir;
    if (access(rootfs_system.c_str(), F_OK) != 0) {
        LOGW("Rootfs system directory not found at: %s", rootfs_system.c_str());
        LOGW("Please extract an ARM64 AOSP GSI into rootfs to launch full Android.");
        return -2;
    }

    // Determine target binary: priority app_process64 -> sh -> toybox
    std::string target_bin = "/system/bin/app_process64";
    if (access((g_config.rootfs_dir + target_bin).c_str(), F_OK) != 0) {
        target_bin = "/system/bin/sh";
        if (access((g_config.rootfs_dir + target_bin).c_str(), F_OK) != 0) {
            target_bin = "/system/bin/toybox";
        }
    }

    anxdoid::LaunchConfig launch_cfg;
    launch_cfg.rootfs_dir = g_config.rootfs_dir;
    launch_cfg.target_binary = target_bin;
    launch_cfg.shim_library_path = g_config.base_dir + "/lib/libanxdoid_shim.so";

    if (target_bin == "/system/bin/app_process64") {
        launch_cfg.args = {
            "/system/bin",
            "--application",
            "--nice-name=anxdoid_system_server",
            "com.android.server.SystemServer"
        };
    }

    std::string err;
    pid_t pid = anxdoid::ProcessLauncher::launch(launch_cfg, &err);
    if (pid > 0) {
        g_guest_pid = pid;
        LOGI("Guest runtime successfully launched with PID: %d", pid);
        return pid;
    } else {
        LOGE("Failed to launch guest runtime: %s", err.c_str());
        return -1;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_anxdoid_runtime_core_NativeBridge_stopGuestRuntime(
    JNIEnv* /* env */,
    jobject /* thiz */) {

    std::lock_guard<std::mutex> lock(g_state_mutex);

    if (g_guest_pid > 0) {
        LOGI("Terminating guest runtime process PID: %d", g_guest_pid);
        kill(g_guest_pid, SIGTERM);
        int status = 0;
        waitpid(g_guest_pid, &status, WNOHANG);
        g_guest_pid = -1;
        return JNI_TRUE;
    }

    return JNI_FALSE;
}

} // extern "C"
