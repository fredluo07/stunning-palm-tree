#pragma once
// ============================================================================
//  resources.h : 全程序化资源
//  几何：迷宫合并网格、球 / 柱 / 八面体水晶 / 圆环 / 面板
//  贴图：石材漫反射 + 由高度场生成的法线贴图（含砖缝、污渍、血迹）
//  无任何外部模型 / 图片依赖，全部在启动时生成。
// ============================================================================
#include "glcore.h"
#include "math3d.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstring>
#include <cstdarg>

using namespace glapi;

struct Vertex {
    float x, y, z, nx, ny, nz, u, v, ao;
    Vertex() : x(0), y(0), z(0), nx(0), ny(1), nz(0), u(0), v(0), ao(1) {}
    Vertex(float X, float Y, float Z, float NX, float NY, float NZ, float U, float V, float AO)
        : x(X), y(Y), z(Z), nx(NX), ny(NY), nz(NZ), u(U), v(V), ao(AO) {}
};

struct Mesh {
    GLuint vao, vbo, ebo;
    int indexCount;
    Mesh() : vao(0), vbo(0), ebo(0), indexCount(0) {}
    void destroy() {
        if (ebo) glDeleteBuffers(1, &ebo);
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = ebo = 0; indexCount = 0;
    }
    void draw() const {
        if (!vao || indexCount <= 0) return;
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    }
};

inline void setupVertexLayout(GLuint vao, GLuint vbo) {
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    const int STRIDE = 9 * (int)sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(8 * sizeof(float)));
    glBindVertexArray(0);
}

inline Mesh makeMesh(const std::vector<Vertex>& V, const std::vector<unsigned int>& I) {
    Mesh m;
    if (V.empty() || I.empty()) return m;
    glGenVertexArrays(1, &m.vao);
    glGenBuffers(1, &m.vbo);
    glGenBuffers(1, &m.ebo);
    glBindVertexArray(m.vao);
    glBindBuffer(GL_ARRAY_BUFFER, m.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(V.size() * sizeof(Vertex)), V.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(I.size() * sizeof(unsigned int)), I.data(), GL_STATIC_DRAW);
    const int STRIDE = 9 * (int)sizeof(float);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)0);
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(3); glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, STRIDE, (const void*)(8 * sizeof(float)));
    glBindVertexArray(0);
    m.indexCount = (int)I.size();
    return m;
}

inline void pushQuad(std::vector<Vertex>& V, std::vector<unsigned int>& I,
                     const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                     const Vec3& n, float u0, float v0, float u1, float v1, float ao) {
    unsigned int base = (unsigned int)V.size();
    V.push_back(Vertex(a.x, a.y, a.z, n.x, n.y, n.z, u0, v0, ao));
    V.push_back(Vertex(b.x, b.y, b.z, n.x, n.y, n.z, u1, v0, ao));
    V.push_back(Vertex(c.x, c.y, c.z, n.x, n.y, n.z, u1, v1, ao));
    V.push_back(Vertex(d.x, d.y, d.z, n.x, n.y, n.z, u0, v1, ao));
    I.push_back(base); I.push_back(base + 1); I.push_back(base + 2);
    I.push_back(base); I.push_back(base + 2); I.push_back(base + 3);
}

