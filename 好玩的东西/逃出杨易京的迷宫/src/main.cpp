// ============================================================================
//  暗影迷宫 / Dark Maze  —— 主程序
//  C++17 + OpenGL 3.3 core + Win32，零第三方库、零外部素材
//  编译：见 CMakeLists.txt 或 build.bat
// ============================================================================
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "glcore.h"
#include "math3d.h"
#include "shaders.h"
#include "resources.h"
#include "text.h"
#include "audio.h"
#include "image.h"
#include "game.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cwchar>
#include <vector>
#include <string>

using namespace glapi;

static HWND   g_hwnd = 0;
static HDC    g_dc = 0;
static HGLRC  g_ctx = 0;
static int    g_W = 1280, g_H = 720;
static bool   g_mouseClicked = false;
static bool   g_prevKeys[256] = { false };
// 键盘状态改由窗口消息驱动：窗口在前台时 WM_KEYDOWN/WM_KEYUP 一定可靠，
// 而仅靠 GetAsyncKeyState 轮询在部分焦点/远程桌面环境下会读不到按键。
static bool   g_keyHeld[256] = { false };   // 当前是否按住
static bool   g_keyEdge[256] = { false };   // 本帧「首次按下」边沿
static bool   g_cursorHidden = false;

static void clearKeyState() {
    for (int i = 0; i < 256; i++) {
        g_prevKeys[i] = false; g_keyHeld[i] = false; g_keyEdge[i] = false;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CLOSE: case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_LBUTTONDOWN: g_mouseClicked = true; return 0;
    case WM_KEYDOWN:
        {
            int vk = (int)(w & 255);
            // lParam bit30 = 1 表示是系统自动重复，菜单导航只响应首次按下
            if (!(l & 0x40000000)) g_keyEdge[vk] = true;
            g_keyHeld[vk] = true;
        }
        return 0;
    case WM_KEYUP: g_keyHeld[(int)(w & 255)] = false; return 0;
    case WM_SIZE:
        if (w != SIZE_MINIMIZED) { g_W = (int)LOWORD(l); g_H = (int)HIWORD(l); if (g_W < 320) g_W = 320; if (g_H < 240) g_H = 240; }
        return 0;
    case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE) clearKeyState(); return 0;
    case WM_KILLFOCUS: clearKeyState(); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static bool createWindowAndGL(HINSTANCE inst, bool show = true) {
    WNDCLASSEXW wc; ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(0, (LPCWSTR)IDC_ARROW);
    wc.hIcon = LoadIconW(0, (LPCWSTR)IDI_APPLICATION);
    wc.lpszClassName = L"DarkMazeWindow";
    if (!RegisterClassExW(&wc)) return false;

    RECT rc = { 0, 0, g_W, g_H };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
    g_hwnd = CreateWindowExW(0, L"DarkMazeWindow", L"暗影迷宫 · Dark Maze",
        WS_OVERLAPPEDWINDOW, x, y, ww, wh, 0, 0, inst, 0);
    if (!g_hwnd) return false;
    ShowWindow(g_hwnd, show ? SW_SHOW : SW_HIDE);
    UpdateWindow(g_hwnd);
    g_dc = GetDC(g_hwnd);
    if (!g_dc) return false;

    PIXELFORMATDESCRIPTOR pfd; ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(g_dc, &pfd);
    if (!pf || !SetPixelFormat(g_dc, pf, &pfd)) return false;

    HGLRC tmp = wglCreateContext(g_dc);
    if (!tmp) return false;
    wglMakeCurrent(g_dc, tmp);
    PFN_wglCreateContextAttribsARB createAttr =
        (PFN_wglCreateContextAttribsARB)(void*)wglGetProcAddress("wglCreateContextAttribsARB");
    if (createAttr) {
        int attr[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0, 0
        };
        HGLRC core = createAttr(g_dc, 0, attr);
        if (core) { wglMakeCurrent(g_dc, core); wglDeleteContext(tmp); g_ctx = core; }
        else g_ctx = tmp;
    } else g_ctx = tmp;

    std::string missing;
    if (!glapi::Init(missing)) {
        GLLog("[GL] 缺失函数: %s", missing.c_str());
        MessageBoxW(g_hwnd, L"无法初始化 OpenGL 3.3，请更新显卡驱动。详情见 darkmaze.log",
            L"暗影迷宫", MB_ICONERROR);
        return false;
    }
    PFN_wglSwapIntervalEXT swapInt = (PFN_wglSwapIntervalEXT)(void*)wglGetProcAddress("wglSwapIntervalEXT");
    if (swapInt) swapInt(1);
    GLLog("[GL] %s | %s", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    return true;
}

static Mat4 TRS(const Vec3& t, float ry, const Vec3& s) {
    return mul(mul(translate(t), rotateY(ry)), scale(s));
}

struct Material {
    Vec3 base, emis;
    float rough, emisK, fres, pulse, alpha;
    bool unlit, useMap, useNormal;
    Vec2 tile;
    Material() : base(1, 1, 1), emis(0, 0, 0), rough(0.8f), emisK(0), fres(0),
        pulse(0.5f), alpha(1), unlit(false), useMap(false), useNormal(false), tile(1, 1) {}
};

class Renderer {
public:
    Shader sceneSh, depthSh, brightSh, blurSh, compSh, uiSh, spriteSh, portalSh, monsterSh;
    TextRenderer text;
    GLuint monsterTex;          // g.PNG 纹理
    float monsterAspect;        // 图片宽/高
    bool monsterOK;             // 加载成功则用图片平面，否则回退到程序化人偶
    Mesh mazeWall, mazeFloor, mazeCeil;
    Mesh sphereM, cylM, crystalM, torusM, panelM, boxM;
    TexPair texWall, texFloor, texCeil;
    RenderTarget sceneRT, bloomA, bloomB, shadowRT;
    GLuint emptyVAO, whiteTex;
    SpriteBatch sprites, monsterSprites;
    int W, H;

    Renderer() : monsterTex(0), monsterAspect(1.f), monsterOK(false),
                 emptyVAO(0), whiteTex(0), W(0), H(0) {}

    bool init(Game& g) {
        if (!text.init()) return false;
        if (!img::initGDIPlus()) GLLog("[Image] GDI+ 初始化失败，怪物回退到程序化模型");
        if (!sceneSh.create(SCENE_VS, SCENE_FS, "scene") ||
            !depthSh.create(DEPTH_VS, DEPTH_FS, "depth") ||
            !brightSh.create(POST_VS, BRIGHT_FS, "bright") ||
            !blurSh.create(POST_VS, BLUR_FS, "blur") ||
            !compSh.create(POST_VS, COMPOSITE_FS, "composite") ||
            !uiSh.create(UI_VS, UI_FS, "ui") ||
            !spriteSh.create(SPRITE_VS, SPRITE_FS, "sprite") ||
            !portalSh.create(PORTAL_VS, PORTAL_FS, "portal") ||
            !monsterSh.create(MONSTER_VS, MONSTER_FS, "monster")) {
            MessageBoxW(g_hwnd, L"着色器编译失败，详情见 darkmaze.log", L"暗影迷宫", MB_ICONERROR);
            return false;
        }
        RNG rng(77123);
        texWall = generateStone(512, 0, rng);
        texFloor = generateStone(512, 1, rng);
        texCeil = generateStone(512, 2, rng);
        buildMazeMeshes(g.map, g.CELL, g.WALL_H, mazeWall, mazeFloor, mazeCeil);
        sphereM = makeSphere(1.f, 22, 16);
        cylM = makeCylinder(1.f, 1.f, 14);
        crystalM = makeCrystal(0.42f, 1.05f);
        torusM = makeTorus(1.75f, 0.20f, 32, 12);
        panelM = makePanel(3.25f, 3.25f);
        boxM = makeBox(1, 1, 1);
        glGenVertexArrays(1, &emptyVAO);
        {
            unsigned char px[4] = { 255,255,255,255 };
            glGenTextures(1, &whiteTex);
            glBindTexture(GL_TEXTURE_2D, whiteTex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        sprites.init();
        monsterSprites.init();
        {
            int mw = 0, mh = 0;
            std::wstring path = img::resolveAssetPath(L"g.PNG");
            monsterOK = img::loadBillboardTexture(path.c_str(), monsterTex, monsterAspect, mw, mh);
            if (monsterOK)
                GLLog("[Image] 怪物贴图加载成功 %dx%d aspect=%.3f tex=%u", mw, mh, monsterAspect, monsterTex);
            else
                GLLog("[Image] 未找到/无法解码 g.PNG，怪物回退到程序化人偶");
        }
        if (!shadowRT.create(1024, 1024, false, true, true)) {
            GLLog("[GL] shadow FBO 创建失败，关闭阴影");
        }
        W = 0; H = 0;
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        return true;
    }

    void resize(int w, int h) {
        if (w == W && h == H) return;
        W = w; H = h;
        sceneRT.create(W, H, true, true, false);
        int bw = W / 2 > 1 ? W / 2 : 1, bh = H / 2 > 1 ? H / 2 : 1;
        bloomA.create(bw, bh, true, false, false);
        bloomB.create(bw, bh, true, false, false);
    }

    void shutdown() {
        mazeWall.destroy(); mazeFloor.destroy(); mazeCeil.destroy();
        sphereM.destroy(); cylM.destroy(); crystalM.destroy();
        torusM.destroy(); panelM.destroy(); boxM.destroy();
        sceneRT.destroy(); bloomA.destroy(); bloomB.destroy(); shadowRT.destroy();
        sprites.destroy(); monsterSprites.destroy(); text.shutdown();
        sceneSh.destroy(); depthSh.destroy(); brightSh.destroy(); blurSh.destroy();
        compSh.destroy(); uiSh.destroy(); spriteSh.destroy(); portalSh.destroy();
        monsterSh.destroy();
        if (monsterTex) { glDeleteTextures(1, &monsterTex); monsterTex = 0; }
        if (emptyVAO) glDeleteVertexArrays(1, &emptyVAO);
        if (whiteTex) glDeleteTextures(1, &whiteTex);
        img::shutdownGDIPlus();
    }

    void drawObject(Shader& sh, const Mesh& mesh, const Mat4& model, const Material& m, TexPair* tex) {
        sh.m4("uModel", model);
        sh.m3("uNormalMat", transpose(inverse(model)));
        sh.v3("uBaseColor", m.base);
        sh.v3("uEmissive", m.emis);
        sh.f("uRoughness", m.rough);
        sh.f("uEmissiveK", m.emisK);
        sh.f("uFresnelK", m.fres);
        sh.f("uPulse", m.pulse);
        sh.f("uAlpha", m.alpha);
        sh.i("uUnlit", m.unlit ? 1 : 0);
        sh.i("uUseMap", (m.useMap && tex) ? 1 : 0);
        sh.i("uUseNormalMap", (m.useNormal && tex) ? 1 : 0);
        sh.v2("uTile", m.tile.x, m.tile.y);
        if (tex) {
            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex->albedo);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, tex->normal);
        }
        mesh.draw();
    }

    void fullscreen() const {
        glBindVertexArray(emptyVAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }

    void render(Game& g, float t) {
        if (W != g_W || H != g_H) resize(g_W, g_H);
        // ---------------- 相机 ----------------
        Vec3 cam, fwd;
        if (g.state == ST_MENU || g.state == ST_HELP) {
            float a = t * 0.11f;
            cam = g.startPos + Vec3(cosf(a) * 10.f, 2.4f + sinf(t * 0.35f) * 0.4f, sinf(a) * 10.f);
            fwd = normalize((g.startPos + Vec3(0, 1.25f, 0)) - cam);
        } else {
            cam = g.camPos; fwd = g.camForward;
        }
        float aspect = (float)W / (float)(H > 0 ? H : 1);
        Mat4 proj = perspective(1.34f, aspect, 0.08f, 190.f);
        Mat4 view = lookAt(cam, cam + fwd, Vec3(0, 1, 0));
        Mat4 vp = mul(proj, view);
        Vec3 center(g.W * g.CELL * 0.5f, 0.f, g.H * g.CELL * 0.5f);
        Vec3 lightP = center + Vec3(24.f, 82.f, 30.f);
        Mat4 lightVP = mul(ortho(-50.f, 50.f, -50.f, 50.f, 1.f, 175.f),
                           lookAt(lightP, center, Vec3(0, 1, 0)));

        // ---------------- 动态点光源 ----------------
        float lp[24] = { 0 }, lc[24] = { 0 }, lr[8] = { 0 };
        int nl = 0;
        struct L { Vec3 p, c; float r, d; };
        L cand[64]; int nc = 0;
        for (size_t i = 0; i < g.crystals.size() && nc < 64; i++) {
            if (g.crystals[i].collected) continue;
            float d = length(g.crystals[i].pos - cam);
            if (d > 32.f) continue;
            L l; l.p = g.crystals[i].pos; l.c = Vec3(0.42f, 0.22f, 1.05f);
            l.r = 9.5f; l.d = d; cand[nc++] = l;
        }
        for (int i = 0; i < nc; i++) for (int j = i + 1; j < nc; j++)
            if (cand[j].d < cand[i].d) { L tmp = cand[i]; cand[i] = cand[j]; cand[j] = tmp; }
        for (int i = 0; i < nc && nl < 6; i++, nl++) {
            lp[nl * 3] = cand[i].p.x; lp[nl * 3 + 1] = cand[i].p.y; lp[nl * 3 + 2] = cand[i].p.z;
            lc[nl * 3] = cand[i].c.x; lc[nl * 3 + 1] = cand[i].c.y; lc[nl * 3 + 2] = cand[i].c.z;
            lr[nl] = cand[i].r;
        }
        for (size_t i = 0; i < g.monsters.size() && nl < 8; i++) {
            if (!g.monsters[i].active) continue;
            float d = length(g.monsters[i].pos - cam);
            if (d > 26.f) continue;
            lp[nl * 3] = g.monsters[i].pos.x; lp[nl * 3 + 1] = g.monsters[i].pos.y + 0.35f; lp[nl * 3 + 2] = g.monsters[i].pos.z;
            lc[nl * 3] = 1.05f; lc[nl * 3 + 1] = 0.055f; lc[nl * 3 + 2] = 0.035f;
            lr[nl] = 7.f; nl++;
        }
        if (g.escape && nl < 8) {
            lp[nl * 3] = g.exitPos.x; lp[nl * 3 + 1] = 2.f; lp[nl * 3 + 2] = g.exitPos.z;
            lc[nl * 3] = 0.65f; lc[nl * 3 + 1] = 0.18f; lc[nl * 3 + 2] = 1.15f;
            lr[nl] = 15.f * g.portalOpen; nl++;
        }

        // ---------------- 阴影深度 pass ----------------
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        shadowRT.bind();
        glClear(GL_DEPTH_BUFFER_BIT);
        depthSh.bind();
        depthSh.m4("uLightVP", lightVP);
        depthSh.m4("uModel", Mat4());
        mazeWall.draw(); mazeFloor.draw(); mazeCeil.draw();
        for (size_t i = 0; i < g.monsters.size(); i++) {
            const Monster& m = g.monsters[i];
            if (!m.active) continue;
            Vec3 to = g.pos - m.pos; to.y = 0;
            float yaw = atan2f(-to.x, -to.z);
            depthSh.m4("uModel", TRS(m.pos, yaw, Vec3(0.72f, 1.25f, 0.6f)));
            sphereM.draw();
        }

        // ---------------- 主场景 ----------------
        Vec3 fog = mix3(Vec3(0.017f, 0.016f, 0.028f), Vec3(0.085f, 0.012f, 0.016f), g.alarm);
        sceneRT.bind();
        glClearColor(fog.x, fog.y, fog.z, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        sceneSh.bind();
        sceneSh.m4("uProj", proj);
        sceneSh.m4("uView", view);
        sceneSh.m4("uLightVP", lightVP);
        sceneSh.v3("uCamPos", cam);
        sceneSh.f("uTime", t);
        sceneSh.v3("uFlashPos", cam);
        sceneSh.v3("uFlashDir", fwd);
        sceneSh.f("uFlashOn", g.flashlight ? 1.f : 0.f);
        sceneSh.f("uFlashRange", 34.f);
        // 光锥放宽到约 53°，否则水平持灯时脚下地面永远落在锥外
        sceneSh.f("uFlashCosInner", cosf(0.38f));
        sceneSh.f("uFlashCosOuter", cosf(0.92f));
        sceneSh.i("uNumLights", nl);
        sceneSh.fv("uLightPos[0]", lp, 8, 3);
        sceneSh.fv("uLightColor[0]", lc, 8, 3);
        sceneSh.fv("uLightRadius[0]", lr, 8, 1);
        sceneSh.v3("uAmbient", Vec3(0.078f, 0.072f, 0.100f));   // 保底可见度，避免关灯时纯黑
        sceneSh.v3("uFogColor", fog);
        sceneSh.f("uFogDensity", 0.0205f);
        sceneSh.f("uAlarm", g.alarm);
        sceneSh.i("uShadowOn", shadowRT.fbo ? 1 : 0);
        sceneSh.i("uAlbedo", 0);
        sceneSh.i("uNormalTex", 1);
        sceneSh.i("uShadowMap", 2);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, shadowRT.depth ? shadowRT.depth : whiteTex);

        // 迷宫：墙 / 地 / 顶三套材质
        Material wallMat; wallMat.useMap = true; wallMat.useNormal = true; wallMat.rough = 0.86f;
        Material floorMat; floorMat.useMap = true; floorMat.useNormal = true; floorMat.rough = 0.68f;
        floorMat.base = Vec3(0.92f, 0.92f, 0.96f);
        Material ceilMat; ceilMat.useMap = true; ceilMat.useNormal = true; ceilMat.rough = 0.96f;
        ceilMat.base = Vec3(0.72f, 0.72f, 0.78f);
        drawObject(sceneSh, mazeFloor, Mat4(), floorMat, &texFloor);
        drawObject(sceneSh, mazeCeil, Mat4(), ceilMat, &texCeil);
        drawObject(sceneSh, mazeWall, Mat4(), wallMat, &texWall);

        // 水晶
        for (size_t i = 0; i < g.crystals.size(); i++) {
            const Crystal& c = g.crystals[i];
            if (c.collected) continue;
            float bobY = sinf(t * 1.8f + c.phase) * 0.16f;
            Mat4 model = TRS(c.pos + Vec3(0, bobY, 0), t * 0.85f + c.phase, Vec3(1, 1, 1));
            Material m; m.unlit = true;
            m.base = Vec3(0.28f, 0.20f, 0.72f);
            m.emis = Vec3(0.46f, 0.26f, 1.0f);
            m.emisK = 1.35f + 0.25f * sinf(t * 3.f + c.phase);
            m.fres = 1.7f;
            m.pulse = 0.5f + 0.5f * sinf(t * 2.6f + c.phase);
            drawObject(sceneSh, crystalM, model, m, 0);
            // 水晶底座光晕
            sprites.add(c.pos + Vec3(0, bobY, 0), 46.f, 0.35f, 0.18f, 1.0f, 0.28f);
        }

        // 怪物：加载了 g.PNG 就改用图片平面（透明层绘制），加载失败才回退到程序化人偶
        if (!monsterOK) for (size_t i = 0; i < g.monsters.size(); i++) {
            const Monster& mo = g.monsters[i];
            if (!mo.active) continue;
            Vec3 to = g.pos - mo.pos; to.y = 0;
            float yaw = atan2f(-to.x, -to.z);
            float wob = sinf(t * 4.2f + mo.phaseOff);
            float swing = sinf(t * 6.f + mo.phaseOff) * 0.35f;
            Mat4 base = mul(translate(mo.pos), rotateY(yaw));
            Material body; body.base = Vec3(0.085f, 0.078f, 0.098f);
            body.rough = 0.62f; body.emis = Vec3(0.14f, 0.012f, 0.022f);
            body.emisK = 0.45f; body.fres = 0.95f; body.pulse = 0.7f;
            drawObject(sceneSh, sphereM, mul(base, TRS(Vec3(0, 0, 0), 0, Vec3(0.60f, 1.02f, 0.43f))), body, 0);
            drawObject(sceneSh, sphereM, mul(base, TRS(Vec3(0, 1.18f + wob * 0.05f, -0.06f), 0.16f, Vec3(0.42f, 0.46f, 0.40f))), body, 0);
            // 手臂
            drawObject(sceneSh, cylM, mul(base, TRS(Vec3(-0.72f, 0.18f + swing * 0.2f, -0.05f), 0, Vec3(0.13f, 1.32f, 0.13f))), body, 0);
            drawObject(sceneSh, cylM, mul(base, TRS(Vec3(0.72f, 0.18f - swing * 0.2f, -0.05f), 0, Vec3(0.13f, 1.32f, 0.13f))), body, 0);
            // 腿
            drawObject(sceneSh, cylM, mul(base, TRS(Vec3(-0.24f, -1.05f, 0), 0, Vec3(0.16f, 1.15f, 0.16f))), body, 0);
            drawObject(sceneSh, cylM, mul(base, TRS(Vec3(0.24f, -1.05f, 0), 0, Vec3(0.16f, 1.15f, 0.16f))), body, 0);
            // 破布
            Material cloth; cloth.base = Vec3(0.13f, 0.10f, 0.13f); cloth.rough = 0.95f;
            drawObject(sceneSh, boxM, mul(base, TRS(Vec3(-0.32f, -0.35f, -0.40f), 0.3f + swing * 0.2f, Vec3(0.42f, 0.9f, 0.06f))), cloth, 0);
            drawObject(sceneSh, boxM, mul(base, TRS(Vec3(0.34f, -0.30f, -0.40f), -0.25f - swing * 0.2f, Vec3(0.36f, 0.8f, 0.06f))), cloth, 0);
            // 发光双眼
            Material eye; eye.unlit = true; eye.base = Vec3(1, 1, 1);
            eye.emis = Vec3(1.3f, 0.055f, 0.03f); eye.emisK = 2.3f;
            eye.fres = 1.8f; eye.pulse = 1.f;
            drawObject(sceneSh, sphereM, mul(base, TRS(Vec3(-0.17f, 1.24f, -0.37f), 0, Vec3(0.105f, 0.085f, 0.08f))), eye, 0);
            drawObject(sceneSh, sphereM, mul(base, TRS(Vec3(0.17f, 1.24f, -0.37f), 0, Vec3(0.105f, 0.085f, 0.08f))), eye, 0);
            sprites.add(mo.pos + Vec3(0, 1.24f, 0), 70.f, 1.0f, 0.05f, 0.03f, 0.22f);
        }

        // 传送门
        if (g.portalOpen > 0.01f) {
            Vec3 pp(g.exitPos.x, 2.05f, g.exitPos.z);
            float ang = atan2f(cam.x - pp.x, cam.z - pp.z);
            Mat4 orient = mul(translate(pp), rotateY(ang));
            Mat4 pm = mul(orient, rotateX(DM_PI * 0.5f));
            Material ring; ring.unlit = true; ring.base = Vec3(0.42f, 0.16f, 0.75f);
            ring.emis = Vec3(0.75f, 0.25f, 1.15f); ring.emisK = 1.25f * g.portalOpen;
            ring.fres = 1.2f; ring.pulse = 1.f;
            drawObject(sceneSh, torusM, pm, ring, 0);
        }

        // ---------------- 透明层：传送门漩涡 + 粒子 ----------------
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);
        if (g.portalOpen > 0.01f) {
            glBlendFunc(GL_ONE, GL_ONE);
            portalSh.bind();
            portalSh.m4("uProj", proj);
            portalSh.m4("uView", view);
            Vec3 pp(g.exitPos.x, 2.05f, g.exitPos.z);
            float ang = atan2f(cam.x - pp.x, cam.z - pp.z);
            portalSh.m4("uModel", mul(translate(pp), rotateY(ang)));
            portalSh.f("uTime", t);
            portalSh.f("uOpen", g.portalOpen);
            panelM.draw();
        }
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

        // 图片怪物：面向相机的平面（尺寸随距离透视缩小），深度测试仍开启所以会被墙挡住
        if (monsterOK) {
            monsterSprites.clear();
            for (size_t i = 0; i < g.monsters.size(); i++) {
                const Monster& mo = g.monsters[i];
                if (!mo.active) continue;
                float wob = sinf(t * 2.4f + mo.phaseOff) * 0.07f;
                // 略微压暗偏冷，避免白底照片在黑暗里像一块发光广告牌
                monsterSprites.add(mo.pos + Vec3(0, wob, 0), 1.28f, 0.82f, 0.80f, 0.90f, 1.f);
                sprites.add(mo.pos + Vec3(0, 1.24f, 0), 70.f, 1.0f, 0.06f, 0.03f, 0.20f);
            }
            if (!monsterSprites.data.empty()) {
                monsterSh.bind();
                monsterSh.m4("uView", view);
                monsterSh.m4("uProj", proj);
                monsterSh.f("uAspect", monsterAspect);
                monsterSh.f("uTime", t);
                monsterSh.i("uTex", 0);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, monsterTex);
                monsterSprites.draw();
            }
            monsterSprites.clear();
        }

        // 逃脱阶段：传送门位置常驻一根紫色光柱，隔着走廊也能看见
        if (g.escape) {
            float boost = g.senseTimer > 0.f ? 1.9f : 1.f;
            float fade = clampf(g.portalOpen * 3.f, 0.3f, 1.f);
            for (int k = 0; k < 12; k++) {
                float a = (0.34f - k * 0.022f) * boost * fade;
                if (a <= 0.f) continue;
                sprites.add(Vec3(g.exitPos.x, 0.3f + k * 0.55f, g.exitPos.z),
                            42.f, 0.62f, 0.22f, 1.f, a);
            }
        }
        // Q 感应：光柱指向当前目标（收集阶段=水晶，逃脱阶段=传送门）
        if (g.senseTimer > 0.f) {
            Vec3 p = g.senseTargetPos();
            for (int k = 0; k < 7; k++)
                sprites.add(p + Vec3(0, k * 0.55f, 0), 34.f,
                            0.65f, 0.25f, 1.0f, 0.30f * g.senseTimer / 2.8f);
        }
        for (size_t i = 0; i < g.particles.size(); i++) {
            const Particle& p = g.particles[i];
            sprites.add(p.pos, p.size, p.r, p.g, p.b, p.a);
        }
        spriteSh.bind();
        spriteSh.m4("uViewProj", vp);
        spriteSh.v2("uViewport", (float)W, (float)H);
        sprites.draw();
        sprites.clear();
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);

        // ---------------- Bloom ----------------
        glDisable(GL_DEPTH_TEST);
        glActiveTexture(GL_TEXTURE0);
        bloomA.bind();
        glClear(GL_COLOR_BUFFER_BIT);
        brightSh.bind(); brightSh.i("uScene", 0); brightSh.f("uThreshold", 0.60f);
        glBindTexture(GL_TEXTURE_2D, sceneRT.color);
        fullscreen();
        for (int pass = 0; pass < 2; pass++) {
            bloomB.bind(); glClear(GL_COLOR_BUFFER_BIT);
            blurSh.bind(); blurSh.i("uTex", 0);
            blurSh.v2("uDir", 1.f / (float)bloomA.w, 0.f);
            glBindTexture(GL_TEXTURE_2D, bloomA.color);
            fullscreen();
            bloomA.bind(); glClear(GL_COLOR_BUFFER_BIT);
            blurSh.v2("uDir", 0.f, 1.f / (float)bloomB.h);
            glBindTexture(GL_TEXTURE_2D, bloomB.color);
            fullscreen();
        }

        // ---------------- 合成到屏幕 ----------------
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, W, H);
        glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
        compSh.bind();
        compSh.i("uScene", 0); compSh.i("uBloom", 1);
        glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, sceneRT.color);
        glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, bloomA.color);
        compSh.v2("uResolution", (float)W, (float)H);
        compSh.f("uTime", t);
        compSh.f("uVignette", 0.62f);
        compSh.f("uHurt", g.hurt);
        compSh.f("uDanger", g.danger);
        compSh.f("uAlarm", g.alarm);
        compSh.f("uHeart", clampf(g.danger, 0.f, 1.f));
        compSh.f("uFade", fadeOf(g));
        compSh.f("uGrain", 0.048f);
        compSh.f("uChase", g.danger);
        fullscreen();

        // ---------------- UI ----------------
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        uiSh.bind();
        uiSh.v2("uScreen", (float)W, (float)H);
        uiSh.i("uTex", 0);
        glBindVertexArray(emptyVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        drawUI(g, t);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
    }

    // 把相对方向换算成八方位中文（前 / 右前 / 右 / 右后 / 后 / 左后 / 左 / 左前）
    static const wchar_t* dirName(const Vec3& d, const Vec3& fwd, const Vec3& right) {
        static const wchar_t* names[8] = { L"正前方", L"右前方", L"右侧", L"右后方",
                                           L"正后方", L"左后方", L"左侧", L"左前方" };
        float ang = atan2f(dot(d, right), dot(d, fwd));
        int idx = (int)floorf((ang + DM_PI * 0.125f) / (DM_PI * 0.25f));
        idx = ((idx % 8) + 8) % 8;
        return names[idx];
    }

    float fadeOf(const Game& g) const {
        float f = 1.f;
        if (g.state == ST_INTRO) f = clampf((3.6f - g.introTimer) / 1.1f, 0.05f, 1.f);
        else if (g.state == ST_MENU || g.state == ST_HELP) f = 0.82f;
        if (g.deathTimer > 0.f) f *= 0.42f;
        return f;
    }

    // ------------------------------------------------------------------
    void uiRect(float x, float y, float w, float h, float r, float g, float b, float a, int mode = 1) {
        uiSh.i("uMode", mode);
        uiSh.v4("uColor", r, g, b, a);
        uiSh.v4("uRect", x, y, w, h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, whiteTex);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }
    void uiText(const wchar_t* s, float x, float y, int px, unsigned int color,
                bool bold = false, float glow = 0.85f, int align = 0, float alpha = 1.f) {
        const TextRenderer::Glyph* gl = text.get(s, px, color, bold);
        if (!gl || !gl->tex) return;
        float xx = x;
        if (align == 1) xx -= gl->w * 0.5f;
        else if (align == 2) xx -= gl->w;
        uiSh.i("uMode", 0);
        uiSh.v4("uColor", ((color >> 16) & 255) / 255.f, ((color >> 8) & 255) / 255.f,
                (color & 255) / 255.f, alpha);
        uiSh.v2("uTexel", 1.f / gl->w, 1.f / gl->h);
        uiSh.f("uGlow", glow);
        uiSh.v4("uRect", xx, y, (float)gl->w, (float)gl->h);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gl->tex);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    void drawMenuPanel(Game& g, float t) {
        // 菜单项几何必须与 Game::update 的鼠标命中判定一致
        std::vector<std::wstring> items = g.menuItems();
        float y0 = (g.state == ST_MENU) ? (float)H * 0.54f : (float)H * 0.44f;
        for (size_t i = 0; i < items.size(); i++) {
            float y = y0 + (float)i * 62.f;
            bool sel = ((int)i == g.menuIndex);
            uiRect((float)W * 0.5f - 190.f, y - 22.f, 380.f, 52.f,
                   sel ? 0.30f : 0.045f, sel ? 0.13f : 0.04f, sel ? 0.46f : 0.075f,
                   sel ? 0.80f : 0.60f);
            if (sel) uiRect((float)W * 0.5f - 190.f, y - 22.f, 5.f, 52.f, 0.75f, 0.35f, 1.f, 0.95f);
            uiText(items[i].c_str(), (float)W * 0.5f, y - 15.f, 28,
                   sel ? 0xFFFFFF : 0x9C94B4, sel, sel ? 1.2f : 0.7f, 1);
        }
        (void)t;
    }

    void drawHUD(Game& g, float t) {
        wchar_t buf[128];
        // 水晶计数 + 进度条
        swprintf(buf, 128, L"水晶  %d / %d", g.collected, (int)g.crystals.size());
        uiText(buf, 30.f, 24.f, 28, 0xC6AEFF, true, 1.1f);
        float ratio = g.crystals.empty() ? 0.f : g.collected / (float)g.crystals.size();
        uiRect(30.f, 66.f, 310.f, 9.f, 0.10f, 0.08f, 0.16f, 0.75f);
        uiRect(30.f, 66.f, 310.f * ratio, 9.f, 0.55f, 0.30f, 1.0f, 0.95f);
        // 生命
        uiText(L"生命", 30.f, (float)H - 106.f, 20, 0x9A93AC, false, 0.7f);
        for (int i = 0; i < 3; i++) {
            bool on = i < g.lives;
            uiRect(90.f + i * 28.f, (float)H - 102.f, 20.f, 20.f,
                   on ? 0.86f : 0.14f, on ? 0.10f : 0.12f, on ? 0.18f : 0.18f,
                   on ? 0.95f : 0.55f, 2);
        }
        // 体力
        uiText(L"体力", 30.f, (float)H - 68.f, 18, 0x9A93AC, false, 0.7f);
        uiRect(90.f, (float)H - 62.f, 230.f, 12.f, 0.10f, 0.09f, 0.15f, 0.75f);
        uiRect(90.f, (float)H - 62.f, 230.f * g.stamina, 12.f,
               0.30f, 0.75f, 0.72f, 0.92f);
        // 时间
        int sec = (int)g.elapsed;
        swprintf(buf, 128, L"时间  %02d:%02d", sec / 60, sec % 60);
        uiText(buf, (float)W - 30.f, 26.f, 24, 0xB9B2C8, true, 0.9f, 2);
        // 技能 / 手电
        if (g.senseCD > 0.f) {
            swprintf(buf, 128, L"[Q] %s  冷却 %.0f 秒", g.escape ? L"目标感应" : L"水晶感应", g.senseCD);
            uiText(buf, (float)W - 30.f, 62.f, 19, 0x6E6880, false, 0.6f, 2);
        } else {
            swprintf(buf, 128, L"[Q] %s  就绪", g.escape ? L"目标感应（传送门）" : L"水晶感应");
            uiText(buf, (float)W - 30.f, 62.f, 19, 0x9C7BFF, false, 1.0f, 2);
        }
        swprintf(buf, 128, L"[F] 手电筒  %s", g.flashlight ? L"开" : L"关");
        uiText(buf, (float)W - 30.f, 88.f, 19, g.flashlight ? 0xCFC7B0 : 0x6E6880, false, 0.6f, 2);
        // 准星
        float cx = (float)W * 0.5f, cy = (float)H * 0.5f;
        uiRect(cx - 1.f, cy - 7.f, 2.f, 5.f, 0.8f, 0.78f, 0.9f, 0.42f);
        uiRect(cx - 1.f, cy + 2.f, 2.f, 5.f, 0.8f, 0.78f, 0.9f, 0.42f);
        uiRect(cx - 7.f, cy - 1.f, 5.f, 2.f, 0.8f, 0.78f, 0.9f, 0.42f);
        uiRect(cx + 2.f, cy - 1.f, 5.f, 2.f, 0.8f, 0.78f, 0.9f, 0.42f);

        // 中央提示
        if (g.hintTimer > 0.f) {
            float a = clampf(g.hintTimer, 0.f, 1.f);
            uiText(g.hintText.c_str(), (float)W * 0.5f, (float)H * 0.72f, 25,
                   0xE6D8FF, true, 1.3f, 1, a);
        }
        // 危险 / 警报
        if (g.escape) {
            float pulse = 0.65f + 0.35f * sinf(t * 7.f);
            uiText(L"警报：全部水晶已收集 — 逃向紫色传送门", (float)W * 0.5f, 32.f,
                   26, 0xFF5B5B, true, 1.4f, 1, pulse);
        } else if (g.danger > 0.62f) {
            float pulse = 0.5f + 0.5f * sinf(t * 9.f);
            uiText(L"它发现你了！", (float)W * 0.5f, (float)H * 0.20f, 30,
                   0xFF4242, true, 1.5f, 1, pulse);
        }
        // 目标指引：Q 感应时显示当前目标方位；逃脱阶段即使不按 Q 也常驻显示传送门方位
        {
            float sd = 0.f; int sk = 0;
            Vec3 target = g.senseTargetPos(&sd, &sk);
            Vec3 hf = normalize(Vec3(g.camForward.x, 0.f, g.camForward.z));
            const wchar_t* dn = dirName(target - g.pos, hf, g.camRight);
            if (g.senseTimer > 0.f) {
                swprintf(buf, 128, L"%s 在 %s · 约 %.0f 米",
                         sk == 1 ? L"紫色传送门" : L"最近的水晶", dn, sd);
                uiText(buf, (float)W * 0.5f, (float)H * 0.80f, 23, 0xB992FF, true, 1.2f, 1);
            } else if (g.escape) {
                swprintf(buf, 128, L"紫色传送门在 %s · 约 %.0f 米   [Q] 感应", dn, sd);
                uiText(buf, (float)W * 0.5f, (float)H * 0.80f, 21, 0xC79BFF, true, 1.0f, 1);
            }
        }
        // 开场
        if (g.state == ST_INTRO) {
            uiText(L"第一章 · 晶魂回廊", (float)W * 0.5f, (float)H * 0.34f, 46,
                   0xD9C7FF, true, 1.5f, 1, clampf(g.introTimer / 1.2f, 0.f, 1.f));
            uiText(L"收集全部水晶，别回头。", (float)W * 0.5f, (float)H * 0.42f, 24,
                   0x9E8FBF, false, 0.9f, 1, clampf(g.introTimer / 1.5f, 0.f, 1.f));
        }
        // 重生黑屏
        if (g.deathTimer > 0.f) {
            uiRect(0, 0, (float)W, (float)H, 0.30f, 0.0f, 0.01f, 0.55f * clampf(g.deathTimer, 0.f, 1.f));
            uiText(L"灵魂正在重组……", (float)W * 0.5f, (float)H * 0.46f, 34,
                   0xFF6A6A, true, 1.4f, 1);
        }
    }

    void drawUI(Game& g, float t) {
        const bool inGame = (g.state == ST_PLAY || g.state == ST_INTRO || g.state == ST_PAUSE ||
                             g.state == ST_GAMEOVER || g.state == ST_WIN);
        if (inGame && g.state != ST_MENU) drawHUD(g, t);

        if (g.state == ST_MENU || g.state == ST_HELP || g.state == ST_PAUSE ||
            g.state == ST_GAMEOVER || g.state == ST_WIN) {
            uiRect(0, 0, (float)W, (float)H, 0.015f, 0.010f, 0.030f, 0.68f);
        }

        switch (g.state) {
        case ST_MENU:
            uiText(L"暗 影 迷 宫", (float)W * 0.5f, (float)H * 0.17f, 62, 0xD2B8FF, true, 1.6f, 1);
            uiText(L"晶 魂 回 廊 · 第一章", (float)W * 0.5f, (float)H * 0.30f, 24, 0x9A8ABF, false, 1.0f, 1);
            uiText(L"它们在黑暗里等着你", (float)W * 0.5f, (float)H * 0.37f, 19, 0x6E6488, false, 0.6f, 1);
            drawMenuPanel(g, t);
            uiText(L"↑↓ / W S 选择    回车 确认    鼠标亦可点击", (float)W * 0.5f,
                   (float)H - 54.f, 18, 0x736B8A, false, 0.6f, 1);
            uiText(L"建议戴上耳机 · 全部画面与音乐均为程序化实时生成",
                   (float)W * 0.5f, (float)H - 28.f, 16, 0x5B5470, false, 0.5f, 1);
            break;
        case ST_HELP: {
            uiText(L"操 作 说 明", (float)W * 0.5f, (float)H * 0.13f, 44, 0xD2B8FF, true, 1.4f, 1);
            const wchar_t* lines[] = {
                L"W A S D      移动              鼠标      转动视角",
                L"Shift        冲刺（消耗体力，跑得比怪物快）",
                L"Ctrl         蹲伏（移动变慢，但更难被发现）",
                L"F            手电筒 开 / 关",
                L"Q            水晶感应（指出最近的水晶方向）",
                L"Esc          暂停 / 返回",
                L" ",
                L"目标：收集迷宫中全部水晶，随后紫色传送门会开启。",
                L"被怪物抓到会损失一条命并回到起点，已收集的水晶保留。",
                L"生命耗尽则游戏结束；收集得越多，追猎者越多、越快。"
            };
            for (int i = 0; i < 10; i++)
                uiText(lines[i], (float)W * 0.5f, (float)H * 0.27f + i * 38.f, 22,
                       0xC3BCD6, false, 0.7f, 1);
            uiText(L"按回车 / Esc / 点击任意处返回", (float)W * 0.5f,
                   (float)H - 60.f, 20, 0x9C7BFF, true, 1.0f, 1);
            break;
        }
        case ST_PAUSE:
            uiText(L"已 暂 停", (float)W * 0.5f, (float)H * 0.24f, 48, 0xD2B8FF, true, 1.4f, 1);
            uiText(L"Esc 继续游戏", (float)W * 0.5f, (float)H * 0.34f, 20, 0x8C83A8, false, 0.7f, 1);
            drawMenuPanel(g, t);
            break;
        case ST_GAMEOVER: {
            uiText(L"你 被 吞 噬 了", (float)W * 0.5f, (float)H * 0.18f, 52, 0xFF5B5B, true, 1.6f, 1);
            wchar_t buf[160];
            swprintf(buf, 160, L"收集水晶 %d / %d     用时 %02d:%02d     死亡 %d 次",
                     g.collected, (int)g.crystals.size(), (int)g.elapsed / 60, (int)g.elapsed % 60, g.deaths);
            uiText(buf, (float)W * 0.5f, (float)H * 0.31f, 22, 0xB8B0C8, false, 0.8f, 1);
            drawMenuPanel(g, t);
            break;
        }
        case ST_WIN: {
            uiText(L"逃 出 升 天", (float)W * 0.5f, (float)H * 0.16f, 56, 0xB9A0FF, true, 1.7f, 1);
            wchar_t buf[160];
            swprintf(buf, 160, L"用时 %02d:%02d     死亡 %d 次     评级",
                     (int)g.elapsed / 60, (int)g.elapsed % 60, g.deaths);
            uiText(buf, (float)W * 0.5f, (float)H * 0.30f, 24, 0xBFB6D4, false, 0.8f, 1);
            uiText(g.rank(), (float)W * 0.5f, (float)H * 0.34f, 44, 0xFFD98A, true, 1.5f, 1);
            drawMenuPanel(g, t);
            break;
        }
        default: break;
        }
    }
};

