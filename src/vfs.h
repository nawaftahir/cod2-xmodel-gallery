// Virtual filesystem over CoD2 asset sources.
//
// A source is either a loose directory (an extracted asset tree such as
// Refrences/stockrawfiles/iw_13, containing xmodel/ xmodelparts/ images/ ...)
// or an .iwd archive (a plain zip). Lookups are case-insensitive; sources added
// later take priority, mirroring the engine's higher-numbered-iwd-wins rule.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <map>

class VFS {
public:
    ~VFS();

    // Register an extracted asset directory (its children are xmodel/, images/, ...).
    void addLooseRoot(const std::string &dir);
    // Register an .iwd/.pk3 archive. Returns false if it cannot be opened.
    bool addIwd(const std::string &path);

    // Read a virtual path (e.g. "xmodel/vehicle_american_sherman"). Empty on miss.
    std::vector<uint8_t> read(const std::string &vpath) const;
    bool exists(const std::string &vpath) const { return find(lower(vpath)) != nullptr; }

    // All virtual paths beginning with `prefix` (e.g. "xmodel/").
    std::set<std::string> listPrefix(const std::string &prefix) const;

    size_t sourceCount() const { return m_looseRoots + m_archives.size(); }

    static std::string lower(std::string s);

private:
    struct Entry {
        int  archive;      // -1 => loose file
        std::string path;  // loose: absolute path; archive: original entry name
        int  fileIndex;    // archive entry index
    };
    std::vector<void*>       m_archives;      // miniz mz_zip_archive* (opaque)
    std::map<std::string, Entry> m_index;     // lowercased vpath -> entry
    size_t m_looseRoots = 0;

    const Entry* find(const std::string &lckey) const;
};
