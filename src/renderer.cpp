#include "renderer.h"
#include "vfs.h"
#include "xmodel.h"
#include "material.h"
#include "image.h"
#include "math.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

static const char *VERT_SRC = R"(#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNorm;
layout(location=2) in vec2 aUV;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vNorm;
out vec2 vUV;
void main(){
    gl_Position = uMVP * vec4(aPos, 1.0);
    vNorm = mat3(uModel) * aNorm;
    vUV = aUV;
}
)";

static const char *FRAG_SRC = R"(#version 330 core
in vec3 vNorm; in vec2 vUV;
uniform sampler2D uTex;
uniform int uAlphaMode;
out vec4 fColor;
void main(){
    vec3 n = normalize(vNorm);
    vec3 L1 = normalize(vec3(1.0, 0.8, 1.2));
    vec3 L2 = normalize(vec3(-0.5, -1.0, 0.3));
    float d = max(dot(n, L1), 0.0) + 0.25 * max(dot(n, L2), 0.0);
    vec4 col = texture(uTex, vUV);
    if(uAlphaMode == 1 && col.a < 0.5) discard;
    float shade = 0.35 + 0.65 * d;
    fColor = vec4(col.rgb * shade, uAlphaMode == 2 ? col.a : 1.0);
}
)";

static GLuint compile(GLenum type, const char *src)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log); fprintf(stderr, "shader: %s\n", log); }
    return s;
}

static GLuint uploadRGBA(const uint8_t *rgba, int w, int h)
{
    GLuint t; glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glGenerateMipmap(GL_TEXTURE_2D);
    return t;
}

bool Renderer::init(int outW, int outH, int supersample)
{
    m_outW = outW; m_outH = outH; m_ss = std::max(1, supersample);
    m_rw = outW * m_ss; m_rh = outH * m_ss;

    GLuint vs = compile(GL_VERTEX_SHADER, VERT_SRC);
    GLuint fs = compile(GL_FRAGMENT_SHADER, FRAG_SRC);
    m_prog = glCreateProgram();
    glAttachShader(m_prog, vs); glAttachShader(m_prog, fs);
    glLinkProgram(m_prog);
    glDeleteShader(vs); glDeleteShader(fs);
    GLint ok = 0; glGetProgramiv(m_prog, GL_LINK_STATUS, &ok);
    if(!ok) { char log[1024]; glGetProgramInfoLog(m_prog, 1024, nullptr, log); fprintf(stderr, "link: %s\n", log); return false; }

    glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glGenRenderbuffers(1, &m_color);
    glBindRenderbuffer(GL_RENDERBUFFER, m_color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, m_rw, m_rh);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_color);
    glGenRenderbuffers(1, &m_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_rw, m_rh);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "renderer: framebuffer incomplete\n"); shutdown(); return false;
    }

    // 2x2 grey fallback for surfaces with no resolvable texture.
    uint8_t grey[2*2*4]; for(int i = 0; i < 4; i++) { grey[i*4]=160; grey[i*4+1]=160; grey[i*4+2]=160; grey[i*4+3]=255; }
    m_white = uploadRGBA(grey, 2, 2);
    return true;
}

void Renderer::shutdown()
{
    clearTextureCache();
    if(m_white) glDeleteTextures(1, &m_white);
    if(m_color) glDeleteRenderbuffers(1, &m_color);
    if(m_depth) glDeleteRenderbuffers(1, &m_depth);
    if(m_fbo)   glDeleteFramebuffers(1, &m_fbo);
    if(m_prog)  glDeleteProgram(m_prog);
    m_white = m_color = m_depth = m_fbo = m_prog = 0;
}

void Renderer::clearTextureCache()
{
    for(auto &kv : m_texCache) if(kv.second) glDeleteTextures(1, &kv.second);
    m_texCache.clear();
}

GLuint Renderer::loadTexture(const std::string &material, const VFS &vfs, int &alphaMode)
{
    alphaMode = 0;
    std::string key = VFS::lower(material);
    auto it = m_texCache.find(key);
    if(it != m_texCache.end()) return it->second;

    // material -> colorMap texture name -> images/<name>.iwi
    std::string texName;
    auto matData = vfs.read("materials/" + material);
    if(!matData.empty()) {
        MaterialInfo mi;
        if(parse_material(matData, mi)) texName = mi.colorMap;
    }
    if(texName.empty()) texName = material; // some models name the image directly

    Image img;
    const char *exts[] = { "", ".iwi", ".dds", ".tga" };
    for(const char *e : exts) {
        auto d = vfs.read("images/" + texName + e);
        if(d.empty()) continue;
        img = decode_texture(d);
        if(img.ok()) break;
    }
    if(!img.ok()) { m_texCache[key] = 0; return 0; }

    if(img.hasAlpha()) alphaMode = 1; // data-driven cutout for foliage/fences/etc.
    GLuint tex = uploadRGBA(img.rgba.data(), img.w, img.h);
    m_texCache[key] = tex;
    return tex;
}

