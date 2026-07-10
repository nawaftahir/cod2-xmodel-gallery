#include "xmodel.h"
#include "vfs.h"
#include "math.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace {

constexpr uint16_t XMODEL_V20 = 0x14;
constexpr int      RIGGED     = 65535;
constexpr int      MAX_BONES  = 256;
constexpr int      MAX_LODS   = 4;

// Sequential little-endian reader with bounds clamping.
struct Reader {
    const uint8_t *buf; int pos, size;
    Reader(const std::vector<uint8_t> &d) : buf(d.data()), pos(0), size((int)d.size()) {}
    uint8_t  u8()  { return pos < size ? buf[pos++] : 0; }
    int8_t   s8()  { return (int8_t)u8(); }
    uint16_t u16() { if(pos + 2 > size) { pos = size; return 0; } uint16_t v = buf[pos] | (buf[pos+1]<<8); pos += 2; return v; }
    int16_t  s16() { return (int16_t)u16(); }
    uint32_t u32() { if(pos + 4 > size) { pos = size; return 0; } uint32_t v = buf[pos]|(buf[pos+1]<<8)|(buf[pos+2]<<16)|((uint32_t)buf[pos+3]<<24); pos += 4; return v; }
    float    f32() { union { uint32_t i; float f; } u; u.i = u32(); return u.f; }
    void     skip(int n) { pos += n; if(pos > size) pos = size; if(pos < 0) pos = size; }
    bool     eof() const { return pos >= size; }
    std::string str() { std::string s; uint8_t c; while((c = u8()) != 0) s += (char)c; return s; }
    void vec3(float *o) { o[0]=f32(); o[1]=f32(); o[2]=f32(); }
    void quat3(float *q) { // 3x s16 / 32768 -> x,y,z; w reconstructed
        float x = s16()/32768.f, y = s16()/32768.f, z = s16()/32768.f;
        float ww = 1.f - x*x - y*y - z*z;
        q[0] = (ww > 0.f) ? sqrtf(ww) : 0.f; q[1] = x; q[2] = y; q[3] = z;
    }
};

struct Bone {
    int   parent;
    float lTrans[3], lRot[4];
    float wTrans[3], wRot[4];
};

// xmodelparts/<lod>: skeleton. v20 == v14 layout minus the 24-byte trailer per
// bone name.
bool loadParts(const VFS &vfs, const std::string &lod, Bone *bones, int &numBones)
{
    auto data = vfs.read("xmodelparts/" + lod);
    if(data.empty()) { fprintf(stderr, "xmodelparts/%s: not found\n", lod.c_str()); return false; }
    Reader r(data);
    if(r.u16() != XMODEL_V20) return false;

    int boneCount = r.u16(), rootCount = r.u16();
    numBones = boneCount + rootCount;
    if(numBones <= 0 || numBones > MAX_BONES) return false;

    for(int i = 0; i < numBones; i++) {
        bones[i].parent = -1;
        bones[i].lRot[0] = 1.f; bones[i].lRot[1] = bones[i].lRot[2] = bones[i].lRot[3] = 0.f;
        memset(bones[i].lTrans, 0, 12);
        bones[i].wRot[0] = 1.f; bones[i].wRot[1] = bones[i].wRot[2] = bones[i].wRot[3] = 0.f;
        memset(bones[i].wTrans, 0, 12);
    }
    // Non-root bones occupy [rootCount, numBones).
    for(int i = 0; i < boneCount; i++) {
        int idx = rootCount + i;
        bones[idx].parent = (int)r.s8();
        r.vec3(bones[idx].lTrans);
        r.quat3(bones[idx].lRot);
    }
    // Names, in full bone order. v20 has no 24-byte partbits trailer (v14 did).
    for(int i = 0; i < numBones; i++) r.str();

    // World transforms; parents always precede children in the array.
    for(int i = 0; i < numBones; i++) {
        int p = bones[i].parent;
        if(p < 0 || p >= numBones) {
            memcpy(bones[i].wTrans, bones[i].lTrans, 12);
            memcpy(bones[i].wRot,   bones[i].lRot,   16);
        } else {
            quat_rotvec(bones[p].wRot, bones[i].lTrans, bones[i].wTrans);
            bones[i].wTrans[0] += bones[p].wTrans[0];
            bones[i].wTrans[1] += bones[p].wTrans[1];
            bones[i].wTrans[2] += bones[p].wTrans[2];
            quat_mul(bones[p].wRot, bones[i].lRot, bones[i].wRot);
        }
    }
    return true;
}

