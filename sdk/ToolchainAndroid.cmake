# ToolchainAndroid.cmake
# Reference: maps abt arch names → clang triples and NDK sysroot layout.
# This file is informational — abt uses NDKBridge.cpp at runtime.
# It can also be used directly with CMake for IDE integration:
#   cmake -DCMAKE_TOOLCHAIN_FILE=sdk/ToolchainAndroid.cmake ..

if(NOT DEFINED ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()
if(NOT DEFINED ANDROID_PLATFORM)
    set(ANDROID_PLATFORM "android-24")
endif()
if(NOT DEFINED ANDROID_NDK)
    set(ANDROID_NDK $ENV{ANDROID_NDK_HOME})
endif()

# ── Map ABI → clang triple ────────────────────────────────────────────────────
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(ANDROID_LLVM_TRIPLE "aarch64-linux-android")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
elseif(ANDROID_ABI STREQUAL "x86_64")
    set(ANDROID_LLVM_TRIPLE "x86_64-linux-android")
    set(CMAKE_SYSTEM_PROCESSOR "x86_64")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(ANDROID_LLVM_TRIPLE "armv7a-linux-androideabi")
    set(CMAKE_SYSTEM_PROCESSOR "armv7-a")
elseif(ANDROID_ABI STREQUAL "x86")
    set(ANDROID_LLVM_TRIPLE "i686-linux-android")
    set(CMAKE_SYSTEM_PROCESSOR "i686")
else()
    message(FATAL_ERROR "Unsupported ANDROID_ABI: ${ANDROID_ABI}")
endif()

# ── Extract API level from platform string ────────────────────────────────────
string(REPLACE "android-" "" ANDROID_API_LEVEL "${ANDROID_PLATFORM}")

# ── Toolchain paths ───────────────────────────────────────────────────────────
set(ANDROID_TOOLCHAIN_ROOT
    "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64")
set(ANDROID_SYSROOT
    "${ANDROID_TOOLCHAIN_ROOT}/sysroot")

set(CMAKE_C_COMPILER
    "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_LLVM_TRIPLE}${ANDROID_API_LEVEL}-clang")
set(CMAKE_CXX_COMPILER
    "${ANDROID_TOOLCHAIN_ROOT}/bin/${ANDROID_LLVM_TRIPLE}${ANDROID_API_LEVEL}-clang++")
set(CMAKE_ASM_COMPILER
    "${CMAKE_C_COMPILER}")

set(CMAKE_SYSROOT "${ANDROID_SYSROOT}")
set(CMAKE_SYSTEM_NAME "Android")
set(CMAKE_SYSTEM_VERSION "${ANDROID_API_LEVEL}")

# ── Default flags ─────────────────────────────────────────────────────────────
set(ANDROID_COMPILER_FLAGS
    "-fPIC -ffunction-sections -fdata-sections -fstack-protector-strong")

if(ANDROID_ABI STREQUAL "arm64-v8a")
    string(APPEND ANDROID_COMPILER_FLAGS " -march=armv8-a+simd")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    string(APPEND ANDROID_COMPILER_FLAGS " -march=armv7-a -mfpu=neon -mfloat-abi=softfp")
endif()

set(CMAKE_C_FLAGS_INIT   "${ANDROID_COMPILER_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ANDROID_COMPILER_FLAGS}")

set(CMAKE_EXE_LINKER_FLAGS_INIT    "-Wl,--gc-sections -Wl,-z,nocopyreloc")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
