#pragma once
#include "ShaderCompiler.h"
#include "MathUtils.h"
#include <string>
#include <unordered_map>

namespace gl {

// ── ShaderProgram ─────────────────────────────────────────────────────────────
// Wraps a linked GLSL program with typed uniform setters and attribute binding.
class ShaderProgram {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    ShaderProgram(const ShaderProgram&) = delete;
    ShaderProgram& operator=(const ShaderProgram&) = delete;
    ShaderProgram(ShaderProgram&&) noexcept;
    ShaderProgram& operator=(ShaderProgram&&) noexcept;

    // Build from source strings; returns false on compile/link failure.
    bool build(const char* vertSrc, const char* fragSrc);

    void use() const;
    void unuse() const;
    bool valid() const { return prog_.valid(); }

    // Typed uniform setters (cache location on first call).
    void setInt  (const std::string& name, int   v);
    void setFloat(const std::string& name, f32   v);
    void setVec3 (const std::string& name, Vec3  v);
    void setVec4 (const std::string& name, Vec4  v);
    void setMat4 (const std::string& name, const Mat4& m);

    GLint attribLocation(const char* name) const;

private:
    GLint uniform(const std::string& name);

    ProgramHandle prog_;
    mutable std::unordered_map<std::string, GLint> uniformCache_;
};

} // namespace gl