// ---------------------------------------------------------------------------
// 迷宫合并网格：只生成「墙朝向可通行空间」的面，以及地面 / 天花板
// ---------------------------------------------------------------------------
inline void buildMazeMeshes(const std::vector<std::string>& map, float cell, float wallH,
                            Mesh& wallMesh, Mesh& floorMesh, Mesh& ceilMesh) {
    std::vector<Vertex> VW, VF, VC;
    std::vector<unsigned int> IW, IF, IC;
    int H = (int)map.size();
    int W = H ? (int)map[0].size() : 0;
    auto solid = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= W || y >= H) return true;
        return map[y][x] == '#';
    };
    const float TS = 2.2f; // 每 2.2 世界单位重复一次纹理
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float x0 = x * cell, x1 = (x + 1) * cell;
            float z0 = y * cell, z1 = (y + 1) * cell;
            if (solid(x, y)) {
                // 只生成朝向走廊的墙面
                if (!solid(x, y - 1))
                    pushQuad(VW, IW, Vec3(x0, 0, z0), Vec3(x1, 0, z0), Vec3(x1, wallH, z0), Vec3(x0, wallH, z0),
                             Vec3(0, 0, -1), x0 / TS, 0.f, x1 / TS, wallH / TS, 0.85f);
                if (!solid(x, y + 1))
                    pushQuad(VW, IW, Vec3(x1, 0, z1), Vec3(x0, 0, z1), Vec3(x0, wallH, z1), Vec3(x1, wallH, z1),
                             Vec3(0, 0, 1), x1 / TS, 0.f, x0 / TS, wallH / TS, 0.85f);
                if (!solid(x - 1, y))
                    pushQuad(VW, IW, Vec3(x0, 0, z1), Vec3(x0, 0, z0), Vec3(x0, wallH, z0), Vec3(x0, wallH, z1),
                             Vec3(-1, 0, 0), z1 / TS, 0.f, z0 / TS, wallH / TS, 0.85f);
                if (!solid(x + 1, y))
                    pushQuad(VW, IW, Vec3(x1, 0, z0), Vec3(x1, 0, z1), Vec3(x1, wallH, z1), Vec3(x1, wallH, z0),
                             Vec3(1, 0, 0), z0 / TS, 0.f, z1 / TS, wallH / TS, 0.85f);
            } else {
                auto aoAt = [&](int cx, int cy, int dx, int dy) -> float {
                    int c = 0;
                    for (int oy = -1; oy <= 0; oy++) for (int ox = -1; ox <= 0; ox++)
                        if (solid(cx + dx + ox, cy + dy + oy)) c++;
                    return 1.0f - 0.085f * c;
                };
                float a00 = aoAt(x, y, 0, 0), a10 = aoAt(x, y, 1, 0);
                float a11 = aoAt(x, y, 1, 1), a01 = aoAt(x, y, 0, 1);
                // 地面
                {
                    unsigned int base = (unsigned int)VF.size();
                    VF.push_back(Vertex(x0, 0, z0, 0, 1, 0, x0 / TS, z0 / TS, a00));
                    VF.push_back(Vertex(x1, 0, z0, 0, 1, 0, x1 / TS, z0 / TS, a10));
                    VF.push_back(Vertex(x1, 0, z1, 0, 1, 0, x1 / TS, z1 / TS, a11));
                    VF.push_back(Vertex(x0, 0, z1, 0, 1, 0, x0 / TS, z1 / TS, a01));
                    IF.push_back(base); IF.push_back(base + 2); IF.push_back(base + 1);
                    IF.push_back(base); IF.push_back(base + 3); IF.push_back(base + 2);
                }
                // 天花板
                {
                    unsigned int base = (unsigned int)VC.size();
                    float a = 0.62f;
                    VC.push_back(Vertex(x0, wallH, z0, 0, -1, 0, x0 / TS, z0 / TS, a));
                    VC.push_back(Vertex(x1, wallH, z0, 0, -1, 0, x1 / TS, z0 / TS, a));
                    VC.push_back(Vertex(x1, wallH, z1, 0, -1, 0, x1 / TS, z1 / TS, a));
                    VC.push_back(Vertex(x0, wallH, z1, 0, -1, 0, x0 / TS, z1 / TS, a));
                    IC.push_back(base); IC.push_back(base + 1); IC.push_back(base + 2);
                    IC.push_back(base); IC.push_back(base + 2); IC.push_back(base + 3);
                }
            }
        }
    }
    wallMesh = makeMesh(VW, IW);
    floorMesh = makeMesh(VF, IF);
    ceilMesh = makeMesh(VC, IC);
}

