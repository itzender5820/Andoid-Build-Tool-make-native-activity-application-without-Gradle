#pragma once
#include "GLTypes.h"
#include <vector>

namespace gl {

// ── Vertex layout ─────────────────────────────────────────────────────────────
struct Vertex {
    f32 pos[3];
    f32 normal[3];
    f32 uv[2];
    f32 color[4];
};

// ── VertexBuffer ──────────────────────────────────────────────────────────────
// Manages one VAO + VBO pair with interleaved Vertex data.
class VertexBuffer {
public:
    VertexBuffer() = default;
    ~VertexBuffer();

    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    // Upload vertex data (and optionally index data).
    void upload(const std::vector<Vertex>& verts,
                const std::vector<u32>&   indices = {});

    // Re-upload changed vertex data (same size or smaller).
    void update(const std::vector<Vertex>& verts);

    void bind()   const;
    void unbind() const;

    // Draw call helpers
    void draw(GLenum mode = GL_TRIANGLES) const;

    u32 vertexCount() const { return vertCount_; }
    u32 indexCount()  const { return idxCount_;  }
    bool hasIndices() const { return ebo_ != 0;  }

    // Set up standard attribute pointers (pos=0, normal=1, uv=2, color=3).
    static void setupAttribs();

private:
    GLuint vao_      = 0;
    GLuint vbo_      = 0;
    GLuint ebo_      = 0;
    u32    vertCount_= 0;
    u32    idxCount_ = 0;
};

} // namespace gl
