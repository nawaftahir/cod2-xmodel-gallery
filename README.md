# cod2-xmodel-gallery

A headless XModel viewer and batch gallery generator for **Call of Duty 2** (XModel
format v20). Point it at a CoD2 install or an extracted asset tree; it reads the
models straight out of the archives, renders each one offscreen.

## Gallery

Live, searchable, filtered by category and theme:
**[GitHub Pages](https://nawaftahir.github.io/cod2-xmodel-gallery/)** ·
**[GitLab Pages](https://cod2-xmodel-gallery-80b783.gitlab.io/)**

Textures live in the sibling [cod2-iwi-gallery](https://nawaftahir.github.io/cod2-iwi-gallery/).

## What it does

- **Reads `.iwd` archives directly** (`iw_00.iwd` … plain zip) — or a loose,
  already-extracted asset tree.
- **Parses XModel v20**: `xmodel/` descriptor → `xmodelparts/` skeleton →
  `xmodelsurfs/` geometry, baking each vertex to world space through its primary
  bone (rigid and skinned models both render in bind pose).
- **Resolves materials → textures**: parses CoD2 `materials/` binaries for the
  `colorMap`, then decodes the referenced **IWI v5** texture (DXT1/3/5).
- **Renders offscreen** to a supersampled framebuffer and writes JPEG (or PNG).
- **Classifies every model** into a type category (Weapons, Vehicles, Structures,
  Foliage, ...) plus a map/theme tag (Egypt, Caen, Stalingrad, ...), so the gallery
  filters by both. Raw asset ids are preserved for use in Radiant/GSC.
- **Batch mode** renders every model and emits `index.html` with client-side search
  and category + theme filters.

For textures, see the sibling tool [cod2-iwi-gallery](../cod2-iwi-gallery).

## Building

No system `-dev` packages and no `sudo` required — it links the Mesa/EGL runtime
that WSLg (or any desktop Mesa) already provides, and vendors its only two
dependencies (`miniz`, `stb_image_write`) as single headers.

```bash
./build.sh
# or: cmake -B build && cmake --build build
```

Needs `g++`/`gcc` (C++17) and a working `libEGL.so.1` + `libGL.so.1`.

## Usage

Sources are repeatable and combined; a later source wins on conflicts.

```bash
# One model from a CoD2 install (reads main/iw_*.iwd)
./build/cod2-xmodel-gallery --basepath="/path/to/Call of Duty 2" vehicle_american_sherman

# One model from an extracted asset tree (a dir with xmodel/, images/, materials/,
# or a parent holding iw_00/, iw_01/, ...)
./build/cod2-xmodel-gallery --loose=/path/to/stockrawfiles weapon_thompson

# Full gallery for GitHub Pages
./build/cod2-xmodel-gallery --basepath="/path/to/Call of Duty 2" --batch --outdir=docs
```

Or use `./deploy.sh --basepath="..."` (or `--loose=...`), which builds the full
gallery into `./gallery` and prints the `gh-pages` publish commands — it never
touches git itself.

The full stock set is ~1,533 models; the tool renders all of them in ~57s (~12 MB).

### Options

| flag | meaning |
|------|---------|
| `--basepath=<dir>` | CoD2 install; reads `main/iw_*.iwd` (+ `localized_*`) |
| `--loose=<dir>` | extracted asset tree, or a parent of `iw_*` dirs (repeatable) |
| `--batch` | render every model + write `index.html` |
| `--outdir=<dir>` | output directory (default `shots`) |
| `--width=N` `--height=N` | thumbnail size (default 640×480) |
| `--ss=1..4` | supersampling factor for anti-aliasing (default 2) |
| `--quality=1..100` | JPEG quality (default 88) |
| `--png` | write PNG instead of JPEG |
| `--limit=N` | batch: only the first N models (for quick tests) |

## Layout

```
src/gl.*         headless EGL context + GL 3.3 loader
src/vfs.*        asset VFS over .iwd archives and loose dirs
src/image.*      DXT1/3/5, IWI v5, DDS, TGA -> RGBA8
src/xmodel.*     XModel v20: descriptor + skeleton + surfaces
src/material.*   CoD2 material -> colorMap texture name
src/categorize.* type + theme classification, display names
src/renderer.*   offscreen FBO renderer + texture pipeline
src/gallery.*    JPEG/PNG output + categorized, searchable index.html
third_party/     miniz (zip), stb_image_write (image output)
```

The `image.*`, `vfs.*`, and `gallery.*` modules are deliberately model-agnostic
so a sibling **IWI texture gallery** can reuse them.

## Notes / limits

- Renders LOD 0 in bind pose (no animation/skinning blend), which is what you
  want for an asset browser.
- Viewmodel hand rigs use engine-baked bone offsets that this tool doesn't apply,
  so a handful of `viewhands_*` models may pose oddly; world models are unaffected.
- Software rendering (llvmpipe) is fine here: ~15–20 models/sec at thumbnail size.

## Credits

Format details cross-checked against the v20 010-templates and Rust parser in
[cod-asset-importer](https://github.com/Har-Kuun/cod-asset-importer) and validated
byte-for-byte against stock CoD2 assets. Original concept:[ cod1-xmodel-gallery.](https://github.com/riicchhaarrd/cod1-xmodel-gallery/tree/master)
