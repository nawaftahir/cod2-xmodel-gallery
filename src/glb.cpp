#include "glb.h"
#include "xmodel.h"
#include "image.h"
#include "material.h"
#include "vfs.h"
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cmath>

#include "../third_party/stb_image_write.h"

static std::string escape_json(const std::string &s) {
    std::string o;
    o.reserve(s.size() + 16);
    for (char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\b') o += "\\b";
        else if (c == '\f') o += "\\f";
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if ((uint8_t)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", c);
            o += buf;
        } else {
            o += c;
        }
    }
    return o;
}

static std::string fmt_float(float f) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%f", f);
    return buf;
}

static void stbi_write_to_vec(void *context, void *data, int size) {
    auto *vec = static_cast<std::vector<uint8_t>*>(context);
    const uint8_t *b = static_cast<const uint8_t*>(data);
    vec->insert(vec->end(), b, b + size);
}

struct TextureDef {
    std::string key;
    std::vector<uint8_t> jpegData;
};

struct PrimData {
    int surfIndex;
    uint32_t numIndices;
    uint32_t numVerts;
    size_t indicesByteOffset;
    size_t posByteOffset;
    size_t normByteOffset;
    size_t uvByteOffset;
    float minPos[3];
    float maxPos[3];
    int materialIndex;
    bool isU32;
};

