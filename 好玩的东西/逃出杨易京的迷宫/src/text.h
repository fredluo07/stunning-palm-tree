#pragma once
// ============================================================================
//  text.h : 基于 GDI 的中文文字 -> OpenGL 纹理缓存
//  直接使用系统自带的中文字体（微软雅黑 / 黑体），无需 FreeType、无需字体文件。
//  文字用抗锯齿灰度绘制到 32 位 DIB，再转成带 alpha 的 RGBA 纹理。
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "glcore.h"
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cstdio>

using namespace glapi;

class TextRenderer {
public:
    struct Glyph {
        GLuint tex;
        int w, h;
        Glyph() : tex(0), w(0), h(0) {}
    };

    TextRenderer() : memDC(0), dib(0), oldBmp(0), bits(0), capW(0), capH(0) {}

    bool init() {
        HDC screen = GetDC(0);
        memDC = CreateCompatibleDC(screen);
        ReleaseDC(0, screen);
        if (!memDC) return false;
        return ensureDIB(2048, 256);
    }

    void shutdown() {
        for (std::map<std::wstring, Glyph>::iterator it = cache.begin(); it != cache.end(); ++it) {
            if (it->second.tex) glDeleteTextures(1, &it->second.tex);
        }
        cache.clear();
        for (std::map<int, HFONT>::iterator it = fonts.begin(); it != fonts.end(); ++it)
            DeleteObject(it->second);
        fonts.clear();
        if (dib) { SelectObject(memDC, oldBmp); DeleteObject(dib); dib = 0; }
        if (memDC) { DeleteDC(memDC); memDC = 0; }
    }

    // color: 0xRRGGBB
    const Glyph* get(const wchar_t* text, int px, unsigned int color, bool bold = false) {
        if (!text || !memDC) return 0;
        if (cache.size() > 220) clearCache(); // 防止动态文本无限占纹理
        std::wstring key(text);
        wchar_t suffix[64];
        swprintf(suffix, 64, L"|%d|%06X|%d", px, color & 0xFFFFFF, bold ? 1 : 0);
        key += suffix;
        std::map<std::wstring, Glyph>::iterator it = cache.find(key);
        if (it != cache.end()) return &it->second;

        HFONT font = getFont(px, bold);
        HFONT old = (HFONT)SelectObject(memDC, font);
        SIZE sz = { 0, 0 };
        int len = (int)wcslen(text);
        if (len > 0) GetTextExtentPoint32W(memDC, text, len, &sz);
        int w = sz.cx + 8, h = sz.cy + 8;
        if (w < 8) w = 8;
        if (h < 8) h = 8;
        if (w > capW || h > capH) {
            if (!ensureDIB(w + 64, h + 64)) { SelectObject(memDC, old); return 0; }
            old = (HFONT)SelectObject(memDC, font);
        }

        // 清空为透明黑
        memset(bits, 0, (size_t)capW * capH * 4);
        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));
        SetTextAlign(memDC, TA_TOP | TA_LEFT);
        if (len > 0) TextOutW(memDC, 4, 4, text, len);
        GdiFlush();
        SelectObject(memDC, old);

        std::vector<unsigned char> rgba((size_t)w * h * 4, 0);
        unsigned char cr = (unsigned char)((color >> 16) & 0xFF);
        unsigned char cg = (unsigned char)((color >> 8) & 0xFF);
        unsigned char cb = (unsigned char)(color & 0xFF);
        const unsigned char* src = (const unsigned char*)bits;
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                const unsigned char* p = src + ((size_t)y * capW + x) * 4;
                int a = p[0]; if (p[1] > a) a = p[1]; if (p[2] > a) a = p[2];
                unsigned char* d = &rgba[((size_t)y * w + x) * 4];
                d[0] = cr; d[1] = cg; d[2] = cb; d[3] = (unsigned char)a;
            }
        }

        Glyph g;
        g.w = w; g.h = h;
        glGenTextures(1, &g.tex);
        glBindTexture(GL_TEXTURE_2D, g.tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
        cache[key] = g;
        return &cache[key];
    }

    void clearCache() {
        for (std::map<std::wstring, Glyph>::iterator it = cache.begin(); it != cache.end(); ++it)
            if (it->second.tex) glDeleteTextures(1, &it->second.tex);
        cache.clear();
    }

private:
    HDC memDC;
    HBITMAP dib, oldBmp;
    void* bits;
    int capW, capH;
    std::map<int, HFONT> fonts;
    std::map<std::wstring, Glyph> cache;

    bool ensureDIB(int w, int h) {
        if (dib) { SelectObject(memDC, oldBmp); DeleteObject(dib); dib = 0; bits = 0; }
        capW = w; capH = h;
        BITMAPINFO bi; ZeroMemory(&bi, sizeof(bi));
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = capW;
        bi.bmiHeader.biHeight = -capH; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        dib = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &bits, 0, 0);
        if (!dib) return false;
        oldBmp = (HBITMAP)SelectObject(memDC, dib);
        return true;
    }

    HFONT getFont(int px, bool bold) {
        int key = (px << 1) | (bold ? 1 : 0);
        std::map<int, HFONT>::iterator it = fonts.find(key);
        if (it != fonts.end()) return it->second;
        HFONT f = CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
        if (!f) f = CreateFontW(-px, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"SimHei");
        fonts[key] = f;
        return f;
    }
};
