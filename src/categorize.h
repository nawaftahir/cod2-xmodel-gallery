// Asset classification for CoD2 stock models.
//
// Two axes: a primary TYPE category (Weapons, Vehicles, Structures, ...) and an
// optional THEME/map tag (Egypt, Caen, Moscow, ...). Rules are ordered keyword
// matches over the asset name, tuned against the full 1533-model stock set and
// cross-checked with ZeRoY's community XModels gallery taxonomy. Also derives a
// human-readable display name while keeping the raw asset id authoritative.
#pragma once
#include <string>

struct AssetClass {
    std::string category; // never empty ("Misc" fallback)
    std::string theme;    // may be empty
    std::string pretty;   // readable title derived from the raw name
};

AssetClass classify_model(const std::string &rawName);