// ---------------------------------------------------------------------------
// 基础几何体
// ---------------------------------------------------------------------------
inline Mesh makeSphere(float r, int seg = 20, int rings = 14) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    for (int j = 0; j <= rings; j++) {
        float phi = DM_PI * j / (float)rings;
        for (int i = 0; i <= seg; i++) {
            float th = 2.f * DM_PI * i / (float)seg;
            Vec3 n(sinf(phi) * cosf(th), cosf(phi), sinf(phi) * sinf(th));
            V.push_back(Vertex(n.x * r, n.y * r, n.z * r, n.x, n.y, n.z,
                               i / (float)seg, j / (float)rings, 1.f));
        }
    }
    for (int j = 0; j < rings; j++) for (int i = 0; i < seg; i++) {
        unsigned int a = j * (seg + 1) + i, b = a + seg + 1;
        I.push_back(a); I.push_back(b); I.push_back(a + 1);
        I.push_back(a + 1); I.push_back(b); I.push_back(b + 1);
    }
    return makeMesh(V, I);
}

inline Mesh makeCylinder(float r, float h, int seg = 14) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    float y0 = -h * 0.5f, y1 = h * 0.5f;
    for (int i = 0; i <= seg; i++) {
        float t = 2.f * DM_PI * i / (float)seg;
        float nx = cosf(t), nz = sinf(t);
        V.push_back(Vertex(nx * r, y0, nz * r, nx, 0, nz, i / (float)seg, 0, 1));
        V.push_back(Vertex(nx * r, y1, nz * r, nx, 0, nz, i / (float)seg, 1, 1));
    }
    for (int i = 0; i < seg; i++) {
        unsigned int a = i * 2;
        I.push_back(a); I.push_back(a + 1); I.push_back(a + 2);
        I.push_back(a + 2); I.push_back(a + 1); I.push_back(a + 3);
    }
    // 端盖
    for (int s = 0; s < 2; s++) {
        float y = s ? y1 : y0;
        float ny = s ? 1.f : -1.f;
        unsigned int c = (unsigned int)V.size();
        V.push_back(Vertex(0, y, 0, 0, ny, 0, 0.5f, 0.5f, 1));
        for (int i = 0; i <= seg; i++) {
            float t = 2.f * DM_PI * i / (float)seg;
            float nx = cosf(t), nz = sinf(t);
            V.push_back(Vertex(nx * r, y, nz * r, 0, ny, 0, 0.5f + nx * 0.5f, 0.5f + nz * 0.5f, 1));
        }
        for (int i = 0; i < seg; i++) {
            if (s) { I.push_back(c); I.push_back(c + i + 1); I.push_back(c + i + 2); }
            else { I.push_back(c); I.push_back(c + i + 2); I.push_back(c + i + 1); }
        }
    }
    return makeMesh(V, I);
}

// 双锥水晶（8 个平面三角，法线为面法线，看起来有棱角）
inline Mesh makeCrystal(float r = 0.42f, float h = 1.15f) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    const int N = 6;
    float top[3] = { 0, h, 0 }, bot[3] = { 0, -h, 0 };
    for (int i = 0; i < N; i++) {
        float a0 = 2.f * DM_PI * i / N, a1 = 2.f * DM_PI * (i + 1) / N;
        Vec3 p0(cosf(a0) * r, 0, sinf(a0) * r);
        Vec3 p1(cosf(a1) * r, 0, sinf(a1) * r);
        Vec3 T(top[0], top[1], top[2]), B(bot[0], bot[1], bot[2]);
        Vec3 nU = normalize(cross(p1 - p0, T - p0));
        Vec3 nD = normalize(cross(B - p0, p1 - p0));
        unsigned int base = (unsigned int)V.size();
        float u0 = i / (float)N, u1 = (i + 1) / (float)N;
        V.push_back(Vertex(p0.x, p0.y, p0.z, nU.x, nU.y, nU.z, u0, 0.5f, 1));
        V.push_back(Vertex(p1.x, p1.y, p1.z, nU.x, nU.y, nU.z, u1, 0.5f, 1));
        V.push_back(Vertex(T.x, T.y, T.z, nU.x, nU.y, nU.z, u0 + 0.5f / N, 1.f, 1));
        I.push_back(base); I.push_back(base + 1); I.push_back(base + 2);
        base = (unsigned int)V.size();
        V.push_back(Vertex(p0.x, p0.y, p0.z, nD.x, nD.y, nD.z, u0, 0.5f, 1));
        V.push_back(Vertex(B.x, B.y, B.z, nD.x, nD.y, nD.z, u0 + 0.5f / N, 0.f, 1));
        V.push_back(Vertex(p1.x, p1.y, p1.z, nD.x, nD.y, nD.z, u1, 0.5f, 1));
        I.push_back(base); I.push_back(base + 1); I.push_back(base + 2);
    }
    return makeMesh(V, I);
}

