// CoD2 XModel Gallery - headless viewer / batch gallery generator.
//
//   cod2-xmodel-gallery [sources] <modelname>          render one model to an image
//   cod2-xmodel-gallery [sources] --batch [--outdir d] render every model + index.html
//
// Sources (repeatable, combined; later wins):
//   --basepath=<CoD2 dir>   read main/iw_*.iwd (+ localized_*.iwd)
//   --loose=<dir>           read an extracted asset tree (has xmodel/, images/, ...)
#include "vfs.h"
#include "xmodel.h"
#include "renderer.h"
#include "gallery.h"
#include "categorize.h"
#include "gl.h"
#include "glb.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;
static const float PI = 3.14159265f;

static std::string safeName(std::string s)
{
    if(s.rfind("xmodel/", 0) == 0) s = s.substr(7);
    for(char &c : s) if(c == '/' || c == '\\') c = '_';
    return s;
}

int main(int argc, char **argv)
{
    std::vector<std::string> looseRoots;
    std::string basepath, modelName, outdir = "shots";
    bool batch = false, glb = false;
    int W = 640, H = 480, ss = 2, quality = 88, limit = 0, texMaxSize = 512;
    std::string imgExt = ".jpg";

    for(int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if(a.rfind("--basepath=", 0) == 0)      basepath = a.substr(11);
        else if(a.rfind("--loose=", 0) == 0)    looseRoots.push_back(a.substr(8));
        else if(a.rfind("--outdir=", 0) == 0)   outdir = a.substr(9);
        else if(a.rfind("--width=", 0) == 0)    W = atoi(a.substr(8).c_str());
        else if(a.rfind("--height=", 0) == 0)   H = atoi(a.substr(9).c_str());
        else if(a.rfind("--ss=", 0) == 0)       ss = atoi(a.substr(5).c_str());
        else if(a.rfind("--quality=", 0) == 0)  quality = atoi(a.substr(10).c_str());
        else if(a.rfind("--limit=", 0) == 0)    limit = atoi(a.substr(8).c_str());
        else if(a == "--png")                   imgExt = ".png";
        else if(a == "--batch")                 batch = true;
        else if(a == "--glb")                   glb = true;
        else if(a.rfind("--texsize=", 0) == 0)  texMaxSize = atoi(a.substr(10).c_str());
        else                                    modelName = a;
    }
    W = std::max(64, W); H = std::max(64, H); ss = std::clamp(ss, 1, 4);
    quality = std::clamp(quality, 1, 100);

    if(!batch && modelName.empty()) {
        fprintf(stderr,
            "usage: cod2-xmodel-gallery [sources] <modelname>\n"
            "       cod2-xmodel-gallery [sources] --batch [--outdir=./shots]\n"
            "sources: --basepath=<CoD2 dir> | --loose=<extracted asset dir> (repeatable)\n"
            "options: --width=N --height=N --ss=1..4 --quality=1..100 --png --limit=N --glb --texsize=N\n");
        return 1;
    }

    // ---- Build the asset VFS ----
    VFS vfs;
    // A loose root may be an asset tree itself (has xmodel/) or a parent holding
    // several extracted archives (stockrawfiles/iw_00, iw_01, ...); expand those.
    auto looksLikeAssetRoot = [](const fs::path &p) {
        std::error_code ec;
        for(const char *sub : { "xmodel", "xmodelparts", "xmodelsurfs", "images", "materials" })
            if(fs::is_directory(p / sub, ec)) return true;
        return false;
    };
    auto addLoose = [&](const std::string &dir) {
        std::error_code ec;
        // Direct asset tree (has xmodel/): use as-is.
        if(fs::is_directory(fs::path(dir) / "xmodel", ec)) { vfs.addLooseRoot(dir); return; }
        // Parent of extracted archives (stockrawfiles/iw_00, iw_01, ...): expand.
        std::vector<std::string> subs;
        for(auto &e : fs::directory_iterator(dir, ec)) {
            if(e.is_directory(ec) && looksLikeAssetRoot(e.path())) subs.push_back(e.path().string());
        }
        std::sort(subs.begin(), subs.end()); // iw_00 .. iw_15: later wins
        if(subs.empty()) vfs.addLooseRoot(dir);
        else for(const auto &s : subs) vfs.addLooseRoot(s);
    };
    for(const auto &r : looseRoots) addLoose(r);
    if(!basepath.empty()) {
        int added = 0;
        auto tryAdd = [&](const std::string &fn) {
            std::string p = basepath + "/main/" + fn;
            std::error_code ec;
            if(fs::exists(p, ec) && vfs.addIwd(p)) added++;
        };
        for(int i = 0; i <= 30; i++) { char fn[32]; snprintf(fn, sizeof fn, "iw_%02d.iwd", i); tryAdd(fn); }
        for(int i = 0; i <= 9; i++)  { char fn[48]; snprintf(fn, sizeof fn, "localized_english_iw%02d.iwd", i); tryAdd(fn); }
        printf("Loaded %d iwd archive(s) from %s/main\n", added, basepath.c_str());
    }
    if(vfs.sourceCount() == 0) { fprintf(stderr, "no asset sources; pass --loose= or --basepath=\n"); return 1; }

    // ---- Collect model list ----
    std::vector<std::string> models;
    if(batch) {
        for(const auto &e : vfs.listPrefix("xmodel/")) {
            if(e.size() <= 7 || e.back() == '/') continue; // skip the dir entry itself
            models.push_back(e.substr(7)); // strip "xmodel/"
        }
        std::sort(models.begin(), models.end());
        models.erase(std::unique(models.begin(), models.end()), models.end());
        if(limit > 0 && (int)models.size() > limit) models.resize(limit);
        printf("Batch: %zu model(s) -> %s/\n", models.size(), outdir.c_str());
    } else {
        models.push_back(modelName);
    }

    std::error_code ec;
    fs::create_directories(outdir, ec);

    // ---- GL + renderer ----
    if(!gl_headless_init()) { fprintf(stderr, "failed to create headless GL context\n"); return 1; }
    Renderer renderer;
    if(!renderer.init(W, H, ss)) { gl_headless_shutdown(); return 1; }

    std::vector<GalleryItem> gallery;
    int ok = 0, failed = 0;
    for(size_t i = 0; i < models.size(); i++) {
        const std::string &name = models[i];
        XModel model;
        if(!load_xmodel(vfs, name, model)) { failed++; continue; }

        auto surfs = renderer.upload(model, vfs);
        float dist = model.radius * 2.5f;
        // Three-quarter front view: azimuth off the -Y axis, slight elevation.
        renderer.render(surfs, -PI * 0.25f, 0.28f, dist);

        std::vector<uint8_t> rgb;
        std::string file = safeName(name) + imgExt;
        if(renderer.readRGB(rgb) && write_image(outdir + "/" + file, rgb, W, H, quality)) {
            std::string glbName;
            if(glb) {
                std::string glbDir = outdir + "/models";
                fs::create_directories(glbDir, ec);
                glbName = safeName(name) + ".glb";
                if(!write_glb(glbDir + "/" + glbName, model, vfs, texMaxSize))
                    glbName.clear();
            }
            AssetClass ac = classify_model(name);
            gallery.push_back({ name, file, ac.pretty, ac.category, ac.theme,
                                glbName.empty() ? "" : "models/" + glbName,
                                (int)model.triangleCount, (int)model.surfaces.size() });
            ok++;
        } else failed++;

        renderer.freeSurfs(surfs);
        renderer.clearTextureCache(); // materials are shared; bound memory per model
        if(batch && (i % 100 == 99 || i + 1 == models.size()))
            printf("  %zu/%zu (ok=%d fail=%d)\n", i + 1, models.size(), ok, failed);
    }

    if(batch) write_gallery(outdir, gallery);
    else if(ok) printf("Wrote %s/%s\n", outdir.c_str(), (safeName(models[0]) + imgExt).c_str());
    printf("Done: %d rendered, %d failed\n", ok, failed);

    renderer.shutdown();
    gl_headless_shutdown();
    return ok > 0 ? 0 : 2;
}
