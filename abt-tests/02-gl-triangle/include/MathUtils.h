#pragma once
#include "GLTypes.h"
#include <cmath>

namespace gl {

// ── Vec3 ──────────────────────────────────────────────────────────────────────
struct Vec3 {
    f32 x=0, y=0, z=0;
    Vec3() = default;
    Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
    Vec3 operator+(Vec3 b) const { return {x+b.x, y+b.y, z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x, y-b.y, z-b.z}; }
    Vec3 operator*(f32 s)  const { return {x*s, y*s, z*s}; }
    f32  dot  (Vec3 b) const { return x*b.x + y*b.y + z*b.z; }
    Vec3 cross(Vec3 b) const {
        return {y*b.z - z*b.y, z*b.x - x*b.z, x*b.y - y*b.x};
    }
    f32  length()      const { return sqrtf(dot(*this)); }
    Vec3 normalized()  const { f32 l=length(); return l>1e-7f ? *this*(1.f/l) : *this; }
};

// ── Vec4 ──────────────────────────────────────────────────────────────────────
struct Vec4 {
    f32 x=0, y=0, z=0, w=1;
    Vec4() = default;
    Vec4(f32 x, f32 y, f32 z, f32 w=1) : x(x), y(y), z(z), w(w) {}
    Vec4(Vec3 v, f32 w=1)              : x(v.x), y(v.y), z(v.z), w(w) {}
};

// ── Mat4 — column-major (OpenGL convention) ────────────────────────────────
struct Mat4 {
    f32 m[16] {};  // m[col*4+row]

    static Mat4 identity();
    static Mat4 perspective(f32 fovYRad, f32 aspect, f32 near, f32 far);
    static Mat4 lookAt(Vec3 eye, Vec3 center, Vec3 up);
    static Mat4 translate(Vec3 t);
    static Mat4 rotateY(f32 rad);
    static Mat4 rotateX(f32 rad);
    static Mat4 scale(Vec3 s);

    Mat4 operator*(const Mat4& b) const;
    Vec4 operator*(Vec4 v)        const;
    const f32* ptr()              const { return m; }
};

// ── Utilities ─────────────────────────────────────────────────────────────────
constexpr f32 kPi    = 3.14159265358979323846f;
constexpr f32 kTwoPi = 6.28318530717958647692f;

inline f32 toRad(f32 deg) { return deg * kPi / 180.f; }
inline f32 lerp(f32 a, f32 b, f32 t) { return a + (b-a)*t; }
inline f32 clamp(f32 v, f32 lo, f32 hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

} // namespace gl
