#pragma once

#include <string>
#include <vector>
#include <sys/types.h>

namespace anxdoid {

struct LaunchConfig {
    std::string rootfs_dir;
    std::string target_binary; // Path inside rootfs (e.g., "/system/bin/toybox")
    std::vector<std::string> args;
    std::vector<std::pair<std::string, std::string>> env_vars;
    std::string shim_library_path;
    bool redirect_stdio = true;
};

class ProcessLauncher {
public:
    static pid_t launch(const LaunchConfig& config, std::string* error_out = nullptr);
    static int wait_exit(pid_t pid, int timeout_seconds = 10);
};

} // namespace anxdoid
