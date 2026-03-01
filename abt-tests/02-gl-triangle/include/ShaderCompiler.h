#pragma once
#include "GLTypes.h"
#include <string>

namespace gl {

// ── ShaderCompiler ────────────────────────────────────────────────────────────
// Compiles GLSL source strings into shader and program objects.
// All methods are static; no state is kept.
class ShaderCompiler {
public:
    // Compile a single shader stage; returns ShaderHandle{0} on failure.
    static ShaderHandle compileStage(GLenum type, const char* src);

    // Link vertex + fragment shaders into a program; deletes the stage objects.
    static ProgramHandle linkProgram(ShaderHandle vert, ShaderHandle frag);

    // Convenience: compile + link in one call.
    static ProgramHandle build(const char* vertSrc, const char* fragSrc);

    // Free a shader or program handle.
    static void deleteShader (ShaderHandle& h);
    static void deleteProgram(ProgramHandle& h);

    // Retrieve the info log for a shader or program (for diagnostics).
    static std::string shaderLog (ShaderHandle  h);
    static std::string programLog(ProgramHandle h);
};

} // namespace gl
