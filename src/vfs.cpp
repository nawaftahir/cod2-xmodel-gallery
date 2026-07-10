#include "vfs.h"
#include <cstdio>
#include <filesystem>
#include "../third_party/miniz.h"

namespace fs = std::filesystem;

std::string VFS::lower(std::string s)
{
    for(char &c : s) { if(c >= 'A' && c <= 'Z') c += 32; if(c == '\\') c = '/'; }
    return s;
}

VFS::~VFS()
{
    for(void *a : m_archives) {
        if(a) { mz_zip_reader_end((mz_zip_archive*)a); delete (mz_zip_archive*)a; }
    }
}

const VFS::Entry* VFS::find(const std::string &lckey) const
{
    auto it = m_index.find(lckey);
    return it == m_index.end() ? nullptr : &it->second;
}

void VFS::addLooseRoot(const std::string &dir)
{
    std::error_code ec;
    fs::path root(dir);
    if(!fs::is_directory(root, ec)) { fprintf(stderr, "vfs: not a directory: %s\n", dir.c_str()); return; }
    m_looseRoots++;

    for(auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
        it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if(ec) { ec.clear(); continue; }
        if(!it->is_regular_file(ec)) continue;
        std::string rel = fs::relative(it->path(), root, ec).generic_string();
        if(rel.empty()) continue;
        m_index[lower(rel)] = Entry{ -1, it->path().string(), 0 };
    }
}

bool VFS::addIwd(const std::string &path)
{
    auto *zip = new mz_zip_archive();
    memset(zip, 0, sizeof(*zip));
    if(!mz_zip_reader_init_file(zip, path.c_str(), 0)) {
        fprintf(stderr, "vfs: cannot open archive %s\n", path.c_str());
        delete zip;
        return false;
    }
    int archiveIdx = (int)m_archives.size();
    m_archives.push_back(zip);

    mz_uint n = mz_zip_reader_get_num_files(zip);
    for(mz_uint i = 0; i < n; i++) {
        if(mz_zip_reader_is_file_a_directory(zip, i)) continue;
        mz_zip_archive_file_stat st;
        if(!mz_zip_reader_file_stat(zip, i, &st)) continue;
        m_index[lower(st.m_filename)] = Entry{ archiveIdx, st.m_filename, (int)i };
    }
    return true;
}

std::vector<uint8_t> VFS::read(const std::string &vpath) const
{
    const Entry *e = find(lower(vpath));
    if(!e) return {};

    if(e->archive < 0) {
        FILE *f = fopen(e->path.c_str(), "rb");
        if(!f) return {};
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::vector<uint8_t> data(sz > 0 ? (size_t)sz : 0);
        if(sz > 0 && fread(data.data(), 1, (size_t)sz, f) != (size_t)sz) data.clear();
        fclose(f);
        return data;
    }

    auto *zip = (mz_zip_archive*)m_archives[e->archive];
    size_t outSize = 0;
    void *p = mz_zip_reader_extract_to_heap(zip, (mz_uint)e->fileIndex, &outSize, 0);
    if(!p) return {};
    std::vector<uint8_t> data((uint8_t*)p, (uint8_t*)p + outSize);
    mz_free(p);
    return data;
}

std::set<std::string> VFS::listPrefix(const std::string &prefix) const
{
    std::set<std::string> out;
    std::string p = lower(prefix);
    for(auto it = m_index.lower_bound(p); it != m_index.end(); ++it) {
        if(it->first.compare(0, p.size(), p) != 0) break;
        out.insert(it->first);
    }
    return out;
}
