#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct XModel;
struct Image;
class VFS;

// Resolve textures for each surface and write a self-contained .glb.
// Returns true on success. The VFS + material pipeline are needed to
// resolve surface material names -> colorMap -> IWI -> RGBA -> JPEG.
bool write_glb(const std::string &path, const XModel &model,
               const VFS &vfs, int texMaxSize = 512);
