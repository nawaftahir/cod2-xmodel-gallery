// CoD2 XModel (format v20) loader: descriptor -> skeleton -> surfaces.
//
// A model is described by xmodel/<name>, which names up to four LOD parts.
// For LOD 0 we read the skeleton (xmodelparts/<lod>) and the mesh
// (xmodelsurfs/<lod>), baking each vertex into world space via its primary
// bone so rigid and skinned models both render in bind pose.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

class VFS;

struct XVertex { float x, y, z, nx, ny, nz, u, v; };

struct XSurface {
    std::vector<XVertex>  verts;
    std::vector<uint32_t> indices;
    std::string           material; // material asset name (resolve via materials/<name>)
};

struct XModel {
    std::string          name;
    std::vector<XSurface> surfaces;
    float center[3] = {0,0,0};
    float radius = 0.f;
    long  triangleCount = 0;
};

// Load xmodel/<name> (name may include or omit the "xmodel/" prefix).
bool load_xmodel(const VFS &vfs, const std::string &name, XModel &out);
