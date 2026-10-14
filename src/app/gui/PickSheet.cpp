// PickSheet.cpp -- see PickSheet.h.

#include "app/gui/PickSheet.h"

#include "external/stb_image_write.h"

#include <algorithm>
#include <filesystem>

namespace gui {

namespace {

void append_bytes(void* ctx, void* data, int size) {
    auto* out = (std::vector<uint8_t>*)ctx;
    const uint8_t* p = (const uint8_t*)data;
    out->insert(out->end(), p, p + size);
}

}  // namespace

std::vector<uint8_t> pick_thumb_jpeg(const uint8_t* rgb, int w, int h) {
    std::vector<uint8_t> jpeg;
    if (!rgb || w <= 0 || h <= 0) return jpeg;
    const float s = std::min(1.0f, (float)kPickThumbSide / (float)std::max(w, h));
    const int ow = std::max(1, (int)(w * s + 0.5f)), oh = std::max(1, (int)(h * s + 0.5f));
    // An 8x8 lattice over each pixel's footprint rather than the whole box: a
    // 5K frame boxed in full is 25 ms a pick on the thread feeding the decoder,
    // and fewer taps than this leave straight edges visibly stepped.
    constexpr int kTaps = 8;
    std::vector<uint8_t> thumb((size_t)ow * oh * 3);
    for (int y = 0; y < oh; y++) {
        const int64_t y0 = (int64_t)y * h / oh, y1 = (int64_t)(y + 1) * h / oh;
        for (int x = 0; x < ow; x++) {
            const int64_t x0 = (int64_t)x * w / ow, x1 = (int64_t)(x + 1) * w / ow;
            unsigned sum[3] = {0, 0, 0};
            for (int j = 0; j < kTaps; j++) {
                const int64_t sy = std::min<int64_t>(
                    h - 1, y0 + (2 * j + 1) * (y1 - y0) / (2 * kTaps));
                for (int i = 0; i < kTaps; i++) {
                    const int64_t sx = std::min<int64_t>(
                        w - 1, x0 + (2 * i + 1) * (x1 - x0) / (2 * kTaps));
                    const uint8_t* p = rgb + ((size_t)sy * w + (size_t)sx) * 3;
                    sum[0] += p[0];
                    sum[1] += p[1];
                    sum[2] += p[2];
                }
            }
            uint8_t* o = thumb.data() + ((size_t)y * ow + x) * 3;
            for (int c = 0; c < 3; c++)
                o[c] = (uint8_t)((sum[c] + kTaps * kTaps / 2) / (kTaps * kTaps));
        }
    }
    stbi_write_jpg_to_func(append_bytes, &jpeg, ow, oh, 3, thumb.data(), 82);
    return jpeg;
}

void PickSheet::reset(std::vector<std::string> names) {
    std::lock_guard<std::mutex> lk(_mu);
    _rows.assign(names.size(), Row{});
    _names = std::move(names);
    _gen++;
}

void PickSheet::add(size_t row, const app::FramePick& p, const uint8_t* rgb, int w,
                    int h, std::string path) {
    add(row, p, pick_thumb_jpeg(rgb, w, h), std::move(path));
}

void PickSheet::add(size_t row, const app::FramePick& p, std::vector<uint8_t>&& jpeg,
                    std::string path, std::string origin, std::vector<std::string> more) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size()) return;
    Row& r = _rows[row];
    Pick pick;
    static_cast<app::FramePick&>(pick) = p;
    r.picks.push_back(pick);
    r.jpegs.push_back(std::move(jpeg));
    r.paths.push_back(std::move(path));
    r.origins.push_back(std::move(origin));
    r.more.push_back(std::move(more));
}

void PickSheet::set_dirs(size_t row, std::string keep_dir, std::string skip_dir) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size()) return;
    _rows[row].keep_dir = std::move(keep_dir);
    _rows[row].skip_dir = std::move(skip_dir);
}

void PickSheet::set_in_place(size_t row, bool yes) {
    std::lock_guard<std::mutex> lk(_mu);
    if (row < _rows.size()) _rows[row].in_place = yes;
}

bool PickSheet::in_place(size_t row) const {
    std::lock_guard<std::mutex> lk(_mu);
    return row < _rows.size() && _rows[row].in_place;
}

