# ── 03-audio-sink ─────────────────────────────────────────────────────────────
# Tests: AAudio stream, deep dep chain, multi-file linking, no GL.
# Dep chain:
#   AudioTypes → RingBuffer  →  Mixer   → AudioEngine → main
#             → Oscillator  ↗
#             → LowPassFilter → Effects ↗
#             → Compressor   ↗
#             → WaveWriter  → AudioEngine
#
#   abt build --64   (ARM64 device)
#   abt build --86   (x86_64 emulator)

abt_project(audio_sink)
abt_set_package(com.abt.test.audiosink)
abt_set_min_sdk(26)
abt_set_target_sdk(34)
abt_version(1.0 1)

abt_architectures(arm64-v8a x86_64)

abt_add_sources(
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    src/RingBuffer.cpp
    src/Oscillator.cpp
    src/LowPassFilter.cpp
    src/Compressor.cpp
    src/Effects.cpp
    src/Mixer.cpp
    src/WaveWriter.cpp
    src/AudioEngine.cpp
    src/main.cpp
)

abt_add_includes(
    include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

abt_link_libraries(
    aaudio
    android
    log
    m
)
