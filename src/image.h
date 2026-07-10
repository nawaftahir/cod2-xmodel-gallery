// Texture decoding to RGBA8.
//
// Primary path is CoD2's IWI v5 (DXT1/3/5, the format model color maps ship in);
// DDS and TGA are handled too so loose/override textures also resolve.
#pragma once
#include <cstdint>
#include <vector>

struct Image {
    int w = 0, h = 0;
    std::vector<uint8_t> rgba; // w*h*4, row 0 = top
    bool ok() const { return w > 0 && h > 0 && rgba.size() == (size_t)w * h * 4; }
    bool hasAlpha() const;     // any texel with alpha != 255
};

// Sniff by magic and decode. Empty Image on failure.
Image decode_texture(const std::vector<uint8_t> &data);

Image decode_iwi(const std::vector<uint8_t> &data);
Image decode_dds(const std::vector<uint8_t> &data);
Image decode_tga(const std::vector<uint8_t> &data);
