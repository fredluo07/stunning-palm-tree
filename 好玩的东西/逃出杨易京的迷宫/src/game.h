#pragma once
// ============================================================================
//  game.h : 完整玩法层（迷宫 / 水晶 / 怪物 AI / 阶段 / 中文菜单状态机）
//  玩法对标《黑暗欺骗》：迷宫中收集全部水晶 -> 警报 -> 逃向传送门，途中被怪物追杀。
// ============================================================================
#include "math3d.h"
#include "audio.h"
#include <vector>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>

enum GameState {
    ST_MENU = 0, ST_HELP, ST_INTRO, ST_PLAY, ST_PAUSE, ST_GAMEOVER, ST_WIN
};

struct InputState {
    bool fwd, back, left, right, sprint, crouch;
    bool flash, sense, pause, confirm, up, down, mouseL, mouseMoved;
    float mouseDX, mouseDY, mouseX, mouseY;
    InputState() : fwd(false), back(false), left(false), right(false), sprint(false), crouch(false),
        flash(false), sense(false), pause(false), confirm(false), up(false), down(false), mouseL(false),
        mouseMoved(false), mouseDX(0), mouseDY(0), mouseX(0), mouseY(0) {}
};

struct Crystal {
    Vec3 pos;
    bool collected;
    float phase;
    Crystal() : collected(false), phase(0) {}
};
struct Monster {
    Vec3 pos, home, target, lastSeenPos;
    bool active;
    float speed, phaseOff, pathTimer, stunTimer, searchTimer, roarTimer;
    int mode; // 0 巡逻 1 追击 2 搜寻 3 眩晕
    Monster() : active(false), speed(3.4f), phaseOff(0), pathTimer(0), stunTimer(0),
        searchTimer(0), roarTimer(0), mode(0) {}
};
struct Particle {
    Vec3 pos, vel;
    float life, maxLife, size, r, g, b, a;
};

class Game {
public:
    static const int CRYSTAL_COUNT = 24;
    static const int MAX_MONSTERS = 3;
    const float CELL = 4.0f;
    const float WALL_H = 4.2f;
    const float EYE = 1.66f;

    std::vector<std::string> map;
    int W, H;
    std::vector<Crystal> crystals;
    std::vector<Monster> monsters;
    std::vector<Particle> particles;
    std::vector<int> flow;       // BFS 距离场：每格到玩家的最短步数
    std::vector<int> walkable;   // 可达格索引

    Vec3 pos, startPos, exitPos, camPos, camForward, camRight;
    float yaw, pitch;
    float stamina, invuln, hurt, shake, deathTimer, bob, stepDist;
    float elapsed, senseTimer, senseCD, flashFlicker, portalOpen, alarm;
    float hintTimer, introTimer, danger, dangerSmooth;
    float breathTimer;
    int lives, deaths, collected, menuIndex, lastMonsterCount;
    bool flashlight, escape, running;
    GameState state;
    std::wstring hintText;
    RNG rng;

    Game() : W(0), H(0), yaw(0), pitch(0), stamina(1.f), invuln(0), hurt(0), shake(0),
        deathTimer(0), bob(0), stepDist(0), elapsed(0), senseTimer(0), senseCD(0),
        flashFlicker(0), portalOpen(0), alarm(0), hintTimer(0), introTimer(0), danger(0),
        dangerSmooth(0), breathTimer(0), lives(3), deaths(0), collected(0), menuIndex(0),
        lastMonsterCount(1), flashlight(true), escape(false), running(true), state(ST_MENU) {}

    // -----------------------------------------------------------------------
    bool isWall(int x, int y) const {
        if (x < 0 || y < 0 || x >= W || y >= H) return true;
        return map[y][x] == '#';
    }
    bool isWallWorld(const Vec3& p) const {
        return isWall((int)(p.x / CELL), (int)(p.z / CELL));
    }
    Vec3 cellCenter(int x, int y) const {
        return Vec3((x + 0.5f) * CELL, 0.f, (y + 0.5f) * CELL);
    }
    int cellIndex(int x, int y) const { return y * W + x; }

