// MaskLayer.cpp -- see MaskLayer.h.

#include "app/gui/mask/MaskLayer.h"

#include "app/FrameMask.h"
#include "core/AtomicFile.h"
#include "data/Json.h"
#include "data/JsonWrite.h"
#include "external/miniz.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <optional>
#include <stdexcept>

namespace fs = std::filesystem;

namespace gui {
namespace mask {

uint64_t fnv1a64(const uint8_t* p, size_t n) {
    uint64_t h = 14695981039346656037ull;
    for (size_t i = 0; i < n; i++) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

std::string fnv_hex(uint64_t h) {
    char buf[17];
    std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
    return buf;
}

bool fnv_parse(const std::string& s, uint64_t& out) {
    if (s.size() != 16) return false;
    uint64_t h = 0;
    for (char c : s) {
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        h = (h << 4) | (uint64_t)d;
    }
    out = h;
    return true;
}

bool read_file(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    out.resize(n > 0 ? (size_t)n : 0);
    const size_t got = n > 0 ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    out.resize(got);
    return n >= 0;
}

std::string normalize_dir(const std::string& dir) {
    fs::path p = fs::path(dir).lexically_normal();
    if (p.filename().empty() && p.has_parent_path()) p = p.parent_path();
    return p.string();
}

std::string frame_key(const std::string& image_root, const std::string& file) {
    const fs::path root(normalize_dir(image_root));
    fs::path rel = fs::path(file).lexically_normal().lexically_relative(root);
    if (rel.empty() || *rel.begin() == "..") rel = fs::path(file).filename();
    rel.replace_extension();
    return rel.generic_string();
}

// A key is '/'-separated (generic_string); make_preferred keeps a Windows
// path from coming out half '\' and half '/' in the messages that name it.
std::string mask_file(const std::string& mask_root, const std::string& key,
                      const MaskPaths& paths) {
    const auto it = paths.find(key);
    return (fs::path(mask_root) / (it == paths.end() ? key + ".png" : it->second))
        .make_preferred().string();
}

std::string layer_file(const std::string& layer_root, const std::string& key,
                       Layer l) {
    const char* tag = l == Layer::Base ? ".base.png"
                    : l == Layer::Drop ? ".drop.png" : ".keep.png";
    return (fs::path(layer_root) / (key + tag)).make_preferred().string();
}

void flip_polarity(uint8_t* px, size_t n) {
    for (size_t i = 0; i < n; i++) px[i] = (uint8_t)(255 - px[i]);
}

void composite(const uint8_t* base, const uint8_t* drop, const uint8_t* keep,
               size_t n, uint8_t* out) {
    for (size_t i = 0; i < n; i++)
        out[i] = (keep && keep[i]) ? 255 : (drop && drop[i]) ? 0 : base[i];
}

// miniz, not stb: an 8K mask takes 43 ms rather than 280 and comes out 7x smaller.
bool encode_gray_png(const uint8_t* px, int w, int h, std::vector<uint8_t>& png) {
    size_t len = 0;
    void* p = tdefl_write_image_to_png_file_in_memory_ex(px, w, h, 1, &len, MZ_BEST_SPEED, 0);
    if (!p || len == 0) {
        mz_free(p);
        return false;
    }
    png.assign((const uint8_t*)p, (const uint8_t*)p + len);
    mz_free(p);
    return true;
}

static bool write_mask_composite(const std::string& mask_root, const std::string& path,
                                 const std::vector<uint8_t>& png) {
    std::error_code ec;
    const auto absolute = fs::absolute(mask_root, ec);
    if (ec) return false;
    auto tree = fs::weakly_canonical(absolute, ec);
    if (ec) return false;
    if (!tree.has_filename()) tree = tree.parent_path();
    const auto parent = tree.parent_path();
    const auto sibling = [&] { return spirula::write_file_atomic(path, png.data(), png.size()); };
    if (parent.empty() || parent == tree) return sibling();

    std::optional<spirula::AtomicOutputFile> staged;
    try {
        staged.emplace(path, 0, parent);
    } catch (const fs::filesystem_error&) {
        return sibling();
    } catch (...) {
        return false;
    }

    try {
        staged->stream().write(reinterpret_cast<const char*>(png.data()),
                               static_cast<std::streamsize>(png.size()));
        staged->close_checked();
        staged->commit();
        return true;
    } catch (const fs::filesystem_error& e) {
        if (e.code() != std::errc::cross_device_link) return false;
        staged.reset();
        return sibling();
    } catch (...) {
        return false;
    }
}

bool fingerprint_file(const std::string& path, uint64_t& out) {
    std::vector<uint8_t> bytes;
    if (!read_file(path, bytes)) return false;
    out = fnv1a64(bytes.data(), bytes.size());
    return true;
}

std::string utc_now_iso() {
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

bool LayerIndex::load(const std::string& layer_root, std::string& error) {
    frames.clear();
    mask_paths.clear();
    mask_root.clear();
    mask_flipped = false;
    const std::string path = (fs::path(layer_root) / kIndexFileName).string();
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;
    JsonValue doc;
    try {
        doc = json_parse_file(path);
    } catch (const std::exception& e) {
        error = path + ": " + e.what();
        return false;
    }
    if (const JsonValue* r = doc.find("mask_root")) mask_root = r->as_string();
    if (const JsonValue* f = doc.find("mask_flipped")) mask_flipped = f->as_bool(false);
    const JsonValue* fr = doc.find("frames");
    if (!fr || !fr->is_object()) return true;
    for (const auto& [key, v] : fr->obj) {
        if (const JsonValue* m = v.find("mask")) {
            const fs::path rel = fs::path(m->as_string()).lexically_normal();
            if (rel.empty() || rel.is_absolute() || rel.has_root_name() || *rel.begin() == "..") {
                error = path + ": invalid mask path";
                return false;
            }
            mask_paths[key] = rel.generic_string();
        }
        IndexEntry e;
        if (const JsonValue* b = v.find("base")) fnv_parse(b->as_string(), e.base_fp);
        if (const JsonValue* c = v.find("composite")) fnv_parse(c->as_string(), e.composite_fp);
        e.kept = (float)v.get_double("kept", 0.0);
        if (const JsonValue* s = v.find("saved_at")) e.saved_at = s->as_string();
        frames[key] = e;
    }
    return true;
}

bool LayerIndex::save(const std::string& layer_root, std::string& error) const {
    if (in_memory) return true;
    JsonWriter w;
    w.object();
    w.field("spirula_mask_edits", 1);
    w.field("mask_root", mask_root);
    w.field("mask_flipped", mask_flipped);
    w.key("frames").object();
    for (const auto& [key, e] : frames) {
        w.key(key.c_str()).object();
        w.field("base", fnv_hex(e.base_fp));
        w.field("composite", fnv_hex(e.composite_fp));
        w.field("kept", e.kept);
        w.field("saved_at", e.saved_at);
        if (const auto it = mask_paths.find(key); it != mask_paths.end())
            w.field("mask", it->second);
        w.end();
    }
    w.end();
    w.end();
    const std::string text = w.str();
    const std::string path = (fs::path(layer_root) / kIndexFileName).string();
    if (!spirula::write_file_atomic(path, text.data(), text.size())) {
        error = path;
        return false;
    }
    return true;
}

BaseState base_state(const std::string& mask_root, const std::string& key,
                     const LayerIndex& idx) {
    const auto it = idx.frames.find(key);
    std::error_code ec;
    const std::string mask = mask_file(mask_root, key, idx.mask_paths);
    const bool have_mask = fs::exists(mask, ec);
    if (it == idx.frames.end()) return have_mask ? BaseState::Unedited : BaseState::Missing;
    if (!have_mask) return BaseState::Missing;
    uint64_t fp = 0;
    if (!fingerprint_file(mask, fp)) return BaseState::Missing;
    return fp == it->second.composite_fp ? BaseState::Unchanged : BaseState::Regenerated;
}

namespace {

bool read_layer_or_zero(const std::string& path, int w, int h,
                        std::vector<uint8_t>& out, std::string& warning) {
    std::error_code ec;
    out.assign((size_t)w * h, 0);
    if (!fs::exists(path, ec)) return true;
    int lw = 0, lh = 0;
    std::vector<uint8_t> px;
    if (!app::load_stencil(path, lw, lh, px) || lw != w || lh != h) {
        if (!warning.empty()) warning += ", ";
        warning += path;
        return false;
    }
    out.swap(px);
    return true;
}

// Is `mask` (app polarity) the composite of the recorded base under `layers`,
// i.e. our own last write re-encoded rather than somebody else's mask?
bool same_picture_as_composite(const std::string& base_path, const FrameLayers& layers,
                               const std::vector<uint8_t>& mask, bool flipped) {
    std::error_code ec;
    if (!fs::exists(base_path, ec)) return false;
    int bw = 0, bh = 0;
    std::vector<uint8_t> base;
    if (!app::load_stencil(base_path, bw, bh, base)) return false;
    if (bw != layers.w || bh != layers.h || base.size() != mask.size()) return false;
    if (flipped) flip_polarity(base.data(), base.size());
    std::vector<uint8_t> out(base.size());
    composite(base.data(), layers.drop.data(), layers.keep.data(), out.size(), out.data());
    return out == mask;
}

}  // namespace

bool read_layers(const std::string& layer_root, const std::string& key, int w,
                 int h, FrameLayers& out, std::string& warning) {
    out.w = w;
    out.h = h;
    const bool d = read_layer_or_zero(layer_file(layer_root, key, Layer::Drop), w, h,
                                      out.drop, warning);
    const bool k = read_layer_or_zero(layer_file(layer_root, key, Layer::Keep), w, h,
                                      out.keep, warning);
    return d && k;
}

bool save_frame(const std::string& layer_root, const std::string& mask_root,
                const std::string& key, int w, int h, const uint8_t* base,
                const uint8_t* drop, const uint8_t* keep, bool write_composite,
                LayerIndex& idx, std::string& error) {
    std::error_code ec;
    const size_t n = (size_t)w * h;
    const std::string base_path = layer_file(layer_root, key, Layer::Base);
    const std::string mask_path = mask_file(mask_root, key, idx.mask_paths);
    IndexEntry e = idx.frames.count(key) ? idx.frames.at(key) : IndexEntry{};

    if (!fs::exists(base_path, ec) && fs::exists(mask_path, ec)) {
        std::vector<uint8_t> bytes;
        if (!read_file(mask_path, bytes)) { error = mask_path; return false; }
        if (!spirula::write_file_atomic(base_path, bytes.data(), bytes.size())) {
            error = base_path;
            return false;
        }
        e.base_fp = fnv1a64(bytes.data(), bytes.size());
    }

    std::vector<uint8_t> png;
    const std::string drop_path = layer_file(layer_root, key, Layer::Drop);
    if (!encode_gray_png(drop, w, h, png) ||
        !spirula::write_file_atomic(drop_path, png.data(), png.size())) {
        error = drop_path;
        return false;
    }
    const std::string keep_path = layer_file(layer_root, key, Layer::Keep);
    if (!encode_gray_png(keep, w, h, png) ||
        !spirula::write_file_atomic(keep_path, png.data(), png.size())) {
        error = keep_path;
        return false;
    }

    if (write_composite) {
        std::vector<uint8_t> out(n);
        composite(base, drop, keep, n, out.data());
        size_t kept = 0;
        for (uint8_t v : out) kept += v ? 1 : 0;
        if (idx.mask_flipped) flip_polarity(out.data(), out.size());
        if (!encode_gray_png(out.data(), w, h, png) ||
            !write_mask_composite(mask_root, mask_path, png)) {
            error = mask_path;
            return false;
        }
        e.composite_fp = fnv1a64(png.data(), png.size());
        e.kept = (float)kept / (float)std::max<size_t>(n, 1);
    }
    e.saved_at = utc_now_iso();
    idx.frames[key] = e;
    return idx.save(layer_root, error);
}

bool recomposite_frame(const std::string& layer_root, const std::string& mask_root,
                       const std::string& key, LayerIndex& idx, BaseState& found,
                       std::string& error) {
    found = base_state(mask_root, key, idx);
    if (found != BaseState::Regenerated) return true;
    const std::string mask_path = mask_file(mask_root, key, idx.mask_paths);
    std::vector<uint8_t> bytes;
    if (!read_file(mask_path, bytes)) { error = mask_path; return false; }
    int w = 0, h = 0;
    std::vector<uint8_t> base;
    if (!app::load_stencil(mask_path, w, h, base)) { error = mask_path; return false; }
    if (idx.mask_flipped) flip_polarity(base.data(), base.size());
    FrameLayers layers;
    std::string warning;
    if (!read_layers(layer_root, key, w, h, layers, warning)) {
        error = warning;
        return false;
    }
    const std::string base_path = layer_file(layer_root, key, Layer::Base);
    // The fingerprint is over FILE BYTES, so oxipng, a different libpng or a
    // metadata strip all read as regenerated. Only a different PICTURE may
    // rebase: .base.png is the only copy of what the run wrote.
    if (same_picture_as_composite(base_path, layers, base, idx.mask_flipped)) {
        idx.frames[key].composite_fp = fnv1a64(bytes.data(), bytes.size());
        found = BaseState::Unchanged;
        return idx.save(layer_root, error);
    }
    if (!spirula::write_file_atomic(base_path, bytes.data(), bytes.size())) {
        error = base_path;
        return false;
    }
    idx.frames[key].base_fp = fnv1a64(bytes.data(), bytes.size());
    return save_frame(layer_root, mask_root, key, w, h, base.data(), layers.drop.data(),
                      layers.keep.data(), true, idx, error);
}

// -1 only if the index would not load. One bad frame does not withhold the
// rest of the batch; each failure is named in `error`, "; "-joined.
int recomposite_all(const std::string& layer_root, std::string& error) {
    LayerIndex idx;
    if (!idx.load(layer_root, error)) return -1;
    if (idx.mask_root.empty()) return 0;
    std::vector<std::string> keys;
    for (const auto& [k, e] : idx.frames) keys.push_back(k);
    int rebased = 0;
    std::string failures;
    for (const std::string& k : keys) {
        BaseState st;
        std::string frame_error;
        if (!recomposite_frame(layer_root, idx.mask_root, k, idx, st, frame_error)) {
            if (!failures.empty()) failures += "; ";
            failures += k + ": " + frame_error;
            continue;
        }
        if (st == BaseState::Regenerated) rebased++;
    }
    error = failures;
    return rebased;
}

bool revert_frame(const std::string& layer_root, const std::string& mask_root,
                  const std::string& key, LayerIndex& idx, std::string& error) {
    std::error_code ec;
    const std::string base_path = layer_file(layer_root, key, Layer::Base);
    if (fs::exists(base_path, ec)) {
        std::vector<uint8_t> bytes;
        if (!read_file(base_path, bytes)) { error = base_path; return false; }
        const std::string mask_path = mask_file(mask_root, key, idx.mask_paths);
        if (!write_mask_composite(mask_root, mask_path, bytes)) {
            error = mask_path;
            return false;
        }
    }
    // An orphaned layer file with no index entry gets silently re-read as a
    // correction next time this frame opens, so a failed removal must leave
    // the entry standing, not just report false.
    std::string failed;
    for (Layer l : {Layer::Base, Layer::Drop, Layer::Keep}) {
        const std::string path = layer_file(layer_root, key, l);
        fs::remove(path, ec);
        if (ec) {
            if (!failed.empty()) failed += ", ";
            failed += path;
        }
    }
    if (!failed.empty()) {
        error = failed;
        return false;
    }
    idx.frames.erase(key);
    return idx.save(layer_root, error);
}

int revert_all(const std::string& layer_root, std::string& error) {
    LayerIndex idx;
    if (!idx.load(layer_root, error)) return -1;
    if (idx.mask_root.empty()) return 0;
    std::vector<std::string> keys;
    for (const auto& [k, e] : idx.frames) keys.push_back(k);
    int reverted = 0;
    std::string failures;
    for (const std::string& k : keys) {
        std::string frame_error;
        if (!revert_frame(layer_root, idx.mask_root, k, idx, frame_error)) {
            if (!failures.empty()) failures += "; ";
            failures += k + ": " + frame_error;
            continue;
        }
        reverted++;
    }
    error = failures;
    return reverted;
}

bool frame_size(const std::string& layer_root, const std::string& mask_root,
                const std::string& key, const std::string& image_file, int& w, int& h,
                std::string& from, const MaskPaths& paths) {
    std::error_code ec;
    from = layer_file(layer_root, key, Layer::Base);
    if (fs::exists(from, ec)) return app::image_size(from, w, h);
    from = mask_file(mask_root, key, paths);
    if (fs::exists(from, ec)) return app::image_size(from, w, h);
    from = image_file;
    return app::image_size(from, w, h);
}

bool snapshot_layers(const std::string& layer_root, const std::string& key,
                     const LayerIndex& idx, LayerSnapshot& out, std::string& error) {
    out = LayerSnapshot{};
    out.key = key;
    error.clear();
    const auto it = idx.frames.find(key);
    out.had_entry = it != idx.frames.end();
    if (out.had_entry) out.entry = it->second;
    std::error_code ec;
    const std::string d = layer_file(layer_root, key, Layer::Drop);
    const std::string k = layer_file(layer_root, key, Layer::Keep);
    // Not exists(): read_file "reads" a directory as 0 bytes, which restore
    // would then write back as an empty, unloadable layer file.
    out.had_drop = fs::is_regular_file(d, ec);
    out.had_keep = fs::is_regular_file(k, ec);
    if (out.had_drop && !read_file(d, out.drop_png)) { error = d; return false; }
    if (out.had_keep && !read_file(k, out.keep_png)) { error = k; return false; }
    return true;
}

bool restore_layers(const std::string& layer_root, const std::string& mask_root,
                    const LayerSnapshot& snap, LayerIndex& idx, std::string& error) {
    std::error_code ec;
    const std::string d = layer_file(layer_root, snap.key, Layer::Drop);
    const std::string k = layer_file(layer_root, snap.key, Layer::Keep);
    const std::string base_path = layer_file(layer_root, snap.key, Layer::Base);
    const std::string mask_path = mask_file(mask_root, snap.key, idx.mask_paths);
    auto put_one = [&](const std::string& path, bool had, const std::vector<uint8_t>& png) {
        if (had) return spirula::write_file_atomic(path, png.data(), png.size());
        fs::remove(path, ec);
        return true;
    };
    // Stops at the first failed write; `bad` names it.
    auto put = [&](const LayerSnapshot& s, std::string& bad) {
        if (!put_one(d, s.had_drop, s.drop_png)) bad = d;
        else if (!put_one(k, s.had_keep, s.keep_png)) bad = k;
        return bad.empty();
    };
    // What a failure puts back. Restoring never reads these, so one that will
    // not read costs only the put-back, not the restore.
    LayerSnapshot now;
    std::string unused;
    const bool can_put_back = snapshot_layers(layer_root, snap.key, idx, now, unused);
    // Orphan layers (no entry) read as a correction on the next open, so
    // they are the target's state too: revert, then put them back.
    if (!snap.had_entry) {
        // revert_frame writes the mask before it removes the files; a removal
        // that fails keeps the entry, so the propagated files go back with it.
        std::vector<uint8_t> found_mask, found_base;
        const bool has_mask = fs::is_regular_file(mask_path, ec), has_base = fs::is_regular_file(base_path, ec);
        const bool readable = can_put_back && (!has_mask || read_file(mask_path, found_mask)) &&
                              (!has_base || read_file(base_path, found_base));
        if (!revert_frame(layer_root, mask_root, snap.key, idx, error)) {
            // Each file on its own: the one that would not go is still there.
            if (readable && idx.frames.count(snap.key)) {
                if (has_base) spirula::write_file_atomic(base_path, found_base.data(), found_base.size());
                put_one(d, now.had_drop, now.drop_png);
                put_one(k, now.had_keep, now.keep_png);
                if (has_mask) write_mask_composite(mask_root, mask_path, found_mask);
            }
            return false;
        }
        if (snap.had_drop && !spirula::write_file_atomic(d, snap.drop_png.data(), snap.drop_png.size())) {
            error = d;
            return false;
        }
        if (snap.had_keep && !spirula::write_file_atomic(k, snap.keep_png.data(), snap.keep_png.size())) {
            error = k;
            return false;
        }
        return true;
    }
    const auto found = idx.frames.find(snap.key);
    const bool had = found != idx.frames.end();
    const IndexEntry prev = had ? found->second : IndexEntry{};
    // A failure puts back the entry found (the snapshot's over the propagated
    // mask would rebase the base) and, while the mask on disk is still that
    // entry's composite, the layers found, so a reopen shows what is on disk.
    auto fail = [&](const std::string& what) {
        if (!what.empty()) error = what;
        if (had) idx.frames[snap.key] = prev;
        else idx.frames.erase(snap.key);
        uint64_t fp = 0;
        std::string ignored;
        if (can_put_back && had && prev.composite_fp != snap.entry.composite_fp &&
            fingerprint_file(mask_path, fp) && fp == prev.composite_fp)
            put(now, ignored);
        return false;
    };
    std::string bad;
    if (!put(snap, bad)) return fail(bad);
    idx.frames[snap.key] = snap.entry;
    if (!fs::exists(base_path, ec)) return idx.save(layer_root, error) || fail("");
    // The base may have been re-based since the snapshot; the file rules.
    fingerprint_file(base_path, idx.frames[snap.key].base_fp);
    int w = 0, h = 0;
    std::vector<uint8_t> base;
    if (!app::load_stencil(base_path, w, h, base)) return fail(base_path);
    if (idx.mask_flipped) flip_polarity(base.data(), base.size());   // as recomposite_frame
    FrameLayers layers;
    std::string warning;
    // save_frame re-encodes what this reads: a misfit read as zero would
    // overwrite the snapshot's bytes just written back.
    if (!read_layers(layer_root, snap.key, w, h, layers, warning)) return fail(warning);
    // A Missing frame gets its layers back and no mask, as MaskDoc::save.
    const bool write_comp = fs::exists(mask_path, ec);
    if (!save_frame(layer_root, mask_root, snap.key, w, h, base.data(), layers.drop.data(),
                    layers.keep.data(), write_comp, idx, error))
        return fail("");
    return true;
}

// A relative root resolves against the working directory, which is no dataset.
static bool kept_root_ok(const std::string& layer_root, std::string& error) {
    if (fs::path(layer_root).is_absolute()) return true;
    error = std::string(kKeptFileName) + ": not an absolute folder: \"" + layer_root + "\"";
    return false;
}

bool KeptCache::load(const std::string& layer_root, std::string& error) {
    frames.clear();
    dirty = false;
    if (!kept_root_ok(layer_root, error)) return false;
    const std::string path = (fs::path(layer_root) / kKeptFileName).string();
    std::error_code ec;
    if (!fs::exists(path, ec)) return true;
    JsonValue doc;
    try {
        doc = json_parse_file(path);
    } catch (const std::exception& e) {
        error = path + ": " + e.what();
        return false;
    }
    const JsonValue* fr = doc.find("frames");
    if (!fr || !fr->is_object()) return true;
    for (const auto& [key, v] : fr->obj) {
        KeptEntry e;
        if (const JsonValue* p = v.find("fp")) fnv_parse(p->as_string(), e.fp);
        if (const JsonValue* q = v.find("flipped")) e.flipped = q->as_bool(false);
        e.kept = (float)v.get_double("kept", 0.0);
        frames[key] = e;
    }
    return true;
}

bool KeptCache::save(const std::string& layer_root, std::string& error) const {
    if (!dirty) return true;
    if (!kept_root_ok(layer_root, error)) return false;
    JsonWriter w;
    w.object();
    w.field("spirula_mask_kept", 1);
    w.key("frames").object();
    for (const auto& [key, e] : frames) {
        w.key(key.c_str()).object();
        w.field("fp", fnv_hex(e.fp));
        w.field("flipped", e.flipped);
        w.field("kept", e.kept);
        w.end();
    }
    w.end();
    w.end();
    const std::string text = w.str();
    const std::string path = (fs::path(layer_root) / kKeptFileName).string();
    if (!spirula::write_file_atomic(path, text.data(), text.size())) {
        error = path;
        return false;
    }
    return true;
}

bool kept_fraction_of(const std::string& mask_root, const std::string& key, bool flipped,
                      KeptCache& cache, float& kept, const MaskPaths& paths) {
    kept = -1.0f;
    const std::string path = mask_file(mask_root, key, paths);
    // A FIFO or a device there would block the open until a writer came.
    std::error_code ec;
    if (!fs::is_regular_file(path, ec)) return false;
    uint64_t fp = 0;
    if (!fingerprint_file(path, fp)) return false;
    const auto it = cache.frames.find(key);
    if (it != cache.frames.end() && it->second.fp == fp && it->second.flipped == flipped) {
        kept = it->second.kept;
        return true;
    }
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    if (!app::load_stencil(path, w, h, px)) return false;
    size_t n = 0;
    for (uint8_t v : px) n += (v != 0) != flipped ? 1 : 0;
    kept = px.empty() ? 0.0f : (float)n / (float)px.size();
    cache.frames[key] = KeptEntry{fp, flipped, kept};
    cache.dirty = true;
    return true;
}

}  // namespace mask
}  // namespace gui
