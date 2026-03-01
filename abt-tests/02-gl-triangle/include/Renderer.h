#pragma once
#include "ShaderProgram.h"
#include "Mesh.h"
#include "Camera.h"
#include "Texture.h"

namespace gl {

// ── Renderer ──────────────────────────────────────────────────────────────────
// Top-level rendering subsystem: owns shaders, scene meshes, camera, textures.
// Tests that all dep-chain headers compile and link together correctly.
class Renderer {
public:
    Renderer() = default;

    // Called once an EGL context + surface exist.
    bool init(int viewportW, int viewportH);
    void shutdown();

    // Called every frame.
    void draw(double elapsedSec);

    // Called on window resize.
    void resize(int w, int h);

    bool isReady() const { return ready_; }

private:
    bool buildShaders();
    void drawMesh(Mesh& mesh, const Mat4& model);

    ShaderProgram shader_;
    Mesh          triMesh_;
    Mesh          cubeMesh_;
    Mesh          sphereMesh_;
    Camera        camera_;
    Texture       checkerTex_;
    Texture       uvGridTex_;

    int  viewW_ = 0, viewH_ = 0;
    bool ready_ = false;

    // Rotation state
    float rotY_ = 0.f;
    float rotX_ = 0.f;
};

} // namespace gl
