#pragma once
#include "MathUtils.h"

namespace gl {

// ── Camera ────────────────────────────────────────────────────────────────────
// Perspective camera with orbit controls and view/projection matrix accessors.
class Camera {
public:
    Camera();

    // Set viewport for projection.
    void setViewport(int w, int h);

    // Orbit controls: modify azimuth/elevation/distance.
    void orbit   (f32 dAzimuth, f32 dElevation);
    void zoom    (f32 dDistance);
    void pan     (f32 dx, f32 dy);

    // Called every frame for smooth lerp.
    void update(float dt);

    const Mat4& view()       const { return view_;       }
    const Mat4& projection() const { return projection_; }
    Mat4        viewProj()   const { return projection_ * view_; }

    Vec3 position()  const { return eye_; }
    Vec3 target()    const { return target_; }

    // Frustum: field of view in degrees.
    void setFov(f32 deg) { fovDeg_ = deg; rebuildProjection(); }

private:
    void rebuildView();
    void rebuildProjection();

    Vec3  target_   {};
    f32   azimuth_  = 0.f;       // degrees
    f32   elevation_= 30.f;      // degrees
    f32   distance_ = 5.f;
    f32   fovDeg_   = 60.f;
    f32   aspect_   = 1.f;
    f32   nearZ_    = 0.1f;
    f32   farZ_     = 100.f;

    // Smooth-lerp targets
    f32 tAzimuth_ = 0.f, tElevation_ = 30.f, tDistance_ = 5.f;

    Vec3 eye_;
    Mat4 view_;
    Mat4 projection_;
};

} // namespace gl
