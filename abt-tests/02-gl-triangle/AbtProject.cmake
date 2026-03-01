# ── 02-gl-triangle ────────────────────────────────────────────────────────────
# Tests: deep dep chain, EGL setup, OpenGL ES 3.0, cross-file linking.
# Dep chain:
#   GLTypes → MathUtils → ShaderCompiler → ShaderProgram
#                       ↘ VertexBuffer → Mesh
#                       ↘ Texture         ↘ Renderer → EGLHelper → main
#                       ↘ Camera         ↗
#
#   abt build --64   (ARM64 device)
#   abt build --86   (x86_64 emulator)

abt_project(gl_triangle)
abt_set_package(com.abt.test.gltriangle)
abt_set_min_sdk(26)
abt_set_target_sdk(34)
abt_version(1.0 1)

abt_architectures(arm64-v8a x86_64)

abt_add_sources(
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c
    src/MathUtils.cpp
    src/ShaderCompiler.cpp
    src/ShaderProgram.cpp
    src/VertexBuffer.cpp
    src/Texture.cpp
    src/Mesh.cpp
    src/Camera.cpp
    src/Renderer.cpp
    src/EGLHelper.cpp
    src/main.cpp
)

abt_add_includes(
    include
    ${ANDROID_NDK}/sources/android/native_app_glue
)

abt_link_libraries(
    android
    log
    EGL
    GLESv3
    m
)
