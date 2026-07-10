#include "gl.h"
#include <cstdio>
#include <cstring>
#include <type_traits>

// ---- Minimal EGL declarations (headers not required) ----------------------
extern "C" {
typedef void* EGLDisplay;
typedef void* EGLConfig;
typedef void* EGLContext;
typedef void* EGLSurface;
typedef int   EGLint;
typedef unsigned int EGLenum;
typedef unsigned int EGLBoolean;

EGLDisplay eglGetPlatformDisplay(EGLenum, void*, const EGLint*);
EGLDisplay eglGetDisplay(void*);
EGLBoolean eglInitialize(EGLDisplay, EGLint*, EGLint*);
EGLBoolean eglChooseConfig(EGLDisplay, const EGLint*, EGLConfig*, EGLint, EGLint*);
EGLBoolean eglBindAPI(EGLenum);
EGLContext eglCreateContext(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
EGLBoolean eglMakeCurrent(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
EGLBoolean eglDestroyContext(EGLDisplay, EGLContext);
EGLBoolean eglTerminate(EGLDisplay);
EGLint     eglGetError(void);
void (*eglGetProcAddress(const char*))(void);
}

#define EGL_NO_CONTEXT   ((EGLContext)0)
#define EGL_NO_SURFACE   ((EGLSurface)0)
#define EGL_NO_DISPLAY   ((EGLDisplay)0)
#define EGL_PLATFORM_SURFACELESS_MESA 0x31DD
#define EGL_OPENGL_API   0x30A2
#define EGL_OPENGL_BIT   0x0008
#define EGL_SURFACE_TYPE 0x3033
#define EGL_PBUFFER_BIT  0x0001
#define EGL_RENDERABLE_TYPE 0x3040
#define EGL_RED_SIZE 0x3024
#define EGL_GREEN_SIZE 0x3023
#define EGL_BLUE_SIZE 0x3022
#define EGL_DEPTH_SIZE 0x3025
#define EGL_NONE 0x3038
#define EGL_CONTEXT_MAJOR_VERSION 0x3098
#define EGL_CONTEXT_MINOR_VERSION 0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK 0x30FD
#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT 0x00000001

// ---- Function pointer storage ---------------------------------------------
PFN_glActiveTexture           glActiveTexture           = nullptr;
PFN_glGenBuffers              glGenBuffers              = nullptr;
PFN_glBindBuffer              glBindBuffer              = nullptr;
PFN_glBufferData              glBufferData              = nullptr;
PFN_glDeleteBuffers           glDeleteBuffers           = nullptr;
PFN_glGenVertexArrays         glGenVertexArrays         = nullptr;
PFN_glBindVertexArray         glBindVertexArray         = nullptr;
PFN_glDeleteVertexArrays      glDeleteVertexArrays      = nullptr;
PFN_glEnableVertexAttribArray glEnableVertexAttribArray = nullptr;
PFN_glVertexAttribPointer     glVertexAttribPointer     = nullptr;
PFN_glCreateShader            glCreateShader            = nullptr;
PFN_glShaderSource            glShaderSource            = nullptr;
PFN_glCompileShader           glCompileShader           = nullptr;
PFN_glGetShaderiv             glGetShaderiv             = nullptr;
PFN_glGetShaderInfoLog        glGetShaderInfoLog        = nullptr;
PFN_glCreateProgram           glCreateProgram           = nullptr;
PFN_glAttachShader            glAttachShader            = nullptr;
PFN_glLinkProgram             glLinkProgram             = nullptr;
PFN_glGetProgramiv            glGetProgramiv            = nullptr;
PFN_glGetProgramInfoLog       glGetProgramInfoLog       = nullptr;
PFN_glDeleteShader            glDeleteShader            = nullptr;
PFN_glDeleteProgram           glDeleteProgram           = nullptr;
PFN_glUseProgram              glUseProgram              = nullptr;
PFN_glGetUniformLocation      glGetUniformLocation      = nullptr;
PFN_glUniformMatrix4fv        glUniformMatrix4fv        = nullptr;
PFN_glUniform1i               glUniform1i               = nullptr;
PFN_glUniform3f               glUniform3f               = nullptr;
PFN_glGenerateMipmap          glGenerateMipmap          = nullptr;
PFN_glGenFramebuffers         glGenFramebuffers         = nullptr;
PFN_glBindFramebuffer         glBindFramebuffer         = nullptr;
PFN_glFramebufferTexture2D    glFramebufferTexture2D    = nullptr;
PFN_glGenRenderbuffers        glGenRenderbuffers        = nullptr;
PFN_glBindRenderbuffer        glBindRenderbuffer        = nullptr;
PFN_glRenderbufferStorage     glRenderbufferStorage     = nullptr;
PFN_glFramebufferRenderbuffer glFramebufferRenderbuffer = nullptr;
PFN_glCheckFramebufferStatus  glCheckFramebufferStatus  = nullptr;
PFN_glDeleteFramebuffers      glDeleteFramebuffers      = nullptr;
PFN_glDeleteRenderbuffers     glDeleteRenderbuffers     = nullptr;

static EGLDisplay g_dpy = EGL_NO_DISPLAY;
static EGLContext g_ctx = EGL_NO_CONTEXT;

static bool loadProcs()
{
    bool ok = true;
    auto load = [&](auto &fp, const char *name) {
        fp = reinterpret_cast<std::decay_t<decltype(fp)>>(eglGetProcAddress(name));
        if(!fp) { fprintf(stderr, "gl: missing entry point %s\n", name); ok = false; }
    };
    load(glActiveTexture, "glActiveTexture");
    load(glGenBuffers, "glGenBuffers");
    load(glBindBuffer, "glBindBuffer");
    load(glBufferData, "glBufferData");
    load(glDeleteBuffers, "glDeleteBuffers");
    load(glGenVertexArrays, "glGenVertexArrays");
    load(glBindVertexArray, "glBindVertexArray");
    load(glDeleteVertexArrays, "glDeleteVertexArrays");
    load(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    load(glVertexAttribPointer, "glVertexAttribPointer");
    load(glCreateShader, "glCreateShader");
    load(glShaderSource, "glShaderSource");
    load(glCompileShader, "glCompileShader");
    load(glGetShaderiv, "glGetShaderiv");
    load(glGetShaderInfoLog, "glGetShaderInfoLog");
    load(glCreateProgram, "glCreateProgram");
    load(glAttachShader, "glAttachShader");
    load(glLinkProgram, "glLinkProgram");
    load(glGetProgramiv, "glGetProgramiv");
    load(glGetProgramInfoLog, "glGetProgramInfoLog");
    load(glDeleteShader, "glDeleteShader");
    load(glDeleteProgram, "glDeleteProgram");
    load(glUseProgram, "glUseProgram");
    load(glGetUniformLocation, "glGetUniformLocation");
    load(glUniformMatrix4fv, "glUniformMatrix4fv");
    load(glUniform1i, "glUniform1i");
    load(glUniform3f, "glUniform3f");
    load(glGenerateMipmap, "glGenerateMipmap");
    load(glGenFramebuffers, "glGenFramebuffers");
    load(glBindFramebuffer, "glBindFramebuffer");
    load(glFramebufferTexture2D, "glFramebufferTexture2D");
    load(glGenRenderbuffers, "glGenRenderbuffers");
    load(glBindRenderbuffer, "glBindRenderbuffer");
    load(glRenderbufferStorage, "glRenderbufferStorage");
    load(glFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    load(glCheckFramebufferStatus, "glCheckFramebufferStatus");
    load(glDeleteFramebuffers, "glDeleteFramebuffers");
    load(glDeleteRenderbuffers, "glDeleteRenderbuffers");
    return ok;
}

bool gl_headless_init()
{
    const EGLint cfgAttr[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 24,
        EGL_NONE
    };
    const EGLint ctxAttr[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };

    g_dpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, nullptr, nullptr);
    if(g_dpy == EGL_NO_DISPLAY) g_dpy = eglGetDisplay(nullptr); // fall back to default device
    if(g_dpy == EGL_NO_DISPLAY) { fprintf(stderr, "egl: no display\n"); return false; }

    EGLint major, minor;
    if(!eglInitialize(g_dpy, &major, &minor)) { fprintf(stderr, "egl: init failed (0x%x)\n", eglGetError()); return false; }
    if(!eglBindAPI(EGL_OPENGL_API)) { fprintf(stderr, "egl: bindAPI failed\n"); return false; }

    EGLConfig cfg; EGLint n = 0;
    if(!eglChooseConfig(g_dpy, cfgAttr, &cfg, 1, &n) || n < 1) { fprintf(stderr, "egl: no config (0x%x)\n", eglGetError()); return false; }

    g_ctx = eglCreateContext(g_dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
    if(g_ctx == EGL_NO_CONTEXT) { fprintf(stderr, "egl: createContext failed (0x%x)\n", eglGetError()); return false; }

    // Surfaceless: render targets are FBOs, so no window/pbuffer surface is bound.
    if(!eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, g_ctx)) {
        fprintf(stderr, "egl: makeCurrent failed (0x%x)\n", eglGetError());
        return false;
    }
    if(!loadProcs()) return false;

    printf("GL %s / %s\n", (const char*)glGetString(GL_VERSION), (const char*)glGetString(GL_RENDERER));
    return true;
}

void gl_headless_shutdown()
{
    if(g_dpy == EGL_NO_DISPLAY) return;
    eglMakeCurrent(g_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if(g_ctx != EGL_NO_CONTEXT) eglDestroyContext(g_dpy, g_ctx);
    eglTerminate(g_dpy);
    g_dpy = EGL_NO_DISPLAY; g_ctx = EGL_NO_CONTEXT;
}
