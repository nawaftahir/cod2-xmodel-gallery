#include "image.h"
#include <cstring>
#include <cstdio>

bool Image::hasAlpha() const
{
    for(size_t i = 0; i < rgba.size(); i += 4)
        if(rgba[i + 3] != 255) return true;
    return false;
}

// ---- DXT block decoders ---------------------------------------------------
namespace {

struct Color8 { uint8_t r, g, b, a; };

Color8 rgb565(uint16_t c)
{
    return { (uint8_t)((c >> 11) << 3), (uint8_t)(((c >> 5) & 63) << 2), (uint8_t)((c & 31) << 3), 255 };
}
Color8 lerp2(Color8 a, Color8 b)
{ return { (uint8_t)((2*a.r+b.r)/3), (uint8_t)((2*a.g+b.g)/3), (uint8_t)((2*a.b+b.b)/3), 255 }; }
Color8 lerp1(Color8 a, Color8 b)
{ return { (uint8_t)((a.r+b.r)/2), (uint8_t)((a.g+b.g)/2), (uint8_t)((a.b+b.b)/2), 255 }; }

void decode_dxt1_block(const uint8_t *src, Color8 out[16], bool punchAlpha)
{
    uint16_t c0 = src[0] | (src[1] << 8);
    uint16_t c1 = src[2] | (src[3] << 8);
    uint32_t lu = src[4] | (src[5] << 8) | (src[6] << 16) | ((uint32_t)src[7] << 24);
    Color8 pal[4];
    pal[0] = rgb565(c0); pal[1] = rgb565(c1);
    if(c0 > c1) {
        pal[2] = lerp2(pal[0], pal[1]); pal[3] = lerp2(pal[1], pal[0]);
    } else {
        pal[2] = lerp1(pal[0], pal[1]);
        pal[3] = punchAlpha ? Color8{0,0,0,0} : pal[2];
    }
    for(int i = 0; i < 16; i++) out[i] = pal[(lu >> (i * 2)) & 3];
}

void decode_dxt3_block(const uint8_t *src, Color8 out[16])
{
    Color8 rgb[16];
    decode_dxt1_block(src + 8, rgb, false);
    for(int i = 0; i < 16; i++) {
        uint8_t a = (i & 1) ? (src[i/2] >> 4) & 0xf : src[i/2] & 0xf;
        out[i] = { rgb[i].r, rgb[i].g, rgb[i].b, (uint8_t)(a * 17) };
    }
}

void decode_dxt5_block(const uint8_t *src, Color8 out[16])
{
    Color8 rgb[16];
    decode_dxt1_block(src + 8, rgb, false);
    uint8_t a0 = src[0], a1 = src[1], apal[8];
    apal[0] = a0; apal[1] = a1;
    if(a0 > a1) { for(int i = 2; i < 8; i++) apal[i] = (uint8_t)(((8-i)*a0 + (i-1)*a1) / 7); }
    else { for(int i = 2; i < 6; i++) apal[i] = (uint8_t)(((6-i)*a0 + (i-1)*a1) / 5); apal[6] = 0; apal[7] = 255; }
    uint64_t bits = 0;
    for(int i = 0; i < 6; i++) bits |= ((uint64_t)src[2 + i]) << (i * 8);
    for(int i = 0; i < 16; i++) {
        uint8_t idx = (bits >> (i * 3)) & 7;
        out[i] = { rgb[i].r, rgb[i].g, rgb[i].b, apal[idx] };
    }
}

// Decode a DXT surface (fmt: 1=DXT1, 3=DXT3, 5=DXT5) into an Image.
Image decode_dxt(const uint8_t *src, size_t srcLen, int w, int h, int fmt)
{
    Image img;
    int bw = (w + 3) / 4, bh = (h + 3) / 4;
    size_t blockBytes = (fmt == 1) ? 8 : 16;
    if(srcLen < (size_t)bw * bh * blockBytes) return img; // truncated
    img.w = w; img.h = h;
    img.rgba.assign((size_t)w * h * 4, 255);
    for(int by = 0; by < bh; by++) for(int bx = 0; bx < bw; bx++) {
        Color8 block[16];
        if(fmt == 1)      decode_dxt1_block(src, block, true);
        else if(fmt == 3) decode_dxt3_block(src, block);
        else              decode_dxt5_block(src, block);
        src += blockBytes;
        for(int py = 0; py < 4; py++) for(int px = 0; px < 4; px++) {
            int ix = bx*4 + px, iy = by*4 + py;
            if(ix >= w || iy >= h) continue;
            Color8 &c = block[py*4 + px];
            size_t idx = ((size_t)iy * w + ix) * 4;
            img.rgba[idx] = c.r; img.rgba[idx+1] = c.g; img.rgba[idx+2] = c.b; img.rgba[idx+3] = c.a;
        }
    }
    return img;
}

} // namespace

