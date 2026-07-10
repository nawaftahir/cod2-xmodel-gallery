#include "gallery.h"
#include <cstdio>
#include <fstream>
#include <algorithm>
#include <map>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"

static std::string lower(std::string s){ for(char &c:s) if(c>='A'&&c<='Z') c+=32; return s; }

bool write_image(const std::string &path, const std::vector<uint8_t> &rgb, int w, int h, int jpegQuality)
{
    std::string ext = lower(path.size() > 4 ? path.substr(path.size() - 4) : "");
    if(ext == ".png") return stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3) != 0;
    return stbi_write_jpg(path.c_str(), w, h, 3, rgb.data(), jpegQuality) != 0;
}

static std::string esc(const std::string &s)
{
    std::string o; o.reserve(s.size() + 16);
    for(char c : s) switch(c) {
        case '&': o += "&amp;"; break;   case '<': o += "&lt;"; break;
        case '>': o += "&gt;"; break;    case '"': o += "&quot;"; break;
        case '\'': o += "&#39;"; break;  default: o += c;
    }
    return o;
}

// Category accent colors (used for the chip on each card and the filter row).
static const char *CSS = R"CSS(
:root{
  --bg:#14161a; --panel:#1b1e24; --card:#22262e; --tile:#0e1013;
  --edge:#313742; --fg:#e9e5d9; --muted:#8b93a1; --accent:#c9a24b; --accent-dim:#8a6f30;
  color-scheme:dark;
}
:root[data-theme="light"]{
  --bg:#e7e3d7; --panel:#efece1; --card:#f6f3ea; --tile:#ded9cb;
  --edge:#cdc6b4; --fg:#23221c; --muted:#6e6a5c; --accent:#9a7420; --accent-dim:#b79240;
  color-scheme:light;
}
@media (prefers-color-scheme:light){:root:not([data-theme="dark"]){
  --bg:#e7e3d7; --panel:#efece1; --card:#f6f3ea; --tile:#ded9cb;
  --edge:#cdc6b4; --fg:#23221c; --muted:#6e6a5c; --accent:#9a7420; --accent-dim:#b79240;
  color-scheme:light;
}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--fg);
  font-family:ui-sans-serif,system-ui,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
.wrap{max-width:1320px;margin:0 auto;padding:0 22px 60px}
header{position:sticky;top:0;z-index:5;background:color-mix(in oklab,var(--bg) 90%,transparent);
  backdrop-filter:blur(10px);border-bottom:1px solid var(--edge)}
.head-in{max-width:1320px;margin:0 auto;padding:18px 22px 14px}
.eyebrow{font:600 11px/1 ui-monospace,Menlo,monospace;letter-spacing:.22em;text-transform:uppercase;
  color:var(--accent);margin:0 0 7px}
h1{margin:0;font-size:24px;font-weight:700;letter-spacing:-.01em}
h1 b{color:var(--accent)}
.sub{margin:6px 0 0;color:var(--muted);font-size:13px}
.controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap;margin-top:14px}
.search{position:relative;flex:1;min-width:220px}
.search input{width:100%;padding:11px 14px 11px 38px;border:1px solid var(--edge);border-radius:9px;
  background:var(--tile);color:var(--fg);font:14px/1.2 ui-monospace,Menlo,monospace}
.search input:focus{outline:2px solid var(--accent-dim);outline-offset:1px}
.search svg{position:absolute;left:12px;top:50%;transform:translateY(-50%);color:var(--muted)}
.count{font:600 12.5px/1 ui-monospace,monospace;color:var(--muted);font-variant-numeric:tabular-nums;white-space:nowrap}
.count b{color:var(--fg)}
.rowlabel{font:600 10px/1 ui-monospace,monospace;letter-spacing:.14em;text-transform:uppercase;
  color:var(--muted);margin:14px 2px 8px}
.filters{display:flex;gap:7px;flex-wrap:wrap}
.filter{font:600 12px/1 ui-monospace,monospace;color:var(--muted);background:transparent;
  border:1px solid var(--edge);border-radius:999px;padding:7px 11px;cursor:pointer;transition:.14s;
  display:inline-flex;gap:6px;align-items:center}