// xmodelsurfs/<lod>: geometry. Plain indexed triangles (unlike v14's fan/strip).
bool loadSurfs(const VFS &vfs, const std::string &lod, const Bone *bones, int numBones,
               const std::vector<std::string> &matNames, XModel &model)
{
    auto data = vfs.read("xmodelsurfs/" + lod);
    if(data.empty()) { fprintf(stderr, "xmodelsurfs/%s: not found\n", lod.c_str()); return false; }
    Reader r(data);
    if(r.u16() != XMODEL_V20) return false;
    int numSurfs = r.u16();
    if(numSurfs <= 0 || numSurfs > 4096) return false;

    for(int s = 0; s < numSurfs; s++) {
        r.skip(1);
        int vertCount = r.u16();
        int triCount  = r.u16();
        int ogBone    = r.u16();
        bool rigged   = (ogBone == RIGGED);
        int defBone   = rigged ? 0 : ogBone;
        if(rigged) r.skip(2);

        XSurface surf;
        if(!matNames.empty())
            surf.material = matNames[std::min<int>(s, (int)matNames.size() - 1)];
        surf.verts.reserve(vertCount);

        for(int j = 0; j < vertCount; j++) {
            float normal[3]; r.vec3(normal);
            r.skip(4);                 // vertex color (RGBA8), unused
            float u = r.f32(), v = r.f32();
            r.skip(24);                // binormal(12) + tangent(12)

            int weightCount = 0, bone = defBone;
            if(rigged) { weightCount = r.u8(); bone = r.u16(); }

            float lp[3]; r.vec3(lp);

            if(weightCount > 0) {
                r.skip(1);
                for(int w = 0; w < weightCount; w++) { r.u16(); r.skip(12); r.u16(); } // bone, offset, influence
            }

            float wp[3], wn[3];
            if(bone >= 0 && bone < numBones) {
                quat_rotvec(bones[bone].wRot, lp, wp);
                wp[0] += bones[bone].wTrans[0]; wp[1] += bones[bone].wTrans[1]; wp[2] += bones[bone].wTrans[2];
                quat_rotvec(bones[bone].wRot, normal, wn);
            } else {
                memcpy(wp, lp, 12); memcpy(wn, normal, 12);
            }
            float nl = v3len(wn); if(nl > 1e-6f) { wn[0]/=nl; wn[1]/=nl; wn[2]/=nl; }

            surf.verts.push_back(XVertex{ wp[0], wp[1], wp[2], wn[0], wn[1], wn[2], u, v });
        }

        surf.indices.reserve((size_t)triCount * 3);
        for(int t = 0; t < triCount; t++) {
            uint16_t a = r.u16(), b = r.u16(), c = r.u16();
            surf.indices.push_back(a); surf.indices.push_back(c); surf.indices.push_back(b); // winding fix
        }

        model.triangleCount += triCount;
        model.surfaces.push_back(std::move(surf));
        if(r.eof() && s + 1 < numSurfs) { fprintf(stderr, "xmodelsurfs/%s: truncated at surf %d\n", lod.c_str(), s); break; }
    }
    return !model.surfaces.empty();
}

} // namespace

bool load_xmodel(const VFS &vfs, const std::string &name, XModel &model)
{
    std::string path = (name.rfind("xmodel/", 0) == 0) ? name : "xmodel/" + name;
    auto data = vfs.read(path);
    if(data.empty()) { fprintf(stderr, "xmodel not found: %s\n", path.c_str()); return false; }

    Reader r(data);
    if(r.u16() != XMODEL_V20) { fprintf(stderr, "%s: not a v20 xmodel\n", path.c_str()); return false; }
    r.skip(25); // flags + bounds, unused (we compute bounds from geometry)

    std::string lodNames[MAX_LODS];
    int numLods = 0;
    for(int i = 0; i < MAX_LODS; i++) {
        r.f32();                       // lod distance
        std::string n = r.str();
        if(!n.empty() && numLods < MAX_LODS) lodNames[numLods++] = n;
    }
    if(numLods == 0) { fprintf(stderr, "%s: no LODs\n", path.c_str()); return false; }

    r.skip(4);
    uint32_t unkCount = r.u32();
    for(uint32_t i = 0; i < unkCount && i < 65536; i++) {
        uint32_t sub = r.u32();
        long long adv = (long long)sub * 48 + 36;       // widen: avoid u32 wrap / int overflow
        r.skip(adv > r.size ? r.size : (int)adv);
    }

    // Material name list per LOD; keep LOD 0's.
    std::vector<std::string> lod0Materials;
    for(int i = 0; i < numLods; i++) {
        int mc = r.u16();
        for(int j = 0; j < mc; j++) {
            std::string mn = r.str();
            if(i == 0) lod0Materials.push_back(mn);
        }
    }

    model.name = name;
    Bone bones[MAX_BONES]; int numBones = 0;
    if(!loadParts(vfs, lodNames[0], bones, numBones)) return false;
    if(!loadSurfs(vfs, lodNames[0], bones, numBones, lod0Materials, model)) return false;

    size_t totalVerts = 0;
    for(auto &s : model.surfaces) totalVerts += s.verts.size();
    if(totalVerts == 0) { fprintf(stderr, "%s: no geometry\n", path.c_str()); return false; }

    // Bounds from geometry, then recenter to origin.
    float mn[3] = {1e30f,1e30f,1e30f}, mx[3] = {-1e30f,-1e30f,-1e30f};
    for(auto &s : model.surfaces) for(auto &v : s.verts) {
        mn[0]=std::min(mn[0],v.x); mn[1]=std::min(mn[1],v.y); mn[2]=std::min(mn[2],v.z);
        mx[0]=std::max(mx[0],v.x); mx[1]=std::max(mx[1],v.y); mx[2]=std::max(mx[2],v.z);
    }
    for(int i = 0; i < 3; i++) model.center[i] = (mn[i] + mx[i]) * 0.5f;
    float d[3]; v3sub(mx, mn, d);
    model.radius = v3len(d) * 0.5f;
    if(model.radius < 0.1f) model.radius = 10.f;

    for(auto &s : model.surfaces) for(auto &v : s.verts) {
        v.x -= model.center[0]; v.y -= model.center[1]; v.z -= model.center[2];
    }
    return true;
}
