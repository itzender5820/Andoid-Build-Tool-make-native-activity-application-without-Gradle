#include "MathUtils.h"
#include <cstring>

namespace gl {

Mat4 Mat4::identity() {
    Mat4 r;
    r.m[0]=r.m[5]=r.m[10]=r.m[15]=1.f;
    return r;
}

Mat4 Mat4::perspective(f32 fovY, f32 aspect, f32 near, f32 far) {
    Mat4 r;
    f32 f = 1.f / tanf(fovY * 0.5f);
    r.m[0]  = f / aspect;
    r.m[5]  = f;
    r.m[10] = (far + near) / (near - far);
    r.m[11] = -1.f;
    r.m[14] = (2.f * far * near) / (near - far);
    return r;
}

Mat4 Mat4::lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = (center - eye).normalized();
    Vec3 r = f.cross(up).normalized();
    Vec3 u = r.cross(f);
    Mat4 m;
    m.m[0]=r.x; m.m[4]=r.y; m.m[8] =r.z; m.m[12]=-r.dot(eye);
    m.m[1]=u.x; m.m[5]=u.y; m.m[9] =u.z; m.m[13]=-u.dot(eye);
    m.m[2]=-f.x;m.m[6]=-f.y;m.m[10]=-f.z;m.m[14]= f.dot(eye);
    m.m[15]=1.f;
    return m;
}

Mat4 Mat4::translate(Vec3 t) {
    Mat4 m = identity();
    m.m[12]=t.x; m.m[13]=t.y; m.m[14]=t.z;
    return m;
}

Mat4 Mat4::rotateY(f32 rad) {
    Mat4 m = identity();
    f32 c=cosf(rad), s=sinf(rad);
    m.m[0]=c; m.m[8]=s; m.m[2]=-s; m.m[10]=c;
    return m;
}

Mat4 Mat4::rotateX(f32 rad) {
    Mat4 m = identity();
    f32 c=cosf(rad), s=sinf(rad);
    m.m[5]=c; m.m[9]=-s; m.m[6]=s; m.m[10]=c;
    return m;
}

Mat4 Mat4::scale(Vec3 s) {
    Mat4 m = identity();
    m.m[0]=s.x; m.m[5]=s.y; m.m[10]=s.z;
    return m;
}

Mat4 Mat4::operator*(const Mat4& b) const {
    Mat4 r;
    for (int col=0; col<4; ++col)
        for (int row=0; row<4; ++row) {
            f32 v=0;
            for (int k=0; k<4; ++k)
                v += m[k*4+row] * b.m[col*4+k];
            r.m[col*4+row]=v;
        }
    return r;
}

Vec4 Mat4::operator*(Vec4 v) const {
    return {
        m[0]*v.x + m[4]*v.y + m[8] *v.z + m[12]*v.w,
        m[1]*v.x + m[5]*v.y + m[9] *v.z + m[13]*v.w,
        m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
        m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w,
    };
}

} // namespace gl