    // -----------------------------------------------------------------------
    void initMap() {
        static const char* rows[] = {
            "#####################",
            "#S....#.......#.....#",
            "#.###.#.#####.#.###.#",
            "#.#...#.....#...#...#",
            "#.#.#####.#.#####.#.#",
            "#...#.....#.....#...#",
            "###.#.###.#####.#.###",
            "#...#.#...#...#.#...#",
            "#.###.#.###.#.#.###.#",
            "#.....#.....#.#.....#",
            "#.#########.#.#####.#",
            "#...........#.......#",
            "#.###.#####.#####.#.#",
            "#.#...#...#.....#.#.#",
            "#.#.###.#.#####.#.#.#",
            "#.#.....#.....#.#...#",
            "#.#########.#.#.###.#",
            "#.........#.#.....#.#",
            "#.#######.#.#####.#.#",
            "#M......E...#.....M.#",
            "#####################"
        };
        map.clear();
        for (int i = 0; i < 21; i++) map.push_back(std::string(rows[i]));
        H = (int)map.size(); W = (int)map[0].size();
    }

    void buildLevel() {
        initMap();
        rng = RNG(20261115ull);
        // 找起点
        int sx = 1, sy = 1;
        for (int y = 0; y < H; y++) for (int x = 0; x < W; x++)
            if (map[y][x] == 'S') { sx = x; sy = y; }
        startPos = cellCenter(sx, sy); startPos.y = EYE;

        // BFS：所有与起点连通的地板格
        std::vector<int> dist((size_t)W * H, -1);
        std::vector<int> q;
        q.push_back(cellIndex(sx, sy));
        dist[cellIndex(sx, sy)] = 0;
        for (size_t head = 0; head < q.size(); head++) {
            int cur = q[head], cx = cur % W, cy = cur / W;
            const int dx[4] = { 1,-1,0,0 }, dy[4] = { 0,0,1,-1 };
            for (int k = 0; k < 4; k++) {
                int nx = cx + dx[k], ny = cy + dy[k];
                if (isWall(nx, ny)) continue;
                int ni = cellIndex(nx, ny);
                if (dist[ni] >= 0) continue;
                dist[ni] = dist[cur] + 1;
                q.push_back(ni);
            }
        }
        walkable = q;
        // 出口：离起点最远的可达格
        int farIdx = walkable[0];
        for (size_t i = 0; i < walkable.size(); i++)
            if (dist[walkable[i]] > dist[farIdx]) farIdx = walkable[i];
        exitPos = cellCenter(farIdx % W, farIdx / W); exitPos.y = 0.f;

        // 水晶：按最小间距均匀撒点，避开起点 / 出口
        std::vector<int> cand;
        for (size_t i = 0; i < walkable.size(); i++) {
            int c = walkable[i], cx = c % W, cy = c / W;
            if (dist[c] < 3) continue;
            if (abs(cx - farIdx % W) + abs(cy - farIdx / W) < 2) continue;
            cand.push_back(c);
        }
        // 洗牌
        for (size_t i = cand.size(); i > 1; i--) {
            size_t j = (size_t)(rng.next() * (float)i);
            int t = cand[i - 1]; cand[i - 1] = cand[j]; cand[j] = t;
        }
        crystals.clear();
        for (size_t i = 0; i < cand.size() && (int)crystals.size() < CRYSTAL_COUNT; i++) {
            int c = cand[i], cx = c % W, cy = c / W;
            bool tooClose = false;
            for (size_t k = 0; k < crystals.size(); k++) {
                int px = (int)(crystals[k].pos.x / CELL), py = (int)(crystals[k].pos.z / CELL);
                if (abs(px - cx) + abs(py - cy) < 3) { tooClose = true; break; }
            }
            if (tooClose) continue;
            Crystal cr;
            cr.pos = cellCenter(cx, cy); cr.pos.y = 1.20f;
            cr.phase = rng.range(0.f, 6.28f);
            crystals.push_back(cr);
        }
        // 若地图太小导致水晶不足，放宽间距补齐
        for (size_t i = 0; i < cand.size() && (int)crystals.size() < CRYSTAL_COUNT; i++) {
            int c = cand[i];
            bool exists = false;
            for (size_t k = 0; k < crystals.size(); k++) {
                Vec3 d = crystals[k].pos - cellCenter(c % W, c / W);
                if (fabsf(d.x) < 0.1f && fabsf(d.z) < 0.1f) { exists = true; break; }
            }
            if (exists) continue;
            Crystal cr; cr.pos = cellCenter(c % W, c / W); cr.pos.y = 1.20f;
            cr.phase = rng.range(0.f, 6.28f);
            crystals.push_back(cr);
        }

        // 怪物出生点：取距离起点较远的三个不同格
        monsters.clear();
        std::vector<int> sorted = walkable;
        for (size_t i = 1; i < sorted.size(); i++) {
            int key = sorted[i]; size_t j = i;
            while (j > 0 && dist[sorted[j - 1]] < dist[key]) { sorted[j] = sorted[j - 1]; j--; }
            sorted[j] = key;
        }
        for (int i = 0; i < MAX_MONSTERS; i++) {
            Monster m;
            size_t pick = sorted.size() * (6 + i * 11) / 100;
            if (pick >= sorted.size()) pick = sorted.size() - 1;
            int c = sorted[pick];
            m.home = cellCenter(c % W, c / W); m.home.y = 1.45f;
            m.pos = m.home; m.target = m.home; m.lastSeenPos = m.home;
            m.active = (i == 0);
            m.speed = 3.3f + i * 0.25f;
            m.phaseOff = i * 2.1f;
            m.stunTimer = 2.5f;
            monsters.push_back(m);
        }
        flow.assign((size_t)W * H, -1);
    }

