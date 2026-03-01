#include "ShaderCompiler.h"
#include <vector>

namespace gl {

ShaderHandle ShaderCompiler::compileStage(GLenum type, const char* src) {
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &src, nullptr);
    glCompileShader(id);
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLOGE("Shader compile failed:\n%s", shaderLog({id}).c_str());
        glDeleteShader(id);
        return {0};
    }
    return {id};
}

ProgramHandle ShaderCompiler::linkProgram(ShaderHandle vert, ShaderHandle frag) {
    if (!vert.valid() || !frag.valid()) return {0};
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vert.id);
    glAttachShader(prog, frag.id);
    glLinkProgram(prog);
    glDetachShader(prog, vert.id);
    glDetachShader(prog, frag.id);
    glDeleteShader(vert.id);
    glDeleteShader(frag.id);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLOGE("Program link failed:\n%s", programLog({prog}).c_str());
        glDeleteProgram(prog);
        return {0};
    }
    GLOGI("Program linked: id=%u", prog);
    return {prog};
}

ProgramHandle ShaderCompiler::build(const char* vSrc, const char* fSrc) {
    return linkProgram(
        compileStage(GL_VERTEX_SHADER,   vSrc),
        compileStage(GL_FRAGMENT_SHADER, fSrc));
}

void ShaderCompiler::deleteShader (ShaderHandle&  h) { glDeleteShader(h.id);  h.id=0; }
void ShaderCompiler::deleteProgram(ProgramHandle& h) { glDeleteProgram(h.id); h.id=0; }

std::string ShaderCompiler::shaderLog(ShaderHandle h) {
    GLint len=0;
    glGetShaderiv(h.id, GL_INFO_LOG_LENGTH, &len);
    if (len<=1) return "";
    std::vector<char> buf(len);
    glGetShaderInfoLog(h.id, len, nullptr, buf.data());
    return {buf.data()};
}

std::string ShaderCompiler::programLog(ProgramHandle h) {
    GLint len=0;
    glGetProgramiv(h.id, GL_INFO_LOG_LENGTH, &len);
    if (len<=1) return "";
    std::vector<char> buf(len);
    glGetProgramInfoLog(h.id, len, nullptr, buf.data());
    return {buf.data()};
}

} // namespace gl
