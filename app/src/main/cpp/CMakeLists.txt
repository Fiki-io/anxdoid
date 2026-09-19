cmake_minimum_required(VERSION 3.22.1)

project("anxdoid_runtime" LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(
    anxdoid_runtime
    SHARED
    runtime/native_bridge.cpp
)

target_link_libraries(
    anxdoid_runtime
    log
    android
)
