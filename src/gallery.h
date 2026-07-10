// Image output (JPEG/PNG via stb) and the static searchable gallery page.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct GalleryItem {
    std::string model;    // raw asset name (authoritative id, used for search)
    std::string file;     // image filename relative to the gallery dir
    std::string pretty;   // human-readable title
    std::string category; // type category (Weapons, Vehicles, ...)
    std::string theme;    // map/theme tag, may be empty
    int triangles = 0;
    int surfaces = 0;
};

// Write top-down RGB pixels. Chooses JPEG/PNG by the path extension.
bool write_image(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h, int jpegQuality);

// Emit index.html with a client-side search box over the rendered thumbnails.
void write_gallery(const std::string &outdir, const std::vector<GalleryItem> &items);
