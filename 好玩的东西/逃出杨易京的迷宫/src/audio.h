#pragma once
// ============================================================================
//  audio.h : 零资源实时合成音频引擎
//  用 waveOut 双/四缓冲流式输出，实时合成氛围 drone、动态心跳、小调配乐与全部音效。
//  不需要任何 mp3 / wav 文件，也不需要 irrKlang / OpenAL / FMOD。
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <cmath>
#include <cstdint>

enum SfxId {
    SFX_CRYSTAL = 0, SFX_STEP, SFX_ROAR, SFX_HIT, SFX_ALARM,
    SFX_PORTAL, SFX_WIN, SFX_LOSE, SFX_UI, SFX_SENSE, SFX_BREATH, SFX_SPAWN
};
enum MusicPhase { MUS_MENU = 0, MUS_EXPLORE, MUS_CHASE, MUS_ESCAPE, MUS_DEAD, MUS_WIN };

class AudioEngine {
public:
    static const int BLOCK = 1024;
    static const int NBUF = 4;
    static const int SR = 44100;

    AudioEngine() : hwo(0), ready(false), master(0.85f), intensity(0.f), phase(MUS_MENU),
                    g_t(0.0), lp(0.f), rs(22222u), beatTimer(0.f), seqTimer(0.f),
                    seqStep(0), noteAge(0.f), noteF(146.83f), evCount(0), volume(0.8f) {
        for (int i = 0; i < NBUF; i++) { ZeroMemory(&hdr[i], sizeof(WAVEHDR)); }
    }