    void newGame() {
        buildLevel();
        pos = startPos;
        yaw = 0.f; pitch = 0.f;
        stamina = 1.f; invuln = 2.5f; hurt = 0.f; shake = 0.f; deathTimer = 0.f;
        bob = 0.f; stepDist = 0.f; elapsed = 0.f; senseTimer = 0.f; senseCD = 0.f;
        portalOpen = 0.f; alarm = 0.f; danger = 0.f; dangerSmooth = 0.f;
        lives = 3; deaths = 0; collected = 0; lastMonsterCount = 1;
        flashlight = true; escape = false; particles.clear();
        hintTimer = 0.f; introTimer = 3.6f;
        state = ST_INTRO;
        setHint(L"收集所有水晶，然后找到传送门离开。别让它们抓到你。", 3.6f);
    }

    void setHint(const std::wstring& s, float t) { hintText = s; hintTimer = t; }

    // Q 感应的当前目标：收集阶段 = 最近未收集的水晶；集齐后 = 紫色传送门。
    // outKind: 0=水晶, 1=传送门；outDist 为水平距离（米）。
    Vec3 senseTargetPos(float* outDist = 0, int* outKind = 0) const {
        Vec3 target(0.f, 0.f, 0.f);
        int kind = 0;
        if (escape || collected >= (int)crystals.size()) {
            target = exitPos; target.y = 1.6f; kind = 1;
        } else {
            float best = 1e9f;
            for (size_t i = 0; i < crystals.size(); i++) {
                if (crystals[i].collected) continue;
                float d = length(crystals[i].pos - pos);
                if (d < best) { best = d; target = crystals[i].pos; }
            }
            if (best > 1e8f) { target = exitPos; target.y = 1.6f; kind = 1; }
        }
        if (outDist) {
            Vec3 d = target - pos; d.y = 0.f;
            *outDist = length(d);
        }
        if (outKind) *outKind = kind;
        return target;
    }

    std::vector<std::wstring> menuItems() const {
        std::vector<std::wstring> v;
        switch (state) {
        case ST_MENU: v.push_back(L"开始游戏"); v.push_back(L"操作说明"); v.push_back(L"退出游戏"); break;
        case ST_PAUSE: v.push_back(L"继续游戏"); v.push_back(L"重新开始"); v.push_back(L"音量 +"); v.push_back(L"音量 -"); v.push_back(L"返回主菜单"); break;
        case ST_GAMEOVER: v.push_back(L"重新开始"); v.push_back(L"返回主菜单"); break;
        case ST_WIN: v.push_back(L"再玩一次"); v.push_back(L"返回主菜单"); break;
        default: break;
        }
        return v;
    }