.filter:hover{color:var(--fg);border-color:var(--muted)}
.filter .n{font-size:11px;opacity:.7;font-variant-numeric:tabular-nums}
.filter.on{color:var(--bg);background:var(--accent);border-color:var(--accent)}
.filter.on .n{opacity:.85}
.grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(212px,1fr));gap:14px;margin-top:20px}
.card{background:var(--card);border:1px solid var(--edge);border-radius:12px;overflow:hidden;
  transition:transform .14s,border-color .14s}
.card:hover{transform:translateY(-2px);border-color:var(--accent-dim)}
.thumb{aspect-ratio:4/3;background:
  radial-gradient(120% 120% at 50% 35%,color-mix(in oklab,var(--tile) 82%,var(--bg)) 0%,var(--tile) 100%)}
.thumb img{display:block;width:100%;height:100%;object-fit:contain}
.meta{padding:10px 11px 11px}
.title{font:600 13px/1.3 ui-sans-serif,system-ui,sans-serif;word-break:break-word}
.slug{margin-top:3px;font:11px/1.3 ui-monospace,Menlo,monospace;color:var(--muted);word-break:break-all}
.chips{display:flex;gap:5px;flex-wrap:wrap;margin-top:8px}
.chip{font:600 10px/1 ui-monospace,monospace;letter-spacing:.02em;text-transform:uppercase;
  padding:4px 7px;border-radius:5px;border:1px solid;opacity:.95}
.chip.theme{color:var(--muted);border-color:var(--edge);text-transform:none;letter-spacing:0}
.hidden{display:none!important}
.empty{color:var(--muted);font:13px/1.5 ui-monospace,monospace;padding:44px 4px}
.foot{margin-top:30px;padding-top:16px;border-top:1px solid var(--edge);color:var(--muted);
  font:12px/1.6 ui-monospace,monospace}
.foot b{color:var(--fg)}
@media (prefers-reduced-motion:reduce){.card{transition:none}}
)CSS";

// One accent per category; text+border chip that reads on both themes.
static const std::vector<std::pair<std::string,std::string>> CAT_COLOR = {
    {"Weapons","#c9a24b"}, {"Vehicles","#6f9bc4"}, {"Characters","#c48a8a"},
    {"Heads","#cf9bb2"}, {"Helmets","#b3a56f"}, {"Foliage","#8fa76a"},
    {"Furniture","#c6975f"}, {"Lights","#d8b64e"}, {"Pipes","#87a2ab"},
    {"Awnings","#bd8a5e"}, {"Military","#b0704f"}, {"Signs","#79b2a0"},
    {"Structures","#9aa2ad"}, {"Backdrops","#79a6c6"}, {"Props","#b0a487"},
    {"System","#6f7782"}, {"Misc","#808892"},
};

