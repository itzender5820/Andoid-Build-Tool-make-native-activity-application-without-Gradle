#include "Camera.h"
#include <cmath>

namespace gl {

Camera::Camera() { rebuildView(); rebuildProjection(); }

void Camera::setViewport(int w, int h) {
    aspect_ = w > 0 && h > 0 ? (f32)w / h : 1.f;
    rebuildProjection();
}

void Camera::orbit(f32 dAz, f32 dEl) {
    tAzimuth_   += dAz;
    tElevation_ = clamp(tElevation_ + dEl, -89.f, 89.f);
}

void Camera::zoom(f32 dd) {
    tDistance_ = clamp(tDistance_ + dd, 0.5f, 50.f);
}

void Camera::pan(f32 dx, f32 dy) {
    // Compute right and up vectors from current eye
    Vec3 forward = (target_ - eye_).normalized();
    Vec3 right   = forward.cross({0,1,0}).normalized();
    Vec3 up      = right.cross(forward);
    target_      = target_ + right * dx + up * dy;
}

void Camera::update(float dt) {
    // Smooth exponential approach
    f32 k = 1.f - expf(-10.f * dt);
    azimuth_   = lerp(azimuth_,   tAzimuth_,   k);
    elevation_ = lerp(elevation_, tElevation_, k);
    distance_  = lerp(distance_,  tDistance_,  k);
    rebuildView();
}

void Camera::rebuildView() {
    f32 az  = toRad(azimuth_);
    f32 el  = toRad(elevation_);
    f32 cosEl = cosf(el), sinEl = sinf(el);
    eye_ = target_ + Vec3{
        distance_ * cosEl * sinf(az),
        distance_ * sinEl,
        distance_ * cosEl * cosf(az)
    };
    view_ = Mat4::lookAt(eye_, target_, {0,1,0});
}

void Camera::rebuildProjection() {
    projection_ = Mat4::perspective(toRad(fovDeg_), aspect_, nearZ_, farZ_);
}

} // namespace gl
