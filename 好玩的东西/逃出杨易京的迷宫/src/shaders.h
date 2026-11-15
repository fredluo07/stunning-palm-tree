#pragma once
// ============================================================================
//  shaders.h : GLSL 330 源码 + 着色器程序封装（编译错误会写入 darkmaze.log）
// ============================================================================
#include "glcore.h"
#include "math3d.h"
#include <string>
#include <map>
#include <cstdio>
#include <cstdarg>

using namespace glapi;

inline void GLLog(const char* fmt, ...) {
    FILE* f = fopen("darkmaze.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

class Shader {
public:
    GLuint p;
    std::map<std::string, GLint> locs;
    Shader() : p(0) {}

    bool create(const char* vsrc, const char* fsrc, const char* name) {
        GLuint vs = compile(GL_VERTEX_SHADER, vsrc, name, "VS");
        if (!vs) return false;
        GLuint fs = compile(GL_FRAGMENT_SHADER, fsrc, name, "FS");
        if (!fs) { glDeleteShader(vs); return false; }
        p = glCreateProgram();
        glAttachShader(p, vs); glAttachShader(p, fs);
        glLinkProgram(p);
        GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
        if (!ok) {
            char buf[8192]; GLint n = 0;
            glGetProgramInfoLog(p, 8191, &n, buf); buf[n > 0 ? n : 0] = 0;
            GLLog("[Shader] %s LINK FAILED:\n%s", name, buf);
        }
        glDeleteShader(vs); glDeleteShader(fs);
        GLLog("[Shader] %s %s", name, ok ? "OK" : "FAILED");
        return ok != 0;
    }
    void destroy() { if (p) glDeleteProgram(p); p = 0; locs.clear(); }
    void bind() const { glUseProgram(p); }
    GLint loc(const char* n) {
        std::map<std::string, GLint>::iterator it = locs.find(n);
        if (it != locs.end()) return it->second;
        GLint l = glGetUniformLocation(p, n);
        locs[n] = l;
        return l;
    }
    void i(const char* n, int v) { GLint l = loc(n); if (l >= 0) glUniform1i(l, v); }
    void f(const char* n, float v) { GLint l = loc(n); if (l >= 0) glUniform1f(l, v); }
    void v2(const char* n, float a, float b) { GLint l = loc(n); if (l >= 0) glUniform2f(l, a, b); }
    void v3(const char* n, const Vec3& v) { GLint l = loc(n); if (l >= 0) glUniform3f(l, v.x, v.y, v.z); }
    void v4(const char* n, float a, float b, float c, float d) { GLint l = loc(n); if (l >= 0) glUniform4f(l, a, b, c, d); }
    void m4(const char* n, const Mat4& m) { GLint l = loc(n); if (l >= 0) glUniformMatrix4fv(l, 1, GL_FALSE, m.m); }
    void m3(const char* n, const Mat4& m) {
        GLint l = loc(n);
        if (l >= 0) {
            float a[9] = { m.m[0], m.m[1], m.m[2], m.m[4], m.m[5], m.m[6], m.m[8], m.m[9], m.m[10] };
            glUniformMatrix3fv(l, 1, GL_FALSE, a);
        }
    }
    void fv(const char* n, const float* a, int count, int stride) {
        GLint l = loc(n); if (l < 0) return;
        if (stride == 1) glUniform1fv(l, count, a);
        else if (stride == 2) glUniform2fv(l, count, a);
        else if (stride == 3) glUniform3fv(l, count, a);
        else if (stride == 4) glUniform4fv(l, count, a);
    }

private:
    static GLuint compile(GLenum type, const char* src, const char* name, const char* stage) {
        GLuint s = glCreateShader(type);
        const GLchar* p = src;
        glShaderSource(s, 1, &p, 0);
        glCompileShader(s);
        GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char buf[8192]; GLint n = 0;
            glGetShaderInfoLog(s, 8191, &n, buf); buf[n > 0 ? n : 0] = 0;
            GLLog("[Shader] %s %s COMPILE FAILED:\n%s", name, stage, buf);
            glDeleteShader(s);
            return 0;
        }
        return s;
    }
};

// ---------------------------------------------------------------------------
// 通用场景着色器：PBR 风格 Blinn-Phong + 法线贴图 + 聚光手电 + 点光 + 阴影 + 雾
// ---------------------------------------------------------------------------
static const char* SCENE_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in float aAO;
uniform mat4 uProj, uView, uModel, uLightVP;
uniform mat3 uNormalMat;
uniform vec2 uTile;
out vec3 vWorld;
out vec3 vNormal;
out vec2 vUV;
out float vAO;
out vec4 vLightClip;
void main(){
  vec4 wp = uModel * vec4(aPos,1.0);
  vWorld = wp.xyz;
  vNormal = normalize(uNormalMat * aNormal);
  vUV = aUV * uTile;
  vAO = aAO;
  vLightClip = uLightVP * wp;
  gl_Position = uProj * uView * wp;
}
)GLSL";

