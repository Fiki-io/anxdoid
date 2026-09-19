#include "process_launcher.h"
#include "../include/anxdoid_common.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <chrono>
#include <thread>

namespace anxdoid {

pid_t ProcessLauncher::launch(const LaunchConfig& config, std::string* error_out) {
    std::string rootfs = config.rootfs_dir;
    std::string target_bin = rootfs + config.target_binary;

    // Check if target binary exists
    if (access(target_bin.c_str(), X_OK) != 0 && access(target_bin.c_str(), F_OK) != 0) {
        if (error_out) *error_out = "Target binary does not exist or is not executable: " + target_bin;
        LOGE("ProcessLauncher: Target binary not found: %s", target_bin.c_str());
        return -1;
    }

    // Dynamic Linker path check
    std::string linker64_path = rootfs + "/system/bin/linker64";
    bool use_linker_trampoline = (access(linker64_path.c_str(), X_OK) == 0 || access(linker64_path.c_str(), F_OK) == 0);

    // Build argument list
    std::vector<std::string> arg_list;
    std::string exec_file;

    if (use_linker_trampoline) {
        exec_file = linker64_path;
        arg_list.push_back(linker64_path);
        arg_list.push_back("--library-path");
        std::string lib_path = rootfs + "/system/lib64:" + rootfs + "/system/lib";
        arg_list.push_back(lib_path);
        arg_list.push_back(target_bin);
    } else {
        exec_file = target_bin;
        arg_list.push_back(target_bin);
    }

    for (const auto& arg : config.args) {
        arg_list.push_back(arg);
    }

    // Prepare argv pointers
    std::vector<char*> argv_ptrs;
    for (const auto& a : arg_list) {
        argv_ptrs.push_back(const_cast<char*>(a.c_str()));
    }
    argv_ptrs.push_back(nullptr);

    // Build environment variables
    std::vector<std::string> env_list;
    env_list.push_back("ANXDOID_ROOTFS=" + rootfs);
    env_list.push_back("ANDROID_ROOT=" + rootfs + "/system");
    env_list.push_back("ANDROID_DATA=" + rootfs + "/data");
    env_list.push_back("PATH=" + rootfs + "/system/bin:" + rootfs + "/system/xbin:/sbin:/bin");
    env_list.push_back("TMPDIR=" + rootfs + "/data/local/tmp");

    if (!config.shim_library_path.empty()) {
        env_list.push_back("LD_PRELOAD=" + config.shim_library_path);
    }

    for (const auto& kv : config.env_vars) {
        env_list.push_back(kv.first + "=" + kv.second);
    }

    // Prepare envp pointers
    std::vector<char*> envp_ptrs;
    for (const auto& e : env_list) {
        envp_ptrs.push_back(const_cast<char*>(e.c_str()));
    }
    envp_ptrs.push_back(nullptr);

    LOGI("ProcessLauncher: Spawning guest process via %s", exec_file.c_str());
    for (size_t i = 0; i < arg_list.size(); ++i) {
        LOGD("  arg[%zu]: %s", i, arg_list[i].c_str());
    }

    pid_t pid = fork();
    if (pid < 0) {
        if (error_out) *error_out = std::string("fork() failed: ") + strerror(errno);
        LOGE("ProcessLauncher: fork() failed: %s", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        // Child Process
        execve(exec_file.c_str(), argv_ptrs.data(), envp_ptrs.data());
        // If execve returns, it must have failed
        fprintf(stderr, "execve failed for %s: %s\n", exec_file.c_str(), strerror(errno));
        _exit(127);
    }

    // Parent Process
    LOGI("ProcessLauncher: Child spawned with PID %d", pid);
    return pid;
}

int ProcessLauncher::wait_exit(pid_t pid, int timeout_seconds) {
    if (pid <= 0) return -1;

    int status = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (true) {
        pid_t res = waitpid(pid, &status, WNOHANG);
        if (res == pid) {
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            } else if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return 0;
        } else if (res < 0) {
            if (errno == ECHILD) return 0; // Already exited and reaped
            return -1;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start_time
        ).count();

        if (elapsed >= timeout_seconds) {
            LOGW("ProcessLauncher: Timeout waiting for PID %d", pid);
            return -ETIMEDOUT;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

} // namespace anxdoid