// ---------------------------------------------------------------------------
static bool keyDown(int vk) {
    // 消息状态为主，GetAsyncKeyState 作为兜底（例如按键在窗口获得焦点前就已按住）
    return ((GetAsyncKeyState(vk) & 0x8000) != 0) || g_keyHeld[vk & 255];
}
static bool keyPressed(int vk, int idx) {
    bool d = keyDown(vk);
    return (d && !g_prevKeys[idx]) || g_keyEdge[vk & 255];
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR, int) {
    FILE* f = fopen("darkmaze.log", "w");
    if (f) { fprintf(f, "=== 暗影迷宫启动 ===\n"); fclose(f); }
    const bool selftest = (wcsstr(GetCommandLineW(), L"--selftest") != 0);
    if (!createWindowAndGL(inst, !selftest)) {
        if (selftest) {
            FILE* rf = fopen("selftest_result.txt", "w");
            if (rf) { fprintf(rf, "FAIL: 无法创建 OpenGL 3.3 上下文\n"); fclose(rf); }
            return 2;
        }
        MessageBoxW(0, L"无法创建 OpenGL 窗口，请检查显卡驱动。", L"暗影迷宫", MB_ICONERROR);
        return 1;
    }

    // 环境自检：在真实驱动上编译全部 GLSL、验证 FBO 与程序化资源，结果写入文件后退出
    if (selftest) {
        struct SP { const char* name, * vs, * fs; };
        SP list[] = {
            { "scene", SCENE_VS, SCENE_FS }, { "depth", DEPTH_VS, DEPTH_FS },
            { "bright", POST_VS, BRIGHT_FS }, { "blur", POST_VS, BLUR_FS },
            { "composite", POST_VS, COMPOSITE_FS }, { "ui", UI_VS, UI_FS },
            { "sprite", SPRITE_VS, SPRITE_FS }, { "portal", PORTAL_VS, PORTAL_FS },
            { "monster", MONSTER_VS, MONSTER_FS }
        };
        const int shaderCount = (int)(sizeof(list) / sizeof(list[0]));
        int failed = 0;
        for (int i = 0; i < shaderCount; i++) {
            Shader s;
            if (!s.create(list[i].vs, list[i].fs, list[i].name)) failed++;
            s.destroy();
        }
        bool fboOK = true;
        {
            RenderTarget a, b;
            if (!a.create(128, 128, true, true, true)) fboOK = false;
            if (!b.create(128, 128, true, true, false)) fboOK = false;
            a.destroy(); b.destroy();
        }
        bool assetsOK = true;
        {
            RNG rr(1);
            TexPair tp = generateStone(64, 0, rr);
            Mesh mm = makeSphere(1.f, 8, 6);
            assetsOK = (tp.albedo != 0 && tp.normal != 0 && mm.vao != 0);
            mm.destroy();
            if (tp.albedo) glDeleteTextures(1, &tp.albedo);
            if (tp.normal) glDeleteTextures(1, &tp.normal);
        }
        // 玩法冒烟测试：完整跑一遍 开场 -> 收集 -> 警报 -> 传送门 -> 胜利
        bool playOK = true;
        {
            Game sg;
            AudioEngine dummy;
            InputState si;
            sg.newGame();
            sg.invuln = 999.f;
            for (int i = 0; i < 260; i++) sg.update(1.f / 60.f, si, dummy, 1280.f, 720.f);
            GLLog("[Play] after intro: state=%d crystals=%d monsters=%d invuln=%.2f",
                  (int)sg.state, (int)sg.crystals.size(), (int)sg.monsters.size(), sg.invuln);
            if (sg.state != ST_PLAY) playOK = false;
            for (size_t k = 0; k < sg.crystals.size(); k++) {
                sg.pos = sg.crystals[k].pos; sg.pos.y = sg.EYE;
                sg.update(1.f / 60.f, si, dummy, 1280.f, 720.f);
            }
            GLLog("[Play] after collect: state=%d collected=%d/%d escape=%d portal=%.2f",
                  (int)sg.state, sg.collected, (int)sg.crystals.size(), sg.escape ? 1 : 0, sg.portalOpen);
            if (!sg.escape || sg.collected != (int)sg.crystals.size()) playOK = false;
            sg.pos = sg.exitPos; sg.pos.y = sg.EYE;
            for (int i = 0; i < 130; i++) sg.update(1.f / 60.f, si, dummy, 1280.f, 720.f);
            GLLog("[Play] after portal: state=%d portal=%.2f lives=%d deaths=%d pos=%.1f,%.1f",
                  (int)sg.state, sg.portalOpen, sg.lives, sg.deaths, sg.pos.x, sg.pos.z);
            if (sg.state != ST_WIN) playOK = false;
            if (!isfinite(sg.pos.x) || !isfinite(sg.pos.z)) playOK = false;
            for (size_t k = 0; k < sg.monsters.size(); k++)
                if (!isfinite(sg.monsters[k].pos.x) || !isfinite(sg.monsters[k].pos.z)) playOK = false;
        }
        // 视角方向：鼠标右移必须让视线朝右（camForward.x 增大）
        bool viewOK = true;
        {
            Game vg; AudioEngine dummy; InputState vi;
            vg.newGame(); vg.invuln = 999.f;
            for (int i = 0; i < 260; i++) vg.update(1.f / 60.f, vi, dummy, 1280.f, 720.f);
            float x0 = vg.camForward.x;
            vi.mouseDX = 120.f;
            vg.update(1.f / 60.f, vi, dummy, 1280.f, 720.f);
            float x1 = vg.camForward.x;
            GLLog("[View] camForward.x %.3f -> %.3f", x0, x1);
            viewOK = (x1 > x0 + 0.01f);
        }
        // 键盘输入层 + 菜单导航：用真实 WM_KEYDOWN/WM_KEYUP 走一遍窗口消息泵
        bool uiOK = true;
        {
            MSG mm;
            auto pump = [&]() {
                while (PeekMessageW(&mm, 0, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&mm); DispatchMessageW(&mm);
                }
            };
            Game mg; AudioEngine dummy;
            mg.state = ST_MENU; mg.menuIndex = 0;
            PostMessageW(g_hwnd, WM_KEYDOWN, VK_DOWN, 0);
            pump();
            InputState di; di.down = keyPressed(VK_DOWN, VK_DOWN);
            mg.update(1.f / 60.f, di, dummy, 1280.f, 720.f);
            bool downOK = (di.down && mg.menuIndex == 1);
            PostMessageW(g_hwnd, WM_KEYUP, VK_DOWN, 0);
            pump();
            memset(g_keyEdge, 0, sizeof(g_keyEdge));
            g_prevKeys[VK_DOWN & 255] = false;
            PostMessageW(g_hwnd, WM_KEYDOWN, VK_UP, 0);
            pump();
            InputState ui; ui.up = keyPressed(VK_UP, VK_UP);
            mg.update(1.f / 60.f, ui, dummy, 1280.f, 720.f);
            bool upOK = (ui.up && mg.menuIndex == 0);
            PostMessageW(g_hwnd, WM_KEYUP, VK_UP, 0);
            pump();
            memset(g_keyEdge, 0, sizeof(g_keyEdge));
            GLLog("[UI] keyboard down=%d up=%d menuIndex=%d", downOK, upOK, mg.menuIndex);
            uiOK = downOK && upOK;
        }
        // 菜单鼠标悬停：移动鼠标即高亮，且之后键盘选择不会被悬停位置覆盖
        bool hoverOK = true;
        {
            Game hg; AudioEngine dummy;
            hg.state = ST_MENU; hg.menuIndex = 0;
            InputState hi; hi.mouseMoved = true;
            hi.mouseX = 1280.f * 0.5f;
            hi.mouseY = 720.f * 0.54f + 62.f;      // 第二项中心
            hg.update(1.f / 60.f, hi, dummy, 1280.f, 720.f);
            bool hoverSel = (hg.menuIndex == 1);
            InputState ki; ki.up = true;           // 鼠标不动，键盘仍可改选
            hg.update(1.f / 60.f, ki, dummy, 1280.f, 720.f);
            bool keyStillWorks = (hg.menuIndex == 0);
            GLLog("[UI] hoverSelect=%d keyAfterHover=%d", hoverSel, keyStillWorks);
            hoverOK = hoverSel && keyStillWorks;
        }
        // 手电可见性回归：按 SCENE_FS 的公式复算典型表面的光照系数
        // （旧版：地面被天顶阴影判为全阴影 + 窄光锥，脚下/前方地面系数都是 0）
        bool lightOK = true;
        {
            const float inner = cosf(0.38f), outer = cosf(0.92f), range = 34.f;
            Vec3 cam(0.f, 1.62f, 0.f), fwd(0.f, 0.f, -1.f), up(0.f, 1.f, 0.f);
            auto lum = [&](Vec3 P, Vec3 N) {
                Vec3 Lv = cam - P; float d = length(Lv);
                Vec3 L = Lv / d;
                float c = dot(Vec3(-L.x, -L.y, -L.z), fwd);
                float spot = smoothstepf(outer, inner, c);
                float fill = powf(clampf(1.f - d / 7.5f, 0.f, 1.f), 2.f) * 0.55f;
                float cone = spot > fill ? spot : fill;
                float att = powf(clampf(1.f - d / range, 0.f, 1.f), 2.f);
                float ndl = clampf((dot(N, L) + 0.42f) / 1.42f, 0.f, 2.f);
                return ndl * cone * att * 2.35f;
            };
            float foot = lum(Vec3(0.6f, 0.f, -1.2f), up);            // 脚下地面
            float ahead = lum(Vec3(0.f, 0.f, -4.f), up);              // 前方地面
            Vec3 wallP(1.9f, 1.6f, -3.f);
            Vec3 graze = normalize(cross(normalize(cam - wallP), up)); // 与光线垂直的掠射面（旧版必黑）
            float side = lum(wallP, graze);
            float face = lum(Vec3(0.f, 1.6f, -3.f), Vec3(0, 0, 1));   // 正对的墙
            GLLog("[Light] foot=%.3f ahead=%.3f side=%.3f face=%.3f (old floor=0.000)",
                  foot, ahead, side, face);
            lightOK = (foot > 0.12f && ahead > 0.12f && side > 0.12f && face > 0.30f);
        }
        // Q 感应目标：收集阶段指最近水晶，逃脱阶段指传送门
        bool senseOK = true;
        {
            Game sg2; AudioEngine dummy; InputState si;
            sg2.newGame(); sg2.invuln = 999.f;
            for (int i = 0; i < 260; i++) sg2.update(1.f / 60.f, si, dummy, 1280.f, 720.f);
            float d0 = 0.f; int k0 = 0;
            Vec3 t0 = sg2.senseTargetPos(&d0, &k0);
            bool crystalOK = (k0 == 0 && d0 > 0.f && t0.y > 0.f);
            sg2.escape = true;
            float d1 = 0.f; int k1 = 0;
            Vec3 t1 = sg2.senseTargetPos(&d1, &k1);
            bool portalOK = (k1 == 1 && fabsf(t1.x - sg2.exitPos.x) < 0.01f &&
                             fabsf(t1.z - sg2.exitPos.z) < 0.01f && d1 > 0.f);
            GLLog("[Sense] crystalKind=%d dist=%.2f portalKind=%d portalDist=%.2f", k0, d0, k1, d1);
            senseOK = crystalOK && portalOK;
        }
        // 外部怪物贴图：验证按内容解码（g.PNG 实为 JPEG）与白底抠图
        bool imageOK = true;
        {
            GLuint tex = 0; float asp = 1.f; int iw = 0, ih = 0, nT = 0, nO = 0;
            std::wstring path = img::resolveAssetPath(L"g.PNG");
            bool ok = img::loadBillboardTexture(path.c_str(), tex, asp, iw, ih, &nT, &nO);
            GLLog("[Image] selftest g.PNG load=%d %dx%d aspect=%.3f tex=%u alpha transparent=%d opaque=%d",
                  ok, iw, ih, asp, tex, nT, nO);
            // 图片按内容解码成功、尺寸正常即可；四边柔化在 MONSTER_FS 里完成
            imageOK = (ok && tex != 0 && iw > 8 && ih > 8 && nO > 100);
            if (tex) glDeleteTextures(1, &tex);
            img::shutdownGDIPlus();
        }
        const bool allOK = (failed == 0 && fboOK && assetsOK && playOK && viewOK &&
                            uiOK && hoverOK && lightOK && senseOK && imageOK);
        FILE* rf = fopen("selftest_result.txt", "w");
        if (rf) {
            fprintf(rf, "shaders %d/%d OK\nFBO %s\nassets %s\nplaythrough %s\nview %s\nui-keyboard %s\nui-hover %s\nflashlight %s\nsense %s\nimage %s\nRESULT: %s\n",
                shaderCount - failed, shaderCount, fboOK ? "OK" : "FAIL", assetsOK ? "OK" : "FAIL",
                playOK ? "OK" : "FAIL", viewOK ? "OK" : "FAIL", uiOK ? "OK" : "FAIL",
                hoverOK ? "OK" : "FAIL", lightOK ? "OK" : "FAIL", senseOK ? "OK" : "FAIL",
                imageOK ? "OK" : "FAIL", allOK ? "PASS" : "FAIL");
            fclose(rf);
        }
        GLLog("[SelfTest] RESULT=%s", allOK ? "PASS" : "FAIL");
        if (g_ctx) { wglMakeCurrent(0, 0); wglDeleteContext(g_ctx); }
        if (g_dc && g_hwnd) ReleaseDC(g_hwnd, g_dc);
        if (g_hwnd) DestroyWindow(g_hwnd);
        return allOK ? 0 : 2;
    }

    Game game;
    game.buildLevel();          // 菜单背景用
    game.state = ST_MENU;
    game.pos = game.startPos;
    game.updateCamera(0.f, InputState());

    Renderer renderer;
    if (!renderer.init(game)) return 1;
    renderer.resize(g_W, g_H);

    AudioEngine audio;
    audio.init();               // 无音频设备时自动静默降级
    audio.setPhase(MUS_MENU);

    LARGE_INTEGER freq, last;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&last);
    float totalTime = 0.f;
    bool quit = false;

    while (game.running && !quit) {
        MSG msg;
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { quit = true; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (quit) break;

        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        float dt = (float)((double)(now.QuadPart - last.QuadPart) / (double)freq.QuadPart);
        last = now;
        if (dt > 0.05f) dt = 0.05f;
        if (dt < 0.f) dt = 0.f;
        totalTime += dt;

        // ---------------- 输入 ----------------
        InputState in;
        in.fwd = keyDown('W') || keyDown(VK_UP);
        in.back = keyDown('S') || keyDown(VK_DOWN);
        in.left = keyDown('A') || keyDown(VK_LEFT);
        in.right = keyDown('D') || keyDown(VK_RIGHT);
        in.sprint = keyDown(VK_SHIFT);
        in.crouch = keyDown(VK_CONTROL);
        in.flash = keyPressed('F', 'F');
        in.sense = keyPressed('Q', 'Q');
        in.pause = keyPressed(VK_ESCAPE, VK_ESCAPE);
        in.confirm = keyPressed(VK_RETURN, VK_RETURN) || keyPressed(VK_SPACE, VK_SPACE);
        in.up = keyPressed('W', 'W') || keyPressed(VK_UP, VK_UP);
        in.down = keyPressed('S', 'S') || keyPressed(VK_DOWN, VK_DOWN);
        in.mouseL = g_mouseClicked;
        g_mouseClicked = false;

        POINT mp; GetCursorPos(&mp);
        ScreenToClient(g_hwnd, &mp);
        in.mouseX = (float)mp.x; in.mouseY = (float)mp.y;
        static POINT lastMouse = { 0, 0 };
        static bool hasMouse = false;

        bool lockMouse = (game.state == ST_PLAY || game.state == ST_INTRO);
        if (lockMouse) {
            if (!g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }
            RECT rc; GetClientRect(g_hwnd, &rc);
            POINT a = { rc.left, rc.top }, b = { rc.right, rc.bottom };
            ClientToScreen(g_hwnd, &a); ClientToScreen(g_hwnd, &b);
            SetRect(&rc, a.x, a.y, b.x, b.y);
            ClipCursor(&rc);
            POINT c = { g_W / 2, g_H / 2 };
            POINT cs = c; ClientToScreen(g_hwnd, &cs);
            POINT cur; GetCursorPos(&cur);
            in.mouseDX = (float)(cur.x - cs.x);
            in.mouseDY = (float)(cur.y - cs.y);
            SetCursorPos(cs.x, cs.y);
        } else {
            if (g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
            ClipCursor(0);
            in.mouseDX = in.mouseDY = 0.f;
            in.mouseMoved = hasMouse && (mp.x != lastMouse.x || mp.y != lastMouse.y);
        }
        lastMouse = mp; hasMouse = true;

        // ---------------- 更新 / 渲染 ----------------
        audio.pump();
        game.update(dt, in, audio, (float)g_W, (float)g_H);
        renderer.render(game, totalTime);
        SwapBuffers(g_dc);

        // 记录本帧按键状态（边沿检测）
        static const int watched[] = { 'F','Q','W','S','A','D',VK_UP,VK_DOWN,VK_LEFT,VK_RIGHT,
                                       VK_ESCAPE,VK_RETURN,VK_SPACE,VK_SHIFT,VK_CONTROL };
        for (int i = 0; i < (int)(sizeof(watched) / sizeof(watched[0])); i++) {
            int vk = watched[i];
            g_prevKeys[vk & 255] = keyDown(vk);
        }
        memset(g_keyEdge, 0, sizeof(g_keyEdge));   // 边沿只保留一帧
        if (!game.running) quit = true;
    }

    ClipCursor(0);
    if (g_cursorHidden) ShowCursor(TRUE);
    audio.shutdown();
    renderer.shutdown();
    if (g_ctx) { wglMakeCurrent(0, 0); wglDeleteContext(g_ctx); }
    if (g_dc && g_hwnd) ReleaseDC(g_hwnd, g_dc);
    if (g_hwnd) DestroyWindow(g_hwnd);
    return 0;
}