static const char* SCENE_FS = R"GLSL(
#version 330 core
in vec3 vWorld;
in vec3 vNormal;
in vec2 vUV;
in float vAO;
in vec4 vLightClip;
out vec4 fragColor;
uniform sampler2D uAlbedo;
uniform sampler2D uNormalTex;
uniform sampler2D uShadowMap;
uniform int uUseMap, uUseNormalMap, uUnlit, uNumLights, uShadowOn;
uniform vec3 uBaseColor, uEmissive, uCamPos, uAmbient, uFogColor, uFlashPos, uFlashDir;
uniform float uRoughness, uEmissiveK, uFresnelK, uPulse, uAlpha, uTime;
uniform float uFogDensity, uAlarm, uFlashOn, uFlashRange, uFlashCosInner, uFlashCosOuter;
uniform vec3 uLightPos[8];
uniform vec3 uLightColor[8];
uniform float uLightRadius[8];

vec3 cotangentNormal(vec3 N, vec3 p, vec2 uv, vec3 texN){
  vec3 dp1 = dFdx(p), dp2 = dFdy(p);
  vec2 duv1 = dFdx(uv), duv2 = dFdy(uv);
  vec3 dp2perp = cross(dp2, N);
  vec3 dp1perp = cross(N, dp1);
  vec3 T = dp2perp*duv1.x + dp1perp*duv2.x;
  vec3 B = dp2perp*duv1.y + dp1perp*duv2.y;
  float invmax = inversesqrt(max(dot(T,T), dot(B,B)) + 1e-8);
  return normalize(mat3(T*invmax, B*invmax, N) * texN);
}
float shadowFactor(){
  if(uShadowOn==0) return 1.0;
  vec3 pc = vLightClip.xyz / max(vLightClip.w,1e-5) * 0.5 + 0.5;
  if(pc.x<0.0||pc.x>1.0||pc.y<0.0||pc.y>1.0||pc.z>1.0) return 1.0;
  float bias = 0.0035;
  vec2 texel = 1.0 / vec2(textureSize(uShadowMap,0));
  float s = 0.0;
  for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++){
    float d = texture(uShadowMap, pc.xy + vec2(x,y)*texel).r;
    s += (pc.z - bias > d) ? 0.0 : 1.0;
  }
  return s/9.0;
}
vec3 shade(vec3 albedo, vec3 N, vec3 V, vec3 P, float rough, float ao){
  // 天顶正交阴影图只能轻微压暗环境光，绝不能乘到手电上：
  // 手电在玩家视点，而该阴影图里天花板挡在地面之前，会把地面和大片墙面判为全阴影。
  float sh = shadowFactor();
  vec3 acc = uAmbient * albedo * (0.30 + 0.70*ao) * mix(0.72, 1.0, sh);
  float shininess = mix(110.0, 9.0, rough);
  float specK = mix(0.85, 0.10, rough);
  if(uFlashOn > 0.5){
    vec3 Lv = uFlashPos - P;
    float d = length(Lv);
    vec3 L = Lv/max(d,1e-4);
    float c = dot(-L, normalize(uFlashDir));
    float spot = smoothstep(uFlashCosOuter, uFlashCosInner, c);
    // 近距离散射补光：与朝向无关，保证脚下地面/侧墙不会陷入纯黑
    float fill = pow(clamp(1.0 - d/7.5, 0.0, 1.0), 2.0) * 0.55;
    float cone = max(spot, fill);
    float att = pow(clamp(1.0 - d/uFlashRange, 0.0, 1.0), 2.0);
    // wrap diffuse：法线与光线夹角大的表面（地面、侧墙）也保留一部分亮度
    float ndl = max((dot(N,L) + 0.42) / 1.42, 0.0);
    vec3 H = normalize(L+V);
    float spec = pow(max(dot(N,H),0.0), shininess)*specK;
    acc += (albedo*ndl + vec3(spec)) * vec3(1.0,0.93,0.83) * cone*att*2.35;
  }
  for(int i=0;i<8;i++){
    if(i>=uNumLights) break;
    vec3 Lv = uLightPos[i]-P;
    float d = length(Lv);
    if(d>uLightRadius[i]) continue;
    vec3 L = Lv/max(d,1e-4);
    float att = pow(clamp(1.0-d/uLightRadius[i],0.0,1.0),2.0);
    float ndl = max(dot(N,L),0.0);
    vec3 H = normalize(L+V);
    float spec = pow(max(dot(N,H),0.0),shininess)*specK;
    acc += (albedo*ndl + vec3(spec)) * uLightColor[i] * att;
  }
  return acc;
}
void main(){
  vec3 albedo = uBaseColor;
  if(uUseMap==1) albedo *= texture(uAlbedo, vUV).rgb;
  vec3 N = normalize(vNormal);
  if(uUseNormalMap==1){
    vec3 tn = texture(uNormalTex, vUV).xyz*2.0-1.0;
    N = cotangentNormal(N, vWorld, vUV, tn);
  }
  vec3 V = normalize(uCamPos - vWorld);
  float fres = pow(1.0 - max(dot(N,V),0.0), 3.0);
  vec3 col;
  if(uUnlit==1){
    col = albedo*(0.72+0.28*uPulse) + uEmissive*uEmissiveK*(0.78+0.22*sin(uTime*3.1+uPulse*6.0));
    col += uEmissive*fres*uFresnelK;
  } else {
    col = shade(albedo,N,V,vWorld,uRoughness,vAO);
    col += uEmissive*uEmissiveK;
    col += uEmissive*fres*uFresnelK*0.6;
  }
  col = mix(col, col*vec3(1.45,0.32,0.32)+vec3(0.055,0.0,0.0), uAlarm*0.5);
  float dist = length(uCamPos - vWorld);
  float fog = 1.0 - exp(-pow(uFogDensity*dist, 2.0));
  col = mix(col, uFogColor*(1.0+uAlarm*0.35), clamp(fog,0.0,0.96));
  fragColor = vec4(col, uAlpha);
}
)GLSL";

