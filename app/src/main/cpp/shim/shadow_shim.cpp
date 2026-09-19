#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <mutex>
#include <android/log.h>

#include "shadow_shim.h"

#define SHIM_TAG "AnxdoidShim"
#define SHIM_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, SHIM_TAG, __VA_ARGS__)
#define SHIM_LOGI(...) __android_log_print(ANDROID_LOG_INFO,  SHIM_TAG, __VA_ARGS__)
#define SHIM_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, SHIM_TAG, __VA_ARGS__)

namespace {

std::string g_rootfs_dir;
bool g_initialized = false;
std::mutex g_init_mutex;

// Function pointers to real libc functions
int (*real_open)(const char*, int, ...) = nullptr;
int (*real_openat)(int, const char*, int, ...) = nullptr;
int (*real_access)(const char*, int) = nullptr;
int (*real_faccessat)(int, const char*, int, int) = nullptr;
int (*real_stat)(const char*, struct stat*) = nullptr;
int (*real_lstat)(const char*, struct stat*) = nullptr;
int (*real_fstatat)(int, const char*, struct stat*, int) = nullptr;
ssize_t (*real_readlink)(const char*, char*, size_t) = nullptr;
ssize_t (*real_readlinkat)(int, const char*, char*, size_t) = nullptr;

uid_t (*real_getuid)() = nullptr;
uid_t (*real_geteuid)() = nullptr;
gid_t (*real_getgid)() = nullptr;
gid_t (*real_getegid)() = nullptr;

void ensure_init() {
    if (g_initialized) return;
    std::lock_guard<std::mutex> lock(g_init_mutex);
    if (g_initialized) return;

    const char* env_root = getenv("ANXDOID_ROOTFS");
    if (env_root && strlen(env_root) > 0) {
        g_rootfs_dir = env_root;
    } else {
        g_rootfs_dir = "/data/data/com.anxdoid.runtime/files/rootfs";
    }

    real_open = (int (*)(const char*, int, ...)) dlsym(RTLD_NEXT, "open");
    real_openat = (int (*)(int, const char*, int, ...)) dlsym(RTLD_NEXT, "openat");
    real_access = (int (*)(const char*, int)) dlsym(RTLD_NEXT, "access");
    real_faccessat = (int (*)(int, const char*, int, int)) dlsym(RTLD_NEXT, "faccessat");
    real_stat = (int (*)(const char*, struct stat*)) dlsym(RTLD_NEXT, "stat");
    real_lstat = (int (*)(const char*, struct stat*)) dlsym(RTLD_NEXT, "lstat");
    real_fstatat = (int (*)(int, const char*, struct stat*, int)) dlsym(RTLD_NEXT, "fstatat");
    real_readlink = (ssize_t (*)(const char*, char*, size_t)) dlsym(RTLD_NEXT, "readlink");
    real_readlinkat = (ssize_t (*)(int, const char*, char*, size_t)) dlsym(RTLD_NEXT, "readlinkat");

    real_getuid = (uid_t (*)()) dlsym(RTLD_NEXT, "getuid");
    real_geteuid = (uid_t (*)()) dlsym(RTLD_NEXT, "geteuid");
    real_getgid = (gid_t (*)()) dlsym(RTLD_NEXT, "getgid");
    real_getegid = (gid_t (*)()) dlsym(RTLD_NEXT, "getegid");

    g_initialized = true;
    SHIM_LOGI("Anxdoid Shadow Shim Initialized with ROOTFS: %s", g_rootfs_dir.c_str());
}

} // namespace

namespace anxdoid {

bool should_redirect_path(const char* path) {
    if (!path || path[0] != '/') return false;

    // Do not redirect if already inside rootfs
    if (strstr(path, g_rootfs_dir.c_str()) == path) {
        return false;
    }

    // Do not redirect dev, proc, sys pseudo filesystems
    if (strncmp(path, "/dev/", 5) == 0 && strncmp(path, "/dev/binder", 11) != 0) return false;
    if (strncmp(path, "/proc/", 6) == 0) return false;
    if (strncmp(path, "/sys/", 5) == 0) return false;

    // Redirect guest root paths
    if (strncmp(path, "/system", 7) == 0 ||
        strncmp(path, "/data", 5) == 0 ||
        strncmp(path, "/vendor", 7) == 0 ||
        strncmp(path, "/apex", 5) == 0 ||
        strncmp(path, "/etc", 4) == 0 ||
        strncmp(path, "/product", 8) == 0 ||
        strncmp(path, "/system_ext", 11) == 0) {
        return true;
    }

    return false;
}

std::string redirect_path(const char* path) {
    ensure_init();
    if (!should_redirect_path(path)) {
        return std::string(path);
    }
    return g_rootfs_dir + path;
}

void init_shim() {
    ensure_init();
}

} // namespace anxdoid

static inline bool open_needs_mode(int flags) {
#ifdef O_TMPFILE
    return (flags & O_CREAT) || ((flags & O_TMPFILE) == O_TMPFILE);
#else
    return (flags & O_CREAT) != 0;
#endif
}

#ifndef SYS_fstatat
#  if defined(__NR_newfstatat)
#    define SYS_fstatat __NR_newfstatat
#  elif defined(__NR_fstatat64)
#    define SYS_fstatat __NR_fstatat64
#  endif
#endif

// ============================================================================
// Libc Intercepted Functions (Path Redirection)
// ============================================================================

extern "C" {

__attribute__((visibility("default")))
int open(const char* pathname, int flags, ...) {
    ensure_init();
    mode_t mode = 0;
    if (open_needs_mode(flags)) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }

    std::string redirected = anxdoid::redirect_path(pathname);
    return real_open ? real_open(redirected.c_str(), flags, mode) : syscall(SYS_openat, AT_FDCWD, redirected.c_str(), flags, mode);
}

