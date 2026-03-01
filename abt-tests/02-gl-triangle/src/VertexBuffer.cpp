#include "VertexBuffer.h"
#include <cstddef>  // offsetof

namespace gl {

VertexBuffer::~VertexBuffer() {
    if (ebo_) glDeleteBuffers(1, &ebo_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void VertexBuffer::upload(const std::vector<Vertex>& verts,
                           const std::vector<u32>& indices) {
    if (!vao_) glGenVertexArrays(1, &vao_);
    if (!vbo_) glGenBuffers(1, &vbo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(Vertex),
                 verts.data(), GL_STATIC_DRAW);
    vertCount_ = (u32)verts.size();

    setupAttribs();

    if (!indices.empty()) {
        if (!ebo_) glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indices.size() * sizeof(u32),
                     indices.data(), GL_STATIC_DRAW);
        idxCount_ = (u32)indices.size();
    }

    glBindVertexArray(0);
    GLOGI("VBO uploaded: %u verts, %u indices", vertCount_, idxCount_);
}

void VertexBuffer::update(const std::vector<Vertex>& verts) {
    if (!vbo_) return;
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    verts.size() * sizeof(Vertex), verts.data());
    vertCount_ = (u32)verts.size();
}

void VertexBuffer::bind()   const { glBindVertexArray(vao_); }
void VertexBuffer::unbind() const { glBindVertexArray(0);    }

void VertexBuffer::draw(GLenum mode) const {
    glBindVertexArray(vao_);
    if (hasIndices())
        glDrawElements(mode, idxCount_, GL_UNSIGNED_INT, nullptr);
    else
        glDrawArrays(mode, 0, vertCount_);
    glBindVertexArray(0);
}

void VertexBuffer::setupAttribs() {
    constexpr GLsizei stride = sizeof(Vertex);
    // pos (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(Vertex, pos)));
    // normal (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(Vertex, normal)));
    // uv (location 2)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(Vertex, uv)));
    // color (location 3)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride,
        reinterpret_cast<void*>(offsetof(Vertex, color)));
}

} // namespace gl