// ---- IWI (CoD2 v5) --------------------------------------------------------
// Header: "IWi"(3) ver(1) | format(1) usage(1) width(2) height(2) depth(2)
//         | 4 mip offsets (u32). The largest inter-offset span is the full-res
//         mip; decode that one. Validated against stock DXT1/DXT5 color maps.
Image decode_iwi(const std::vector<uint8_t> &d)
{
    Image img;
    if(d.size() < 32 || memcmp(d.data(), "IWi", 3) != 0) return img;
    uint8_t version = d[3];
    if(version != 0x05 && version != 0x06) return img; // CoD2 (5); 6 shares the layout
    uint8_t format = d[4];
    auto r16 = [&](size_t o){ return (uint16_t)(d[o] | (d[o+1] << 8)); };
    auto r32 = [&](size_t o){ return (uint32_t)(d[o] | (d[o+1]<<8) | (d[o+2]<<16) | ((uint32_t)d[o+3]<<24)); };
    int w = r16(6), h = r16(8);
    if(w <= 0 || h <= 0 || w > 16384 || h > 16384) return img;

    // Four mipmap end-offsets follow the 12-byte header.
    uint32_t off[4];
    for(int i = 0; i < 4; i++) off[i] = r32(12 + i * 4);
    size_t headerEnd = 28;             // first mip data begins here
    size_t fileSize  = d.size();

    // Reconstruct [offset,size) spans; the biggest span is the base (full-res) mip.
    struct Span { size_t off, size; } spans[4];
    for(int i = 0; i < 4; i++) {
        if(i == 0)      spans[i] = { off[0], fileSize > off[0] ? fileSize - off[0] : 0 };
        else if(i == 3) spans[i] = { headerEnd, off[3] > headerEnd ? off[3] - headerEnd : 0 };
        else            spans[i] = { off[i], off[i-1] > off[i] ? off[i-1] - off[i] : 0 };
    }
    int best = 0;
    for(int i = 1; i < 4; i++) if(spans[i].size > spans[best].size) best = i;
    if(spans[best].size == 0 || spans[best].off + spans[best].size > fileSize) return img;

    const uint8_t *src = d.data() + spans[best].off;
    size_t srcLen = spans[best].size;

    switch(format) {
        case 0x0b: return decode_dxt(src, srcLen, w, h, 1); // DXT1
        case 0x0c: return decode_dxt(src, srcLen, w, h, 3); // DXT3
        case 0x0d: return decode_dxt(src, srcLen, w, h, 5); // DXT5
        case 0x01: { // ARGB32 (stored BGRA)
            if(srcLen < (size_t)w*h*4) return img;
            img.w = w; img.h = h; img.rgba.resize((size_t)w*h*4);
            for(size_t i = 0; i < (size_t)w*h; i++) {
                img.rgba[i*4+0] = src[i*4+2]; img.rgba[i*4+1] = src[i*4+1];
                img.rgba[i*4+2] = src[i*4+0]; img.rgba[i*4+3] = src[i*4+3];
            }
            return img;
        }
        case 0x02: { // RGB24 (stored BGR)
            if(srcLen < (size_t)w*h*3) return img;
            img.w = w; img.h = h; img.rgba.assign((size_t)w*h*4, 255);
            for(size_t i = 0; i < (size_t)w*h; i++) {
                img.rgba[i*4+0] = src[i*3+2]; img.rgba[i*4+1] = src[i*3+1]; img.rgba[i*4+2] = src[i*3+0];
            }
            return img;
        }
        default: return img; // unsupported (e.g. GA16/A8) -> caller falls back
    }
}