bool write_glb(const std::string &path, const XModel &model, const VFS &vfs, int texMaxSize) {
    if (model.surfaces.empty()) return false;
    
    std::vector<PrimData> prims;
    std::vector<std::string> matNames;
    
    for (size_t i = 0; i < model.surfaces.size(); ++i) {
        const auto &s = model.surfaces[i];
        if (s.verts.empty() || s.indices.empty()) continue;
        
        PrimData p = {};
        p.surfIndex = (int)i;
        p.numIndices = (uint32_t)s.indices.size();
        p.numVerts = (uint32_t)s.verts.size();
        p.isU32 = (p.numVerts >= 65536);
        
        p.minPos[0] = p.minPos[1] = p.minPos[2] = 1e30f;
        p.maxPos[0] = p.maxPos[1] = p.maxPos[2] = -1e30f;
        for (const auto &v : s.verts) {
            if (v.x < p.minPos[0]) p.minPos[0] = v.x;
            if (v.y < p.minPos[1]) p.minPos[1] = v.y;
            if (v.z < p.minPos[2]) p.minPos[2] = v.z;
            if (v.x > p.maxPos[0]) p.maxPos[0] = v.x;
            if (v.y > p.maxPos[1]) p.maxPos[1] = v.y;
            if (v.z > p.maxPos[2]) p.maxPos[2] = v.z;
        }
        
        p.materialIndex = -1;
        if (!s.material.empty()) {
            std::string matKey = VFS::lower(s.material);
            auto it = std::find(matNames.begin(), matNames.end(), matKey);
            if (it != matNames.end()) {
                p.materialIndex = (int)std::distance(matNames.begin(), it);
            } else {
                p.materialIndex = (int)matNames.size();
                matNames.push_back(matKey);
            }
        }
        prims.push_back(p);
    }
    
    if (prims.empty()) return false;
    
    std::vector<TextureDef> textures;
    std::vector<int> matToTex(matNames.size(), -1);
    
    for (size_t i = 0; i < matNames.size(); ++i) {
        std::string matKey = matNames[i];
        std::string texName;
        auto matData = vfs.read("materials/" + matKey);
        if (!matData.empty()) {
            MaterialInfo mi;
            if (parse_material(matData, mi)) texName = mi.colorMap;
        }
        if (texName.empty()) texName = matKey;
        
        std::string texKey = VFS::lower(texName);
        auto texIt = std::find_if(textures.begin(), textures.end(), [&](const TextureDef &t){ return t.key == texKey; });
        if (texIt != textures.end()) {
            matToTex[i] = (int)std::distance(textures.begin(), texIt);
            continue;
        }
        
        Image img;
        const char *exts[] = { "", ".iwi", ".dds", ".tga" };
        for (const char *e : exts) {
            auto d = vfs.read("images/" + texKey + e);
            if (d.empty()) continue;
            img = decode_texture(d);
            if (img.ok()) break;
        }
        
        if (img.ok()) {
            while (img.w > texMaxSize || img.h > texMaxSize) {
                int nw = std::max(1, img.w / 2);
                int nh = std::max(1, img.h / 2);
                std::vector<uint8_t> out(nw * nh * 4);
                for (int y = 0; y < nh; ++y) {
                    for (int x = 0; x < nw; ++x) {
                        int r = 0, g = 0, b = 0, a = 0;
                        for (int sy = 0; sy < 2; ++sy) {
                            for (int sx = 0; sx < 2; ++sx) {
                                int srcx = std::min(x * 2 + sx, img.w - 1);
                                int srcy = std::min(y * 2 + sy, img.h - 1);
                                int srcidx = (srcy * img.w + srcx) * 4;
                                r += img.rgba[srcidx];
                                g += img.rgba[srcidx + 1];
                                b += img.rgba[srcidx + 2];
                                a += img.rgba[srcidx + 3];
                            }
                        }
                        int dstidx = (y * nw + x) * 4;
                        out[dstidx] = r / 4;
                        out[dstidx + 1] = g / 4;
                        out[dstidx + 2] = b / 4;
                        out[dstidx + 3] = a / 4;
                    }
                }
                img.w = nw; img.h = nh;
                img.rgba = std::move(out);
            }
            
            TextureDef tdef;
            tdef.key = texKey;
            stbi_write_jpg_to_func(stbi_write_to_vec, &tdef.jpegData, img.w, img.h, 4, img.rgba.data(), 85);
            matToTex[i] = (int)textures.size();
            textures.push_back(std::move(tdef));
        }
    }
    
    std::vector<uint8_t> bin;
    auto add_pad = [&]() {
        while (bin.size() % 4 != 0) bin.push_back(0);
    };
    
    add_pad();
    size_t indices_bv_offset = bin.size();
    for (auto &p : prims) {
        p.indicesByteOffset = bin.size() - indices_bv_offset;
        const auto &s = model.surfaces[p.surfIndex];
        if (p.isU32) {
            for (uint32_t idx : s.indices) {
                uint32_t v = idx;
                bin.insert(bin.end(), (uint8_t*)&v, (uint8_t*)&v + 4);
            }
        } else {
            for (uint32_t idx : s.indices) {
                uint16_t v = (uint16_t)idx;
                bin.insert(bin.end(), (uint8_t*)&v, (uint8_t*)&v + 2);
            }
        }
        add_pad();
    }
    size_t indices_bv_length = bin.size() - indices_bv_offset;
    
    add_pad();
    size_t pos_bv_offset = bin.size();
    for (auto &p : prims) {
        p.posByteOffset = bin.size() - pos_bv_offset;
        const auto &s = model.surfaces[p.surfIndex];
        for (const auto &v : s.verts) {
            float pos[3] = {v.x, v.y, v.z};
            bin.insert(bin.end(), (uint8_t*)pos, (uint8_t*)pos + 12);
        }
    }
    size_t pos_bv_length = bin.size() - pos_bv_offset;
    
    add_pad();
    size_t norm_bv_offset = bin.size();
    for (auto &p : prims) {
        p.normByteOffset = bin.size() - norm_bv_offset;
        const auto &s = model.surfaces[p.surfIndex];
        for (const auto &v : s.verts) {
            float norm[3] = {v.nx, v.ny, v.nz};
            bin.insert(bin.end(), (uint8_t*)norm, (uint8_t*)norm + 12);
        }
    }
    size_t norm_bv_length = bin.size() - norm_bv_offset;
    
    add_pad();
    size_t uv_bv_offset = bin.size();
    for (auto &p : prims) {
        p.uvByteOffset = bin.size() - uv_bv_offset;
        const auto &s = model.surfaces[p.surfIndex];
        for (const auto &v : s.verts) {
            float uv[2] = {v.u, v.v};
            bin.insert(bin.end(), (uint8_t*)uv, (uint8_t*)uv + 8);
        }
    }
    size_t uv_bv_length = bin.size() - uv_bv_offset;
    
    struct ImgBV { size_t offset, length; };
    std::vector<ImgBV> img_bvs;
    for (const auto &t : textures) {
        add_pad();
        size_t off = bin.size();
        bin.insert(bin.end(), t.jpegData.begin(), t.jpegData.end());
        img_bvs.push_back({off, bin.size() - off});
    }
    add_pad();
    
    std::string json = "{";
    json += "\"asset\":{\"version\":\"2.0\",\"generator\":\"cod2-xmodel-gallery\"},";
    json += "\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],";
    
    json += "\"meshes\":[{\"primitives\":[";
    for (size_t i = 0; i < prims.size(); ++i) {
        if (i > 0) json += ",";
        json += "{\"attributes\":{";
        int acc_idx = (int)i;
        int acc_pos = (int)(prims.size() + i);
        int acc_norm = (int)(prims.size() * 2 + i);
        int acc_uv = (int)(prims.size() * 3 + i);
        json += "\"POSITION\":" + std::to_string(acc_pos) + ",";
        json += "\"NORMAL\":" + std::to_string(acc_norm) + ",";
        json += "\"TEXCOORD_0\":" + std::to_string(acc_uv);
        json += "},\"indices\":" + std::to_string(acc_idx);
        if (prims[i].materialIndex >= 0) {
            json += ",\"material\":" + std::to_string(prims[i].materialIndex);
        }
        json += "}";
    }
    json += "]}],";
    
    if (!matNames.empty()) {
        json += "\"materials\":[";
        for (size_t i = 0; i < matNames.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"pbrMetallicRoughness\":{\"metallicFactor\":0.0,\"roughnessFactor\":1.0";
            if (matToTex[i] >= 0) {
                json += ",\"baseColorTexture\":{\"index\":" + std::to_string(matToTex[i]) + "}";
            }
            json += "},\"name\":\"" + escape_json(matNames[i]) + "\"}";
        }
        json += "],";
    }
    
    if (!textures.empty()) {
        json += "\"textures\":[";
        for (size_t i = 0; i < textures.size(); ++i) {
            if (i > 0) json += ",";
            json += "{\"sampler\":0,\"source\":" + std::to_string(i) + "}";
        }
        json += "],\"images\":[";
        for (size_t i = 0; i < textures.size(); ++i) {
            if (i > 0) json += ",";
            int bv_idx = 4 + (int)i;
            json += "{\"bufferView\":" + std::to_string(bv_idx) + ",\"mimeType\":\"image/jpeg\"}";
        }
        json += "],\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],";
    }
    
    json += "\"bufferViews\":[";
    auto add_bv = [&](size_t offset, size_t len, int target) {
        std::string s = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(len);
        if (target) s += ",\"target\":" + std::to_string(target);
        s += "}";
        return s;
    };
    json += add_bv(indices_bv_offset, indices_bv_length, 34963) + ",";
    json += add_bv(pos_bv_offset, pos_bv_length, 34962) + ",";
    json += add_bv(norm_bv_offset, norm_bv_length, 34962) + ",";
    json += add_bv(uv_bv_offset, uv_bv_length, 34962);
    for (const auto &ibv : img_bvs) {
        json += "," + add_bv(ibv.offset, ibv.length, 0);
    }
    json += "],";
    
    json += "\"accessors\":[";
    bool first_acc = true;
    auto add_acc = [&](int bv, size_t off, int type, size_t count, const char *t, const float *minp, const float *maxp) {
        if (!first_acc) json += ",";
        first_acc = false;
        json += "{\"bufferView\":" + std::to_string(bv) + ",\"byteOffset\":" + std::to_string(off);
        json += ",\"componentType\":" + std::to_string(type) + ",\"count\":" + std::to_string(count);
        json += ",\"type\":\"" + std::string(t) + "\"";
        if (minp && maxp) {
            json += ",\"min\":[" + fmt_float(minp[0]) + "," + fmt_float(minp[1]) + "," + fmt_float(minp[2]) + "],";
            json += "\"max\":[" + fmt_float(maxp[0]) + "," + fmt_float(maxp[1]) + "," + fmt_float(maxp[2]) + "]";
        }
        json += "}";
    };
    
    for (const auto &p : prims) add_acc(0, p.indicesByteOffset, p.isU32 ? 5125 : 5123, p.numIndices, "SCALAR", nullptr, nullptr);
    for (const auto &p : prims) add_acc(1, p.posByteOffset, 5126, p.numVerts, "VEC3", p.minPos, p.maxPos);
    for (const auto &p : prims) add_acc(2, p.normByteOffset, 5126, p.numVerts, "VEC3", nullptr, nullptr);
    for (const auto &p : prims) add_acc(3, p.uvByteOffset, 5126, p.numVerts, "VEC2", nullptr, nullptr);
    json += "],";
    
    json += "\"buffers\":[{\"byteLength\":" + std::to_string(bin.size()) + "}]}";
    
    while (json.size() % 4 != 0) json += " ";
    
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    
    uint32_t magic = 0x46546C67;
    uint32_t version = 2;
    uint32_t length = 12 + 8 + (uint32_t)json.size() + 8 + (uint32_t)bin.size();
    
    out.write((char*)&magic, 4);
    out.write((char*)&version, 4);
    out.write((char*)&length, 4);
    
    uint32_t jsonLen = (uint32_t)json.size();
    uint32_t jsonType = 0x4E4F534A;
    out.write((char*)&jsonLen, 4);
    out.write((char*)&jsonType, 4);
    out.write(json.data(), json.size());
    
    uint32_t binLen = (uint32_t)bin.size();
    uint32_t binType = 0x004E4942;
    out.write((char*)&binLen, 4);
    out.write((char*)&binType, 4);
    if (binLen > 0) out.write((char*)bin.data(), bin.size());
    
    return true;
}
