#include "categorize.h"
#include <regex>
#include <vector>
#include <utility>
#include <cctype>

namespace {

struct Rule { const char *cat; std::regex rx; };

// Ordered; first match wins. Kept in lockstep with the validated prototype.
const std::vector<std::pair<const char*, const char*>> TYPE_SRC = {
    // Helper / test assets first, so "..._rig_test" never reads as content.
    {"System",    R"((rig_test|_joints\b|joints_|_drones|drones_|viewcam|gun_positio|basementwall_rig|_rope_rig|climb_rope))"},
    {"System",    R"(^(default|void|temp|tag_|fx$|fx_|static$|sundirection|pointeground|mp_|shadow|health_))"},
    {"Weapons",   R"(^(weapon_|viewmodel_|projectile_|defaultweapon))"},
    {"Weapons",   R"((panzerschreck|bazooka|binoculars|grenade_bag))"},
    {"Vehicles",  R"(^(vehicle_|defaultvehicle|civiliancar))"},
    {"Vehicles",  R"((traincar|higgins|_tank\b|tank_|halftrack|sherman|_tiger|panzer\b|kubel|opel|condor|stuka|spitfire|cargoship|locomotive|boxcar|flatcar|plane_rig|_jeep\b))"},
    {"Heads",     R"(^(head_|playerhead))"},
    {"Helmets",   R"(^helmet_)"},
    {"Characters",R"(^(character_|playerbody_|mannequin|defaultactor))"},
    {"Foliage",   R"(^(tree|brush))"},
    {"Foliage",   R"((foliage|grasstuft|grassflower|spikeybush|_bush|bushweed|hedge|_ivy\b|_fern|_palm\b|_weed|_vine|_shrub|_leaf|leaves|flowerplant))"},
    {"Furniture", R"(^furniture_)"},
    {"Furniture", R"((churchpew|_pew|_couch|_armchair|_chair|_desk|_table|_bed\b|_dresser|_piano|bookshelf|armoire|bathtub))"},
    {"Awnings",   R"(^awning)"},
    {"Lights",    R"(^light_)"},
    {"Lights",    R"((chandelier|chandalier|streetlamp|_lamp|lantern|utillight|sconce|hanginglight|hanginglamp))"},
    {"Pipes",     R"(^(railpipe|waterpipe|powerpole|woodenpole))"},
    {"Pipes",     R"((_pipe\b|pipetower|pipe_))"},
    {"Signs",     R"(^(sign|railsign))"},
    {"Military",  R"(^(military_|minefield))"},
    {"Military",  R"((sandbag|barbedwire|barbed_wire|tanktrap|dragonstooth|hedgehog|_mine\b|landmine|_tnt|tntbomb|ammo|flak88|mortar|artillery|_shell\b|panzerfaust|_bomb\b|gun_positio))"},
    {"Backdrops", R"(^(backdrop|dome_|dome0))"},
    {"Backdrops", R"((mountains|_backdrop|silotown_trees|newvillers_trees))"},
    {"Structures",R"((_wall|^wall|basementwall|window|_door|^door|doorframe|trench|^bunker|bunker0|_bunker|silo0|^silo|storage_tank|cliff|halfrock|_rock|^rock|stonewell|_well\d|railtie|_pallet|pallet0|_fence|_roof|_building|_arch\b|pillar|_column|_stairs|rubble|debris|redbrick|brickpile|_brick|_board|_plank|_ruin|powerline|scaffold|_bridge|bridge_|tower))"},
    {"Props",     R"((^prop_|_prop_))"},
    {"Props",     R"(^(cow|horse)_)"},
    {"Props",     R"((_bear\b|teddy|crate|barrel|tinbox|_box\b|_vase|_cloth\b|_radio|radio_|haycart|winebottle|diningplate|kitchen|_book\b|_flag\b|bottle|_cart\b|_bag\b|_sack))"},
};

const std::vector<std::pair<const char*, const char*>> THEME_SRC = {
    {"Egypt",    R"((^egypt|_egypt|elalamein|el_alamein|matmata|libya|tunisia|northafrica|_africa|africa_|desert|dak1|dak_))"},
    {"Caen",     R"((^caen|_caen))"},
    {"Duhoc",    R"((^duhoc|_duhoc))"},
    {"Hill400",  R"((^hill400|_hill400))"},
    {"Toujane",  R"((^toujane|_toujane))"},
    {"Moscow",   R"((^moscow|_moscow|_winter|winter_|_snow|snow_|russian|stalingrad))"},
    {"Normandy", R"((_normandy|normandy_|newviller|coreviller|_rhine|rhine_|eldaba|gully|_french|french_))"},
};

std::vector<Rule> compile(const std::vector<std::pair<const char*, const char*>> &src)
{
    std::vector<Rule> out;
    out.reserve(src.size());
    for(auto &s : src) out.push_back({ s.first, std::regex(s.second, std::regex::ECMAScript | std::regex::icase) });
    return out;
}

const std::vector<Rule>& typeRules()  { static auto r = compile(TYPE_SRC);  return r; }
const std::vector<Rule>& themeRules() { static auto r = compile(THEME_SRC); return r; }

std::string toLower(std::string s) { for(char &c : s) c = (char)std::tolower((unsigned char)c); return s; }

// Uppercase tokens that read wrong in Title Case (acronyms, weapon designations).
bool allDigits(const std::string &t) { for(char c : t) if(!std::isdigit((unsigned char)c)) return false; return !t.empty(); }

std::string prettify(const std::string &raw)
{
    std::string n = toLower(raw);
    // Drop a single leading type-classifier word; the rest carries the real name.
    static const std::regex lead(R"(^(viewmodel|playerbody|character|vehicle|weapon|head|helmet|furniture|military|awning|light|sign|backdrop|brush|tree|prop|projectile)_)");
    n = std::regex_replace(n, lead, "");
    for(char &c : n) if(c == '_') c = ' ';
    // Title-case, upper-casing acronym-ish tokens.
    std::string out; std::string tok;
    auto flush = [&]() {
        if(tok.empty()) return;
        bool acro = allDigits(tok) || tok.size() <= 2 ||
                    tok == "smg" || tok == "mg" || tok == "tnt" || tok == "bar" || tok == "us";
        if(acro) for(char c : tok) out += (char)std::toupper((unsigned char)c);
        else { out += (char)std::toupper((unsigned char)tok[0]); out += tok.substr(1); }
        tok.clear();
    };
    for(char c : n) {
        if(c == ' ') { flush(); if(!out.empty()) out += ' '; }
        else tok += c;
    }
    flush();
    return out.empty() ? raw : out;
}

} // namespace

AssetClass classify_model(const std::string &rawName)
{
    std::string n = toLower(rawName);
    AssetClass r;
    r.category = "Misc";
    for(const auto &rule : typeRules())
        if(std::regex_search(n, rule.rx)) { r.category = rule.cat; break; }
    for(const auto &rule : themeRules())
        if(std::regex_search(n, rule.rx)) { r.theme = rule.cat; break; }
    r.pretty = prettify(rawName);
    return r;
}