// ---------------------------------------------------------------------------
// 阴影深度 pass
// ---------------------------------------------------------------------------
static const char* DEPTH_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uLightVP, uModel;
void main(){ gl_Position = uLightVP * uModel * vec4(aPos,1.0); }
)GLSL";
static const char* DEPTH_FS = R"GLSL(
#version 330 core
void main(){}
)GLSL";

// ---------------------------------------------------------------------------
// 全屏后处理（共用顶点：全屏三角形，无需 VBO）
// ---------------------------------------------------------------------------
static const char* POST_VS = R"GLSL(
#version 330 core
out vec2 vUV;
void main(){
  vec2 p = vec2(float((gl_VertexID<<1)&2), float(gl_VertexID&2));
  vUV = p;
  gl_Position = vec4(p*2.0-1.0, 0.0, 1.0);
}
)GLSL";
static const char* BRIGHT_FS = R"GLSL(
#version 330 core
in vec2 vUV; out vec4 frag;
uniform sampler2D uScene; uniform float uThreshold;
void main(){
  vec3 c = texture(uScene, vUV).rgb;
  float l = dot(c, vec3(0.2126,0.7152,0.0722));
  frag = vec4(c*smoothstep(uThreshold, uThreshold+0.38, l), 1.0);
}
)GLSL";
static const char* BLUR_FS = R"GLSL(
#version 330 core
in vec2 vUV; out vec4 frag;
uniform sampler2D uTex; uniform vec2 uDir;
void main(){
  vec3 s = texture(uTex,vUV).rgb*0.227027;
  s += (texture(uTex,vUV+uDir*1.3846).rgb + texture(uTex,vUV-uDir*1.3846).rgb)*0.316216;
  s += (texture(uTex,vUV+uDir*3.2307).rgb + texture(uTex,vUV-uDir*3.2307).rgb)*0.070270;
  frag = vec4(s,1.0);
}
)GLSL";
static const char* COMPOSITE_FS = R"GLSL(
#version 330 core
in vec2 vUV; out vec4 frag;
uniform sampler2D uScene, uBloom;
uniform vec2 uResolution;
uniform float uTime, uVignette, uHurt, uDanger, uAlarm, uHeart, uFade, uGrain, uChase;
float hash(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
void main(){
  vec2 cc = vUV-0.5;
  float r2 = dot(cc,cc);
  float beat = uHeart*0.0045*sin(6.2831*1.15*uTime);
  vec2 uv = 0.5 + cc*(1.0+beat+uDanger*0.005*sin(uTime*8.0+r2*28.0));
  float ca = 0.0011 + uDanger*0.0036 + uHurt*0.0045;
  vec2 off = normalize(cc+vec2(1e-5))*ca*(0.25+r2*3.0);
  vec3 c;
  c.r = texture(uScene, clamp(uv+off,0.0,1.0)).r;
  c.g = texture(uScene, clamp(uv,0.0,1.0)).g;
  c.b = texture(uScene, clamp(uv-off,0.0,1.0)).b;
  c += texture(uBloom, clamp(uv,0.0,1.0)).rgb*(0.90+uAlarm*0.35);
  float edge = smoothstep(0.09,0.28,r2);
  c = mix(c, c*vec3(1.35,0.38,0.40), edge*uChase*0.6);
  float lum = dot(c, vec3(0.3,0.59,0.11));
  c = mix(c, vec3(lum)*vec3(1.28,0.38,0.38), uAlarm*0.20);
  c = c/(c+vec3(0.82))*1.32;
  c *= 1.0 - smoothstep(0.10,0.72,r2)*uVignette;
  c = mix(c, vec3(0.62,0.02,0.03), uHurt*0.62);
  float g = hash(uv*uResolution + vec2(fract(uTime)*137.0, fract(uTime*1.7)*311.0));
  c += (g-0.5)*uGrain;
  c *= 1.0 - 0.016*sin(uv.y*uResolution.y*1.55);
  c *= uFade;
  frag = vec4(clamp(c,0.0,1.0),1.0);
}
)GLSL";

