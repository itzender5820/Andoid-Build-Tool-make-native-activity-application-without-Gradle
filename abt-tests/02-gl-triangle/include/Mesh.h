#pragma once
#include "VertexBuffer.h"
#include "MathUtils.h"

namespace gl {

// ── Mesh ──────────────────────────────────────────────────────────────────────
// Owns a VertexBuffer and provides procedural geometry generators.
class Mesh {
public:
    Mesh() = default;

    // Procedural geometry builders
    void buildTriangle(f32 size = 1.f);
    void buildQuad    (f32 size = 1.f);
    void buildCube    (f32 size = 1.f);
    void buildSphere  (int latDiv = 16, int lonDiv = 16, f32 radius = 1.f);

    // Draw using the current shader program's attribute layout.
    void draw(GLenum mode = GL_TRIANGLES) const { vbo_.draw(mode); }

    // Transform matrix for this mesh instance.
    void setTransform(const Mat4& m) { transform_ = m; }
    const Mat4& transform() const    { return transform_; }

private:
    VertexBuffer vbo_;
    Mat4         transform_ = Mat4::identity();

    static Vertex makeVertex(Vec3 pos, Vec3 normal, f32 u, f32 v,
                              Vec4 color = {1,1,1,1});
};

} // namespace gl