    void activateMenu(AudioEngine& audio) {
        audio.play(SFX_UI);
        int n = (int)menuItems().size();
        if (menuIndex < 0) menuIndex = 0;
        if (n > 0 && menuIndex >= n) menuIndex = n - 1;
        switch (state) {
        case ST_MENU:
            if (menuIndex == 0) newGame();
            else if (menuIndex == 1) { state = ST_HELP; menuIndex = 0; }
            else running = false;
            break;
        case ST_PAUSE:
            if (menuIndex == 0) state = ST_PLAY;
            else if (menuIndex == 1) newGame();
            else if (menuIndex == 2) audio.setVolume(audio.getVolume() + 0.1f);
            else if (menuIndex == 3) audio.setVolume(audio.getVolume() - 0.1f);
            else { state = ST_MENU; menuIndex = 0; audio.setPhase(MUS_MENU); }
            break;
        case ST_GAMEOVER:
            if (menuIndex == 0) newGame();
            else { state = ST_MENU; menuIndex = 0; audio.setPhase(MUS_MENU); }
            break;
        case ST_WIN:
            if (menuIndex == 0) newGame();
            else { state = ST_MENU; menuIndex = 0; audio.setPhase(MUS_MENU); }
            break;
        default: break;
        }
    }

    const wchar_t* rank() const {
        if (elapsed < 200.f && deaths == 0) return L"S";
        if (elapsed < 330.f && deaths <= 1) return L"A";
        if (elapsed < 500.f) return L"B";
        return L"C";
    }

    // -----------------------------------------------------------------------
    // 碰撞：圆 vs 墙格 AABB
    void collide(Vec3& p, float r) const {
        int cx = (int)(p.x / CELL), cy = (int)(p.z / CELL);
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int wx = cx + dx, wy = cy + dy;
            if (!isWall(wx, wy)) continue;
            float ax0 = wx * CELL, ax1 = ax0 + CELL;
            float az0 = wy * CELL, az1 = az0 + CELL;
            float nx = clampf(p.x, ax0, ax1), nz = clampf(p.z, az0, az1);
            float ddx = p.x - nx, ddz = p.z - nz;
            float d2 = ddx * ddx + ddz * ddz;
            if (d2 >= r * r) continue;
            float d = sqrtf(d2);
            if (d > 1e-5f) {
                float push = r - d;
                p.x += ddx / d * push; p.z += ddz / d * push;
            } else {
                float l = p.x - ax0, rr = ax1 - p.x, u = p.z - az0, b = az1 - p.z;
                float mn = l; int side = 0;
                if (rr < mn) { mn = rr; side = 1; }
                if (u < mn) { mn = u; side = 2; }
                if (b < mn) { mn = b; side = 3; }
                if (side == 0) p.x = ax0 - r;
                else if (side == 1) p.x = ax1 + r;
                else if (side == 2) p.z = az0 - r;
                else p.z = az1 + r;
            }
        }
    }

    bool lineOfSight(const Vec3& a, const Vec3& b) const {
        float dist = length(b - a);
        int steps = (int)(dist / 0.45f) + 1;
        for (int i = 1; i < steps; i++) {
            Vec3 p = a + (b - a) * (i / (float)steps);
            if (isWallWorld(p)) return false;
        }
        return true;
    }

    void computeFlow() {
        flow.assign((size_t)W * H, -1);
        int px = (int)(pos.x / CELL), py = (int)(pos.z / CELL);
        if (isWall(px, py)) return;
        std::vector<int> q;
        int s = cellIndex(px, py);
        flow[s] = 0; q.push_back(s);
        const int dx[4] = { 1,-1,0,0 }, dy[4] = { 0,0,1,-1 };
        for (size_t head = 0; head < q.size(); head++) {
            int cur = q[head], cx = cur % W, cy = cur / W;
            for (int k = 0; k < 4; k++) {
                int nx = cx + dx[k], ny = cy + dy[k];
                if (isWall(nx, ny)) continue;
                int ni = cellIndex(nx, ny);
                if (flow[ni] >= 0) continue;
                flow[ni] = flow[cur] + 1;
                q.push_back(ni);
            }
        }
    }

    void burst(const Vec3& p, float r, float g, float b, int count, float speed) {
        for (int i = 0; i < count; i++) {
            Particle pt;
            pt.pos = p;
            pt.vel = Vec3(rng.range(-1.f, 1.f), rng.range(-0.2f, 1.f), rng.range(-1.f, 1.f)) * speed;
            pt.life = pt.maxLife = rng.range(0.5f, 1.35f);
            pt.size = rng.range(3.f, 9.f);
            pt.r = r; pt.g = g; pt.b = b; pt.a = 0.9f;
            particles.push_back(pt);
        }
    }