    bool init() {
        WAVEFORMATEX fmt;
        ZeroMemory(&fmt, sizeof(fmt));
        fmt.wFormatTag = WAVE_FORMAT_PCM;
        fmt.nChannels = 2;
        fmt.nSamplesPerSec = SR;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = 4;
        fmt.nAvgBytesPerSec = SR * 4;
        MMRESULT r = waveOutOpen(&hwo, WAVE_MAPPER, &fmt, 0, 0, CALLBACK_NULL);
        if (r != MMSYSERR_NOERROR || !hwo) { hwo = 0; return false; }
        for (int i = 0; i < NBUF; i++) {
            ZeroMemory(&hdr[i], sizeof(WAVEHDR));
            hdr[i].lpData = (LPSTR)buf[i];
            hdr[i].dwBufferLength = BLOCK * 2 * sizeof(short);
            fill(i);
            waveOutPrepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
            hdr[i].dwFlags |= 0;
            waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR));
        }
        ready = true;
        return true;
    }

    void shutdown() {
        if (!hwo) return;
        ready = false;
        waveOutReset(hwo);
        for (int i = 0; i < NBUF; i++) {
            if (hdr[i].dwFlags & WHDR_PREPARED)
                waveOutUnprepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
        }
        waveOutClose(hwo);
        hwo = 0;
    }

    // 每帧调用一次：把播放完的缓冲重新填满并投递
    void pump() {
        if (!ready || !hwo) return;
        for (int i = 0; i < NBUF; i++) {
            if ((hdr[i].dwFlags & WHDR_DONE) && (hdr[i].dwFlags & WHDR_PREPARED)) {
                waveOutUnprepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
                fill(i);
                waveOutPrepareHeader(hwo, &hdr[i], sizeof(WAVEHDR));
                waveOutWrite(hwo, &hdr[i], sizeof(WAVEHDR));
            }
        }
    }

    void setIntensity(float v) { intensity = v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    void setPhase(MusicPhase p) { phase = p; }
    void setVolume(float v) { volume = v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }
    float getVolume() const { return volume; }
    bool isReady() const { return ready; }

    void play(SfxId id, float amp = 1.f) {
        if (!ready) return;
        if (evCount < 24) {
            ev[evCount].id = id; ev[evCount].age = 0.f;
            ev[evCount].dur = sfxDur(id); ev[evCount].amp = amp;
            evCount++;
        } else {
            // 覆盖最老的一个
            ev[0].id = id; ev[0].age = 0.f; ev[0].dur = sfxDur(id); ev[0].amp = amp;
        }
    }

private:
    struct Event { SfxId id; float age, dur, amp; };
    HWAVEOUT hwo;
    WAVEHDR hdr[NBUF];
    short buf[NBUF][BLOCK * 2];
    bool ready;
    float master, intensity, volume;
    MusicPhase phase;
    double g_t;
    float lp;
    uint32_t rs;
    float beatTimer, seqTimer, noteAge, noteF;
    int seqStep;
    Event ev[24];
    int evCount;

    static float sfxDur(SfxId id) {
        switch (id) {
        case SFX_STEP: return 0.14f;
        case SFX_UI: return 0.09f;
        case SFX_BREATH: return 0.45f;
        case SFX_CRYSTAL: return 0.85f;
        case SFX_SENSE: return 0.9f;
        case SFX_SPAWN: return 1.1f;
        case SFX_ROAR: return 1.15f;
        case SFX_HIT: return 1.5f;
        case SFX_ALARM: return 2.2f;
        case SFX_PORTAL: return 1.6f;
        case SFX_WIN: return 2.8f;
        case SFX_LOSE: return 2.4f;
        }
        return 0.5f;
    }
    float white() {
        rs = rs * 1664525u + 1013904223u;
        return ((int32_t)(rs >> 8) / 8388608.f) - 1.f;
    }
    static float env(float age, float dur, float attack) {
        if (age < 0.f || age > dur) return 0.f;
        float a = age < attack ? age / attack : 1.f;
        float d = (age - attack) / (dur - attack > 1e-4f ? dur - attack : 1e-4f);
        return a * (1.f - d) * (1.f - d);
    }

    void synthEvent(const Event& e, float& L, float& R, double t) {
        float a = e.age, d = e.dur, amp = e.amp;
        float E = env(a, d, d * 0.06f);
        float w = white();
        switch (e.id) {
        case SFX_CRYSTAL: {
            float f = 880.f + 520.f * (a / d);
            float s = sinf(6.2831853f * f * (float)t) * 0.5f
                    + sinf(6.2831853f * f * 1.5f * (float)t) * 0.28f
                    + sinf(6.2831853f * f * 2.01f * (float)t) * 0.16f;
            L += s * E * amp * 0.32f; R += s * E * amp * 0.32f;
            break;
        }
        case SFX_STEP: {
            float s = sinf(6.2831853f * (82.f - 45.f * (a / d)) * (float)t) * 0.6f + w * 0.35f;
            L += s * E * amp * 0.24f; R += s * E * amp * 0.24f;
            break;
        }
        case SFX_ROAR: {
            float f = 68.f - 26.f * (a / d);
            float saw = 2.f * fmodf(f * (float)t, 1.f) - 1.f;
            float s = saw * 0.38f + sinf(6.2831853f * f * 0.5f * (float)t) * 0.4f + w * 0.35f;
            float vib = 0.7f + 0.3f * sinf(6.2831853f * 17.f * (float)t);
            L += s * E * amp * vib * 0.44f; R += s * E * amp * vib * 0.44f;
            break;
        }
        case SFX_HIT: {
            float f = 260.f * expf(-4.f * (a / d)) + 40.f;
            float s = sinf(6.2831853f * f * (float)t) * 0.5f + w * 0.75f;
            L += s * E * amp * 0.62f; R += s * E * amp * 0.62f;
            break;
        }
        case SFX_ALARM: {
            float pulse = fmodf(a, 0.42f) < 0.21f ? 1.f : 0.f;
            float s = sinf(6.2831853f * 440.f * (float)t) * 0.35f
                    + sinf(6.2831853f * 466.f * (float)t) * 0.35f;
            L += s * pulse * E * amp * 0.28f; R += s * pulse * E * amp * 0.28f;
            break;
        }
        case SFX_PORTAL: {
            float f = 120.f + 900.f * (a / d);
            float s = sinf(6.2831853f * f * (float)t) * 0.45f
                    + sinf(6.2831853f * f * 1.5f * (float)t) * 0.2f + w * 0.1f;
            L += s * E * amp * 0.33f; R += s * E * amp * 0.33f;
            break;
        }
        case SFX_WIN: {
            float n = floorf(a / 0.22f);
            float f = 146.83f * powf(1.122462f, n);
            float ne = expf(-3.f * fmodf(a, 0.22f));
            float s = (sinf(6.2831853f * f * (float)t) + 0.4f * sinf(6.2831853f * f * 2.f * (float)t)) * ne;
            L += s * E * amp * 0.3f; R += s * E * amp * 0.3f;
            break;
        }
        case SFX_LOSE: {
            float f = 220.f * expf(-1.6f * (a / d)) + 35.f;
            float s = sinf(6.2831853f * f * (float)t) * 0.6f + w * 0.2f;
            L += s * E * amp * 0.4f; R += s * E * amp * 0.4f;
            break;
        }
        case SFX_UI: {
            float s = sinf(6.2831853f * 1050.f * (float)t);
            L += s * E * amp * 0.16f; R += s * E * amp * 0.16f;
            break;
        }
        case SFX_SENSE: {
            float s = sinf(6.2831853f * (620.f + 300.f * (a / d)) * (float)t) * 0.7f;
            float ping = expf(-6.f * a);
            L += s * ping * amp * 0.24f; R += s * ping * amp * 0.24f;
            break;
        }
        case SFX_BREATH: {
            float w2 = white();
            float s = w2 * (0.5f + 0.5f * sinf(6.2831853f * 1.3f * (float)t));
            L += s * E * amp * 0.2f; R += s * E * amp * 0.2f;
            break;
        }
        case SFX_SPAWN: {
            float s = sinf(6.2831853f * (55.f + 180.f * (a / d)) * (float)t) * 0.5f + w * 0.3f;
            L += s * E * amp * 0.36f; R += s * E * amp * 0.36f;
            break;
        }
        }
    }

    void fill(int index) {
        short* out = buf[index];
        const float dt = 1.f / (float)SR;
        // 情绪参数
        float musGain = 0.022f;
        float droneGain = 0.13f;
        float beatInterval = 1.25f - 0.55f * intensity;
        float seqDur = 0.62f;
        switch (phase) {
        case MUS_MENU:    musGain = 0.030f; droneGain = 0.15f; break;
        case MUS_EXPLORE: musGain = 0.026f; droneGain = 0.12f; break;
        case MUS_CHASE:   musGain = 0.055f; droneGain = 0.17f; seqDur = 0.30f; break;
        case MUS_ESCAPE:  musGain = 0.075f; droneGain = 0.20f; seqDur = 0.24f; break;
        case MUS_DEAD:    musGain = 0.010f; droneGain = 0.09f; break;
        case MUS_WIN:     musGain = 0.045f; droneGain = 0.10f; break;
        }
        static const float scale[8] = { 146.83f, 174.61f, 220.00f, 261.63f,
                                       233.08f, 174.61f, 155.56f, 138.59f };

        for (int n = 0; n < BLOCK; n++) {
            float t = (float)g_t;
            float w = white();
            lp += (w - lp) * 0.012f;

            // --- 低频嗡鸣（三个微失谐振子，形成拍频）---
            float drone = sinf(6.2831853f * 55.0f * t) * 0.55f
                        + sinf(6.2831853f * 55.31f * t) * 0.40f
                        + sinf(6.2831853f * 82.41f * t) * 0.24f;
            drone *= 0.65f + 0.35f * sinf(0.21f * t);
            // --- 金属摩擦般的高频层 ---
            float metal = sinf(6.2831853f * 1174.6f * t + 2.f * sinf(0.7f * t))
                        * (0.5f + 0.5f * sinf(0.13f * t)) * 0.012f;
            // --- 风 ---
            float wind = lp * (0.55f + 0.45f * sinf(0.37f * t)) * 0.30f;

            float s = drone * droneGain + wind + metal;

            // --- 心跳 ---
            beatTimer += dt;
            if (beatTimer >= beatInterval) beatTimer -= beatInterval;
            float bp = beatTimer;
            float p1 = bp < 0.26f ? sinf(3.14159265f * bp / 0.26f) * expf(-bp * 9.f) : 0.f;
            float dp = bp - 0.25f;
            float p2 = (dp >= 0.f && dp < 0.26f) ? sinf(3.14159265f * dp / 0.26f) * expf(-dp * 9.f) : 0.f;
            float heart = (p1 + p2 * 0.65f) * sinf(6.2831853f * 46.f * t)
                        * (0.10f + 0.30f * intensity);
            s += heart;

            // --- 配乐音序 ---
            seqTimer += dt;
            noteAge += dt;
            if (seqTimer >= seqDur) {
                seqTimer -= seqDur;
                seqStep = (seqStep + 1) & 7;
                noteF = scale[seqStep];
                noteAge = 0.f;
            }
            float ne = expf(-noteAge * (seqDur < 0.4f ? 5.5f : 2.6f));
            float tone = sinf(6.2831853f * noteF * t) * 0.62f
                       + sinf(6.2831853f * noteF * 2.f * t) * 0.22f
                       + sinf(6.2831853f * noteF * 3.f * t) * 0.09f;
            s += tone * ne * musGain;
            // 逃脱/追逐时的三全音与鼓点
            if (phase == MUS_CHASE || phase == MUS_ESCAPE) {
                s += sinf(6.2831853f * (noteF * 1.41421f) * t) * ne * musGain * 0.42f;
                float kick = sinf(6.2831853f * (52.f + 40.f * expf(-seqTimer * 20.f)) * t)
                           * expf(-seqTimer * 18.f);
                s += kick * 0.10f;
            }
            // 逃脱阶段红色警报底噪
            if (phase == MUS_ESCAPE) {
                s += sinf(6.2831853f * 220.f * t) * 0.016f * (0.5f + 0.5f * sinf(6.f * t));
            }

            // --- 事件音效 ---
            float eL = 0.f, eR = 0.f;
            for (int i = 0; i < evCount; i++) {
                Event e = ev[i];
                e.age += n * dt;   // 包络按采样推进，避免 23ms 阶梯感
                synthEvent(e, eL, eR, g_t);
            }
            s += eL;
            float sR = s + (eR - eL) * 0.12f + metal * 0.35f;

            float vol = master * volume;
            float l = s * vol, r = sR * vol;
            if (l > 1.f) l = 1.f; if (l < -1.f) l = -1.f;
            if (r > 1.f) r = 1.f; if (r < -1.f) r = -1.f;
            out[n * 2] = (short)(l * 32767.f);
            out[n * 2 + 1] = (short)(r * 32767.f);

            g_t += dt;
        }

        // 推进事件时钟（按整个缓冲长度）
        float adv = (float)BLOCK / (float)SR;
        int w = 0;
        for (int i = 0; i < evCount; i++) {
            ev[i].age += adv;
            if (ev[i].age < ev[i].dur) ev[w++] = ev[i];
        }
        evCount = w;
    }
};
