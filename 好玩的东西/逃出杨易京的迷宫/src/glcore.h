#pragma once
// ============================================================================
//  glcore.h : 自包含 OpenGL 3.3 运行时
//  不依赖 GLAD / GLEW / glext.h / GLFW，自己从 opengl32.dll + wglGetProcAddress
//  加载所需函数。链接库只需要 opengl32.lib / gdi32.lib / user32.lib 等系统库。
// ============================================================================
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstddef>
#include <string>

typedef unsigned int   GLenum;
typedef unsigned int   GLuint;
typedef unsigned int   GLbitfield;
typedef int            GLint;
typedef int            GLsizei;
typedef float          GLfloat;
typedef float          GLclampf;
typedef double         GLdouble;
typedef char           GLchar;
typedef signed char    GLbyte;
typedef unsigned char  GLubyte;
typedef unsigned char  GLboolean;
typedef short          GLshort;
typedef unsigned short GLushort;
typedef void           GLvoid;
typedef ptrdiff_t      GLintptr;
typedef ptrdiff_t      GLsizeiptr;

// ------------------------------ GL 常量 -------------------------------------
#define GL_FALSE                       0
#define GL_TRUE                        1
#define GL_NONE                        0
#define GL_ZERO                        0
#define GL_ONE                         1
#define GL_LINES                       0x0001
#define GL_TRIANGLES                   0x0004
#define GL_TRIANGLE_STRIP              0x0005
#define GL_SRC_ALPHA                   0x0302
#define GL_ONE_MINUS_SRC_ALPHA         0x0303
#define GL_FRONT                       0x0404
#define GL_BACK                        0x0405
#define GL_LESS                        0x0201
#define GL_LEQUAL                      0x0203
#define GL_CCW                         0x0901
#define GL_DEPTH_BUFFER_BIT            0x00000100
#define GL_COLOR_BUFFER_BIT            0x00004000
#define GL_DEPTH_TEST                  0x0B71
#define GL_BLEND                       0x0BE2
#define GL_CULL_FACE                   0x0B44
#define GL_SCISSOR_TEST                0x0C11
#define GL_UNSIGNED_BYTE               0x1401
#define GL_UNSIGNED_SHORT              0x1403
#define GL_UNSIGNED_INT                0x1405
#define GL_FLOAT                       0x1406
#define GL_HALF_FLOAT                  0x140B
#define GL_RGB                         0x1907
#define GL_RGBA                        0x1908
#define GL_DEPTH_COMPONENT             0x1902
#define GL_NEAREST                     0x2600
#define GL_LINEAR                      0x2601
#define GL_NEAREST_MIPMAP_NEAREST      0x2700
#define GL_LINEAR_MIPMAP_LINEAR        0x2703
#define GL_TEXTURE_MAG_FILTER          0x2800
#define GL_TEXTURE_MIN_FILTER          0x2801
#define GL_TEXTURE_WRAP_S              0x2802
#define GL_TEXTURE_WRAP_T              0x2803
#define GL_TEXTURE_WRAP_R              0x8072
#define GL_TEXTURE_BORDER_COLOR        0x1004
#define GL_TEXTURE_COMPARE_MODE        0x884C
#define GL_TEXTURE_COMPARE_FUNC        0x884D
#define GL_COMPARE_REF_TO_TEXTURE      0x884E
#define GL_REPEAT                      0x2901
#define GL_CLAMP_TO_EDGE               0x812F
#define GL_CLAMP_TO_BORDER             0x812D
#define GL_TEXTURE0                    0x84C0
#define GL_TEXTURE1                    0x84C1
#define GL_TEXTURE2                    0x84C2
#define GL_TEXTURE3                    0x84C3
#define GL_TEXTURE4                    0x84C4
#define GL_TEXTURE5                    0x84C5
#define GL_TEXTURE6                    0x84C6
#define GL_TEXTURE7                    0x84C7
#define GL_UNPACK_ALIGNMENT            0x0CF5
#define GL_ARRAY_BUFFER                0x8892
#define GL_ELEMENT_ARRAY_BUFFER        0x8893
#define GL_STREAM_DRAW                 0x88E0
#define GL_STATIC_DRAW                 0x88E4
#define GL_DYNAMIC_DRAW                0x88E8
#define GL_FRAGMENT_SHADER             0x8B30
#define GL_VERTEX_SHADER               0x8B31
#define GL_COMPILE_STATUS              0x8B81
#define GL_LINK_STATUS                 0x8B82
#define GL_INFO_LOG_LENGTH             0x8B84
#define GL_MAX_TEXTURE_SIZE            0x0D33
#define GL_VERSION                     0x1F02
#define GL_VENDOR                      0x1F00
#define GL_RENDERER                    0x1F01
#define GL_SHADING_LANGUAGE_VERSION    0x8B8C
#define GL_FRAMEBUFFER                 0x8D40
#define GL_RENDERBUFFER                0x8D41
#define GL_COLOR_ATTACHMENT0           0x8CE0
#define GL_COLOR_ATTACHMENT1           0x8CE1
#define GL_DEPTH_ATTACHMENT            0x8D00
#define GL_FRAMEBUFFER_COMPLETE        0x8CD5
#define GL_RGB8                        0x8051
#define GL_RGBA8                       0x8058
#define GL_RGBA16F                     0x881A
#define GL_RGB16F                      0x881B
#define GL_DEPTH_COMPONENT24           0x81A6
#define GL_FUNC_ADD                    0x8006
#define GL_TEXTURE_2D                  0x0DE1
#define GL_TEXTURE_CUBE_MAP            0x8513
#define GL_MAX_TEXTURE_IMAGE_UNITS     0x8872
#define GL_MAX_VERTEX_ATTRIBS          0x8869
#define GL_INVALID_ENUM                0x0500
#define GL_INVALID_VALUE               0x0501
#define GL_INVALID_OPERATION           0x0502
#define GL_OUT_OF_MEMORY               0x0505
#define GL_NO_ERROR                    0