private:
    void updateParticles(float dt) {
        for (size_t i = 0; i < particles.size();) {
            Particle& p = particles[i];
            p.life -= dt;
            if (p.life <= 0.f) { particles[i] = particles.back(); particles.pop_back(); continue; }
            p.pos = p.pos + p.vel * dt;
            p.vel = p.vel * (1.f - 1.6f * dt);
            p.vel.y -= 1.1f * dt;
            p.a = (p.life / p.maxLife) * 0.9f;
            i++;
        }
        if (particles.size() > 900) particles.resize(900);
    }

    void onCaught(AudioEngine& audio) {
        lives--; deaths++;
        hurt = 1.f; shake = 1.6f; invuln = 3.0f;
        audio.play(SFX_HIT);
        burst(pos, 0.75f, 0.03f, 0.04f, 40, 4.5f);
        if (lives <= 0) {
            state = ST_GAMEOVER; menuIndex = 0;
            audio.setPhase(MUS_DEAD); audio.play(SFX_LOSE);
        } else {
            deathTimer = 1.55f;
            setHint(L"你被抓住了…… 灵魂被送回起点", 2.6f);
        }
    }

    void respawn() {
        pos = startPos;
        stamina = 1.f; invuln = 2.6f;
        for (size_t i = 0; i < monsters.size(); i++) {
            monsters[i].pos = monsters[i].home;
            monsters[i].target = monsters[i].home;
            monsters[i].mode = 3;
            monsters[i].stunTimer = 2.6f;
        }
        computeFlow();
    }