// ---- DDS (DXT1/3/5 + uncompressed RGBA) -----------------------------------
Image decode_dds(const std::vector<uint8_t> &data)
{
    Image img;
    if(data.size() < 128 || memcmp(data.data(), "DDS ", 4) != 0) return img;
    const uint8_t *d = data.data() + 4;
    auto r32 = [&](int off){ uint32_t v; memcpy(&v, d + off, 4); return v; };
    int h = (int)r32(8), w = (int)r32(12);
    uint32_t pfFlags = r32(76), fourCC = r32(80);
    if(w <= 0 || h <= 0) return img;
    const uint8_t *src = data.data() + 128;
    size_t srcLen = data.size() - 128;

    if(pfFlags & 4) { // compressed
        if(fourCC == 0x31545844) return decode_dxt(src, srcLen, w, h, 1);
        if(fourCC == 0x33545844) return decode_dxt(src, srcLen, w, h, 3);
        if(fourCC == 0x35545844) return decode_dxt(src, srcLen, w, h, 5);
        return img;
    }
    if(pfFlags & 0x41) { // RGB/RGBA
        uint32_t bpp = r32(88) / 8;
        uint32_t rmask=r32(92), gmask=r32(96), bmask=r32(100), amask=r32(104);
        auto shift = [](uint32_t m){ int s=0; if(!m) return 0; while(!(m&1)){m>>=1;s++;} return s; };
        int rs=shift(rmask), gs=shift(gmask), bs=shift(bmask), as=shift(amask);
        if(srcLen < (size_t)w*h*bpp) return img;
        img.w = w; img.h = h; img.rgba.assign((size_t)w*h*4, 255);
        for(size_t i = 0; i < (size_t)w*h; i++) {
            uint32_t pix = 0; memcpy(&pix, src + i*bpp, bpp < 4 ? bpp : 4);
            img.rgba[i*4+0] = (uint8_t)((pix & rmask) >> rs);
            img.rgba[i*4+1] = (uint8_t)((pix & gmask) >> gs);
            img.rgba[i*4+2] = (uint8_t)((pix & bmask) >> bs);
            img.rgba[i*4+3] = amask ? (uint8_t)((pix & amask) >> as) : 255;
        }
    }
    return img;
}

// ---- TGA (uncompressed + RLE, 24/32-bit and 8-bit grey) -------------------
Image decode_tga(const std::vector<uint8_t> &data)
{
    Image img;
    if(data.size() < 18) return img;
    const uint8_t *d = data.data();
    int idLen = d[0], cmapType = d[1], imgType = d[2];
    if(cmapType != 0) return img;
    if(imgType != 2 && imgType != 3 && imgType != 10 && imgType != 11) return img;
    int w = d[12] | (d[13] << 8), h = d[14] | (d[15] << 8), bpp = d[16];
    uint8_t desc = d[17];
    if(w <= 0 || h <= 0 || w > 16384 || h > 16384) return img;
    int bppx = (imgType == 3 || imgType == 11) ? (bpp == 8 ? 1 : 0)
             : (bpp == 24 ? 3 : bpp == 32 ? 4 : 0);
    if(bppx == 0) return img;

    size_t off = 18 + (size_t)idLen, pixels = (size_t)w * h;
    // Reject header dimensions the file can't possibly back before allocating, so a
    // corrupt TGA can't request a multi-GB buffer. RLE expands at most ~128x.
    if(pixels > (data.size() - off) * 128) return img;
    img.w = w; img.h = h; img.rgba.assign(pixels * 4, 255);
    auto place = [&](size_t i)->size_t{
        size_t x = i % (size_t)w, y = i / (size_t)w;
        if(desc & 0x10) x = w - 1 - x;
        if(!(desc & 0x20)) y = h - 1 - y;
        return (y * (size_t)w + x) * 4;
    };
    auto put = [&](size_t i, const uint8_t *s){
        size_t o = place(i);
        if(bppx == 1) { img.rgba[o]=s[0]; img.rgba[o+1]=s[0]; img.rgba[o+2]=s[0]; img.rgba[o+3]=255; }
        else { img.rgba[o]=s[2]; img.rgba[o+1]=s[1]; img.rgba[o+2]=s[0]; img.rgba[o+3]=(bppx==4)?s[3]:255; }
    };

    if(imgType == 2 || imgType == 3) {
        if(off + pixels * bppx > data.size()) { img = Image(); return img; }
        for(size_t i = 0; i < pixels; i++, off += bppx) put(i, d + off);
        return img;
    }
    size_t written = 0;
    while(written < pixels && off < data.size()) {
        uint8_t packet = data[off++];
        size_t count = (packet & 0x7f) + 1;
        if(packet & 0x80) {
            if(off + bppx > data.size()) break;
            const uint8_t *s = d + off; off += bppx;
            for(size_t i = 0; i < count && written < pixels; i++, written++) put(written, s);
        } else {
            if(off + count * bppx > data.size()) break;
            for(size_t i = 0; i < count && written < pixels; i++, written++, off += bppx) put(written, d + off);
        }
    }
    if(written != pixels) img = Image();
    return img;
}

Image decode_texture(const std::vector<uint8_t> &data)
{
    if(data.size() >= 4) {
        if(memcmp(data.data(), "IWi", 3) == 0)  return decode_iwi(data);
        if(memcmp(data.data(), "DDS ", 4) == 0)  return decode_dds(data);
    }
    return decode_tga(data);
}