// ------------------------- WGL 扩展常量 / 原型 ------------------------------
#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_SAMPLE_BUFFERS_ARB         0x2041
#define WGL_SAMPLES_ARB                0x2042
#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_FLAGS_ARB          0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB   0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB          0x00000001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB    0x00000002

// wglCreateContext / wglMakeCurrent / wglDeleteContext / wglGetProcAddress
// 已由 Windows SDK 的 wingdi.h 声明，链接 opengl32.lib 即可，不重复声明。
typedef HGLRC (WINAPI * PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL  (WINAPI * PFN_wglSwapIntervalEXT)(int);
typedef BOOL  (WINAPI * PFN_wglChoosePixelFormatARB)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);

// --------------------------- 函数表宏 ---------------------------------------
#define GL_PROC_LIST(X)                                                                                    \
  X(const GLubyte*, glGetString,          (GLenum name))                                                   \
  X(GLenum,         glGetError,           (void))                                                          \
  X(void,           glGetIntegerv,        (GLenum pname, GLint* params))                                  \
  X(void,           glClear,              (GLbitfield mask))                                              \
  X(void,           glClearColor,         (GLfloat r, GLfloat g, GLfloat b, GLfloat a))                  \
  X(void,           glClearDepth,         (GLdouble depth))                                               \
  X(void,           glEnable,             (GLenum cap))                                                   \
  X(void,           glDisable,            (GLenum cap))                                                   \
  X(void,           glBlendFunc,          (GLenum sfactor, GLenum dfactor))                               \
  X(void,           glBlendFuncSeparate,  (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)) \
  X(void,           glBlendEquation,      (GLenum mode))                                                  \
  X(void,           glDepthFunc,          (GLenum func))                                                  \
  X(void,           glDepthMask,          (GLboolean flag))                                               \
  X(void,           glColorMask,          (GLboolean r, GLboolean g, GLboolean b, GLboolean a))           \
  X(void,           glCullFace,           (GLenum mode))                                                  \
  X(void,           glFrontFace,          (GLenum mode))                                                  \
  X(void,           glViewport,           (GLint x, GLint y, GLsizei w, GLsizei h))                       \
  X(void,           glScissor,            (GLint x, GLint y, GLsizei w, GLsizei h))                       \
  X(void,           glPolygonMode,        (GLenum face, GLenum mode))                                     \
  X(void,           glFinish,             (void))                                                         \
  X(void,           glGenBuffers,         (GLsizei n, GLuint* buffers))                                    \
  X(void,           glBindBuffer,         (GLenum target, GLuint buffer))                                 \
  X(void,           glBufferData,         (GLenum target, GLsizeiptr size, const void* data, GLenum usage)) \
  X(void,           glBufferSubData,      (GLenum target, GLintptr offset, GLsizeiptr size, const void* data)) \
  X(void,           glDeleteBuffers,      (GLsizei n, const GLuint* buffers))                              \
  X(void,           glGenVertexArrays,    (GLsizei n, GLuint* arrays))                                     \
  X(void,           glBindVertexArray,    (GLuint array))                                                  \
  X(void,           glDeleteVertexArrays, (GLsizei n, const GLuint* arrays))                               \
  X(void,           glEnableVertexAttribArray,  (GLuint index))                                             \
  X(void,           glDisableVertexAttribArray, (GLuint index))                                              \
  X(void,           glVertexAttribPointer,(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer)) \
  X(void,           glVertexAttribDivisor,(GLuint index, GLuint divisor))                                   \
  X(void,           glDrawArrays,         (GLenum mode, GLint first, GLsizei count))                        \
  X(void,           glDrawElements,       (GLenum mode, GLsizei count, GLenum type, const void* indices))   \
  X(void,           glDrawArraysInstanced,(GLenum mode, GLint first, GLsizei count, GLsizei primcount))     \
  X(void,           glGenTextures,        (GLsizei n, GLuint* textures))                                    \
  X(void,           glBindTexture,        (GLenum target, GLuint texture))                                  \
  X(void,           glTexImage2D,         (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels)) \
  X(void,           glTexSubImage2D,      (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels)) \
  X(void,           glTexParameteri,      (GLenum target, GLenum pname, GLint param))                        \
  X(void,           glTexParameterf,      (GLenum target, GLenum pname, GLfloat param))                      \
  X(void,           glTexParameterfv,     (GLenum target, GLenum pname, const GLfloat* params))              \
  X(void,           glGenerateMipmap,     (GLenum target))                                                 \
  X(void,           glActiveTexture,      (GLenum texture))                                                \
  X(void,           glDeleteTextures,     (GLsizei n, const GLuint* textures))                              \
  X(void,           glPixelStorei,        (GLenum pname, GLint param))                                     \
  X(GLuint,         glCreateShader,       (GLenum type))                                                   \
  X(void,           glShaderSource,       (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length)) \
  X(void,           glCompileShader,      (GLuint shader))                                                 \
  X(void,           glGetShaderiv,        (GLuint shader, GLenum pname, GLint* params))                     \
  X(void,           glGetShaderInfoLog,   (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
  X(void,           glDeleteShader,       (GLuint shader))                                                 \
  X(GLuint,         glCreateProgram,      (void))                                                          \
  X(void,           glAttachShader,       (GLuint program, GLuint shader))                                 \
  X(void,           glLinkProgram,        (GLuint program))                                                \
  X(void,           glGetProgramiv,       (GLuint program, GLenum pname, GLint* params))                    \
  X(void,           glGetProgramInfoLog,  (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog)) \
  X(void,           glUseProgram,         (GLuint program))                                                \
  X(void,           glDeleteProgram,      (GLuint program))                                                \
  X(void,           glBindAttribLocation, (GLuint program, GLuint index, const GLchar* name))               \
  X(GLint,          glGetUniformLocation, (GLuint program, const GLchar* name))                             \
  X(void,           glUniform1i,          (GLint location, GLint v0))                                       \
  X(void,           glUniform1f,          (GLint location, GLfloat v0))                                     \
  X(void,           glUniform2f,          (GLint location, GLfloat v0, GLfloat v1))                         \
  X(void,           glUniform3f,          (GLint location, GLfloat v0, GLfloat v1, GLfloat v2))              \
  X(void,           glUniform4f,          (GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3))   \
  X(void,           glUniform1fv,         (GLint location, GLsizei count, const GLfloat* value))              \
  X(void,           glUniform2fv,         (GLint location, GLsizei count, const GLfloat* value))              \
  X(void,           glUniform3fv,         (GLint location, GLsizei count, const GLfloat* value))              \
  X(void,           glUniform4fv,         (GLint location, GLsizei count, const GLfloat* value))              \
  X(void,           glUniformMatrix4fv,   (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)) \
  X(void,           glUniformMatrix3fv,   (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)) \
  X(void,           glGenFramebuffers,    (GLsizei n, GLuint* framebuffers))                                 \
  X(void,           glBindFramebuffer,    (GLenum target, GLuint framebuffer))                               \
  X(void,           glFramebufferTexture2D,(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)) \
  X(void,           glFramebufferRenderbuffer,(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)) \
  X(GLenum,         glCheckFramebufferStatus,(GLenum target))                                                \
  X(void,           glDeleteFramebuffers, (GLsizei n, const GLuint* framebuffers))                           \
  X(void,           glGenRenderbuffers,   (GLsizei n, GLuint* renderbuffers))                                \
  X(void,           glBindRenderbuffer,   (GLenum target, GLuint renderbuffer))                              \
  X(void,           glRenderbufferStorage,(GLenum target, GLenum internalformat, GLsizei width, GLsizei height)) \
  X(void,           glDeleteRenderbuffers,(GLsizei n, const GLuint* renderbuffers))                           \
  X(void,           glDrawBuffer,         (GLenum buf))                                                       \
  X(void,           glDrawBuffers,        (GLsizei n, const GLenum* bufs))                                   \
  X(void,           glReadBuffer,         (GLenum src))                                                      \
  X(void,           glReadPixels,         (GLint x, GLint y, GLsizei w, GLsizei h, GLenum format, GLenum type, void* pixels))

// --------------------------- 函数指针定义 -----------------------------------
namespace glapi {
#define GL_DECLARE(ret, name, args) typedef ret (APIENTRY * PFN_##name) args; inline PFN_##name name = nullptr;
GL_PROC_LIST(GL_DECLARE)
#undef GL_DECLARE

inline void* LoadProc(const char* n) {
    void* p = (void*)wglGetProcAddress(n);
    if (p == nullptr || p == (void*)1 || p == (void*)2 || p == (void*)3 || p == (void*)-1) {
        HMODULE mod = GetModuleHandleW(L"opengl32.dll");
        if (mod) p = (void*)GetProcAddress(mod, n);
    }
    return p;
}

inline bool Init(std::string& missing) {
    bool ok = true;
#define GL_LOAD(ret, name, args) name = (PFN_##name)LoadProc(#name); if (!name) { if (!missing.empty()) missing += ", "; missing += #name; ok = false; }
    GL_PROC_LIST(GL_LOAD)
#undef GL_LOAD
    return ok;
}
} // namespace glapi
