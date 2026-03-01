#include "ShaderProgram.h"

namespace gl {

ShaderProgram::~ShaderProgram() {
    if (prog_.valid()) ShaderCompiler::deleteProgram(prog_);
}

ShaderProgram::ShaderProgram(ShaderProgram&& o) noexcept
    : prog_(o.prog_), uniformCache_(std::move(o.uniformCache_)) {
    o.prog_ = {0};
}
ShaderProgram& ShaderProgram::operator=(ShaderProgram&& o) noexcept {
    if (prog_.valid()) ShaderCompiler::deleteProgram(prog_);
    prog_         = o.prog_;
    uniformCache_ = std::move(o.uniformCache_);
    o.prog_       = {0};
    return *this;
}

bool ShaderProgram::build(const char* vs, const char* fs) {
    if (prog_.valid()) ShaderCompiler::deleteProgram(prog_);
    uniformCache_.clear();
    prog_ = ShaderCompiler::build(vs, fs);
    return prog_.valid();
}

void ShaderProgram::use()   const { if(prog_.valid()) glUseProgram(prog_.id); }
void ShaderProgram::unuse() const { glUseProgram(0); }

GLint ShaderProgram::uniform(const std::string& name) {
    auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    GLint loc = glGetUniformLocation(prog_.id, name.c_str());
    uniformCache_[name] = loc;
    return loc;
}

void ShaderProgram::setInt  (const std::string& n, int  v) { glUniform1i(uniform(n), v); }
void ShaderProgram::setFloat(const std::string& n, f32  v) { glUniform1f(uniform(n), v); }
void ShaderProgram::setVec3 (const std::string& n, Vec3 v) { glUniform3f(uniform(n), v.x, v.y, v.z); }
void ShaderProgram::setVec4 (const std::string& n, Vec4 v) { glUniform4f(uniform(n), v.x, v.y, v.z, v.w); }
void ShaderProgram::setMat4 (const std::string& n, const Mat4& m) {
    glUniformMatrix4fv(uniform(n), 1, GL_FALSE, m.ptr());
}

GLint ShaderProgram::attribLocation(const char* n) const {
    return glGetAttribLocation(prog_.id, n);
}

} // namespace gl