// ---------------------------------------------------------------------------
// UI（预乘 alpha 输出，需配套 glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA)）
// ---------------------------------------------------------------------------
static const char* UI_VS = R"GLSL(
#version 330 core
out vec2 vUV;
uniform vec4 uRect;
uniform vec2 uScreen;
void main(){
  vec2 v[6] = vec2[6](vec2(0,0),vec2(1,0),vec2(1,1),vec2(0,0),vec2(1,1),vec2(0,1));
  vec2 q = v[gl_VertexID];
  vUV = q;
  vec2 px = uRect.xy + q*uRect.zw;
  gl_Position = vec4(px.x/uScreen.x*2.0-1.0, 1.0-px.y/uScreen.y*2.0, 0.0, 1.0);
}
)GLSL";
static const char* UI_FS = R"GLSL(
#version 330 core
in vec2 vUV; out vec4 frag;
uniform sampler2D uTex;
uniform vec4 uColor;
uniform int uMode;
uniform vec2 uTexel;
uniform float uGlow;
void main(){
  if(uMode==0){
    float a = texture(uTex, vUV).a;
    float o = a;
    for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++)
      o = max(o, texture(uTex, clamp(vUV+vec2(x,y)*uTexel*1.7,0.0,1.0)).a);
    float e = max(o-a,0.0);
    vec3 rgb = uColor.rgb*a + vec3(0.015,0.012,0.025)*e*uGlow;
    float alpha = min(1.0, a + e*uGlow*0.92);
    frag = vec4(rgb, alpha*uColor.a);
  } else if(uMode==1){
    frag = vec4(uColor.rgb*uColor.a, uColor.a);
  } else {
    // 带柔边的发光矩形
    vec2 d = abs(vUV-0.5)*2.0;
    float k = 1.0-smoothstep(0.75,1.0,max(d.x,d.y));
    frag = vec4(uColor.rgb*uColor.a*k, uColor.a*k);
  }
}
)GLSL";

