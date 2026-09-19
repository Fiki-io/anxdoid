#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>

int main() {
    printf("[Guest Payload] Starting in Userspace Sandbox...\n");

    // 1. Verify getuid() spoofing
    uid_t uid = getuid();
    printf("[Guest Payload] Current UID: %d\n", static_cast<int>(uid));

    // 2. Test setuid() / setresuid() bypass
    int setuid_res = setuid(0);
    printf("[Guest Payload] setuid(0) result: %d (0 = success)\n", setuid_res);

    int setresuid_res = setresuid(1000, 1000, 1000);
    printf("[Guest Payload] setresuid(1000, 1000, 1000) result: %d\n", setresuid_res);

    if (setuid_res != 0 || setresuid_res != 0) {
        fprintf(stderr, "[Guest Payload] FAIL: setuid failed!\n");
        return 2;
    }

    // 3. Test transparent path redirection
    const char* test_path = "/system/build.prop";
    printf("[Guest Payload] Opening %s (should be intercepted to sandbox)...\n", test_path);
    FILE* fp = fopen(test_path, "r");
    if (!fp) {
        fprintf(stderr, "[Guest Payload] FAIL: Could not open %s\n", test_path);
        return 3;
    }

    char line[256];
    bool found_prop = false;
    while (fgets(line, sizeof(line), fp)) {
        printf("[Guest Payload] Read: %s", line);
        if (strstr(line, "ro.product.model=Anxdoid Virtual ARM64")) {
            found_prop = true;
        }
    }
    fclose(fp);

    if (!found_prop) {
        fprintf(stderr, "[Guest Payload] FAIL: Target build.prop property not found!\n");
        return 4;
    }

    printf("[Guest Payload] SUCCESS: All sandbox hooks verified!\n");
    return 0;
}