inline Mesh makeTorus(float R, float r, int seg = 28, int rings = 12) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    for (int j = 0; j <= rings; j++) {
        float v = 2.f * DM_PI * j / rings;
        for (int i = 0; i <= seg; i++) {
            float u = 2.f * DM_PI * i / seg;
            Vec3 n(cosf(u) * cosf(v), sinf(v), sinf(u) * cosf(v));
            float rr = R + r * cosf(v);
            V.push_back(Vertex(cosf(u) * rr, r * sinf(v), sinf(u) * rr,
                               n.x, n.y, n.z, i / (float)seg, j / (float)rings, 1));
        }
    }
    for (int j = 0; j < rings; j++) for (int i = 0; i < seg; i++) {
        unsigned int a = j * (seg + 1) + i, b = a + seg + 1;
        I.push_back(a); I.push_back(b); I.push_back(a + 1);
        I.push_back(a + 1); I.push_back(b); I.push_back(b + 1);
    }
    return makeMesh(V, I);
}

// 正对 +Z 的方形面板（传送门内部）
inline Mesh makePanel(float w, float h) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    pushQuad(V, I, Vec3(-w / 2, -h / 2, 0), Vec3(w / 2, -h / 2, 0),
             Vec3(w / 2, h / 2, 0), Vec3(-w / 2, h / 2, 0),
             Vec3(0, 0, 1), 0, 0, 1, 1, 1.f);
    return makeMesh(V, I);
}

inline Mesh makeBox(float sx, float sy, float sz) {
    std::vector<Vertex> V;
    std::vector<unsigned int> I;
    float x = sx * 0.5f, y = sy * 0.5f, z = sz * 0.5f;
    pushQuad(V, I, Vec3(-x, -y, z), Vec3(x, -y, z), Vec3(x, y, z), Vec3(-x, y, z), Vec3(0, 0, 1), 0, 0, 1, 1, 0.8f);
    pushQuad(V, I, Vec3(x, -y, -z), Vec3(-x, -y, -z), Vec3(-x, y, -z), Vec3(x, y, -z), Vec3(0, 0, -1), 0, 0, 1, 1, 0.8f);
    pushQuad(V, I, Vec3(x, -y, z), Vec3(x, -y, -z), Vec3(x, y, -z), Vec3(x, y, z), Vec3(1, 0, 0), 0, 0, 1, 1, 0.8f);
    pushQuad(V, I, Vec3(-x, -y, -z), Vec3(-x, -y, z), Vec3(-x, y, z), Vec3(-x, y, -z), Vec3(-1, 0, 0), 0, 0, 1, 1, 0.8f);
    pushQuad(V, I, Vec3(-x, y, z), Vec3(x, y, z), Vec3(x, y, -z), Vec3(-x, y, -z), Vec3(0, 1, 0), 0, 0, 1, 1, 1.f);
    pushQuad(V, I, Vec3(-x, -y, -z), Vec3(x, -y, -z), Vec3(x, -y, z), Vec3(-x, -y, z), Vec3(0, -1, 0), 0, 0, 1, 1, 0.5f);
    return makeMesh(V, I);
}

// ---------------------------------------------------------------------------
// 程序化石材贴图（mode 0=墙砖 1=地面石板 2=天花板岩面）
// ---------------------------------------------------------------------------
struct TexPair { GLuint albedo, normal; TexPair() : albedo(0), normal(0) {} };

