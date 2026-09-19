cmake_minimum_required(VERSION 3.22.1)

project("anxdoid_runtime" LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/shim
)

# 1. The Shadow Interceptor Shim (injected via LD_PRELOAD)
add_library(
    anxdoid_shim
    SHARED
    shim/shadow_shim.cpp
)

target_link_libraries(
    anxdoid_shim
    log
    dl
)

# 2. The Main Runtime Engine (JNI bridge for Android App)
add_library(
    anxdoid_runtime
    SHARED
    runtime/native_bridge.cpp
    launcher/process_launcher.cpp
)

target_link_libraries(
    anxdoid_runtime
    log
    android
)

# 3. Standalone Test Runner (for CI/CD automated verification)
add_executable(
    anxdoid_test_runner
    test/test_runner.cpp
    launcher/process_launcher.cpp
)

target_link_libraries(
    anxdoid_test_runner
    log
)

# 4. Guest Test Payload (runs inside the guest sandbox during CI test)
add_executable(
    guest_payload
    test/guest_payload.cpp
)

target_link_libraries(
    guest_payload
    log
)
