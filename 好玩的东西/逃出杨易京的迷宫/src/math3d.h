#pragma once
// ============================================================================
//  math3d.h : 轻量向量 / 矩阵 / 噪声库（列主序，与 GLSL 一致）
// ============================================================================
#include <cmath>
#include <cstdint>

#ifndef DM_PI
#define DM_PI 3.14159265358979323846f
#endif

struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float a, float b) : x(a), y(b) {}
};
struct Vec3 {
    float x, y, z;
    Vec3() : x(0), y(0), z(0) {}
    Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
    Vec3 operator+(const Vec3& o) const { return Vec3(x + o.x, y + o.y, z + o.z); }
    Vec3 operator-(const Vec3& o) const { return Vec3(x - o.x, y - o.y, z - o.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator/(float s) const { return Vec3(x / s, y / s, z / s); }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    Vec3 operator-() const { return Vec3(-x, -y, -z); }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return Vec3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
inline float length(const Vec3& v) { return sqrtf(dot(v, v)); }
inline Vec3 normalize(const Vec3& v) { float l = length(v); return l > 1e-7f ? v / l : Vec3(0, 0, 0); }
inline Vec3 vmin(const Vec3& a, const Vec3& b) {
    return Vec3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z);
}
inline Vec3 vmax(const Vec3& a, const Vec3& b) {
    return Vec3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z);
}

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
    Vec4(const Vec3& v, float d) : x(v.x), y(v.y), z(v.z), w(d) {}
};

inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstepf(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}
inline Vec3 mix3(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }

struct Mat4 {
    float m[16]; // column-major: m[col*4+row]
    Mat4() {
        for (int i = 0; i < 16; i++) m[i] = 0.f;
        m[0] = m[5] = m[10] = m[15] = 1.f;
    }
    static Mat4 Identity() { return Mat4(); }
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline Mat4 mul(const Mat4& A, const Mat4& B) {
    Mat4 R;
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            float s = 0.f;
            for (int k = 0; k < 4; k++) s += A.at(r, k) * B.at(k, c);
            R.at(r, c) = s;
        }
    }
    return R;
}
inline Vec4 mulv(const Mat4& A, const Vec4& v) {
    Vec4 r;
    for (int row = 0; row < 4; row++) {
        float s = 0.f;
        for (int k = 0; k < 4; k++) s += A.at(row, k) * (&v.x)[k];
        (&r.x)[row] = s;
    }
    return r;
}
inline Vec3 transformPoint(const Mat4& A, const Vec3& p) {
    Vec4 r = mulv(A, Vec4(p, 1.f));
    return Vec3(r.x, r.y, r.z);
}
inline Vec3 transformDir(const Mat4& A, const Vec3& p) {
    Vec4 r = mulv(A, Vec4(p, 0.f));
    return Vec3(r.x, r.y, r.z);
}
inline Mat4 transpose(const Mat4& A) {
    Mat4 R;
    for (int c = 0; c < 4; c++) for (int r = 0; r < 4; r++) R.at(r, c) = A.at(c, r);
    return R;
}
inline Mat4 inverse(const Mat4& in) {
    float a[4][8];
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) {
        a[r][c] = in.at(r, c);
        a[r][4 + c] = (r == c) ? 1.f : 0.f;
    }
    for (int i = 0; i < 4; i++) {
        int piv = i;
        for (int r = i + 1; r < 4; r++) if (fabsf(a[r][i]) > fabsf(a[piv][i])) piv = r;
        if (fabsf(a[piv][i]) < 1e-9f) return Mat4();
        if (piv != i) for (int c = 0; c < 8; c++) { float t = a[piv][c]; a[piv][c] = a[i][c]; a[i][c] = t; }
        float p = a[i][i];
        for (int c = 0; c < 8; c++) a[i][c] /= p;
        for (int r = 0; r < 4; r++) if (r != i) {
            float f = a[r][i];
            for (int c = 0; c < 8; c++) a[r][c] -= f * a[i][c];
        }
    }
    Mat4 R;
    for (int r = 0; r < 4; r++) for (int c = 0; c < 4; c++) R.at(r, c) = a[r][4 + c];
    return R;
}
inline Mat4 translate(const Vec3& t) {
    Mat4 R; R.at(0, 3) = t.x; R.at(1, 3) = t.y; R.at(2, 3) = t.z; return R;
}
inline Mat4 scale(const Vec3& s) {
    Mat4 R; R.at(0, 0) = s.x; R.at(1, 1) = s.y; R.at(2, 2) = s.z; return R;
}
inline Mat4 rotateX(float a) {
    Mat4 R; float c = cosf(a), s = sinf(a);
    R.at(1, 1) = c; R.at(1, 2) = -s; R.at(2, 1) = s; R.at(2, 2) = c; return R;
}
inline Mat4 rotateY(float a) {
    Mat4 R; float c = cosf(a), s = sinf(a);
    R.at(0, 0) = c; R.at(0, 2) = s; R.at(2, 0) = -s; R.at(2, 2) = c; return R;
}
inline Mat4 rotateZ(float a) {
    Mat4 R; float c = cosf(a), s = sinf(a);
    R.at(0, 0) = c; R.at(0, 1) = -s; R.at(1, 0) = s; R.at(1, 1) = c; return R;
}
inline Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
    Vec3 f = normalize(center - eye);
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);
    Mat4 R;
    R.at(0, 0) = s.x; R.at(0, 1) = s.y; R.at(0, 2) = s.z;
    R.at(1, 0) = u.x; R.at(1, 1) = u.y; R.at(1, 2) = u.z;
    R.at(2, 0) = -f.x; R.at(2, 1) = -f.y; R.at(2, 2) = -f.z;
    R.at(0, 3) = -dot(s, eye); R.at(1, 3) = -dot(u, eye); R.at(2, 3) = dot(f, eye);
    return R;
}
inline Mat4 perspective(float fovy, float aspect, float zn, float zf) {
    Mat4 R;
    float f = 1.f / tanf(fovy * 0.5f);
    R.m[0] = f / aspect; R.m[5] = f;
    R.m[10] = (zf + zn) / (zn - zf); R.m[11] = -1.f;
    R.m[14] = (2.f * zf * zn) / (zn - zf); R.m[15] = 0.f;
    return R;
}
inline Mat4 ortho(float l, float r, float b, float t, float zn, float zf) {
    Mat4 R;
    R.m[0] = 2.f / (r - l); R.m[5] = 2.f / (t - b); R.m[10] = -2.f / (zf - zn);
    R.m[12] = -(r + l) / (r - l); R.m[13] = -(t + b) / (t - b);
    R.m[14] = -(zf + zn) / (zf - zn);
    return R;
}