public:
    void update(float dt, const InputState& in, AudioEngine& audio, float vw, float vh) {
        (void)vw; (void)vh;
        if (dt > 0.05f) dt = 0.05f;
        // 全局衰减量
        hurt = fmaxf(0.f, hurt - dt * 1.15f);
        shake = fmaxf(0.f, shake - dt * 1.5f);
        flashFlicker = fmaxf(0.f, flashFlicker - dt * 2.f);
        hintTimer = fmaxf(0.f, hintTimer - dt);
        senseTimer = fmaxf(0.f, senseTimer - dt);
        senseCD = fmaxf(0.f, senseCD - dt);
        invuln = fmaxf(0.f, invuln - dt);
        updateParticles(dt);

        // ---------------- 菜单 / 暂停 / 结算 ----------------
        if (state != ST_PLAY && state != ST_INTRO) {
            if (state == ST_PAUSE && in.pause) { state = ST_PLAY; audio.play(SFX_UI); return; }
            int n = (int)menuItems().size();
            if (state == ST_HELP) {
                if (in.confirm || in.pause || in.mouseL) { state = ST_MENU; menuIndex = 0; audio.play(SFX_UI); }
            } else if (n > 0) {
                if (in.up) { menuIndex = (menuIndex + n - 1) % n; audio.play(SFX_UI, 0.6f); }
                if (in.down) { menuIndex = (menuIndex + 1) % n; audio.play(SFX_UI, 0.6f); }
                // 命中判定与 main.cpp::drawMenuPanel 的布局保持一致
                float y0 = (state == ST_MENU) ? vh * 0.54f : vh * 0.44f;
                auto hit = [&]() -> int {
                    if (in.mouseX < vw * 0.5f - 190.f || in.mouseX > vw * 0.5f + 190.f) return -1;
                    for (int i = 0; i < n; i++) {
                        float yy = y0 + i * 62.f;
                        if (in.mouseY > yy - 22.f && in.mouseY < yy + 30.f) return i;
                    }
                    return -1;
                };
                // 鼠标移动时悬停即高亮；只有真的移动过才覆盖键盘选择
                if (in.mouseMoved) {
                    int h = hit();
                    if (h >= 0 && h != menuIndex) { menuIndex = h; audio.play(SFX_UI, 0.35f); }
                }
                bool clicked = false;
                if (in.mouseL) {
                    int h = hit();
                    if (h >= 0) { menuIndex = h; activateMenu(audio); clicked = true; }
                }
                if (!clicked && in.confirm) activateMenu(audio);
            }
            audio.setIntensity(0.f);
            if (state == ST_MENU) audio.setPhase(MUS_MENU);
            return;
        }
        // ---------------- 开场 ----------------
        if (state == ST_INTRO) {
            introTimer -= dt;
            audio.setPhase(MUS_EXPLORE);
            audio.setIntensity(0.05f);
            portalOpen = 0.f;
            yaw += in.mouseDX * 0.0022f;   // 鼠标右移 => 视线右转
            pitch -= in.mouseDY * 0.0022f;
            pitch = clampf(pitch, -1.42f, 1.42f);
            updateCamera(dt, in);
            if (introTimer <= 0.f) state = ST_PLAY;
            return;
        }

        // ---------------- 主玩法 ----------------
        elapsed += dt;
        if (in.pause) { state = ST_PAUSE; menuIndex = 0; audio.play(SFX_UI); return; }
        if (in.flash) { flashlight = !flashlight; audio.play(SFX_UI, 0.5f); }
        if (in.sense && senseCD <= 0.f) {
            senseTimer = 2.8f; senseCD = 10.f;
            audio.play(SFX_SENSE);
            if (escape) setHint(L"目标感应：跟随紫色光柱，找到传送门", 2.6f);
            else setHint(L"水晶感应：跟随紫色的光", 2.2f);
        }

        // 视角
        const float SENS = 0.0022f;
        yaw += in.mouseDX * SENS;   // 与 camForward=(sin yaw, ..., -cos yaw) 的右手系一致
        pitch -= in.mouseDY * SENS;
        pitch = clampf(pitch, -1.42f, 1.42f);
        updateCamera(dt, in);

        // 移动
        if (deathTimer > 0.f) {
            deathTimer -= dt;
            if (deathTimer <= 0.f) respawn();
        } else {
            Vec3 f = normalize(Vec3(camForward.x, 0, camForward.z));
            Vec3 r = normalize(cross(f, Vec3(0, 1, 0)));
            Vec3 wish(0, 0, 0);
            if (in.fwd) wish = wish + f;
            if (in.back) wish = wish - f;
            if (in.right) wish = wish + r;
            if (in.left) wish = wish - r;
            float wl = length(wish);
            if (wl > 1e-4f) wish = wish / wl;

            bool wantSprint = in.sprint && stamina > 0.02f && wl > 1e-4f && !in.crouch;
            float speed = in.crouch ? 2.25f : (wantSprint ? 8.1f : 4.5f);
            if (wantSprint) {
                stamina = fmaxf(0.f, stamina - dt * 0.29f);
                breathTimer -= dt;
                if (breathTimer <= 0.f) { breathTimer = 0.72f; audio.play(SFX_BREATH, 0.55f); }
            } else {
                stamina = fminf(1.f, stamina + dt * 0.17f);
            }
            Vec3 np = pos + wish * (speed * dt);
            collide(np, 0.42f);
            float moved = length(np - pos);
            pos = np;
            if (moved > 1e-4f) {
                bob += moved * (wantSprint ? 3.4f : 2.4f);
                stepDist += moved;
                if (stepDist > (in.crouch ? 2.6f : 1.85f)) {
                    stepDist = 0.f;
                    audio.play(SFX_STEP, in.crouch ? 0.35f : (wantSprint ? 0.9f : 0.6f));
                }
            }
        }

        // 水晶拾取
        for (size_t i = 0; i < crystals.size(); i++) {
            Crystal& c = crystals[i];
            c.phase += dt;
            if (c.collected) continue;
            Vec3 d = c.pos - pos;
            if (d.x * d.x + d.y * d.y + d.z * d.z < 1.35f * 1.35f) {
                c.collected = true; collected++;
                audio.play(SFX_CRYSTAL);
                burst(c.pos, 0.55f, 0.25f, 1.0f, 26, 3.2f);
                if (collected == (int)crystals.size()) {
                    escape = true;
                    audio.setPhase(MUS_ESCAPE);
                    audio.play(SFX_ALARM);
                    audio.play(SFX_PORTAL);
                    setHint(L"全部水晶已收集！红色警报——快逃向紫色传送门！", 5.f);
                } else if (collected == (int)crystals.size() / 2) {
                    setHint(L"它们醒了…… 脚步声越来越近", 3.f);
                }
            }
        }
        float ratio = crystals.empty() ? 0.f : collected / (float)crystals.size();
        // 必须钳制下界：否则收集阶段会累积成很大的负数，导致传送门永远无法开启
        alarm = clampf(alarm + (escape ? dt * 0.65f : -dt * 0.8f), 0.f, 1.f);
        portalOpen = clampf(portalOpen + (escape ? dt * 0.7f : -dt * 2.f), 0.f, 1.f);

        // 难度：随收集进度增加怪物数量与速度
        int want = ratio < 0.34f ? 1 : (ratio < 0.67f ? 2 : 3);
        if (escape) want = 3;
        for (int i = 0; i < (int)monsters.size(); i++) {
            Monster& m = monsters[i];
            if (i < want && !m.active) {
                m.active = true;
                m.stunTimer = 1.4f;
                m.mode = 3;
                audio.play(SFX_SPAWN);
                if (i > lastMonsterCount - 1)
                    setHint(i == 1 ? L"第二只猎手加入了追逐" : L"最后一只…… 它就在附近", 3.2f);
            }
            m.speed = 3.25f + ratio * 1.15f + (escape ? 0.55f : 0.f) + i * 0.12f;
        }
        lastMonsterCount = want;

        // 寻路距离场（全场一次 BFS，供所有怪物使用）
        flowTimer -= dt;
        if (flowTimer <= 0.f) { flowTimer = 0.19f; computeFlow(); }

        // 怪物 AI
        float nearest = 999.f;
        bool anyChase = false;
        for (size_t i = 0; i < monsters.size(); i++) {
            Monster& m = monsters[i];
            if (!m.active) continue;
            m.roarTimer -= dt;
            if (m.stunTimer > 0.f) {
                m.stunTimer -= dt;
                if (m.stunTimer <= 0.f) m.mode = 0;
                continue;
            }
            Vec3 toP = pos - m.pos;
            float dist = length(toP);
            if (dist < nearest) nearest = dist;
            float senseR = (in.sprint ? 19.f : 13.5f) * (in.crouch ? 0.62f : 1.f);
            bool see = dist < senseR && lineOfSight(m.pos, pos);
            if (dist < 4.0f && lineOfSight(m.pos, pos)) see = true;
            if (see) {
                m.mode = 1; m.searchTimer = 3.4f; m.lastSeenPos = pos;
            } else if (m.mode == 1) {
                m.searchTimer -= dt;
                if (m.searchTimer <= 0.f) m.mode = 2;
            }
            if (m.mode == 1) anyChase = true;

            Vec3 goal = m.lastSeenPos;
            m.pathTimer -= dt;
            if (m.pathTimer <= 0.f) {
                m.pathTimer = 0.22f;
                if (m.mode == 1 || m.mode == 2) {
                    int cx = (int)(m.pos.x / CELL), cy = (int)(m.pos.z / CELL);
                    int here = cellIndex(cx, cy);
                    int best = here, bestD = (here >= 0 && here < (int)flow.size()) ? flow[here] : -1;
                    if (bestD < 0) bestD = 999999;
                    const int dx[4] = { 1,-1,0,0 }, dy[4] = { 0,0,1,-1 };
                    for (int k = 0; k < 4; k++) {
                        int nx = cx + dx[k], ny = cy + dy[k];
                        if (isWall(nx, ny)) continue;
                        int ni = cellIndex(nx, ny);
                        if (ni >= 0 && ni < (int)flow.size() && flow[ni] >= 0 && flow[ni] < bestD) {
                            bestD = flow[ni]; best = ni;
                        }
                    }
                    goal = cellCenter(best % W, best / W); goal.y = 1.45f;
                } else {
                    // 巡逻：附近随机可达格
                    int cx = (int)(m.pos.x / CELL), cy = (int)(m.pos.z / CELL);
                    for (int tries = 0; tries < 8; tries++) {
                        int nx = cx + (int)(rng.range(-4.f, 4.f));
                        int ny = cy + (int)(rng.range(-4.f, 4.f));
                        if (!isWall(nx, ny)) { goal = cellCenter(nx, ny); goal.y = 1.45f; break; }
                    }
                }
                m.target = goal;
            }
            if (m.mode == 2 && length(m.target - m.pos) < 1.2f) { m.mode = 0; m.pathTimer = 0.f; }

            Vec3 dir = m.target - m.pos; dir.y = 0;
            float dl = length(dir);
            if (dl > 0.15f) {
                dir = dir / dl;
                float sp = m.speed * (m.mode == 0 ? 0.55f : 1.f);
                Vec3 np = m.pos + dir * (sp * dt);
                np.y = 1.45f;
                collide(np, 0.55f);
                m.pos = np;
            }
            m.pos.y = 1.45f + sinf((float)elapsed * 3.2f + m.phaseOff) * 0.09f;

            if (m.roarTimer <= 0.f && dist < 22.f && m.mode == 1) {
                m.roarTimer = rng.range(3.5f, 7.f);
                audio.play(SFX_ROAR, clampf(1.2f - dist / 24.f, 0.25f, 1.f));
            }
            // 抓取判定
            if (invuln <= 0.f && deathTimer <= 0.f && dist < 1.30f) {
                onCaught(audio);
                break;
            }
        }

        // 传送门逃脱
        if (escape && portalOpen > 0.75f && deathTimer <= 0.f) {
            Vec3 d = exitPos - pos; d.y = 0;
            if (length(d) < 1.65f) {
                state = ST_WIN; menuIndex = 0;
                audio.setPhase(MUS_WIN); audio.play(SFX_WIN);
                burst(Vec3(exitPos.x, 2.f, exitPos.z), 0.7f, 0.3f, 1.f, 60, 6.f);
            }
        }

        // 危险度 -> 音乐 / 后处理
        float rawDanger = clampf(1.f - nearest / 22.f, 0.f, 1.f);
        if (anyChase) rawDanger = fmaxf(rawDanger, 0.55f);
        if (escape) rawDanger = fmaxf(rawDanger, 0.7f);
        dangerSmooth = dangerSmooth + (rawDanger - dangerSmooth) * fminf(1.f, dt * 3.f);
        danger = dangerSmooth;
        audio.setIntensity(danger);
        if (!escape) audio.setPhase(anyChase ? MUS_CHASE : MUS_EXPLORE);

        // 偶发环境粒子
        if (rng.next() < dt * 7.f) {
            Particle p;
            p.pos = pos + Vec3(rng.range(-9.f, 9.f), rng.range(-0.8f, 2.6f), rng.range(-9.f, 9.f));
            p.vel = Vec3(rng.range(-0.15f, 0.15f), rng.range(0.02f, 0.18f), rng.range(-0.15f, 0.15f));
            p.life = p.maxLife = rng.range(2.5f, 5.f);
            p.size = rng.range(1.5f, 3.5f);
            p.r = 0.55f; p.g = 0.55f; p.b = 0.62f; p.a = 0.30f;
            particles.push_back(p);
        }
        if (escape && rng.next() < dt * 14.f)
            burst(Vec3(exitPos.x, rng.range(0.4f, 3.4f), exitPos.z), 0.75f, 0.2f, 1.f, 2, 1.6f);
    }

    void updateCamera(float dt, const InputState& in) {
        (void)dt; (void)in;
        camForward = Vec3(sinf(yaw) * cosf(pitch), sinf(pitch), -cosf(yaw) * cosf(pitch));
        camRight = normalize(cross(camForward, Vec3(0, 1, 0)));
        float bobY = sinf(bob) * 0.045f;
        float bobX = cosf(bob * 0.5f) * 0.025f;
        float sh = shake * 0.09f;
        camPos = pos + Vec3(camRight.x * (bobX + sh * 0.4f),
                            bobY + sh * 0.3f,
                            camRight.z * (bobX + sh * 0.4f));
    }

    float flowTimer = 0.f;
};