// ---------------------------------------------------------------------------
// 世界空间粒子（billboard）
// ---------------------------------------------------------------------------
static const char* SPRITE_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aCenter;
layout(location=1) in vec2 aCorner;
layout(location=2) in float aSize;
layout(location=3) in vec4 aColor;
uniform mat4 uViewProj;
uniform vec2 uViewport;
out vec2 vUV; out vec4 vColor;
void main(){
  vec4 clip = uViewProj*vec4(aCenter,1.0);
  clip.xy += aCorner*aSize*2.0/uViewport*clip.w;
  gl_Position = clip;
  vUV = aCorner*0.5+0.5;
  vColor = aColor;
}
)GLSL";
static const char* SPRITE_FS = R"GLSL(
#version 330 core
in vec2 vUV; in vec4 vColor; out vec4 frag;
void main(){
  float d = length(vUV*2.0-1.0);
  float a = smoothstep(1.0,0.12,d);
  frag = vec4(vColor.rgb*vColor.a*a, vColor.a*a);
}
)GLSL";

// ---------------------------------------------------------------------------
// 图片怪物 billboard：视图空间展开的平面，尺寸随距离透视缩小（粒子是屏幕固定尺寸）
// aSize = 半高（世界单位），uAspect = 图片宽/高
// ---------------------------------------------------------------------------
static const char* MONSTER_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aCenter;
layout(location=1) in vec2 aCorner;
layout(location=2) in float aSize;
layout(location=3) in vec4 aColor;
uniform mat4 uView, uProj;
uniform float uAspect;
out vec2 vUV;
out vec4 vColor;
void main(){
  vec4 vp = uView * vec4(aCenter, 1.0);
  vp.xy += vec2(aCorner.x * aSize * uAspect, aCorner.y * aSize);
  gl_Position = uProj * vp;
  // GDI+ 数据是自上而下的，v=0 对应图片顶部
  vUV = vec2(aCorner.x * 0.5 + 0.5, 0.5 - aCorner.y * 0.5);
  vColor = aColor;
}
)GLSL";
static const char* MONSTER_FS = R"GLSL(
#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 frag;
uniform sampler2D uTex;
uniform float uTime;
void main(){
  vec4 t = texture(uTex, vUV);
  // 四边柔和淡出，避免整张照片出现生硬的白色方块边
  vec2 q = (vUV - 0.5) * 2.0;
  float edge = 1.0 - smoothstep(0.80, 1.0, max(abs(q.x), abs(q.y)));
  float a = t.a * vColor.a * edge;
  if(a < 0.012) discard;
  vec3 rgb = t.rgb * vColor.rgb * (0.94 + 0.06 * sin(uTime * 2.4));
  frag = vec4(rgb * a, a);   // 预乘 alpha，配合 GL_ONE / GL_ONE_MINUS_SRC_ALPHA
}
)GLSL";

// ---------------------------------------------------------------------------
// 传送门漩涡
// ---------------------------------------------------------------------------
static const char* PORTAL_VS = R"GLSL(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in float aAO;
uniform mat4 uProj, uView, uModel;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=uProj*uView*uModel*vec4(aPos,1.0); }
)GLSL";
static const char* PORTAL_FS = R"GLSL(
#version 330 core
in vec2 vUV; out vec4 frag;
uniform float uTime, uOpen;
float h(vec2 p){ return fract(sin(dot(p,vec2(127.1,311.7)))*43758.5453); }
float n(vec2 p){
  vec2 i=floor(p), f=fract(p); f=f*f*(3.0-2.0*f);
  return mix(mix(h(i),h(i+vec2(1,0)),f.x), mix(h(i+vec2(0,1)),h(i+vec2(1,1)),f.x), f.y);
}
void main(){
  vec2 p = vUV*2.0-1.0;
  float r = length(p);
  if(r>1.0) discard;
  float ang = atan(p.y,p.x);
  float s1 = n(vec2(ang*2.3+uTime*0.75, r*5.0-uTime*1.7));
  float s2 = n(vec2(ang*4.6-uTime*0.45, r*10.0+uTime*1.1));
  float v = s1*0.66+s2*0.34;
  float edge = smoothstep(1.0,0.68,r);
  vec3 c = mix(vec3(0.20,0.02,0.40), vec3(0.92,0.34,1.0), v);
  c += vec3(0.55,0.16,0.95)*pow(1.0-r,3.0)*1.5;
  c *= (0.30+0.70*(1.0-r))*edge*uOpen;
  frag = vec4(c,1.0);
}
)GLSL";