int PickSheet::resort(size_t row, app::PickRule rule) {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size()) return 0;
    Row& r = _rows[row];
    const fs::path keep(r.keep_dir), skip(r.skip_dir);
    int moved = 0;
    // One file to the side its frame is on now, at the same place under it.
    const auto shift = [&](std::string& path, bool kept) {
        const fs::path from(path);
        fs::path rel = from.lexically_relative(keep);
        if (rel.empty() || *rel.begin() == "..") rel = from.lexically_relative(skip);
        if (rel.empty() || *rel.begin() == "..") return false;
        const fs::path to = (kept ? keep : skip) / rel;
        if (to == from) return false;
        std::error_code ec;
        fs::create_directories(to.parent_path(), ec);
        fs::rename(from, to, ec);
        if (ec) {   // another filesystem
            ec.clear();
            fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
            if (ec) return false;
            fs::remove(from, ec);
        }
        path = to.string();
        return true;
    };
    for (size_t i = 0; i < r.picks.size(); i++) {
        rule.judge(r.picks[i]);
        if (r.paths[i].empty() || r.keep_dir.empty() || r.skip_dir.empty()) continue;
        const bool kept = r.picks[i].verdict == app::PickVerdict::Kept;
        if (shift(r.paths[i], kept)) moved++;
        for (std::string& other : r.more[i]) shift(other, kept);
    }
    return moved;
}

int PickSheet::discard_skipped(size_t row) {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size() || _rows[row].skip_dir.empty()) return 0;
    Row& r = _rows[row];
    const fs::path skip(r.skip_dir);
    std::error_code ec;
    int gone = 0;
    for (size_t i = 0; i < r.picks.size(); i++) {
        if (r.picks[i].verdict == app::PickVerdict::Kept || r.paths[i].empty()) continue;
        // Only what this sheet put there: never a file outside the folder.
        const fs::path rel = fs::path(r.paths[i]).lexically_relative(skip);
        if (rel.empty() || *rel.begin() == "..") continue;
        if (fs::remove(r.paths[i], ec)) gone++;
        r.paths[i].clear();
    }
    // Deepest first; a folder that still holds something refuses and stays.
    std::vector<fs::path> dirs;
    for (fs::recursive_directory_iterator it(skip, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) dirs.push_back(it->path());
    std::sort(dirs.rbegin(), dirs.rend());
    for (const fs::path& d : dirs) fs::remove(d, ec);
    fs::remove(skip, ec);
    r.skip_dir.clear();
    return gone;
}

std::vector<std::string> PickSheet::left_out(size_t row) const {
    namespace fs = std::filesystem;
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    if (row >= _rows.size()) return out;
    const Row& r = _rows[row];
    for (size_t i = 0; i < r.picks.size(); i++) {
        if (r.picks[i].verdict == app::PickVerdict::Kept || r.paths[i].empty()) continue;
        std::vector<std::string> all{r.paths[i]};
        all.insert(all.end(), r.more[i].begin(), r.more[i].end());
        for (const std::string& file : all) {
            const fs::path at(file);
            fs::path rel = at.lexically_relative(fs::path(r.skip_dir));
            if (rel.empty() || *rel.begin() == "..")
                rel = at.lexically_relative(fs::path(r.keep_dir));
            if (!rel.empty() && *rel.begin() != "..") out.push_back(rel.generic_string());
        }
    }
    return out;
}

uint64_t PickSheet::generation() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _gen;
}

std::vector<Pick> PickSheet::picks(size_t row) const {
    std::lock_guard<std::mutex> lk(_mu);
    return row < _rows.size() ? _rows[row].picks : std::vector<Pick>();
}

std::vector<std::string> PickSheet::names() const {
    std::lock_guard<std::mutex> lk(_mu);
    return _names;
}

bool PickSheet::any() const {
    std::lock_guard<std::mutex> lk(_mu);
    for (const Row& r : _rows)
        if (!r.picks.empty()) return true;
    return false;
}

std::string PickSheet::path(size_t row, size_t index) const {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size() || index >= _rows[row].paths.size()) return {};
    const Row& r = _rows[row];
    return r.paths[index].empty() ? r.origins[index] : r.paths[index];
}

std::vector<uint8_t> PickSheet::jpeg(size_t row, size_t index) const {
    std::lock_guard<std::mutex> lk(_mu);
    if (row >= _rows.size() || index >= _rows[row].jpegs.size()) return {};
    return _rows[row].jpegs[index];
}

}  // namespace gui
