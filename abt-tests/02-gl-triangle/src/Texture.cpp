#include "Texture.h"
#include <vector>
#include <cstdint>

namespace gl {

Texture::~Texture() {
    if (id_) { glDeleteTextures(1, &id_); id_ = 0; }
}

void Texture::allocate() {
    if (!id_) glGenTextures(1, &id_);
}

void Texture::setParams(GLenum minF, GLenum magF) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magF);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void Texture::upload(const uint8_t* rgba, int w, int h, GLenum minF, GLenum magF) {
    allocate();
    w_ = w; h_ = h;
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    setParams(minF, magF);
    if (minF == GL_LINEAR_MIPMAP_LINEAR || minF == GL_NEAREST_MIPMAP_NEAREST ||
        minF == GL_LINEAR_MIPMAP_NEAREST || minF == GL_NEAREST_MIPMAP_LINEAR)
        glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLOGI("Texture %dx%d uploaded (id=%u)", w, h, id_);
}

void Texture::generateCheckerboard(int size, int cellSize) {
    std::vector<uint8_t> px(size * size * 4);
    for (int y=0; y<size; ++y)
        for (int x=0; x<size; ++x) {
            bool on = ((x/cellSize) + (y/cellSize)) & 1;
            int idx = (y*size+x)*4;
            px[idx+0] = on ? 0xFF : 0x22;
            px[idx+1] = on ? 0xFF : 0x22;
            px[idx+2] = on ? 0xFF : 0x22;
            px[idx+3] = 0xFF;
        }
    upload(px.data(), size, size);
}

void Texture::generateGradient(int w, int h) {
    std::vector<uint8_t> px(w * h * 4);
    for (int x=0; x<w; ++x) {
        uint8_t r = (uint8_t)(x * 255 / (w-1));
        for (int y=0; y<h; ++y) {
            int idx = (y*w+x)*4;
            px[idx+0]=r; px[idx+1]=128; px[idx+2]=255-r; px[idx+3]=255;
        }
    }
    upload(px.data(), w, h, GL_LINEAR, GL_LINEAR);
}

void Texture::generateUVGrid(int size) {
    std::vector<uint8_t> px(size * size * 4);
    for (int y=0; y<size; ++y)
        for (int x=0; x<size; ++x) {
            int idx = (y*size+x)*4;
            bool bx = (x==0||x==size-1||x%(size/8)==0);
            bool by = (y==0||y==size-1||y%(size/8)==0);
            px[idx+0] = bx||by ? 0 : (uint8_t)(x*255/size);
            px[idx+1] = bx||by ? 0 : (uint8_t)(y*255/size);
            px[idx+2] = bx||by ? 0xFF : 0x40;
            px[idx+3] = 0xFF;
        }
    upload(px.data(), size, size);
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}
void Texture::unbind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace gl