__attribute__((visibility("default")))
int openat(int dirfd, const char* pathname, int flags, ...) {
    ensure_init();
    mode_t mode = 0;
    if (open_needs_mode(flags)) {
        va_list args;
        va_start(args, flags);
        mode = static_cast<mode_t>(va_arg(args, int));
        va_end(args);
    }

    if (pathname && pathname[0] == '/') {
        std::string redirected = anxdoid::redirect_path(pathname);
        return real_openat ? real_openat(dirfd, redirected.c_str(), flags, mode) : syscall(SYS_openat, dirfd, redirected.c_str(), flags, mode);
    }

    return real_openat ? real_openat(dirfd, pathname, flags, mode) : syscall(SYS_openat, dirfd, pathname, flags, mode);
}

__attribute__((visibility("default")))
int access(const char* pathname, int mode) {
    ensure_init();
    std::string redirected = anxdoid::redirect_path(pathname);
    return real_access ? real_access(redirected.c_str(), mode) : syscall(SYS_faccessat, AT_FDCWD, redirected.c_str(), mode, 0);
}

__attribute__((visibility("default")))
int faccessat(int dirfd, const char* pathname, int mode, int flags) {
    ensure_init();
    if (pathname && pathname[0] == '/') {
        std::string redirected = anxdoid::redirect_path(pathname);
        return real_faccessat ? real_faccessat(dirfd, redirected.c_str(), mode, flags) : syscall(SYS_faccessat, dirfd, redirected.c_str(), mode, flags);
    }
    return real_faccessat ? real_faccessat(dirfd, pathname, mode, flags) : syscall(SYS_faccessat, dirfd, pathname, mode, flags);
}

__attribute__((visibility("default")))
int stat(const char* pathname, struct stat* statbuf) {
    ensure_init();
    std::string redirected = anxdoid::redirect_path(pathname);
    return real_stat ? real_stat(redirected.c_str(), statbuf) : syscall(SYS_fstatat, AT_FDCWD, redirected.c_str(), statbuf, 0);
}

__attribute__((visibility("default")))
int lstat(const char* pathname, struct stat* statbuf) {
    ensure_init();
    std::string redirected = anxdoid::redirect_path(pathname);
    return real_lstat ? real_lstat(redirected.c_str(), statbuf) : syscall(SYS_fstatat, AT_FDCWD, redirected.c_str(), statbuf, AT_SYMLINK_NOFOLLOW);
}

__attribute__((visibility("default")))
int fstatat(int dirfd, const char* pathname, struct stat* statbuf, int flags) {
    ensure_init();
    if (pathname && pathname[0] == '/') {
        std::string redirected = anxdoid::redirect_path(pathname);
        return real_fstatat ? real_fstatat(dirfd, redirected.c_str(), statbuf, flags) : syscall(SYS_fstatat, dirfd, redirected.c_str(), statbuf, flags);
    }
    return real_fstatat ? real_fstatat(dirfd, pathname, statbuf, flags) : syscall(SYS_fstatat, dirfd, pathname, statbuf, flags);
}

__attribute__((visibility("default")))
ssize_t readlink(const char* pathname, char* buf, size_t bufsiz) {
    ensure_init();
    std::string redirected = anxdoid::redirect_path(pathname);
    return real_readlink ? real_readlink(redirected.c_str(), buf, bufsiz) : syscall(SYS_readlinkat, AT_FDCWD, redirected.c_str(), buf, bufsiz);
}

__attribute__((visibility("default")))
ssize_t readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz) {
    ensure_init();
    if (pathname && pathname[0] == '/') {
        std::string redirected = anxdoid::redirect_path(pathname);
        return real_readlinkat ? real_readlinkat(dirfd, redirected.c_str(), buf, bufsiz) : syscall(SYS_readlinkat, dirfd, redirected.c_str(), buf, bufsiz);
    }
    return real_readlinkat ? real_readlinkat(dirfd, pathname, buf, bufsiz) : syscall(SYS_readlinkat, dirfd, pathname, buf, bufsiz);
}

// ============================================================================
// Libc Intercepted Functions (Fake Root & Privilege Neutralization)
// ============================================================================

__attribute__((visibility("default")))
int setuid(uid_t /* uid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int setgid(gid_t /* gid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int setreuid(uid_t /* ruid */, uid_t /* euid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int setregid(gid_t /* rgid */, gid_t /* egid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int setresuid(uid_t /* ruid */, uid_t /* euid */, uid_t /* suid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int setresgid(gid_t /* rgid */, gid_t /* egid */, gid_t /* sgid */) {
    return 0; // Success
}

__attribute__((visibility("default")))
int capset(void* /* hdrp */, const void* /* datap */) {
    return 0; // Fake capability drop success
}

__attribute__((visibility("default")))
uid_t getuid() {
    const char* fake_uid = getenv("ANXDOID_FAKE_UID");
    if (fake_uid) {
        return static_cast<uid_t>(atoi(fake_uid));
    }
    return 0; // Default to root
}

__attribute__((visibility("default")))
uid_t geteuid() {
    const char* fake_uid = getenv("ANXDOID_FAKE_UID");
    if (fake_uid) {
        return static_cast<uid_t>(atoi(fake_uid));
    }
    return 0; // Default to root
}

__attribute__((visibility("default")))
gid_t getgid() {
    return 0;
}

__attribute__((visibility("default")))
gid_t getegid() {
    return 0;
}

} // extern "C"
