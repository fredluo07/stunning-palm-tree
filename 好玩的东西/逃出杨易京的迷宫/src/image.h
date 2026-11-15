#pragma once
// ============================================================================
//  image.h : 用 GDI+ 按「文件内容」解码外部图片，上传为 GL 纹理
//
//  说明：用户给的 g.PNG 实际内容是 JPEG（文件头 FF D8 FF），扩展名不可信，
//  所以这里把整个文件读进内存，用 SHCreateMemStream 交给 GDI+ 按内容识别。
//  原图像素（含自带 alpha）完整保留，平面四边的柔化在 MONSTER_FS 中处理。
//  链接需要 gdiplus.lib shlwapi.lib（见 build.bat / CMakeLists.txt）。
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include "glcore.h"
#include <vector>
#include <string>
#include <cstdio>

namespace img {

inline ULONG_PTR& gdiToken() { static ULONG_PTR t = 0; return t; }

inline bool initGDIPlus() {
    if (gdiToken()) return true;
    Gdiplus::GdiplusStartupInput si;
    return Gdiplus::GdiplusStartup(&gdiToken(), &si, 0) == Gdiplus::Ok;
}
inline void shutdownGDIPlus() {
    if (gdiToken()) { Gdiplus::GdiplusShutdown(gdiToken()); gdiToken() = 0; }
}

// 返回是否成功；outTex 为 0 表示失败。outAspect = 宽/高。
// outTransparent/outOpaque 仅用于自检统计图片的 alpha 情况。
inline bool loadBillboardTexture(const wchar_t* path, GLuint& outTex,
                                 float& outAspect, int& outW, int& outH,
                                 int* outTransparent = 0, int* outOpaque = 0) {
    using namespace glapi;
    outTex = 0; outAspect = 1.f; outW = 0; outH = 0;
    if (!initGDIPlus()) return false;

    FILE* f = _wfopen(path, L"rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 64 * 1024 * 1024) { fclose(f); return false; }
    std::vector<unsigned char> bytes((size_t)sz);
    if (fread(bytes.data(), 1, bytes.size(), f) != bytes.size()) { fclose(f); return false; }
    fclose(f);

    IStream* st = SHCreateMemStream(bytes.data(), (UINT)bytes.size());
    if (!st) return false;
    Gdiplus::Bitmap src(st);
    st->Release();
    if (src.GetLastStatus() != Gdiplus::Ok || src.GetWidth() == 0 || src.GetHeight() == 0)
        return false;

    const int w = (int)src.GetWidth(), h = (int)src.GetHeight();
    Gdiplus::Bitmap conv(w, h, PixelFormat32bppARGB);
    {
        Gdiplus::Graphics gr(&conv);
        gr.Clear(Gdiplus::Color(0, 0, 0, 0));
        gr.DrawImage(&src, 0, 0, w, h);
    }
    Gdiplus::Rect rc(0, 0, w, h);
    Gdiplus::BitmapData bd;
    if (conv.LockBits(&rc, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bd) != Gdiplus::Ok)
        return false;

    // 不做色键抠图：人像照片的背景与主体颜色是连续的，色键只会抠出破碎的洞，
    // 反而更难看。这里保留原图完整像素（含图片自带的 alpha），
    // 四周的硬边由 MONSTER_FS 的柔和渐隐处理，视觉上像一张浮在黑暗里的旧照片。
    std::vector<unsigned char> rgba((size_t)w * h * 4, 0);
    int nTransparent = 0, nOpaque = 0;
    for (int y = 0; y < h; y++) {
        const unsigned char* row = (const unsigned char*)bd.Scan0 + (ptrdiff_t)y * bd.Stride;
        for (int x = 0; x < w; x++) {
            const unsigned char b = row[x * 4 + 0], g = row[x * 4 + 1];
            const unsigned char r = row[x * 4 + 2], a = row[x * 4 + 3];
            if (a < 16) nTransparent++;
            else if (a > 240) nOpaque++;
            unsigned char* o = &rgba[((size_t)y * w + x) * 4];
            o[0] = r; o[1] = g; o[2] = b; o[3] = a;   // straight alpha，shader 里再做预乘
        }
    }
    conv.UnlockBits(&bd);

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    outTex = tex; outAspect = (float)w / (float)(h > 0 ? h : 1);
    outW = w; outH = h;
    if (outTransparent) *outTransparent = nTransparent;
    if (outOpaque) *outOpaque = nOpaque;
    return true;
}

// 优先取 exe 同目录下的素材，其次当前工作目录
inline std::wstring resolveAssetPath(const wchar_t* name) {
    wchar_t buf[MAX_PATH] = { 0 };
    GetModuleFileNameW(0, buf, MAX_PATH);
    std::wstring exe(buf);
    size_t p = exe.find_last_of(L"\\/");
    if (p != std::wstring::npos) {
        std::wstring full = exe.substr(0, p + 1) + name;
        FILE* f = _wfopen(full.c_str(), L"rb");
        if (f) { fclose(f); return full; }
    }
    return std::wstring(name);
}

} // namespace img
