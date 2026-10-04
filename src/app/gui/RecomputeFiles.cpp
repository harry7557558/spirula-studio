// RecomputeFiles.cpp -- see RecomputeFiles.h.

#include "app/gui/RecomputeFiles.h"

#include <fstream>
#include <iterator>
#include <vector>

namespace fs = std::filesystem;

namespace gui {

namespace {

// The files a recompute replaces; the replaced ones are kept with this suffix.
constexpr const char* kReplaced[] = {"images.bin", "points3D.bin"};
constexpr const char* kOriginalSuffix = "_original";
// In the model folder: the sizes and times of the files the last recompute
// put there, so a solve exported over them since is told apart from them.
constexpr const char* kMarker = ".spirula_recompute";

std::string file_stamp(const fs::path& model) {
    std::string out;
    for (const char* name : kReplaced) {
        std::error_code ec, ec2;
        const auto size = fs::file_size(model / name, ec);
        const auto t = fs::last_write_time(model / name, ec2);
        out += std::string(name) + " " + std::to_string(ec ? 0 : size) + " " +
               std::to_string(ec2 ? 0 : (long long)t.time_since_epoch().count()) + "\n";
    }
    return out;
}

bool recomputed_here(const fs::path& model) {
    std::ifstream f(model / kMarker, std::ios::binary);
    if (!f) return false;
    const std::string have((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return have == file_stamp(model);
}

fs::path original(const fs::path& model, const char* name) {
    return model / (std::string(name) + kOriginalSuffix);
}

}  // namespace

// Not fs::remove_all (AGENTS.md: libtorch interposes it).
void remove_dir_tree(const fs::path& dir) {
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_directory(ec)) dirs.push_back(it->path());
        else fs::remove(it->path(), ec);
    }
    for (auto d = dirs.rbegin(); d != dirs.rend(); ++d) fs::remove(*d, ec);
    fs::remove(dir, ec);
}

bool holds_recompute(const fs::path& model) {
    std::error_code ec;
    return recomputed_here(model) && fs::exists(original(model, "images.bin"), ec);
}

// The new files in place of the model's own, which become *_original -- unless
// they are a previous recompute's, whose originals already there stay. "" or
// what failed.
std::string install_recomputed(const fs::path& model, const fs::path& made) {
    std::error_code ec;
    const bool again = recomputed_here(model);
    for (const char* name : kReplaced) {
        const fs::path tmp = model / (std::string(name) + ".new");
        fs::copy_file(made / name, tmp, fs::copy_options::overwrite_existing, ec);
        if (ec) return ec.message();
    }
    for (const char* name : kReplaced) {
        const fs::path cur = model / name, orig = original(model, name);
        if (again) {
            fs::remove(cur, ec);
        } else if (fs::exists(cur, ec)) {
            fs::remove(orig, ec);
            fs::rename(cur, orig, ec);
        }
        if (ec) return ec.message();
        fs::rename(model / (std::string(name) + ".new"), cur, ec);
        if (ec) return ec.message();
    }
    std::ofstream(model / kMarker, std::ios::binary | std::ios::trunc) << file_stamp(model);
    return "";
}

// Back to *_original. A file with no original was not there before.
std::string restore_original(const fs::path& model) {
    std::error_code ec;
    for (const char* name : kReplaced) {
        const fs::path cur = model / name, orig = original(model, name);
        if (fs::exists(orig, ec)) {
            fs::remove(cur, ec);
            fs::rename(orig, cur, ec);
        } else {
            fs::remove(cur, ec);
        }
        if (ec) return ec.message();
    }
    fs::remove(model / kMarker, ec);
    return "";
}

}  // namespace gui
