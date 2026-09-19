#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "../launcher/process_launcher.h"
#include "../include/anxdoid_common.h"

int main(int argc, char** argv) {
    printf("=== Anxdoid Native Test Runner ===\n");

    std::string rootfs_dir = "./test_rootfs";
    std::string shim_path = "./libanxdoid_shim.so";
    std::string target_bin = "/system/bin/toybox";
    std::vector<std::string> target_args;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--rootfs") == 0 && i + 1 < argc) {
            rootfs_dir = argv[++i];
        } else if (strcmp(argv[i], "--shim") == 0 && i + 1 < argc) {
            shim_path = argv[++i];
        } else if (strcmp(argv[i], "--exec") == 0 && i + 1 < argc) {
            target_bin = argv[++i];
        } else {
            target_args.push_back(argv[i]);
        }
    }

    printf("Configuration:\n");
    printf("  Rootfs Dir: %s\n", rootfs_dir.c_str());
    printf("  Shim Path : %s\n", shim_path.c_str());
    printf("  Target Bin: %s\n", target_bin.c_str());

    anxdoid::LaunchConfig config;
    config.rootfs_dir = rootfs_dir;
    config.target_binary = target_bin;
    config.args = target_args;
    config.shim_library_path = shim_path;

    std::string error;
    pid_t pid = anxdoid::ProcessLauncher::launch(config, &error);
    if (pid < 0) {
        fprintf(stderr, "TEST FAILED: Failed to launch process: %s\n", error.c_str());
        return 1;
    }

    printf("Guest process spawned successfully (PID: %d). Waiting for completion...\n", pid);
    int exit_code = anxdoid::ProcessLauncher::wait_exit(pid, 15);
    printf("Guest process exited with code: %d\n", exit_code);

    if (exit_code == 0) {
        printf("=== ALL CHECKS PASSED: TRAMPOLINE & SHIM TEST SUCCESSFUL ===\n");
        return 0;
    } else {
        fprintf(stderr, "=== TEST FAILED: Process exited with non-zero code %d ===\n", exit_code);
        return exit_code;
    }
}