// ------------------------------ 伪随机 / 噪声 --------------------------------
struct RNG {
    uint64_t s;
    explicit RNG(uint64_t seed = 88172645463325252ull) : s(seed) {}
    uint32_t nextU() {
        s ^= s << 7; s ^= s >> 9;
        return (uint32_t)(s >> 16);
    }
    float next() { return (nextU() & 0xFFFFFF) / 16777216.f; }
    int nextInt(int lo, int hi) { return lo + (int)(next() * (hi - lo + 1)) % (hi - lo + 1); }
    float range(float a, float b) { return a + (b - a) * next(); }
};
inline float hash21(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFFFF) / 16777215.f;
}
inline float vnoise(float x, float y) {
    int xi = (int)floorf(x), yi = (int)floorf(y);
    float xf = x - (float)xi, yf = y - (float)yi;
    float u = xf * xf * (3.f - 2.f * xf), v = yf * yf * (3.f - 2.f * yf);
    float a = hash21(xi, yi), b = hash21(xi + 1, yi), c = hash21(xi, yi + 1), d = hash21(xi + 1, yi + 1);
    return lerpf(lerpf(a, b, u), lerpf(c, d, u), v);
}
inline float fbm(float x, float y, int oct = 5) {
    float s = 0.f, amp = 0.5f, sum = 0.f;
    for (int i = 0; i < oct; i++) {
        s += amp * vnoise(x, y);
        sum += amp;
        x *= 2.03f; y *= 2.03f; amp *= 0.5f;
    }
    return s / (sum > 0.f ? sum : 1.f);
}
