// Offscreen model renderer: uploads an XModel to GPU buffers, resolves its
// textures through the material -> IWI pipeline, and renders an orbit view into
// a supersampled FBO that is downscaled on readback for clean thumbnails.
#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include "gl.h"

class VFS;
struct XModel;

struct GpuSurf {
    GLuint vao = 0, vbo = 0, ibo = 0;
    int    indexCount = 0;
    GLuint tex = 0;
    int    alphaMode = 0; // 0 opaque, 1 cutout, 2 blend
};

class Renderer {
public:
    // outW/outH = final image size; supersample renders at ss x that internally.
    bool init(int outW, int outH, int supersample = 2);
    void shutdown();

    std::vector<GpuSurf> upload(const XModel &model, const VFS &vfs);
    void freeSurfs(std::vector<GpuSurf> &surfs);
    void clearTextureCache();

    // theta = azimuth, phi = elevation (radians), dist = camera distance.
    void render(const std::vector<GpuSurf> &surfs, float theta, float phi, float dist);

    // Downscaled, vertically-corrected RGB (outW*outH*3).
    bool readRGB(std::vector<uint8_t> &rgb);

    int outWidth() const { return m_outW; }
    int outHeight() const { return m_outH; }

private:
    GLuint m_prog = 0, m_fbo = 0, m_color = 0, m_depth = 0, m_white = 0;
    int m_outW = 0, m_outH = 0, m_ss = 1, m_rw = 0, m_rh = 0;
    std::unordered_map<std::string, GLuint> m_texCache;

    GLuint loadTexture(const std::string &material, const VFS &vfs, int &alphaMode);
};