void write_gallery(const std::string &outdir, const std::vector<GalleryItem> &items)
{
    std::ofstream f(outdir + "/index.html", std::ios::binary);
    if(!f) { fprintf(stderr, "cannot write %s/index.html\n", outdir.c_str()); return; }

    // Counts for filter chips.
    std::map<std::string,int> catCount, themeCount;
    for(const auto &it : items) {
        catCount[it.category]++;
        if(!it.theme.empty()) themeCount[it.theme]++;
    }
    // Categories ordered by our canonical list; themes by frequency.
    std::vector<std::pair<std::string,int>> cats, themes;
    for(auto &c : CAT_COLOR) if(catCount.count(c.first)) cats.push_back({c.first, catCount[c.first]});
    for(auto &t : themeCount) themes.push_back({t.first, t.second});
    std::sort(themes.begin(), themes.end(), [](auto &a, auto &b){ return a.second > b.second; });

    f << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      << "<title>CoD2 XModel Gallery</title><style>" << CSS;
    // Per-category chip colors.
    for(auto &c : CAT_COLOR)
        f << ".chip[data-c=\"" << c.first << "\"]{color:" << c.second
          << ";border-color:color-mix(in oklab," << c.second << " 45%,transparent)}"
          << ".filter[data-f=\"" << c.first << "\"].on{background:" << c.second
          << ";border-color:" << c.second << "}";
    f << "</style></head><body>";

    f << "<header><div class=\"head-in\">"
      << "<p class=\"eyebrow\">Call of Duty 2 &middot; Stock Asset Library</p>"
      << "<h1>XModel <b>Gallery</b></h1>"
      << "<p class=\"sub\">" << items.size() << " models &middot; "
      << cats.size() << " categories &middot; rendered straight from the game archives</p>"
      << "<div class=\"controls\"><label class=\"search\">"
      << "<svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\">"
      << "<circle cx=\"11\" cy=\"11\" r=\"7\"/><path d=\"m21 21-4.3-4.3\"/></svg>"
      << "<input id=\"q\" type=\"search\" placeholder=\"Search " << items.size() << " models...\" autofocus></label>"
      << "<span class=\"count\"><b id=\"n\">" << items.size() << "</b> / " << items.size() << "</span></div>";

    f << "<div class=\"rowlabel\">Category</div><div class=\"filters\" id=\"catf\">"
      << "<button class=\"filter on\" data-f=\"All\">All <span class=\"n\">" << items.size() << "</span></button>";
    for(auto &c : cats)
        f << "<button class=\"filter\" data-f=\"" << c.first << "\">" << c.first
          << " <span class=\"n\">" << c.second << "</span></button>";
    f << "</div>";

    if(!themes.empty()) {
        f << "<div class=\"rowlabel\">Theme / Map</div><div class=\"filters\" id=\"themef\">"
          << "<button class=\"filter on\" data-t=\"All\">All</button>";
        for(auto &t : themes)
            f << "<button class=\"filter\" data-t=\"" << t.first << "\">" << t.first
              << " <span class=\"n\">" << t.second << "</span></button>";
        f << "</div>";
    }
    f << "</div></header>";

    f << "<div class=\"wrap\"><main class=\"grid\" id=\"grid\">";
    for(const auto &it : items) {
        std::string search = lower(it.model + " " + it.pretty);
        f << "<article class=\"card\" data-name=\"" << esc(search) << "\" data-cat=\"" << esc(it.category)
          << "\" data-theme=\"" << esc(it.theme) << "\">"
          << "<div class=\"thumb\"><img loading=\"lazy\" src=\"" << esc(it.file) << "\" alt=\"" << esc(it.pretty) << "\"></div>"
          << "<div class=\"meta\"><div class=\"title\">" << esc(it.pretty) << "</div>"
          << "<div class=\"slug\">" << esc(it.model) << "</div><div class=\"chips\">"
          << "<span class=\"chip\" data-c=\"" << esc(it.category) << "\">" << esc(it.category) << "</span>";
        if(!it.theme.empty()) f << "<span class=\"chip theme\">" << esc(it.theme) << "</span>";
        f << "</div></div></article>";
    }
    f << "</main><p class=\"empty hidden\" id=\"empty\">No models match.</p>"
      << "<p class=\"foot\">Rendered headless via EGL/Mesa &middot; classified into " << cats.size()
      << " categories &middot; raw asset ids preserved for use in Radiant/GSC.</p></div>";

    f << "<script>"
      "const q=document.getElementById('q'),cards=[...document.querySelectorAll('.card')],"
      "nEl=document.getElementById('n'),empty=document.getElementById('empty');"
      "let cat='All',theme='All';"
      "function apply(){const s=q.value.trim().toLowerCase();let n=0;"
      "for(const c of cards){const okS=!s||c.dataset.name.includes(s);"
      "const okC=cat==='All'||c.dataset.cat===cat;const okT=theme==='All'||c.dataset.theme===theme;"
      "const ok=okS&&okC&&okT;c.classList.toggle('hidden',!ok);if(ok)n++;}"
      "nEl.textContent=n;empty.classList.toggle('hidden',n>0);}"
      "q.addEventListener('input',apply);"
      "document.getElementById('catf').addEventListener('click',e=>{const b=e.target.closest('.filter');if(!b)return;"
      "cat=b.dataset.f;catf.querySelectorAll('.filter').forEach(x=>x.classList.toggle('on',x===b));apply();});"
      "const tf=document.getElementById('themef');if(tf)tf.addEventListener('click',e=>{const b=e.target.closest('.filter');if(!b)return;"
      "theme=b.dataset.t;tf.querySelectorAll('.filter').forEach(x=>x.classList.toggle('on',x===b));apply();});"
      "apply();</script></body></html>\n";
    printf("Gallery: %s/index.html (%zu models, %zu categories)\n", outdir.c_str(), items.size(), cats.size());
}
