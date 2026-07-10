// CoD2 (v20) material files: a small header of file offsets pointing at
// null-terminated strings for the material name, techset, and each texture
// map (type string + .iwi name). We only need the colorMap for a gallery.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MaterialInfo {
    std::string name;
    std::string colorMap; // texture name to load from images/<colorMap>.iwi
};

// Parse a materials/<name> blob. Returns false if it isn't a plausible v20 material.
bool parse_material(const std::vector<uint8_t> &data, MaterialInfo &out);
