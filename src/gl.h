// Headless OpenGL for the gallery renderer.
//
// Replaces the usual SDL2 + GLEW stack: an EGL surfaceless context (Mesa/llvmpipe
// under WSLg, or a real GPU) plus a hand-rolled loader for the GL 2.0/3.0 entry
// points we use. GL <=1.1 symbols are linked directly from libGL; everything
// newer is resolved at runtime with eglGetProcAddress. No -dev packages required.
#pragma once
#include <cstdint>
#include <cstddef>

// ---- GL types -------------------------------------------------------------
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef void           GLvoid;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef char           GLchar;
typedef unsigned char  GLubyte;
typedef ptrdiff_t      GLsizeiptr;
typedef ptrdiff_t      GLintptr;

// ---- GL constants ---------------------------------------------------------
#define GL_FALSE 0
#define GL_TRUE  1
#define GL_NO_ERROR 0
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_INT 0x1405
#define GL_FLOAT 0x1406
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_RGBA8 0x8058
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_BLEND 0x0BE2
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_FRAMEBUFFER 0x8D40
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5

// ---- GL <=1.1: linked directly from libGL --------------------------------
extern "C" {
    void  glClear(GLbitfield);
    void  glClearColor(GLfloat, GLfloat, GLfloat, GLfloat);
    void  glViewport(GLint, GLint, GLsizei, GLsizei);
    void  glEnable(GLenum);
    void  glDisable(GLenum);
    void  glDepthMask(GLboolean);
    void  glBlendFunc(GLenum, GLenum);
    void  glDrawElements(GLenum, GLsizei, GLenum, const void*);
    void  glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);
    void  glPixelStorei(GLenum, GLint);
    GLenum glGetError(void);
    void  glFinish(void);
    void  glFlush(void);
    void  glGenTextures(GLsizei, GLuint*);
    void  glBindTexture(GLenum, GLuint);
    void  glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*);
    void  glTexParameteri(GLenum, GLenum, GLint);
    void  glDeleteTextures(GLsizei, const GLuint*);
    const GLubyte* glGetString(GLenum);
}

// ---- GL 1.3 / 1.5 / 2.0 / 3.0: resolved via eglGetProcAddress -------------
typedef void   (*PFN_glActiveTexture)(GLenum);
typedef void   (*PFN_glGenBuffers)(GLsizei, GLuint*);
typedef void   (*PFN_glBindBuffer)(GLenum, GLuint);
typedef void   (*PFN_glBufferData)(GLenum, GLsizeiptr, const void*, GLenum);
typedef void   (*PFN_glDeleteBuffers)(GLsizei, const GLuint*);
typedef void   (*PFN_glGenVertexArrays)(GLsizei, GLuint*);
typedef void   (*PFN_glBindVertexArray)(GLuint);
typedef void   (*PFN_glDeleteVertexArrays)(GLsizei, const GLuint*);
typedef void   (*PFN_glEnableVertexAttribArray)(GLuint);
typedef void   (*PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
typedef GLuint (*PFN_glCreateShader)(GLenum);
typedef void   (*PFN_glShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*);
typedef void   (*PFN_glCompileShader)(GLuint);
typedef void   (*PFN_glGetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (*PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef GLuint (*PFN_glCreateProgram)(void);
typedef void   (*PFN_glAttachShader)(GLuint, GLuint);
typedef void   (*PFN_glLinkProgram)(GLuint);
typedef void   (*PFN_glGetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (*PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
typedef void   (*PFN_glDeleteShader)(GLuint);
typedef void   (*PFN_glDeleteProgram)(GLuint);
typedef void   (*PFN_glUseProgram)(GLuint);
typedef GLint  (*PFN_glGetUniformLocation)(GLuint, const GLchar*);
typedef void   (*PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
typedef void   (*PFN_glUniform1i)(GLint, GLint);
typedef void   (*PFN_glUniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (*PFN_glGenerateMipmap)(GLenum);
typedef void   (*PFN_glGenFramebuffers)(GLsizei, GLuint*);
typedef void   (*PFN_glBindFramebuffer)(GLenum, GLuint);
typedef void   (*PFN_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void   (*PFN_glGenRenderbuffers)(GLsizei, GLuint*);
typedef void   (*PFN_glBindRenderbuffer)(GLenum, GLuint);
typedef void   (*PFN_glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
typedef void   (*PFN_glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
typedef GLenum (*PFN_glCheckFramebufferStatus)(GLenum);
typedef void   (*PFN_glDeleteFramebuffers)(GLsizei, const GLuint*);
typedef void   (*PFN_glDeleteRenderbuffers)(GLsizei, const GLuint*);

extern PFN_glActiveTexture           glActiveTexture;
extern PFN_glGenBuffers              glGenBuffers;
extern PFN_glBindBuffer              glBindBuffer;
extern PFN_glBufferData              glBufferData;
extern PFN_glDeleteBuffers           glDeleteBuffers;
extern PFN_glGenVertexArrays         glGenVertexArrays;
extern PFN_glBindVertexArray         glBindVertexArray;
extern PFN_glDeleteVertexArrays      glDeleteVertexArrays;
extern PFN_glEnableVertexAttribArray glEnableVertexAttribArray;
extern PFN_glVertexAttribPointer     glVertexAttribPointer;
extern PFN_glCreateShader            glCreateShader;
extern PFN_glShaderSource            glShaderSource;
extern PFN_glCompileShader           glCompileShader;
extern PFN_glGetShaderiv             glGetShaderiv;
extern PFN_glGetShaderInfoLog        glGetShaderInfoLog;
extern PFN_glCreateProgram           glCreateProgram;
extern PFN_glAttachShader            glAttachShader;
extern PFN_glLinkProgram             glLinkProgram;
extern PFN_glGetProgramiv            glGetProgramiv;
extern PFN_glGetProgramInfoLog       glGetProgramInfoLog;
extern PFN_glDeleteShader            glDeleteShader;
extern PFN_glDeleteProgram           glDeleteProgram;
extern PFN_glUseProgram              glUseProgram;
extern PFN_glGetUniformLocation      glGetUniformLocation;
extern PFN_glUniformMatrix4fv        glUniformMatrix4fv;
extern PFN_glUniform1i               glUniform1i;
extern PFN_glUniform3f               glUniform3f;
extern PFN_glGenerateMipmap          glGenerateMipmap;
extern PFN_glGenFramebuffers         glGenFramebuffers;
extern PFN_glBindFramebuffer         glBindFramebuffer;
extern PFN_glFramebufferTexture2D    glFramebufferTexture2D;
extern PFN_glGenRenderbuffers        glGenRenderbuffers;
extern PFN_glBindRenderbuffer        glBindRenderbuffer;
extern PFN_glRenderbufferStorage     glRenderbufferStorage;
extern PFN_glFramebufferRenderbuffer glFramebufferRenderbuffer;
extern PFN_glCheckFramebufferStatus  glCheckFramebufferStatus;
extern PFN_glDeleteFramebuffers      glDeleteFramebuffers;
extern PFN_glDeleteRenderbuffers     glDeleteRenderbuffers;

// Create a headless GL 3.3 context and resolve all entry points.
// Returns false (with a message on stderr) if no EGL device is usable.
bool gl_headless_init();
void gl_headless_shutdown();