inline GLuint uploadTex(const std::vector<unsigned char>& data, int size, bool mipmap = true) {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

inline TexPair generateStone(int size, int mode, RNG& rng) {
    std::vector<float> H((size_t)size * size, 0.f);
    std::vector<unsigned char> rgb((size_t)size * size * 4, 0);
    std::vector<unsigned char> nrm((size_t)size * size * 4, 0);
    float ox = rng.range(0.f, 100.f), oy = rng.range(0.f, 100.f);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float u = x / (float)size, v = y / (float)size;
            float fx = u + ox, fy = v + oy;
            float n = fbm(fx * 8.f, fy * 8.f, 5);
            float fine = fbm(fx * 34.f, fy * 34.f, 3);
            float seam = 0.f;
            if (mode == 0) {
                float row = floorf(v * 9.f);
                float shift = fmodf(row, 2.f) * 0.5f;
                float bx = u * 6.f + shift, by = v * 9.f;
                bx -= floorf(bx); by -= floorf(by);
                float d = fminf(fminf(bx, 1.f - bx), fminf(by, 1.f - by));
                seam = 1.f - smoothstepf(0.f, 0.035f, d);
            } else if (mode == 1) {
                float bx = u * 5.f, by = v * 5.f;
                bx -= floorf(bx); by -= floorf(by);
                float d = fminf(fminf(bx, 1.f - bx), fminf(by, 1.f - by));
                seam = 1.f - smoothstepf(0.f, 0.028f, d);
            } else {
                float crack = fabsf(fbm(fx * 5.f, fy * 5.f, 4) - 0.5f);
                seam = 1.f - smoothstepf(0.f, 0.018f, crack);
            }
            float height = n * 0.62f + fine * 0.16f - seam * 0.42f;
            H[(size_t)y * size + x] = height;

            float damp = fbm(fx * 2.1f + 13.f, fy * 2.1f + 7.f, 4);
            float base = (mode == 2 ? 0.24f : (mode == 1 ? 0.30f : 0.34f));
            float val = base * (0.58f + 0.75f * n + 0.18f * fine) * (0.68f + 0.62f * damp);
            val *= (1.f - seam * 0.72f);
            // 苔痕（偏绿）
            float moss = smoothstepf(0.60f, 0.78f, damp) * (mode == 1 ? 0.35f : 0.18f);
            // 血迹（低频斑块）
            float bl = smoothstepf(0.655f, 0.755f, fbm(fx * 3.3f + 31.f, fy * 3.3f + 17.f, 4));
            float blod = bl * (mode == 1 ? 0.75f : 0.42f);
            float r = val * 0.98f + blod * 0.30f - moss * 0.05f;
            float g = val * 0.96f - blod * 0.16f + moss * 0.10f;
            float b = val * 0.93f - blod * 0.19f + moss * 0.02f;
            size_t k = ((size_t)y * size + x) * 4;
            rgb[k + 0] = (unsigned char)(clampf(r, 0.f, 1.f) * 255.f);
            rgb[k + 1] = (unsigned char)(clampf(g, 0.f, 1.f) * 255.f);
            rgb[k + 2] = (unsigned char)(clampf(b, 0.f, 1.f) * 255.f);
            rgb[k + 3] = 255;
        }
    }
    const float STR = 3.4f;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int xl = x > 0 ? x - 1 : size - 1, xr = x + 1 < size ? x + 1 : 0;
            int yu = y > 0 ? y - 1 : size - 1, yd = y + 1 < size ? y + 1 : 0;
            float dx = H[(size_t)y * size + xr] - H[(size_t)y * size + xl];
            float dy = H[(size_t)yd * size + x] - H[(size_t)yu * size + x];
            Vec3 N = normalize(Vec3(-dx * STR, -dy * STR, 1.f));
            size_t k = ((size_t)y * size + x) * 4;
            nrm[k + 0] = (unsigned char)((N.x * 0.5f + 0.5f) * 255.f);
            nrm[k + 1] = (unsigned char)((N.y * 0.5f + 0.5f) * 255.f);
            nrm[k + 2] = (unsigned char)((N.z * 0.5f + 0.5f) * 255.f);
            nrm[k + 3] = 255;
        }
    }
    TexPair t;
    t.albedo = uploadTex(rgb, size);
    t.normal = uploadTex(nrm, size);
    return t;
}

