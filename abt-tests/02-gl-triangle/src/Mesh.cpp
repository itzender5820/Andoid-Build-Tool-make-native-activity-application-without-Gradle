#include "Mesh.h"
#include <cmath>

namespace gl {

Vertex Mesh::makeVertex(Vec3 pos, Vec3 n, f32 u, f32 v, Vec4 col) {
    return {
        {pos.x, pos.y, pos.z},
        {n.x,   n.y,   n.z  },
        {u,     v            },
        {col.x, col.y, col.z, col.w}
    };
}

void Mesh::buildTriangle(f32 s) {
    f32 h = s * 0.866f;
    Vec3 n = {0,0,1};
    std::vector<Vertex> verts = {
        makeVertex({0.f,     h*0.667f, 0}, n, 0.5f, 1.f, {1,0,0,1}),
        makeVertex({-s*.5f,-h*0.333f, 0}, n, 0.f,  0.f, {0,1,0,1}),
        makeVertex({ s*.5f,-h*0.333f, 0}, n, 1.f,  0.f, {0,0,1,1}),
    };
    vbo_.upload(verts);
}

void Mesh::buildQuad(f32 s) {
    f32 h = s * 0.5f;
    Vec3 n = {0,0,1};
    std::vector<Vertex> verts = {
        makeVertex({-h,-h,0}, n, 0,0, {1,0,0,1}),
        makeVertex({ h,-h,0}, n, 1,0, {0,1,0,1}),
        makeVertex({ h, h,0}, n, 1,1, {0,0,1,1}),
        makeVertex({-h, h,0}, n, 0,1, {1,1,0,1}),
    };
    std::vector<u32> idx = {0,1,2, 0,2,3};
    vbo_.upload(verts, idx);
}

void Mesh::buildCube(f32 s) {
    f32 h = s * 0.5f;
    struct Face { Vec3 n; Vec3 t; Vec3 b; Vec3 c; };
    Face faces[6] = {
        {{ 0, 0, 1},{1,0,0},{0,1,0},{1,.5f,.5f}},
        {{ 0, 0,-1},{-1,0,0},{0,1,0},{.5f,.5f,1}},
        {{ 0, 1, 0},{1,0,0},{0,0,-1},{.5f,1,.5f}},
        {{ 0,-1, 0},{1,0,0},{0,0,1},{1,1,.5f}},
        {{ 1, 0, 0},{0,0,-1},{0,1,0},{.5f,1,1}},
        {{-1, 0, 0},{0,0,1},{0,1,0},{1,.5f,1}},
    };
    std::vector<Vertex> verts;
    std::vector<u32>    idx;
    for (auto& f : faces) {
        u32 base = (u32)verts.size();
        Vec3 o = f.n * h;
        verts.push_back(makeVertex(o - f.t*h - f.b*h, f.n, 0,0, {f.c.x,f.c.y,f.c.z,1}));
        verts.push_back(makeVertex(o + f.t*h - f.b*h, f.n, 1,0, {f.c.x,f.c.y,f.c.z,1}));
        verts.push_back(makeVertex(o + f.t*h + f.b*h, f.n, 1,1, {f.c.x,f.c.y,f.c.z,1}));
        verts.push_back(makeVertex(o - f.t*h + f.b*h, f.n, 0,1, {f.c.x,f.c.y,f.c.z,1}));
        for (u32 i : {0u,1u,2u, 0u,2u,3u}) idx.push_back(base + i);
    }
    vbo_.upload(verts, idx);
}

void Mesh::buildSphere(int lat, int lon, f32 r) {
    std::vector<Vertex> verts;
    std::vector<u32>    idx;
    for (int i=0; i<=lat; ++i) {
        f32 phi  = kPi * i / lat;
        f32 sinP = sinf(phi), cosP = cosf(phi);
        for (int j=0; j<=lon; ++j) {
            f32 th   = kTwoPi * j / lon;
            f32 sinT = sinf(th), cosT = cosf(th);
            Vec3 n   = {sinP*cosT, cosP, sinP*sinT};
            Vec3 p   = n * r;
            f32 u    = (f32)j / lon;
            f32 v    = (f32)i / lat;
            Vec4 col = {(n.x+1)*.5f,(n.y+1)*.5f,(n.z+1)*.5f,1};
            verts.push_back(makeVertex(p, n, u, v, col));
        }
    }
    for (int i=0; i<lat; ++i)
        for (int j=0; j<lon; ++j) {
            u32 a = i*(lon+1)+j, b=a+1, c=(i+1)*(lon+1)+j, d=c+1;
            for (u32 vi : {a,b,c, b,d,c}) idx.push_back(vi);
        }
    vbo_.upload(verts, idx);
}

} // namespace gl
