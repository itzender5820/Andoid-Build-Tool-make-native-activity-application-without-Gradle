#include "Renderer.h"
#include <cmath>

namespace gl {

// ── GLSL sources ──────────────────────────────────────────────────────────────
static const char* VERT_SRC = R"(#version 300 es
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aColor;

uniform mat4 uMVP;
uniform mat4 uModel;
uniform mat4 uNormalMat;

out vec3 vNormal;
out vec2 vUV;
out vec4 vColor;
out vec3 vFragPos;

void main() {
    vec4 world = uModel * vec4(aPos, 1.0);
    vFragPos   = world.xyz;
    vNormal    = mat3(uNormalMat) * aNormal;
    vUV        = aUV;
    vColor     = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* FRAG_SRC = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec2 vUV;
in vec4 vColor;
in vec3 vFragPos;

uniform sampler2D uTex;
uniform vec3  uLightDir;
uniform float uTime;
uniform int   uUseTex;

out vec4 fragColor;

void main() {
    vec3 n    = normalize(vNormal);
    float ndl = max(dot(n, normalize(uLightDir)), 0.0);
    float amb = 0.2;
    float lit = amb + (1.0 - amb) * ndl;

    vec4 baseCol = uUseTex > 0 ? texture(uTex, vUV) : vColor;
    fragColor = vec4(baseCol.rgb * lit, baseCol.a);
}
)";

// ── Renderer ──────────────────────────────────────────────────────────────────
bool Renderer::init(int w, int h) {
    viewW_ = w; viewH_ = h;

    if (!buildShaders()) {
        GLOGE("Renderer: shader build failed");
        return false;
    }

    // Build geometry
    triMesh_.buildTriangle(1.5f);
    cubeMesh_.buildCube(1.2f);
    sphereMesh_.buildSphere(24, 24, 0.8f);

    // Build textures
    checkerTex_.generateCheckerboard(128, 16);
    uvGridTex_.generateUVGrid(256);

    // Camera setup
    camera_.setViewport(w, h);
    camera_.orbit(0, 20);
    camera_.zoom(-2.f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glViewport(0, 0, w, h);
    ready_ = true;
    GLOGI("Renderer ready: %dx%d", w, h);
    return true;
}

void Renderer::shutdown() {
    ready_ = false;
}

void Renderer::resize(int w, int h) {
    viewW_ = w; viewH_ = h;
    camera_.setViewport(w, h);
    glViewport(0, 0, w, h);
}

bool Renderer::buildShaders() {
    return shader_.build(VERT_SRC, FRAG_SRC);
}

void Renderer::drawMesh(Mesh& mesh, const Mat4& model) {
    Mat4 mvp = camera_.viewProj() * model;
    shader_.setMat4("uMVP",       mvp);
    shader_.setMat4("uModel",     model);
    shader_.setMat4("uNormalMat", model); // simplified (no scale)
    mesh.draw();
}

void Renderer::draw(double elapsed) {
    if (!ready_) return;

    float t = (float)elapsed;
    camera_.update(0.016f);

    rotY_ = t * 45.f;  // degrees/sec
    rotX_ = sinf(t * 0.5f) * 20.f;

    glClearColor(0.08f, 0.08f, 0.12f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shader_.use();
    shader_.setVec3 ("uLightDir", Vec3{0.7f, 1.f, 0.5f});
    shader_.setFloat("uTime",     t);

    // Triangle — left, no texture
    {
        shader_.setInt("uUseTex", 0);
        Mat4 model = Mat4::translate({-2.f, 0.f, 0.f}) *
                     Mat4::rotateY(toRad(rotY_));
        drawMesh(triMesh_, model);
    }

    // Cube — centre, checker texture
    {
        shader_.setInt("uUseTex", 1);
        checkerTex_.bind(0);
        shader_.setInt("uTex", 0);
        Mat4 model = Mat4::translate({0.f, 0.f, 0.f}) *
                     Mat4::rotateY(toRad(rotY_ * 0.7f)) *
                     Mat4::rotateX(toRad(rotX_));
        drawMesh(cubeMesh_, model);
        checkerTex_.unbind(0);
    }

    // Sphere — right, UV grid texture
    {
        shader_.setInt("uUseTex", 1);
        uvGridTex_.bind(0);
        shader_.setInt("uTex", 0);
        Mat4 model = Mat4::translate({2.f, 0.f, 0.f}) *
                     Mat4::rotateY(toRad(-rotY_ * 0.5f));
        drawMesh(sphereMesh_, model);
        uvGridTex_.unbind(0);
    }

    shader_.unuse();
}

} // namespace gl