std::vector<GpuSurf> Renderer::upload(const XModel &model, const VFS &vfs)
{
    std::vector<GpuSurf> out;
    out.reserve(model.surfaces.size());
    for(const auto &s : model.surfaces) {
        GpuSurf g;
        if(s.verts.empty() || s.indices.empty()) { out.push_back(g); continue; }

        glGenVertexArrays(1, &g.vao);
        glBindVertexArray(g.vao);
        glGenBuffers(1, &g.vbo);
        glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
        glBufferData(GL_ARRAY_BUFFER, s.verts.size() * sizeof(XVertex), s.verts.data(), GL_STATIC_DRAW);
        glGenBuffers(1, &g.ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, s.indices.size() * sizeof(uint32_t), s.indices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(XVertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(XVertex), (void*)12);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(XVertex), (void*)24);
        glBindVertexArray(0);
        g.indexCount = (int)s.indices.size();

        if(!s.material.empty()) g.tex = loadTexture(s.material, vfs, g.alphaMode);
        if(!g.tex) g.tex = m_white;
        out.push_back(g);
    }
    return out;
}

void Renderer::freeSurfs(std::vector<GpuSurf> &surfs)
{
    for(auto &g : surfs) {
        if(g.vao) glDeleteVertexArrays(1, &g.vao);
        if(g.vbo) glDeleteBuffers(1, &g.vbo);
        if(g.ibo) glDeleteBuffers(1, &g.ibo);
    }
    surfs.clear();
}

// CoD space is Z-up; the camera orbits on a sphere looking at the origin.
void Renderer::render(const std::vector<GpuSurf> &surfs, float theta, float phi, float dist)
{
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_rw, m_rh);
    glClearColor(0.16f, 0.16f, 0.19f, 1.f);
    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    if(phi >  1.48f) phi =  1.48f;
    if(phi < -1.48f) phi = -1.48f;

    mat4 proj, view, modelM, tmp, mvp;
    mat4_perspective(proj, 55.f * 3.14159265f / 180.f, (float)m_rw / m_rh, dist * 0.01f, dist * 50.f);
    float ex = dist * cosf(phi) * cosf(theta);
    float ey = dist * cosf(phi) * sinf(theta);
    float ez = dist * sinf(phi);
    float eye[3] = { ex, ey, ez }, center[3] = { 0, 0, 0 }, up[3] = { 0, 0, 1 };
    mat4_lookat(view, eye, center, up);
    mat4_identity(modelM);
    mat4_mul(view, modelM, tmp);
    mat4_mul(proj, tmp, mvp);

    glUseProgram(m_prog);
    glUniformMatrix4fv(glGetUniformLocation(m_prog, "uMVP"), 1, GL_FALSE, mvp);
    glUniformMatrix4fv(glGetUniformLocation(m_prog, "uModel"), 1, GL_FALSE, modelM);
    GLint alphaLoc = glGetUniformLocation(m_prog, "uAlphaMode");
    glUniform1i(glGetUniformLocation(m_prog, "uTex"), 0);
    glActiveTexture(GL_TEXTURE0);

    for(const auto &g : surfs) {
        if(!g.vao) continue;
        if(g.alphaMode == 2) { glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); glDepthMask(GL_FALSE); }
        else                 { glDisable(GL_BLEND); glDepthMask(GL_TRUE); }
        glUniform1i(alphaLoc, g.alphaMode);
        glBindTexture(GL_TEXTURE_2D, g.tex);
        glBindVertexArray(g.vao);
        glDrawElements(GL_TRIANGLES, g.indexCount, GL_UNSIGNED_INT, 0);
    }
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glFinish();
}

bool Renderer::readRGB(std::vector<uint8_t> &out)
{
    std::vector<uint8_t> big((size_t)m_rw * m_rh * 3);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, m_rw, m_rh, GL_RGB, GL_UNSIGNED_BYTE, big.data());
    if(glGetError() != GL_NO_ERROR) return false;

    // Box-downscale ss x ss and flip vertically (GL origin is bottom-left).
    out.assign((size_t)m_outW * m_outH * 3, 0);
    int ss = m_ss;
    for(int y = 0; y < m_outH; y++) {
        int srcY0 = (m_outH - 1 - y) * ss; // vertical flip
        for(int x = 0; x < m_outW; x++) {
            int srcX0 = x * ss;
            int r = 0, gg = 0, b = 0;
            for(int sy = 0; sy < ss; sy++) for(int sx = 0; sx < ss; sx++) {
                size_t si = ((size_t)(srcY0 + sy) * m_rw + (srcX0 + sx)) * 3;
                r += big[si]; gg += big[si+1]; b += big[si+2];
            }
            int n = ss * ss;
            size_t di = ((size_t)y * m_outW + x) * 3;
            out[di] = (uint8_t)(r/n); out[di+1] = (uint8_t)(gg/n); out[di+2] = (uint8_t)(b/n);
        }
    }
    return true;
}
