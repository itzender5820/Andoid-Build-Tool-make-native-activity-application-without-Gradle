#pragma once
#include <GLES3/gl3.h>
#include <android/log.h>
#include <cstdint>
#include <string>

#define GL_TAG "gl.tri"
#define GLOGI(...) __android_log_print(ANDROID_LOG_INFO,  GL_TAG, __VA_ARGS__)
#define GLOGE(...) __android_log_print(ANDROID_LOG_ERROR, GL_TAG, __VA_ARGS__)
#define GLOGD(...) __android_log_print(ANDROID_LOG_DEBUG, GL_TAG, __VA_ARGS__)

namespace gl {

// ── GL error checker ──────────────────────────────────────────────────────────
// Usage: GL_CHECK(glDrawArrays(...));
inline bool _glCheckError(const char* call, const char* file, int line) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return true;
    GLOGE("GL error 0x%04x in %s (%s:%d)", err, call, file, line);
    return false;
}
#define GL_CHECK(call) (call, gl::_glCheckError(#call, __FILE__, __LINE__))

// ── Primitive types ───────────────────────────────────────────────────────────
using u32 = uint32_t;
using i32 = int32_t;
using f32 = float;

// ── Handle wrappers ───────────────────────────────────────────────────────────
struct ShaderHandle { GLuint id = 0; bool valid() const { return id != 0; } };
struct ProgramHandle{ GLuint id = 0; bool valid() const { return id != 0; } };
struct VaoHandle    { GLuint id = 0; bool valid() const { return id != 0; } };
struct VboHandle    { GLuint id = 0; bool valid() const { return id != 0; } };
struct TexHandle    { GLuint id = 0; bool valid() const { return id != 0; } };

} // namespace gl