// ---------------------------------------------------------------------------
// 渲染目标（颜色 / 深度纹理）
// ---------------------------------------------------------------------------
struct RenderTarget {
    GLuint fbo, color, depth, rbo;
    int w, h;
    bool hasColor;
    RenderTarget() : fbo(0), color(0), depth(0), rbo(0), w(0), h(0), hasColor(true) {}
    void destroy() {
        if (rbo) glDeleteRenderbuffers(1, &rbo);
        if (depth) glDeleteTextures(1, &depth);
        if (color) glDeleteTextures(1, &color);
        if (fbo) glDeleteFramebuffers(1, &fbo);
        fbo = color = depth = rbo = 0;
    }
    bool create(int W, int H, bool withColor, bool withDepth, bool depthTexture) {
        destroy();
        w = W; h = H; hasColor = withColor;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if (withColor) {
            glGenTextures(1, &color);
            glBindTexture(GL_TEXTURE_2D, color);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
        } else {
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
        }
        if (withDepth) {
            if (depthTexture) {
                glGenTextures(1, &depth);
                glBindTexture(GL_TEXTURE_2D, depth);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, W, H, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                float border[4] = { 1.f, 1.f, 1.f, 1.f };
                glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
            } else {
                glGenRenderbuffers(1, &rbo);
                glBindRenderbuffer(GL_RENDERBUFFER, rbo);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W, H);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
            }
        }
        GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (st != GL_FRAMEBUFFER_COMPLETE) {
            GLLogFallback("RenderTarget incomplete: 0x%X", (unsigned)st);
            return false;
        }
        return true;
    }
    void bind() const {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
    }
private:
    static void GLLogFallback(const char* fmt, ...) {
        FILE* f = fopen("darkmaze.log", "a");
        if (f) { va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap); fputc('\n', f); fclose(f); }
    }
};

// ---------------------------------------------------------------------------
// 粒子批（billboard）
// ---------------------------------------------------------------------------
class SpriteBatch {
public:
    static const size_t MAX_FLOATS = 60000; // 1000 个粒子
    GLuint vao, vbo;
    std::vector<float> data;
    SpriteBatch() : vao(0), vbo(0) {}
    void init() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(MAX_FLOATS * sizeof(float)), 0, GL_STREAM_DRAW);
        const int S = 10 * (int)sizeof(float);
        glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, (const void*)0);
        glEnableVertexAttribArray(1); glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, S, (const void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(2); glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, S, (const void*)(5 * sizeof(float)));
        glEnableVertexAttribArray(3); glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, S, (const void*)(6 * sizeof(float)));
        glBindVertexArray(0);
    }
    void destroy() {
        if (vbo) glDeleteBuffers(1, &vbo);
        if (vao) glDeleteVertexArrays(1, &vao);
        vao = vbo = 0;
    }
    void clear() { data.clear(); }
    void add(const Vec3& p, float sizePx, float r, float g, float b, float a) {
        static const float C[6][2] = { {-1,-1},{1,-1},{1,1},{-1,-1},{1,1},{-1,1} };
        for (int i = 0; i < 6; i++) {
            data.push_back(p.x); data.push_back(p.y); data.push_back(p.z);
            data.push_back(C[i][0]); data.push_back(C[i][1]);
            data.push_back(sizePx);
            data.push_back(r); data.push_back(g); data.push_back(b); data.push_back(a);
        }
    }
    void draw() const {
        if (data.empty()) return;
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        size_t floats = data.size();
        if (floats > MAX_FLOATS) floats = MAX_FLOATS - (MAX_FLOATS % 10);
        size_t bytes = floats * sizeof(float);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, data.data());
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(floats / 10));
        glBindVertexArray(0);
    }
};
