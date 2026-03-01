# ── 01-simple-native ──────────────────────────────────────────────────────────
# Tests: dependency graph depth, parallel compile, hash-cache, looper, input.
# Dep chain: Types → Logger → Config → EventQueue → InputHandler
#                                    ↘ LifecycleManager → StateManager → main
#
#   abt build --64   (ARM64)
#   abt build --86   (x86_64 emulator)

abt_project(simple_native)
abt_set_package(com.abt.test.simple)
abt_set_min_sdk(26)
abt_set_target_sdk(34)
abt_version(1.0 1)

abt_architectures(arm64-v8a x86_64)

abt_add_sources(
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    src/Logger.cpp
    src/Config.cpp
    src/Timer.cpp
    src/EventQueue.cpp
    src/InputHandler.cpp
    src/LifecycleManager.cpp
    src/StateManager.cpp
    src/main.cpp
)

abt_add_includes(
    include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

abt_link_libraries(
    android
    log
    m
)
