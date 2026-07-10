#include "material.h"
#include <cstring>

namespace {
uint32_t rd32(const std::vector<uint8_t> &d, size_t o)
{ return (o + 4 <= d.size()) ? (d[o] | (d[o+1]<<8) | (d[o+2]<<16) | ((uint32_t)d[o+3]<<24)) : 0xFFFFFFFFu; }
uint16_t rd16(const std::vector<uint8_t> &d, size_t o)
{ return (o + 2 <= d.size()) ? (uint16_t)(d[o] | (d[o+1]<<8)) : 0; }
std::string strAt(const std::vector<uint8_t> &d, uint32_t o)
{
    std::string s;
    if(o == 0xFFFFFFFFu) return s;
    for(size_t i = o; i < d.size() && d[i]; i++) s += (char)d[i];
    return s;
}
} // namespace

// Layout (v20): name_off u32 | skip 48 | tex_count u16 | skip 2 |
//               techset_off u32 | textures_off u32 | ... strings ...
// textures[]: type_off u32, flags u32, name_off u32.
bool parse_material(const std::vector<uint8_t> &d, MaterialInfo &out)
{
    if(d.size() < 64) return false;
    uint32_t nameOff = rd32(d, 0);
    uint16_t texCount = rd16(d, 52); // 4 + 48
    uint32_t techsetOff = rd32(d, 56);
    uint32_t texturesOff = rd32(d, 60);
    if(nameOff >= d.size() || texturesOff >= d.size()) return false;
    if(texCount == 0 || texCount > 64) return false;
    (void)techsetOff;

    out.name = strAt(d, nameOff);
    if(out.name.empty()) return false;

    std::string firstColor, anyMap;
    for(uint16_t i = 0; i < texCount; i++) {
        size_t rec = texturesOff + (size_t)i * 12;
        if(rec + 12 > d.size()) break;
        uint32_t typeOff = rd32(d, rec);
        uint32_t nOff    = rd32(d, rec + 8);
        std::string type = strAt(d, typeOff);
        std::string tex  = strAt(d, nOff);
        if(tex.empty()) continue;
        if(anyMap.empty()) anyMap = tex;
        if(type == "colorMap" || type == "Diffuse_MapSampler") { firstColor = tex; break; }
    }
    out.colorMap = firstColor.empty() ? anyMap : firstColor;
    return !out.colorMap.empty();
}
