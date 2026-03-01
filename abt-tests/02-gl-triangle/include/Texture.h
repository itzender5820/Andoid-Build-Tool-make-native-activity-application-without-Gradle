#pragma once
#include "GLTypes.h"

namespace gl {

// ── Texture ───────────────────────────────────────────────────────────────────
// 2D texture wrapper with procedural generation (no stb_image dependency).
class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Upload raw RGBA pixels.
    void upload(const uint8_t* rgba, int w, int h,
                GLenum minFilter = GL_LINEAR_MIPMAP_LINEAR,
                GLenum magFilter = GL_LINEAR);

    // Generate a procedural checkerboard.
    void generateCheckerboard(int size = 64, int cellSize = 8);

    // Generate a gradient.
    void generateGradient(int w = 256, int h = 1);

    // Generate UV debug grid.
    void generateUVGrid(int size = 128);

    void bind  (int unit = 0) const;
    void unbind(int unit = 0) const;

    int width()  const { return w_; }
    int height() const { return h_; }
    bool valid() const { return id_ != 0; }

private:
    GLuint id_ = 0;
    int    w_  = 0;
    int    h_  = 0;

    void allocate();
    void setParams(GLenum minF, GLenum magF);
};

} // namespace gl
