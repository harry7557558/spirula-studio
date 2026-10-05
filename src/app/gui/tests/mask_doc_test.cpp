// mask_doc_test -- app/gui/mask/: the correction layer on disk, the document
// in memory, the view math, and the session's worker, over synthetic frames
// whose every pixel is known. SS_MASK_BENCH=<dir> also runs the timing
// floors at 7680x3840 and writes that fixture dataset into <dir>.

#include "app/FrameLook.h"
#include "app/FrameMask.h"
#include "app/gui/edit/Selection.h"
#include "app/gui/mask/Livewire.h"
#include "app/gui/mask/MaskAdd.h"
#include "app/gui/MaskPrompt.h"
#include "app/gui/Picture.h"
#include "app/gui/MaskSettings.h"
#include "app/gui/mask/MaskSam.h"
#include "app/gui/mask/MaskDoc.h"
#include "app/gui/mask/PathTool.h"
#include "app/gui/mask/PenTool.h"
#include "app/gui/mask/MaskLayer.h"
#include "app/gui/mask/MaskSession.h"
#include "app/gui/mask/MaskSlideshow.h"
#include "app/gui/mask/MaskWindow.h"
#include "core/ImageOrient.h"
#include "core/MaskMargin.h"
#include "core/SourcePath.h"
#include "external/stb_image_write.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/MaskEdit.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
namespace mk = gui::mask;

namespace {

int g_failures = 0;

// A FAIL is flushed at once: a later abort would lose a piped buffer, and with
// it the name of the check that saw the defect.
void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) {
        g_failures++;
        std::fflush(stdout);
    }
}

// A chmod can inject a read or write failure only for a non-root POSIX user.
bool chmod_injects() {
#ifndef _WIN32
    if (geteuid() != 0) return true;
#endif
    static bool said = false;
    if (!said) std::printf("skip chmod failure-injection arms: skipped (Windows/root)\n");
    said = true;
    return false;
}

fs::path scratch(const char* name) {
    const fs::path d = fs::temp_directory_path() / "spirula_mask_doc_test" / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

// A deterministic 0/255 mask: a filled ellipse of keep on a dropped
// background, plus speckles from a fixed LCG so no two frames are equal.
std::vector<uint8_t> synth_mask(int w, int h, uint32_t seed) {
    std::vector<uint8_t> px((size_t)w * h, 0);
    const float cx = 0.5f * w, cy = 0.5f * h, rx = 0.38f * w, ry = 0.42f * h;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.0f) px[(size_t)y * w + x] = 255;
        }
    uint32_t s = seed * 2654435761u + 12345u;
    for (int k = 0; k < w * h / 64; k++) {
        s = s * 1664525u + 1013904223u;
        const size_t i = (size_t)(s % (uint32_t)(w * h));
        px[i] = 255 - px[i];
    }
    return px;
}

std::vector<uint8_t> synth_rgb(int w, int h, uint32_t seed) {
    std::vector<uint8_t> px((size_t)w * h * 3);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t* p = &px[((size_t)y * w + x) * 3];
            p[0] = (uint8_t)((x * 255) / std::max(1, w - 1));
            p[1] = (uint8_t)((y * 255) / std::max(1, h - 1));
            p[2] = (uint8_t)((x + y + (int)seed) & 255);
        }
    return px;
}

bool write_png_gray(const fs::path& p, int w, int h, const std::vector<uint8_t>& px) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    return stbi_write_png(p.string().c_str(), w, h, 1, px.data(), w) != 0;
}

bool write_jpg_rgb(const fs::path& p, int w, int h, const std::vector<uint8_t>& px) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    return stbi_write_jpg(p.string().c_str(), w, h, 3, px.data(), 90) != 0;
}

// write_jpg_rgb with an Exif APP1 carrying `orientation` spliced in after SOI:
// stbi writes none, so every other session fixture has the identity turn.
bool write_jpg_rgb_oriented(const fs::path& p, int w, int h, const std::vector<uint8_t>& px,
                            int orientation) {
    std::vector<uint8_t> jpg;
    stbi_write_jpg_to_func(
        [](void* ctx, void* data, int n) {
            auto* out = static_cast<std::vector<uint8_t>*>(ctx);
            out->insert(out->end(), (const uint8_t*)data, (const uint8_t*)data + n);
        },
        &jpg, w, h, 3, px.data(), 90);
    if (jpg.size() < 2) return false;
    std::vector<uint8_t> seg = {0xFF, 0xE1, 0, 0, 'E', 'x', 'i', 'f', 0, 0,
                                'I', 'I', 42, 0, 8, 0, 0, 0,          // TIFF, IFD0 at 8
                                1, 0, 0x12, 0x01, 3, 0, 1, 0, 0, 0,   // Orientation, SHORT
                                (uint8_t)orientation, 0, 0, 0, 0, 0, 0, 0};
    seg[2] = (uint8_t)((seg.size() - 2) >> 8);
    seg[3] = (uint8_t)(seg.size() - 2);
    jpg.insert(jpg.begin() + 2, seg.begin(), seg.end());
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    return mk::write_file_atomic(p.string(), jpg.data(), jpg.size());
}

std::vector<uint8_t> file_bytes(const fs::path& p) {
    std::vector<uint8_t> out;
    mk::read_file(p.string(), out);
    return out;
}

// A dataset: images/<key>.jpg and masks/<key>.png for every key given, all
// w x h, each frame's mask seeded by its index.
struct Fixture {
    fs::path root, images, masks, layer;
    std::vector<std::string> keys;
    int w = 0, h = 0;
};

Fixture make_dataset(const char* name, int w, int h,
                     const std::vector<std::string>& keys, bool with_masks = true) {
    Fixture f;
    f.root = scratch(name);
    f.images = f.root / "images";
    f.masks = f.root / "masks";
    f.layer = f.root / mk::kLayerDirName;
    f.keys = keys;
    f.w = w;
    f.h = h;
    for (size_t i = 0; i < keys.size(); i++) {
        write_jpg_rgb(f.images / (keys[i] + ".jpg"), w, h, synth_rgb(w, h, (uint32_t)i));
        if (with_masks)
            write_png_gray(f.masks / (keys[i] + ".png"), w, h, synth_mask(w, h, (uint32_t)i));
    }
    return f;
}

// ---------------------------------------------------------------------------
// FNV-1a
// ---------------------------------------------------------------------------

void test_fnv() {
    // Published FNV-1a 64 test vectors.
    check(mk::fnv1a64(nullptr, 0) == 0xcbf29ce484222325ull, "fnv1a64 of empty");
    const char* a = "a";
    check(mk::fnv1a64((const uint8_t*)a, 1) == 0xaf63dc4c8601ec8cull, "fnv1a64 of \"a\"");
    const char* foobar = "foobar";
    check(mk::fnv1a64((const uint8_t*)foobar, 6) == 0x85944171f73967e8ull,
          "fnv1a64 of \"foobar\"");
    check(mk::fnv_hex(0xcbf29ce484222325ull) == "cbf29ce484222325", "fnv_hex pads to 16");
    check(mk::fnv_hex(1) == "0000000000000001", "fnv_hex leading zeros");
    uint64_t v = 0;
    check(mk::fnv_parse("cbf29ce484222325", v) && v == 0xcbf29ce484222325ull, "fnv_parse");
    check(!mk::fnv_parse("cbf29ce48422232", v), "fnv_parse rejects 15 chars");
    check(!mk::fnv_parse("cbf29ce48422232g", v), "fnv_parse rejects non-hex");
    check(mk::fnv1a64((const uint8_t*)a, 1) != mk::fnv1a64((const uint8_t*)"b", 1),
          "different bytes differ");
}

// ---------------------------------------------------------------------------
// Composite truth table, keys, paths
// ---------------------------------------------------------------------------

void test_composite_truth_table() {
    // Every (base, drop, keep) state. Polarity: 255 = keep.
    const uint8_t base[8] = {0, 0, 0, 0, 255, 255, 255, 255};
    const uint8_t drop[8] = {0, 255, 0, 255, 0, 255, 0, 255};
    const uint8_t keep[8] = {0, 0, 255, 255, 0, 0, 255, 255};
    uint8_t out[8];
    mk::composite(base, drop, keep, 8, out);
    const uint8_t want[8] = {0, 0, 255, 255, 255, 0, 255, 255};
    for (int i = 0; i < 8; i++)
        check(out[i] == want[i], "composite state " + std::to_string(i));
    // Absent layers mean "no correction".
    mk::composite(base, nullptr, nullptr, 8, out);
    for (int i = 0; i < 8; i++)
        check(out[i] == base[i], "composite with no layers is the base, " + std::to_string(i));
    mk::composite(base, drop, nullptr, 8, out);
    check(out[5] == 0 && out[4] == 255, "drop only");
    mk::composite(base, nullptr, keep, 8, out);
    check(out[2] == 255 && out[0] == 0, "keep only");
}

void test_keys_and_paths() {
    const std::string root = "/data/set/images";
    check(mk::frame_key(root, "/data/set/images/00023.jpg") == "00023", "key at root");
    check(mk::frame_key(root, "/data/set/images/cam0/00023.jpg") == "cam0/00023", "key in camera");
    check(mk::frame_key(root + "/", "/data/set/images/cam0/left/x.png") == "cam0/left/x",
          "key with trailing slash on root");
    // lexically_relative does not normalize its own argument, so a dot
    // segment in image_root (not the file) needs normalize_dir's call.
    check(mk::frame_key("/data/set/other/../images", "/data/set/images/cam0/00023.jpg") ==
              "cam0/00023",
          "key with dot segment in root");
    check(mk::frame_key(root, "/elsewhere/y.jpg") == "y", "key outside root is the stem");
    check(mk::normalize_dir("/a/b/") == "/a/b", "normalize_dir strips the slash");
    check(mk::normalize_dir("/a/./b/../c") == "/a/c", "normalize_dir is lexical");
    check(mk::mask_file("/data/set/masks", "cam0/00023") == "/data/set/masks/cam0/00023.png",
          "mask_file");
    check(mk::layer_file("/data/set/mask_edits", "cam0/00023", mk::Layer::Base) ==
              "/data/set/mask_edits/cam0/00023.base.png", "layer_file base");
    check(mk::layer_file("/data/set/mask_edits", "00023", mk::Layer::Drop) ==
              "/data/set/mask_edits/00023.drop.png", "layer_file drop");
    check(mk::layer_file("/data/set/mask_edits", "00023", mk::Layer::Keep) ==
              "/data/set/mask_edits/00023.keep.png", "layer_file keep");
}

// ---------------------------------------------------------------------------
// PNG bytes, atomic write, index round trip
// ---------------------------------------------------------------------------

void test_png_and_atomic_write() {
    const fs::path d = scratch("png");
    const std::vector<uint8_t> px = synth_mask(64, 48, 7);
    std::vector<uint8_t> png;
    check(mk::encode_gray_png(px.data(), 64, 48, png), "encode_gray_png");
    check(png.size() > 8 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G', "PNG signature");
    const fs::path p = d / "sub" / "a.png";
    check(mk::write_file_atomic(p.string(), png.data(), png.size()), "write_file_atomic creates dirs");
    bool tmp_leftover = false;
    for (const auto& entry : fs::directory_iterator(d / "sub"))
        if (entry.path().extension() == ".tmp") tmp_leftover = true;
    check(!tmp_leftover, "no temp file left behind");
    check(file_bytes(p) == png, "file holds exactly the encoded bytes");
    int w = 0, h = 0;
    std::vector<uint8_t> back;
    check(app::load_stencil(p.string(), w, h, back) && w == 64 && h == 48, "load_stencil reads it");
    check(back == px, "decoded pixels are the input, every one");
    uint64_t fp = 0;
    check(mk::fingerprint_file(p.string(), fp) && fp == mk::fnv1a64(png.data(), png.size()),
          "fingerprint_file is the fingerprint of the bytes");
    // Writing again replaces the directory entry rather than the inode: a
    // hard link to the first file must keep the first bytes.
    const fs::path link = d / "sub" / "link.png";
    std::error_code ec;
    fs::create_hard_link(p, link, ec);
    if (!ec) {
        const std::vector<uint8_t> other = {1, 2, 3, 4};
        check(mk::write_file_atomic(p.string(), other.data(), other.size()), "rewrite");
        check(file_bytes(link) == png, "hard link still holds the old bytes");
        check(file_bytes(p) == other, "path holds the new bytes");
    }
    check(!mk::fingerprint_file((d / "missing.png").string(), fp), "fingerprint of a missing file fails");
}

// A fixed temp name (<dst>.tmp) lets two writers of the same destination
// fopen() the same inode and interleave writes before either renames. The
// property that rules that out: every call gets its own sibling name.
void test_temp_write_path_unique() {
    const fs::path d = scratch("temp_names");
    const fs::path dst = d / "shared.bin";
    const std::string a = mk::temp_write_path(dst.string());
    const std::string b = mk::temp_write_path(dst.string());
    check(a != b, "temp_write_path differs across calls for the same destination");
    check(fs::path(a).parent_path() == dst.parent_path() &&
              fs::path(b).parent_path() == dst.parent_path(),
          "temp paths are siblings of the destination");
    // Concurrent-in-flight writers do not collide on disk, and the write
    // that lands last still wins cleanly -- the hard-link guarantee holds.
    const std::vector<uint8_t> first = {9, 9, 9};
    check(mk::write_file_atomic(dst.string(), first.data(), first.size()), "first write lands");
    const fs::path link = d / "shared.link";
    std::error_code ec;
    fs::create_hard_link(dst, link, ec);
    if (!ec) {
        const std::vector<uint8_t> second = {1, 2};
        check(mk::write_file_atomic(dst.string(), second.data(), second.size()), "second write lands");
        check(file_bytes(link) == first, "hard link unaffected by the second write");
        check(file_bytes(dst) == second, "destination holds the second write");
    }
}

void test_index_roundtrip() {
    const fs::path d = scratch("index");
    mk::LayerIndex idx;
    std::string err;
    check(idx.load(d.string(), err) && idx.frames.empty(), "absent index loads empty");
    idx.mask_root = "/data/set/masks";
    mk::IndexEntry e;
    e.base_fp = 0xcbf29ce484222325ull;
    e.composite_fp = 0x85944171f73967e8ull;
    e.kept = 0.4375f;
    e.saved_at = "2026-09-21T10:00:00Z";
    idx.frames["cam0/00023"] = e;
    idx.frames["00001"] = mk::IndexEntry{};
    check(idx.save(d.string(), err), "index saves: " + err);
    check(fs::exists(d / mk::kIndexFileName), "index.json exists");
    mk::LayerIndex back;
    check(back.load(d.string(), err), "index loads: " + err);
    check(back.mask_root == "/data/set/masks", "mask_root round trips");
    check(back.frames.size() == 2, "two entries");
    const mk::IndexEntry& r = back.frames["cam0/00023"];
    check(r.base_fp == e.base_fp, "base fingerprint round trips exactly (64 bits)");
    check(r.composite_fp == e.composite_fp, "composite fingerprint round trips exactly");
    check(r.kept == e.kept, "kept round trips");
    check(r.saved_at == e.saved_at, "saved_at round trips");
    check(back.frames["00001"].base_fp == 0 && back.frames["00001"].composite_fp == 0,
          "zero fingerprints round trip");
    const std::vector<uint8_t> text = file_bytes(d / mk::kIndexFileName);
    const std::string s(text.begin(), text.end());
    check(s.find("\"85944171f73967e8\"") != std::string::npos, "fingerprints are hex strings");
    check(s.find("spirula_mask_edits") != std::string::npos, "marker key present");
    // Garbage is an error, not an empty index.
    const std::vector<uint8_t> junk = {'{', 'x'};
    mk::write_file_atomic((d / mk::kIndexFileName).string(), junk.data(), junk.size());
    check(!back.load(d.string(), err) && !err.empty(), "corrupt index fails with a message");
    check(mk::utc_now_iso().size() == 20 && mk::utc_now_iso()[10] == 'T', "utc_now_iso shape");
}

// ---------------------------------------------------------------------------
// Save, fingerprint decides, revert byte-exact
// ---------------------------------------------------------------------------

std::vector<uint8_t> box_layer(int w, int h, int x0, int y0, int x1, int y1) {
    std::vector<uint8_t> v((size_t)w * h, 0);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) v[(size_t)y * w + x] = 255;
    return v;
}

void test_save_and_layers_roundtrip() {
    Fixture f = make_dataset("save", 64, 48, {"a", "cam0/b"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> original = file_bytes(f.masks / "a.png");
    int w = 0, h = 0;
    std::vector<uint8_t> base;
    app::load_stencil((f.masks / "a.png").string(), w, h, base);
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    check(mk::base_state(mask_root, "a", idx) == mk::BaseState::Unedited, "unedited before save");
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, base.data(), drop.data(),
                         keep.data(), true, idx, err), "save_frame: " + err);
    // The base is a byte copy of the mask as the run wrote it.
    check(file_bytes(f.layer / "a.base.png") == original, "base is byte-identical to the original");
    // The layers read back as the same sets, through the app's own reader.
    mk::FrameLayers back;
    std::string warn;
    check(mk::read_layers(layer_root, "a", 64, 48, back, warn) && warn.empty(), "read_layers");
    check(back.drop == drop, "drop layer round trips");
    check(back.keep == keep, "keep layer round trips");
    int lw, lh;
    std::vector<uint8_t> raw;
    app::load_stencil((f.layer / "a.drop.png").string(), lw, lh, raw);
    check(raw == drop, "load_stencil reads .drop.png as the same set");
    // The layer files are 0/255 and nothing else.
    std::vector<uint8_t> png_px;
    check(app::load_stencil((f.layer / "a.keep.png").string(), lw, lh, png_px), "keep decodes");
    bool binary = true;
    for (uint8_t v : back.keep) binary = binary && (v == 0 || v == 255);
    check(binary, "keep layer is binary");
    // masks/a.png is now the composite.
    std::vector<uint8_t> comp;
    app::load_stencil((f.masks / "a.png").string(), lw, lh, comp);
    std::vector<uint8_t> want(base.size());
    mk::composite(base.data(), drop.data(), keep.data(), base.size(), want.data());
    check(comp == want, "masks/a.png holds the composite");
    // Index entry.
    check(idx.frames.count("a") == 1, "index has the entry");
    uint64_t fp = 0;
    mk::fingerprint_file((f.masks / "a.png").string(), fp);
    check(idx.frames["a"].composite_fp == fp, "composite fingerprint is the file's");
    mk::fingerprint_file((f.layer / "a.base.png").string(), fp);
    check(idx.frames["a"].base_fp == fp, "base fingerprint is the file's");
    size_t kept = 0;
    for (uint8_t v : want) kept += v ? 1 : 0;
    check(std::abs(idx.frames["a"].kept - (float)kept / (64.0f * 48.0f)) < 1e-6f, "kept fraction recorded");
    check(mk::base_state(mask_root, "a", idx) == mk::BaseState::Unchanged, "unchanged after save");
    mk::LayerIndex reloaded;
    check(reloaded.load(layer_root, err) && reloaded.frames.count("a") == 1, "index was written to disk");
    // A second save keeps the base as it was (never re-copied over).
    const std::vector<uint8_t> drop2 = box_layer(64, 48, 0, 0, 2, 2);
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, base.data(), drop2.data(),
                         keep.data(), true, idx, err), "second save");
    check(file_bytes(f.layer / "a.base.png") == original, "base untouched by a second save");
    // Nested key lands nested.
    check(mk::save_frame(layer_root, mask_root, "cam0/b", 64, 48, base.data(), drop.data(),
                         keep.data(), true, idx, err), "save nested key");
    check(fs::exists(f.layer / "cam0" / "b.drop.png"), "nested layer path");
    // A layer of another size is skipped and named.
    write_png_gray(f.layer / "a.keep.png", 32, 24, std::vector<uint8_t>(32 * 24, 255));
    mk::FrameLayers mism;
    check(!mk::read_layers(layer_root, "a", 64, 48, mism, warn) && !warn.empty(),
          "size mismatch is reported");
    check(mism.keep.size() == 64 * 48 && mism.keep[0] == 0, "mismatched layer reads as zero");
    check(mism.drop == drop2, "the other layer still reads");
    // Both wrong: both are named, not only the second.
    write_png_gray(f.layer / "a.drop.png", 32, 24, std::vector<uint8_t>(32 * 24, 255));
    warn.clear();
    check(!mk::read_layers(layer_root, "a", 64, 48, mism, warn) &&
              warn.find("a.drop.png") != std::string::npos &&
              warn.find("a.keep.png") != std::string::npos,
          "both mismatched layers are named in the warning");
}

void test_fingerprint_decides() {
    Fixture f = make_dataset("rebase", 64, 48, {"a", "b"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    int w, h;
    std::vector<uint8_t> base_a;
    app::load_stencil((f.masks / "a.png").string(), w, h, base_a);
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, base_a.data(), drop.data(),
                         keep.data(), true, idx, err), "save a");
    const std::vector<uint8_t> composite_bytes = file_bytes(f.masks / "a.png");

    // Same bytes: nothing regenerated, nothing touched.
    mk::BaseState st;
    check(mk::recomposite_frame(layer_root, mask_root, "a", idx, st, err) &&
              st == mk::BaseState::Unchanged, "same bytes are Unchanged");
    check(mk::recomposite_all(layer_root, err) == 0, "recomposite_all over an unchanged set is 0");

    // Different bytes: the run regenerated it. The NEW mask must become the
    // base, byte for byte, and the composite must be the layers over it.
    const std::vector<uint8_t> new_mask = synth_mask(64, 48, 99);
    write_png_gray(f.masks / "a.png", 64, 48, new_mask);
    const std::vector<uint8_t> new_bytes = file_bytes(f.masks / "a.png");
    check(new_bytes != composite_bytes, "fixture: the rewrite really differs");
    check(mk::base_state(mask_root, "a", idx) == mk::BaseState::Regenerated, "Regenerated detected");
    check(mk::recomposite_all(layer_root, err) == 1, "recomposite_all re-based one: " + err);
    check(file_bytes(f.layer / "a.base.png") == new_bytes, ".base.png is the regenerated file, byte for byte");
    std::vector<uint8_t> comp;
    app::load_stencil((f.masks / "a.png").string(), w, h, comp);
    std::vector<uint8_t> want(comp.size());
    mk::composite(new_mask.data(), drop.data(), keep.data(), want.size(), want.data());
    check(comp == want, "composite is the layers over the NEW base");
    mk::LayerIndex re;
    re.load(layer_root, err);
    uint64_t fp;
    mk::fingerprint_file((f.masks / "a.png").string(), fp);
    check(re.frames["a"].composite_fp == fp, "index updated with the new composite fingerprint");
    check(re.frames["a"].base_fp == mk::fnv1a64(new_bytes.data(), new_bytes.size()),
          "index updated with the new base fingerprint");
    // The re-mask failure signature: a base equal to a previous composite.
    check(file_bytes(f.layer / "a.base.png") != composite_bytes, "no base equals an old composite");

    // Missing: a cancelled re-run. Layers stay, nothing is written.
    fs::remove(f.masks / "a.png");
    check(mk::base_state(mask_root, "a", idx) == mk::BaseState::Missing, "Missing detected");
    check(mk::recomposite_all(layer_root, err) == 0, "missing is not re-based");
    check(fs::exists(f.layer / "a.drop.png") && fs::exists(f.layer / "a.keep.png"), "layers kept");
    check(!fs::exists(f.masks / "a.png"), "no mask conjured");

    // A layer at another size refuses to re-base and reports, and does not
    // touch the base. "a" is the batch's only entry and it failed, so the
    // honest count is 0 rebased, not -1, reported through `err`.
    write_png_gray(f.masks / "a.png", 64, 48, synth_mask(64, 48, 5));
    write_png_gray(f.layer / "a.drop.png", 32, 24, std::vector<uint8_t>(32 * 24, 255));
    const std::vector<uint8_t> base_before = file_bytes(f.layer / "a.base.png");
    // The batch error is "<key>: <why>" per failure; a bare find("a") matched the scratch path.
    check(mk::recomposite_all(layer_root, err) == 0 && err.rfind("a: ", 0) == 0 &&
              err.find("; ") == std::string::npos,
          "mismatch refuses without a batch abort, naming key a alone: " + err);
    check(file_bytes(f.layer / "a.base.png") == base_before, "base untouched on refusal");
}

void test_revert_is_byte_exact() {
    Fixture f = make_dataset("revert", 64, 48, {"a", "b"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> original_a = file_bytes(f.masks / "a.png");
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "b.png");
    int w, h;
    std::vector<uint8_t> base_a, base_b;
    app::load_stencil((f.masks / "a.png").string(), w, h, base_a);
    app::load_stencil((f.masks / "b.png").string(), w, h, base_b);
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    mk::save_frame(layer_root, mask_root, "a", 64, 48, base_a.data(), drop.data(), keep.data(), true, idx, err);
    mk::save_frame(layer_root, mask_root, "b", 64, 48, base_b.data(), keep.data(), drop.data(), true, idx, err);
    check(file_bytes(f.masks / "a.png") != original_a, "fixture: a was changed by the save");
    check(mk::revert_frame(layer_root, mask_root, "a", idx, err), "revert_frame: " + err);
    check(file_bytes(f.masks / "a.png") == original_a, "masks/a.png is byte-identical to the original");
    for (const char* tag : {"a.base.png", "a.drop.png", "a.keep.png"})
        check(!fs::exists(f.layer / tag), std::string("removed ") + tag);
    check(idx.frames.count("a") == 0, "entry removed");
    check(idx.frames.count("b") == 1 && fs::exists(f.layer / "b.base.png"), "b untouched");
    mk::LayerIndex disk;
    disk.load(layer_root, err);
    check(disk.frames.count("a") == 0 && disk.frames.count("b") == 1, "index on disk agrees");
    check(mk::revert_all(layer_root, err) == 1, "revert_all reverts the remaining one");
    check(file_bytes(f.masks / "b.png") == original_b, "masks/b.png byte-identical after revert_all");
    disk.load(layer_root, err);
    check(disk.frames.empty(), "index empty after revert_all");
}

void test_save_without_mask() {
    // A frame with no mask on disk: layers and an entry, no base, no composite.
    Fixture f = make_dataset("nomask", 64, 48, {"a"}, /*with_masks=*/false);
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> all_keep(64 * 48, 255);
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> none(64 * 48, 0);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    check(mk::base_state(mask_root, "a", idx) == mk::BaseState::Missing, "no mask is Missing");
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, all_keep.data(), drop.data(),
                         none.data(), /*write_composite=*/false, idx, err), "save layers only: " + err);
    check(fs::exists(f.layer / "a.drop.png") && !fs::exists(f.layer / "a.base.png"), "layers, no base");
    check(!fs::exists(f.masks / "a.png"), "no composite written");
    check(idx.frames["a"].base_fp == 0 && idx.frames["a"].composite_fp == 0, "zero fingerprints");
    // The run then writes the mask: it is a new base, and the layers apply.
    const std::vector<uint8_t> m = synth_mask(64, 48, 3);
    write_png_gray(f.masks / "a.png", 64, 48, m);
    check(mk::recomposite_all(layer_root, err) == 1, "the arriving mask is re-based: " + err);
    std::vector<uint8_t> comp;
    int w, h;
    app::load_stencil((f.masks / "a.png").string(), w, h, comp);
    std::vector<uint8_t> want(comp.size());
    mk::composite(m.data(), drop.data(), nullptr, want.size(), want.data());
    check(comp == want, "layers applied over the arriving mask");
    // Revert of an entry that never had a base deletes the layers only.
    fs::remove(f.layer / "a.base.png");
    idx.load(layer_root, err);
    check(mk::revert_frame(layer_root, mask_root, "a", idx, err), "revert without base");
    check(fs::exists(f.masks / "a.png") && !fs::exists(f.layer / "a.drop.png"), "mask left, layers gone");
}

// ---------------------------------------------------------------------------
// A removal failure must not report success, and one bad frame must not
// stop the rest of a batch.
// ---------------------------------------------------------------------------

// fs::remove refuses a non-empty directory everywhere -- a portable stand-in
// for revert's removal step failing, without a platform-specific immutable
// flag (the reviewer used chflags uchg, macOS-only).
void test_revert_reports_removal_failure() {
    Fixture f = make_dataset("revert_fail", 64, 48, {"a"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    int w, h;
    std::vector<uint8_t> base_a;
    app::load_stencil((f.masks / "a.png").string(), w, h, base_a);
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, base_a.data(), drop.data(),
                         keep.data(), true, idx, err), "save before forcing a removal failure");

    const fs::path drop_path = f.layer / "a.drop.png";
    std::error_code ec;
    fs::remove(drop_path, ec);
    fs::create_directories(drop_path / "nested", ec);
    check(fs::is_directory(drop_path) && !fs::is_empty(drop_path),
          "fixture: a.drop.png is a non-empty directory fs::remove refuses");

    check(!mk::revert_frame(layer_root, mask_root, "a", idx, err) && !err.empty(),
          "revert_frame reports the removal failure: " + err);
    check(err == drop_path.string(),
          "the error is the path list and nothing else -- err_write supplies the sentence");
    // The invariant that matters: no layer files on disk with no entry.
    check(idx.frames.count("a") == 1, "index entry kept -- an orphan file must keep its owner");
    mk::LayerIndex disk;
    disk.load(layer_root, err);
    check(disk.frames.count("a") == 1, "index on disk still names the frame");
    check(fs::exists(drop_path), "the undeletable layer path is still there, and still tracked");
}

void test_recomposite_all_continues_past_failure() {
    Fixture f = make_dataset("recomp_continue", 64, 48, {"a", "b", "c"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    for (const char* k : {"a", "b", "c"}) {
        int w, h;
        std::vector<uint8_t> base;
        app::load_stencil((f.masks / (std::string(k) + ".png")).string(), w, h, base);
        check(mk::save_frame(layer_root, mask_root, k, 64, 48, base.data(), drop.data(),
                             keep.data(), true, idx, err), std::string("save ") + k);
    }
    const std::vector<uint8_t> base_b_before = file_bytes(f.layer / "b.base.png");

    // All three are regenerated by "the run"; "b", the middle key in sorted
    // order, is the one whose layer is sized wrong and must refuse.
    for (const char* k : {"a", "b", "c"})
        write_png_gray(f.masks / (std::string(k) + ".png"), 64, 48, synth_mask(64, 48, 42));
    write_png_gray(f.layer / "b.drop.png", 32, 24, std::vector<uint8_t>(32 * 24, 255));

    const int rebased = mk::recomposite_all(layer_root, err);
    check(rebased == 2, "a and c rebase despite b's refusal, got " + std::to_string(rebased));
    check(err.rfind("b: ", 0) == 0 && err.find("; ") == std::string::npos,
          "the batch error is exactly one entry, keyed b: " + err);
    // recomposite_all loads its own index off disk; read that back rather
    // than the caller's now-stale copy.
    mk::LayerIndex disk;
    check(disk.load(layer_root, err), "reload the persisted index: " + err);
    check(mk::base_state(mask_root, "a", disk) == mk::BaseState::Unchanged &&
              mk::base_state(mask_root, "c", disk) == mk::BaseState::Unchanged,
          "a and c are durably rebased, not just counted");
    check(mk::base_state(mask_root, "b", disk) == mk::BaseState::Regenerated,
          "b is still pending -- it was skipped, not silently marked done");
    check(file_bytes(f.layer / "b.base.png") == base_b_before,
          "b's base untouched: refusing one frame must not corrupt it");
}

// revert_all, like recomposite_all, must not abort on the first failing
// frame.
void test_revert_all_continues_past_failure() {
    Fixture f = make_dataset("revert_continue", 64, 48, {"a", "b", "c"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> original_a = file_bytes(f.masks / "a.png");
    const std::vector<uint8_t> original_c = file_bytes(f.masks / "c.png");
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    for (const char* k : {"a", "b", "c"}) {
        int w, h;
        std::vector<uint8_t> base;
        app::load_stencil((f.masks / (std::string(k) + ".png")).string(), w, h, base);
        check(mk::save_frame(layer_root, mask_root, k, 64, 48, base.data(), drop.data(),
                             keep.data(), true, idx, err), std::string("save ") + k);
    }
    check(file_bytes(f.masks / "a.png") != original_a && file_bytes(f.masks / "c.png") != original_c,
          "fixture: a and c were changed by the save");

    // "b", the middle key in sorted order, cannot have its .drop.png removed.
    const fs::path drop_b = f.layer / "b.drop.png";
    std::error_code ec;
    fs::remove(drop_b, ec);
    fs::create_directories(drop_b / "nested", ec);
    check(fs::is_directory(drop_b) && !fs::is_empty(drop_b),
          "fixture: b.drop.png is a non-empty directory fs::remove refuses");

    const int reverted = mk::revert_all(layer_root, err);
    check(reverted == 2, "a and c revert despite b's refusal, got " + std::to_string(reverted));
    check(err.rfind("b: ", 0) == 0 && err.find("; ") == std::string::npos,
          "the batch error is exactly one entry, keyed b: " + err);
    check(file_bytes(f.masks / "a.png") == original_a, "a genuinely reverted, not just counted");
    check(file_bytes(f.masks / "c.png") == original_c, "c genuinely reverted -- attempted past b");
    for (const char* tag : {"a.base.png", "a.drop.png", "a.keep.png",
                            "c.base.png", "c.drop.png", "c.keep.png"})
        check(!fs::exists(f.layer / tag), std::string("removed ") + tag);

    mk::LayerIndex disk;
    check(disk.load(layer_root, err), "reload the persisted index: " + err);
    check(disk.frames.count("a") == 0 && disk.frames.count("c") == 0,
          "a and c fully reverted on disk");
    check(disk.frames.count("b") == 1, "b kept its entry -- skipped, not silently marked done");
    check(fs::exists(drop_b), "b's undeletable layer path is still there");
}

// revert_all lacked recomposite_all's empty-mask_root guard: a hand-edited or
// downgraded index.json without mask_root resolves mask_file("", key) to a
// bare "<key>.png", writing outside the dataset relative to the process cwd.
void test_revert_all_guards_empty_mask_root() {
    Fixture f = make_dataset("revert_empty_root", 64, 48, {"a"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    int w, h;
    std::vector<uint8_t> base;
    app::load_stencil((f.masks / "a.png").string(), w, h, base);
    check(mk::save_frame(layer_root, mask_root, "a", 64, 48, base.data(), drop.data(),
                         keep.data(), true, idx, err), "save a: " + err);
    check(fs::exists(f.layer / "a.base.png"), "fixture: a.base.png exists to revert from");
    // save_frame already composited over masks/a.png; that composite, not the
    // pre-save original, is what a no-op revert_all must leave standing.
    const std::vector<uint8_t> composited_a = file_bytes(f.masks / "a.png");

    // What a hand-edited or downgraded index.json leaves: LayerIndex::load
    // clears mask_root before parsing, so a document missing that field (or
    // set to "") loads with the frame entries intact but mask_root empty.
    idx.mask_root.clear();
    check(idx.save(layer_root, err), "persist an index with empty mask_root: " + err);

    const fs::path stray = fs::current_path() / "a.png";
    std::error_code ec;
    fs::remove(stray, ec);

    const int reverted = mk::revert_all(layer_root, err);
    check(reverted == 0, "revert_all on an empty mask_root writes nothing, got " +
                             std::to_string(reverted));
    check(!fs::exists(stray),
          "revert_all did not write a bare '<key>.png' into the working directory");
    check(file_bytes(f.masks / "a.png") == composited_a, "masks/a.png left untouched");
    check(fs::exists(f.layer / "a.base.png"), "layer files untouched -- the guard returns early");
    fs::remove(stray, ec);
}

// ---------------------------------------------------------------------------
// The document
// ---------------------------------------------------------------------------

gui::Stencil box_stencil(int W, int H, int x0, int y0, int x1, int y1) {
    gui::ShapeStroke s;
    s.kind = gui::ShapeKind::Box;
    s.pts = std::vector<float>{(float)x0, (float)y0, (float)x1, (float)y1};
    gui::Stencil st;
    gui::rasterize_shape(s, W, H, st);
    return st;
}

bool exclusive(const mk::MaskDoc& d) {
    for (size_t i = 0; i < d.drop().size(); i++)
        if (d.drop()[i] && d.keep()[i]) return false;
    return true;
}

bool composite_consistent(const mk::MaskDoc& d) {
    std::vector<uint8_t> want(d.base().size());
    mk::composite(d.base().data(), d.drop().data(), d.keep().data(), want.size(), want.data());
    if (want != d.composite()) return false;
    int64_t kept = 0;
    for (uint8_t v : want) kept += v ? 1 : 0;
    return kept == d.kept();
}

void test_doc_load_and_paint() {
    Fixture f = make_dataset("doc", 64, 48, {"a"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(layer_root, mask_root, "a", 64, 48, idx, err, warn), "load: " + err);
    check(d.width() == 64 && d.height() == 48, "size from the mask");
    check(d.base_state() == mk::BaseState::Unedited, "unedited");
    check(d.base() == synth_mask(64, 48, 0), "base is the mask on disk");
    check(d.composite() == d.base() && !d.dirty(), "composite is the base, clean");
    check(composite_consistent(d), "kept count at load");
    const uint64_t rev0 = d.revision();

    // ForceDrop over a keep region. `bounds` deliberately larger than the box.
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 20, 16, 30, 26), mk::Rect{10, 10, 40, 40});
    check(d.dirty() && d.revision() != rev0, "dirty after a paint");
    check(d.drop()[(size_t)20 * 64 + 25] == 255 && d.composite()[(size_t)20 * 64 + 25] == 0,
          "forced drop: layer set and composite 0");
    check(d.drop()[(size_t)20 * 64 + 35] == 0, "outside the box untouched");
    check(exclusive(d) && composite_consistent(d), "invariant after ForceDrop");
    check(d.last_change().x0 <= 20 && d.last_change().x1 >= 30, "last_change covers the stroke");

    // ForceKeep over a dropped region, overlapping the drop box: keep wins,
    // and the overlap leaves the drop layer.
    d.paint(mk::Paint::ForceKeep, box_stencil(64, 48, 25, 20, 60, 44), mk::Rect{0, 0, 64, 48});
    check(d.keep()[(size_t)30 * 64 + 50] == 255 && d.composite()[(size_t)30 * 64 + 50] == 255,
          "forced keep: layer set and composite 255");
    check(d.drop()[(size_t)22 * 64 + 27] == 0 && d.keep()[(size_t)22 * 64 + 27] == 255,
          "overlap: drop cleared where keep painted");
    check(exclusive(d) && composite_consistent(d), "invariant after ForceKeep");

    // ForceDrop back over PART of the still-standing keep region, with no
    // Clear in between: the overlap must clear keep there, not just set drop.
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 40, 25, 55, 35), mk::Rect{0, 0, 64, 48});
    check(d.drop()[(size_t)30 * 64 + 45] == 255 && d.keep()[(size_t)30 * 64 + 45] == 0,
          "ForceDrop over a keep region clears keep, not just sets drop");
    check(d.keep()[(size_t)40 * 64 + 58] == 255, "keep survives outside the new drop box");
    check(exclusive(d) && composite_consistent(d), "invariant after ForceDrop-over-keep overlap");

    // Clear puts both layers back over its stencil.
    d.paint(mk::Paint::Clear, box_stencil(64, 48, 0, 0, 64, 48), mk::Rect{0, 0, 64, 48});
    check(d.drop() == std::vector<uint8_t>(64 * 48, 0) && d.keep() == std::vector<uint8_t>(64 * 48, 0),
          "clear everything");
    check(d.composite() == d.base() && composite_consistent(d), "composite back to the base");

    // A stencil pixel outside `bounds` is ignored, so bounds really bound.
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 0, 0, 64, 48), mk::Rect{0, 0, 8, 8});
    check(d.drop()[0] == 255 && d.drop()[(size_t)10 * 64 + 10] == 0, "bounds clip the stencil");
    check(composite_consistent(d), "kept count tracks a clipped paint");

    // A bounds rectangle that runs off the image edge must be clipped to the
    // canvas before it is used, not just to the stencil.
    d.paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 64, 48), mk::Rect{60, 44, 200, 200});
    check(d.keep()[(size_t)47 * 64 + 63] == 255, "the clipped corner is painted");
    check(d.keep()[(size_t)43 * 64 + 62] == 0 && d.drop()[(size_t)43 * 64 + 62] == 0,
          "just above the off-edge bounds is untouched");
    check(exclusive(d) && composite_consistent(d), "invariant after an off-edge bounds paint");

    // Save through the document, reload, same planes.
    check(d.save(layer_root, mask_root, idx, err), "doc save: " + err);
    check(!d.dirty(), "clean after save");
    mk::MaskDoc e;
    check(e.load(layer_root, mask_root, "a", 64, 48, idx, err, warn), "reload");
    check(e.base_state() == mk::BaseState::Unchanged, "unchanged after our own save");
    check(e.drop() == d.drop() && e.keep() == d.keep() && e.composite() == d.composite(),
          "planes survive a save and load");
    check(mk::clip(mk::Rect{-5, -5, 100, 100}, 64, 48).x1 == 64, "clip");
    check(mk::join(mk::Rect{1, 1, 2, 2}, mk::Rect{5, 5, 9, 9}).x1 == 9, "join");
    check(mk::join(mk::Rect{}, mk::Rect{5, 5, 9, 9}).x0 == 5, "join with empty");
}

void test_doc_without_mask() {
    Fixture f = make_dataset("docnomask", 64, 48, {"a"}, false);
    mk::LayerIndex idx;
    idx.mask_root = f.masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(f.layer.string(), f.masks.string(), "a", 64, 48, idx, err, warn), "load without mask");
    check(d.base_state() == mk::BaseState::Missing, "state Missing");
    check(d.base() == std::vector<uint8_t>(64 * 48, 255), "base is all keep");
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 0, 0, 8, 8), mk::Rect{0, 0, 64, 48});
    check(d.save(f.layer.string(), f.masks.string(), idx, err), "save without mask: " + err);
    check(!fs::exists(f.masks / "a.png") && fs::exists(f.layer / "a.drop.png"), "layers only");
}

// ---------------------------------------------------------------------------
// read_rect/write_rect round trip, and a paint that flips no bytes must not
// dirty the document.
// ---------------------------------------------------------------------------

void test_read_write_rect_roundtrip() {
    Fixture f = make_dataset("rw_rect", 64, 48, {"a"});
    mk::LayerIndex idx;
    idx.mask_root = f.masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(f.layer.string(), f.masks.string(), "a", 64, 48, idx, err, warn), "load");
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 5, 5, 20, 20), mk::Rect{0, 0, 64, 48});
    d.paint(mk::Paint::ForceKeep, box_stencil(64, 48, 15, 15, 40, 40), mk::Rect{0, 0, 64, 48});

    const mk::Rect r{3, 3, 45, 45};
    std::vector<uint8_t> drop_r, keep_r;
    d.read_rect(r, drop_r, keep_r);
    check(drop_r != keep_r, "fixture: the two layers differ inside the rect");

    d.write_rect(r, drop_r.data(), keep_r.data());
    std::vector<uint8_t> drop_r2, keep_r2;
    d.read_rect(r, drop_r2, keep_r2);
    check(drop_r2 == drop_r && keep_r2 == keep_r, "read_rect/write_rect round trip is byte-exact");
}

void test_noop_paint_does_not_dirty() {
    Fixture f = make_dataset("noop_paint", 64, 48, {"a"});
    mk::LayerIndex idx;
    idx.mask_root = f.masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(f.layer.string(), f.masks.string(), "a", 64, 48, idx, err, warn), "load");
    const uint64_t rev0 = d.revision();
    const mk::Rect last0 = d.last_change();

    // Fully off-canvas: box_stencil clips to an empty run, so every pixel is 0.
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 100, 100, 120, 120), mk::Rect{0, 0, 64, 48});
    check(d.revision() == rev0, "a paint that flips no bytes leaves the revision unchanged");
    check(!d.dirty(), "and leaves the doc clean");
    check(!d.can_undo() && d.history_size() == 0, "and pushes no undo entry");
    check(d.last_change().x0 == last0.x0 && d.last_change().x1 == last0.x1 &&
          d.last_change().y0 == last0.y0 && d.last_change().y1 == last0.y1,
          "and reports no change rectangle");
}

// ---------------------------------------------------------------------------
// Undo restores exactly; caps
// ---------------------------------------------------------------------------

void test_undo_redo() {
    Fixture f = make_dataset("undo", 64, 48, {"a"});
    mk::LayerIndex idx;
    idx.mask_root = f.masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(f.layer.string(), f.masks.string(), "a", 64, 48, idx, err, warn), "load");
    const std::vector<uint8_t> drop0 = d.drop(), keep0 = d.keep(), comp0 = d.composite();
    check(!d.can_undo() && !d.can_redo() && d.history_size() == 0, "empty history");

    // 96 strokes, alternating modes, each different.
    for (int k = 0; k < 96; k++) {
        const int x = (k * 7) % 56, y = (k * 5) % 40;
        const mk::Paint mode = k % 3 == 0 ? mk::Paint::ForceDrop
                             : k % 3 == 1 ? mk::Paint::ForceKeep : mk::Paint::Clear;
        d.paint(mode, box_stencil(64, 48, x, y, x + 8, y + 8), mk::Rect{0, 0, 64, 48});
        check(exclusive(d), "invariant after stroke " + std::to_string(k));
    }
    check(d.history_size() == 96 && d.can_undo() && !d.can_redo(), "96 ops held");
    // No cap check here: 96 small strokes fill 0.01% of kMaxHistoryBytes, so it
    // could not fail. test_byte_cap_eviction drives the cap at a reachable size.
    check(d.last_label() == &spirula::i18n::msg::maskedit::op_clear, "last label is the 96th op's");
    // The 95th op (k=94, 94%3==1) is ForceKeep -- separates a ForceDrop/
    // ForceKeep label swap from the all-Clear tail this loop ends on.
    d.undo();
    check(d.last_label() == &spirula::i18n::msg::maskedit::op_keep, "label after undo is the 95th op's");
    d.redo();
    const std::vector<uint8_t> dropN = d.drop(), keepN = d.keep(), compN = d.composite();
    check(dropN != drop0 || keepN != keep0, "fixture: the strokes changed something");

    for (int k = 0; k < 96; k++) d.undo();
    check(!d.can_undo() && d.can_redo(), "at the start of history");
    check(d.drop() == drop0 && d.keep() == keep0, "96 undos restore both layers byte-exact");
    check(d.composite() == comp0 && composite_consistent(d), "composite and count restored");

    for (int k = 0; k < 96; k++) d.redo();
    check(d.drop() == dropN && d.keep() == keepN && d.composite() == compN, "96 redos replay exactly");
    check(composite_consistent(d), "count after redo");

    // Undo, then a new stroke truncates the redo branch.
    d.undo();
    d.undo();
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 1, 1, 3, 3), mk::Rect{0, 0, 64, 48});
    check(!d.can_redo() && d.history_size() == 95, "redo branch truncated");

    // The 97th op evicts the oldest: still 96, and undo bottoms out early.
    // Alternating mode keeps both strokes real changes -- a repeated identical
    // stroke is a no-op and would push only one.
    d.redo();
    for (int k = 0; k < 2; k++)
        d.paint(k == 0 ? mk::Paint::ForceKeep : mk::Paint::ForceDrop,
                box_stencil(64, 48, 2, 2, 4, 4), mk::Rect{0, 0, 64, 48});
    check(d.history_size() == mk::kMaxHistoryOps, "op cap holds at 96");
    // last_change reports the undone rectangle.
    d.undo();
    check(d.last_change().x0 <= 2 && d.last_change().x1 >= 4, "last_change after undo");
}

// ---------------------------------------------------------------------------
// The byte cap must evict independently of the op-count cap.
// ---------------------------------------------------------------------------

void test_byte_cap_eviction() {
    Fixture f = make_dataset("bytecap", 64, 48, {"a"});
    mk::LayerIndex idx;
    idx.mask_root = f.masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    check(d.load(f.layer.string(), f.masks.string(), "a", 64, 48, idx, err, warn), "load");
    const std::vector<uint8_t> drop0 = d.drop(), keep0 = d.keep();
    const mk::Rect full{0, 0, 64, 48};

    // A's own before/after RLE size, computed the same way StrokeOp does, so
    // the check below pins an exact value rather than an inequality a wrong
    // byte count could still satisfy.
    std::vector<uint8_t> bd, bk;
    d.read_rect(full, bd, bk);
    d.paint(mk::Paint::ForceDrop, box_stencil(64, 48, 4, 4, 12, 12), full);
    check(exclusive(d) && composite_consistent(d), "invariant after A");
    std::vector<uint8_t> ad, ak;
    d.read_rect(full, ad, ak);
    const size_t want_bytes_a = gui::rle_encode(bd).size() + gui::rle_encode(bk).size() +
                                 gui::rle_encode(ad).size() + gui::rle_encode(ak).size();
    check(d.history_bytes() == want_bytes_a,
          "history_bytes after one push equals its own RLE size, not an under-count");
    check(d.history_size() == 1 && d.can_undo(), "one op recorded so far");
    const std::vector<uint8_t> dropAfterA = d.drop(), keepAfterA = d.keep();

    // A cap just over A's own size forces B's push to evict A on bytes alone;
    // 2 ops sits far under kMaxHistoryOps, so the op-count cap never fires.
    d.set_history_byte_cap_for_test(want_bytes_a + 1);
    d.paint(mk::Paint::ForceKeep, box_stencil(64, 48, 30, 20, 45, 35), full);
    check(exclusive(d) && composite_consistent(d), "invariant after B");
    std::vector<uint8_t> bd2, bk2;
    d.read_rect(full, bd2, bk2);
    const size_t want_bytes_b = gui::rle_encode(dropAfterA).size() + gui::rle_encode(keepAfterA).size() +
                                 gui::rle_encode(bd2).size() + gui::rle_encode(bk2).size();
    check(d.history_bytes() == want_bytes_b,
          "history_bytes after the eviction equals B's own RLE size, not a stale total");
    check(d.history_size() == 1, "the byte cap evicted one op, well under the 96-op cap");
    check(d.can_undo(), "the newest op (B) is still undoable");

    d.undo();
    check(d.drop() == dropAfterA && d.keep() == keepAfterA,
          "undo lands on the state after A, before B -- B's entry survived the eviction");
    check(d.drop() != drop0 || d.keep() != keep0,
          "and NOT on the pristine original -- A's own undo entry is gone, not B's");
    check(!d.can_undo(), "no further undo: the evicted op cannot be recovered");
    check(exclusive(d) && composite_consistent(d), "invariant holds across the eviction boundary");
}

// ---------------------------------------------------------------------------
// Orientation mapping vs orient_pixels
// ---------------------------------------------------------------------------

void test_orientation_mapping() {
    const int W = 64, H = 48;   // stored
    for (int o = 1; o <= 8; o++) {
        const sfm::ExifTransform t = sfm::exifTransform(o);
        int dw = W, dh = H;
        spirula::oriented_size(t.turns_cw, dw, dh);
        // Three probe pixels; orient_pixels says where each lands.
        const int probes[3][2] = {{0, 0}, {W - 1, H - 1}, {10, 42}};
        for (const auto& p : probes) {
            std::vector<uint8_t> stored((size_t)W * H, 0), shown((size_t)dw * dh, 0);
            stored[(size_t)p[1] * W + p[0]] = 255;
            spirula::orient_pixels(stored.data(), W, H, 1, t.turns_cw, t.mirror, shown.data());
            int found_x = -1, found_y = -1;
            for (int y = 0; y < dh; y++)
                for (int x = 0; x < dw; x++)
                    if (shown[(size_t)y * dw + x]) { found_x = x; found_y = y; }
            int sx, sy;
            mk::to_stored(t, W, H, found_x, found_y, sx, sy);
            check(sx == p[0] && sy == p[1],
                  "to_stored inverts orient_pixels, orientation " + std::to_string(o) +
                      " probe " + std::to_string(p[0]) + "," + std::to_string(p[1]));
            int dx, dy;
            mk::to_displayed(t, W, H, sx, sy, dx, dy);
            check(dx == found_x && dy == found_y, "to_displayed agrees, orientation " + std::to_string(o));
            // The continuous form at the pixel centre lands in the same pixel.
            float fx, fy;
            mk::to_stored(t, W, H, found_x + 0.5f, found_y + 0.5f, fx, fy);
            check((int)std::floor(fx) == p[0] && (int)std::floor(fy) == p[1],
                  "continuous form at the centre, orientation " + std::to_string(o));
        }
        // A one-point brush paints that stored pixel plus its 4 edge
        // neighbours only: rasterize_shape clamps radius to 1.0 and
        // stamps `<= r*r` (SelectShape.cpp:20-33,:133-135) -- a plus disc.
        {
            const int dx = 5, dy = 10;
            gui::ShapeStroke s;
            s.kind = gui::ShapeKind::Brush;
            s.brush_radius = 0.4f;
            s.pts = std::vector<float>{dx + 0.5f, dy + 0.5f};
            const gui::ShapeStroke st = mk::stroke_to_stored(s, t, W, H);
            gui::Stencil sten;
            gui::rasterize_shape(st, W, H, sten);
            int sx, sy;
            mk::to_stored(t, W, H, dx, dy, sx, sy);
            size_t count = 0, adjacent = 0;
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++)
                    if (sten.at(x, y)) {
                        count++;
                        adjacent += std::abs(x - sx) + std::abs(y - sy) <= 1;
                    }
            check(count == 5 && adjacent == 5 && sten.at(sx, sy),
                  "brush point lands on the mapped stored pixel (plus its 4 neighbours), orientation " +
                      std::to_string(o));
            const mk::Rect b = mk::stroke_bounds(st, W, H);
            check(b.x0 <= sx && sx < b.x1 && b.y0 <= sy && sy < b.y1, "bounds contain it");
            const mk::Rect back = mk::rect_to_displayed(mk::Rect{sx, sy, sx + 1, sy + 1}, t, W, H);
            check(back.x0 == dx && back.y0 == dy && back.x1 == dx + 1 && back.y1 == dy + 1,
                  "rect_to_displayed maps a one-pixel rect back, orientation " + std::to_string(o));
        }
        // A genuinely multi-pixel, non-square rect: area and aspect are
        // invariant under any 90-degree turn/mirror, so a corner-crossing
        // bug shows up here as a wrong or negative w()/h(), not just on 1x1.
        {
            const mk::Rect sr{4, 6, 13, 9};   // 9 x 3, stored
            const mk::Rect dr = mk::rect_to_displayed(sr, t, W, H);
            const bool swapped = (t.turns_cw & 1) != 0;
            check(dr.x1 > dr.x0 && dr.y1 > dr.y0,
                  "multi-pixel rect stays ordered, orientation " + std::to_string(o));
            check(dr.w() == (swapped ? sr.h() : sr.w()) && dr.h() == (swapped ? sr.w() : sr.h()),
                  "multi-pixel rect area/aspect preserved, orientation " + std::to_string(o));
        }
    }
    // Orientation 6 (one turn clockwise), the phone-portrait case, by hand:
    // displayed (5, 10) is stored (10, H-1-5) = (10, 42).
    int sx, sy;
    mk::to_stored(sfm::exifTransform(6), W, H, 5, 10, sx, sy);
    check(sx == 10 && sy == 42, "orientation 6 by hand");
    // A multi-pixel rect maps to a rect of the same area.
    const mk::Rect r = mk::rect_to_displayed(mk::Rect{2, 3, 12, 8}, sfm::exifTransform(6), W, H);
    check(r.w() * r.h() == 50 && r.w() == 5 && r.h() == 10, "rect area and turn preserved");
}

// ---------------------------------------------------------------------------
// View math and the window's pixels
// ---------------------------------------------------------------------------

void test_view_math() {
    const int dw = 800, dh = 600;
    const float pw = 400.0f, ph = 400.0f;
    check(std::fabs(mk::fit_scale(dw, dh, pw, ph) - 0.5f) < 1e-6f, "fit_scale is the smaller ratio");
    mk::View v;
    v.zoom = 1.0f;
    v.cx = 400.0f;
    v.cy = 300.0f;
    mk::Mapping m = mk::mapping(v, dw, dh, pw, ph);
    check(std::fabs(m.scale - 0.5f) < 1e-6f, "scale at zoom 1");
    check(std::fabs(m.to_mask_x(200.0f) - 400.0f) < 1e-3f && std::fabs(m.to_mask_y(200.0f) - 300.0f) < 1e-3f,
          "pane centre is the view centre");
    check(std::fabs(m.to_screen_x(400.0f) - 200.0f) < 1e-3f, "to_screen inverts to_mask");
    // Zoom about a point keeps that point under the cursor.
    const float sx = 260.0f, sy = 150.0f;
    const float before_x = m.to_mask_x(sx), before_y = m.to_mask_y(sy);
    mk::zoom_about(v, 2.0f, sx, sy, dw, dh, pw, ph);
    m = mk::mapping(v, dw, dh, pw, ph);
    check(std::fabs(v.zoom - 2.0f) < 1e-6f, "zoom doubled");
    check(std::fabs(m.to_mask_x(sx) - before_x) < 1e-2f && std::fabs(m.to_mask_y(sy) - before_y) < 1e-2f,
          "the point under the cursor stayed put");
    mk::zoom_about(v, 0.01f, sx, sy, dw, dh, pw, ph);
    check(v.zoom == mk::kMinZoom, "zoom clamps at kMinZoom");
    m = mk::mapping(v, dw, dh, pw, ph);
    check(m.to_screen_x(0.0f) > 0.0f && m.to_screen_x((float)dw) < pw,
          "zoomed out, the pane has room on both sides of the mask");
    for (int i = 0; i < 40; i++) mk::zoom_about(v, 2.0f, sx, sy, dw, dh, pw, ph);
    check(v.zoom == mk::kMaxZoom, "zoom clamps at kMaxZoom");
    // Cursor at a pane corner, zooming in from an already-cornered view: the
    // recentred point lands off-image and must come back clamped, not just
    // the zoom factor -- the final clamp_view() has to run after recentring.
    mk::View corner{1.0f, 0.0f, 0.0f};
    mk::zoom_about(corner, 8.0f, 0.0f, 0.0f, dw, dh, pw, ph);
    check(corner.zoom == 8.0f && corner.cx == 0.0f && corner.cy == 0.0f,
          "zoom_about clamps the recentred centre, not only the zoom factor");
    v.zoom = 4.0f;
    v.cx = 400.0f;
    v.cy = 300.0f;
    m = mk::mapping(v, dw, dh, pw, ph);
    mk::pan(v, 20.0f, -10.0f, m, dw, dh);
    check(std::fabs(v.cx - (400.0f - 20.0f / m.scale)) < 1e-3f, "pan moves the centre against the drag");
    mk::pan(v, -1e6f, 0.0f, m, dw, dh);
    check(v.cx == (float)dw, "centre clamps to the mask");

    // Window: at zoom 1 the whole 800x600 fits and needs no decimation.
    v = mk::View{1.0f, 400.0f, 300.0f};
    m = mk::mapping(v, dw, dh, pw, ph);
    mk::Window w = mk::window_for(m, dw, dh, pw, ph);
    check(w.r.x0 == 0 && w.r.y0 == 0 && w.r.x1 == dw && w.r.y1 == dh, "window is the whole mask");
    check(w.step == 1 && w.tw == dw && w.th == dh, "no decimation under 4096");
    const mk::View out{mk::kMinZoom, 400.0f, 300.0f};
    const mk::Window ow = mk::window_for(mk::mapping(out, dw, dh, pw, ph), dw, dh, pw, ph);
    check(ow.r.x0 == 0 && ow.r.y0 == 0 && ow.r.x1 == dw && ow.r.y1 == dh,
          "zoomed out, the window stops at the mask's edges");
    // A 9000-wide mask fully visible decimates by 3.
    mk::View big{1.0f, 4500.0f, 2000.0f};
    const mk::Mapping bm = mk::mapping(big, 9000, 4000, pw, ph);
    const mk::Window bw = mk::window_for(bm, 9000, 4000, pw, ph);
    check(bw.step == 3 && bw.tw == 3000 && bw.th == 1334, "9000 wide decimates by 3 into 3000x1334");
    // Zoomed to 64 the window is a small rect.
    mk::View z{64.0f, 400.0f, 300.0f};
    const mk::Mapping zm = mk::mapping(z, dw, dh, pw, ph);
    const mk::Window zw = mk::window_for(zm, dw, dh, pw, ph);
    check(zw.step == 1 && zw.r.w() <= 16 && zw.r.h() <= 16 && zw.r.w() >= 12, "64x window is ~12.5 px wide");
    check(mk::same_window(zw, zw) && !mk::same_window(zw, w), "same_window");
    // Same rect, different step/tw/th: a hand-built Window (as derive_window's
    // own fixtures do) must not read as the same window just because the
    // visible rect agrees.
    mk::Window step_diff = zw; step_diff.step += 1;
    check(!mk::same_window(zw, step_diff), "same_window: same rect, different step");
    mk::Window tw_diff = zw; tw_diff.tw += 1;
    check(!mk::same_window(zw, tw_diff), "same_window: same rect, different tw");
    mk::Window th_diff = zw; th_diff.th += 1;
    check(!mk::same_window(zw, th_diff), "same_window: same rect, different th");
}

void test_derive_window() {
    // A 4x2 stored mask, identity turn, frame the same size.
    const int W = 4, H = 2;
    const uint8_t rgb[4 * 2 * 3] = {
        90, 60, 30,  100, 100, 100,  200, 40, 80,  10, 20, 30,
        90, 60, 30,  100, 100, 100,  200, 40, 80,  10, 20, 30};
    const uint8_t comp[8] = {0, 255, 255, 255, 0, 255, 255, 255};
    const uint8_t drop[8] = {0, 255, 0, 0, 0, 255, 0, 0};
    const uint8_t keep[8] = {0, 0, 255, 0, 0, 0, 255, 0};
    mk::WindowSource src;
    src.rgb = rgb; src.fw = W; src.fh = H;
    src.composite = comp; src.drop = drop; src.keep = keep;
    src.W = W; src.H = H;
    mk::Window win;
    win.r = {0, 0, W, H};
    win.step = 1;
    win.tw = W;
    win.th = H;
    std::vector<uint8_t> rgba;
    const mk::Rect t = mk::derive_window(win, win.r, src, rgba);
    check(t.x0 == 0 && t.y0 == 0 && t.x1 == W && t.y1 == H, "texel rect is the whole window");
    check(rgba.size() == (size_t)W * H * 4, "rgba sized");
    // Texel 0: dropped, no layer -> Picture.cpp tint (r/3+150, g/3, b/3).
    check(rgba[0] == 90 / 3 + 150 && rgba[1] == 60 / 3 && rgba[2] == 30 / 3 && rgba[3] == 255,
          "dropped pixel tinted like Picture.cpp");
    // Texel 1: drop layer over a kept composite -> 25% toward (235,45,45).
    check(rgba[4] == (100 * 3 + 235) / 4 && rgba[5] == (100 * 3 + 45) / 4 && rgba[6] == (100 * 3 + 45) / 4,
          "drop layer tint");
    // Texel 2: keep layer -> 25% toward (60,220,90).
    check(rgba[8] == (200 * 3 + 60) / 4 && rgba[9] == (40 * 3 + 220) / 4 && rgba[10] == (80 * 3 + 90) / 4,
          "keep layer tint");
    // Texel 3: kept, no layer -> the photo.
    check(rgba[12] == 10 && rgba[13] == 20 && rgba[14] == 30, "kept pixel is the photo");
    // A sub-rectangle writes only its texels.
    std::fill(rgba.begin(), rgba.end(), 7);
    const mk::Rect part = mk::derive_window(win, mk::Rect{2, 0, 3, 2}, src, rgba);
    check(part.x0 == 2 && part.x1 == 3 && part.y0 == 0 && part.y1 == 2, "part texel rect");
    check(rgba[0] == 7 && rgba[8] == (200 * 3 + 60) / 4 && rgba[12] == 7, "only the part was rewritten");
    // Step 2 over uniform 2x2 blocks averages exactly; the mask decides by majority.
    const uint8_t rgb2[4 * 2 * 3] = {
        8, 8, 8,  8, 8, 8,  40, 40, 40,  40, 40, 40,
        8, 8, 8,  8, 8, 8,  40, 40, 40,  40, 40, 40};
    const uint8_t comp2[8] = {0, 0, 0, 255, 0, 0, 255, 255};
    const uint8_t zero[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    src.rgb = rgb2; src.composite = comp2; src.drop = zero; src.keep = zero;
    mk::Window w2;
    w2.r = {0, 0, W, H};
    w2.step = 2;
    w2.tw = 2;
    w2.th = 1;
    mk::derive_window(w2, w2.r, src, rgba);
    check(rgba[0] == 8 / 3 + 150 && rgba[1] == 8 / 3, "decimated block, all dropped, tinted");
    check(rgba[4] == 40 && rgba[5] == 40 && rgba[6] == 40, "decimated block, 3 of 4 kept, photo");
    // An exact 2-of-4 tie in a step-2 block counts as dropped: hiding a
    // correction is worse than over-showing one. This is reachable on real
    // 8K captures, whose SS_MASK_BENCH size decimates to exactly step 2.
    const uint8_t rgb3[4 * 2 * 3] = {
        100, 100, 100,  100, 100, 100,  40, 40, 40,  40, 40, 40,
        100, 100, 100,  100, 100, 100,  40, 40, 40,  40, 40, 40};
    const uint8_t comp3[8] = {0, 255, 255, 255, 0, 255, 255, 255};
    src.rgb = rgb3; src.composite = comp3; src.drop = zero; src.keep = zero;
    mk::derive_window(w2, w2.r, src, rgba);
    check(rgba[0] == 100 / 3 + 150 && rgba[1] == 100 / 3, "2-of-4 tie counts as dropped");
    check(rgba[4] == 40 && rgba[5] == 40 && rgba[6] == 40, "non-tied block stays kept");
    // A turned source: orientation 6, stored 4x2 shows as 2x4; displayed
    // (1,3) is stored (3,0), whose photo is 40 and comp 255.
    src.turn = sfm::exifTransform(6);
    mk::Window w3;
    w3.r = {0, 0, 2, 4};
    w3.step = 1;
    w3.tw = 2;
    w3.th = 4;
    mk::derive_window(w3, w3.r, src, rgba);
    check(rgba[((size_t)3 * 2 + 1) * 4] == 40, "turned: displayed (1,3) reads stored (3,0)");
    // A frame at another size than the mask is sampled to the mask grid.
    const uint8_t rgb8[8 * 4 * 3] = {0};
    src.turn = sfm::ExifTransform{};
    src.rgb = rgb8; src.fw = 8; src.fh = 4;
    src.composite = comp; src.drop = zero; src.keep = zero;
    mk::derive_window(win, win.r, src, rgba);
    check(rgba[12] == 0 && rgba[15] == 255, "frame sampled to mask grid, no crash");
}

// ---------------------------------------------------------------------------
// The three display styles, pixel-exact
// ---------------------------------------------------------------------------

void test_window_styles() {
    const uint8_t rgb[12] = {100, 200, 50, 100, 200, 50, 100, 200, 50, 100, 200, 50};
    const uint8_t base[4] = {255, 0, 255, 0};
    const uint8_t drop[4] = {0, 0, 255, 0};
    const uint8_t keep[4] = {0, 0, 0, 255};
    uint8_t comp[4];
    mk::composite(base, drop, keep, 4, comp);
    check(comp[0] == 255 && comp[1] == 0 && comp[2] == 0 && comp[3] == 255, "fixture composite");
    mk::Window win;
    win.r = {0, 0, 4, 1};
    win.step = 1;
    win.tw = 4;
    win.th = 1;
    mk::WindowSource src;
    src.rgb = rgb;
    src.fw = 4;
    src.fh = 1;
    src.composite = comp;
    src.drop = drop;
    src.keep = keep;
    src.W = 4;
    src.H = 1;
    std::vector<uint8_t> rgba;
    auto px = [&](int i, int r, int g, int b) {
        return rgba[(size_t)i * 4] == r && rgba[(size_t)i * 4 + 1] == g &&
               rgba[(size_t)i * 4 + 2] == b && rgba[(size_t)i * 4 + 3] == 255;
    };
    check(src.style == mk::Style::Overlay, "default style is Overlay");
    mk::derive_window(win, win.r, src, rgba);
    check(px(0, 100, 200, 50), "overlay: kept pixel is the photo");
    check(px(1, 183, 66, 16), "overlay: dropped pixel is Picture.cpp's tint (r/3+150, g/3, b/3)");
    check(px(2, 196, 60, 23), "overlay: drop layer blends 25% red over the tint");
    check(px(3, 90, 205, 60), "overlay: keep layer blends 25% green over the photo");
    src.style = mk::Style::MaskOnly;
    mk::derive_window(win, win.r, src, rgba);
    check(px(0, 230, 230, 230), "mask only: kept is light");
    check(px(1, 30, 30, 30), "mask only: dropped is dark");
    check(px(2, 81, 33, 33), "mask only: drop layer tints the dark");
    check(px(3, 187, 227, 195), "mask only: keep layer tints the light");
    src.style = mk::Style::Photo;
    mk::derive_window(win, win.r, src, rgba);
    for (int i = 0; i < 4; i++)
        check(px(i, 100, 200, 50), "photo: pixel " + std::to_string(i) + " is the frame, no tint");
}

// The frame at twice the mask's size, every pixel distinct and every texel
// kept, so each texel IS a frame pixel: skipping the scale reads another one.
void test_derive_window_frame_scale() {
    const int W = 4, H = 2, fw = 8, fh = 4;
    std::vector<uint8_t> rgb((size_t)fw * fh * 3);
    auto px = [&](int x, int y) { return &rgb[((size_t)y * fw + x) * 3]; };
    for (int y = 0; y < fh; y++)
        for (int x = 0; x < fw; x++) {
            px(x, y)[0] = (uint8_t)(10 + 20 * x);
            px(x, y)[1] = (uint8_t)(7 + 50 * y);
            px(x, y)[2] = (uint8_t)(3 * x + y);
        }
    const std::vector<uint8_t> kept((size_t)W * H, 255), zero((size_t)W * H, 0);
    mk::WindowSource src;
    src.rgb = rgb.data(); src.fw = fw; src.fh = fh;
    src.composite = kept.data(); src.drop = zero.data(); src.keep = zero.data();
    src.W = W; src.H = H;
    mk::Window win;
    win.r = {0, 0, W, H};
    win.step = 1;
    win.tw = W;
    win.th = H;
    std::vector<uint8_t> rgba;
    mk::derive_window(win, win.r, src, rgba);
    bool scaled = true, separates = false;
    for (int my = 0; my < H; my++)
        for (int mx = 0; mx < W; mx++) {
            const uint8_t* want = px(2 * mx, 2 * my);
            const uint8_t* got = &rgba[((size_t)my * W + mx) * 4];
            scaled &= got[0] == want[0] && got[1] == want[1] && got[2] == want[2];
            separates |= std::memcmp(px(mx, my), want, 3) != 0;
        }
    check(separates, "frame scale: fixture, the unscaled pixel differs from the scaled one");
    check(scaled, "frame scale: texel (x, y) of a 4x2 mask shows frame pixel (2x, 2y) of an 8x4 frame");
}

// Every other view-math check uses a square pane, where a pane_w/pane_h swap
// changes nothing. 500x300 over 800x600 separates them at every step.
void test_view_math_non_square_pane() {
    const int dw = 800, dh = 600;
    const float pw = 500.0f, ph = 300.0f;
    check(std::fabs(mk::fit_scale(dw, dh, pw, ph) - 0.5f) < 1e-6f &&
              std::fabs(mk::fit_scale(dw, dh, ph, pw) - 0.375f) < 1e-6f,
          "non-square pane: fit_scale is 0.5, and 0.375 with the pane transposed");
    mk::View v{2.0f, 400.0f, 300.0f};
    const mk::Mapping m = mk::mapping(v, dw, dh, pw, ph);
    check(std::fabs(m.scale - 1.0f) < 1e-6f && std::fabs(m.x0 - 150.0f) < 1e-3f &&
              std::fabs(m.y0 - 150.0f) < 1e-3f,
          "non-square pane: mapping's origin is half the pane WIDTH left, half its HEIGHT up");
    const mk::Window w = mk::window_for(m, dw, dh, pw, ph);
    check(w.r.x0 == 150 && w.r.y0 == 150 && w.r.x1 == 651 && w.r.y1 == 451,
          "non-square pane: the window spans pane_w across and pane_h down");
    const float sx = 420.0f, sy = 40.0f;
    const float bx = m.to_mask_x(sx), by = m.to_mask_y(sy);
    mk::zoom_about(v, 1.5f, sx, sy, dw, dh, pw, ph);
    const mk::Mapping after = mk::mapping(v, dw, dh, pw, ph);
    check(std::fabs(after.to_mask_x(sx) - bx) < 1e-2f && std::fabs(after.to_mask_y(sy) - by) < 1e-2f,
          "non-square pane: zoom_about keeps an off-centre cursor's point put");
}

// ---------------------------------------------------------------------------
// The session end to end, without drawing
// ---------------------------------------------------------------------------

void settle(mk::MaskSession& s) {
    for (int i = 0; i < 2000 && !s.idle(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    s.pump();
}

void wait_scanned(mk::MaskSession& s) {
    for (int i = 0; i < 2000 && s.scanned_count() < s.frame_count(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

void test_session() {
    Fixture f = make_dataset("session", 64, 48, {"cam0/a", "cam0/b", "cam1/c"});
    const std::vector<uint8_t> original_a = file_bytes(f.masks / "cam0" / "a.png");
    mk::MaskSession s;
    std::string err;
    // Refusals.
    check(!s.open(f.images.string(), f.images.string(), f.masks.string(), false, err) &&
              err == spirula::i18n::msg::maskedit::err_workspace_inside_images.get(),
          "workspace inside images is refused with the message");
    check(!s.open((f.images / "sub").string(), f.images.string(), f.masks.string(), false, err) &&
              err == spirula::i18n::msg::maskedit::err_workspace_inside_images.get(),
          "a workspace under the image root is refused");
    const fs::path empty = scratch("session_empty");
    check(!s.open(f.root.string(), empty.string(), f.masks.string(), false, err) &&
              err.find(mk::normalize_dir(empty.string())) != std::string::npos,
          "no frames is refused naming the folder");
    check(!s.is_open(), "not open after refusals");

    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    check(s.is_open() && s.frame_count() == 3, "three frames");
    check(s.frames()[0].key == "cam0/a" && s.frames()[0].camera == "cam0" &&
              s.frames()[2].key == "cam1/c" && s.frames()[2].camera == "cam1",
          "frames keyed and grouped as the run keys them");
    check(s.layer_root() == (f.root / mk::kLayerDirName).string(), "layer root under the workspace");
    check(mk::MaskSession::paint_for(false, false, false) == mk::Paint::ForceDrop &&
              mk::MaskSession::paint_for(true, false, false) == mk::Paint::ForceDrop &&
              mk::MaskSession::paint_for(false, true, false) == mk::Paint::ForceKeep &&
              mk::MaskSession::paint_for(true, true, false) == mk::Paint::Clear,
          "paint_for, brush: plain/Shift drop, Ctrl keep, both clear");
    check(mk::MaskSession::paint_for(false, false, true) == mk::Paint::ForceKeep &&
              mk::MaskSession::paint_for(true, false, true) == mk::Paint::ForceKeep &&
              mk::MaskSession::paint_for(false, true, true) == mk::Paint::ForceDrop &&
              mk::MaskSession::paint_for(true, true, true) == mk::Paint::Clear,
          "paint_for, eraser: plain/Shift keep, Ctrl drop, both clear");
    // The point of the pair above is that they DIFFER, so assert it directly:
    // an eraser that ignored its flag would satisfy either table alone.
    check(mk::MaskSession::paint_for(false, false, false) !=
                  mk::MaskSession::paint_for(false, false, true) &&
              mk::MaskSession::paint_for(false, true, false) !=
                  mk::MaskSession::paint_for(false, true, true),
          "the eraser flag changes the mode on a plain drag and on Ctrl+drag");
    s.set_subtract(true);
    check(s.paint_now(false, false) == mk::Paint::ForceKeep &&
              s.paint_now(false, true) == mk::Paint::ForceDrop &&
              s.paint_now(true, true) == mk::Paint::Clear,
          "subtract: a plain drag keeps, Ctrl drops, both still clear");
    s.set_erasing(true);
    check(s.paint_now(false, false) == mk::Paint::ForceDrop,
          "subtract with the eraser: the two swaps cancel");
    s.set_erasing(false);
    s.set_subtract(false);
    check(mk::MaskSession::paint_for(true, true, false) ==
              mk::MaskSession::paint_for(true, true, true),
          "Shift+Ctrl clears under both tools: clearing is not the eraser");
    check(mk::MaskSession::step_brush(100.0f, true) == 100.0f * 1.18f &&
              mk::MaskSession::step_brush(100.0f, false) == 100.0f * 0.85f,
          "step_brush: grow x1.18, shrink x0.85");
    check(mk::MaskSession::step_brush(4096.0f, true) == 4096.0f &&
              mk::MaskSession::step_brush(4000.0f, true) == 4096.0f,
          "step_brush clamps growth at 4096, exactly, not past it");
    check(mk::MaskSession::step_brush(1.0f, false) == 1.0f &&
              mk::MaskSession::step_brush(1.1f, false) == 1.0f,
          "step_brush clamps shrink at 1, exactly, not below it");
    check(mk::MaskSession::clamp_brush(0.0f) == 1.0f &&
              mk::MaskSession::clamp_brush(-5.0f) == 1.0f &&
              mk::MaskSession::clamp_brush(1e30f) == 4096.0f &&
              mk::MaskSession::clamp_brush(24.0f) == 24.0f,
          "clamp_brush pins to [1, 4096] and leaves an interior value alone");
    {
        // std::clamp PROPAGATES a NaN, and a NaN radius rasterizes nothing
        // while the slider and the strip still read a number.
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        check(mk::MaskSession::clamp_brush(nan) == 1.0f, "clamp_brush folds NaN to the minimum");
        check(mk::MaskSession::clamp_brush(inf) == 4096.0f &&
                  mk::MaskSession::clamp_brush(-inf) == 1.0f,
              "clamp_brush pins both infinities");
    }
    {
        // Alt+wheel: a notch is a `]`, but the factor is the RECIPROCAL of
        // 1.18 rather than the bracket's 0.85, so the wheel is exactly
        // reversible. The bracket check below is what makes that testable.
        const float r = 100.0f;
        check(mk::MaskSession::wheel_brush(r, 0.0f) == r, "a zero wheel delta moves nothing");
        check(std::fabs(mk::MaskSession::wheel_brush(r, 1.0f) -
                        mk::MaskSession::step_brush(r, true)) < 1e-3f,
              "one wheel notch up is one `]`");
        check(mk::MaskSession::wheel_brush(r, 2.0f) > mk::MaskSession::wheel_brush(r, 1.0f) &&
                  mk::MaskSession::wheel_brush(r, -1.0f) < r,
              "the wheel grows upward and shrinks downward");
        const float there_and_back =
            mk::MaskSession::wheel_brush(mk::MaskSession::wheel_brush(r, 1.0f), -1.0f);
        check(std::fabs(there_and_back - r) < 1e-3f, "a notch back exactly undoes a notch");
        const float bracket_round_trip =
            mk::MaskSession::step_brush(mk::MaskSession::step_brush(r, true), false);
        check(std::fabs(bracket_round_trip - r) > 0.2f,
              "the bracket pair is NOT reversible (1.18*0.85 = 1.003), so the check above discriminates");
        check(mk::MaskSession::wheel_brush(4000.0f, 40.0f) == 4096.0f &&
                  mk::MaskSession::wheel_brush(2.0f, -40.0f) == 1.0f,
              "wheel_brush clamps at both ends");
    }
    check(!s.erasing() && s.radius() == 24.0f, "the session opens on the brush at 24 px");
    {
        // Monotonic in both directions, and the clamp is REACHED, not
        // approached asymptotically: 4096/0.85^n < 1 well inside 200 steps.
        float r = 24.0f;
        bool grew = true, shrank = true;
        for (int i = 0; i < 200; i++) {
            const float next = mk::MaskSession::step_brush(r, true);
            grew = grew && next >= r;
            r = next;
        }
        check(grew && r == 4096.0f, "repeated growth is monotonic and lands exactly on 4096");
        for (int i = 0; i < 200; i++) {
            const float next = mk::MaskSession::step_brush(r, false);
            shrank = shrank && next <= r;
            r = next;
        }
        check(shrank && r == 1.0f, "repeated shrink is monotonic and lands exactly on 1");
    }
    settle(s);
    check(s.doc() != nullptr && s.frame_index() == 0, "frame 0 loaded");
    check(s.shown_width() == 64 && s.shown_height() == 48, "shown size (no EXIF turn)");
    check(s.corrected_count() == 0 && s.error().empty(), "nothing corrected yet");

    // A stroke in pane pixels under an identity mapping.
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    const mk::Rect changed = s.commit_stroke(box, mk::Paint::ForceDrop, m);
    check(!changed.empty() && changed.x0 <= 4 && changed.x1 >= 14, "commit returns the changed rect");
    check(s.doc()->dirty() && s.doc()->drop()[(size_t)8 * 64 + 8] == 255, "painted and dirty");
    // A 2x mapping: pane (80,8)-(100,28) is mask (40,4)-(50,14).
    mk::Mapping m2;
    m2.scale = 2.0f;
    gui::ShapeStroke box2;
    box2.kind = gui::ShapeKind::Box;
    box2.pts = std::vector<float>{80.0f, 8.0f, 100.0f, 28.0f};
    s.commit_stroke(box2, mk::Paint::ForceKeep, m2);
    check(s.doc()->keep()[(size_t)8 * 64 + 45] == 255 && s.doc()->keep()[(size_t)8 * 64 + 39] == 0,
          "pane pixels mapped through the scale");
    const mk::Rect un = s.undo();
    check(!un.empty() && s.doc()->keep()[(size_t)8 * 64 + 45] == 0, "undo through the session");
    check(!s.redo().empty() && s.doc()->keep()[(size_t)8 * 64 + 45] == 255, "redo through the session");

    // Explicit save: the composite lands, the doc stays open and clean.
    s.save();
    settle(s);
    check(!s.doc()->dirty() && s.error().empty(), "clean after save: " + s.error());
    check(s.corrected_count() == 1, "one corrected frame");
    check(file_bytes(f.masks / "cam0" / "a.png") != original_a, "masks/cam0/a.png rewritten");
    check(file_bytes(f.layer / "cam0" / "a.base.png") == original_a, "base is the original");

    // Paint, then switch frames: autosave, then the next frame loads.
    s.commit_stroke(box, mk::Paint::Clear, m);
    check(s.doc()->dirty(), "dirty again");
    s.go_to(2);
    settle(s);
    check(s.frame_index() == 2 && s.doc() && s.doc()->key() == "cam1/c", "frame 2 open");
    check(s.doc()->base_state() == mk::BaseState::Unedited, "frame 2 untouched");
    mk::LayerIndex disk;
    disk.load(s.layer_root(), err);
    check(disk.frames.count("cam0/a") == 1, "frame 0 was saved on the way out");
    // Back to 0: our own composite is Unchanged and the layers are there.
    s.go_to(0);
    settle(s);
    check(s.doc()->base_state() == mk::BaseState::Unchanged, "reopened frame is Unchanged");
    check(s.doc()->drop()[(size_t)8 * 64 + 8] == 0 && s.doc()->keep()[(size_t)8 * 64 + 45] == 255,
          "layers as saved (clear applied, keep kept)");

    // Someone regenerated the mask meanwhile: reopening re-bases it.
    s.go_to(1);
    settle(s);
    write_png_gray(f.masks / "cam0" / "a.png", 64, 48, synth_mask(64, 48, 77));
    const std::vector<uint8_t> regenerated = file_bytes(f.masks / "cam0" / "a.png");
    s.go_to(0);
    settle(s);
    check(s.doc()->base_state() == mk::BaseState::Regenerated, "regenerated mask detected on open");
    check(file_bytes(f.layer / "cam0" / "a.base.png") == regenerated, "re-based to the regenerated file");
    check(s.doc()->base() == synth_mask(64, 48, 77), "doc base is the new mask");

    // Revert the open frame: the mask is the base again, entry gone.
    s.revert_open_frame();
    settle(s);
    check(file_bytes(f.masks / "cam0" / "a.png") == regenerated, "revert copies the base back");
    check(s.corrected_count() == 0 && s.doc() && s.doc()->base_state() == mk::BaseState::Unedited,
          "reverted and reloaded");

    // Close with a dirty frame saves it.
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.close();
    check(!s.is_open(), "closed");
    disk.load(s.layer_root().empty() ? (f.root / mk::kLayerDirName).string() : s.layer_root(), err);
    check(disk.frames.count("cam0/a") == 1, "close saved the dirty frame");
    check(fs::exists(f.layer / "cam0" / "a.drop.png"), "layer file written by close");

    // revert_every_frame.
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "reopen");
    settle(s);
    s.revert_every_frame();
    settle(s);
    check(s.corrected_count() == 0 && !fs::exists(f.layer / "cam0" / "a.drop.png"), "everything reverted");
    s.close();
}

// ---------------------------------------------------------------------------
// Propagate copies layers, not masks; refuses a size; snapshot and restore
// put a target back
// ---------------------------------------------------------------------------

std::vector<uint8_t> stencil_pixels(const fs::path& p) {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    app::load_stencil(p.string(), w, h, px);
    return px;
}

void test_propagate_targets() {
    std::vector<mk::FrameRef> fr = {
        {"/i/cam0/a.jpg", "cam0/a", "cam0"}, {"/i/cam0/b.jpg", "cam0/b", "cam0"},
        {"/i/cam0/c.jpg", "cam0/c", "cam0"}, {"/i/cam0/e.jpg", "cam0/e", "cam0"},
        {"/i/cam1/d.jpg", "cam1/d", "cam1"}};
    using S = mk::PropagateScope;
    check(mk::propagate_targets(fr, 0, S::Camera, 0, 0) == std::vector<int>({1, 2, 3}),
          "camera: every other frame with the key, never the source");
    check(mk::propagate_targets(fr, 4, S::Camera, 0, 0).empty(), "camera: alone in its camera");
    check(mk::propagate_targets(fr, 0, S::Next, 0, 0) == std::vector<int>({1}),
          "next: one frame, not the rest of the camera");
    check(mk::propagate_targets(fr, 2, S::Next, 0, 0) == std::vector<int>({3}), "next: same camera");
    check(mk::propagate_targets(fr, 3, S::Next, 0, 0).empty(), "next: refused across cameras");
    check(mk::propagate_targets(fr, 4, S::Next, 0, 0).empty(), "next: at the end");
    check(mk::propagate_targets(fr, 0, S::Range, 0, 4) == std::vector<int>({1, 2, 3}),
          "range: clipped to the camera, source excluded");
    check(mk::propagate_targets(fr, 1, S::Range, 2, 2) == std::vector<int>({2}), "range: one frame");
    check(mk::propagate_targets(fr, 1, S::Range, 3, 1).empty(), "range: inverted is empty");
    check(mk::propagate_targets(fr, 1, S::Range, -5, 99) == std::vector<int>({0, 2, 3}),
          "range: clamped to the frames");
    check(mk::propagate_targets(fr, 9, S::Camera, 0, 0).empty(), "source out of range");
}

void test_propagate_to() {
    Fixture f = make_dataset("propagate", 64, 48, {"cam0/a", "cam0/b", "cam0/c", "cam1/d"});
    // A fifth frame of the same camera at another size.
    write_jpg_rgb(f.images / "cam0" / "e.jpg", 32, 24, synth_rgb(32, 24, 9));
    write_png_gray(f.masks / "cam0" / "e.png", 32, 24, synth_mask(32, 24, 9));
    // g and h exist only so the three sizes frame_size chooses between differ.
    write_jpg_rgb(f.images / "cam0" / "g.jpg", 64, 48, synth_rgb(64, 48, 10));
    write_png_gray(f.masks / "cam0" / "g.png", 16, 12, synth_mask(16, 12, 10));
    write_jpg_rgb(f.images / "cam0" / "h.jpg", 64, 48, synth_rgb(64, 48, 11));
    write_png_gray(f.masks / "cam0" / "h.png", 16, 12, synth_mask(16, 12, 11));
    write_png_gray(f.layer / "cam0" / "h.base.png", 8, 6, synth_mask(8, 6, 11));
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "cam0" / "b.png");
    const std::vector<uint8_t> original_e = file_bytes(f.masks / "cam0" / "e.png");
    const std::vector<uint8_t> base_b = stencil_pixels(f.masks / "cam0" / "b.png");
    const std::vector<uint8_t> base_c = stencil_pixels(f.masks / "cam0" / "c.png");
    check(base_b != base_c, "fixture: b and c have different bases");
    const std::vector<uint8_t> drop = box_layer(64, 48, 4, 4, 14, 14);
    const std::vector<uint8_t> keep = box_layer(64, 48, 40, 20, 50, 30);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    int w = 0, h = 0;
    std::string from;
    check(mk::frame_size(layer_root, mask_root, "cam0/b", (f.images / "cam0" / "b.jpg").string(), w, h, from) &&
              w == 64 && h == 48, "frame_size from the mask");
    check(mk::frame_size(layer_root, mask_root, "cam0/e", (f.images / "cam0" / "e.jpg").string(), w, h, from) &&
              w == 32 && h == 24, "frame_size of the odd frame");
    fs::remove(f.masks / "cam1" / "d.png");
    check(mk::frame_size(layer_root, mask_root, "cam1/d", (f.images / "cam1" / "d.jpg").string(), w, h, from) &&
              w == 64 && h == 48 && from == (f.images / "cam1" / "d.jpg").string(),
          "frame_size falls back to the image when the mask is gone");
    // frame_size's order, on frames whose three candidate sizes differ: the
    // mask beats the image, the base beats the mask.
    check(mk::frame_size(layer_root, mask_root, "cam0/g", (f.images / "cam0" / "g.jpg").string(), w, h, from) &&
              w == 16 && h == 12 && from == (f.masks / "cam0" / "g.png").string(),
          "frame_size prefers the mask over the image");
    check(mk::frame_size(layer_root, mask_root, "cam0/h", (f.images / "cam0" / "h.jpg").string(), w, h, from) &&
              w == 8 && h == 6 && from == (f.layer / "cam0" / "h.base.png").string(),
          "frame_size prefers the base over the mask");

    // Onto b: b's composite is b's OWN base under a's layers.
    mk::PropagateRefusal refused;
    check(mk::propagate_to(layer_root, mask_root, "cam0/b", (f.images / "cam0" / "b.jpg").string(),
                           64, 48, drop.data(), keep.data(), idx, refused, err),
          "propagate_to b: " + err);
    std::vector<uint8_t> want(base_b.size());
    mk::composite(base_b.data(), drop.data(), keep.data(), want.size(), want.data());
    check(stencil_pixels(f.masks / "cam0" / "b.png") == want, "b's composite = b's base under the layers");
    check(file_bytes(f.layer / "cam0" / "b.base.png") == original_b, "b's base is b's original, byte for byte");
    check(stencil_pixels(f.layer / "cam0" / "b.drop.png") == drop &&
              stencil_pixels(f.layer / "cam0" / "b.keep.png") == keep, "b's layers are the source's");
    check(idx.frames.count("cam0/b") == 1, "b indexed");
    // Not the source's mask: c gets c's base under the same layers, which differs from b's.
    check(mk::propagate_to(layer_root, mask_root, "cam0/c", (f.images / "cam0" / "c.jpg").string(),
                           64, 48, drop.data(), keep.data(), idx, refused, err), "propagate_to c");
    check(stencil_pixels(f.masks / "cam0" / "c.png") != stencil_pixels(f.masks / "cam0" / "b.png"),
          "c and b differ after the same propagate: the mask was not copied");

    // The odd size is refused, with its size, and nothing is written.
    check(!mk::propagate_to(layer_root, mask_root, "cam0/e", (f.images / "cam0" / "e.jpg").string(),
                            64, 48, drop.data(), keep.data(), idx, refused, err) &&
              err.empty() && refused.w == 32 && refused.h == 24, "e refused with its size");
    check(file_bytes(f.masks / "cam0" / "e.png") == original_e && !fs::exists(f.layer / "cam0" / "e.drop.png") &&
              idx.frames.count("cam0/e") == 0, "refusal writes nothing");
    // Refused before the load, which would rebase a regenerated mask first.
    const std::vector<uint8_t> e_px = stencil_pixels(f.masks / "cam0" / "e.png");
    const std::vector<uint8_t> e_zero((size_t)32 * 24, 0);
    check(mk::save_frame(layer_root, mask_root, "cam0/e", 32, 24, e_px.data(), e_zero.data(),
                         e_zero.data(), true, idx, err), "fixture: e edited");
    write_png_gray(f.masks / "cam0" / "e.png", 32, 24, synth_mask(32, 24, 12));
    check(mk::base_state(mask_root, "cam0/e", idx) == mk::BaseState::Regenerated, "fixture: e regenerated");
    const std::vector<uint8_t> e_mask = file_bytes(f.masks / "cam0" / "e.png");
    const std::vector<uint8_t> e_base = file_bytes(f.layer / "cam0" / "e.base.png");
    check(!mk::propagate_to(layer_root, mask_root, "cam0/e", (f.images / "cam0" / "e.jpg").string(),
                            64, 48, drop.data(), keep.data(), idx, refused, err) && refused.w == 32 &&
              file_bytes(f.masks / "cam0" / "e.png") == e_mask &&
              file_bytes(f.layer / "cam0" / "e.base.png") == e_base,
          "a regenerated odd frame is refused without being rebased");

    // A frame with no mask takes layers only, no composite.
    check(mk::propagate_to(layer_root, mask_root, "cam1/d", (f.images / "cam1" / "d.jpg").string(),
                           64, 48, drop.data(), keep.data(), idx, refused, err) &&
              refused.w == 0 && refused.h == 0,
          "propagate_to d (no mask), and the previous refusal was cleared at entry");
    check(!fs::exists(f.masks / "cam1" / "d.png") && fs::exists(f.layer / "cam1" / "d.drop.png"),
          "no mask conjured, layers written");

    // A target whose own layer file is the wrong size is refused with that
    // file named, not silently bulldozed.
    Fixture f2 = make_dataset("propagate_badlayer", 64, 48, {"cam0/a", "cam0/b"});
    const std::string mr2 = f2.masks.string(), lr2 = f2.layer.string();
    write_png_gray(f2.layer / "cam0" / "b.drop.png", 32, 24, synth_mask(32, 24, 3));
    const std::vector<uint8_t> b2_before = file_bytes(f2.masks / "cam0" / "b.png");
    mk::LayerIndex idx2;
    idx2.mask_root = mr2;
    check(!mk::propagate_to(lr2, mr2, "cam0/b", (f2.images / "cam0" / "b.jpg").string(), 64, 48,
                            drop.data(), keep.data(), idx2, refused, err) &&
              err.find("b.drop.png") != std::string::npos && refused.w == 0,
          "a mis-sized layer file is a failure that names itself: " + err);
    check(file_bytes(f2.masks / "cam0" / "b.png") == b2_before && idx2.frames.count("cam0/b") == 0,
          "and b's mask is untouched");

    // A size read that fails names the file it failed on, not the image.
    const fs::path junk = f2.layer / "cam0" / "a.base.png";
    const uint8_t not_png[] = {'n', 'o', 't', ' ', 'p', 'n', 'g'};
    mk::write_file_atomic(junk.string(), not_png, sizeof not_png);
    check(!mk::propagate_to(lr2, mr2, "cam0/a", (f2.images / "cam0" / "a.jpg").string(), 64, 48,
                            drop.data(), keep.data(), idx2, refused, err) &&
              err == junk.string() && refused.w == 0, "a corrupt base is named as the failure: " + err);
}

void test_snapshot_restore() {
    Fixture f = make_dataset("snapshot", 64, 48, {"a", "b", "c"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "b.png");
    const std::vector<uint8_t> base_c = stencil_pixels(f.masks / "c.png");
    const std::vector<uint8_t> drop1 = box_layer(64, 48, 4, 4, 14, 14), keep1 = box_layer(64, 48, 40, 20, 50, 30);
    const std::vector<uint8_t> drop2 = box_layer(64, 48, 20, 20, 30, 30), keep2 = box_layer(64, 48, 0, 0, 5, 5);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    mk::PropagateRefusal refused;

    // b was unedited: a snapshot records absence, and restoring it is a revert.
    mk::LayerSnapshot sb;
    check(mk::snapshot_layers(layer_root, "b", idx, sb, err) && !sb.had_entry && !sb.had_drop &&
              !sb.had_keep && sb.bytes() == 0, "snapshot of an unedited frame is empty");
    check(mk::propagate_to(layer_root, mask_root, "b", (f.images / "b.jpg").string(), 64, 48,
                           drop1.data(), keep1.data(), idx, refused, err), "propagate b");
    check(file_bytes(f.masks / "b.png") != original_b, "fixture: b changed");
    check(mk::restore_layers(layer_root, mask_root, sb, idx, err), "restore b: " + err);
    check(file_bytes(f.masks / "b.png") == original_b, "b's mask is byte-identical to before");
    check(!fs::exists(f.layer / "b.drop.png") && !fs::exists(f.layer / "b.base.png") &&
              idx.frames.count("b") == 0, "b's layers, base and entry are gone");

    // c had layers: a snapshot records them, and restoring puts them back
    // with the composite re-derived over c's base.
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           drop1.data(), keep1.data(), idx, refused, err), "first layers on c");
    mk::LayerSnapshot sc;
    check(mk::snapshot_layers(layer_root, "c", idx, sc, err) && sc.had_entry && sc.had_drop &&
              sc.had_keep && sc.bytes() > 0, "snapshot of an edited frame holds its layer files");
    check(sc.drop_png == file_bytes(f.layer / "c.drop.png"), "snapshot bytes are the file's");
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           drop2.data(), keep2.data(), idx, refused, err), "second layers on c");
    check(stencil_pixels(f.layer / "c.drop.png") == drop2, "fixture: c now carries the second layers");
    check(mk::restore_layers(layer_root, mask_root, sc, idx, err), "restore c: " + err);
    check(file_bytes(f.layer / "c.drop.png") == sc.drop_png && file_bytes(f.layer / "c.keep.png") == sc.keep_png,
          "c's layer files are the snapshot's bytes");
    std::vector<uint8_t> want(base_c.size());
    mk::composite(base_c.data(), drop1.data(), keep1.data(), want.size(), want.data());
    check(stencil_pixels(f.masks / "c.png") == want, "c's composite is the first layers over c's base");
    check(mk::base_state(mask_root, "c", idx) == mk::BaseState::Unchanged, "c's index entry matches its file");
    uint64_t fp = 0;
    mk::fingerprint_file((f.layer / "c.base.png").string(), fp);
    check(idx.frames["c"].base_fp == fp, "restored base fingerprint is the file's");

    // A write the restore cannot make is reported, not swallowed. A directory
    // is only ever a RENAME target here, never a file this test reads.
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           drop2.data(), keep2.data(), idx, refused, err), "c carries the second layers again");
    const std::vector<uint8_t> keep2_png = file_bytes(f.layer / "c.keep.png");
    const fs::path blocked = f.layer / "c.drop.png";
    std::error_code ec;
    fs::remove(blocked, ec);
    fs::create_directories(blocked, ec);
    check(fs::is_directory(blocked), "fixture: a directory blocks c's drop layer");
    check(!mk::restore_layers(layer_root, mask_root, sc, idx, err) &&
              err.find("c.drop.png") != std::string::npos,
          "restore reports the file it could not write: " + err);
    check(keep2_png != sc.keep_png && file_bytes(f.layer / "c.keep.png") == keep2_png,
          "and stops there: the keep layer is not half-restored");
    fs::remove_all(blocked, ec);

    // had_entry without had_drop: the absent layer is removed, not left
    // behind, and the frame comes back with an all-zero drop.
    check(!fs::exists(blocked), "fixture: c has an entry and no drop layer");
    mk::LayerSnapshot sc2;
    check(mk::snapshot_layers(layer_root, "c", idx, sc2, err) && sc2.had_entry && !sc2.had_drop &&
              sc2.had_keep, "snapshot of an entry whose drop layer is gone");
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           drop2.data(), keep2.data(), idx, refused, err), "third layers on c");
    check(stencil_pixels(blocked) == drop2, "fixture: c carries drop2 again");
    check(mk::restore_layers(layer_root, mask_root, sc2, idx, err), "restore c from sc2: " + err);
    check(stencil_pixels(blocked) == std::vector<uint8_t>(64 * 48, 0),
          "the drop layer the snapshot did not hold is zero, not drop2");
    check(file_bytes(f.layer / "c.keep.png") == sc2.keep_png, "and the keep layer is the snapshot's");

    // A snapshot whose layer does not fit the base is refused, not re-encoded
    // as zero, and the layers restore found go back.
    const std::vector<uint8_t> found_drop = file_bytes(f.layer / "c.drop.png");
    mk::LayerSnapshot bad = sc2;
    bad.had_drop = true;
    mk::encode_gray_png(synth_mask(32, 24, 3).data(), 32, 24, bad.drop_png);
    check(!mk::restore_layers(layer_root, mask_root, bad, idx, err) &&
              err.find("c.drop.png") != std::string::npos &&
              file_bytes(f.layer / "c.drop.png") == found_drop && found_drop != bad.drop_png &&
              file_bytes(f.layer / "c.keep.png") == sc2.keep_png,
          "a mis-sized snapshot layer is reported, and the layers restore found are put back: " + err);

    // A base re-based after the snapshot: the entry takes the file's print.
    mk::LayerSnapshot sc3;
    check(mk::restore_layers(layer_root, mask_root, sc2, idx, err) &&
              mk::snapshot_layers(layer_root, "c", idx, sc3, err) && sc3.had_entry, "snapshot before a rebase");
    write_png_gray(f.layer / "c.base.png", 64, 48, synth_mask(64, 48, 7));
    uint64_t rebased = 0;
    mk::fingerprint_file((f.layer / "c.base.png").string(), rebased);
    check(rebased != sc3.entry.base_fp && mk::restore_layers(layer_root, mask_root, sc3, idx, err) &&
              idx.frames["c"].base_fp == rebased, "restore takes the rebased base's fingerprint: " + err);
}

// The operator's own masks are 255 = DROP, and .base.png is a byte copy in
// that convention: a restore that skipped the flip would write the inverse.
void test_snapshot_restore_flipped() {
    Fixture f = make_dataset("snapshot_flipped", 64, 48, {"a", "c"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> d1 = box_layer(64, 48, 4, 4, 14, 14), k1 = box_layer(64, 48, 40, 20, 50, 30);
    const std::vector<uint8_t> d2 = box_layer(64, 48, 20, 20, 30, 30), k2 = box_layer(64, 48, 0, 0, 5, 5);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    idx.mask_flipped = true;
    std::string err;
    mk::PropagateRefusal refused;
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           d1.data(), k1.data(), idx, refused, err),
          "flipped restore: first layers on c: " + err);
    const std::vector<uint8_t> after_first = file_bytes(f.masks / "c.png");
    // The file is 255 = DROP: d1's box reads 255 in it, k1's box reads 0.
    std::vector<uint8_t> app_base = stencil_pixels(f.layer / "c.base.png"), app_want(app_base.size());
    mk::flip_polarity(app_base.data(), app_base.size());
    mk::composite(app_base.data(), d1.data(), k1.data(), app_base.size(), app_want.data());
    mk::flip_polarity(app_want.data(), app_want.size());
    const std::vector<uint8_t> first_px = stencil_pixels(f.masks / "c.png");
    check(first_px == app_want && first_px[5 * 64 + 5] == 255 && first_px[25 * 64 + 45] == 0,
          "flipped restore: propagate wrote c's base under the layers, in the file's polarity");
    // What a restore that forgot the flip writes: the file's own pixels
    // composited as if they were the app's, then flipped on the way out.
    std::vector<uint8_t> raw = stencil_pixels(f.layer / "c.base.png"), wrong(raw.size());
    mk::composite(raw.data(), d1.data(), k1.data(), raw.size(), wrong.data());
    mk::flip_polarity(wrong.data(), wrong.size());
    check(wrong != stencil_pixels(f.masks / "c.png"),
          "flipped restore: fixture: a restore without the flip would write other pixels");
    mk::LayerSnapshot sc;
    check(mk::snapshot_layers(layer_root, "c", idx, sc, err) && sc.had_entry && sc.had_drop,
          "flipped restore: c's snapshot has an entry, so restore takes its write path");
    check(mk::propagate_to(layer_root, mask_root, "c", (f.images / "c.jpg").string(), 64, 48,
                           d2.data(), k2.data(), idx, refused, err),
          "flipped restore: second layers on c");
    check(file_bytes(f.masks / "c.png") != after_first, "flipped restore: fixture: c changed");
    check(mk::restore_layers(layer_root, mask_root, sc, idx, err), "flipped restore: restore c: " + err);
    check(file_bytes(f.masks / "c.png") == after_first,
          "flipped restore: c's mask file is the first propagate's, byte for byte");
}

// A restore that fails late must leave the entry it found: a snapshot entry
// over the propagated mask reads Regenerated, and a reopen rebases .base.png.
enum class Late { BaseUnreadable, LayerMisfit, MaskWrite, IndexSave };

void restore_failure_arm(bool flipped, Late step, const char* step_name) {
    if (step != Late::LayerMisfit && !chmod_injects()) return;
    const std::string tag = std::string("restore failure (") + (flipped ? "flipped" : "unflipped") +
                            ", " + step_name + "): ";
    Fixture f = make_dataset(flipped ? "restore_fail_f" : "restore_fail_u", 64, 48, {"cam0/a", "cam0/c"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> d1 = box_layer(64, 48, 4, 4, 14, 14), k1 = box_layer(64, 48, 40, 20, 50, 30);
    const std::vector<uint8_t> d2 = box_layer(64, 48, 20, 20, 30, 30), k2 = box_layer(64, 48, 0, 0, 5, 5);
    const fs::path base = f.layer / "cam0" / "c.base.png", img = f.images / "cam0" / "c.jpg";
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    idx.mask_flipped = flipped;
    std::string err;
    mk::PropagateRefusal refused;
    check(mk::propagate_to(layer_root, mask_root, "cam0/c", img.string(), 64, 48, d1.data(), k1.data(),
                           idx, refused, err), tag + "first layers: " + err);
    const std::vector<uint8_t> base_before = file_bytes(base);
    mk::LayerSnapshot sc;
    check(mk::snapshot_layers(layer_root, "cam0/c", idx, sc, err) && sc.had_entry, tag + "snapshot");
    check(mk::propagate_to(layer_root, mask_root, "cam0/c", img.string(), 64, 48, d2.data(), k2.data(),
                           idx, refused, err), tag + "second layers: " + err);
    const mk::IndexEntry before = idx.frames["cam0/c"];
    check(before.composite_fp != sc.entry.composite_fp, tag + "fixture: the snapshot's entry is stale");
    const fs::path ro = step == Late::MaskWrite ? f.masks / "cam0" : f.layer;
    const auto ro_was = fs::status(ro).permissions();
    if (step == Late::BaseUnreadable) fs::permissions(base, fs::perms::none);
    if (step == Late::LayerMisfit) mk::encode_gray_png(synth_mask(32, 24, 3).data(), 32, 24, sc.drop_png);
    if (step == Late::MaskWrite || step == Late::IndexSave)
        fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec);
    const bool restored = mk::restore_layers(layer_root, mask_root, sc, idx, err);
    fs::permissions(base, fs::perms::owner_read | fs::perms::owner_write);
    fs::permissions(ro, ro_was);
    check(!restored && !err.empty(), tag + "restore fails: " + err);
    const mk::IndexEntry& now = idx.frames["cam0/c"];
    check(now.composite_fp == before.composite_fp && now.base_fp == before.base_fp &&
              now.saved_at == before.saved_at, tag + "the entry is the one restore found");
    mk::BaseState st;
    check(mk::recomposite_frame(layer_root, mask_root, "cam0/c", idx, st, err) || step == Late::LayerMisfit,
          tag + "recomposite: " + err);
    mk::MaskDoc doc;
    std::string warn;
    const bool loaded = doc.load(layer_root, mask_root, "cam0/c", 64, 48, idx, err, warn);
    check(file_bytes(base) == base_before, tag + ".base.png survives a reopen, byte for byte");
    // The editor must not show one picture while training reads another.
    std::vector<uint8_t> shown = doc.composite();
    if (flipped) mk::flip_polarity(shown.data(), shown.size());
    check(loaded && shown == stencil_pixels(f.masks / "cam0" / "c.png"),
          tag + "a reopen shows the mask on disk: " + warn);
}

// A rollback after a propagate that wrote its drop and failed on its keep:
// the mask is still the snapshot's, so a failed rollback keeps the snapshot's
// layers rather than putting the half-propagated ones back.
void test_restore_rollback_keeps_snapshot_layers() {
    if (!chmod_injects()) return;
    for (bool flipped : {false, true}) {
        const std::string tag = flipped ? "rollback (flipped): " : "rollback (unflipped): ";
        Fixture f = make_dataset(flipped ? "rollback_f" : "rollback_u", 64, 48, {"cam0/c"});
        const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
        const std::vector<uint8_t> d1 = box_layer(64, 48, 4, 4, 14, 14), zero(64 * 48, 0);
        const std::vector<uint8_t> d2 = box_layer(64, 48, 20, 20, 30, 30);
        const fs::path drop = f.layer / "cam0" / "c.drop.png", ro = f.masks / "cam0";
        mk::LayerIndex idx;
        idx.mask_root = mask_root;
        idx.mask_flipped = flipped;
        std::string err;
        mk::PropagateRefusal refused;
        check(mk::propagate_to(layer_root, mask_root, "cam0/c", (f.images / "cam0" / "c.jpg").string(), 64,
                               48, d1.data(), zero.data(), idx, refused, err), tag + "first layers: " + err);
        mk::LayerSnapshot sc;
        check(mk::snapshot_layers(layer_root, "cam0/c", idx, sc, err) && sc.had_entry, tag + "snapshot");
        std::vector<uint8_t> png;
        mk::encode_gray_png(d2.data(), 64, 48, png);
        mk::write_file_atomic(drop.string(), png.data(), png.size());   // the half-written propagate
        const auto ro_was = fs::status(ro).permissions();
        fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec);
        const bool restored = mk::restore_layers(layer_root, mask_root, sc, idx, err);
        fs::permissions(ro, ro_was);
        check(!restored, tag + "fixture: the rollback fails on the mask write");
        mk::MaskDoc doc;
        std::string warn;
        const bool loaded = doc.load(layer_root, mask_root, "cam0/c", 64, 48, idx, err, warn);
        std::vector<uint8_t> shown = doc.composite();
        if (flipped) mk::flip_polarity(shown.data(), shown.size());
        check(loaded && doc.drop() == d1 && shown == stencil_pixels(f.masks / "cam0" / "c.png"),
              tag + "the snapshot's layers stay, and a reopen shows the mask on disk");
    }
}

void test_restore_failure_keeps_base() {
    for (bool flipped : {false, true}) {
        restore_failure_arm(flipped, Late::BaseUnreadable, "base unreadable");
        restore_failure_arm(flipped, Late::LayerMisfit, "layer misfit");
        restore_failure_arm(flipped, Late::MaskWrite, "mask write");
        restore_failure_arm(flipped, Late::IndexSave, "index save");
    }
}

// Missing (an entry, no mask on disk) stays Missing through an undo, as it
// does through the propagate.
void test_restore_writes_no_mask() {
    for (bool flipped : {false, true}) {
        const std::string tag = flipped ? "no mask (flipped): " : "no mask (unflipped): ";
        Fixture f = make_dataset(flipped ? "restore_nomask_f" : "restore_nomask_u", 64, 48, {"a", "m"});
        const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
        const std::vector<uint8_t> d1 = box_layer(64, 48, 4, 4, 14, 14), k1 = box_layer(64, 48, 40, 20, 50, 30);
        const std::vector<uint8_t> d2 = box_layer(64, 48, 20, 20, 30, 30), k2 = box_layer(64, 48, 0, 0, 5, 5);
        mk::LayerIndex idx;
        idx.mask_root = mask_root;
        idx.mask_flipped = flipped;
        std::string err;
        mk::PropagateRefusal refused;
        const std::string img = (f.images / "m.jpg").string();
        check(mk::propagate_to(layer_root, mask_root, "m", img, 64, 48, d1.data(), k1.data(), idx, refused, err),
              tag + "first layers");
        fs::remove(f.masks / "m.png");
        mk::LayerSnapshot sm;
        check(mk::snapshot_layers(layer_root, "m", idx, sm, err) && sm.had_entry &&
                  mk::base_state(mask_root, "m", idx) == mk::BaseState::Missing, tag + "fixture: m is Missing");
        check(mk::propagate_to(layer_root, mask_root, "m", img, 64, 48, d2.data(), k2.data(), idx, refused, err) &&
                  !fs::exists(f.masks / "m.png"), tag + "propagate conjures no mask");
        check(mk::restore_layers(layer_root, mask_root, sm, idx, err), tag + "restore: " + err);
        check(!fs::exists(f.masks / "m.png"), tag + "restore conjures no mask");
        check(file_bytes(f.layer / "m.drop.png") == sm.drop_png && file_bytes(f.layer / "m.keep.png") == sm.keep_png,
              tag + "and the layers are the snapshot's");
    }
}

// The layer arms test_snapshot_restore does not reach: an absent keep, a
// snapshot read failure, an entry with no base, and orphan layers.
void test_snapshot_restore_edges() {
    Fixture f = make_dataset("snapshot_edges", 64, 48, {"a", "c", "o", "p", "cam0/n"});
    fs::remove(f.masks / "a.png");
    fs::remove(f.masks / "cam0" / "n.png");
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    const std::vector<uint8_t> d1 = box_layer(64, 48, 4, 4, 14, 14), k1 = box_layer(64, 48, 40, 20, 50, 30);
    const std::vector<uint8_t> d2 = box_layer(64, 48, 20, 20, 30, 30), k2 = box_layer(64, 48, 0, 0, 5, 5);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    std::string err;
    mk::PropagateRefusal refused;
    auto prop = [&](const char* key, const std::vector<uint8_t>& d, const std::vector<uint8_t>& k) {
        return mk::propagate_to(layer_root, mask_root, key, (f.images / (std::string(key) + ".jpg")).string(),
                                64, 48, d.data(), k.data(), idx, refused, err);
    };

    check(prop("c", d1, k1), "edges: first layers on c");
    fs::remove(f.layer / "c.keep.png");
    mk::LayerSnapshot sc;
    check(mk::snapshot_layers(layer_root, "c", idx, sc, err) && sc.had_drop && !sc.had_keep,
          "edges: snapshot of an entry whose keep layer is gone");
    check(prop("c", d2, k2) && stencil_pixels(f.layer / "c.keep.png") == k2, "edges: c carries k2");
    check(mk::restore_layers(layer_root, mask_root, sc, idx, err) &&
              stencil_pixels(f.layer / "c.keep.png") == std::vector<uint8_t>(64 * 48, 0),
          "edges: the keep layer the snapshot did not hold is zero, not k2");

    if (chmod_injects()) {
        for (const char* name : {"c.drop.png", "c.keep.png"}) {
            const fs::path layer = f.layer / name;
            fs::permissions(layer, fs::perms::none);
            mk::LayerSnapshot unreadable;
            const bool snapped = mk::snapshot_layers(layer_root, "c", idx, unreadable, err);
            fs::permissions(layer, fs::perms::owner_read | fs::perms::owner_write);
            check(!snapped && err == layer.string(),
                  std::string("edges: a snapshot read failure names ") + name + ": " + err);
        }
    }

    // A late failure on a key the index has lost leaves it lost, not the
    // snapshot's entry; the misfit layer needs no chmod.
    mk::LayerSnapshot lost;
    check(mk::snapshot_layers(layer_root, "c", idx, lost, err) && lost.had_entry, "edges: snapshot of c");
    mk::encode_gray_png(synth_mask(32, 24, 3).data(), 32, 24, lost.drop_png);
    lost.had_drop = true;
    const mk::IndexEntry c_entry = idx.frames["c"];
    idx.frames.erase("c");
    check(!mk::restore_layers(layer_root, mask_root, lost, idx, err) && idx.frames.count("c") == 0,
          "edges: a failed restore does not invent an entry the index did not have");
    idx.frames["c"] = c_entry;

    // a has no mask and so no base: restore puts back the entry and saves it.
    check(prop("a", d1, k1) && !fs::exists(f.layer / "a.base.png"), "edges: a takes layers only");
    mk::LayerSnapshot sa;
    check(mk::snapshot_layers(layer_root, "a", idx, sa, err) && sa.had_entry, "edges: snapshot of a");
    sa.entry.saved_at = "2000-01-01T00:00:00Z";
    check(prop("a", d2, k2), "edges: second layers on a");
    check(mk::restore_layers(layer_root, mask_root, sa, idx, err) &&
              idx.frames["a"].saved_at == sa.entry.saved_at, "edges: a's entry is the snapshot's: " + err);
    mk::LayerIndex reread;
    check(reread.load(layer_root, err) && reread.frames.count("a") == 1 &&
              reread.frames["a"].saved_at == sa.entry.saved_at, "edges: and it reached index.json");
    check(file_bytes(f.layer / "a.drop.png") == sa.drop_png && !fs::exists(f.masks / "a.png"),
          "edges: a's layers are the snapshot's, still with no mask");
    if (chmod_injects()) {
        // cam0/n's layers sit in cam0/, so only index.json's write is blocked.
        check(prop("cam0/n", d1, k1) && !fs::exists(f.layer / "cam0" / "n.base.png"), "edges: n takes layers only");
        mk::LayerSnapshot sn;
        check(mk::snapshot_layers(layer_root, "cam0/n", idx, sn, err) && sn.had_entry, "edges: snapshot of n");
        sn.entry.saved_at = "2000-01-01T00:00:00Z";
        check(prop("cam0/n", d2, k2), "edges: second layers on n");
        const mk::IndexEntry n_found = idx.frames["cam0/n"];
        const auto was = fs::status(f.layer).permissions();
        fs::permissions(f.layer, fs::perms::owner_read | fs::perms::owner_exec);
        const bool ok = mk::restore_layers(layer_root, mask_root, sn, idx, err);
        fs::permissions(f.layer, was);
        check(!ok && idx.frames["cam0/n"].saved_at == n_found.saved_at,
              "edges: a failed index save with no base puts back the entry it found");
    }

    // Orphan layers (no entry) are what the target carried; undo keeps them.
    const std::vector<uint8_t> original_o = file_bytes(f.masks / "o.png");
    std::vector<uint8_t> orphan;
    mk::encode_gray_png(box_layer(64, 48, 8, 8, 20, 20).data(), 64, 48, orphan);
    mk::write_file_atomic((f.layer / "o.drop.png").string(), orphan.data(), orphan.size());
    mk::LayerSnapshot so;
    check(mk::snapshot_layers(layer_root, "o", idx, so, err) && !so.had_entry && so.had_drop && !so.had_keep,
          "edges: snapshot of an orphan drop layer");
    check(prop("o", d2, k2), "edges: propagate onto o");
    check(mk::restore_layers(layer_root, mask_root, so, idx, err), "edges: restore o: " + err);
    check(file_bytes(f.layer / "o.drop.png") == orphan && !fs::exists(f.layer / "o.keep.png"),
          "edges: o's orphan drop layer is back, byte for byte, and no keep layer");
    check(file_bytes(f.masks / "o.png") == original_o && idx.frames.count("o") == 0 &&
              !fs::exists(f.layer / "o.base.png"), "edges: o's mask, entry and base are as before");
    std::vector<uint8_t> orphan_keep;
    mk::encode_gray_png(box_layer(64, 48, 30, 10, 40, 20).data(), 64, 48, orphan_keep);
    mk::write_file_atomic((f.layer / "p.keep.png").string(), orphan_keep.data(), orphan_keep.size());
    mk::LayerSnapshot sp;
    check(mk::snapshot_layers(layer_root, "p", idx, sp, err) && !sp.had_entry && !sp.had_drop && sp.had_keep &&
              prop("p", d2, k2) && mk::restore_layers(layer_root, mask_root, sp, idx, err),
          "edges: orphan keep on p, propagated over, restored: " + err);
    check(file_bytes(f.layer / "p.keep.png") == orphan_keep && !fs::exists(f.layer / "p.drop.png"),
          "edges: p's orphan keep layer is back, byte for byte, and no drop layer");
}

// ---------------------------------------------------------------------------
// Propagate through the session, and its undo
// ---------------------------------------------------------------------------

void test_session_propagate() {
    Fixture f = make_dataset("session_prop", 64, 48, {"cam0/a", "cam0/b", "cam0/c", "cam1/d"});
    write_jpg_rgb(f.images / "cam0" / "e.jpg", 32, 24, synth_rgb(32, 24, 9));
    write_png_gray(f.masks / "cam0" / "e.png", 32, 24, synth_mask(32, 24, 9));
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "cam0" / "b.png");
    const std::vector<uint8_t> original_d = file_bytes(f.masks / "cam1" / "d.png");
    const std::vector<uint8_t> base_b = stencil_pixels(f.masks / "cam0" / "b.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    check(s.frame_count() == 5 && s.frames()[3].key == "cam0/e" && s.frames()[4].key == "cam1/d",
          "five frames, e sorted into cam0");
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    const std::vector<uint8_t> drop_a = s.doc()->drop(), keep_a = s.doc()->keep();

    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    const mk::PropagateReport r = s.last_propagate();
    check(r.done == 2 && r.refused == 1 && r.refused_key == "cam0/e" && r.refused_w == 32 &&
              r.refused_h == 24 && r.w == 64 && r.h == 48, "report: b and c done, e refused with sizes");
    check(r.undoable && s.can_undo_propagate(), "undoable while the source is open");
    std::vector<uint8_t> want(base_b.size());
    mk::composite(base_b.data(), drop_a.data(), keep_a.data(), want.size(), want.data());
    check(stencil_pixels(f.masks / "cam0" / "b.png") == want, "b's composite is b's base under a's layers");
    check(file_bytes(f.masks / "cam1" / "d.png") == original_d, "the other camera is untouched");
    check(s.corrected_count() == 3, "a, b, c corrected");
    check(!s.doc()->dirty(), "the source was saved first");
    check(s.error().find("cam0/e") != std::string::npos, "the refusal names the frame");

    s.undo_propagate();
    settle(s);
    check(!s.can_undo_propagate() && s.last_propagate().done == 0, "record consumed, report reset");
    check(file_bytes(f.masks / "cam0" / "b.png") == original_b, "b's mask is byte-identical after undo");
    check(!fs::exists(f.layer / "cam0" / "b.drop.png") && !fs::exists(f.layer / "cam0" / "c.drop.png"),
          "targets' layers gone");
    check(s.corrected_count() == 1, "only a remains corrected");

    // Next, then entering the target drops the record.
    s.propagate(mk::PropagateScope::Next, 0, 0);
    check(s.propagate_total() == 1, "next: the bar counts one target while it runs");
    settle(s);
    check(s.last_propagate().done == 1 && s.can_undo_propagate(), "next: one target");
    check(s.propagate_total() == 0 && s.propagate_done() == 1,
          "next: the bar is gone once it finishes, having counted the target");
    s.go_to(1);
    settle(s);
    check(s.doc() && s.doc()->key() == "cam0/b" && s.doc()->drop() == drop_a, "b opens with a's layers");
    // The drop must be observed from the SOURCE. can_undo_propagate() is
    // false at any other frame whether or not the record still exists.
    s.go_to(0);
    settle(s);
    check(s.doc() && s.doc()->key() == "cam0/a" && !s.can_undo_propagate(),
          "the record was dropped on entering a target, not merely unmatched");
    s.go_to(1);
    settle(s);
    // Range from b covering c and e: c done, e refused.
    s.propagate(mk::PropagateScope::Range, 2, 3);
    settle(s);
    check(s.last_propagate().done == 1 && s.last_propagate().refused == 1, "range: c done, e refused");
    // No target in range. The text, not merely a non-empty _error: nothing
    // clears _error between propagates, so the refusal above satisfies
    // "not empty" on its own.
    s.propagate(mk::PropagateScope::Range, 4, 4);
    settle(s);
    check(s.last_propagate().done == 0 &&
              s.error() == spirula::i18n::msg::maskedit::prop_no_targets.get(),
          "empty range reports its own message: " + s.error());

    // The 256 MB drop, at a cap the fixture can reach. c carries layers now,
    // so the record is not empty and the cap is what decides.
    s.set_propagate_byte_cap_for_test(1);
    s.propagate(mk::PropagateScope::Range, 2, 2);
    settle(s);
    const mk::PropagateReport big = s.last_propagate();
    check(big.done == 1 && big.bytes > 1 && !big.undoable && !s.can_undo_propagate(),
          "over the cap: propagated, and not offered for undo");
    check(s.error() == spirula::i18n::format(spirula::i18n::msg::maskedit::prop_not_undoable,
                                             {(int)(big.bytes >> 20)}),
          "and the status line says why: " + s.error());
    s.set_propagate_byte_cap_for_test(mk::kMaxHistoryBytes);
    s.close();
}

// A directory where a layer file must land makes write_file_atomic's rename
// fail on every platform, and only after save_frame wrote the .base.png:
// the partial-write case, induced without touching any read path.
void test_session_propagate_failures() {
    Fixture f = make_dataset("session_prop_fail", 64, 48, {"cam0/a", "cam0/b", "cam0/c", "cam0/e"});
    write_jpg_rgb(f.images / "cam0" / "e.jpg", 32, 24, synth_rgb(32, 24, 9));
    write_png_gray(f.masks / "cam0" / "e.png", 32, 24, synth_mask(32, 24, 9));
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "cam0" / "b.png");
    const std::vector<uint8_t> original_c = file_bytes(f.masks / "cam0" / "c.png");
    const std::vector<uint8_t> base_b = stencil_pixels(f.masks / "cam0" / "b.png");
    const fs::path obstacle = f.layer / "cam0" / "c.drop.png";
    std::error_code ec;
    fs::create_directories(obstacle, ec);
    check(fs::is_directory(obstacle), "fixture: a directory blocks c's drop layer");

    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    const std::vector<uint8_t> drop_a = s.doc()->drop(), keep_a = s.doc()->keep();

    // Whole camera: b written, c fails mid-save and is rolled back, e refused.
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    mk::PropagateReport r = s.last_propagate();
    check(r.done == 1 && r.refused == 1 && r.failed == 1 && r.unrestored == 0,
          "report counts every attempted target: done 1, refused 1, failed 1");
    check(r.failed_key == "cam0/c" && r.failed_path.find("c.drop.png") != std::string::npos,
          "the failure names the frame and the file: " + r.failed_path);
    check(s.error().find("cam0/c") != std::string::npos, "the failure survives to the error line");
    check(file_bytes(f.masks / "cam0" / "c.png") == original_c, "c's mask is byte-identical: rolled back");
    check(!fs::exists(f.layer / "cam0" / "c.base.png") && !fs::exists(f.layer / "cam0" / "c.keep.png") &&
              !fs::exists(obstacle),
          "c's half-written base is gone and the obstacle was cleared by the rollback");
    std::vector<uint8_t> want(base_b.size());
    mk::composite(base_b.data(), drop_a.data(), keep_a.data(), want.size(), want.data());
    check(stencil_pixels(f.masks / "cam0" / "b.png") == want, "b was still propagated");
    check(!s.doc()->dirty(), "the source was saved by the job");
    check(s.corrected_count() == 2, "a and b corrected, c not");
    check(r.undoable && s.can_undo_propagate(), "undoable: the record holds b and c");
    s.undo_propagate();
    settle(s);
    check(file_bytes(f.masks / "cam0" / "b.png") == original_b && !fs::exists(f.layer / "cam0" / "b.drop.png"),
          "undo restores b byte for byte");
    check(file_bytes(f.masks / "cam0" / "c.png") == original_c && !fs::exists(f.layer / "cam0" / "c.drop.png"),
          "undo leaves c as it was");
    check(s.corrected_count() == 1 && !s.can_undo_propagate(), "only a remains; record consumed");

    // The failed target alone: nothing done, yet the snapshot is in the
    // record, so undo is offered and retries it.
    fs::create_directories(obstacle, ec);
    s.propagate(mk::PropagateScope::Range, 2, 2);
    settle(s);
    r = s.last_propagate();
    check(r.done == 0 && r.failed == 1 && r.unrestored == 0, "range on c alone: failed 1");
    check(r.undoable && s.can_undo_propagate(), "a failed target alone is still undoable");
    check(s.error().find("cam0/c") != std::string::npos, "error names c");
    s.undo_propagate();
    settle(s);
    check(!s.can_undo_propagate() && file_bytes(f.masks / "cam0" / "c.png") == original_c &&
              !fs::exists(f.layer / "cam0" / "c.base.png"),
          "undo after a lone failure leaves c clean");

    // A rollback that cannot run. c is corrected first so its snapshot has
    // an ENTRY and restore_layers takes its write path, not revert_frame's;
    // a .base.png that is not a PNG then fails frame_size and load_stencil.
    s.propagate(mk::PropagateScope::Range, 2, 2);
    settle(s);
    check(s.last_propagate().done == 1 && fs::exists(f.layer / "cam0" / "c.base.png"),
          "fixture: c is corrected, so its snapshot has an entry");
    const std::vector<uint8_t> junk(64, 0x7f);
    check(mk::write_file_atomic((f.layer / "cam0" / "c.base.png").string(), junk.data(), junk.size()),
          "fixture: c's base is not a PNG");
    s.propagate(mk::PropagateScope::Range, 2, 2);
    settle(s);
    r = s.last_propagate();
    check(r.done == 0 && r.failed == 1 && r.unrestored == 1, "a rollback that failed is counted");
    check(r.failed_path.find("c.base.png") != std::string::npos,
          "the failure names the base, not the image it was handed: " + r.failed_path);
    check(r.unrestored_key == "cam0/c" &&
              s.error() == spirula::i18n::format(spirula::i18n::msg::maskedit::prop_failed_not_restored,
                                                 {r.unrestored_key, r.unrestored_path, 1}),
          "and the not-restored wording is the one shown: " + s.error());
    fs::remove(f.layer / "cam0" / "c.base.png", ec);
    s.undo_propagate();
    settle(s);

    // The SOURCE's own save fails: nothing is propagated and the save's
    // error is what the status line shows.
    const fs::path src_obstacle = f.layer / "cam0" / "a.drop.png";
    fs::remove(src_obstacle, ec);
    fs::create_directories(src_obstacle, ec);
    gui::ShapeStroke box2;
    box2.kind = gui::ShapeKind::Box;
    box2.pts = std::vector<float>{20.0f, 20.0f, 30.0f, 30.0f};
    s.commit_stroke(box2, mk::Paint::ForceKeep, m);
    check(s.doc()->dirty(), "fixture: the source is dirty again");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    r = s.last_propagate();
    check(r.done == 0 && r.refused == 0 && r.failed == 0, "a failed source save propagates nothing");
    check(s.status().empty(), "and does not leave the working line up: " + s.status());
    check(s.error().find("a.drop.png") != std::string::npos, "the save's error survives: " + s.error());
    check(file_bytes(f.masks / "cam0" / "b.png") == original_b, "b untouched after the aborted propagate");
    check(!s.can_undo_propagate(), "nothing to undo");
    check(s.doc()->dirty(), "the source is still dirty");
    fs::remove_all(src_obstacle, ec);
    s.close();
}

// Undo propagate is the safety net for propagating over handheld stills, and
// masks in the wild are often 255 = DROP: it must put a flipped target back,
// including one that had corrections of its own before the propagate.
void test_session_propagate_flipped() {
    Fixture f = make_dataset("session_prop_flipped", 64, 48, {"cam0/a", "cam0/b", "cam0/c"});
    const std::vector<uint8_t> original_b = file_bytes(f.masks / "cam0" / "b.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), true, err),
          "flipped propagate: open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> b_own = file_bytes(f.masks / "cam0" / "b.png");
    check(s.last_propagate().done == 1 && b_own != original_b,
          "flipped propagate: fixture: b carries corrections before the propagate under test");
    gui::ShapeStroke box2 = box;
    box2.pts = std::vector<float>{30.0f, 20.0f, 50.0f, 40.0f};
    s.commit_stroke(box2, mk::Paint::ForceKeep, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    check(s.last_propagate().done == 1 && s.can_undo_propagate() &&
              file_bytes(f.masks / "cam0" / "b.png") != b_own,
          "flipped propagate: the propagate replaced b's corrections");
    // In the file's polarity, 255 = DROP: a's drop box reads 255, its keep box 0.
    const std::vector<uint8_t> px = stencil_pixels(f.masks / "cam0" / "b.png");
    check(px[5 * 64 + 5] == 255 && px[30 * 64 + 40] == 0,
          "flipped propagate: b's file reads a's drop as 255 and a's keep as 0");
    s.undo_propagate();
    settle(s);
    check(file_bytes(f.masks / "cam0" / "b.png") == b_own,
          "flipped propagate: undo puts b back to its own corrections, byte for byte");
    s.close();
}

// Frame keys repeat across datasets. A propagate record that outlived close()
// would write one dataset's layer bytes into another's files on Undo.
void test_propagate_forgotten_on_reopen() {
    Fixture x = make_dataset("prop_reopen_x", 64, 48, {"cam0/a", "cam0/b"});
    Fixture y = make_dataset("prop_reopen_y", 64, 48, {"cam0/a", "cam0/b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(x.root.string(), x.images.string(), x.masks.string(), false, err),
          "reopen: open x: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    gui::ShapeStroke box2 = box;
    box2.pts = std::vector<float>{30.0f, 20.0f, 50.0f, 40.0f};
    s.commit_stroke(box2, mk::Paint::ForceKeep, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    check(s.can_undo_propagate() && s.last_propagate().bytes > 0,
          "reopen: fixture: x's record holds layer bytes an undo would write");
    s.close();
    const std::vector<uint8_t> y_b = file_bytes(y.masks / "cam0" / "b.png");
    check(s.open(y.root.string(), y.images.string(), y.masks.string(), false, err),
          "reopen: open y: " + err);
    settle(s);
    check(s.doc() && s.doc()->key() == "cam0/a",
          "reopen: fixture: y opens on the key x's record names as its source");
    check(!s.can_undo_propagate(), "reopen: another dataset is not offered x's undo");
    s.undo_propagate();
    settle(s);
    check(file_bytes(y.masks / "cam0" / "b.png") == y_b &&
              !fs::exists(y.layer / "cam0" / "b.drop.png"),
          "reopen: an undo in y writes nothing into y's files");
    check(s.last_propagate().done == 0, "reopen: the report was forgotten with the record");
    s.close();
}

// An Undo propagate that cannot write a target's mask. The editor must not
// then show one picture while training reads another, and Undo must retry.
void undo_failure_arm(bool flipped) {
    if (!chmod_injects()) return;
    const std::string tag = flipped ? "undo failure (flipped): " : "undo failure (unflipped): ";
    Fixture f = make_dataset(flipped ? "undo_fail_f" : "undo_fail_u", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png", b_drop = f.layer / "cam0" / "b.drop.png",
                   b_keep = f.layer / "cam0" / "b.keep.png", ro = f.masks / "cam0";
    const auto ro_was = fs::status(ro).permissions();
    const auto lock = [&] { fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec); };
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), tag + "open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> b_own = file_bytes(b_png);
    gui::ShapeStroke box2 = box;
    box2.pts = std::vector<float>{30.0f, 20.0f, 50.0f, 40.0f};
    s.commit_stroke(box2, mk::Paint::ForceKeep, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> b_prop = file_bytes(b_png), drop_prop = file_bytes(b_drop),
                               keep_prop = file_bytes(b_keep);
    const uint8_t dropped = flipped ? 255 : 0, kept = flipped ? 0 : 255;
    const std::vector<uint8_t> px = stencil_pixels(b_png);
    check(s.can_undo_propagate() && b_prop != b_own && px[5 * 64 + 5] == dropped &&
              px[30 * 64 + 40] == kept,
          tag + "fixture: b holds a's drop and keep, in the file's polarity");
    lock();
    s.undo_propagate();
    settle(s);
    fs::permissions(ro, ro_was);
    const std::string b_path = mk::mask_file(s.mask_root(), "cam0/b");
    check(s.error() == spirula::i18n::format(spirula::i18n::msg::maskedit::prop_undo_failed,
                                             {std::string("cam0/b"), b_path}),
          tag + "the failure names the frame and the file: " + s.error());
    check(file_bytes(b_png) == b_prop && file_bytes(b_drop) == drop_prop && file_bytes(b_keep) == keep_prop,
          tag + "b is left wholly as the propagate left it, layers included");
    check(s.can_undo_propagate(), tag + "Undo propagate is offered again, to retry");
    s.undo_propagate();
    settle(s);
    check(file_bytes(b_png) == b_own && !s.can_undo_propagate(),
          tag + "the retry puts b back to its own corrections, byte for byte");
    const std::vector<uint8_t> back = stencil_pixels(b_png);
    check(back[5 * 64 + 5] == dropped && back[30 * 64 + 40] == synth_mask(64, 48, 1)[30 * 64 + 40],
          tag + "b's own drop reads dropped and a's keep is gone from it");

    // Fail again, then open b: the record goes, and b shows
    // exactly what is on disk.
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    lock();
    s.undo_propagate();
    settle(s);
    fs::permissions(ro, ro_was);
    s.go_to(1);
    settle(s);
    std::vector<uint8_t> shown = s.doc() ? s.doc()->composite() : std::vector<uint8_t>();
    if (flipped) mk::flip_polarity(shown.data(), shown.size());
    check(s.doc() && s.doc()->key() == "cam0/b" && !s.doc()->dirty() && shown == stencil_pixels(b_png),
          tag + "b opens showing the mask training reads");
    s.go_to(0);
    settle(s);
    check(!s.can_undo_propagate(), tag + "and entering b dropped the record");
    s.close();
}

void test_undo_propagate_failure() {
    undo_failure_arm(false);
    undo_failure_arm(true);
}

// Opens `f` and drops a's box at (4,4)-(14,14), the source every fix-round test uses.
void open_and_drop(mk::MaskSession& s, const Fixture& f, bool flipped, const std::string& tag) {
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), tag + "open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
}

void keep_box(mk::MaskSession& s) {
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{30.0f, 20.0f, 50.0f, 40.0f};
    s.commit_stroke(box, mk::Paint::ForceKeep, m);
}

// Revert all removes every correction; an Undo propagate after it would write
// the targets' old layers and entries back under masks it never recomposites.
void test_revert_all_drops_propagate_record() {
    for (bool flipped : {false, true}) {
        const std::string tag = flipped ? "revert all (flipped): " : "revert all (unflipped): ";
        Fixture f = make_dataset(flipped ? "prop_revall_f" : "prop_revall_u", 64, 48, {"cam0/a", "cam0/b"});
        const fs::path b_png = f.masks / "cam0" / "b.png";
        const std::vector<uint8_t> original_b = file_bytes(b_png);
        mk::MaskSession s;
        open_and_drop(s, f, flipped, tag);
        s.propagate(mk::PropagateScope::Next, 0, 0);
        settle(s);
        keep_box(s);
        s.propagate(mk::PropagateScope::Next, 0, 0);
        settle(s);
        check(s.can_undo_propagate() && s.last_propagate().bytes > 0,
              tag + "fixture: the record holds b's own corrections");
        s.revert_every_frame();
        settle(s);
        check(s.doc() && s.doc()->key() == "cam0/a" && file_bytes(b_png) == original_b &&
                  s.corrected_count() == 0, tag + "fixture: back at the source, every frame reverted");
        check(!s.can_undo_propagate(), tag + "Undo propagate is not offered after Revert all");
        s.undo_propagate();
        settle(s);
        check(file_bytes(b_png) == original_b && !fs::exists(f.layer / "cam0" / "b.drop.png") &&
                  s.corrected_count() == 0, tag + "and an undo writes nothing");
        s.close();
    }
}

// A never-edited target: its undo is a revert_frame, which writes the mask and
// then removes the files. A removal that fails must leave the mask propagated.
void undo_failure_no_entry_arm(bool flipped) {
    if (!chmod_injects()) return;
    const std::string tag = flipped ? "undo failure, no entry (flipped): " : "undo failure, no entry (unflipped): ";
    Fixture f = make_dataset(flipped ? "undo_fail_ne_f" : "undo_fail_ne_u", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png", ro = f.layer / "cam0";
    const std::vector<uint8_t> original_b = file_bytes(b_png);
    mk::MaskSession s;
    open_and_drop(s, f, flipped, tag);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> b_prop = file_bytes(b_png);
    const auto ro_was = fs::status(ro).permissions();
    const auto undo_locked = [&] {
        fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec);
        s.undo_propagate();
        settle(s);
        fs::permissions(ro, ro_was);
    };
    check(s.can_undo_propagate() && b_prop != original_b, tag + "fixture: b was propagated, no entry before");
    undo_locked();
    check(s.error().find("cam0/b") != std::string::npos && file_bytes(b_png) == b_prop &&
              fs::exists(f.layer / "cam0" / "b.drop.png"),
          tag + "b is left as the propagate left it, mask included: " + s.error());
    check(s.can_undo_propagate(), tag + "Undo propagate is offered again");
    s.undo_propagate();
    settle(s);
    check(file_bytes(b_png) == original_b && !fs::exists(f.layer / "cam0" / "b.base.png") &&
              !s.can_undo_propagate(), tag + "the retry puts b back byte for byte");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    undo_locked();
    s.go_to(1);
    settle(s);
    std::vector<uint8_t> shown = s.doc() ? s.doc()->composite() : std::vector<uint8_t>();
    if (flipped) mk::flip_polarity(shown.data(), shown.size());
    check(s.doc() && !s.doc()->dirty() && shown == stencil_pixels(b_png) && file_bytes(b_png) == b_prop,
          tag + "b opens showing the mask training reads");
    s.close();
}

void test_undo_propagate_failure_no_entry() {
    undo_failure_no_entry_arm(false);
    undo_failure_no_entry_arm(true);
}

// macOS's user immutable flag: the file can be neither removed nor replaced,
// while its siblings can. Cleared when this goes out of scope.
struct Immutable {
    fs::path p;
    bool on = false;
    explicit Immutable(const fs::path& path) : p(path) {
#if defined(__APPLE__)
        on = geteuid() != 0 && ::chflags(p.c_str(), UF_IMMUTABLE) == 0;
#endif
    }
    ~Immutable() { clear(); }
    void clear() {
#if defined(__APPLE__)
        if (on) ::chflags(p.c_str(), 0);
#endif
        on = false;
    }
};

bool immutable_injects() {
#if defined(__APPLE__)
    if (geteuid() != 0) return true;
#endif
    static bool said = false;
    if (!said) std::printf("skip immutable-file arms: skipped (not macOS, or root)\n");
    said = true;
    return false;
}

// One layer that will not go: revert_frame removes the base and the keep, then
// fails. Every file goes back on its own, the resisting one included.
void undo_one_layer_resists_arm(bool flipped) {
    if (!immutable_injects()) return;
    const std::string tag = flipped ? "one layer resists (flipped): " : "one layer resists (unflipped): ";
    Fixture f = make_dataset(flipped ? "undo_uchg_f" : "undo_uchg_u", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png", dir = f.layer / "cam0";
    const fs::path b_base = dir / "b.base.png", b_drop = dir / "b.drop.png", b_keep = dir / "b.keep.png";
    mk::MaskSession s;
    open_and_drop(s, f, flipped, tag);
    keep_box(s);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> mask_p = file_bytes(b_png), base_p = file_bytes(b_base),
                               drop_p = file_bytes(b_drop), keep_p = file_bytes(b_keep);
    check(!base_p.empty() && !drop_p.empty() && !keep_p.empty() && s.can_undo_propagate(),
          tag + "fixture: b was propagated with a drop and a keep, no entry before");
    {
        Immutable lock(b_drop);
        check(lock.on, tag + "fixture: b's drop layer is immutable");
        s.undo_propagate();
        settle(s);
    }
    check(s.error().find("cam0/b") != std::string::npos && s.can_undo_propagate(),
          tag + "the failure is named and the undo offered again: " + s.error());
    check(file_bytes(b_png) == mask_p && file_bytes(b_base) == base_p && file_bytes(b_drop) == drop_p &&
              file_bytes(b_keep) == keep_p,
          tag + "the base, both layers and the mask are the propagate's, byte for byte");
    s.go_to(1);
    settle(s);
    std::vector<uint8_t> shown = s.doc() ? s.doc()->composite() : std::vector<uint8_t>();
    if (flipped) mk::flip_polarity(shown.data(), shown.size());
    check(s.doc() && !s.doc()->dirty() && shown == stencil_pixels(b_png),
          tag + "b opens showing the mask training reads");
    s.close();
}

void test_undo_propagate_one_layer_resists() {
    undo_one_layer_resists_arm(false);
    undo_one_layer_resists_arm(true);
}

// Revert all queued right behind a propagate: the record the propagate's job
// sets is dropped by the revert's job. Whether the propagate's job has run by
// the time of the UI-thread drop varies, so the pair is repeated kQueued times.
void test_revert_all_drops_queued_record() {
    constexpr int kQueued = 10;
    Fixture f = make_dataset("prop_revall_queued", 64, 48, {"cam0/a", "cam0/b"});
    mk::MaskSession s;
    open_and_drop(s, f, false, "queued revert all: ");
    int survived = 0;
    for (int i = 0; i < kQueued; i++) {
        if (i) {
            mk::Mapping m;
            gui::ShapeStroke box;
            box.kind = gui::ShapeKind::Box;
            box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
            s.commit_stroke(box, mk::Paint::ForceDrop, m);
        }
        s.propagate(mk::PropagateScope::Next, 0, 0);
        s.revert_every_frame();
        settle(s);
        if (!s.doc() || s.doc()->key() != "cam0/a") break;
        survived += s.can_undo_propagate() ? 1 : 0;
    }
    check(s.doc() && s.doc()->key() == "cam0/a" && survived == 0,
          "queued revert all: no record outlives a Revert all queued behind its propagate (" +
              std::to_string(survived) + " of " + std::to_string(kQueued) + ")");
    s.close();
}

// The same undo failing only at its index write: the files are already gone,
// so nothing is put back and masks/ already holds the undone mask.
void test_undo_propagate_index_save_fails() {
    if (!chmod_injects()) return;
    Fixture f = make_dataset("undo_fail_index", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png";
    const std::vector<uint8_t> original_b = file_bytes(b_png);
    mk::MaskSession s;
    open_and_drop(s, f, false, "index save: ");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const auto was = fs::status(f.layer).permissions();
    fs::permissions(f.layer, fs::perms::owner_read | fs::perms::owner_exec);
    s.undo_propagate();
    settle(s);
    fs::permissions(f.layer, was);
    check(s.error().find("index.json") != std::string::npos, "index save: fixture: only index.json failed: " +
                                                                 s.error());
    check(file_bytes(b_png) == original_b && !fs::exists(f.layer / "cam0" / "b.base.png") &&
              !fs::exists(f.layer / "cam0" / "b.drop.png"),
          "index save: the files stay gone, not put back beside an index without b");
    s.close();
}

// Several of each outcome in one camera, and a record whose second target is
// the one opened: every rule a single-target fixture cannot tell apart.
void test_propagate_camera_many() {
    Fixture f = make_dataset("prop_many", 64, 48,
                             {"cam0/a", "cam0/b", "cam0/c", "cam0/d", "cam0/e", "cam1/x"});
    for (const char* k : {"f", "g"}) {
        write_jpg_rgb(f.images / "cam0" / (std::string(k) + ".jpg"), 32, 24, synth_rgb(32, 24, 9));
        write_png_gray(f.masks / "cam0" / (std::string(k) + ".png"), 32, 24, synth_mask(32, 24, 9));
    }
    std::error_code ec;
    const fs::path d_drop = f.layer / "cam0" / "d.drop.png", e_keep = f.layer / "cam0" / "e.keep.png";
    fs::create_directories(d_drop, ec);
    fs::create_directories(e_keep, ec);
    mk::MaskSession s;
    open_and_drop(s, f, false, "many: ");
    check(s.frame_count() == 8 && s.frames()[2].key == "cam0/c" && s.frames()[7].key == "cam1/x",
          "many: fixture: a..g in cam0, x in cam1");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    const mk::PropagateReport r = s.last_propagate();
    check(r.done == 2 && r.failed == 2 && r.refused == 2 && r.unrestored == 0,
          "many: two done, two failed, two refused, each counted");
    check(!fs::exists(d_drop) && !fs::exists(e_keep) && !fs::exists(f.layer / "cam0" / "e.base.png"),
          "many: both rollbacks cleared their obstacle and wrote no layer back");
    s.go_to(7);
    settle(s);
    check(s.doc() && s.doc()->key() == "cam1/x" && !s.can_undo_propagate(),
          "many: a frame that is neither source nor target is not offered the undo");
    s.go_to(0);
    settle(s);
    check(s.can_undo_propagate(), "many: back at the source, the record is still there");
    s.go_to(2);
    settle(s);
    s.go_to(0);
    settle(s);
    check(s.doc() && s.doc()->key() == "cam0/a" && !s.can_undo_propagate(),
          "many: opening the second target dropped the record");
    s.close();
}

// The status line names a frame left not put back even when a stray base or a
// clean rollback came first, and promises a retry only while one is possible.
void test_propagate_names_unrestored() {
    Fixture f = make_dataset("prop_unrestored", 64, 48, {"cam0/a", "cam0/b", "cam0/c", "cam0/d"});
    write_png_gray(f.layer / "cam0" / "b.base.png", 64, 48, synth_mask(64, 48, 50));
    std::error_code ec;
    const fs::path c_drop = f.layer / "cam0" / "c.drop.png", d_base = f.layer / "cam0" / "d.base.png";
    mk::MaskSession s;
    open_and_drop(s, f, false, "unrestored: ");
    s.propagate(mk::PropagateScope::Range, 3, 3);
    settle(s);
    check(s.last_propagate().done == 1 && fs::exists(d_base), "unrestored: fixture: d has an entry");
    const std::vector<uint8_t> junk(64, 0x7f);
    mk::write_file_atomic(d_base.string(), junk.data(), junk.size());
    using namespace spirula::i18n;
    for (bool capped : {false, true}) {
        const std::string tag = capped ? "unrestored (over the cap): " : "unrestored: ";
        fs::create_directories(c_drop, ec);
        s.set_propagate_byte_cap_for_test(capped ? 1 : mk::kMaxHistoryBytes);
        s.propagate(mk::PropagateScope::Camera, 0, 0);
        settle(s);
        const mk::PropagateReport r = s.last_propagate();
        check(r.failed == 3 && r.unrestored == 1 && r.failed_key == "cam0/b" && r.failed_stray &&
                  r.unrestored_key == "cam0/d" && r.undoable == !capped,
              tag + "fixture: b stray, c rolled back, d not put back");
        check(s.error() == format(capped ? msg::maskedit::prop_failed_not_restored_final
                                         : msg::maskedit::prop_failed_not_restored,
                                  {r.unrestored_key, r.unrestored_path, 1}),
              tag + "d is the frame named: " + s.error());
    }
    s.set_propagate_byte_cap_for_test(mk::kMaxHistoryBytes);
    fs::remove(d_base, ec);
    s.close();
}

// A rollback that fails after the propagate really wrote: the source is saved
// first, so only c's mask write meets the read-only folder, after c's layers
// went in; the rollback puts the drop back and then fails on the same write.
void test_propagate_unrestored_after_a_write() {
    if (!chmod_injects()) return;
    Fixture f = make_dataset("prop_wrote", 64, 48, {"cam0/a", "cam0/c"});
    const fs::path c_drop = f.layer / "cam0" / "c.drop.png", ro = f.masks / "cam0";
    mk::MaskSession s;
    open_and_drop(s, f, false, "wrote: ");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> drop_before = file_bytes(c_drop), c_before = file_bytes(ro / "c.png");
    gui::ShapeStroke big;
    big.kind = gui::ShapeKind::Box;
    big.pts = std::vector<float>{20.0f, 20.0f, 40.0f, 40.0f};
    mk::Mapping m;
    s.commit_stroke(big, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    check(!s.doc()->dirty(), "wrote: fixture: the source is saved, so the job writes nothing of its own");
    const auto was = fs::status(ro).permissions();
    fs::permissions(ro, fs::perms::owner_read | fs::perms::owner_exec);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    fs::permissions(ro, was);
    const mk::PropagateReport r = s.last_propagate();
    check(r.failed == 1 && r.unrestored == 1 && r.unrestored_path.find("c.png") != std::string::npos,
          "wrote: c failed at its mask, after its layers, and the rollback failed too: " + r.unrestored_path);
    check(file_bytes(c_drop) == drop_before && file_bytes(ro / "c.png") == c_before,
          "wrote: the rollback still put c's drop back, and the mask was never touched");
    s.close();
}

// Restore never reads the layer it is about to replace, so one that will not
// read (mode 000 in a writable folder) does not stop the undo.
void test_undo_propagate_unreadable_layer() {
    if (!chmod_injects()) return;
    Fixture f = make_dataset("prop_unreadable", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png", b_drop = f.layer / "cam0" / "b.drop.png";
    mk::MaskSession s;
    open_and_drop(s, f, false, "unreadable: ");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::vector<uint8_t> b_own = file_bytes(b_png);
    keep_box(s);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    fs::permissions(b_drop, fs::perms::none);
    std::vector<uint8_t> probe;
    check(!mk::read_file(b_drop.string(), probe), "unreadable: fixture: b's drop layer will not read");
    s.undo_propagate();
    settle(s);
    fs::permissions(b_drop, fs::perms::owner_read | fs::perms::owner_write);
    check(file_bytes(b_png) == b_own && !s.can_undo_propagate(),
          "unreadable: the undo still puts b back byte for byte: " + s.error());
    s.close();
}

// A .base.png with no index entry: restore would revert through it, copying
// it over whatever mask is on disk. Such a target is not propagated at all.
void test_propagate_refuses_stray_base() {
    for (bool flipped : {false, true}) {
        const std::string tag = flipped ? "stray base (flipped): " : "stray base (unflipped): ";
        Fixture f = make_dataset(flipped ? "prop_stray_f" : "prop_stray_u", 64, 48, {"cam0/a", "cam0/b"});
        const fs::path stray = f.layer / "cam0" / "b.base.png";
        write_png_gray(stray, 64, 48, synth_mask(64, 48, 50));
        const std::vector<uint8_t> b_mask = file_bytes(f.masks / "cam0" / "b.png"), b_base = file_bytes(stray);
        check(b_mask != b_base, tag + "fixture: the stray base is not b's mask");
        mk::MaskSession s;
        std::string err;
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), tag + "open: " + err);
        settle(s);
        mk::Mapping m;
        gui::ShapeStroke box;
        box.kind = gui::ShapeKind::Box;
        box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
        s.commit_stroke(box, mk::Paint::ForceDrop, m);
        s.propagate(mk::PropagateScope::Next, 0, 0);
        settle(s);
        const mk::PropagateReport r = s.last_propagate();
        check(r.done == 0 && r.failed == 1 && r.failed_key == "cam0/b" && r.failed_path.find("b.base.png") !=
                  std::string::npos && !r.undoable, tag + "b is counted failed, named by its base");
        check(file_bytes(f.masks / "cam0" / "b.png") == b_mask && file_bytes(stray) == b_base &&
                  !fs::exists(f.layer / "cam0" / "b.drop.png"), tag + "and nothing of b was written");
        check(s.error() == spirula::i18n::format(spirula::i18n::msg::maskedit::prop_failed_stray_base,
                                                 {r.failed_key, r.failed_path}),
              tag + "the status line says why: " + s.error());
        s.close();
    }
}

// Byte for byte does not hold for a target whose mask was regenerated before
// the propagate: its load rebases it, so the undo gives what opening it would.
void test_undo_propagate_regenerated_target() {
    for (bool flipped : {false, true}) {
        const std::string tag = flipped ? "regenerated (flipped): " : "regenerated (unflipped): ";
        Fixture f = make_dataset(flipped ? "prop_regen_f" : "prop_regen_u", 64, 48, {"cam0/a", "cam0/b"});
        mk::MaskSession s;
        std::string err;
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), tag + "open: " + err);
        settle(s);
        mk::Mapping m;
        gui::ShapeStroke box;
        box.kind = gui::ShapeKind::Box;
        box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
        s.commit_stroke(box, mk::Paint::ForceDrop, m);
        const std::vector<uint8_t> d1 = s.doc()->drop(), k1 = s.doc()->keep();
        s.propagate(mk::PropagateScope::Next, 0, 0);
        settle(s);
        const std::vector<uint8_t> regen = synth_mask(64, 48, 77);
        write_png_gray(f.masks / "cam0" / "b.png", 64, 48, regen);
        const std::vector<uint8_t> on_disk = file_bytes(f.masks / "cam0" / "b.png");
        gui::ShapeStroke box2 = box;
        box2.pts = std::vector<float>{30.0f, 20.0f, 50.0f, 40.0f};
        s.commit_stroke(box2, mk::Paint::ForceKeep, m);
        s.propagate(mk::PropagateScope::Next, 0, 0);
        settle(s);
        s.undo_propagate();
        settle(s);
        std::vector<uint8_t> app = regen, want(regen.size());
        if (flipped) mk::flip_polarity(app.data(), app.size());
        mk::composite(app.data(), d1.data(), k1.data(), app.size(), want.data());
        if (flipped) mk::flip_polarity(want.data(), want.size());
        check(file_bytes(f.layer / "cam0" / "b.base.png") == on_disk &&
                  stencil_pixels(f.masks / "cam0" / "b.png") == want,
              tag + "undo gives the regenerated base under b's own layers");
        check(file_bytes(f.masks / "cam0" / "b.png") != on_disk,
              tag + "which is not the file on disk before the propagate: excluded from byte for byte");
        s.close();
    }
}

// The eraser, through the same call the panel makes. The base drops a 16x12
// block, so force-keep and clear give DIFFERENT kept counts over it -- which
// is the only fixture shape that can tell the two readings of "eraser" apart.
void test_session_eraser() {
    const int W = 64, H = 48;
    Fixture f = make_dataset("session_eraser", W, H, {"a"}, /*with_masks=*/false);
    std::vector<uint8_t> base((size_t)W * H, 255);
    for (int y = 0; y < 12; y++)
        for (int x = 0; x < 16; x++) base[(size_t)y * W + x] = 0;
    write_png_gray(f.masks / "a.png", W, H, base);

    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "eraser fixture opens: " + err);
    settle(s);
    check(s.doc() != nullptr, "frame loaded");
    const int64_t kAll = (int64_t)W * H;           // 3072
    const int64_t kBase = kAll - 16 * 12;          // 2880
    check(s.doc()->kept() == kBase, "the base drops the 16x12 block and nothing else");

    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke over_block;                   // covers the block with margin
    over_block.kind = gui::ShapeKind::Box;
    over_block.pts = std::vector<float>{0.0f, 0.0f, 20.0f, 16.0f};

    s.commit_stroke(over_block, mk::MaskSession::paint_for(false, false, /*erasing=*/true), m);
    check(s.doc()->kept() == kAll,
          "the eraser force-keeps what the BASE dropped: every pixel of the frame is kept");
    s.undo();
    check(s.doc()->kept() == kBase, "undo puts the block back");
    // The same stroke as Shift+Ctrl. Clear returns those pixels to the base,
    // which still drops them -- so this fixture separates the two readings,
    // and an eraser wired to Paint::Clear would fail the check above.
    s.commit_stroke(over_block, mk::MaskSession::paint_for(true, true, /*erasing=*/true), m);
    check(s.doc()->kept() == kBase,
          "clear over the same box leaves the block dropped: clear is not the eraser");
    s.undo();
    check(s.doc()->kept() == kBase, "back to the base");

    // The round trip the operator asked for: brush, then erase the same area.
    gui::ShapeStroke inside;
    inside.kind = gui::ShapeKind::Box;
    inside.pts = std::vector<float>{30.0f, 20.0f, 40.0f, 30.0f};
    s.commit_stroke(inside, mk::MaskSession::paint_for(false, false, /*erasing=*/false), m);
    const int64_t after_brush = s.doc()->kept();
    check(after_brush < kBase, "the brush drops something");
    s.commit_stroke(inside, mk::MaskSession::paint_for(false, false, /*erasing=*/true), m);
    check(s.doc()->kept() == kBase, "the eraser puts back exactly what the brush took");

    // paint_now reads the session's own flag, which is what the two panel
    // call sites use -- a tool that highlights but paints the other mode
    // would pass every static check above.
    s.set_erasing(false);
    check(s.paint_now(false, false) == mk::Paint::ForceDrop, "paint_now, brush selected: drop");
    s.set_erasing(true);
    check(s.paint_now(false, false) == mk::Paint::ForceKeep, "paint_now, eraser selected: keep");
    check(s.paint_now(false, true) == mk::Paint::ForceDrop, "paint_now, eraser + Ctrl: drop");
    // The pen and SAM paint with the brush's grammar, not the eraser's.
    s.set_mode(mk::CanvasMode::Path);
    check(s.paint_now(false, false) == mk::Paint::ForceDrop, "paint_now, pen selected: drop");
    check(s.paint_now(false, true) == mk::Paint::ForceKeep, "paint_now, pen + Ctrl: keep");
    s.set_mode(mk::CanvasMode::Sam);
    check(s.paint_now(false, false) == mk::Paint::ForceDrop, "paint_now, SAM selected: drop");
    check(s.paint_now(false, true) == mk::Paint::ForceKeep, "paint_now, SAM + Ctrl: keep");
    check(s.paint_now(true, true) == mk::Paint::Clear, "paint_now, SAM + Shift+Ctrl: clear");

    // ONE radius, shared. The operator asked for the size to carry across a
    // tool switch, so the defect to guard is a SECOND copy surviving: a
    // switch that forgets to carry it, or a writer that moves only one.
    s.set_erasing(false);
    s.set_radius(50.0f);
    check(s.radius() == 50.0f, "the brush's radius is set to 50");
    s.set_erasing(true);
    check(s.radius() == 50.0f, "switching to the eraser carries the brush's 50 px across");
    s.set_radius(300.0f);
    check(s.radius() == 300.0f, "the eraser's radius is set to 300");
    s.set_erasing(false);
    check(s.radius() == 300.0f,
          "and switching back carries the eraser's 300 px: the carry is BOTH ways");
    // The clamp has to carry too. A second copy that syncs only the value the
    // caller passed would read 99999 here, or fall back to 300.
    s.set_erasing(true);
    s.set_radius(99999.0f);
    s.set_erasing(false);
    check(s.radius() == 4096.0f, "a value clamped under the eraser reaches the brush clamped");
    // The invariant, stated as one: set_erasing must never change the radius,
    // whichever way it goes and however often. A divergence of any kind
    // breaks this even where the four checks above happen to agree.
    bool invariant = true;
    for (int i = 0; i < 6; i++) {
        const float before = s.radius();
        s.set_erasing(i % 2 == 0);
        invariant = invariant && s.radius() == before;
    }
    check(invariant, "set_erasing never moves the radius, in either direction");
    s.close();
}

// Two mismatched layer files, each its own size: the joined warning from
// read_layers must not collapse into one file's dimensions for both names.
void test_session_size_mismatch() {
    Fixture f = make_dataset("session_mismatch", 64, 48, {"a"});
    write_png_gray(f.layer / "a.drop.png", 32, 24, std::vector<uint8_t>(32 * 24, 255));
    write_png_gray(f.layer / "a.keep.png", 16, 12, std::vector<uint8_t>(16 * 12, 255));
    const std::vector<uint8_t> drop_before = file_bytes(f.layer / "a.drop.png");
    const std::vector<uint8_t> keep_before = file_bytes(f.layer / "a.keep.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    const std::string e = s.error();
    check(e.find((f.layer / "a.drop.png").string()) != std::string::npos &&
              e.find((f.layer / "a.keep.png").string()) != std::string::npos,
          "both mismatched files are named");
    check(e.find("32x24") != std::string::npos && e.find("16x12") != std::string::npos,
          "each file reports its own size, not the first file's for both");
    // A layer that would not load reads as all zero. If the document opened on
    // those zeros, the next save would encode them over the real correction.
    check(s.doc() == nullptr, "no document, so nothing can be painted or saved over it");
    s.save();
    settle(s);
    check(file_bytes(f.layer / "a.drop.png") == drop_before &&
              file_bytes(f.layer / "a.keep.png") == keep_before,
          "the mismatched layer files are byte-identical after a save attempt");
    s.close();
}

// The mask folder holds 255 = REMOVE: what the editor shows, scores and
// writes back must be the other way round from the file.
void test_session_flipped_polarity() {
    Fixture f = make_dataset("session_flipped", 64, 48, {"a"}, /*with_masks=*/false);
    // Asymmetric on purpose: 25% remove / 75% keep, so the complement is a
    // different number and a missing flip cannot pass.
    std::vector<uint8_t> file_px = box_layer(64, 48, 0, 0, 32, 24);
    const size_t white = (size_t)32 * 24, n = (size_t)64 * 48;
    write_png_gray(f.masks / "a.png", 64, 48, file_px);
    const std::vector<uint8_t> original = file_bytes(f.masks / "a.png");

    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), true, err),
          "flipped session opens: " + err);
    settle(s);
    check(s.doc() != nullptr, "flipped frame loaded");
    const float kept = s.doc()->kept_fraction();
    check(std::fabs(kept - (float)(n - white) / (float)n) < 1e-6f,
          "kept is what the TRAINER keeps (0.75), not the file's white fraction");
    check(s.doc()->base()[0] == 0 && s.doc()->base()[n - 1] == 255,
          "the base in memory is the app's polarity, inverted from the file");

    // A forced KEEP over a corner the file marks remove must come back as 0
    // in the file, because 0 is keep there.
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{2.0f, 2.0f, 10.0f, 10.0f};
    s.commit_stroke(box, mk::Paint::ForceKeep, m);
    s.save();
    settle(s);
    check(s.error().empty(), "flipped save reported no error: " + s.error());
    int w = 0, h = 0;
    std::vector<uint8_t> written;
    check(app::load_stencil((f.masks / "a.png").string(), w, h, written) && w == 64 && h == 48,
          "the written mask decodes");
    check(written[(size_t)5 * 64 + 5] == 0, "force-keep wrote 0 (keep) into a flipped file");
    check(written[(size_t)20 * 64 + 20] == 255, "an untouched remove pixel is still 255");
    check(file_bytes(f.layer / "a.base.png") == original,
          ".base.png is the file's bytes, unconverted");

    // Reopening reads the composite we just wrote and lands on the same number.
    s.go_to(0);
    s.close();
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), true, err),
          "flipped session reopens: " + err);
    settle(s);
    check(s.doc() && s.doc()->base_state() == mk::BaseState::Unchanged,
          "our own flipped composite reads back as Unchanged, not Regenerated");
    check(s.doc()->kept_fraction() > kept, "the forced keep raised the kept fraction");
    s.close();

    // The same folder opened as if it were the app's convention is refused,
    // rather than reinterpreting every recorded correction.
    check(!s.open(f.root.string(), f.images.string(), f.masks.string(), false, err) &&
              err == spirula::i18n::msg::maskedit::err_other_mask_polarity.get(),
          "the other convention over recorded corrections is refused");
}

// One layer folder, two mask folders: the second must not reinterpret the
// first's bases.
void test_session_other_mask_root_refused() {
    Fixture f = make_dataset("session_two_roots", 64, 48, {"a"});
    const fs::path other = f.root / "masks_b";
    write_png_gray(other / "a.png", 64, 48, synth_mask(64, 48, 99));
    const std::vector<uint8_t> original_a = file_bytes(f.masks / "a.png");

    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "open on the first root: " + err);
    settle(s);
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    check(s.error().empty() && file_bytes(f.layer / "a.base.png") == original_a,
          "the first root's original is the base");

    s.close();
    check(!s.open(f.root.string(), f.images.string(), other.string(), false, err),
          "a second mask root over recorded corrections is refused");
    check(err.find(mk::normalize_dir(f.masks.string())) != std::string::npos,
          "the refusal names the root the corrections were made against");
    check(file_bytes(f.layer / "a.base.png") == original_a,
          "the first root's base survived the attempt");
}

// A re-encode of our own composite -- oxipng, another libpng, a metadata
// strip -- changes the bytes and not the picture, and must not rebase.
void test_session_reencoded_mask_is_not_regenerated() {
    Fixture f = make_dataset("session_reencode", 64, 48, {"a"});
    const std::vector<uint8_t> original = file_bytes(f.masks / "a.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "open: " + err);
    settle(s);
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    s.close();

    int w = 0, h = 0;
    std::vector<uint8_t> px;
    check(app::load_stencil((f.masks / "a.png").string(), w, h, px), "composite decodes");
    const std::vector<uint8_t> before = file_bytes(f.masks / "a.png");
    const int level = stbi_write_png_compression_level;
    stbi_write_png_compression_level = level == 1 ? 9 : 1;
    check(write_png_gray(f.masks / "a.png", w, h, px), "re-encoded at another level");
    stbi_write_png_compression_level = level;
    const std::vector<uint8_t> after = file_bytes(f.masks / "a.png");
    std::vector<uint8_t> px2;
    int w2 = 0, h2 = 0;
    app::load_stencil((f.masks / "a.png").string(), w2, h2, px2);
    check(after != before && px2 == px,
          "fixture: the rewrite changed the bytes and not one pixel");

    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "reopen: " + err);
    settle(s);
    check(s.doc() && s.doc()->base_state() == mk::BaseState::Unchanged,
          "a byte-different, pixel-identical mask is NOT a regeneration");
    check(file_bytes(f.layer / "a.base.png") == original,
          ".base.png is still the run's original, not the correction");
    s.close();

    // And revert still restores the original rather than reporting success
    // over a base that had become the correction.
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "reopen 2");
    settle(s);
    s.revert_open_frame();
    settle(s);
    check(file_bytes(f.masks / "a.png") == original, "revert restored the run's own mask");
    s.close();
}

// A corrupt index is every recorded correction; replacing it with an empty
// one loses them all while the layer files are still on disk.
void test_session_corrupt_index_refuses() {
    Fixture f = make_dataset("session_corrupt", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    s.close();
    const std::vector<uint8_t> drop_before = file_bytes(f.layer / "a.drop.png");

    const std::vector<uint8_t> junk = {'{', 'x'};
    mk::write_file_atomic((f.layer / mk::kIndexFileName).string(), junk.data(), junk.size());
    check(!s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "a corrupt index refuses the session");
    check(!s.is_open() && err.find(mk::kIndexFileName) != std::string::npos,
          "not open, and the refusal names index.json");
    check(file_bytes(f.layer / "a.drop.png") == drop_before,
          "the corrections are untouched, so a repaired index still finds them");
}

// A save that fails on the way out has no status strip left to reach.
void test_session_close_reports_a_failed_save() {
    Fixture f = make_dataset("session_close_fail", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err, logged;
    s.set_log([&logged](const std::string& t) { logged = t; });
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    // A regular file where the layer folder goes: every layer write fails.
    // The scan's kept.json made the folder, so it goes first.
    wait_scanned(s);
    for (int i = 0; i < 2000 && !fs::exists(f.layer / mk::kKeptFileName); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::error_code ec;
    fs::remove_all(f.layer, ec);
    fs::create_directories(f.layer.parent_path(), ec);
    const std::vector<uint8_t> one = {'x'};
    check(mk::write_file_atomic(f.layer.string(), one.data(), one.size()) &&
              fs::is_regular_file(f.layer, ec),
          "fixture: the layer root is a file, so no layer can be written");

    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    check(s.doc()->dirty(), "dirty going into close");
    s.close();
    check(!logged.empty(), "close reported the failed save through the log");
    check(logged.find(f.layer.string()) != std::string::npos,
          "and it names the file that could not be written: " + logged);
}

void test_session_close_resets_paths() {
    Fixture f = make_dataset("session_close_reset", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    s.close();
    check(s.layer_root().empty() && s.mask_root().empty(), "closed session reports no paths");
    check(s.frame_count() == 0 && s.frame_index() == -1, "closed session reports no frames");
    check(!s.is_open(), "closed session reports not open");
}

// Orientation 6 (one clockwise turn) on a 64x48 frame whose mask is 32x24:
// shown 24x32. Displayed pixel (dx, dy) is stored (dy, 23 - dx), then 2x to
// the frame, so the turn and the frame scale are each told apart.
void test_session_exif_turn() {
    Fixture f = make_dataset("session_exif_turn", 64, 48, {"a"}, /*with_masks=*/false);
    const fs::path jpg = f.images / "a.jpg";
    check(write_jpg_rgb_oriented(jpg, 64, 48, synth_rgb(64, 48, 0), 6),
          "exif turn: fixture JPEG written");
    check(app::photo_turn(jpg.string()).turns_cw == 1,
          "exif turn: fixture, photo_turn reads Orientation 6 back as one clockwise turn");
    write_png_gray(f.masks / "a.png", 32, 24, std::vector<uint8_t>((size_t)32 * 24, 255));
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "exif turn: open: " + err);
    settle(s);
    check(s.doc() && s.doc()->width() == 32 && s.doc()->height() == 24,
          "exif turn: the document is the mask, stored 32x24");
    check(s.turn().turns_cw == 1 && !s.turn().mirror, "exif turn: the session holds the frame's turn");
    check(s.shown_width() == 24 && s.shown_height() == 32, "exif turn: shown 24x32, the turned mask");
    if (!s.doc()) return;

    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{2.0f, 26.0f, 6.0f, 30.0f};
    const mk::Rect shown = s.commit_stroke(box, mk::Paint::ForceDrop, m);
    const std::vector<uint8_t>& drop = s.doc()->drop();
    int painted = 0, stray = 0;
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 32; x++)
            if (drop[(size_t)y * 32 + x]) {
                painted++;
                stray += x < 25 || x > 30 || y < 17 || y > 22;
            }
    check(drop[(size_t)19 * 32 + 28] == 255 && painted > 0 && stray == 0,
          "exif turn: a stroke at displayed (2..6, 26..30) paints stored (26..30, 18..21) only");
    check(shown.x0 <= 2 && shown.x1 >= 6 && shown.x1 <= 8 && shown.y0 <= 26 && shown.y1 >= 30,
          "exif turn: commit_stroke reports the change in DISPLAYED pixels");

    const mk::PathSpace sp = s.path_space(m);
    float fx = 0.0f, fy = 0.0f, bx = 0.0f, by = 0.0f;
    sp.to_frame(4.5f, 28.5f, fx, fy);
    check(std::fabs(fx - 57.0f) < 1e-3f && std::fabs(fy - 39.0f) < 1e-3f,
          "exif turn: path_space maps displayed (4.5, 28.5) to frame (57, 39)");
    sp.from_frame(57.0f, 39.0f, bx, by);
    check(std::fabs(bx - 4.5f) < 1e-3f && std::fabs(by - 28.5f) < 1e-3f,
          "exif turn: path_space's from_frame inverts to_frame");
}

// ---------------------------------------------------------------------------
// The GUI lasso and the CLI path are one fill
// ---------------------------------------------------------------------------

void test_path_fill_parity() {
    // Non-square, and a polygon that is not symmetric under a transpose, so a
    // fill that swaps its axes cannot pass.
    const int W = 96, H = 40;
    const std::vector<float> norm = {0.08f, 0.12f, 0.91f, 0.07f, 0.66f, 0.55f,
                                     0.97f, 0.93f, 0.21f, 0.88f, 0.44f, 0.41f};
    gui::ShapeStroke lasso;
    lasso.kind = gui::ShapeKind::Lasso;
    for (size_t i = 0; i + 1 < norm.size(); i += 2) {
        lasso.pts.push_back(norm[i] * (float)W);
        lasso.pts.push_back(norm[i + 1] * (float)H);
    }
    gui::Stencil st;
    gui::rasterize_shape(lasso, W, H, st);

    app::FrameMask m;
    app::MaskShape p;
    p.kind = app::MaskShape::Kind::Path;
    p.remove = true;
    p.pts = norm;
    m.shapes.push_back(p);
    std::vector<uint8_t> out;
    std::string err;
    check(app::rasterize_frame_mask(m, W, H, out, err), "parity: rasterizes");
    size_t differ = 0, inside = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) {
        inside += st.in[i] != 0;
        differ += (st.in[i] != 0) != (out[i] == 0);
    }
    check(inside > 300, "parity: the lasso covers something: " + std::to_string(inside));
    check(differ == 0, "parity: every pixel agrees between lasso and -path: " +
                           std::to_string(differ) + " differ");
}

// ---------------------------------------------------------------------------
// The livewire's cost image
// ---------------------------------------------------------------------------

// Grey RGB frames whose every gradient is known by hand.
std::vector<uint8_t> gray_rgb(int w, int h, const std::function<uint8_t(int, int)>& f) {
    std::vector<uint8_t> px((size_t)w * h * 3);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const uint8_t v = f(x, y);
            uint8_t* p = &px[((size_t)y * w + x) * 3];
            p[0] = p[1] = p[2] = v;
        }
    return px;
}

// 40 left of column 100, 200 from it on: a vertical step edge between
// columns 99 and 100. Sobel puts Gx = 640 on both, 0 elsewhere.
std::vector<uint8_t> step_edge_rgb(int w, int h) {
    return gray_rgb(w, h, [](int x, int) { return (uint8_t)(x < 100 ? 40 : 200); });
}

// 40 up to column 98, 120 at 99, 200 from 100: a two-step ramp whose
// Laplacian is +80, 0, -80 on columns 98, 99, 100.
std::vector<uint8_t> ramp_rgb(int w, int h) {
    return gray_rgb(w, h, [](int x, int) {
        return (uint8_t)(x <= 98 ? 40 : x == 99 ? 120 : 200);
    });
}

std::vector<uint8_t> flat_rgb(int w, int h, uint8_t v) {
    return gray_rgb(w, h, [v](int, int) { return v; });
}

// Dark for x < 100, or x >= 100 and y < 60; light otherwise: a right-angle
// edge (vertical at 99/100 for y >= 60, horizontal at 59/60 for x >= 100).
// The straight line between a point on each arm misses the edge.
std::vector<uint8_t> l_corner_rgb(int w, int h) {
    return gray_rgb(w, h, [](int x, int y) {
        return (uint8_t)((x < 100 || y < 60) ? 40 : 200);
    });
}

// Vertical edge flipping polarity at row 60: (100,59)/(100,60) are then
// 8-adjacent with differing codes and opposite sign vs the vertical link
// -- a corner's diagonal transition always bisects, so hand-built here.
std::vector<uint8_t> bowtie_rgb(int w, int h) {
    return gray_rgb(w, h, [](int x, int y) {
        const bool right = x >= 100;
        return (uint8_t)((y < 60) == right ? 40 : 200);
    });
}

size_t count_zero_crossings(const mk::Livewire& lw) {
    size_t n = 0;
    for (int y = 0; y < lw.height(); y++)
        for (int x = 0; x < lw.width(); x++) n += lw.zero_crossing(x, y);
    return n;
}

void test_livewire_features() {
    const int W = 200, H = 120;
    mk::Livewire lw;
    lw.build(step_edge_rgb(W, H).data(), W, H);
    check(lw.ready() && lw.step() == 1 && lw.width() == W && lw.height() == H,
          "step edge: grid is the frame at step 1");
    check(lw.builds() == 1, "one build counted");
    // Exact lower bound: three per-pixel planes plus the three 256x8 tables
    // -- capacity() >= size() always, so this can't pass on an under-count.
    const size_t min_bytes = (size_t)W * H * 3 + sizeof(float) * 256 * 8 * 2 + 256 * 8;
    check(lw.bytes() >= min_bytes,
          "bytes accounts for the feature planes and tables: " + std::to_string(lw.bytes()));

    // fG: 0 on the two ridge columns (G = Gmax), 255 off them.
    check(lw.magnitude_cost(99, 60) == 0 && lw.magnitude_cost(100, 60) == 0,
          "ridge columns have zero magnitude cost");
    check(lw.magnitude_cost(50, 60) == 255 && lw.magnitude_cost(101, 60) == 255 &&
              lw.magnitude_cost(98, 60) == 255,
          "flat columns have full magnitude cost");

    // fZ: exactly the two ridge columns over the interior rows.
    check(count_zero_crossings(lw) == 2 * (size_t)(H - 2),
          "zero crossings are the two ridge columns, interior rows: " +
              std::to_string(count_zero_crossings(lw)));
    check(lw.zero_crossing(99, 60) && lw.zero_crossing(100, 60) &&
              !lw.zero_crossing(98, 60) && !lw.zero_crossing(101, 60),
          "zero crossing on 99 and 100 only");

    // D' is perpendicular to the gradient: vertical on a vertical edge.
    float dx, dy;
    lw.direction(99, 60, dx, dy);
    check(std::fabs(dx) < 0.02f && std::fabs(dy) > 0.99f, "D' on the ridge is vertical");
    lw.direction(50, 60, dx, dy);
    check(dx == 0.0f && dy == 0.0f, "D' is zero where there is no gradient");

    // fD: along the edge ~0, across it ~2/3 (0.0026 and 0.6641 after the
    // 255-code angle quantisation; hand-checked). The parallel-direction
    // mutant swaps the two.
    const float along = lw.direction_cost(99, 60, 99, 61);
    const float across = lw.direction_cost(99, 60, 100, 60);
    check(along >= 0.0f && along < 0.02f, "direction cost along the edge ~0: " + std::to_string(along));
    check(std::fabs(across - 2.0f / 3.0f) < 0.02f,
          "direction cost across the edge ~2/3: " + std::to_string(across));
    check(along < across, "along < across");
    check(lw.direction_cost(99, 60, 99, 62) < 0.0f, "non-adjacent pixels have no direction cost");
    check(lw.direction_cost(-1, 60, 0, 60) < 0.0f, "outside the grid has no direction cost");

    // The full link: ~0 along the ridge; 0.43 + 0.43*2/3 + 0.14 = 0.85667 on
    // a flat pixel, times sqrt(2) on a diagonal.
    check(lw.link_cost(99, 60, 99, 61) < 0.02f, "link along the ridge is almost free");
    check(std::fabs(lw.link_cost(50, 60, 50, 61) - 0.85667f) < 0.01f,
          "flat axial link costs 0.85667: " + std::to_string(lw.link_cost(50, 60, 50, 61)));
    check(std::fabs(lw.link_cost(50, 60, 51, 61) - 0.85667f * 1.41421356f) < 0.02f,
          "flat diagonal link is sqrt(2) times that");
    check(lw.link_cost(50, 60, 52, 60) < 0.0f, "non-adjacent link cost is -1");

    // The ramp: its centre column has Laplacian exactly 0 between +80 and
    // -80 and must be the one and only zero crossing.
    mk::Livewire ramp;
    ramp.build(ramp_rgb(W, H).data(), W, H);
    check(count_zero_crossings(ramp) == (size_t)(H - 2),
          "ramp: one zero-crossing column: " + std::to_string(count_zero_crossings(ramp)));
    check(ramp.zero_crossing(99, 60) && !ramp.zero_crossing(98, 60) && !ramp.zero_crossing(100, 60),
          "ramp: the centre column is the crossing");

    // Flat: no gradient anywhere, full magnitude cost, no crossings.
    mk::Livewire flat;
    flat.build(flat_rgb(64, 32, 128).data(), 64, 32);
    check(flat.magnitude_cost(10, 10) == 255 && count_zero_crossings(flat) == 0,
          "flat image: fG = 1 everywhere, no zero crossings");

    // clear() empties the grid but keeps the build count.
    flat.clear();
    check(!flat.ready() && flat.builds() == 1, "clear keeps the build counter");

    // A cancel flag already set: the build stops between passes, leaves the
    // grid empty and does not count. Catches a checkpoint that is ignored.
    std::atomic<bool> cancel{true};
    mk::Livewire stopped;
    stopped.build(flat_rgb(64, 32, 128).data(), 64, 32, mk::kLivewireMaxEdge,
                  mk::LivewireWeights{}, &cancel);
    check(!stopped.ready() && stopped.builds() == 0, "a set cancel flag stops the build before it counts");
    cancel = false;
    stopped.build(flat_rgb(64, 32, 128).data(), 64, 32, mk::kLivewireMaxEdge,
                  mk::LivewireWeights{}, &cancel);
    check(stopped.ready() && stopped.builds() == 1, "a clear cancel flag builds as before");
}

void test_livewire_mapping() {
    // 4097 x 5 at the default cap: step 2, 2049 x 3. The fifth row is a
    // partial block; the y factor must still be the step, never fh / gh.
    mk::Livewire lw;
    lw.build(flat_rgb(4097, 5, 90).data(), 4097, 5);
    check(lw.step() == 2 && lw.width() == 2049 && lw.height() == 3,
          "4097x5 decimates to 2049x3 at step 2");
    float fx, fy;
    lw.to_frame(2048, 0, fx, fy);
    check((int)std::floor(fx) == 4096, "last grid column lands on the last frame column");
    lw.to_frame(0, 1, fx, fy);
    check((int)std::floor(fy) == 3, "grid row 1 is frame row 3 (step), not row 2 (fh/gh)");
    lw.to_frame(0, 2, fx, fy);
    check((int)std::floor(fy) == 4, "last grid row clamps to the last frame row");
    lw.to_frame(0, 0, fx, fy);
    check((int)std::floor(fx) == 1 && (int)std::floor(fy) == 1, "grid (0,0) is the block centre (1,1)");
    int gx, gy;
    lw.to_grid(4096.9f, 4.9f, gx, gy);
    check(gx == 2048 && gy == 2, "frame corner maps to the last grid cell");
    lw.to_grid(0.0f, 0.0f, gx, gy);
    check(gx == 0 && gy == 0, "frame origin maps to grid origin");
    lw.to_grid(-3.0f, -3.0f, gx, gy);
    check(gx == 0 && gy == 0, "outside clamps");
    lw.to_grid(3.0f, 2.0f, gx, gy);
    check(gx == 1 && gy == 1, "frame (3,2) is grid (1,1)");

    // An explicit cap.
    mk::Livewire small_lw;
    small_lw.build(step_edge_rgb(200, 120).data(), 200, 120, 50);
    check(small_lw.step() == 4 && small_lw.width() == 50 && small_lw.height() == 30,
          "cap 50 on 200x120 gives step 4, 50x30");
    small_lw.to_frame(25, 15, fx, fy);
    check(std::fabs(fx - 102.0f) < 1e-4f && std::fabs(fy - 62.0f) < 1e-4f,
          "block centre at step 4: (25.5*4, 15.5*4)");
}

// ---------------------------------------------------------------------------
// The livewire search
// ---------------------------------------------------------------------------

void test_livewire_edge_path() {
    const int W = 200, H = 120;
    mk::Livewire lw;
    lw.build(step_edge_rgb(W, H).data(), W, H);
    lw.set_anchor(100, 10);
    check(lw.has_anchor(), "anchor set");
    std::vector<int> p;
    check(lw.path_to(100, 110, p), "path found along the edge");
    check(p.size() >= 2 * 101, "path has at least 101 points: " + std::to_string(p.size() / 2));
    check(p.size() >= 4 && p[0] == 100 && p[1] == 10, "path starts at the anchor");
    check(p.size() >= 4 && p[p.size() - 2] == 100 && p[p.size() - 1] == 110,
          "path ends at the target");
    // Every point within one working pixel of the edge at
    // x = 99.5, and consecutive points 8-adjacent.
    float worst = 0.0f;
    bool adjacent = true;
    for (size_t i = 0; i + 1 < p.size(); i += 2) {
        worst = std::max(worst, std::fabs((float)p[i] - 99.5f));
        if (i >= 2)
            adjacent &= std::abs(p[i] - p[i - 2]) <= 1 && std::abs(p[i + 1] - p[i - 1]) <= 1 &&
                        (p[i] != p[i - 2] || p[i + 1] != p[i - 1]);
    }
    check(worst <= 1.0f, "every path pixel within 1 px of the edge (worst " +
                             std::to_string(worst) + ")");
    check(adjacent, "consecutive path points are 8-adjacent and distinct");
    const double cost = lw.path_cost(100, 110);
    check(cost >= 0.0 && cost < 1.0, "the edge path is nearly free: " + std::to_string(cost));
    check(lw.pops() < 2000, "lazy: settled far fewer nodes than the grid holds: " +
                                std::to_string(lw.pops()));
    // A target off the edge is reachable too, and costlier per pixel.
    check(lw.path_to(150, 60, p) && p[p.size() - 2] == 150 && p[p.size() - 1] == 60,
          "off-edge target reached");
    check(lw.path_cost(150, 60) > 20.0, "leaving the edge costs about 0.86 per pixel");
    check(!lw.path_to(-1, 5, p), "outside the grid: no path");
    mk::Livewire empty;
    check(!empty.path_to(0, 0, p) && empty.path_cost(0, 0) < 0.0, "no anchor: no path");
}

void test_livewire_reanchor() {
    mk::Livewire lw;
    lw.build(step_edge_rgb(200, 120).data(), 200, 120);
    std::vector<int> p1, p2;
    lw.set_anchor(20, 20);
    check(lw.path_to(150, 60, p1) && p1[0] == 20 && p1[1] == 20, "first anchor's path");
    lw.set_anchor(180, 100);
    check(lw.pops() == 0, "set_anchor resets the pop count");
    check(lw.path_to(150, 60, p2) && p2[0] == 180 && p2[1] == 100,
          "second anchor's path starts at the second anchor");
    check(lw.path_cost(180, 100) == 0.0, "the anchor itself costs nothing");
}

void test_livewire_diagonal() {
    // Uniform image: every link costs 0.85667 per unit length, so the cost
    // to (20, 10) is (10 sqrt2 + 10) * 0.85667 = 20.6818 (hand-checked), and
    // to (20, 0) is 17.1333. A Chebyshev metric gives 17.13 for both.
    mk::Livewire lw;
    lw.build(flat_rgb(64, 32, 128).data(), 64, 32);
    lw.set_anchor(0, 0);
    const double d = lw.path_cost(20, 10);
    check(std::fabs(d - 20.6818) < 0.02, "diagonal links weighted by sqrt2: " + std::to_string(d));
    const double a = lw.path_cost(20, 0);
    check(std::fabs(a - 17.1333) < 0.02, "axial run: " + std::to_string(a));
}

void test_livewire_once() {
    mk::Livewire lw;
    lw.build(step_edge_rgb(200, 120).data(), 200, 120);
    lw.set_anchor(100, 10);
    std::vector<int> p;
    for (int i = 0; i < 100; i++) lw.path_to(100 + (i % 7) - 3, 20 + i, p);
    check(lw.builds() == 1, "100 cursor moves, one build");
}

// ---------------------------------------------------------------------------
// The pen tool
// ---------------------------------------------------------------------------

gui::ViewportInput at(float x, float y) {
    gui::ViewportInput in;
    in.hovered = true;
    in.x = x;
    in.y = y;
    in.W = 200;
    in.H = 120;
    return in;
}

gui::ViewportInput click_at(float x, float y) {
    gui::ViewportInput in = at(x, y);
    in.clicked = in.down = true;
    return in;
}

gui::ViewportInput right_click_at(float x, float y) {
    gui::ViewportInput in = at(x, y);
    in.right_clicked = true;
    return in;
}

bool same_points(const std::vector<float>& a, const std::vector<float>& b, float tol) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (std::fabs(a[i] - b[i]) > tol) return false;
    return true;
}

void test_path_tool_basic() {
    mk::PathTool t;
    std::vector<float> out;
    bool consumed = false;
    check(!t.in_progress() && !t.snapping(), "idle, no livewire");
    check(!t.update(at(10, 10), out, consumed) && !consumed, "a move does nothing while idle");
    gui::ViewportInput off = click_at(10, 10);
    off.hovered = false;
    check(!t.update(off, out, consumed) && !consumed && !t.in_progress(),
          "a click off the canvas does nothing");

    check(!t.update(click_at(10, 10), out, consumed) && consumed && t.in_progress(),
          "first click drops an anchor and is consumed");
    check(t.anchor_count() == 1, "one anchor");
    std::vector<float> an, co, li;
    t.update(at(30, 30), out, consumed);
    t.overlay(an, co, li);
    check(same_points(an, {10, 10}, 1e-6f) && same_points(co, {10, 10}, 1e-6f) &&
              same_points(li, {10, 10, 30, 30}, 1e-6f),
          "live segment is a straight line to the cursor without a livewire");
    check(!consumed, "a plain move is not consumed");
    gui::ViewportInput drag = at(30, 30);
    drag.down = true;
    t.update(drag, out, consumed);
    check(consumed, "the held button stays the tool's");

    t.update(click_at(50, 10), out, consumed);
    check(t.anchor_count() == 2 && !t.commit_pending(out), "two anchors cannot close");
    check(t.in_progress(), "a refused commit keeps the path");
    t.update(click_at(50, 40), out, consumed);
    t.overlay(an, co, li);
    check(same_points(co, {10, 10, 50, 10, 50, 40}, 1e-6f), "committed polyline is the anchors");

    // Close on the first anchor: within 10 px of it, with 3 anchors.
    t.update(at(40, 27), out, consumed);
    check(!t.near_first(), "34.5 px away is not near the first anchor");
    t.update(at(12, 11), out, consumed);
    check(t.near_first(), "2.2 px away is near");
    // Straddle kPathCloseRadius = 10.0f exactly, so the constant is pinned.
    t.update(at(19.9f, 10.0f), out, consumed);
    check(t.near_first(), "9.9 px away is inside the close radius");
    t.update(at(20.1f, 10.0f), out, consumed);
    check(!t.near_first(), "10.1 px away is outside the close radius");
    check(t.update(click_at(12, 11), out, consumed) && consumed, "clicking the first anchor closes");
    check(same_points(out, {10, 10, 50, 10, 50, 40}, 1e-6f), "closed polygon is the three anchors");
    check(!t.in_progress() && t.anchor_count() == 0, "closing resets the tool");

    // Enter closes.
    t.update(click_at(10, 10), out, consumed);
    t.update(click_at(50, 10), out, consumed);
    t.update(click_at(50, 40), out, consumed);
    check(t.commit_pending(out) && out.size() == 6, "Enter closes three anchors");

    // A right click closes; below 3 anchors it is refused and keeps the path.
    t.update(click_at(10, 10), out, consumed);
    t.update(click_at(50, 10), out, consumed);
    check(!t.update(right_click_at(50, 40), out, consumed) && consumed && t.in_progress(),
          "right click with two anchors: refused, consumed, path kept");
    t.update(click_at(50, 40), out, consumed);
    check(t.update(right_click_at(70, 70), out, consumed) && out.size() == 6,
          "right click with three anchors closes");

    // Pop.
    t.update(click_at(10, 10), out, consumed);
    t.update(click_at(50, 10), out, consumed);
    t.update(click_at(50, 40), out, consumed);
    check(t.pop_anchor() && t.anchor_count() == 2, "pop takes one anchor");
    t.overlay(an, co, li);
    check(same_points(co, {10, 10, 50, 10}, 1e-6f), "pop trims the committed polyline");
    check(t.pop_anchor() && t.pop_anchor() && !t.in_progress(), "pop to empty");
    check(!t.pop_anchor(), "pop on empty is false");

    // Cancel.
    t.update(click_at(10, 10), out, consumed);
    t.cancel();
    check(!t.in_progress(), "cancel empties the path");
}

// Regression test: a commit site once read modifiers at close time instead
// of at the first anchor. The captured mode must survive modifiers changing
// mid-path and at the close click itself.
void test_path_tool_mode_latch() {
    mk::PathTool t;
    std::vector<float> out;
    bool consumed = false;

    t.note_modifiers(false, true);
    check(!t.mode_shift() && t.mode_ctrl(), "idle mirrors ctrl held");
    t.note_modifiers(true, false);
    check(t.mode_shift() && !t.mode_ctrl(), "idle mirrors shift held instead");
    t.note_modifiers(false, false);
    check(!t.mode_shift() && !t.mode_ctrl(), "idle mirrors neither held");

    // Ctrl held on the click that plants the first anchor: captured.
    t.note_modifiers(false, true);
    t.update(click_at(10, 10), out, consumed);
    check(t.in_progress(), "first anchor placed");
    check(!t.mode_shift() && t.mode_ctrl(), "captured ctrl at the first anchor");

    // The exact mutant: modifiers change mid-path and again at the close
    // click. The captured mode must not move.
    t.note_modifiers(true, false);
    check(!t.mode_shift() && t.mode_ctrl(),
          "frozen while in progress despite shift now held instead");
    t.update(click_at(50, 10), out, consumed);
    t.note_modifiers(false, false);
    check(!t.mode_shift() && t.mode_ctrl(), "still frozen with no modifiers held");
    t.update(click_at(50, 40), out, consumed);
    t.note_modifiers(true, true);
    check(!t.mode_shift() && t.mode_ctrl(),
          "still frozen with both modifiers held, right before the close click");
    check(t.update(click_at(12, 11), out, consumed) && out.size() >= 6,
          "closes on the first anchor");
    check(!t.mode_shift() && t.mode_ctrl(),
          "closed mode is still the first anchor's, not the close click's");

    // Idle again after closing: mirrors live modifiers, not stuck frozen.
    t.note_modifiers(true, true);
    check(t.mode_shift() && t.mode_ctrl(), "idle again after close, mirrors live state");

    // Cancel mid-path: the frozen mode is abandoned, idle mirroring resumes.
    t.note_modifiers(false, false);
    t.update(click_at(10, 10), out, consumed);
    t.note_modifiers(true, true);
    check(!t.mode_shift() && !t.mode_ctrl(), "frozen through cancel's setup");
    t.cancel();
    t.note_modifiers(true, false);
    check(t.mode_shift() && !t.mode_ctrl(), "idle mirroring resumes after cancel");
}

void test_path_tool_livewire() {
    const int W = 200, H = 120;
    mk::Livewire lw;
    lw.build(step_edge_rgb(W, H).data(), W, H);
    mk::PathTool t;
    t.set_livewire(&lw);
    check(t.snapping(), "snapping with a ready livewire");
    std::vector<float> out, an, co, li;
    bool consumed;
    t.update(click_at(100, 10), out, consumed);
    check(lw.has_anchor(), "the first anchor seeds the search");
    // A near hop and a far hop must both grow, so no constant satisfies both.
    t.update(at(100, 11), out, consumed);
    const double ms_tiny = t.last_segment_ms();
    const size_t pops_tiny = lw.pops();
    t.update(at(100, 60), out, consumed);
    t.overlay(an, co, li);
    check(li.size() >= 2 * 51, "live segment follows the edge: " + std::to_string(li.size() / 2));
    bool on_edge = true;
    for (size_t i = 2; i + 3 < li.size(); i += 2) on_edge &= std::floor(li[i]) == 99.0f || std::floor(li[i]) == 100.0f;
    check(on_edge, "live segment's interior points sit on the ridge");
    check(li[0] == 100.0f && li[1] == 10.0f && li[li.size() - 2] == 100.0f && li[li.size() - 1] == 60.0f,
          "live segment's ends are the exact anchor and cursor");
    const double ms = t.last_segment_ms();
    check(ms >= 0.0 && ms < 1000.0, "segment time stays in a sane range: " + std::to_string(ms) + " ms");
    check(lw.pops() > pops_tiny, "the far hover visits more nodes: " + std::to_string(lw.pops()) +
                                      " vs " + std::to_string(pops_tiny));
    // Printed, never asserted: both readings are under 0.01 ms and invert
    // under scheduler noise. pops() above is the deterministic proof.
    std::printf("note  far/near segment time %.6f vs %.6f ms\n", ms, ms_tiny);

    t.update(click_at(100, 110), out, consumed);
    t.overlay(an, co, li);
    const size_t after_two = co.size();
    check(after_two >= 2 * 101, "committed polyline runs down the edge");
    check(co[0] == 100.0f && co[1] == 10.0f && co[after_two - 2] == 100.0f && co[after_two - 1] == 110.0f,
          "committed ends are exact anchors");
    check(li.empty() || li.size() == 4, "live segment cleared after a click");

    t.update(click_at(150, 60), out, consumed);
    t.overlay(an, co, li);
    check(co.size() > after_two, "third anchor appends its segment");
    check(t.pop_anchor(), "pop the third");
    t.overlay(an, co, li);
    check(co.size() == after_two, "pop restores the two-anchor polyline exactly");
    t.update(click_at(150, 60), out, consumed);
    check(t.commit_pending(out) && out.size() >= 6 && out[0] == 100.0f && out[1] == 10.0f,
          "Enter closes with the edge path back to the first anchor");
    check(lw.builds() == 1, "the tool never rebuilt the cost image");

    // Detaching the livewire falls back to straight segments.
    t.set_livewire(nullptr);
    check(!t.snapping(), "no livewire, no snapping");
    t.update(click_at(10, 10), out, consumed);
    t.update(at(30, 30), out, consumed);
    t.overlay(an, co, li);
    check(same_points(li, {10, 10, 30, 30}, 1e-6f), "straight again");
    t.cancel();
}

void test_path_tool_space() {
    // Fed pixels are frame pixels scaled 2x on X, 4x on Y, each offset --
    // different axes catch a transposed scale a uniform factor would miss.
    // Anchors and the closed polygon come back in fed pixels; the livewire sees frame pixels.
    const int W = 200, H = 120;
    mk::Livewire lw;
    lw.build(step_edge_rgb(W, H).data(), W, H);
    mk::PathTool t;
    t.set_livewire(&lw);
    mk::PathSpace sp;
    sp.to_frame = [](float x, float y, float& fx, float& fy) { fx = (x - 5.0f) * 0.5f; fy = (y - 7.0f) * 0.25f; };
    sp.from_frame = [](float fx, float fy, float& x, float& y) { x = fx * 2.0f + 5.0f; y = fy * 4.0f + 7.0f; };
    t.set_space(sp);
    std::vector<float> out, an, co, li;
    bool consumed;
    t.update(click_at(205, 47), out, consumed);       // frame (100, 10)
    t.update(click_at(205, 447), out, consumed);      // frame (100, 110)
    t.update(click_at(305, 247), out, consumed);      // frame (150, 60)
    t.overlay(an, co, li);
    check(same_points(an, {205, 47, 205, 447, 305, 247}, 1e-3f), "anchors reported in fed pixels");
    bool on_edge = true;
    for (size_t i = 2; i + 3 < co.size() && i < 2 * 100; i += 2) {
        const float fx = (co[i] - 5.0f) * 0.5f;
        on_edge &= std::floor(fx) == 99.0f || std::floor(fx) == 100.0f;
    }
    check(on_edge, "the committed edge run maps back to the ridge in frame pixels");
    t.update(at(30, 30), out, consumed);
    check(!t.near_first(), "fed (30,30) is 175.8 px from the first anchor");
    t.update(at(210, 45), out, consumed);
    check(t.near_first(), "fed (210,45) is 5.4 px from it");
    check(t.update(click_at(210, 45), out, consumed) && out.size() >= 6, "closes in fed space");
    check(std::fabs(out[0] - 205.0f) < 1e-3f && std::fabs(out[1] - 47.0f) < 1e-3f,
          "closed polygon starts at the fed first anchor");
}

// ---------------------------------------------------------------------------
// The Bezier pen
// ---------------------------------------------------------------------------

gui::ViewportInput held_at(float x, float y) {
    gui::ViewportInput in = at(x, y);
    in.down = true;
    return in;
}

// Press, drag through (x, y) and let go there.
void pen_drag(mk::PenTool& t, float px, float py, float x, float y,
              gui::ViewportInput mods = gui::ViewportInput{}) {
    std::vector<float> out;
    bool consumed;
    auto with = [&](gui::ViewportInput in) {
        in.shift = mods.shift;
        in.alt = mods.alt;
        in.ctrl = mods.ctrl;
        return in;
    };
    t.update(with(click_at(px, py)), out, consumed);
    t.update(with(held_at(0.5f * (px + x), 0.5f * (py + y))), out, consumed);
    t.update(with(held_at(x, y)), out, consumed);
    t.update(with(at(x, y)), out, consumed);
}

void test_pen_tool_basic() {
    mk::PenTool t;
    std::vector<float> out, an;
    bool consumed = false;
    check(!t.update(at(10, 10), out, consumed) && !consumed && !t.in_progress(),
          "pen: a move does nothing while idle");
    check(!t.update(click_at(10, 10), out, consumed) && consumed && t.anchor_count() == 1,
          "pen: a click drops an anchor and is consumed");
    t.update(at(10, 10), out, consumed);
    t.overlay(an);
    check(same_points(an, {10, 10, 10, 10, 10, 10}, 1e-6f), "pen: a click is a corner");

    // A drag pulls a smooth anchor: out under the pointer, in mirrored.
    pen_drag(t, 100, 10, 130, 20);
    t.overlay(an);
    check(t.anchor_count() == 2 && same_points({an.begin() + 6, an.end()},
                                               {70, 0, 100, 10, 130, 20}, 1e-4f),
          "pen: a drag makes a smooth anchor with mirrored handles");
    check(t.can_close(), "pen: two anchors with a curve between them can close");
    float c[8];
    t.update(at(60, 80), out, consumed);
    check(t.preview(c) && same_points({c, c + 8}, {100, 10, 130, 20, 60, 80, 60, 80}, 1e-4f),
          "pen: the preview runs from the last anchor's out-handle to the pointer");

    // A drag shorter than kPenDragStart is still a click.
    pen_drag(t, 100, 100, 101, 101);
    t.overlay(an);
    check(t.anchor_count() == 3 && same_points({an.begin() + 12, an.end()},
                                               {100, 100, 100, 100, 100, 100}, 1e-6f),
          "pen: a 1.4 px wobble is a click, not a curve");
    t.update(at(12, 11), out, consumed);
    check(t.cue() == mk::PenTool::Cue::Close, "pen: over the first anchor the badge says close");
    check(!t.update(click_at(12, 11), out, consumed) && t.in_progress(),
          "pen: closing waits for the release");
    check(t.update(at(12, 11), out, consumed) && out.size() == 18 && !t.in_progress(),
          "pen: the release on the first anchor closes three anchors");
    check(same_points({out.begin() + 6, out.begin() + 12}, {70, 0, 100, 10, 130, 20}, 1e-4f),
          "pen: the closed path keeps the smooth anchor's handles");

    // Two corners enclose nothing.
    t.update(click_at(10, 10), out, consumed);
    t.update(at(10, 10), out, consumed);
    t.update(click_at(50, 10), out, consumed);
    t.update(at(50, 10), out, consumed);
    check(!t.can_close() && !t.commit_pending(out) && t.in_progress(),
          "pen: two corners cannot close, and a refused commit keeps the path");
    check(!t.update(right_click_at(80, 80), out, consumed) && consumed && t.in_progress(),
          "pen: nor can a right click");
    t.update(click_at(50, 50), out, consumed);
    t.update(at(50, 50), out, consumed);
    check(t.update(right_click_at(80, 80), out, consumed) && out.size() == 18,
          "pen: a right click closes three");
    t.update(click_at(10, 10), out, consumed);
    t.update(at(10, 10), out, consumed);
    t.update(click_at(50, 10), out, consumed);
    t.update(at(50, 10), out, consumed);
    check(t.pop_anchor() && t.anchor_count() == 1 && t.pop_anchor() && !t.in_progress() &&
              !t.pop_anchor(),
          "pen: pop takes anchors back to empty");
}

void test_pen_tool_modifiers() {
    mk::PenTool t;
    std::vector<float> out, an;
    bool consumed = false;
    gui::ViewportInput shift, alt, ctrl;
    shift.shift = true;
    alt.alt = true;
    ctrl.ctrl = true;

    t.update(click_at(0, 0), out, consumed);
    t.update(at(0, 0), out, consumed);
    // Shift on a click: the new anchor turns about the last to 45 degrees.
    gui::ViewportInput sc = click_at(100, 90);
    sc.shift = true;
    t.update(sc, out, consumed);
    t.update(at(100, 90), out, consumed);
    t.overlay(an);
    const float r = std::hypot(100.0f, 90.0f) / std::sqrt(2.0f);
    check(same_points({an.begin() + 8, an.begin() + 10}, {r, r}, 1e-3f),
          "pen: Shift puts the click on the 45-degree line, at the same distance");
    t.cancel();

    // Shift on a drag: the handle snaps.
    t.update(click_at(0, 0), out, consumed);
    t.update(at(0, 0), out, consumed);
    pen_drag(t, 100, 0, 150, 4, shift);
    t.overlay(an);
    check(std::fabs(an[11] - 0.0f) < 1e-3f && an[10] > 140.0f,
          "pen: Shift levels a handle drawn nearly level");
    t.cancel();

    // Alt mid-drag: the in-handle stays where the drag had put it.
    t.update(click_at(0, 0), out, consumed);
    t.update(at(0, 0), out, consumed);
    t.update(click_at(100, 0), out, consumed);
    t.update(held_at(130, 0), out, consumed);
    gui::ViewportInput ah = held_at(100, 40);
    ah.alt = true;
    t.update(ah, out, consumed);
    t.update(at(100, 40), out, consumed);
    t.overlay(an);
    check(same_points({an.begin() + 6, an.end()}, {70, 0, 100, 0, 100, 40}, 1e-4f),
          "pen: Alt breaks the handles -- in stays at (70,0), out follows to (100,40)");

    // A click on the last anchor takes its out-handle back.
    t.update(at(100, 1), out, consumed);
    check(t.cue() == mk::PenTool::Cue::Retract, "pen: over the last anchor the badge says so");
    t.update(click_at(100, 1), out, consumed);
    t.update(at(100, 1), out, consumed);
    t.overlay(an);
    check(same_points({an.begin() + 6, an.end()}, {70, 0, 100, 0, 100, 0}, 1e-4f),
          "pen: clicking the last anchor retracts its out-handle only");

    // Ctrl+drag moves a point already placed, handles and all.
    t.update(click_at(50, 80), out, consumed);
    t.update(at(50, 80), out, consumed);
    gui::ViewportInput hover = at(1, 1);
    hover.ctrl = true;
    t.update(hover, out, consumed);
    check(t.cue() == mk::PenTool::Cue::Edit, "pen: Ctrl over an anchor offers to move it");
    pen_drag(t, 1, 1, 11, -9, ctrl);
    t.overlay(an);
    check(t.anchor_count() == 3 && same_points({an.begin(), an.begin() + 6},
                                               {10, -10, 10, -10, 10, -10}, 1e-4f),
          "pen: Ctrl+drag moved the first anchor and added none");
    // Ctrl+drag a smooth anchor's handle: the other swings opposite, keeping its length.
    t.cancel();
    t.update(click_at(0, 0), out, consumed);
    t.update(at(0, 0), out, consumed);
    pen_drag(t, 100, 0, 130, 0);
    t.update(click_at(50, 80), out, consumed);
    t.update(at(50, 80), out, consumed);
    pen_drag(t, 130, 0, 100, 40, ctrl);
    t.overlay(an);
    check(same_points({an.begin() + 6, an.begin() + 12}, {100, -30, 100, 0, 100, 40}, 1e-3f),
          "pen: moving one handle of a smooth anchor swings the other opposite");
    t.cancel();

    // Space while dragging carries the new anchor; the handles resume from there.
    t.update(click_at(0, 0), out, consumed);
    t.update(at(0, 0), out, consumed);
    t.update(click_at(100, 0), out, consumed);
    t.note_space(true);
    t.update(held_at(110, 5), out, consumed);
    t.update(held_at(120, 10), out, consumed);
    t.note_space(false);
    t.update(held_at(150, 10), out, consumed);
    t.update(at(150, 10), out, consumed);
    t.overlay(an);
    check(same_points({an.begin() + 6, an.end()}, {90, 10, 120, 10, 150, 10}, 1e-4f),
          "pen: Space moved the anchor to (120,10), then the drag pulled handles from it");

    // The paint mode latches on the first anchor, as the livewire path's does.
    t.cancel();
    t.note_modifiers(false, true);
    t.update(click_at(0, 0), out, consumed);
    t.note_modifiers(false, false);
    check(t.mode_ctrl(), "pen: Ctrl held on the first anchor is kept after it is released");
}

void test_pen_tool_space() {
    // Fed = frame scaled 2x and offset: anchors report in fed pixels, and a
    // drag's handles are measured there too.
    mk::PenTool t;
    mk::PathSpace sp;
    sp.to_frame = [](float x, float y, float& fx, float& fy) { fx = (x - 5.0f) * 0.5f; fy = (y - 7.0f) * 0.5f; };
    sp.from_frame = [](float fx, float fy, float& x, float& y) { x = fx * 2.0f + 5.0f; y = fy * 2.0f + 7.0f; };
    t.set_space(sp);
    std::vector<float> out, an;
    bool consumed;
    t.update(click_at(25, 27), out, consumed);
    t.update(at(25, 27), out, consumed);
    pen_drag(t, 125, 27, 145, 27);
    t.update(click_at(125, 127), out, consumed);
    t.update(at(125, 127), out, consumed);
    t.overlay(an);
    check(same_points({an.begin() + 6, an.begin() + 12}, {105, 27, 125, 27, 145, 27}, 1e-3f),
          "pen: anchors and handles report in fed pixels");
    check(t.commit_pending(out) && same_points({out.begin(), out.begin() + 2}, {25, 27}, 1e-3f),
          "pen: the closed path comes back in fed pixels");
}

// ---------------------------------------------------------------------------
// stored -> displayed, continuous, is the inverse of to_stored for all
// eight EXIF orientations
// ---------------------------------------------------------------------------

void test_to_displayed_float() {
    const int W = 40, H = 24;
    const float pts[][2] = {{0.5f, 0.5f}, {39.5f, 23.5f}, {12.25f, 3.75f}, {0.0f, 23.0f}, {40.0f, 0.0f}};
    for (int o = 1; o <= 8; o++) {
        const sfm::ExifTransform t = sfm::exifTransform(o);
        int dw = W, dh = H;
        spirula::oriented_size(t.turns_cw, dw, dh);
        for (const float* p : pts) {
            float dx, dy, sx, sy;
            mk::to_displayed(t, W, H, p[0], p[1], dx, dy);
            check(dx >= -1e-4f && dx <= (float)dw + 1e-4f && dy >= -1e-4f && dy <= (float)dh + 1e-4f,
                  "to_displayed lands inside the displayed size, orientation " + std::to_string(o));
            mk::to_stored(t, W, H, dx, dy, sx, sy);
            check(std::fabs(sx - p[0]) < 1e-4f && std::fabs(sy - p[1]) < 1e-4f,
                  "to_stored(to_displayed(p)) == p, orientation " + std::to_string(o));
        }
    }
    // Orientation 6 by hand, the phone-portrait case: stored (10, H - 5) is
    // displayed (5, 10) in the pixel form, so the continuous form at
    // (10.5, 18.5) is (5.5, 10.5).
    float dx, dy;
    mk::to_displayed(sfm::exifTransform(6), W, H, 10.5f, 18.5f, dx, dy);
    check(std::fabs(dx - 5.5f) < 1e-4f && std::fabs(dy - 10.5f) < 1e-4f, "orientation 6 by hand");
}

// Anchor on the vertical arm, target on the horizontal: the straight line
// between them is off both edges, so the path has to follow the edge: the
// true minimum curves around the corner.
void test_livewire_corner_path() {
    const int W = 200, H = 120;
    mk::Livewire lw;
    lw.build(l_corner_rgb(W, H).data(), W, H);
    lw.set_anchor(100, 100);
    std::vector<int> p;
    check(lw.path_to(150, 60, p), "corner path found");
    check(p.size() >= 4 && p[0] == 100 && p[1] == 100, "corner path starts at the anchor");
    check(p.size() >= 4 && p[p.size() - 2] == 150 && p[p.size() - 1] == 60,
          "corner path ends at the target");
    // Distance to the nearer arm; also flags a link crossing two
    // non-degenerate, different direction codes -- the pair link_cost_k
    // takes through the search that no prior fixture reached.
    float worst = 0.0f;
    bool asymmetric_pair = false;
    for (size_t i = 0; i + 1 < p.size(); i += 2) {
        const float px = (float)p[i], py = (float)p[i + 1];
        worst = std::max(worst, std::min(std::fabs(px - 99.5f), std::fabs(py - 59.5f)));
        if (i + 3 < p.size()) {
            float dpx, dpy, dqx, dqy;
            lw.direction(p[i], p[i + 1], dpx, dpy);
            lw.direction(p[i + 2], p[i + 3], dqx, dqy);
            const bool both_gradient = (dpx != 0.0f || dpy != 0.0f) && (dqx != 0.0f || dqy != 0.0f);
            if (both_gradient && dpx * dqx + dpy * dqy < 0.5f) asymmetric_pair = true;
        }
    }
    check(worst <= 1.0f, "corner path within 1 px of the nearer arm (worst " +
                             std::to_string(worst) + ")");
    check(asymmetric_pair,
          "the search crosses a genuinely different, non-degenerate direction code pair");
    const double cost = lw.path_cost(150, 60);
    check(cost >= 0.0 && cost < 5.0,
          "the corner path is cheap, not the straight diagonal: " + std::to_string(cost));
}

// Pins link_cost_k's sign source directly: 0.327843 hand-derived (fd 2/3,
// fz 0 on the zero crossing, fg 75/255, all axial) against the bowtie
// pair. The sign[cq] mutant gives 0.186196 -- fails at this tolerance.
void test_livewire_sign_alignment() {
    mk::Livewire lw;
    lw.build(bowtie_rgb(200, 120).data(), 200, 120);
    float dpx, dpy, dqx, dqy;
    lw.direction(100, 59, dpx, dpy);
    lw.direction(100, 60, dqx, dqy);
    check((dpx != 0.0f || dpy != 0.0f) && (dqx != 0.0f || dqy != 0.0f),
          "both sides of the bowtie have a gradient");
    check(dpx * dqx + dpy * dqy < 0.5f, "the bowtie pair's codes genuinely differ");
    const float lc = lw.link_cost(100, 59, 100, 60);
    check(std::fabs(lc - 0.327843f) < 0.001f,
          "link_cost_k sign-aligns to D'(p), not D'(q): " + std::to_string(lc));
}

// ---------------------------------------------------------------------------
// Floors at 8K. SS_MASK_BENCH=<dir> writes the fixture there and
// prints medians of three repeats; nothing here fails on a number.
// ---------------------------------------------------------------------------

template <class F>
double median_ms(F&& f, int repeats = 3) {
    std::vector<double> t;
    for (int i = 0; i < repeats; i++) {
        const auto a = std::chrono::steady_clock::now();
        f();
        const auto b = std::chrono::steady_clock::now();
        t.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    std::sort(t.begin(), t.end());
    return t[t.size() / 2];
}

// More 1080p frames than the ring's biggest window (kReelSlots - 1 = 11), so
// the 1080p arm measures decoding rather than a cache of the whole dataset.
constexpr int kHdFrames = 16;

void bench_8k(const char* dir) {
    const int W = 7680, H = 3840;
    const fs::path root(dir), images = root / "images", masks = root / "masks";
    std::error_code ec;
    fs::create_directories(images, ec);
    fs::create_directories(masks, ec);
    for (int i = 0; i < 3; i++) {
        const std::string key = "f000" + std::to_string(i);
        if (!fs::exists(images / (key + ".jpg")))
            write_jpg_rgb(images / (key + ".jpg"), W, H, synth_rgb(W, H, (uint32_t)i));
        if (!fs::exists(masks / (key + ".png")))
            write_png_gray(masks / (key + ".png"), W, H, synth_mask(W, H, (uint32_t)i));
    }
    std::printf("bench: fixture at %s\n", dir);
    int fw, fh;
    std::vector<uint8_t> rgb;
    std::printf("bench load_rgb 8K JPEG           %8.1f ms\n",
                median_ms([&] { app::load_rgb((images / "f0000.jpg").string(), fw, fh, rgb); }));
    int mw, mh;
    std::vector<uint8_t> base;
    std::printf("bench load_stencil 8K PNG        %8.1f ms\n",
                median_ms([&] { app::load_stencil((masks / "f0000.png").string(), mw, mh, base); }));
    std::vector<uint8_t> png;
    std::printf("bench encode_gray_png 8K mask    %8.1f ms\n",
                median_ms([&] { mk::encode_gray_png(base.data(), W, H, png); }));
    std::printf("bench rle_encode 8K plane        %8.1f ms\n",
                median_ms([&] { gui::rle_encode(base); }));

    const fs::path layer = root / mk::kLayerDirName;
    fs::remove_all(layer, ec);
    mk::LayerIndex idx;
    idx.mask_root = masks.string();
    std::string err, warn;
    mk::MaskDoc d;
    d.load(layer.string(), masks.string(), "f0000", W, H, idx, err, warn);

    // Brush commit, CPU half: 20 strokes, radius 100 px, 500 px long:
    // rasterize + paint (with the op's RLE) + derive the stroke's part of a
    // 4096-square window at step 1.
    mk::Window win;
    win.r = {1000, 0, 5096, 3840};
    win.step = 1;
    win.tw = 4096;
    win.th = 3840;
    std::vector<uint8_t> rgba;
    mk::WindowSource src;
    src.rgb = rgb.data(); src.fw = fw; src.fh = fh;
    src.W = W; src.H = H;
    std::vector<double> strokes;
    for (int k = 0; k < 20; k++) {
        gui::ShapeStroke s;
        s.kind = gui::ShapeKind::Brush;
        s.brush_radius = 100.0f;
        const float x = 1200.0f + 150.0f * k, y = 400.0f + 120.0f * k;
        s.pts = std::vector<float>{x, y, x + 250.0f, y + 50.0f, x + 500.0f, y};
        const auto a = std::chrono::steady_clock::now();
        const mk::Rect r = mk::stroke_bounds(s, W, H);
        gui::Stencil st;
        gui::rasterize_shape(s, W, H, st);
        d.paint(k % 2 ? mk::Paint::ForceKeep : mk::Paint::ForceDrop, std::move(st), r);
        src.composite = d.composite().data(); src.drop = d.drop().data(); src.keep = d.keep().data();
        mk::derive_window(win, d.last_change(), src, rgba);
        const auto b = std::chrono::steady_clock::now();
        strokes.push_back(std::chrono::duration<double, std::milli>(b - a).count());
    }
    std::sort(strokes.begin(), strokes.end());
    std::printf("bench stroke commit (CPU) 8K     median %8.1f ms   max %8.1f ms   [bar: median <= 100, max <= 250]\n",
                strokes[10], strokes.back());
    std::printf("bench history bytes after 20     %zu\n", d.history_bytes());
    std::printf("bench derive full 4096x3840 win  %8.1f ms\n",
                median_ms([&] { mk::derive_window(win, win.r, src, rgba); }));
    mk::Window whole;
    whole.r = {0, 0, W, H};
    whole.step = 2;
    whole.tw = 3840;
    whole.th = 1920;
    std::printf("bench derive whole mask step 2   %8.1f ms\n",
                median_ms([&] { mk::derive_window(whole, whole.r, src, rgba); }));
    std::printf("bench MaskDoc::save 8K           %8.1f ms\n",
                median_ms([&] { d.save(layer.string(), masks.string(), idx, err); }));
    std::printf("bench undo x20                   %8.1f ms\n",
                median_ms([&] { for (int k = 0; k < 20; k++) d.undo(); for (int k = 0; k < 20; k++) d.redo(); }, 1));
    // What the slideshow decodes per frame, single-threaded.
    gui::Picture pic;
    const std::string img0 = (images / "f0000.jpg").string(), msk0 = (masks / "f0000.png").string();
    const double p1024 = median_ms([&] { gui::load_picture(img0, msk0, 1024, pic); });
    std::printf("bench load_picture 8K -> 1024    %8.1f ms   (%dx%d picture, %.1f MB; 1 thread = %.1f fps)\n",
                p1024, pic.w, pic.h, pic.bytes() / 1048576.0, 1000.0 / p1024);
    const double p2048 = median_ms([&] { gui::load_picture(img0, msk0, 2048, pic); });
    std::printf("bench load_picture 8K -> 2048    %8.1f ms   (%dx%d picture, %.1f MB)\n",
                p2048, pic.w, pic.h, pic.bytes() / 1048576.0);
    // The app's own target on a full-screen pane: the row an in-app
    // slideshow rate is read against.
    const double p4096 = median_ms([&] { gui::load_picture(img0, msk0, 4096, pic); });
    std::printf("bench load_picture 8K -> 4096    %8.1f ms   (%dx%d picture, %.1f MB; 1 thread = %.1f fps)\n",
                p4096, pic.w, pic.h, pic.bytes() / 1048576.0, 1000.0 / p4096);
    const fs::path hd = root / "hd", hd_images = hd / "images", hd_masks = hd / "masks";
    fs::create_directories(hd_images, ec);
    fs::create_directories(hd_masks, ec);
    for (int i = 0; i < kHdFrames; i++) {
        char buf[16];
        std::snprintf(buf, sizeof buf, "h%04d", i);
        const std::string key(buf);
        if (!fs::exists(hd_images / (key + ".jpg")))
            write_jpg_rgb(hd_images / (key + ".jpg"), 1920, 1080, synth_rgb(1920, 1080, (uint32_t)i));
        if (!fs::exists(hd_masks / (key + ".png")))
            write_png_gray(hd_masks / (key + ".png"), 1920, 1080, synth_mask(1920, 1080, (uint32_t)i));
    }
    const double h1024 = median_ms([&] {
        gui::load_picture((hd_images / "h0000.jpg").string(), (hd_masks / "h0000.png").string(), 1024, pic);
    });
    std::printf("bench load_picture 1080p -> 1024 %8.1f ms   (1 thread = %.1f fps)\n", h1024, 1000.0 / h1024);
    // load_picture takes an absent mask as silently as a present one, so pin
    // that each timed load returned, at its step size, with the tint laid on.
    gui::Picture bare;
    const std::string hd0 = (hd_images / "h0000.jpg").string(), hd0m = (hd_masks / "h0000.png").string();
    const bool hd_ok = gui::load_picture(hd0, hd0m, 1024, pic);
    check(hd_ok && gui::load_picture(hd0, "", 1024, bare) && pic.w == 960 && pic.h == 540 &&
              pic.rgb != bare.rgb, "bench: the 1080p load decoded at 960x540 with its mask applied");
    const bool k8_ok = gui::load_picture(img0, msk0, 4096, pic);
    check(k8_ok && gui::load_picture(img0, "", 4096, bare) && pic.w == 3840 && pic.h == 1920 &&
              pic.rgb != bare.rgb, "bench: the 8K load decoded at 3840x1920 with its mask applied");
    // The pool over 100 frames, slide_threads' decoders, at the window
    // depth the session would ask for at this target. Reports the sustained rate, the
    // longest wait once warm, and the cold first frame as its own number.
    auto pool_rate = [&](const fs::path& im, const fs::path& mk_, const char* fmt, int files,
                         int src_w, int src_h, int target, const char* label, double bar_fps) {
        std::vector<mk::SlideFrame> frames;
        for (int i = 0; i < 100; i++) {
            char buf[16];
            std::snprintf(buf, sizeof buf, fmt, i % files);
            const std::string key(buf);
            frames.push_back({(im / (key + ".jpg")).string(), (mk_ / (key + ".png")).string()});
        }
        const int depth = mk::slide_depth(mk::slide_picture_bytes(src_w, src_h, target), 100);
        mk::SlidePrefetch p;
        p.set_target(target);
        const int threads = mk::slide_threads(src_w, src_h, std::thread::hardware_concurrency());
        p.start(frames, threads);
        gui::Picture pic;
        // The first kWarm takes fill an empty ring from cold. Timing them puts
        // one cold 8K decode into a number the spec defines as a sustained rate.
        const int kWarm = 8;
        double max_gap = 0.0, first_ms = 0.0;
        const auto t_cold = std::chrono::steady_clock::now();
        auto t0 = t_cold, last = t_cold;
        p.want(0, depth);
        for (int i = 0; i < 100; i++) {
            while (!p.take(i, pic)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            p.want((i + 1) % 100, depth);
            const auto now = std::chrono::steady_clock::now();
            if (i == 0) first_ms = std::chrono::duration<double, std::milli>(now - t_cold).count();
            if (i < kWarm - 1) continue;
            if (i == kWarm - 1) { t0 = now; last = now; continue; }
            max_gap = std::max(max_gap, std::chrono::duration<double, std::milli>(now - last).count());
            last = now;
        }
        const double s = std::chrono::duration<double>(last - t0).count();
        const double fps = (100.0 - kWarm) / s;
        const int decodes = p.decoded();
        p.stop();
        // One decode a shown frame and the window wrapping onto frames 0..depth-1
        // as the last ones are shown; the + 1 keeps the bar the rows were set against.
        const int max_decodes = 100 + 1 + depth;
        std::printf("bench slideshow pool %s %4d  threads %d  depth %2d  %8.1f fps   longest wait %8.1f ms   "
                    "first frame %8.1f ms   decodes %3d   [bar: >= %.0f fps, wait <= 400 ms, "
                    "decodes <= %d] %s\n",
                    label, target, threads, depth, fps, max_gap, first_ms, decodes, bar_fps, max_decodes,
                    (fps >= bar_fps && max_gap <= 400.0 && decodes <= max_decodes) ? "PASS" : "MISS");
    };
    pool_rate(images, masks, "f%04d", 3, 7680, 3840, 1024, "8K   ", 5.0);
    pool_rate(images, masks, "f%04d", 3, 7680, 3840, 4096, "8K   ", 5.0);
    pool_rate(hd_images, hd_masks, "h%04d", kHdFrames, 1920, 1080, 1024, "1080p", 30.0);
    pool_rate(hd_images, hd_masks, "h%04d", kHdFrames, 1920, 1080, 4096, "1080p", 30.0);
    fs::remove_all(layer, ec);
    for (int i = 0; i < 3; i++) {
        const std::string key = "f000" + std::to_string(i);
        write_png_gray(masks / (key + ".png"), W, H, synth_mask(W, H, (uint32_t)i));
    }
}

// Propagate from the first of kPropFrames 8K frames onto the rest, end to end
// through the session. Copies bench_8k's f0000 fixture, so runs after it.
constexpr int kPropFrames = 24;

void bench_propagate(const char* dir) {
    const fs::path src(dir), root = src / "propagate", images = root / "images", masks = root / "masks";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(images, ec);
    fs::create_directories(masks, ec);
    for (int i = 0; i < kPropFrames; i++) {
        char key[16];
        std::snprintf(key, sizeof key, "p%04d", i);
        fs::copy_file(src / "images" / "f0000.jpg", images / (std::string(key) + ".jpg"), ec);
        fs::copy_file(src / "masks" / "f0000.png", masks / (std::string(key) + ".png"), ec);
    }
    mk::MaskSession s;
    std::string err;
    if (!s.open(root.string(), images.string(), masks.string(), false, err)) {
        std::printf("bench propagate: open failed: %s\n", err.c_str());
        return;
    }
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{1000.0f, 1000.0f, 3000.0f, 2000.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    for (const char* what : {"propagate", "undo"}) {
        const auto a = std::chrono::steady_clock::now();
        if (what[0] == 'p') s.propagate(mk::PropagateScope::Camera, 0, 0);
        else s.undo_propagate();
        while (!s.idle()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        s.pump();
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - a).count();
        std::printf("bench %-9s 8K x%d            %8.1f ms  (%.1f ms a frame, %d done)\n", what,
                    kPropFrames - 1, ms, ms / (kPropFrames - 1), s.last_propagate().done);
    }
    s.close();
    fs::remove_all(root, ec);
}

// ---------------------------------------------------------------------------
// Livewire floors: build <= 300 ms at 8K, built once; <= 16 ms per cursor
// move, p95, after the first 200 ms.
// ---------------------------------------------------------------------------

// p95/max of a sorted-in-place sample; (0, 0) on an empty one.
void p95_max(std::vector<double>& v, double& p95, double& mx) {
    std::sort(v.begin(), v.end());
    p95 = v.empty() ? 0.0 : v[(size_t)(0.95 * (double)(v.size() - 1))];
    mx = v.empty() ? 0.0 : v.back();
}

// Pearson r between move index and its time, in call order: whether cost
// trends with index (it does).
double index_corr(const std::vector<double>& y) {
    const size_t n = y.size();
    double mi = 0.0, my = 0.0;
    for (size_t i = 0; i < n; i++) { mi += (double)i; my += y[i]; }
    mi /= (double)n; my /= (double)n;
    double cov = 0.0, vi = 0.0, vy = 0.0;
    for (size_t i = 0; i < n; i++) {
        const double di = (double)i - mi, dy = y[i] - my;
        cov += di * dy; vi += di * di; vy += dy * dy;
    }
    return (vi > 0.0 && vy > 0.0) ? cov / std::sqrt(vi * vy) : 0.0;
}

// p95 of the last n moves in call order: what the warm-up filter would keep
// if it admitted a fixed suffix instead of a cumulative-time threshold.
double tail_p95(const std::vector<double>& all, size_t n) {
    std::vector<double> tail(all.end() - (long)std::min(n, all.size()), all.end());
    double p95, mx;
    p95_max(tail, p95, mx);
    return p95;
}

void bench_livewire_on(const char* label, const std::vector<uint8_t>& rgb, int fw, int fh) {
    mk::Livewire lw;
    // One direct call, not median_ms (repeats=3 would leave builds() at 3
    // before a single cursor move, making builds==1 unmeasurable).
    const auto t0 = std::chrono::steady_clock::now();
    lw.build(rgb.data(), fw, fh);
    const double build_ms = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - t0).count();
    std::printf("bench livewire %-8s %dx%d -> grid %dx%d step %d  build %8.1f ms  [bar: <= 300 at 8K]  bytes %.1f MB\n",
                label, fw, fh, lw.width(), lw.height(), lw.step(), build_ms,
                (double)lw.bytes() / 1048576.0);
    // A segment: anchor a third in, cursor walks right 2 grid px/move, 200
    // moves each timed alone. `ms` is the brief's cumulative-time warm-up
    // filter; `ms_all` is unfiltered (see report: the filter can starve).
    const int ax = lw.width() / 3, ay = lw.height() / 2;
    lw.set_anchor(ax, ay);
    std::vector<int> path;
    std::vector<double> ms, ms_all;
    std::vector<size_t> dpops;
    double elapsed = 0.0;
    size_t skipped = 0, found = 0, prev_pops = lw.pops();
    for (int i = 1; i <= 200; i++) {
        const auto a = std::chrono::steady_clock::now();
        const bool ok = lw.path_to(std::min(lw.width() - 1, ax + 2 * i), ay + (i % 5) - 2, path);
        const auto b = std::chrono::steady_clock::now();
        if (ok && !path.empty()) found++;
        const double t = std::chrono::duration<double, std::milli>(b - a).count();
        ms_all.push_back(t);
        dpops.push_back(lw.pops() - prev_pops);
        prev_pops = lw.pops();
        elapsed += t;
        if (elapsed <= 200.0) { skipped++; continue; }
        ms.push_back(t);
    }
    double p95, mx, p95_all, mx_all;
    p95_max(ms, p95, mx);
    std::vector<double> ms_all_sorted = ms_all;
    p95_max(ms_all_sorted, p95_all, mx_all);
    std::printf("bench livewire %-8s cursor moves: %zu timed (%zu in the first 200 ms), p95 %8.2f ms  max %8.2f ms  | all 200: p95 %8.2f ms  max %8.2f ms  pops %zu  builds %d  found %zu/200  [bar: p95 <= 16, builds == 1]\n",
                label, ms.size(), skipped, p95, mx, p95_all, mx_all, lw.pops(), lw.builds(), found);
    // Does cost trend with move index, and what would a suffix-based filter
    // (rather than the cumulative-time one above) have reported.
    std::vector<size_t> dp = dpops;
    std::sort(dp.begin(), dp.end());
    std::printf("bench livewire %-8s tail p95: last150 %7.3f  last100 %7.3f  last50 %7.3f ms  corr(idx,ms) %+.3f  elapsed_total %7.1f/200 ms  pops/move min/median/max %zu/%zu/%zu\n",
                label, tail_p95(ms_all, 150), tail_p95(ms_all, 100), tail_p95(ms_all, 50),
                index_corr(ms_all), elapsed, dp.front(), dp[dp.size() / 2], dp.back());
}

void bench_livewire(const char* dir) {
    const fs::path images = fs::path(dir) / "images";
    const fs::path f0 = images / "f0000.jpg";
    if (!fs::exists(f0))
        write_jpg_rgb(f0, 7680, 3840, synth_rgb(7680, 3840, 0));
    int fw, fh;
    std::vector<uint8_t> rgb;
    if (app::load_rgb(f0.string(), fw, fh, rgb)) bench_livewire_on("8K", rgb, fw, fh);
    else std::printf("bench livewire 8K: could not read %s\n", f0.string().c_str());
    if (const char* real = std::getenv("SS_LIVEWIRE_IMAGE")) {
        if (app::load_rgb(real, fw, fh, rgb)) bench_livewire_on("still", rgb, fw, fh);
        else std::printf("bench livewire still: could not read %s\n", real);
    }
}

// ---------------------------------------------------------------------------
// SAM assist: the seam (MaskAdd.h)
// ---------------------------------------------------------------------------

// A w x h plane holding one filled disc; every seam fixture is built from these
// so the expected counts are computed, not typed.
mk::AddRegion disc_region(int w, int h, float cx, float cy, float r) {
    mk::AddRegion g;
    g.w = w;
    g.h = h;
    g.mask.assign((size_t)w * h, 0);
    g.score = 0.9f;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) g.mask[(size_t)y * w + x] = 255;
        }
    return g;
}

size_t set_pixels(const std::vector<uint8_t>& v) {
    size_t n = 0;
    for (uint8_t b : v) n += b ? 1 : 0;
    return n;
}

// The exact extent of a plane's set pixels; empty when none are set.
mk::Rect extent(const std::vector<uint8_t>& v, int w, int h) {
    mk::Rect r{w, h, 0, 0};
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (v[(size_t)y * w + x]) {
                r.x0 = std::min(r.x0, x);
                r.y0 = std::min(r.y0, y);
                r.x1 = std::max(r.x1, x + 1);
                r.y1 = std::max(r.y1, y + 1);
            }
    return r;
}

bool same_rect(const mk::Rect& a, const mk::Rect& b) {
    return a.x0 == b.x0 && a.y0 == b.y0 && a.x1 == b.x1 && a.y1 == b.y1;
}

void test_add_stencil_same_size() {
    std::vector<mk::AddRegion> r{disc_region(64, 48, 20.0f, 20.0f, 6.0f)};
    const std::vector<uint8_t> plane = r[0].mask;
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, 64, 48, st, b, set), "add stencil: same size builds");
    check(st.W == 64 && st.H == 48 && st.in == plane,
          "add stencil: same size is the plane, pixel for pixel");
    check(set == (int64_t)set_pixels(plane), "add stencil: set_px counts the plane's set pixels");
    check(same_rect(b, extent(plane, 64, 48)),
          "add stencil: bounds are the exact extent, not the frame");
}

void test_add_stencil_resamples() {
    std::vector<mk::AddRegion> r{disc_region(64, 48, 20.0f, 20.0f, 6.0f)};
    const size_t src = set_pixels(r[0].mask);
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, 128, 96, st, b, set), "add stencil: resample builds");
    check(st.W == 128 && st.H == 96 && st.in.size() == (size_t)128 * 96,
          "add stencil: resampled to the document size");
    const size_t n = set_pixels(st.in);
    check(n == 4 * src, "add stencil: a 2x nearest resample quadruples the area exactly");
    check(set == (int64_t)n, "add stencil: set_px counts the resampled plane");
    check(same_rect(b, extent(st.in, 128, 96)),
          "add stencil: bounds are the resampled plane's exact extent");
}

// The first two discs overlap, so a count summed per region is caught.
// A 3:2 upscale, where floor(d * src / dst) and a pixel-centre mapping part:
// sam::Masker's rule lands source (1,1) on (2,2) alone, the centre rule on 2x2.
void test_add_stencil_resample_matches_masker() {
    std::vector<mk::AddRegion> r(1);
    r[0].w = 2;
    r[0].h = 2;
    r[0].mask = {0, 0, 0, 255};
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, 3, 3, st, b, set), "add stencil: 3:2 resample builds");
    const std::vector<uint8_t> want{0, 0, 0, 0, 0, 0, 0, 0, 255};
    check(st.in == want && set == 1 && same_rect(b, mk::Rect{2, 2, 3, 3}),
          "add stencil: a 3:2 resample follows sam::Masker's floor mapping");
}

void test_add_stencil_unions_three() {
    std::vector<mk::AddRegion> r{disc_region(64, 48, 10.0f, 10.0f, 4.0f),
                                 disc_region(64, 48, 14.0f, 12.0f, 4.0f),
                                 disc_region(64, 48, 52.0f, 38.0f, 4.0f)};
    std::vector<uint8_t> want((size_t)64 * 48, 0);
    for (const mk::AddRegion& g : r)
        for (size_t i = 0; i < want.size(); i++)
            if (g.mask[i]) want[i] = 255;
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, 64, 48, st, b, set), "add stencil: three regions build");
    check(st.in == want, "add stencil: three regions union pixel for pixel");
    check(set == (int64_t)set_pixels(want), "add stencil: set_px counts an overlap once");
    check(same_rect(b, extent(want, 64, 48)), "add stencil: bounds span exactly all three");
}

void test_add_stencil_rejects_empty() {
    gui::Stencil st;
    st.W = 7;
    mk::Rect b{1, 2, 3, 4};
    int64_t set = 99;
    std::vector<mk::AddRegion> none;
    check(!mk::build_add_stencil(none, 64, 48, st, b, set), "add stencil: no regions is false");
    std::vector<mk::AddRegion> blank{disc_region(64, 48, 10.0f, 10.0f, 0.0f)};
    check(!mk::build_add_stencil(blank, 64, 48, st, b, set), "add stencil: an empty plane is false");
    std::vector<mk::AddRegion> bad(1);
    bad[0].w = 4;
    bad[0].h = 4;
    bad[0].mask.assign(3, 255);
    check(!mk::build_add_stencil(bad, 64, 48, st, b, set),
          "add stencil: a plane of the wrong length is skipped");
    std::vector<mk::AddRegion> big(1);
    big[0].w = 4;
    big[0].h = 4;
    big[0].mask.assign(20, 255);
    check(!mk::build_add_stencil(big, 64, 48, st, b, set),
          "add stencil: an oversized plane is skipped too");
    check(st.W == 7 && st.in.empty() && same_rect(b, mk::Rect{1, 2, 3, 4}) && set == 99,
          "add stencil: false leaves all three outputs untouched");
}

// One prompt is one undo step, for one region and a three-region union,
// each at the document's size and at half of it. The half-size cases are what
// a std::move implementation fails: MaskDoc::paint returns silently there.
void test_add_stencil_is_one_undo_step() {
    const char* names[] = {"add_undo_1r_1x", "add_undo_1r_2x", "add_undo_3r_1x",
                           "add_undo_3r_2x"};
    int case_no = 0;
    for (int regions = 1; regions <= 3; regions += 2)
        for (int scale = 1; scale <= 2; scale++) {
            const std::string tag = "add undo, " + std::to_string(regions) +
                                    " region(s) at 1/" + std::to_string(scale) + ": ";
            mk::MaskDoc doc;
            mk::LayerIndex idx;
            const fs::path d = scratch(names[case_no++]);
            idx.mask_root = (d / "masks").string();
            std::string err, warn;
            check(doc.load((d / "layers").string(), (d / "masks").string(), "f", 64, 48, idx,
                           err, warn),
                  tag + "document loads");
            const std::vector<uint8_t> before = doc.composite();
            const int64_t kept_before = doc.kept();
            const int w = 64 / scale, h = 48 / scale;
            const float k = 1.0f / (float)scale;
            std::vector<mk::AddRegion> r{disc_region(w, h, 20.0f * k, 20.0f * k, 6.0f * k)};
            if (regions == 3) {
                r.push_back(disc_region(w, h, 40.0f * k, 30.0f * k, 5.0f * k));
                r.push_back(disc_region(w, h, 10.0f * k, 38.0f * k, 4.0f * k));
            }
            gui::Stencil st;
            mk::Rect b;
            int64_t set = 0;
            check(mk::build_add_stencil(r, 64, 48, st, b, set), tag + "stencil builds");
            doc.paint(mk::Paint::ForceDrop, std::move(st), b);
            check(doc.history_size() == 1, tag + "ONE history entry");
            check(same_rect(doc.last_change(), b), tag + "the entry touched exactly the bounds");
            check(doc.kept() == kept_before - set, tag + "the add dropped exactly set_px pixels");
            doc.undo();
            check(doc.composite() == before, tag + "undo restores every pixel");
            check(doc.kept() == kept_before, tag + "undo restores the kept count");
        }
}

// What one ForceDrop add of a disc well inside a W x H document costs.
struct AddCost {
    size_t bytes = 0;        // history growth from the one add; 0 = the arm failed
    double paint_ms = 0.0;   // MaskDoc::paint alone
};

// `prior` first paints a speckled edit into the top-left tenth -- the only
// thing a whole-frame rect re-encodes. `whole_frame` is the mutant: the same
// stencil handed to paint with the full-frame rect instead of the bounds.
AddCost add_cost(const char* name, int W, int H, bool prior, bool whole_frame) {
    AddCost c;
    mk::MaskDoc doc;
    mk::LayerIndex idx;
    const fs::path d = scratch(name);
    idx.mask_root = (d / "masks").string();
    std::string err, warn;
    if (!doc.load((d / "layers").string(), (d / "masks").string(), "f", W, H, idx, err, warn))
        return c;
    if (prior) {
        gui::Stencil sp;
        sp.W = W;
        sp.H = H;
        sp.in.assign((size_t)W * H, 0);
        uint32_t s = 1;
        for (int y = 0; y < H / 10; y++)
            for (int x = 0; x < W / 10; x++) {
                s = s * 1664525u + 1013904223u;
                if ((s >> 24) & 1u) sp.in[(size_t)y * W + x] = 255;
            }
        doc.paint(mk::Paint::ForceDrop, std::move(sp), mk::Rect{0, 0, W / 10, H / 10});
    }
    const int ops_before = doc.history_size();
    const size_t bytes_before = doc.history_bytes();
    std::vector<mk::AddRegion> r{disc_region(W, H, 0.6f * W, 0.55f * H, H / 5.0f)};
    gui::Stencil st;
    mk::Rect b;
    int64_t n = 0;
    if (!mk::build_add_stencil(r, W, H, st, b, n)) return c;
    const auto t0 = std::chrono::steady_clock::now();
    doc.paint(mk::Paint::ForceDrop, std::move(st), whole_frame ? mk::Rect{0, 0, W, H} : b);
    c.paint_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0)
                     .count();
    if (doc.history_size() != ops_before + 1) return AddCost{};
    c.bytes = doc.history_bytes() - bytes_before;
    return c;
}

// The fresh-document pair records why the fixture carries an earlier edit:
// without one, history_bytes cannot tell the whole-frame rect from the bounds.
void test_add_history_bytes() {
    const AddCost own = add_cost("add_bytes_own", 1552, 776, true, false);
    const AddCost whole = add_cost("add_bytes_whole", 1552, 776, true, true);
    const AddCost fresh_own = add_cost("add_bytes_fresh_own", 1552, 776, false, false);
    const AddCost fresh_whole = add_cost("add_bytes_fresh_whole", 1552, 776, false, true);
    std::printf("add bytes 1552x776: own %zu, whole %zu, fresh own %zu, fresh whole %zu bytes\n",
                own.bytes, whole.bytes, fresh_own.bytes, fresh_whole.bytes);
    check(own.bytes > 0 && whole.bytes > 0 && fresh_own.bytes > 0 && fresh_whole.bytes > 0,
          "add bytes: every arm painted exactly one add");
    check(whole.bytes >= 10 * own.bytes,
          "add bytes: after an earlier edit the whole-frame rect costs 10x the bounds");
    check(fresh_whole.bytes < 2 * fresh_own.bytes,
          "add bytes: on a fresh document the whole-frame rect is invisible to history_bytes");
    constexpr size_t kAddBar = 2 * 1472;
    check(own.bytes <= kAddBar && whole.bytes > kAddBar,
          "add bytes: one add is within the bar, and the whole-frame rect is not");
}

// Add history bytes at the size that matters, four arms, one document at a time: ~1 GB peak
// (four 120 MB planes, the 120 MB disc and stencil, and the mutant's read_rect).
void bench_add_history() {
    const int W = 15520, H = 7760;
    for (int prior = 0; prior <= 1; prior++)
        for (int whole = 0; whole <= 1; whole++) {
            const AddCost c = add_cost("bench_add_history", W, H, prior != 0, whole != 0);
            std::printf("bench add history %dx%d earlier_edit=%d whole_frame=%d: %zu bytes, paint %.1f ms\n",
                        W, H, prior, whole, c.bytes, c.paint_ms);
        }
}

// ---------------------------------------------------------------------------
// SAM assist: the guarded half's stub, and the editor's own clicks
// ---------------------------------------------------------------------------

void test_mask_sam_stub_refuses() {
    mk::MaskSam sam;
    const auto px = std::make_shared<const std::vector<uint8_t>>((size_t)4 * 4 * 3, 7);
    check(!mk::MaskSam::available(), "sam stub: unavailable without SS_BUILD_SAM");
    check(mk::MaskSam::pool_mib() < 0.0, "sam stub: no pool to read");
    check(!sam.has_model(), "sam stub: no model by default");
    sam.set_model("/nonexistent/sam3.ggml", true);
    check(sam.has_model(), "sam stub: set_model is recorded");
    check(!sam.text_supported(), "sam stub: no text tower whatever the catalog says");
    check(!sam.busy(), "sam stub: never busy");
    check(sam.vram_mib() < 0.0, "sam stub: reports no device accounting");
    check(!sam.start_points("f", px, 4, 4, 4, 4, {mk::SamPoint{1.0f, 1.0f}}, mk::Paint::ForceDrop,
                            0.05f),
          "sam stub: refuses a point");
    check(!sam.start_text("f", px, 4, 4, 4, 4, "person", 0.05f), "sam stub: refuses a phrase");
    check(px.use_count() == 1, "sam stub: a refused start keeps no reference to the frame");
    mk::SamResult out;
    out.frame_key = "untouched";
    out.mode = mk::Paint::Clear;
    out.score = -1.0f;
    out.ms = -1.0;
    out.detections = 7;
    out.set_px = 5;
    check(!sam.take_result(out), "sam stub: no result to take");
    check(out.frame_key == "untouched" && out.mode == mk::Paint::Clear && out.score == -1.0f &&
              out.ms == -1.0 && out.detections == 7 && out.set_px == 5,
          "sam stub: take_result left every output alone");
    check(sam.status().empty(), "sam stub: status is empty with no job run");
    check(sam.error() == spirula::i18n::msg::maskedit::sam_unavailable_build.get(),
          "sam stub: a refused start puts its reason in error, not in status");
    sam.cancel();
    sam.release();
    check(!sam.busy() && sam.vram_mib() < 0.0 && sam.has_model(),
          "sam stub: release leaves it idle, unaccounted, and still pointed at its model");
}

// Clicks on two objects, two frames and two cameras: a prompt must send only
// the current object's clicks on this frame and camera, in click order.
void test_mask_sam_clicks() {
    mk::MaskSam sam;
    gui::MaskSettings& p = sam.prompt();
    check(p.clicks.empty() && p.object_count == 1 && p.current_object == 0,
          "sam clicks: the editor's state starts empty");
    sam.add_click(3, "cam0", 10.0f, 20.0f);
    sam.add_click(3, "cam0", 30.0f, 40.0f);
    sam.add_click(4, "cam0", 50.0f, 60.0f);
    sam.add_click(3, "cam1", 70.0f, 80.0f);
    p.current_object = p.object_count++;
    sam.add_click(3, "cam0", 90.0f, 99.0f);
    const gui::MaskClick& last = p.clicks.back();
    check(p.clicks.size() == 5 && last.object == 1 && last.frame == 3 && last.camera == "cam0" &&
              last.source.empty() && last.positive && last.x == 90.0f && last.y == 99.0f,
          "sam clicks: a click is keyed by frame index, camera and current object, source empty");
    const std::vector<mk::SamPoint> one = sam.object_points(3, "cam0");
    check(one.size() == 1 && one[0].x == 90.0f && one[0].y == 99.0f,
          "sam clicks: a prompt sends only the current object's clicks");
    p.current_object = 0;
    const std::vector<mk::SamPoint> zero = sam.object_points(3, "cam0");
    check(zero.size() == 2 && zero[0].x == 10.0f && zero[1].x == 30.0f,
          "sam clicks: ... only this frame and camera, in click order");
    check(sam.object_points(3, "cam2").empty(), "sam clicks: a camera with no clicks sends none");
    const mk::MaskSam& ro = sam;
    check(&ro.prompt() == &p && ro.prompt().clicks.size() == 5 && ro.prompt().object_count == 2,
          "sam clicks: the const prompt() reads the state the non-const one wrote");
}

// A "not this" click travels with its label, in click order among the "this"
// clicks; filtering or relabelling either kind changes what SAM is told.
void test_mask_sam_negative_clicks() {
    mk::MaskSam sam;
    sam.add_click(3, "cam0", 10.0f, 20.0f);
    sam.add_click(3, "cam0", 30.0f, 40.0f, /*positive=*/false);
    sam.add_click(3, "cam0", 50.0f, 60.0f);
    const std::vector<mk::SamPoint> pts = sam.object_points(3, "cam0");
    check(pts.size() == 3, "sam negative: a negative click is sent, not dropped");
    check(pts.size() == 3 && pts[0].positive && !pts[1].positive && pts[2].positive &&
              pts[1].x == 30.0f && pts[1].y == 40.0f,
          "sam negative: each point keeps its own label, in click order");
    check(!sam.prompt().clicks[1].positive, "sam negative: the stored click is negative");
}

// Frame 6 holds a click that must be sent beside one that must not, so a dropped
// source filter shows as a second point; frame 5's "not this" is sent, labelled.
void test_mask_sam_click_filters() {
    mk::MaskSam sam;
    gui::MaskSettings& p = sam.prompt();
    sam.add_click(5, "cam0", 1.0f, 1.0f);
    sam.add_click(5, "cam0", 2.0f, 2.0f);
    p.clicks.back().positive = false;
    sam.add_click(6, "cam0", 3.0f, 3.0f);
    sam.add_click(6, "cam0", 4.0f, 4.0f);
    p.clicks.back().source = "/captures/a.mp4";
    const std::vector<mk::SamPoint> neg = sam.object_points(5, "cam0");
    check(neg.size() == 2 && neg[0].x == 1.0f && neg[0].positive && neg[1].x == 2.0f &&
              !neg[1].positive,
          "sam clicks: a negative click is sent beside the positive one, labelled");
    const std::vector<mk::SamPoint> src = sam.object_points(6, "cam0");
    check(src.size() == 1 && src[0].x == 3.0f,
          "sam clicks: a click carrying a dataset source is excluded");
}

// ---------------------------------------------------------------------------
// SAM assist: the frame buffer a job holds (P8a)
// ---------------------------------------------------------------------------

// A holder's frame buffer outlives the frame change that replaces it and the
// close that drops it. The two frames differ, so a shared or recycled buffer
// would change under its holder: the use-after-free, in behavioural form.
void test_session_frame_pixels_outlive_navigation() {
    Fixture f = make_dataset("frame_pixels", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "frame pixels: open: " + err);
    settle(s);
    const std::shared_ptr<const std::vector<uint8_t>> held = s.frame_pixels();
    check(held && held->size() == (size_t)64 * 48 * 3,
          "frame pixels: frame 0 is held at fw x fh x 3");
    const std::vector<uint8_t> frame0 = held ? *held : std::vector<uint8_t>();
    s.go_to(1);
    settle(s);
    const std::shared_ptr<const std::vector<uint8_t>> now = s.frame_pixels();
    check(now && now.get() != held.get() && *now != frame0,
          "frame pixels: frame 1 is a different buffer with different pixels");
    check(held && *held == frame0,
          "frame pixels: the held frame-0 buffer is unchanged after the frame change");
    s.close();
    check(!s.frame_pixels(), "frame pixels: close drops the session's reference");
    check(held && *held == frame0, "frame pixels: the held buffer is unchanged after close");
}

// GuiApp pushes its model every frame: only a changed path counts, and a change
// drops the session but never the editor's own clicks, which are frame pixels.
void test_session_model_sync() {
    mk::MaskSession s;
    for (int i = 0; i < 3; i++) s.set_sam_model("/m/a.ggml", true);
    check(s.sam_model_changes() == 1 && s.sam_model_path() == "/m/a.ggml",
          "model sync: the same path pushed every frame is one change");
    gui::MaskClick c;
    c.x = 5.0f;
    s.sam_prompt().clicks.push_back(c);
    s.set_sam_model("/m/b.ggml", false);
    s.sam_pump();
    check(s.sam_model_changes() == 2 && s.sam_model_path() == "/m/b.ggml",
          "model sync: a different path is a second change");
    check(s.sam_click_count() == 1, "model sync: a model change keeps the editor's clicks");
    s.set_sam_model("", false);
    check(s.sam_model_changes() == 3 && !s.sam_has_model(),
          "model sync: an uncached pick leaves no model");
}

// A job's stamp names the document it started on, not only the frame's key: a
// revert reopens the same key, and so can another dataset, and a result from
// before either must not land on what is open after it.
void test_session_sam_stamp() {
    Fixture f = make_dataset("sam_stamp", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam stamp: open: " + err);
    settle(s);
    const std::string key0 = s.doc() ? s.doc()->key() : std::string();
    const std::string first = s.sam_frame_stamp();
    s.revert_open_frame();
    settle(s);
    check(s.doc() && !key0.empty() && s.doc()->key() == key0,
          "sam stamp: a revert reopens the same key");
    check(s.sam_frame_stamp() != first, "sam stamp: a revert moves the stamp");
    const std::string reverted = s.sam_frame_stamp();
    s.go_to(1);
    settle(s);
    s.go_to(0);
    settle(s);
    check(s.sam_frame_stamp() != reverted && s.sam_frame_stamp() != first,
          "sam stamp: leaving a frame and coming back moves the stamp");
    const std::string back = s.sam_frame_stamp();
    s.revert_every_frame();
    settle(s);
    check(s.sam_frame_stamp() != back, "sam stamp: revert all moves the stamp");
    const std::string last = s.sam_frame_stamp();
    s.close();
    Fixture g = make_dataset("sam_stamp_other", 64, 48, {"a", "b"});
    check(s.open(g.root.string(), g.images.string(), g.masks.string(), false, err),
          "sam stamp: reopen: " + err);
    settle(s);
    check(s.doc() && s.doc()->key() == key0 && s.sam_frame_stamp() != first &&
              s.sam_frame_stamp() != last,
          "sam stamp: another dataset with the same keys never repeats a stamp");
}

// Another inference user holds the device: the editor names it and refuses,
// and a yield hands the device back without costing the operator's clicks.
void test_session_sam_blocker() {
    namespace em = spirula::i18n::msg::maskedit;
    const std::string preview = em::sam_blocked_preview.get();
    const std::string run = em::sam_blocked_run.get();
    check(mk::MaskSession::sam_blocker(false, false, false).empty(),
          "sam blocker: nothing open and nothing running leaves SAM free");
    check(mk::MaskSession::sam_blocker(true, false, false) == preview,
          "sam blocker: the mask preview blocks");
    check(mk::MaskSession::sam_blocker(false, true, false) == preview,
          "sam blocker: the depth preview blocks");
    check(mk::MaskSession::sam_blocker(false, false, true) == run,
          "sam blocker: a run blocks");
    check(preview != run && !preview.empty() && !run.empty(),
          "sam blocker: the two reasons are distinct sentences");

    Fixture f = make_dataset("sam_blocker", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam blocker: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.set_sam_blocker(run);
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) && s.sam_error() == run,
          "sam blocker: a blocked click is refused, the reason in error");
    check(!s.sam_prompt_text("person") && s.sam_error() == run && s.sam_status().empty(),
          "sam blocker: a blocked phrase is refused, the reason in error");
    s.set_sam_blocker("");
    const std::string stub = em::sam_unavailable_build.get();
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) && s.sam_error() == stub,
          "sam blocker: cleared, the prompt reaches the job and its own reason");

    s.set_sam_blocker(run);
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    s.set_sam_blocker("");
    s.sam_prompt().clicks.push_back(gui::MaskClick{});
    s.set_sam_model("/m/b.ggml", true);
    s.sam_yield();
    check(s.sam_click_count() == 1, "sam yield: the editor's clicks survive a yield");
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) && s.sam_error() == stub &&
              s.sam_model_path() == "/m/b.ggml",
          "sam yield: a yield settles a pending model change");
}

// release() hands the device back AND forgets what the last job said, in both
// builds: a model switch must not go on showing the old checkpoint's error.
void test_mask_sam_release_forgets() {
    mk::MaskSam sam;
    sam.refuse("the old checkpoint failed to load");
    sam.post_result("k", std::vector<mk::AddRegion>(1), 4, 4, mk::Paint::ForceKeep, 0.0f, 0.5f, 1.0);
    sam.release();
    check(sam.error().empty(), "sam release: the last error is forgotten");
    mk::SamResult out;
    check(!sam.take_result(out), "sam release: an untaken result is forgotten");
}

// The job body runs inside run_guarded(): a throw becomes a reported error,
// never std::terminate on the job thread.
void test_mask_sam_run_guarded() {
    std::string got = "untouched";
    int ran = 0;
    mk::run_guarded([&] { ran++; }, [&](const std::string& e) { got = e; });
    check(ran == 1 && got == "untouched",
          "sam guard: a body that returns runs once and reports nothing");
    mk::run_guarded([] { throw std::bad_alloc(); }, [&](const std::string& e) { got = e; });
    check(got == std::bad_alloc().what(), "sam guard: a std::exception lands with its what()");
    got = "untouched";
    mk::run_guarded([] { throw 7; }, [&](const std::string& e) { got = e; });
    check(!got.empty() && got != "untouched", "sam guard: a throw of a non-exception still lands");
}

// A planted result stands in for the job: one stamped before a revert is
// counted and dropped with the document clean, one stamped now paints, and a
// finished result a yield throws away is counted too.
void test_session_sam_result_stamp() {
    Fixture f = make_dataset("sam_result_stamp", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam result: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    mk::AddRegion g;
    g.w = 64;
    g.h = 48;
    g.mask.assign((size_t)64 * 48, 0);
    for (int y = 10; y < 20; y++)
        for (int x = 10; x < 30; x++) g.mask[(size_t)y * 64 + x] = 255;
    g.score = 0.9f;
    const std::string old = s.sam_frame_stamp();
    s.revert_open_frame();
    settle(s);
    s.sam().post_result(old, {g}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 5.0);
    const mk::Rect r0 = s.sam_pump();
    check(s.sam_dropped() == 1 && s.sam_results() == 0 && r0.empty() && s.doc() &&
              !s.doc()->dirty(),
          "sam result: a result stamped before a revert is dropped, the document clean");
    s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 5.0);
    const mk::Rect r1 = s.sam_pump();
    check(s.sam_results() == 1 && s.sam_dropped() == 1 && !r1.empty() && s.doc() &&
              s.doc()->dirty() && s.sam_last_area() == 200,
          "sam result: a result with the current stamp paints its 200 pixels");
    s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 5.0);
    s.sam_yield();
    check(s.sam_dropped() == 2 && s.sam_results() == 1,
          "sam yield: a finished result the yield throws away is counted as dropped");
}

// Lifting a pause takes its message with it; any other error stays.
void test_session_sam_blocker_clears_its_own_error() {
    namespace em = spirula::i18n::msg::maskedit;
    const std::string run = em::sam_blocked_run.get();
    Fixture f = make_dataset("sam_blocker_clear", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam unpause: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.set_sam_blocker(run);
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    check(s.sam_error() == run, "sam unpause: the paused prompt reported the pause");
    s.set_sam_blocker("");
    check(s.sam_error().empty(), "sam unpause: the pause message goes when the pause does");
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    const std::string real = s.sam_error();
    s.set_sam_blocker(run);
    s.set_sam_blocker("");
    check(!real.empty() && real != run && s.sam_error() == real,
          "sam unpause: lifting a pause leaves a real error standing");
}

// ---------------------------------------------------------------------------
// SAM assist: the canvas mode
// ---------------------------------------------------------------------------

// Every ordered pair of canvas modes: picking the second must leave exactly
// the second on, whatever the first was. A mode kept in its own flag fails.
void test_canvas_mode_is_exclusive() {
    mk::MaskSession s;
    const mk::CanvasMode all[] = {mk::CanvasMode::Shape, mk::CanvasMode::Eraser,
                                  mk::CanvasMode::Path, mk::CanvasMode::Pen,
                                  mk::CanvasMode::Sam};
    bool exclusive = true;
    for (mk::CanvasMode a : all)
        for (mk::CanvasMode b : all) {
            s.set_mode(a);
            s.set_mode(b);
            exclusive = exclusive && s.mode() == b &&
                        s.erasing() == (b == mk::CanvasMode::Eraser) &&
                        s.path_mode() == (b == mk::CanvasMode::Path) &&
                        s.pen_mode() == (b == mk::CanvasMode::Pen) &&
                        s.sam_mode() == (b == mk::CanvasMode::Sam);
        }
    check(exclusive, "canvas mode: selecting any mode deselects every other");
    s.set_mode(mk::CanvasMode::Sam);
    s.set_erasing(false);
    check(s.mode() == mk::CanvasMode::Shape, "canvas mode: set_erasing(false) leaves SAM for the shapes");
    s.set_mode(mk::CanvasMode::Path);
    s.set_erasing(true);
    check(s.erasing() && !s.path_mode(), "canvas mode: set_erasing(true) leaves the pen");
}

// The dataset screen's own branch order over all eight inputs: a download in
// flight shows progress even for a cached entry, and no entry shows nothing.
void test_mask_picker_row() {
    using gui::PickerRow;
    const PickerRow want[8] = {PickerRow::None,        PickerRow::Downloading,
                               PickerRow::None,        PickerRow::Downloading,
                               PickerRow::GetModel,    PickerRow::Downloading,
                               PickerRow::Ready,       PickerRow::Downloading};
    bool all = true;
    for (int i = 0; i < 8; i++)
        all = all && gui::mask_picker_row(i & 4, i & 2, i & 1) == want[i];
    check(all, "picker row: the dataset screen's branch order, all eight inputs");
}

// ---------------------------------------------------------------------------
// SAM assist: a stable canvas, and a click mapped by what was shown
// ---------------------------------------------------------------------------

// Within a mode at one width, a strip line that goes away keeps its space; a
// change of mode or width is deliberate and starts over.
void test_strip_reserve() {
    mk::StripReserve r;
    r.update(206.0f, 3, 1600.0f);
    const float busy = r.update(228.0f, 3, 1600.0f);
    const float done = r.update(206.0f, 3, 1600.0f);
    check(busy == 228.0f && done == 228.0f,
          "strip reserve: a transient line leaving keeps the canvas where it was");
    check(r.update(110.0f, 0, 1600.0f) == 110.0f, "strip reserve: a mode change starts over");
    check(r.update(90.0f, 0, 900.0f) == 90.0f, "strip reserve: a width change starts over");
}

// A click is mapped through the layout the last drawn frame used, never the one
// this frame computed; and a newly arrived document has no layout to map by.
void test_click_maps_by_shown_layout() {
    Fixture f = make_dataset("shown_layout", 64, 32, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "shown layout: open: " + err);
    settle(s);
    float fx = -1.0f, fy = -1.0f;
    check(!s.shown_to_frame(10.0f, 10.0f, fx, fy), "shown layout: nothing drawn, no click");
    mk::View v;
    const mk::Mapping seen = mk::mapping(v, 64, 32, 640.0f, 320.0f);
    const mk::Mapping now = mk::mapping(v, 64, 32, 640.0f, 298.0f);
    s.note_shown(seen, 100.0f, 50.0f);
    float want_x = 0.0f, want_y = 0.0f, other_x = 0.0f, other_y = 0.0f;
    s.path_space(seen).to_frame(40.0f, 30.0f, want_x, want_y);
    s.path_space(now).to_frame(40.0f, 30.0f, other_x, other_y);
    check(std::fabs(want_x - other_x) > 0.5f,
          "shown layout: the two layouts send the point to different frame pixels");
    check(s.shown_to_frame(140.0f, 80.0f, fx, fy) && fx == want_x && fy == want_y,
          "shown layout: a click lands where the drawn picture had it");
    s.revert_open_frame();
    settle(s);
    check(!s.shown_to_frame(140.0f, 80.0f, fx, fy),
          "shown layout: a reloaded document forgets the old layout");
}

// Side by side draws one view twice: the same picture point in either pane
// must reach the same frame pixel, and a click in the gap reaches none.
void test_click_maps_by_clicked_pane() {
    Fixture f = make_dataset("shown_panes", 64, 32, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "shown panes: open: " + err);
    settle(s);
    mk::View v;
    const float pane_w = 300.0f, gap = 6.0f;
    const mk::Mapping m = mk::mapping(v, 64, 32, pane_w, 150.0f);
    float wx = 0.0f, wy = 0.0f, gx = 0.0f, gy = 0.0f;
    s.path_space(m).to_frame(40.0f, 30.0f, wx, wy);
    // Where a right-pane click goes through the LEFT pane's origin: the defect.
    s.path_space(m).to_frame(40.0f + pane_w + gap, 30.0f, gx, gy);
    check(std::fabs(gx - wx) > 1.0f,
          "shown panes: fixture: the left origin sends a right-pane click elsewhere");
    s.note_shown(m, 100.0f, 50.0f, 2, pane_w, gap);
    float lx = -1.0f, ly = -1.0f, rx = -1.0f, ry = -1.0f;
    check(s.shown_to_frame(140.0f, 80.0f, lx, ly) && lx == wx && ly == wy,
          "shown panes: a left-pane click lands where the left pane had it");
    check(s.shown_to_frame(140.0f + pane_w + gap, 80.0f, rx, ry) && rx == wx && ry == wy,
          "shown panes: a right-pane click lands on the same frame pixel");
    check(!s.shown_to_frame(100.0f + pane_w + 0.5f * gap, 80.0f, rx, ry),
          "shown panes: a click in the gap is no click");
    s.note_shown(m, 100.0f, 50.0f);
    check(s.shown_to_frame(140.0f, 80.0f, lx, ly) && lx == wx && ly == wy,
          "shown panes: one pane maps as it always did");
    s.close();
}

// The pane under a canvas x: each pane_w wide, gap apart; none in a gap or past the end.
void test_pane_at() {
    const float w = 300.0f, g = 6.0f;
    check(mk::pane_at(10.0f, 2, w, g) == 0 && mk::pane_at(299.9f, 2, w, g) == 0,
          "pane at: inside the left pane");
    check(mk::pane_at(300.0f, 2, w, g) == -1 && mk::pane_at(303.0f, 2, w, g) == -1,
          "pane at: the gap, from the pane's right edge, is no pane");
    check(mk::pane_at(306.0f, 2, w, g) == 1 && mk::pane_at(605.9f, 2, w, g) == 1,
          "pane at: inside the right pane");
    check(mk::pane_at(606.0f, 2, w, g) == -1 && mk::pane_at(-0.5f, 2, w, g) == -1,
          "pane at: off either end is no pane");
    check(mk::pane_at(450.0f, 1, 600.0f, 0.0f) == 0, "pane at: one pane is pane 0");
    check(mk::pane_left(1, w, g) == 306.0f && mk::pane_left(0, w, g) == 0.0f,
          "pane at: the right pane starts a pane and a gap in");
}

// A held button stays with the pane it pressed in; anything else maps through
// the pane under the pointer, so a click-placed point lands where it was aimed.
void test_bind_pane() {
    mk::MaskSession s;
    check(s.bind_pane(0, true, true, 2) == 0, "bind pane: a press takes the pane under it");
    check(s.bind_pane(1, false, true, 2) == 0,
          "bind pane: a drag into the other pane stays in the pane it pressed in");
    check(s.bind_pane(-1, false, true, 2) == 0, "bind pane: a drag over the gap stays too");
    check(s.bind_pane(1, false, false, 2) == 0,
          "bind pane: the release frame still belongs to the pressed pane");
    check(s.bind_pane(1, false, false, 2) == 1,
          "bind pane: released, the hover maps through the pane under it");
    check(s.bind_pane(1, true, true, 2) == 1,
          "bind pane: a click placed in the other pane maps through that pane");
    check(s.bind_pane(0, false, true, 1) == 0,
          "bind pane: a held pane that left the layout lets go");
    s.bind_pane(0, false, false, 1);
    check(s.bind_pane(-1, false, false, 2) == 0, "bind pane: off every pane, pane 0");
}

// The pen across side by side, fed as MaskPanel.cpp feeds it: an anchor on the
// photo pane and the next on the mask pane close into one polygon in pane pixels.
void test_pen_across_panes() {
    Fixture f = make_dataset("pen_panes", 64, 32, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "pen panes: open: " + err);
    settle(s);
    const float w = 300.0f, g = 6.0f;
    const mk::Mapping m = mk::mapping(mk::View{}, 64, 32, w, 150.0f);
    mk::PathTool t;
    t.set_space(s.path_space(m));
    std::vector<float> out, an, co, li;
    bool consumed = false;
    // cx is canvas px; pressed is this frame's left press, down the button held.
    auto feed = [&](float cx, float y, bool pressed, bool down) {
        const int p = s.bind_pane(mk::pane_at(cx, 2, w, g), pressed, down, 2);
        gui::ViewportInput in = at(cx - mk::pane_left(p, w, g), y);
        in.clicked = pressed;
        in.down = down;
        in.released = !down && !pressed;
        return t.update(in, out, consumed);
    };
    feed(40.0f, 30.0f, true, true);                 // photo pane
    feed(40.0f, 30.0f, false, false);
    feed(306.0f + 200.0f, 30.0f, false, false);     // hovering the mask pane
    t.overlay(an, co, li);
    check(!li.empty() && std::fabs(li[li.size() - 2] - 200.0f) < 1e-3f,
          "pen panes: the live segment follows the cursor into the mask pane");
    feed(306.0f + 200.0f, 30.0f, true, true);       // mask pane
    feed(306.0f + 200.0f, 30.0f, false, false);
    feed(306.0f + 120.0f, 120.0f, true, true);
    feed(306.0f + 120.0f, 120.0f, false, false);
    t.overlay(an, co, li);
    check(same_points(an, {40, 30, 200, 30, 120, 120}, 1e-3f),
          "pen panes: an anchor on either pane lands where that pane shows it");
    check(feed(306.0f + 41.0f, 31.0f, true, true), "pen panes: the first anchor, clicked on the mask pane, closes");
    check(same_points(out, {40, 30, 200, 30, 120, 120}, 1e-3f),
          "pen panes: the polygon is all in one pane's pixels");
    s.close();
}

// V and the radios wait for a shape mid-stroke: its points are pane pixels, and
// the pane width is part of the mapping. The pen keeps frame pixels, so it may.
void test_switch_view() {
    using V = mk::ViewMode;
    const mk::View v;
    const mk::Mapping one = mk::mapping(v, 64, 32, 600.0f, 150.0f);
    const mk::Mapping two = mk::mapping(v, 64, 32, 297.0f, 150.0f);
    check(std::fabs(one.to_mask_x(40.0f) - two.to_mask_x(40.0f)) > 1.0f,
          "switch view: fixture: one pane pixel is another mask pixel in the other layout");
    check(mk::switch_view(V::Overlay, V::SideBySide, false) == V::SideBySide,
          "switch view: idle, the view changes");
    check(mk::switch_view(V::Overlay, V::SideBySide, true) == V::Overlay,
          "switch view: a shape mid-stroke keeps one pane");
    check(mk::switch_view(V::SideBySide, V::MaskOnly, true) == V::SideBySide,
          "switch view: a shape mid-stroke keeps two panes");
    check(mk::switch_view(V::Overlay, V::MaskOnly, true) == V::Overlay,
          "switch view: a shape mid-stroke keeps even a same-width view");
}

// The pen may switch views mid-path: its anchors are frame pixels, so a new
// pane width re-maps them and the path closes over the same frame pixels.
void test_pen_survives_view_switch() {
    Fixture f = make_dataset("pen_view", 64, 32, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "pen view: open: " + err);
    settle(s);
    const mk::View v;
    const mk::Mapping one = mk::mapping(v, 64, 32, 600.0f, 150.0f);
    const mk::Mapping two = mk::mapping(v, 64, 32, 297.0f, 150.0f);
    mk::PathTool t;
    t.set_space(s.path_space(one));
    std::vector<float> out;
    bool consumed = false;
    const float pts[3][2] = {{250.0f, 40.0f}, {350.0f, 40.0f}, {300.0f, 110.0f}};
    float want[6];
    for (int i = 0; i < 3; i++) {
        t.update(click_at(pts[i][0], pts[i][1]), out, consumed);
        s.path_space(one).to_frame(pts[i][0], pts[i][1], want[2 * i], want[2 * i + 1]);
    }
    t.set_space(s.path_space(two));
    check(t.commit_pending(out) && out.size() == 6, "pen view: the path closes after the switch");
    bool same = out.size() == 6;
    for (int i = 0; same && i < 3; i++) {
        float fx = 0.0f, fy = 0.0f;
        s.path_space(two).to_frame(out[2 * i], out[2 * i + 1], fx, fy);
        same = std::fabs(fx - want[2 * i]) < 1e-2f && std::fabs(fy - want[2 * i + 1]) < 1e-2f;
    }
    check(same, "pen view: the closed polygon covers the frame pixels it was drawn over");
    check(std::fabs(out[0] - pts[0][0]) > 1.0f,
          "pen view: fixture: the new layout moved the anchor in pane pixels");
    s.close();
}

// Every peek in every view: side by side shows photo and mask bare, and a peek
// turns exactly one pane into the overlay. One pane ignores the pane index.
void test_pane_style_for() {
    using S = mk::Style;
    using P = mk::Peek;
    using V = mk::ViewMode;
    struct Row { V v; P p; S left, right; const char* what; };
    const Row rows[] = {
        {V::Overlay, P::None, S::Overlay, S::Overlay, "overlay, no peek"},
        {V::Overlay, P::Photo, S::Photo, S::Photo, "overlay, Tab"},
        {V::Overlay, P::Mask, S::MaskOnly, S::MaskOnly, "overlay, Shift+Tab"},
        {V::MaskOnly, P::None, S::MaskOnly, S::MaskOnly, "mask only, no peek"},
        {V::MaskOnly, P::Photo, S::Photo, S::Photo, "mask only, Tab"},
        {V::MaskOnly, P::Mask, S::MaskOnly, S::MaskOnly, "mask only, Shift+Tab"},
        {V::SideBySide, P::None, S::Photo, S::MaskOnly, "side by side, no peek"},
        {V::SideBySide, P::Photo, S::Photo, S::Overlay, "side by side, Tab"},
        {V::SideBySide, P::Mask, S::Overlay, S::MaskOnly, "side by side, Shift+Tab"},
    };
    for (const Row& r : rows) {
        check(mk::pane_style_for(r.p, r.v, 0) == r.left,
              std::string("pane style: ") + r.what + ", left pane");
        check(mk::pane_style_for(r.p, r.v, 1) == r.right,
              std::string("pane style: ") + r.what + ", right pane");
    }
}

// A cached pane window is reused only while it is the same window in the same
// style. The right pane is forgotten off screen: upload_rect keeps only panes
// on screen current, so a stroke painted meanwhile would never reach it.
void test_plan_derive() {
    using S = mk::Style;
    using P = mk::Peek;
    using V = mk::ViewMode;
    mk::Window a, b;
    a.r = mk::Rect{0, 0, 32, 32};
    b.r = mk::Rect{16, 0, 48, 32};
    a.tw = a.th = b.tw = b.th = 32;
    check(!mk::same_window(a, b), "plan derive: fixture: the two windows differ");
    mk::Window w0, w1;
    S s0 = S::Overlay, s1 = S::MaskOnly;
    auto run = [&](bool dirty, const mk::Window& want, V v, P p) {
        return mk::plan_derive(dirty, want, v, p, w0, s0, w1, s1);
    };
    mk::PaneDerive d = run(true, a, V::Overlay, P::None);
    check(d.left && !d.right && mk::same_window(w0, a),
          "plan derive: dirty derives the one pane shown");
    d = run(false, a, V::Overlay, P::None);
    check(!d.left && !d.right, "plan derive: nothing changed, nothing derived");
    d = run(false, a, V::Overlay, P::Photo);
    check(d.left && s0 == S::Photo, "plan derive: a peek re-derives in the new style");
    d = run(false, a, V::Overlay, P::None);
    check(d.left && s0 == S::Overlay, "plan derive: releasing the peek re-derives");
    d = run(false, b, V::Overlay, P::None);
    check(d.left && mk::same_window(w0, b), "plan derive: a moved window re-derives");
    d = run(false, b, V::SideBySide, P::None);
    check(d.left && d.right && s0 == S::Photo && s1 == S::MaskOnly && mk::same_window(w1, b),
          "plan derive: side by side derives both panes, photo and mask");
    d = run(false, b, V::SideBySide, P::None);
    check(!d.left && !d.right, "plan derive: side by side, nothing changed, nothing derived");
    d = run(false, b, V::SideBySide, P::Photo);
    check(!d.left && d.right && s1 == S::Overlay,
          "plan derive: side by side, Tab re-derives only the mask pane");
    run(false, b, V::SideBySide, P::None);
    d = run(false, b, V::Overlay, P::None);
    check(d.left && !d.right, "plan derive: back to one pane, the right pane is not derived");
    d = run(false, b, V::SideBySide, P::None);
    check(d.right, "plan derive: the right pane comes back derived afresh, not stale");
    d = run(true, b, V::SideBySide, P::None);
    check(d.left && d.right, "plan derive: dirty derives both panes");
}

// A result paints in the mode its click asked for: plain drops, Ctrl keeps,
// Shift+Ctrl clears back to the base -- the paint grammar, on SAM's region.
void test_session_sam_paint_modes() {
    Fixture f = make_dataset("sam_paint_modes", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam paint: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    const int64_t base = s.doc()->kept(), all = (int64_t)64 * 48;
    check(base > 0 && base < all, "sam paint: the base keeps some pixels and drops others");
    mk::AddRegion g;
    g.w = 64;
    g.h = 48;
    g.mask.assign((size_t)all, 255);
    s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->kept() == 0, "sam paint: a plain click's result drops the object");
    s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, mk::Paint::Clear, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->kept() == base, "sam paint: a Shift+Ctrl result clears back to the base");
    s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, mk::Paint::ForceKeep, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->kept() == all, "sam paint: a Ctrl result keeps the object");
}

// A Clear reverts SAM's region only: a hand stroke beside it survives.
void test_session_sam_clear_is_local() {
    const int W = 64, H = 48;
    Fixture f = make_dataset("sam_clear_local", W, H, {"a"}, /*with_masks=*/false);
    write_png_gray(f.masks / "a.png", W, H, std::vector<uint8_t>((size_t)W * H, 255));
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam clear: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    const int64_t all = (int64_t)W * H;
    mk::Mapping m;
    m.scale = 1.0f;
    gui::ShapeStroke hand;
    hand.kind = gui::ShapeKind::Box;
    hand.pts = std::vector<float>{0.0f, 0.0f, 10.0f, 10.0f};
    s.commit_stroke(hand, mk::Paint::ForceDrop, m);
    const int64_t after_hand = s.doc()->kept();
    mk::AddRegion g;
    g.w = W;
    g.h = H;
    g.mask.assign((size_t)all, 0);
    for (int y = 20; y < 30; y++)
        for (int x = 30; x < 40; x++) g.mask[(size_t)y * W + x] = 255;
    s.sam().post_result(s.sam_frame_stamp(), {g}, W, H, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(after_hand < all && s.doc()->kept() == after_hand - 100,
          "sam clear: the hand stroke and SAM's drop both landed");
    s.sam().post_result(s.sam_frame_stamp(), {g}, W, H, mk::Paint::Clear, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->kept() == after_hand, "sam clear: a Clear reverts SAM's region, the stroke stays");
}

// ---------------------------------------------------------------------------
// SAM assist: the drop margin (core/MaskMargin.h, sam::Masker's rule)
// ---------------------------------------------------------------------------

void test_margin_radius() {
    check(margin::radius_px(0, 0, 400, 400, 0.05f) == 10,
          "margin radius: 5% of a 400 px box is the odd kernel 21, radius 10");
    check(margin::radius_px(0, 0, 200, 50, 0.3f) == 18,
          "margin radius: the mean side, not the long one (kernel 37)");
    check(margin::radius_px(0, 0, 4, 4, 0.05f) == 1, "margin radius: the floor of 3 keeps a tiny box moving");
    check(margin::radius_px(0, 0, 400, 400, 0.0f) == 0, "margin radius: ratio 0 is exactly none");
    check(margin::radius_px(0, 0, 400, 400, -0.05f) == -10, "margin radius: signed, as Masker takes it");
}

// A drop grows by its radius on every side; the grown count is the margin's own.
void test_add_stencil_drop_margin() {
    const int W = 64, H = 48;
    std::vector<mk::AddRegion> r(1);
    r[0].w = W;
    r[0].h = H;
    r[0].mask.assign((size_t)W * H, 0);
    for (int y = 14; y < 34; y++)
        for (int x = 20; x < 40; x++) r[0].mask[(size_t)y * W + x] = 255;
    // With no model box the extent stands in, inclusive as sam::mask_bounding_box
    // measures it: 19 x 19 here, radius 2, where the exclusive 20 x 20 gives 3.
    const int rad = margin::radius_px(20, 14, 39, 33, 0.3f);
    std::vector<uint8_t> hit((size_t)W * H, 0);
    margin::accumulate(r[0].mask.data(), W, H, rad, hit);
    int64_t want = 0;
    for (uint8_t v : hit) want += v;
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(rad == 2 && margin::radius_px(20, 14, 40, 34, 0.3f) == 3 &&
              mk::build_add_stencil(r, W, H, st, b, set, 0.3f),
          "drop margin: a 20 px square at 30% has radius 2 and builds");
    check(same_rect(b, mk::Rect{18, 12, 42, 36}) && set == want && set > 400,
          "drop margin: the square grows 2 px on every side, as Masker's box sizes it");
}

// Every mode takes the margin; never signed, so no trim can reach the editor.
void test_drop_margin_modes() {
    check(mk::add_margin(0.2f) == 0.2f, "add margin: the ratio, whatever the mode");
    check(mk::add_margin(-0.1f) == 0.0f, "add margin: a negative ratio never trims");
    Fixture f = make_dataset("sam_margin_modes", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "drop margin: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    mk::AddRegion g;
    g.w = 64;
    g.h = 48;
    g.mask.assign((size_t)64 * 48, 0);
    for (int y = 14; y < 34; y++)
        for (int x = 20; x < 40; x++) g.mask[(size_t)y * 64 + x] = 255;
    int64_t area[3];
    const mk::Paint modes[3] = {mk::Paint::ForceDrop, mk::Paint::ForceKeep, mk::Paint::Clear};
    for (int i = 0; i < 3; i++) {
        s.sam().post_result(s.sam_frame_stamp(), {g}, 64, 48, modes[i], 0.3f, 0.9f, 1.0);
        s.sam_pump();
        area[i] = s.sam_last_area();
    }
    check(area[0] > 400 && area[1] == area[0] && area[2] == area[0],
          "add margin: a drop, a keep and a clear all grow alike");
}

// The editor's margin is its own MaskSettings, never the dataset screen's.
void test_editor_margin_is_its_own() {
    gui::MaskSettings dataset;
    mk::MaskSession s;
    check(s.sam_margin() < 0.0f, "editor margin: nothing before SAM is used");
    s.sam_prompt().dilate_ratio = 0.2f;
    check(dataset.dilate_ratio == 0.05f && s.sam_margin() == 0.2f,
          "editor margin: setting the editor's leaves a dataset's at its 5% default");
    mk::MaskSession fresh;
    check(fresh.sam_prompt().dilate_ratio == 0.05f, "editor margin: a new editor starts at 5%");
}

// A point off the frame never reaches the model: no job, and no error either.
void test_prompt_point_off_frame() {
    Fixture f = make_dataset("sam_off_frame", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "off frame: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    const float bad[][2] = {{-0.5f, 10.0f}, {10.0f, -1.0f}, {64.0f, 10.0f}, {10.0f, 48.0f}};
    bool quiet = true;
    for (const auto& p : bad) quiet = quiet && !s.sam_prompt_point(p[0], p[1], mk::Paint::ForceDrop);
    check(quiet && s.sam_error().empty(), "off frame: four points outside are refused silently");
    s.sam_prompt_point(63.5f, 47.5f, mk::Paint::ForceDrop);
    check(!s.sam_error().empty(), "off frame: the last pixel inside still reaches the job (the stub refuses it)");
}

// ---------------------------------------------------------------------------
// SAM assist: a re-prompt of the same object replaces its add
// ---------------------------------------------------------------------------

// One job's result for the open frame, built as the job builds it (prepare()).
mk::SamResult sam_result(mk::MaskSession& s, mk::AddRegion g, mk::Paint mode, float margin) {
    s.sam().post_result(s.sam_frame_stamp(), {std::move(g)}, s.doc()->width(),
                        s.doc()->height(), mode, margin, 0.9f, 1.0);
    mk::SamResult r;
    s.sam().take_result(r);
    return r;
}

mk::Rect sam_add(mk::MaskSession& s, const mk::AddRegion& g, int object,
                 mk::Paint mode = mk::Paint::ForceDrop, float margin = 0.0f) {
    return s.apply_sam_add(sam_result(s, g, mode, margin), object);
}

// A re-prompt of the object on top of this frame's history replaces its add;
// an edit in between, another object's add, or a frame change makes it add.
void test_session_sam_add_replaces() {
    Fixture f = make_dataset("sam_replace", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam replace: open: " + err);
    settle(s);
    const int h0 = s.doc()->history_size();
    const std::vector<uint8_t> comp0 = s.doc()->composite(), drop0 = s.doc()->drop();
    const mk::AddRegion big = disc_region(64, 48, 20.0f, 20.0f, 10.0f);
    // Not a subset of `big`: an additive re-prompt would change pixels and
    // record a step of its own, so every count below separates the two rules.
    const mk::AddRegion small_add = disc_region(64, 48, 27.0f, 20.0f, 5.0f);
    auto drop_is = [&](const std::vector<uint8_t>& plane) {   // drop0 | plane, exactly
        const std::vector<uint8_t>& d = s.doc()->drop();
        for (size_t i = 0; i < d.size(); i++)
            if ((d[i] != 0) != (drop0[i] != 0 || plane[i] != 0)) return false;
        return true;
    };
    std::vector<uint8_t> both = big.mask;
    for (size_t i = 0; i < both.size(); i++) both[i] |= small_add.mask[i];
    check(!drop_is(big.mask) && !drop_is(small_add.mask) && both != big.mask && both != small_add.mask,
          "sam replace: neither disc is already dropped, and neither holds the other");
    sam_add(s, big, 0);
    const mk::Rect shrunk = sam_add(s, small_add, 0);
    check(s.doc()->history_size() == h0 + 1, "sam replace: refining the top object is one step");
    const mk::Rect was = extent(big.mask, 64, 48);
    check(shrunk.x0 <= was.x0 && shrunk.y0 <= was.y0 && shrunk.x1 >= was.x1 && shrunk.y1 >= was.y1,
          "sam replace: the redrawn rect covers what the first add painted, not only the refinement");
    check(drop_is(small_add.mask), "sam replace: the refinement replaced the first add, pixel for pixel");
    s.doc()->undo();
    check(s.doc()->composite() == comp0, "sam replace: one undo removes the object's add entirely");

    sam_add(s, big, 0);
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 50, 5, 60, 15), mk::Rect{50, 5, 60, 15});
    const int h1 = s.doc()->history_size();
    sam_add(s, small_add, 0);
    check(s.doc()->history_size() == h1 + 1 && s.doc()->keep()[(size_t)10 * 64 + 55] == 255,
          "sam replace: an edit in between makes a re-prompt add, and the edit survives");
    sam_add(s, disc_region(64, 48, 44.0f, 30.0f, 4.0f), 1);
    const int h2 = s.doc()->history_size();
    sam_add(s, disc_region(64, 48, 47.0f, 30.0f, 3.0f), 0);
    check(s.doc()->history_size() == h2 + 1, "sam replace: another object's add on top means an add");
    const int h3 = s.doc()->history_size();
    sam_add(s, disc_region(64, 48, 8.0f, 40.0f, 3.0f), -1);
    sam_add(s, disc_region(64, 48, 11.0f, 40.0f, 3.0f), -1);
    check(s.doc()->history_size() == h3 + 2, "sam replace: a text prompt (-1) never replaces");

    // A fresh session so the stamp's revision is 1, which a reload plus one
    // edit reproduces exactly: only the document's own stamp tells them apart.
    mk::MaskSession r;
    check(r.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam replace: reopen: " + err);
    settle(r);
    sam_add(r, big, 0);
    check(r.doc()->revision() == 1, "sam replace: the first add is revision 1");
    r.go_to(1);
    settle(r);
    r.go_to(0);
    settle(r);
    r.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 50, 30, 60, 40), mk::Rect{50, 30, 60, 40});
    const int h4 = r.doc()->history_size();
    check(r.doc()->revision() == 1, "sam replace: the reloaded frame's edit is revision 1 too");
    sam_add(r, small_add, 0);
    check(r.doc()->history_size() == h4 + 1 && r.doc()->keep()[(size_t)35 * 64 + 55] == 255,
          "sam replace: a frame change forgets the stamp, so a reload cannot collide with it");
}

// A "not this" refines the object's add in that add's own mode; with no add of
// this object on top it takes the modifiers like any other click.
void test_session_sam_negative_mode() {
    Fixture f = make_dataset("sam_negative_mode", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam not-this: open: " + err);
    settle(s);
    const mk::Paint fb = mk::Paint::ForceDrop;
    check(s.sam_refine_mode(fb) == fb, "sam not-this: nothing added yet takes the modifiers");
    sam_add(s, disc_region(64, 48, 20.0f, 20.0f, 6.0f), 0, mk::Paint::ForceKeep);
    check(s.sam_refine_mode(fb) == mk::Paint::ForceKeep,
          "sam not-this: refining a kept object keeps keeping it");
    s.sam_prompt().current_object = s.sam_prompt().object_count++;
    check(s.sam_refine_mode(fb) == fb, "sam not-this: another current object takes the modifiers");
    s.sam_prompt().current_object = 0;
    s.doc()->paint(mk::Paint::ForceDrop, box_stencil(64, 48, 50, 5, 60, 15), mk::Rect{50, 5, 60, 15});
    check(s.sam_refine_mode(fb) == fb, "sam not-this: an edit in between takes the modifiers");
}

// What a click sends: the object's clicks on this frame, then the new one,
// each with its own label -- "not this" included.
void test_mask_sam_prompt_points() {
    mk::MaskSam sam;
    sam.add_click(3, "cam0", 10.0f, 20.0f);
    const std::vector<mk::SamPoint> neg =
        sam.prompt_points(3, "cam0", mk::SamPoint{30.0f, 40.0f, false});
    check(neg.size() == 2 && neg[0].positive && neg[0].x == 10.0f && !neg[1].positive &&
              neg[1].x == 30.0f && neg[1].y == 40.0f,
          "sam prompt: a right click is sent after the object's clicks, as \"not this\"");
    sam.add_click(3, "cam0", 30.0f, 40.0f, /*positive=*/false);
    const std::vector<mk::SamPoint> pos =
        sam.prompt_points(3, "cam0", mk::SamPoint{50.0f, 60.0f, true});
    check(pos.size() == 3 && pos[0].positive && !pos[1].positive && pos[2].positive,
          "sam prompt: a stored \"not this\" rides along with the next click");
    check(sam.prompt_points(4, "cam0", mk::SamPoint{1.0f, 1.0f, true}).size() == 1,
          "sam prompt: another frame's clicks are not sent");
}

// A click is recorded only once its job starts: the stub refuses every start,
// so a click here must leave no dot that drove nothing.
void test_session_refused_click_leaves_no_dot() {
    Fixture f = make_dataset("sam_refused_click", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam refused: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    const bool started = s.sam_prompt_point(10.0f, 10.0f, mk::Paint::ForceDrop);
    const bool started_neg = s.sam_prompt_point(12.0f, 12.0f, mk::Paint::ForceDrop, false);
    check(!started && !started_neg && !s.sam_error().empty(),
          "sam refused: the stub refused both starts, with a reason");
    check(s.sam_click_count() == 0, "sam refused: a refused click is not recorded");
}

// A detection held as its cropped set pixels rebuilds to the same plane.
void test_held_region_roundtrip() {
    const mk::AddRegion g = disc_region(64, 48, 20.0f, 30.0f, 6.0f);
    const mk::HeldRegion h = mk::hold_region(g);
    check(h.w == 64 && h.h == 48 && same_rect(h.box, extent(g.mask, 64, 48)) &&
              h.mask.size() == (size_t)h.box.w() * h.box.h() && h.mask.size() < g.mask.size(),
          "held region: cropped to the set pixels' extent");
    const mk::AddRegion back = mk::expand_region(h);
    check(back.w == 64 && back.h == 48 && back.mask == g.mask,
          "held region: expanding it gives back the plane, byte for byte");
    mk::AddRegion boxed = g;
    boxed.box = mk::RegionBox{2.0f, 3.0f, 50.0f, 40.0f, true};
    const mk::AddRegion rebox = mk::expand_region(mk::hold_region(boxed));
    check(rebox.box.set && rebox.box.x0 == 2.0f && rebox.box.y0 == 3.0f &&
              rebox.box.x1 == 50.0f && rebox.box.y1 == 40.0f,
          "held region: the model's box survives the hold, so a re-apply sizes from it too");
    const mk::HeldRegion none = mk::hold_region(disc_region(64, 48, 20.0f, 30.0f, 0.0f));
    check(none.box.empty() && none.mask.empty(), "held region: nothing set holds nothing");
}

// A slider release, as the panel reports it: the margin job starts on one
// frame (the stub builds it inline) and lands on the next.
void reapply(mk::MaskSession& s) {
    s.sam_margin_changed();
    s.sam_pump();
    s.sam_pump();
}

// Moving the margin re-applies it to the object just clicked, in place, while
// that add is still on top; after any other edit it waits for the next click.
void test_session_sam_margin_reapply() {
    Fixture f = make_dataset("sam_margin_reapply", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "margin reapply: open: " + err);
    settle(s);
    const mk::AddRegion g = disc_region(64, 48, 30.0f, 24.0f, 6.0f);
    check(!s.sam_margin_reapplies(), "margin reapply: nothing to re-apply before an add");
    s.sam_prompt().dilate_ratio = 0.0f;
    sam_add(s, g, 0);
    const int h0 = s.doc()->history_size();
    const int64_t tight = s.sam_last_area();
    s.sam_prompt().dilate_ratio = 0.4f;
    check(s.sam_margin_reapplies(), "margin reapply: the add on top can be re-applied");
    reapply(s);
    std::vector<mk::AddRegion> want{g};
    gui::Stencil st;
    mk::Rect b;
    int64_t grown = 0;
    mk::build_add_stencil(want, 64, 48, st, b, grown, 0.4f);
    check(grown > tight && s.doc()->history_size() == h0 && s.sam_last_area() == grown,
          "margin reapply: the add is replaced in place at the new margin");
    int64_t dropped_in = 0;
    for (size_t i = 0; i < st.in.size(); i++)
        dropped_in += (st.in[i] && s.doc()->drop()[i]) ? 1 : 0;
    check(dropped_in == grown, "margin reapply: every pixel of the grown outline is dropped");
    s.sam_prompt().dilate_ratio = 0.0f;
    reapply(s);
    check(s.sam_last_area() == tight && s.doc()->history_size() == h0,
          "margin reapply: moving it back shrinks the add again");
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    s.sam_prompt().dilate_ratio = 0.4f;
    const int h1 = s.doc()->history_size();
    check(!s.sam_margin_reapplies(), "margin reapply: an edit in between ends it");
    reapply(s);
    check(s.doc()->history_size() == h1 && s.doc()->keep()[0] == 255,
          "margin reapply: ... and the edit survives");
    check(s.sam_held_bytes() == 0, "margin reapply: the next frame lets go of a detection that cannot re-apply");
    check(sam_result(s, g, mk::Paint::ForceKeep, 0.4f).held.size() == 1 &&
              sam_result(s, g, mk::Paint::Clear, 0.4f).held.size() == 1 &&
              sam_result(s, g, mk::Paint::ForceDrop, 0.4f).held.size() == 1,
          "margin reapply: a job holds its detection in every mode");
    // Subtract: a keep over a drop made with a margin must take the margin too,
    // or the rim the drop grew stays dropped around the object.
    s.sam_prompt().dilate_ratio = 0.4f;
    mk::SamResult drop = sam_result(s, g, mk::Paint::ForceDrop, 0.4f);
    s.apply_sam_add(std::move(drop), 1);
    mk::SamResult keep = sam_result(s, g, mk::Paint::ForceKeep, 0.4f);
    s.apply_sam_add(std::move(keep), 2);
    int64_t rim = 0;
    for (size_t i = 0; i < st.in.size(); i++) rim += (st.in[i] && s.doc()->drop()[i]) ? 1 : 0;
    check(rim == 0, "margin: a keep with the same margin clears the whole grown drop, " +
                        std::to_string(rim) + " px left");
    check(s.sam_margin_reapplies(), "margin reapply: a keep on top can be re-applied");
    s.sam_prompt().dilate_ratio = 0.0f;
    reapply(s);
    int64_t left = 0;
    for (size_t i = 0; i < st.in.size(); i++) left += (st.in[i] && s.doc()->drop()[i]) ? 1 : 0;
    check(left > 0, "margin reapply: shrinking the keep brings the drop's rim back");
}

// Ctrl+Z in SAM mode takes back a click: an object's only click goes with its
// add, and an add that is not on top leaves the clicks to the plain undo.
void test_session_sam_undo_click() {
    Fixture f = make_dataset("sam_undo_click", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "undo click: open: " + err);
    settle(s);
    const mk::AddRegion g = disc_region(64, 48, 30.0f, 24.0f, 6.0f);
    s.sam().add_click(0, s.frames()[0].camera, 30.0f, 24.0f, true);
    sam_add(s, g, 0);
    mk::Rect r;
    check(s.sam_undo_click(r) && s.sam_click_count() == 0 && !s.doc()->can_undo() &&
              s.doc()->can_redo(),
          "undo click: the only click goes, and its add with it");
    check(!s.sam_undo_click(r), "undo click: nothing of SAM's on top, so the plain undo decides");
    s.sam().add_click(0, s.frames()[0].camera, 30.0f, 24.0f, true);
    sam_add(s, g, 0);
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    check(!s.sam_undo_click(r) && s.sam_click_count() == 1,
          "undo click: after another edit the click stays for the plain undo");
}

// A re-prompt whose mask lands on pixels already dropped records no step: the
// add it replaced is gone, and nothing of ours is on top to be replaced next.
void test_session_sam_noop_replace() {
    Fixture f = make_dataset("sam_noop_replace", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam no-op: open: " + err);
    settle(s);
    s.doc()->paint(mk::Paint::ForceDrop, box_stencil(64, 48, 40, 10, 60, 40), mk::Rect{40, 10, 60, 40});
    const int h0 = s.doc()->history_size();
    sam_add(s, disc_region(64, 48, 15.0f, 20.0f, 6.0f), 0);
    sam_add(s, disc_region(64, 48, 50.0f, 25.0f, 4.0f), 0);
    check(s.doc()->drop()[(size_t)20 * 64 + 15] == 0,
          "sam no-op: the replaced add is undone and the refinement changed nothing");
    check(!s.doc()->can_redo(), "sam no-op: the replaced add does not linger on the redo stack");
    sam_add(s, disc_region(64, 48, 15.0f, 36.0f, 5.0f), 0);
    check(s.doc()->history_size() == h0 + 1 && s.doc()->drop()[(size_t)12 * 64 + 42] == 255,
          "sam no-op: the next re-prompt adds, and the hand edit beneath survives");
}

// Clear or Clear all makes the object numbers mean new things: a click on the
// same number afterwards is a new object, and must add, never replace.
void test_session_sam_clear_forgets_add() {
    Fixture f = make_dataset("sam_clear_forgets", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam clear list: open: " + err);
    settle(s);
    const std::vector<uint8_t> drop0 = s.doc()->drop();
    const size_t monopod = (size_t)20 * 64 + 12, chair = (size_t)24 * 64 + 48;
    check(drop0[monopod] == 0 && drop0[chair] == 0, "sam clear list: neither spot starts dropped");
    const int h0 = s.doc()->history_size();
    sam_add(s, disc_region(64, 48, 12.0f, 20.0f, 5.0f), 0);
    s.sam_objects_edited();
    check(!s.sam_margin_reapplies() && s.sam_held_bytes() == 0,
          "sam clear list: nothing is left to re-apply the margin to");
    sam_add(s, disc_region(64, 48, 48.0f, 24.0f, 5.0f), 0);
    check(s.doc()->history_size() == h0 + 2, "sam clear list: after a clear the same number adds");
    s.doc()->undo();
    check(s.doc()->drop()[monopod] == 255 && s.doc()->drop()[chair] == 0,
          "sam clear list: undoing the new object brings the cleared one's add back");
}

// What sam_prompt_point records once a job starts, reached without a model:
// the object a result replaces, and each click with its own label.
void test_session_sam_prompt_bookkeeping() {
    Fixture f = make_dataset("sam_bookkeeping", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam bookkeeping: open: " + err);
    settle(s);
    const int h0 = s.doc()->history_size();
    const mk::AddRegion big = disc_region(64, 48, 20.0f, 20.0f, 10.0f);
    const mk::AddRegion small_add = disc_region(64, 48, 27.0f, 20.0f, 5.0f);
    s.sam_prompt_started(20.0f, 20.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {big}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    s.sam_prompt_started(14.0f, 20.0f, false);
    s.sam().post_result(s.sam_frame_stamp(), {small_add}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->history_size() == h0 + 1 && s.doc()->drop()[(size_t)20 * 64 + 12] == 0,
          "sam bookkeeping: a prompt's result replaces its object's add through sam_pump");
    const std::vector<mk::SamPoint> third =
        s.sam().prompt_points(0, "", mk::SamPoint{30.0f, 20.0f, true});
    check(third.size() == 3 && third[0].positive && !third[1].positive && third[2].positive,
          "sam bookkeeping: a third click sends this, not this, this");
    s.sam_prompt_started(30.0f, 20.0f, true);
    const std::vector<gui::MaskClick>& c = s.sam_prompt().clicks;
    check(c.size() == 3 && c[0].positive && !c[1].positive && c[2].positive && c[1].x == 14.0f &&
              c[0].object == 0 && c[0].frame == 0,
          "sam bookkeeping: the stored clicks keep their labels, object and frame");
}

// A slider release while a prompt is in flight: the prompt lands first, then
// the margin re-applies to it, so the picture ends at what the slider shows.
void test_session_sam_margin_after_prompt() {
    Fixture f = make_dataset("sam_margin_after_prompt", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "margin after prompt: open: " + err);
    settle(s);
    s.sam_prompt().dilate_ratio = 0.0f;
    s.sam_prompt_started(30.0f, 24.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 30.0f, 24.0f, 4.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    const int h0 = s.doc()->history_size();
    const mk::AddRegion g2 = disc_region(64, 48, 34.0f, 24.0f, 7.0f);
    s.sam_prompt_started(34.0f, 24.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {g2}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_prompt().dilate_ratio = 0.4f;
    reapply(s);
    std::vector<mk::AddRegion> want{g2};
    gui::Stencil st;
    mk::Rect b;
    int64_t grown = 0;
    mk::build_add_stencil(want, 64, 48, st, b, grown, 0.4f);
    check(s.doc()->history_size() == h0 && s.sam_last_area() == grown && s.sam_results() == 2,
          "margin after prompt: the prompt landed, then took the slider's margin");

    s.sam_prompt().dilate_ratio = 0.1f;
    s.sam_margin_changed();
    s.sam_pump();
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    const int h1 = s.doc()->history_size();
    const int dropped = s.sam_dropped();
    s.sam_pump();
    check(s.doc()->history_size() == h1 && s.doc()->keep()[0] == 255 &&
              s.sam_dropped() == dropped + 1 && s.sam_last_area() == grown,
          "margin after prompt: a margin that lands after another edit is dropped, not stacked");
}

// ---------------------------------------------------------------------------
// SAM assist: text prompts, the model's box, the exception chips
// ---------------------------------------------------------------------------

// A negative phrase's pixels leave the positive region they overlap, and only
// those; a region the veto does not reach is untouched.
void test_veto_regions() {
    std::vector<mk::AddRegion> pos{disc_region(64, 48, 20.0f, 20.0f, 8.0f),
                                   disc_region(64, 48, 44.0f, 24.0f, 6.0f)};
    const std::vector<mk::AddRegion> veto{disc_region(64, 48, 26.0f, 20.0f, 8.0f)};
    std::vector<uint8_t> want0 = pos[0].mask;
    for (size_t i = 0; i < want0.size(); i++)
        if (veto[0].mask[i]) want0[i] = 0;
    const std::vector<uint8_t> want1 = pos[1].mask;
    const size_t overlap = set_pixels(pos[0].mask) - set_pixels(want0);
    check(overlap > 0 && overlap < set_pixels(pos[0].mask),
          "veto: the fixture's veto overlaps part of region 0, not all of it");
    const int64_t cleared = mk::veto_regions(pos, veto);
    check(pos[0].mask == want0, "veto: region 0 loses exactly the vetoed pixels");
    check(pos[1].mask == want1, "veto: region 1, which the veto does not reach, is untouched");
    check(cleared == (int64_t)overlap, "veto: the count is the pixels cleared");
}

// The veto comes after the margin, as sam::compose_hit orders it: the grown rim
// may not cover what a negative phrase said to keep.
void test_add_stencil_veto_beats_margin() {
    const int W = 64, H = 48;
    const mk::AddRegion pos = disc_region(W, H, 24.0f, 24.0f, 8.0f);
    const mk::AddRegion veto = disc_region(W, H, 36.0f, 24.0f, 6.0f);
    const float ratio = 0.4f;
    const mk::Rect e = extent(pos.mask, W, H);
    const int rad = margin::radius_px((float)e.x0, (float)e.y0, (float)e.x1 - 1,
                                      (float)e.y1 - 1, ratio);
    std::vector<uint8_t> grown((size_t)W * H, 0), cut = pos.mask, regrown((size_t)W * H, 0);
    margin::accumulate(pos.mask.data(), W, H, rad, grown);
    for (size_t i = 0; i < cut.size(); i++)
        if (veto.mask[i]) cut[i] = 0;
    margin::accumulate(cut.data(), W, H, rad, regrown);
    std::vector<uint8_t> want((size_t)W * H, 0);
    int64_t n = 0, refilled = 0;
    for (size_t i = 0; i < want.size(); i++) {
        want[i] = grown[i] && !veto.mask[i] ? 255 : 0;
        n += want[i] ? 1 : 0;
        refilled += regrown[i] && veto.mask[i] ? 1 : 0;
    }
    check(rad > 0 && refilled > 0,
          "veto margin: vetoing first, the margin would grow back over the vetoed pixels");
    std::vector<mk::AddRegion> r{pos};
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, W, H, st, b, set, ratio, {veto}),
          "veto margin: the vetoed region still builds");
    check(st.in == want && set == n,
          "veto margin: every vetoed pixel stays clear of the grown outline");
}

// The margin is sized from the model's box when it has one: a text detection's
// regressed box is not its mask's extent, and the dataset screen uses the box.
void test_add_stencil_box_sizes_margin() {
    const int W = 96, H = 64;
    mk::AddRegion g = disc_region(W, H, 48.0f, 32.0f, 6.0f);
    const mk::Rect e = extent(g.mask, W, H);
    g.box = mk::RegionBox{20.0f, 8.0f, 76.0f, 56.0f, true};
    const float ratio = 0.3f;
    const int from_box = margin::radius_px(20.0f, 8.0f, 76.0f, 56.0f, ratio);
    const int from_extent = margin::radius_px((float)e.x0, (float)e.y0, (float)e.x1 - 1,
                                              (float)e.y1 - 1, ratio);
    check(from_box > from_extent,
          "box margin: the fixture's box and the mask's extent give different radii");
    std::vector<uint8_t> hit((size_t)W * H, 0);
    margin::accumulate(g.mask.data(), W, H, from_box, hit);
    std::vector<mk::AddRegion> r{g};
    gui::Stencil st;
    mk::Rect b;
    int64_t set = -1;
    check(mk::build_add_stencil(r, W, H, st, b, set, ratio), "box margin: builds");
    bool same = st.in.size() == hit.size();
    for (size_t i = 0; same && i < hit.size(); i++) same = (st.in[i] != 0) == (hit[i] != 0);
    check(same, "box margin: the radius comes from the model's box, not the mask's extent");
}

// A click on the object whose add is on top inherits that add's mode unless a
// modifier is held; with nothing of its on top the modifiers decide.
void test_session_sam_click_mode() {
    Fixture f = make_dataset("sam_click_mode", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "click mode: open: " + err);
    settle(s);
    using P = mk::Paint;
    check(s.sam_click_mode(false, false) == P::ForceDrop && s.sam_click_mode(false, true) == P::ForceKeep,
          "click mode: with nothing on top, plain drops and Ctrl keeps");
    sam_add(s, disc_region(64, 48, 20.0f, 20.0f, 6.0f), 0, P::ForceKeep);
    check(s.sam_click_mode(false, false) == P::ForceKeep,
          "click mode: a plain click on a kept object keeps keeping it");
    check(s.sam_click_mode(true, true) == P::Clear, "click mode: Shift+Ctrl on a kept object clears");
    sam_add(s, disc_region(64, 48, 24.0f, 20.0f, 6.0f), 0, P::ForceDrop);
    check(s.sam_click_mode(false, true) == P::ForceKeep,
          "click mode: Ctrl on a dropped object is an explicit keep");
    check(s.sam_click_mode(false, false) == P::ForceDrop,
          "click mode: a plain click on a dropped object drops");
}

// Undo then redo puts the same add back on top, so a refinement replaces it;
// an undo and a new edit in its place does not.
void test_session_sam_redo_then_refine() {
    Fixture f = make_dataset("sam_redo_refine", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "redo refine: open: " + err);
    settle(s);
    const int h0 = s.doc()->history_size();
    const std::vector<uint8_t> comp0 = s.doc()->composite();
    const mk::AddRegion big = disc_region(64, 48, 20.0f, 20.0f, 10.0f);
    const mk::AddRegion small_add = disc_region(64, 48, 27.0f, 20.0f, 5.0f);
    sam_add(s, big, 0);
    s.undo();
    s.redo();
    sam_add(s, small_add, 0);
    check(s.doc()->history_size() == h0 + 1, "redo refine: after undo and redo a refinement replaces");
    s.undo();
    check(s.doc()->composite() == comp0, "redo refine: one undo removes the object entirely");
    sam_add(s, big, 0);
    s.undo();
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 50, 5, 60, 15), mk::Rect{50, 5, 60, 15});
    const int h1 = s.doc()->history_size();
    sam_add(s, small_add, 0);
    check(s.doc()->history_size() == h1 + 1 && s.doc()->keep()[(size_t)10 * 64 + 55] == 255,
          "redo refine: an undo and a new edit in its place makes a re-prompt add");
}

// An undo alone takes the add off the top: a refinement then adds, and the
// hand edit beneath the undone add is never undone in its place.
void test_session_sam_undo_then_refine() {
    Fixture f = make_dataset("sam_undo_refine", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "undo refine: open: " + err);
    settle(s);
    const int h0 = s.doc()->history_size();
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 50, 5, 60, 15), mk::Rect{50, 5, 60, 15});
    sam_add(s, disc_region(64, 48, 20.0f, 20.0f, 10.0f), 0);
    s.undo();
    sam_add(s, disc_region(64, 48, 27.0f, 20.0f, 5.0f), 0);
    check(s.doc()->history_size() == h0 + 2 && s.doc()->keep()[(size_t)10 * 64 + 55] == 255,
          "undo refine: after an undo a refinement adds, and the hand edit survives");
}

// A fresh add that changes nothing leaves the redo stack alone: only a
// replacement's own undone add is dropped from it.
void test_session_sam_noop_fresh_keeps_redo() {
    Fixture f = make_dataset("sam_noop_fresh", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "noop fresh: open: " + err);
    settle(s);
    s.doc()->paint(mk::Paint::ForceDrop, box_stencil(64, 48, 40, 10, 60, 40), mk::Rect{40, 10, 60, 40});
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    s.undo();
    const uint64_t rev = s.doc()->revision();
    sam_add(s, disc_region(64, 48, 50.0f, 25.0f, 4.0f), 0);
    check(s.doc()->revision() == rev, "noop fresh: the add lands on dropped pixels and changes nothing");
    check(s.doc()->can_redo(), "noop fresh: a no-op fresh add keeps the undone hand stroke's redo");
}

// Shift alone is a held modifier: on a kept object it drops, not keeps.
void test_session_sam_click_mode_shift() {
    Fixture f = make_dataset("sam_click_shift", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "click shift: open: " + err);
    settle(s);
    sam_add(s, disc_region(64, 48, 20.0f, 20.0f, 6.0f), 0, mk::Paint::ForceKeep);
    check(s.sam_click_mode(false, false) == mk::Paint::ForceKeep,
          "click shift: the fixture's add is a keep on top");
    check(s.sam_click_mode(true, false) == mk::Paint::ForceDrop,
          "click shift: Shift alone on a kept object drops");
}

// At the history cap every add evicts the oldest step; the serials must be
// evicted with them, or a hand edit after the add reads as the add.
void test_session_sam_refine_at_cap() {
    Fixture f = make_dataset("sam_refine_cap", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "refine cap: open: " + err);
    settle(s);
    for (int k = 0; k < mk::kMaxHistoryOps - 1; k++)
        s.doc()->paint(k % 2 ? mk::Paint::ForceKeep : mk::Paint::ForceDrop,
                       box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    sam_add(s, disc_region(64, 48, 20.0f, 30.0f, 6.0f), 0);
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 50, 30, 60, 40), mk::Rect{50, 30, 60, 40});
    check(s.doc()->history_size() == mk::kMaxHistoryOps,
          "refine cap: the fixture is at the op cap, so the hand edit evicted a step");
    sam_add(s, disc_region(64, 48, 24.0f, 30.0f, 6.0f), 0);
    check(s.doc()->keep()[(size_t)35 * 64 + 55] == 255,
          "refine cap: at the cap a hand edit after the add survives a re-prompt");
}

// A phrase list with no phrase in it never starts a job (and its encode).
void test_session_sam_empty_phrase() {
    Fixture f = make_dataset("sam_empty_phrase", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "empty phrase: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    check(!s.sam_prompt_text("") && !s.sam_prompt_text("  ; \t;") && s.sam_error().empty(),
          "empty phrase: an empty or blank list is refused before any job");
    check(!s.sam_prompt_text(" ; door") && !s.sam_error().empty(),
          "empty phrase: a list with one phrase in it reaches the job (the stub refuses it)");
}

// The editor's clicks on `frame` for `object`, as the object list counts them.
int clicks_on(const gui::MaskSettings& p, long long frame, int object) {
    int n = 0;
    for (const gui::MaskClick& c : p.clicks) n += (c.frame == frame && c.object == object) ? 1 : 0;
    return n;
}

// Operator report: a revert left the reverted frame's SAM clicks in the object
// list, so the next click re-prompted with a correction just thrown away.
void test_session_sam_revert_forgets_clicks() {
    Fixture f = make_dataset("sam_revert_clicks", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam revert: open: " + err);
    settle(s);
    gui::MaskSettings& p = s.sam_prompt();
    p.prompt = "door";
    p.negative_prompt = "statue";
    s.sam_prompt_started(10.0f, 10.0f, true);
    s.sam_prompt_started(12.0f, 14.0f, false);
    s.go_to(1);
    settle(s);
    s.sam_prompt_started(30.0f, 20.0f, true);
    p.object_count = 2;
    p.current_object = 1;
    s.sam_prompt_started(40.0f, 30.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 40.0f, 30.0f, 6.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    s.sam_margin_changed();
    check(clicks_on(p, 0, 0) == 2 && clicks_on(p, 1, 0) == 1 && clicks_on(p, 1, 1) == 1 &&
              s.sam_held_bytes() > 0 && s.sam_margin_reapplies(),
          "sam revert: set up, clicks on two frames and a re-appliable add on frame 1");

    s.revert_open_frame();
    check(clicks_on(p, 1, 0) == 0 && clicks_on(p, 1, 1) == 0,
          "sam revert frame: the reverted frame's clicks are gone");
    check(clicks_on(p, 0, 0) == 2 && p.clicks.size() == 2 && p.clicks[0].x == 10.0f &&
              !p.clicks[1].positive,
          "sam revert frame: clicks on other frames stay, labels and all");
    check(s.sam_held_bytes() == 0 && !s.sam_margin_reapplies(),
          "sam revert frame: no held detection or margin re-apply outlives it");
    check(p.object_count == 2 && p.current_object == 1 && p.prompt == "door" &&
              p.negative_prompt == "statue",
          "sam revert frame: the object list, the phrase and the exceptions stay");
    settle(s);
    p.current_object = 0;
    check(s.sam().prompt_points(1, "", mk::SamPoint{33.0f, 21.0f, true}).size() == 1,
          "sam revert frame: a new click on the reverted frame sends only itself");
    const int h0 = s.doc()->history_size();
    s.sam_prompt_started(33.0f, 21.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 33.0f, 21.0f, 4.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->history_size() == h0 + 1 && clicks_on(p, 1, 0) == 1 &&
              clicks_on(p, 0, 0) == 2,
          "sam revert frame: that click adds, and the list reads 1 here, 2 elsewhere");

    s.sam_margin_changed();
    check(s.sam_held_bytes() > 0 && s.sam_margin_reapplies(),
          "sam revert all: set up, a re-appliable add on the open frame");
    s.revert_every_frame();
    check(p.clicks.empty() && p.object_count == 1 && p.current_object == 0,
          "sam revert all: every click is gone and the object list is fresh");
    check(s.sam_held_bytes() == 0 && !s.sam_margin_reapplies(),
          "sam revert all: no held detection or margin re-apply outlives it");
    check(p.prompt == "door" && p.negative_prompt == "statue",
          "sam revert all: the phrase and the exceptions stay");
    settle(s);
}

// The Find button and Enter share one gate and one action (sam_submit_text).
void test_session_sam_text_gate() {
    namespace em = spirula::i18n::msg::maskedit;
    using S = mk::MaskSession;
    const std::string model_first = spirula::i18n::msg::dataset::mask_model_first.get();
    check(S::sam_text_refusal(false, true, "", false, "door") == model_first,
          "text gate: no model names the download");
    check(S::sam_text_refusal(true, false, "", false, "door") == em::sam_text_unsupported.get(),
          "text gate: a checkpoint with no text tower says so");
    check(S::sam_text_refusal(true, true, "paused", false, "door") == "paused",
          "text gate: a blocker is its own reason");
    check(S::sam_text_refusal(true, true, "", true, "door") == em::sam_working.get(),
          "text gate: a job or margin re-apply running refuses");
    check(S::sam_text_refusal(true, true, "", false, "") == em::sam_text_empty.get() &&
              S::sam_text_refusal(true, true, "", false, " ;\t; ") == em::sam_text_empty.get(),
          "text gate: an empty or whitespace-only field refuses");
    check(S::sam_text_refusal(true, true, "", false, " ; door").empty(),
          "text gate: one phrase and nothing in the way is ready");
    Fixture f = make_dataset("sam_text_gate", 64, 48, {"a"});
    S s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "text gate: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam_prompt().prompt = " ;  ";
    check(!s.sam_submit_text() && s.sam_error().empty(),
          "text gate: submitting a blank field starts nothing");
    s.sam_prompt().prompt = "door";
    check(!s.sam_submit_text() && !s.sam_error().empty(),
          "text gate: submitting a phrase reaches the job (the stub refuses it)");
    s.set_sam_model("/m/sam2.ggml", false);
    check(!s.sam_submit_text() && s.sam_error() == em::sam_text_unsupported.get(),
          "text gate: a phrase to a SAM 2 checkpoint says to switch to SAM 3");
}

// ---------------------------------------------------------------------------
// The kept cache is keyed by fingerprint; the predicate
// ---------------------------------------------------------------------------

// A write is measured by whether it happened, never by the bytes: a
// byte-identical unconditional rewrite satisfies byte equality exactly.
// A key the writer never emits survives a read and dies in a rewrite.
std::vector<uint8_t> plant_write_probe(const fs::path& file) {
    std::vector<uint8_t> v = file_bytes(file);
    const std::string probe = "\"ss_probe\":1,";
    const bool object = !v.empty() && v[0] == '{';
    check(object, "probe: the file is a JSON object");
    if (!object) return v;   // an insert at begin() + 1 of nothing would crash, not fail
    v.insert(v.begin() + 1, probe.begin(), probe.end());
    check(mk::write_file_atomic(file.string(), v.data(), v.size()), "probe: planted");
    return v;
}

void test_kept_cache() {
    Fixture f = make_dataset("kept", 64, 48, {"a", "b"});
    const std::string mask_root = f.masks.string(), layer_root = f.layer.string();
    // The kept cache must never touch index.json. An index
    // that EXISTS and holds one entry makes that measurable; an assertion
    // that index.json is absent is satisfied by anything at all.
    mk::LayerIndex seed_idx;
    seed_idx.mask_root = mask_root;
    seed_idx.frames["b"] = mk::IndexEntry{};
    std::string ierr;
    check(seed_idx.save(layer_root, ierr), "fixture: an index.json to leave alone: " + ierr);
    const fs::path index_file = f.layer / mk::kIndexFileName;
    const std::vector<uint8_t> index_before = file_bytes(index_file);
    check(!index_before.empty(), "fixture: index.json has bytes to compare");
    std::vector<uint8_t> px = stencil_pixels(f.masks / "a.png");
    size_t n = 0;
    for (uint8_t v : px) n += v ? 1 : 0;
    const float truth = (float)n / (float)px.size();
    mk::KeptCache cache;
    std::string err;
    check(cache.load(layer_root, err) && cache.frames.empty() && !cache.dirty, "absent cache loads empty");
    float kept = 0.0f;
    check(mk::kept_fraction_of(mask_root, "a", false, cache, kept) && std::abs(kept - truth) < 1e-6f,
          "decoded kept fraction");
    check(cache.frames.count("a") == 1 && cache.dirty, "cached and dirty");
    uint64_t fp = 0;
    mk::fingerprint_file((f.masks / "a.png").string(), fp);
    check(cache.frames["a"].fp == fp, "cached under the file's fingerprint");
    // A hit is read from the cache and not decoded: plant a wrong value under
    // the right fingerprint and expect it back.
    cache.frames["a"].kept = 0.123f;
    check(mk::kept_fraction_of(mask_root, "a", false, cache, kept) && std::abs(kept - 0.123f) < 1e-6f,
          "a fingerprint hit is not decoded");
    // A stale fingerprint is decoded and replaced.
    cache.frames["a"].fp = fp ^ 1u;
    check(mk::kept_fraction_of(mask_root, "a", false, cache, kept) && std::abs(kept - truth) < 1e-6f &&
              cache.frames["a"].fp == fp, "a stale fingerprint is re-decoded and re-keyed");
    // A missing mask: false, -1, and no entry.
    check(!mk::kept_fraction_of(mask_root, "zzz", false, cache, kept) && kept == -1.0f &&
              cache.frames.count("zzz") == 0, "missing mask is false and uncached");
    // A mask that exists but is not an image: false, -1, and no entry.
    const std::vector<uint8_t> junk(64, 0x7f);
    check(mk::write_file_atomic((f.masks / "junk.png").string(), junk.data(), junk.size()),
          "fixture: a mask that is not a PNG");
    check(!mk::kept_fraction_of(mask_root, "junk", false, cache, kept) && kept == -1.0f &&
              cache.frames.count("junk") == 0, "an undecodable mask is false and uncached");
    // The operator's masks are 255 = DROP. q is a quarter white, so the two
    // conventions disagree by half, and a fraction cached under one is not
    // a hit under the other.
    write_png_gray(f.masks / "q.png", 64, 48, box_layer(64, 48, 0, 0, 32, 24));
    check(mk::kept_fraction_of(mask_root, "q", false, cache, kept) && std::abs(kept - 0.25f) < 1e-6f,
          "flipped: fixture: q keeps a quarter in the app's convention");
    check(mk::kept_fraction_of(mask_root, "q", true, cache, kept) && std::abs(kept - 0.75f) < 1e-6f,
          "flipped: a cached fraction does not outlive a change of convention");
    mk::KeptCache fresh;
    check(mk::kept_fraction_of(mask_root, "q", true, fresh, kept) && std::abs(kept - 0.75f) < 1e-6f,
          "flipped: kept counts the zeros when the folder's 255 is drop");
    check(cache.frames["q"].flipped, "flipped: the entry records its convention");
    // Round trip, and an unchanged cache is not rewritten.
    check(cache.save(layer_root, err), "save: " + err);
    const fs::path file = f.layer / mk::kKeptFileName;
    check(fs::exists(file), "kept.json written");
    mk::KeptCache back;
    check(back.load(layer_root, err) && back.frames.count("a") == 1 && back.frames["a"].fp == fp &&
              std::abs(back.frames["a"].kept - truth) < 1e-6f && !back.dirty, "round trip");
    check(back.frames.count("q") == 1 && back.frames["q"].flipped && !back.frames["a"].flipped,
          "flipped: the convention survives the round trip");
    const std::vector<uint8_t> probed = plant_write_probe(file);
    check(back.load(layer_root, err) && back.frames.count("a") == 1 && !back.dirty,
          "the probe key does not disturb the load");
    check(back.save(layer_root, err) && file_bytes(file) == probed, "a clean cache does not rewrite");
    check(mk::kept_fraction_of(mask_root, "a", false, back, kept) && !back.dirty, "a hit leaves it clean");
    check(mk::kept_fraction_of(mask_root, "b", false, back, kept) && back.dirty, "a decode marks it dirty");
    // A corrupt cache fails rather than reading as empty: a kept fraction
    // silently taken as zero would call every frame missing.
    check(mk::write_file_atomic(file.string(), junk.data(), junk.size()),
          "fixture: kept.json is not JSON");
    mk::KeptCache bad;
    std::string berr;
    check(!bad.load(layer_root, berr) && berr.find(mk::kKeptFileName) != std::string::npos,
          "a corrupt kept.json fails and names itself: " + berr);
    // index.json is not touched by any of this, and an index entry still
    // means "corrected": a scanned frame must not acquire one.
    check(file_bytes(index_file) == index_before, "index.json is byte-unchanged");
    mk::LayerIndex after_idx;
    check(after_idx.load(layer_root, ierr) && after_idx.frames.size() == 1 &&
              after_idx.frames.count("b") == 1,
          "still exactly the one entry the fixture put there");
}

void test_missing_predicate() {
    std::vector<mk::FrameHealth> v(7);
    v[0] = {true, false, 0.50f};
    v[1] = {true, true, -1.0f};
    v[2] = {true, false, 0.99f};
    v[3] = {false, false, -1.0f};
    v[4] = {true, false, 0.04f};
    v[5] = {true, false, 0.05f};
    v[6] = {true, false, 0.98f};
    const float lo = 0.05f, hi = 0.98f;
    check(!mk::is_missing(v[0], lo, hi), "inside the band");
    check(mk::is_missing(v[1], lo, hi), "no mask file");
    check(mk::is_missing(v[2], lo, hi), "above the band");
    check(!mk::is_missing(v[3], lo, hi), "unscanned is not missing");
    check(mk::is_missing(v[4], lo, hi), "below the band");
    check(!mk::is_missing(v[5], lo, hi), "the lower edge is inclusive");
    check(!mk::is_missing(v[6], lo, hi), "the upper edge is inclusive");
    check(mk::is_missing({true, true, 0.50f}, lo, hi), "no mask file, whatever kept says");
    check(mk::next_missing(v, 0, +1, lo, hi) == 1, "next from 0");
    check(mk::next_missing(v, 1, +1, lo, hi) == 2, "next from 1");
    check(mk::next_missing(v, 2, +1, lo, hi) == 4, "next from 2 skips the unscanned");
    check(mk::next_missing(v, 4, +1, lo, hi) == -1, "none after 4");
    check(mk::next_missing(v, 5, -1, lo, hi) == 4, "previous from 5");
    check(mk::next_missing(v, 1, -1, lo, hi) == -1, "none before 1");
    check(mk::next_missing(v, -1, +1, lo, hi) == 1, "from before the start");
}

// ---------------------------------------------------------------------------
// The scan, and jumping between missing frames
// ---------------------------------------------------------------------------

void test_session_find_missing() {
    Fixture f = make_dataset("find", 64, 48, {"a", "b", "c", "d", "e"});
    // b nearly empty (kept 0.01), c absent, e all kept (1.0).
    std::vector<uint8_t> nearly(64 * 48, 0);
    for (int i = 0; i < 31; i++) nearly[(size_t)i] = 255;
    write_png_gray(f.masks / "b.png", 64, 48, nearly);
    fs::remove(f.masks / "c.png");
    write_png_gray(f.masks / "e.png", 64, 48, std::vector<uint8_t>(64 * 48, 255));
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    wait_scanned(s);
    check(s.scanned_count() == 5, "all five scanned");
    check(s.health(1).scanned && !s.health(1).missing_mask && s.health(1).kept < 0.02f, "b's kept is tiny");
    check(s.health(2).scanned && s.health(2).missing_mask, "c has no mask");
    check(s.health(4).kept == 1.0f, "e is all kept");
    check(s.missing_count() == 3, "b, c, e are missing at the default band");
    check(s.band_lo() == 0.05f && s.band_hi() == 0.98f, "default band");
    check(s.frame_index() == 0 && s.go_to_missing(+1), "jump forward");
    settle(s);
    check(s.frame_index() == 1, "landed on b");
    check(s.go_to_missing(+1), "jump again");
    settle(s);
    check(s.frame_index() == 2 && s.doc() && s.doc()->base_state() == mk::BaseState::Missing, "landed on c, Missing");
    check(s.go_to_missing(+1), "and again");
    settle(s);
    check(s.frame_index() == 4, "landed on e");
    check(!s.go_to_missing(+1) && s.frame_index() == 4 &&
              s.error() == spirula::i18n::msg::maskedit::find_none.get(),
          "none further, and says which: " + s.error());
    check(s.go_to_missing(-1), "back");
    settle(s);
    check(s.frame_index() == 2, "back on c");
    s.set_band(0.0f, 1.0f);
    check(s.missing_count() == 1, "with a full band only the absent mask is missing");
    // set_band orders its arguments. The panel clamps; the API must too,
    // because lo above hi makes is_missing true for every scanned frame.
    s.set_band(0.90f, 0.10f);
    check(s.band_lo() == 0.10f && s.band_hi() == 0.90f, "set_band orders its arguments");
    s.set_band(0.0f, 1.0f);
    // The cache on disk: a, b, d, e; never c.
    mk::KeptCache cache;
    check(cache.load(s.layer_root(), err) && cache.frames.size() == 4 && cache.frames.count("c") == 0,
          "kept.json holds the four decodable masks");
    const fs::path kept_path = fs::path(s.layer_root()) / mk::kKeptFileName;
    check(s.scan_running(), "a scan thread is outstanding while the session is open");
    s.close();
    check(!s.is_open() && !s.scan_running(), "closed with the scan stopped and joined");
    // Second open: nothing changed, so the cache is read and not rewritten.
    // Byte equality means that only because of the probe key: the
    // writer never emits it, so a rewrite would drop it.
    const std::vector<uint8_t> probed = plant_write_probe(kept_path);
    // The band goes into the reopen at 0.0/1.0, which is NOT the member
    // default: "open() keeps it" and "open() resets it" are otherwise the
    // same observation, since 0.05/0.98 is what a reset would produce.
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "reopen");
    settle(s);
    wait_scanned(s);
    check(s.band_lo() == 0.0f && s.band_hi() == 1.0f, "open() leaves the band alone");
    check(s.missing_count() == 1, "and that band is the one the count uses");
    check(file_bytes(kept_path) == probed, "cache read, not rewritten");
    s.set_band(0.05f, 0.98f);
    check(s.missing_count() == 3, "same answer from the cache at the default band");
    // A save on the open frame refreshes its health without a rescan.
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 64.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    check(s.health(0).scanned && s.health(0).kept == 0.0f, "a's health follows the save");
    check(s.missing_count() == 4, "a is now missing too");

    // A propagate rewrites OTHER frames' masks. Only d is healthy at this
    // point, so d alone changes state and the count is a sharp instrument:
    // without the refresh it stays at 4 through both of these.
    check(s.health(3).scanned && !mk::is_missing(s.health(3), s.band_lo(), s.band_hi()),
          "fixture: d is the one healthy frame");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(s.last_propagate().done == 4, "propagated onto b, c, d, e");
    check(mk::is_missing(s.health(3), s.band_lo(), s.band_hi()) && s.missing_count() == 5,
          "d's health followed the propagate");
    check(s.can_undo_propagate(), "still on the source");
    s.undo_propagate();
    settle(s);
    check(!mk::is_missing(s.health(3), s.band_lo(), s.band_hi()) && s.missing_count() == 4,
          "and followed the undo back");

    // Revert frame rewrites the open frame's mask on the worker, not through
    // the save path, so only its own refresh can tell the count.
    check(mk::is_missing(s.health(0), s.band_lo(), s.band_hi()), "fixture: a is missing before the revert");
    s.revert_open_frame();
    settle(s);
    check(!mk::is_missing(s.health(0), s.band_lo(), s.band_hi()) && s.missing_count() == 3,
          "a's health followed Revert frame");

    // Revert all rewrites every corrected mask, the open frame's included, and
    // the reload after it rebases nothing: only its own refresh can tell.
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(mk::is_missing(s.health(0), s.band_lo(), s.band_hi()) &&
              mk::is_missing(s.health(3), s.band_lo(), s.band_hi()) && s.missing_count() == 5,
          "fixture: a and d are missing before Revert all");
    s.revert_every_frame();
    settle(s);
    check(!mk::is_missing(s.health(0), s.band_lo(), s.band_hi()) &&
              !mk::is_missing(s.health(3), s.band_lo(), s.band_hi()) && s.missing_count() == 3,
          "a's and d's health followed Revert all");

    check(s.scan_running(), "the reopened session's scan thread is outstanding too");
    s.close();
    check(!s.is_open() && !s.scan_running(), "the second close stopped and joined it as well");
}

// A directory where kept.json must land makes write_file_atomic's rename
// fail. close() joins the scan first, so the final save has happened by the
// time error() is read and there is no race with the scan thread.
void test_scan_save_failure() {
    Fixture f = make_dataset("scanfail", 64, 48, {"a", "b"});
    fs::create_directories(f.layer / mk::kKeptFileName);
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    wait_scanned(s);
    check(s.scanned_count() == 2, "the scan finished although its cache write could not");
    check(!s.health(0).missing_mask && !s.health(1).missing_mask, "and _health is still the product");
    s.close();
    check(s.error().find(mk::kKeptFileName) != std::string::npos,
          "the failed cache write named itself on the status line: " + s.error());
}

// The operator's masks are 255 = DROP: the scan counts the zeros there, so the
// band describes what the trainer keeps, as the open document's Kept does.
void test_session_find_missing_flipped() {
    Fixture f = make_dataset("find_flipped", 64, 48, {"a", "b"});
    write_png_gray(f.masks / "a.png", 64, 48, box_layer(64, 48, 0, 0, 32, 24));    // a quarter white
    write_png_gray(f.masks / "b.png", 64, 48, std::vector<uint8_t>(64 * 48, 0));   // all black
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), true, err),
          "find flipped: open: " + err);
    settle(s);
    wait_scanned(s);
    check(std::abs(s.health(0).kept - 0.75f) < 1e-6f,
          "find flipped: a keeps three quarters when the folder's 255 is drop");
    check(s.doc() && std::abs(s.doc()->kept_fraction() - s.health(0).kept) < 1e-6f,
          "find flipped: the scan agrees with the open document's Kept");
    check(s.health(1).kept == 1.0f && s.missing_count() == 1,
          "find flipped: an all-black flipped mask keeps everything, and is the one missing");
    // refresh_health reads under the same convention: a propagated corner drop.
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{32.0f, 24.0f, 64.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    mk::KeptCache fresh;
    float disk = -2.0f;
    mk::kept_fraction_of(s.mask_root(), "b", true, fresh, disk);
    check(s.last_propagate().done == 1 && disk > 0.6f && disk < 0.8f,
          "find flipped: fixture: b keeps all but a propagated corner: " + std::to_string(disk));
    check(std::abs(s.health(1).kept - disk) < 1e-6f,
          "find flipped: the propagate's refresh counts b in the folder's convention");
    s.close();
}

// The scan reads masks while the worker writes them. A job that ran during a
// frame's read makes that read stale: it is dropped and the frame read again,
// or the job's own refresh would be overwritten with the mask it replaced.
void test_scan_yields_to_a_write() {
    Fixture f = make_dataset("scanrace", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::atomic<int> phase{0};
    s.set_scan_hook_for_test([&phase](int i, bool read) {
        if (!read || i != 1 || phase.load() != 0) return;
        phase = 1;
        for (int k = 0; k < 4000 && phase.load() != 2; k++)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "scan race: open: " + err);
    settle(s);
    for (int k = 0; k < 4000 && phase.load() != 1; k++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(phase.load() == 1, "scan race: the scan holds b's read, not yet published");
    const float before = s.health(1).kept;
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 64.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    check(s.last_propagate().done == 1 && s.health(1).kept == 0.0f,
          "scan race: the propagate's refresh published b's new mask");
    phase = 2;
    wait_scanned(s);
    check(before == -1.0f, "scan race: fixture: b was unpublished when the job ran");
    check(s.scanned_count() == 2 && s.health(1).kept == 0.0f,
          "scan race: the scan's stale read of b did not overwrite the refresh");
    s.close();
    mk::KeptCache cache;
    uint64_t fp = 0;
    check(cache.load(f.layer.string(), err) && cache.frames.count("b") &&
              mk::fingerprint_file((f.masks / "b.png").string(), fp) &&
              cache.frames["b"].fp == fp && cache.frames["b"].kept == 0.0f,
          "scan race: kept.json holds b as it is on disk now");
}

// A FIFO at a mask path blocks an open until a writer comes; the scan must not
// wait on one, or close() waits on the scan.
void test_scan_skips_a_fifo() {
#ifndef _WIN32
    Fixture f = make_dataset("scanfifo", 64, 48, {"a", "b", "c"});
    const fs::path fifo = f.masks / "b.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "scan fifo: fixture: b's mask is a FIFO");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "scan fifo: open: " + err);
    settle(s);
    wait_scanned(s);
    check(s.scanned_count() == 3 && s.health(1).missing_mask && !s.health(2).missing_mask,
          "scan fifo: the scan passes b as missing and reaches c");
    for (int i = 0; i < 2000 && s.scanned_count() < 3; i++) {   // frees a mutant's blocked open
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    s.close();
#endif
}

// A re-run between sessions rewrites a corrected frame's mask. The scan sees
// the new file (its old fingerprint misses); the load rebases the frame and
// writes a new composite, which the scan never read.
void test_scan_follows_rebase() {
    Fixture f = make_dataset("scanrebase", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "rebase: open: " + err);
    settle(s);
    wait_scanned(s);
    s.go_to(1);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 32.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    s.close();
    write_png_gray(f.masks / "b.png", 64, 48, std::vector<uint8_t>(64 * 48, 255));
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "rebase: reopen: " + err);
    settle(s);
    wait_scanned(s);
    check(s.health(1).kept == 1.0f && s.missing_count() == 1,
          "rebase: the scan reads the re-run's mask, not its cached predecessor");
    check(s.go_to_missing(+1), "rebase: M");
    settle(s);
    const float composite = s.doc() ? s.doc()->kept_fraction() : -1.0f;
    check(s.frame_index() == 1 && composite > 0.4f && composite < 0.6f,
          "rebase: fixture: the load rebased b under its left-half drop");
    check(s.health(1).kept == composite && s.missing_count() == 0,
          "rebase: b's health followed the composite its load wrote");
    s.close();
}

// A mask that exists and will not decode is missing to the scan; the load
// that M starts fails and names the file. The next M must move past it, and
// the way back must load the frame the failed load left in place.
void test_find_missing_unreadable() {
    Fixture f = make_dataset("find_corrupt", 64, 48, {"a", "b", "c", "d"});
    write_png_gray(f.masks / "a.png", 64, 48, std::vector<uint8_t>(64 * 48, 0));
    const char junk[] = "not a png";
    check(mk::write_file_atomic((f.masks / "b.png").string(), (const uint8_t*)junk, sizeof junk - 1),
          "unreadable: fixture written");
    fs::remove(f.masks / "c.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "unreadable: open: " + err);
    settle(s);
    wait_scanned(s);
    check(s.health(1).scanned && s.health(1).missing_mask && s.missing_count() == 3,
          "unreadable: the scan counts b missing");
    check(s.go_to_missing(+1), "unreadable: M");
    settle(s);
    const std::string named = spirula::i18n::format(spirula::i18n::msg::maskedit::err_read,
                                                    {mk::mask_file(s.mask_root(), "b")});
    check(!s.doc() && s.error() == named, "unreadable: the status line names b's mask: " + s.error());
    check(s.go_to_missing(-1), "unreadable: Shift+M");
    settle(s);
    check(s.frame_index() == 0 && s.doc(), "unreadable: back onto a, which the failed load left open");
    check(s.go_to_missing(+1), "unreadable: M again");
    settle(s);
    check(s.go_to_missing(+1), "unreadable: and M once more");
    settle(s);
    check(s.frame_index() == 2 && s.doc() && s.doc()->base_state() == mk::BaseState::Missing,
          "unreadable: the next M moves past the frame that would not load");
    s.close();
}

// Spec 9.2's second arm, "a layer but no base (5.4)": 5.4's base is
// masks/<key>.png, and with it absent the first arm already holds.
void test_find_missing_layer_without_mask() {
    Fixture f = make_dataset("find_layer", 64, 48, {"a", "c"});
    fs::remove(f.masks / "c.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "layer: open: " + err);
    settle(s);
    s.go_to(1);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 16.0f, 16.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.save();
    settle(s);
    check(s.health(1).scanned && s.health(1).missing_mask,
          "layer: the save leaves c missing, before any reopen");
    s.close();
    check(fs::exists(mk::layer_file(f.layer.string(), "c", mk::Layer::Drop)) &&
              !fs::exists(mk::layer_file(f.layer.string(), "c", mk::Layer::Base)) &&
              !fs::exists(f.masks / "c.png"),
          "layer: fixture: c has a drop layer, no base and no mask");
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "layer: reopen: " + err);
    settle(s);
    wait_scanned(s);
    s.set_band(0.0f, 1.0f);
    check(s.health(1).missing_mask && s.missing_count() == 1,
          "layer: a frame with a correction layer and no mask is missing at any band");
    s.close();
}

float disk_kept(const std::string& mask_root, const std::string& key, bool flipped) {
    mk::KeptCache fresh;
    float k = -2.0f;
    mk::kept_fraction_of(mask_root, key, flipped, fresh, k);
    return k;
}

// Fix a missing frame, then leave it every way the editor offers. The save
// those make lands after the document is gone, so only a refresh in the
// save's own job can move b out of the count.
void fix_then_leave_arm(bool flipped, int route) {
    const char* names[5] = {"M", "Right", "First", "Last", "Play"};
    const std::string tag = std::string(flipped ? "fix flipped, " : "fix, ") + names[route] + ": ";
    Fixture f = make_dataset(flipped ? "fix_leave_f" : "fix_leave_u", 64, 48, {"a", "b", "c", "d"});
    std::vector<uint8_t> nearly(64 * 48, 0), all(64 * 48, 255);
    for (int i = 0; i < 31; i++) nearly[(size_t)i] = 255;
    if (flipped) {
        for (uint8_t& v : nearly) v = (uint8_t)(255 - v);
        std::fill(all.begin(), all.end(), 0);
    }
    write_png_gray(f.masks / "b.png", 64, 48, nearly);   // kept 0.01
    write_png_gray(f.masks / "d.png", 64, 48, all);      // kept 1.0
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), tag + "open: " + err);
    settle(s);
    wait_scanned(s);
    check(s.missing_count() == 2 && s.go_to_missing(+1), tag + "fixture: b and d missing, M");
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 32.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceKeep, m);
    check(s.frame_index() == 1 && s.doc() && s.doc()->dirty(), tag + "fixture: b fixed, unsaved");
    if (route == 0) s.go_to_missing(+1);
    if (route == 1) s.go_to(s.frame_index() + 1);
    if (route == 2) s.go_to(0);
    if (route == 3) s.go_to(s.frame_count() - 1);
    if (route == 4) {
        s.start_slideshow();
        check(s.slideshow_playing(), tag + "fixture: playing");
        settle(s);
        s.stop_slideshow();
    }
    settle(s);
    settle(s);
    const float disk = disk_kept(s.mask_root(), "b", flipped);
    check(disk > 0.4f && disk < 0.6f, tag + "fixture: b's fix is on disk: " + std::to_string(disk));
    check(std::abs(s.health(1).kept - disk) < 1e-6f && s.missing_count() == 1,
          tag + "b's health follows the save leaving it made");
    bool back = false;
    for (int k = 0; k < 4 && s.go_to_missing(-1); k++) {
        settle(s);
        back = back || s.frame_index() == 1;
    }
    check(!back, tag + "Shift+M never returns to the fixed b");
    s.close();
}

void test_fix_then_leave() {
    for (int route = 0; route < 5; route++) {
        fix_then_leave_arm(false, route);
        fix_then_leave_arm(true, route);
    }
}

// A read the scan begins while a job runs must wait the job out: the job may
// write the mask after the read and refresh it before it finishes, and the
// read's value would then be published over the refresh.
void test_scan_waits_out_a_running_job() {
    Fixture f = make_dataset("scanwait", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::atomic<bool> armed{false}, at_b{false}, read_b{false}, started{false}, refreshed{false};
    const auto until = [](const std::atomic<bool>& flag, int ms) {
        for (int k = 0; k < ms && !flag.load(); k++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    };
    s.set_scan_hook_for_test([&](int i, bool read) {
        if (i != 1) return;
        if (!read && !at_b.exchange(true)) until(started, 3000);
        if (read && started.load() && !read_b.exchange(true)) until(refreshed, 1000);
    });
    s.set_worker_hook_for_test([&](bool end) {
        if (!armed.load()) return;
        if (!end) {
            started = true;
            until(read_b, 300);
            return;
        }
        refreshed = true;
        for (int k = 0; k < 300 && s.scanned_count() < 2; k++)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "scan wait: open: " + err);
    settle(s);
    until(at_b, 3000);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 64.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    armed = true;
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    armed = false;
    wait_scanned(s);
    check(at_b.load() && started.load() && s.last_propagate().done == 1,
          "scan wait: fixture: the scan stood at b when the propagate began");
    check(s.scanned_count() == 2 && s.health(1).kept == 0.0f,
          "scan wait: a read begun during a job does not overwrite that job's refresh");
    s.close();
}

// close() stops the scan before it forgets the frames the scan
// reads. The hook holds the scan mid-dataset and looks at them after a wait.
void test_close_mid_scan() {
    std::vector<std::string> keys;
    for (int i = 0; i < 40; i++) {
        char b[16];
        std::snprintf(b, sizeof b, "f%04d", i);
        keys.push_back(b);
    }
    Fixture f = make_dataset("close_mid", 64, 48, keys);
    {
        mk::MaskSession s;
        std::atomic<bool> held{false};
        std::atomic<int> seen{-1};
        s.set_scan_hook_for_test([&](int i, bool read) {
            if (read || i != 5 || held.exchange(true)) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            seen = (int)s.frames().size();
        });
        std::string err;
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "close mid: open: " + err);
        for (int k = 0; k < 3000 && !held.load(); k++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        s.close();
        check(held.load() && seen.load() == 40, "close mid: close() stops the scan before it forgets the frames");
    }
    // Closed at any point, kept.json is whole and every entry is its file's.
    for (int rep = 0; rep < 4; rep++) {
        mk::MaskSession s;
        std::string err;
        s.open(f.root.string(), f.images.string(), f.masks.string(), false, err);
        std::this_thread::sleep_for(std::chrono::milliseconds(rep * 3));
        s.close();
        mk::KeptCache c;
        const bool loaded = c.load(f.layer.string(), err);
        int bad = 0;
        for (const auto& [k, e] : c.frames) {
            uint64_t fp = 0;
            if (!mk::fingerprint_file(mk::mask_file(f.masks.string(), k), fp) || fp != e.fp) bad++;
        }
        check(loaded && bad == 0, "close mid: kept.json is whole and true after close " + std::to_string(rep));
    }
}

// A cancel before the first target: nothing written, all counted as not
// reached. Cancelling the undo keeps every frame in the record.
void test_propagate_cancel() {
    check(mk::propagate_threads(7680, 3840, 16) == 6 && mk::propagate_threads(7680, 3840, 4) == 3 &&
              mk::propagate_threads(1920, 1080, 64) == 8 && mk::propagate_threads(0, 0, 16) == 1 &&
              mk::propagate_threads(64, 48, 1) == 1,
          "cancel: threads by the 1 GB budget, the cores and the cap of 8");
    Fixture f = make_dataset("prop_cancel", 64, 48, {"a", "b", "c", "d", "e"});
    const std::vector<uint8_t> original_c = file_bytes(f.masks / "c.png");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "cancel: open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.set_worker_hook_for_test([&s](bool finished) { if (!finished) s.cancel_propagate(); });
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    const mk::PropagateReport r = s.last_propagate();
    check(r.done == 0 && r.skipped == 4 && !s.can_undo_propagate() &&
              file_bytes(f.masks / "c.png") == original_c && !fs::exists(f.layer / "c.drop.png"),
          "cancel: no target written, four not reached");
    check(s.status() == spirula::i18n::format(spirula::i18n::msg::maskedit::prop_stopped, {0, 4, 0, 0}),
          "cancel: the status says it stopped: " + s.status());
    s.set_worker_hook_for_test(nullptr);
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(s.last_propagate().done == 4 && s.last_propagate().skipped == 0 && s.can_undo_propagate(),
          "cancel: the next run is not cancelled");
    s.set_worker_hook_for_test([&s](bool finished) { if (!finished) s.cancel_propagate(); });
    s.undo_propagate();
    settle(s);
    s.set_worker_hook_for_test(nullptr);
    check(s.can_undo_propagate() && fs::exists(f.layer / "c.drop.png"),
          "cancel: a stopped undo keeps every frame undoable");
    s.undo_propagate();
    settle(s);
    check(!s.can_undo_propagate() && file_bytes(f.masks / "c.png") == original_c &&
              !fs::exists(f.layer / "c.drop.png") && s.corrected_count() == 1,
          "cancel: undo again puts them all back");
    s.close();
}

// An undo whose target's mask went back but whose index write failed: the
// target stays in the record, and its key must still reach the refresh.
void test_undo_refresh_after_index_failure() {
    Fixture f = make_dataset("undo_index_fail", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "undo index: open: " + err);
    settle(s);
    wait_scanned(s);
    const float before = s.health(1).kept;
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{0.0f, 0.0f, 64.0f, 48.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const fs::path index = f.layer / mk::kIndexFileName;
    fs::remove(index);
    fs::create_directories(index);
    s.undo_propagate();
    settle(s);
    const float disk = disk_kept(s.mask_root(), "b", false);
    check(!s.error().empty() && s.can_undo_propagate() && std::abs(disk - before) < 1e-6f,
          "undo index: fixture: b's mask went back, its index write failed: " + s.error());
    check(std::abs(s.health(1).kept - disk) < 1e-6f,
          "undo index: the failed target's health follows its mask anyway");
    std::error_code ec;
    fs::remove_all(index, ec);
    s.close();
}

// A close() that cleared the roots under a running scan once made its final
// save write kept.json into the working folder. The cache refuses such a root.
void test_kept_cache_refuses_a_relative_root() {
    const fs::path d = scratch("kept_relative"), was = fs::current_path();
    fs::current_path(d);
    mk::KeptCache c;
    c.frames["a"] = mk::KeptEntry{1, false, 0.5f};
    c.dirty = true;
    std::string e1, e2, e3;
    const bool empty_saved = c.save("", e1), rel_saved = c.save("rel", e2);
    const bool loaded = mk::KeptCache().load("", e3);
    const bool wrote = fs::exists(d / mk::kKeptFileName) || fs::exists(d / "rel");
    fs::current_path(was);
    check(!empty_saved && !rel_saved && !wrote, "kept root: an empty or relative root writes nothing");
    check(e1.find(mk::kKeptFileName) != std::string::npos && e2.find("rel") != std::string::npos,
          "kept root: and says why: " + e1);
    check(!loaded && !e3.empty(), "kept root: nor reads from the working folder");
}

void test_band_edit() {
    int lo = 99, hi = 98;
    mk::band_edit(lo, hi, true);
    check(lo == 98 && hi == 98, "band: typing Min above Max stops at Max, and Max stays");
    lo = 50;
    hi = 7;
    mk::band_edit(lo, hi, false);
    check(lo == 50 && hi == 50, "band: typing Max below Min stops at Min, and Min stays");
    lo = -4;
    hi = 150;
    mk::band_edit(lo, hi, true);
    check(lo == 0 && hi == 150, "band: Min held at 0");
    mk::band_edit(lo, hi, false);
    check(hi == 100, "band: Max held at 100");
}

// ---------------------------------------------------------------------------
// The slideshow's decoder ring and clock
// ---------------------------------------------------------------------------

bool wait_has(const mk::SlidePrefetch& p, int index, int ms = 3000) {
    for (int i = 0; i < ms / 5; i++) {
        if (p.has(index)) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return p.has(index);
}

bool wait_decoded(const mk::SlidePrefetch& p, int n, int ms = 3000) {
    for (int i = 0; i < ms / 5; i++) {
        if (p.decoded() >= n) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return p.decoded() >= n;
}

// load_picture reads stb's buffers in place and boxes the photo before
// it decodes the mask. The oracle shares no code with Picture.cpp: the photo
// and the 0/255 stencil come from FrameMask.cpp, the box and the tint are here.
void oracle_picture(const std::string& img, const std::string& msk, int target, bool flipped,
                    gui::Picture& out) {
    int w = 0, h = 0, mw = 0, mh = 0;
    std::vector<uint8_t> rgb, m;
    app::load_rgb(img, w, h, rgb);
    const bool got = !msk.empty() && app::load_stencil(msk, mw, mh, m);
    const int step = std::max(1, (std::max(w, h) + target - 1) / target);
    out = gui::Picture{};
    out.w = std::max(1, w / step);
    out.h = std::max(1, h / step);
    out.src_w = w;
    out.src_h = h;
    out.made_for = target;
    out.rgb.resize((size_t)out.w * out.h * 3);
    for (int y = 0; y < out.h; y++)
        for (int x = 0; x < out.w; x++) {
            int acc[3] = {0, 0, 0}, n = 0, keep = 0;
            for (int sy = y * step; sy < std::min(h, y * step + step); sy++)
                for (int sx = x * step; sx < std::min(w, x * step + step); sx++, n++) {
                    for (int c = 0; c < 3; c++) acc[c] += rgb[((size_t)sy * w + sx) * 3 + c];
                    if (!got) continue;
                    const int my = std::min(mh - 1, sy * mh / h), mx = std::min(mw - 1, sx * mw / w);
                    keep += (m[(size_t)my * mw + mx] == 255) != flipped;
                }
            uint8_t* px = &out.rgb[((size_t)y * out.w + x) * 3];
            for (int c = 0; c < 3; c++) px[c] = (uint8_t)(acc[c] / n);
            if (got && 2 * keep < n) {
                px[0] = (uint8_t)(px[0] / 3 + 150);
                px[1] = (uint8_t)(px[1] / 3);
                px[2] = (uint8_t)(px[2] / 3);
            }
        }
}

// Grey, not 0/255: 127 and 128 sit either side of the keep threshold. The left
// half alternates them by column, so every block of even width is exactly half kept.
std::vector<uint8_t> grey_mask(int w, int h) {
    std::vector<uint8_t> px((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            px[(size_t)y * w + x] = x < w / 2 ? (uint8_t)(x % 2 ? 128 : 127)
                                              : (uint8_t)((x * 7 + y * 13 + (x * y) % 97) & 255);
    return px;
}

// Blocks of `step` in which exactly half the pixels are kept (> 127).
int half_kept_blocks(const std::vector<uint8_t>& m, int w, int h, int step) {
    int count = 0;
    for (int y = 0; y + step <= h; y += step)
        for (int x = 0; x + step <= w; x += step) {
            int keep = 0;
            for (int sy = y; sy < y + step; sy++)
                for (int sx = x; sx < x + step; sx++) keep += m[(size_t)sy * w + sx] > 127;
            count += keep * 2 == step * step;
        }
    return count;
}

void test_load_picture_matches_make_picture() {
    const int W = 1536, H = 768;
    const fs::path d = scratch("picture_parity");
    const std::string img = (d / "a.jpg").string(), msk = (d / "a.png").string();
    write_jpg_rgb(img, W, H, synth_rgb(W, H, 3));
    const std::vector<uint8_t> m = grey_mask(W, H);
    write_png_gray(msk, W, H, m);
    for (int target : {1536, 768, 512, 256, 192}) {
        const int step = (std::max(W, H) + target - 1) / target;
        if (step % 2 == 0)
            check(half_kept_blocks(m, W, H, step) > 0,
                  "picture parity: fixture holds exactly-half-kept blocks at step " + std::to_string(step));
        for (bool flipped : {false, true}) {
            gui::Picture got, want;
            const bool ok = gui::load_picture(img, msk, target, got, flipped);
            oracle_picture(img, msk, target, flipped, want);
            const std::string tag = "picture parity: target " + std::to_string(target) +
                                    (flipped ? " flipped" : "");
            check(ok && got.w == want.w && got.h == want.h && got.src_w == W && got.made_for == target,
                  tag + ": the oracle's size " + std::to_string(want.w) + "x" + std::to_string(want.h));
            check(ok && got.rgb == want.rgb, tag + ": byte for byte the oracle's");
        }
    }
    int w = 0, h = 0, mw = 0, mh = 0;
    std::vector<uint8_t> rgb, st;
    app::load_rgb(img, w, h, rgb);
    app::load_stencil(msk, mw, mh, st);
    for (int target : {768, 256}) {
        gui::Picture made, want;
        gui::make_picture(rgb.data(), w, h, st.data(), target, made);
        oracle_picture(img, msk, target, false, want);
        check(made.rgb == want.rgb && made.w == want.w,
              "picture parity: make_picture keeps its bytes, target " + std::to_string(target));
    }
    gui::Picture bare, masked;
    gui::load_picture(img, "", 256, bare);
    gui::load_picture(img, msk, 256, masked);
    check(!bare.empty() && bare.rgb != masked.rgb, "picture parity: fixture check: the mask tints something");
}

void test_load_picture_reuses_its_buffer() {
    const fs::path d = scratch("picture_reuse");
    const std::string img = (d / "a.jpg").string(), msk = (d / "a.png").string();
    write_jpg_rgb(img, 640, 480, synth_rgb(640, 480, 1));
    write_png_gray(msk, 640, 480, synth_mask(640, 480, 1));
    gui::Picture out;
    // Room for twice the picture: a fresh vector would come back exactly sized.
    out.rgb.reserve((size_t)320 * 240 * 3 * 2);
    const uint8_t* data = out.rgb.data();
    const size_t cap = out.rgb.capacity();
    bool same = true, ok = true;
    for (int i = 0; i < 20; i++) {
        ok = ok && gui::load_picture(img, msk, 320, out) && out.w == 320;
        same = same && out.rgb.data() == data && out.rgb.capacity() == cap;
    }
    check(ok && same, "picture reuse: 20 loads at one target keep the buffer they were given");
    const bool failed = !gui::load_picture((d / "missing.jpg").string(), msk, 320, out);
    check(failed && out.empty() && out.w == 0 && out.rgb.capacity() == cap,
          "picture reuse: a file that does not decode empties the picture and keeps its buffer");
}

void test_load_picture_mask_other_size() {
    const fs::path d = scratch("picture_other_size");
    const std::string img = (d / "a.jpg").string(), msk = (d / "a.png").string();
    // Odd sizes, so nearest sampling of a half-size mask lands on row and
    // column boundaries a rounded or shifted map would cross.
    write_jpg_rgb(img, 203, 151, synth_rgb(203, 151, 5));
    write_png_gray(msk, 101, 75, synth_mask(101, 75, 5));
    for (int target : {203, 101, 40})
        for (bool flipped : {false, true}) {
            gui::Picture got, want;
            gui::load_picture(img, msk, target, got, flipped);
            oracle_picture(img, msk, target, flipped, want);
            check(!got.empty() && got.rgb == want.rgb,
                  "picture other size: a half-size mask samples like the oracle, target " +
                      std::to_string(target) + (flipped ? " flipped" : ""));
        }
}

// A JPEG mask stored sideways with Orientation 6 is turned upright before it is
// laid on the photo, as load_stencil does; stb's buffer alone would not be.
void test_load_picture_turned_mask() {
    const fs::path d = scratch("picture_turned");
    const std::string img = (d / "a.jpg").string(), msk = (d / "a_mask.jpg").string();
    write_jpg_rgb(img, 64, 48, synth_rgb(64, 48, 2));
    std::vector<uint8_t> side((size_t)48 * 64 * 3);
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 48; x++) {
            const uint8_t v = (x < 12 || y < 20) ? 0 : 255;
            for (int c = 0; c < 3; c++) side[((size_t)y * 48 + x) * 3 + c] = v;
        }
    check(write_jpg_rgb_oriented(msk, 48, 64, side, 6) && !app::photo_turn(msk).identity(),
          "picture turned: fixture: a 48x64 JPEG mask carrying Orientation 6");
    for (bool flipped : {false, true}) {
        gui::Picture got, want;
        gui::load_picture(img, msk, 32, got, flipped);
        oracle_picture(img, msk, 32, flipped, want);
        check(!got.empty() && got.rgb == want.rgb,
              std::string("picture turned: the mask is turned before it is laid on") + (flipped ? ", flipped" : ""));
    }
}

void test_slide_prefetch() {
    Fixture f = make_dataset("slide", 64, 48, {"a", "b", "c", "d", "e", "g"});
    std::vector<mk::SlideFrame> frames;
    for (const std::string& k : f.keys)
        frames.push_back({(f.images / (k + ".jpg")).string(), (f.masks / (k + ".png")).string()});
    // The last frame has no mask: the picture is the bare photo, not a failure.
    fs::remove(f.masks / "g.png");
    mk::SlidePrefetch p;
    check(!p.running(), "not running before start");
    p.set_target(32);
    p.start(frames, 2);
    check(p.running(), "running after start");
    p.want(0, 4);
    for (int i = 0; i < 4; i++) check(wait_has(p, i), "frame " + std::to_string(i) + " decoded");
    check(!p.has(4) && !p.has(5), "outside the window is not decoded");
    // A decode of an unwanted index lands nowhere, so has() cannot see one and
    // only this counter can. Four wanted, four decodes, nothing speculative.
    check(p.decoded() == 4, "four wanted, four decodes: " + std::to_string(p.decoded()));
    check(p.bytes() <= gui::kReelBudget && p.bytes() > 0, "under the budget");
    gui::Picture pic;
    check(p.take(0, pic) && pic.w == 32 && pic.h == 24 && pic.src_w == 64, "take 0: 64x48 at a 32 target is 32x24");
    // The mask was composed in: the corner of synth_mask is dropped, and
    // Picture.cpp tints a dropped block r/3+150, so the red channel is >= 150.
    check(pic.rgb[0] >= 150, "mask tinted into the picture");
    // The operator's masks are 255 = DROP. Flipped, synth_mask's dropped corner
    // is kept, so the same corner must come out untinted.
    std::vector<mk::SlideFrame> flipped_frames = frames;
    for (mk::SlideFrame& fr : flipped_frames) fr.flipped = true;
    mk::SlidePrefetch fp;
    fp.set_target(32);
    fp.start(flipped_frames, 1);
    fp.want(0, 1);
    gui::Picture fpic;
    check(wait_has(fp, 0) && fp.take(0, fpic) && fpic.rgb[0] < 150,
          "flipped: the file's dropped corner is kept, so it is not tinted");
    // The file KEEPS the ellipse centre, so flipped it must be tinted: a flag
    // that threw the mask away would leave corner and centre both untinted.
    const size_t mid = ((size_t)12 * 32 + 16) * 3;
    check(pic.rgb.size() > mid && fpic.rgb.size() > mid && pic.rgb[mid] < 150 && fpic.rgb[mid] >= 150,
          "flipped: the file's kept centre is dropped, so it is tinted");
    fp.stop();
    check(!p.take(0, pic), "a taken frame is gone");
    p.want(1, 4);
    check(wait_has(p, 4), "the window moved: 4 decoded");
    // Behind the window nothing is decoded and nothing is re-decoded: this is
    // what makes one shown frame cost one decode.
    const int settled = p.decoded();
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    check(p.decoded() == settled, "the pool stops when the window is full: " + std::to_string(p.decoded()));
    check(!p.has(0), "the frame taken stays behind the window");
    p.want(2, 4);
    check(!p.has(1), "1 is behind the window and dropped without being taken");
    check(wait_has(p, 5), "5 decoded");
    p.want(4, 4);
    check(wait_has(p, 0) && wait_has(p, 1) && wait_has(p, 5), "wraps at the end: 4,5,0,1");
    check(p.take(5, pic) && !pic.empty() && pic.rgb[0] < 150, "no mask: the bare photo, untinted");
    const int before = p.decoded();
    p.stop();
    const int after = p.decoded();
    check(!p.running(), "not running after stop");
    // A worker mid-decode finishes that one before it sees _stop, so the
    // count may rise by at most one per thread; it must not rise after.
    check(after >= before && after <= before + 2, "stop adds at most one decode a thread");
    check(p.decoded() == after, "the count is stable once the threads are joined");
    check(p.bytes() == 0, "stop drops the ring");

    // The byte budget, with the real one overridden so it binds on a fixture
    // that fits in a test: 5,000 bytes holds two 32x24 pictures, not three.
    mk::SlidePrefetch b;
    b.set_target(32);
    b.set_byte_budget_for_test(5000);
    b.start(frames, 1);
    b.want(0, 4);
    check(wait_decoded(b, 4), "budget arm: four decodes attempted");
    size_t peak = 0;
    for (int i = 0; i < 40; i++) {
        peak = std::max(peak, b.bytes());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    b.stop();
    check(peak > 0 && peak <= 5000, "the ring never exceeds the byte budget: " + std::to_string(peak));
    check(peak + 2304 > 5000, "fixture check: a third picture would not have fitted");

    check(mk::slide_picture_bytes(7680, 3840, 4096) == 3840u * 1920u * 3u, "4096: an 8K frame steps by 2");
    check(mk::slide_depth(mk::slide_picture_bytes(7680, 3840, 4096), 100) == 3, "4096: the budget holds three");
    check(mk::slide_depth(mk::slide_picture_bytes(7680, 3840, 1024), 100) == (int)gui::kReelSlots - 1,
          "1024: the slot count binds, not the budget");
    check(mk::slide_depth(mk::slide_picture_bytes(7680, 3840, 1024), 4) == 3,
          "never as wide as the dataset: the frame being shown is not in the window");
    check(mk::slide_depth(mk::slide_picture_bytes(64, 48, 32), 2) == 1, "two frames: one frame ahead");
    check(mk::slide_depth(64u << 20, 100) == 1, "one picture over half the budget: a window of one");

    mk::SlideClock c;
    c.fps = 10.0;
    c.start(100.0);
    check(!c.due(100.05), "not due before the period");
    check(c.due(100.10), "due at the period");
    check(!c.due(100.15), "the next period runs from the frame just shown");
    check(c.due(100.50) && !c.due(100.55), "a late frame does not queue up a burst");
    // 5 and 30 are the ends of the UI's range and the two rates the
    // slideshow is benched at; a hard-coded 0.1 period passes the 10 fps rows alone.
    mk::SlideClock c5;
    c5.fps = 5.0;
    c5.start(200.0);
    check(!c5.due(200.15) && c5.due(200.20), "5 fps: a 200 ms period");
    check(!c5.due(200.35) && c5.due(200.40), "5 fps: due() re-arms at 200 ms, not only start()");
    mk::SlideClock c30;
    c30.fps = 30.0;
    c30.start(300.0);
    check(!c30.due(300.030) && c30.due(300.034), "30 fps: a 33.3 ms period");
    check(!c30.due(300.066) && c30.due(300.068), "30 fps: due() re-arms at 33.3 ms, not only start()");
}

// How many decoders run at once is set by the bytes one decode holds.
void test_slide_threads() {
    check(mk::slide_threads(7680, 3840, 18) == 2, "slide threads: 8K runs two decoders");
    check(mk::slide_threads(3840, 2160, 18) == 4, "slide threads: 4K is held at four by the cap, not the budget");
    check(mk::slide_threads(1920, 1080, 18) == 4, "slide threads: 1080p runs four");
    check(mk::slide_threads(15520, 7760, 18) == 1, "slide threads: a frame over the budget still decodes, alone");
    check(mk::slide_threads(0, 0, 18) == 1, "slide threads: an unknown size decodes one at a time");
    check(mk::slide_threads(1920, 1080, 2) == 1, "slide threads: two cores leave one for the UI");
    check(mk::slide_threads(1920, 1080, 3) == 2, "slide threads: three cores run two");
    check(mk::slide_threads(1920, 1080, 0) == 1, "slide threads: an unknown core count runs one");
    check(2 * mk::slide_decode_bytes(7680, 3840) <= mk::kSlideDecodeBudget &&
              mk::kSlideDecodeBudget < 3 * mk::slide_decode_bytes(7680, 3840),
          "slide threads: the budget holds two 8K decodes and not three");
}

// While playing the ring clears and swaps pictures, never frees them: over 100
// frames the buffers made are bounded by the decoders, the window and the one shown.
void test_slide_prefetch_keeps_buffers() {
    Fixture f = make_dataset("slide_keep", 64, 48, {"a", "b", "c"});
    std::vector<mk::SlideFrame> frames;
    for (int i = 0; i < 100; i++) {
        const std::string& k = f.keys[(size_t)(i % 3)];
        frames.push_back({(f.images / (k + ".jpg")).string(), (f.masks / (k + ".png")).string()});
    }
    const size_t one = mk::slide_picture_bytes(64, 48, 32);
    const int depth = 3;
    const int threads = mk::slide_threads(64, 48, std::thread::hardware_concurrency());
    mk::SlidePrefetch p;
    p.set_target(32);
    p.set_byte_budget_for_test((size_t)depth * one);
    p.start(frames, threads);
    p.want(0, depth);
    gui::Picture pic;
    int shown = 0;
    size_t peak_cap = 0;
    for (int i = 0; i < 100; i++) {
        bool got = false;
        for (int k = 0; k < 3000 && !(got = p.take(i, pic)); k++)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        shown += got && pic.w == 32 && !pic.empty();
        p.want((i + 1) % 100, depth);
        peak_cap = std::max(peak_cap, p.capacity_bytes());
    }
    const int allocs = p.picture_allocs(), decodes = p.decoded();
    p.stop();
    check(shown == 100, "keeps buffers: every frame shown once: " + std::to_string(shown));
    check(allocs >= 1 && allocs <= threads + depth + 1,
          "keeps buffers: pictures made " + std::to_string(allocs) + " <= threads + depth + 1 = " +
              std::to_string(threads + depth + 1));
    check(peak_cap <= (size_t)(depth + 1) * one,
          "keeps buffers: the ring holds " + std::to_string(peak_cap) + " bytes <= (depth + 1) pictures");
    // One decode a frame, plus the window wrapping onto frames 0..depth-1.
    check(decodes <= 100 + depth, "keeps buffers: one decode a frame: " + std::to_string(decodes) +
                                      " <= 100 + depth");

    // A window that jumps drops every held frame at once; those buffers stay.
    mk::SlidePrefetch j;
    j.set_target(32);
    j.set_byte_budget_for_test((size_t)depth * one);
    j.start(frames, threads);
    bool held = true;
    for (int k = 0; k < 20; k++) {
        const int from = (k % 2) * 50;
        j.want(from, depth);
        for (int d = 0; d < depth; d++) held = held && wait_has(j, from + d);
    }
    const int jump_allocs = j.picture_allocs();
    j.stop();
    check(held && jump_allocs <= threads + depth,
          "keeps buffers: 20 jumps of the window make " + std::to_string(jump_allocs) +
              " pictures <= threads + depth");

    // Over budget the ring evicts; an evicted frame's buffer stays too.
    mk::SlidePrefetch e;
    e.set_target(32);
    e.set_byte_budget_for_test(2 * one);
    e.start(frames, threads);
    e.want(0, depth);
    const bool churned = wait_decoded(e, 60);
    const int evict_allocs = e.picture_allocs();
    e.stop();
    check(churned && evict_allocs <= threads + depth,
          "keeps buffers: 60 decodes into an over-full window make " + std::to_string(evict_allocs) +
              " pictures <= threads + depth");
}

// Over budget the ring evicts the frame needed LAST. Room for two of three
// wanted keeps the front one held throughout, however the other two churn;
// an eviction of the nearest drops frame 0 on every other put.
void test_slide_prefetch_evicts_farthest() {
    Fixture f = make_dataset("slide_evict", 64, 48, {"a", "b", "c", "d"});
    std::vector<mk::SlideFrame> frames;
    for (const std::string& k : f.keys)
        frames.push_back({(f.images / (k + ".jpg")).string(), (f.masks / (k + ".png")).string()});
    mk::SlidePrefetch p;
    p.set_target(32);
    p.set_byte_budget_for_test(5000);
    p.start(frames, 1);
    p.want(0, 3);
    check(wait_has(p, 0) && wait_decoded(p, 6), "evict: the over-full window churns");
    bool front_held = true;
    for (int i = 0; i < 200 && front_held; i++) {
        front_held = p.has(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const int churned = p.decoded();
    p.stop();
    check(front_held, "evict: the front of the window is never the frame evicted");
    check(churned > 6, "evict: puts kept arriving while the front was watched");
}

// A decode whose index left the window while it ran lands nowhere. Frame 0's
// mask is a FIFO, so its decode blocks in open() until the test opens the
// write end: the window provably moves while frame 0 is mid-decode.
void test_slide_prefetch_discards_stale() {
#ifndef _WIN32
    Fixture f = make_dataset("slide_stale", 64, 48, {"a", "b"});
    const fs::path fifo = f.masks / "a.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "stale: the fixture's mask is a FIFO");
    std::vector<mk::SlideFrame> frames;
    for (const std::string& k : f.keys)
        frames.push_back({(f.images / (k + ".jpg")).string(), (f.masks / (k + ".png")).string()});
    mk::SlidePrefetch p;
    p.set_target(32);
    p.start(frames, 1);
    p.want(0, 1);
    // O_NONBLOCK: the write end opens only once the worker holds the read end.
    bool in_flight = false;
    for (int i = 0; i < 3000 && !in_flight; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { in_flight = true; p.want(1, 1); close(fd); }
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(in_flight && p.decoded() == 0, "stale: fixture check: frame 0 was mid-decode when the window moved");
    // Each open-and-close hands the blocked reader an EOF; the mask fails to
    // load and frame 0 decodes as the bare photo, which must then be discarded.
    for (int i = 0; i < 3000 && p.decoded() < 1; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(p.decoded() >= 1 && !p.has(0), "stale: a decode that left the window is not put");
    check(wait_has(p, 1), "stale: the window's own frame still lands");
    p.stop();
#endif
}

// halt() is stop() without the join: a decode in flight finishes on its own
// and is never put, and a later stop() joins it.
void test_slide_prefetch_halt() {
#ifndef _WIN32
    Fixture f = make_dataset("slide_halt", 64, 48, {"a", "b"});
    const fs::path fifo = f.masks / "a.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "halt: the fixture's mask is a FIFO");
    std::vector<mk::SlideFrame> frames;
    for (const std::string& k : f.keys)
        frames.push_back({(f.images / (k + ".jpg")).string(), (f.masks / (k + ".png")).string()});
    mk::SlidePrefetch p;
    p.set_target(32);
    p.start(frames, 2);
    p.want(0, 2);
    check(wait_has(p, 1), "halt: fixture: b is held");
    bool in_flight = false;
    for (int i = 0; i < 3000 && !in_flight; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { in_flight = true; close(fd); }
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(in_flight && p.decoded() == 1, "halt: fixture: a's decode is in flight");
    std::atomic<bool> done{false};
    std::thread halter([&] { p.halt(); done = true; });
    for (int i = 0; i < 500 && !done; i++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool returned = done;
    for (int i = 0; i < 3000 && !done; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    halter.join();
    check(returned, "halt: returns while a decode is in flight");
    check(!p.running() && !p.has(1) && p.bytes() == 0, "halt: stopped, and the ring is dropped");
    for (int i = 0; i < 3000 && p.decoded() < 2; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(p.decoded() == 2 && !p.has(0) && p.bytes() == 0, "halt: the decode in flight is not put");
    p.want(1, 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    check(p.decoded() == 2 && !p.has(1), "halt: a halted pool decodes nothing more");
    p.stop();
    p.start(frames, 1);
    p.want(1, 1);
    check(wait_has(p, 1), "halt: start after a halt decodes again");
    p.stop();
#endif
}

// ---------------------------------------------------------------------------
// The slideshow through the session, driven by a fake clock
// ---------------------------------------------------------------------------

void test_session_slideshow() {
    Fixture f = make_dataset("slideshow", 64, 48, {"a", "b", "c", "d"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err), "open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    check(s.doc()->dirty(), "fixture: dirty before play");
    s.set_slide_fps(10.0f);
    check(!s.animating() && !s.slideshow_playing(), "not animating before play");
    s.start_slideshow();
    check(s.slide_fresh(), "play marks its press frame, so the key that pressed Play does not stop it");
    check(s.animating() && s.slideshow_playing() && s.doc() == nullptr, "playing, document released");
    check(s.frame_pixels() == nullptr, "playing, the frame's pixels are released");
    settle(s);
    check(fs::exists(f.layer / "a.drop.png"), "the dirty frame was saved on play");
    gui::Picture pic;
    // `at` is held fixed while a picture is awaited, so the shown times, and
    // with them the reported rate and gap, are the ones driven here.
    int shown_i = -1;
    auto show_at = [&](double at, const std::string& what) {
        const int wanted = (shown_i + 1) % 4;
        bool shown = false;
        for (int i = 0; i < 400 && !shown; i++) {
            shown = s.slideshow_tick(at, 32, pic);
            if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        check(shown && s.slide_index() == wanted, what);
        shown_i = s.slide_index();
    };
    show_at(100.0, "frame 0 shown first");
    check(pic.w == 32 && pic.h == 24, "pane-sized picture");
    // Unflipped, synth_mask drops the corner and keeps the ellipse centre.
    const size_t mid = ((size_t)12 * 32 + 16) * 3;
    check(pic.rgb.size() > mid && pic.rgb[0] >= 150 && pic.rgb[mid] < 150,
          "slideshow: the dropped corner is tinted and the kept centre is not");
    check(!s.slideshow_tick(100.05, 32, pic), "not due yet at 10 fps");
    check(s.slide_index() == 0, "a tick that is not due does not advance the frame");
    // Driven at 0.2 s against a 10 fps clock: the rate reported must be the
    // achieved 5 fps, not the set one.
    for (int k = 1; k < 6; k++)
        show_at(100.0 + 0.2 * k, "frame " + std::to_string(k % 4) + " at the driven cadence");
    check(s.slider_index() == s.slide_index(), "the frame slider follows the frame shown");
    const double fps = s.slide_shown_fps();
    check(fps > 4.95 && fps < 5.05, "shown fps is the driven cadence: " + std::to_string(fps));
    const double gap = s.slide_max_gap_ms();
    check(gap > 195.0 && gap < 205.0, "the longest gap is one period: " + std::to_string(gap));
    // 300 ms, not the 1.3 s since play began nor the 1.1 s sum of the gaps.
    show_at(101.3, "frame after a 300 ms stall");
    const double stall = s.slide_max_gap_ms();
    check(stall > 295.0 && stall < 305.0, "the 300 ms stall is the max, not a sum: " + std::to_string(stall));

    // A real 10 ms per frame gives the pool time to re-decode the frame just
    // taken if the window's front is the frame shown: 40 decodes, not 20.
    // Four frames against a depth of 3; re-derive the band if either changes.
    const int d0 = s.slide_decoded();
    for (int k = 0; k < 20; k++) {
        show_at(101.5 + 0.2 * k, "steady frame " + std::to_string(k));
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const int cost = s.slide_decoded() - d0;
    check(cost >= 20 && cost <= 30, "20 frames cost about 20 decodes, not 40: " + std::to_string(cost));

    s.stop_slideshow();
    check(!s.animating() && !s.slideshow_playing(), "stopped");
    // -1 until a stop is timed, so a dropped measurement fails here.
    check(s.slide_stop_ms() >= 0.0, "the join was timed: " + std::to_string(s.slide_stop_ms()));
    settle(s);
    const int last = shown_i;
    check(s.doc() && s.frame_index() == last && s.doc()->key() == f.keys[(size_t)last],
          "stop lands on the frame shown");
    // A second playback, at a rate that is no default: it restarts from the
    // frame stopped on, follows the new clock, and forgets the last one's gap.
    s.set_slide_fps(20.0f);
    s.start_slideshow();
    check(s.slideshow_playing() && s.frame_pixels() == nullptr, "second play: playing");
    check(s.slide_join_ms() >= 0.0, "second play: the start's join was timed: " + std::to_string(s.slide_join_ms()));
    shown_i = (last + 3) % 4;
    show_at(200.0, "second play: starts on the frame it stopped on");
    check(!s.slideshow_tick(200.04, 32, pic), "second play: not due after 40 ms at 20 fps");
    show_at(200.06, "second play: due after 50 ms at 20 fps");
    check(s.slide_max_gap_ms() > 55.0 && s.slide_max_gap_ms() < 65.0,
          "second play: the gap starts again from zero: " + std::to_string(s.slide_max_gap_ms()));
    s.stop_slideshow();
    settle(s);
    s.close();
    check(s.slide_stop_ms() < 0.0 && s.slide_max_gap_ms() == 0.0 && s.slide_shown_fps() == 0.0,
          "close forgets the last playback's numbers");
    s.set_slide_fps(1.0f);
    const float lo = s.slide_fps();
    s.set_slide_fps(100.0f);
    check(lo == 5.0f && s.slide_fps() == 30.0f, "the rate is clamped to 5..30 fps");
}

// The operator's masks are 255 = DROP: the slideshow tints what the trainer
// drops, so synth_mask's dropped corner, read flipped, plays untinted.
void test_session_slideshow_flipped() {
    Fixture f = make_dataset("slideshow_flipped", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), true, err),
          "slideshow flipped: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 400 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 32, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(shown && !pic.empty() && pic.rgb[0] < 150,
          "slideshow flipped: the file's dropped corner plays as kept, untinted");
    // A flag that threw the mask away would leave the centre untinted too.
    const size_t mid = ((size_t)12 * 32 + 16) * 3;
    check(shown && pic.rgb.size() > mid && pic.rgb[mid] >= 150,
          "slideshow flipped: the file's kept centre plays as dropped, tinted");
    s.stop_slideshow();
    settle(s);
    s.close();
}

// The window is what the ring's bytes hold ahead of the frame shown: before a
// picture reports its source a 4096 target is costed at 4096^2 x 3 (one fits),
// then 64 x 48 frames fit all three others; a slot count would give 4 both times.
void test_session_slideshow_window() {
    Fixture f = make_dataset("slideshow_window", 64, 48, {"a", "b", "c", "d"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "slideshow window: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 400 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 4096, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(shown && pic.w == 64 && s.slide_window() == 1,
          "slideshow window: unknown source at 4096 holds one: " + std::to_string(s.slide_window()));
    s.slideshow_tick(100.01, 4096, pic);
    check(s.slide_window() == 3,
          "slideshow window: a known 64x48 source holds the other three: " + std::to_string(s.slide_window()));
    s.stop_slideshow();
    settle(s);
    s.close();
}

// Stop runs inside an ImGui frame, so it must not wait out an 8K decode (321
// to 536 ms in the app when it joined). A FIFO mask holds b's decode in flight.
void test_session_slideshow_stop_does_not_join() {
#ifndef _WIN32
    Fixture f = make_dataset("slideshow_stop", 64, 48, {"a", "b"});
    const fs::path fifo = f.masks / "b.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "slideshow stop: fixture: b's mask is a FIFO");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "slideshow stop: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 400 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 32, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    bool in_flight = false;
    for (int i = 0; i < 3000 && !in_flight; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { in_flight = true; close(fd); }
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(shown && in_flight, "slideshow stop: fixture: a shown, b's decode in flight");
    std::atomic<bool> done{false};
    std::thread stopper([&] { s.stop_slideshow(); done = true; });
    for (int i = 0; i < 500 && !done; i++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool returned = done;
    for (int i = 0; i < 3000 && !done; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    stopper.join();
    check(returned && s.slide_stop_ms() >= 0.0 && s.slide_stop_ms() < 100.0,
          "slideshow stop: returns while a decode is in flight: " + std::to_string(s.slide_stop_ms()));
    check(!s.slideshow_playing(), "slideshow stop: stopped");
    for (int i = 0; i < 300; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    settle(s);
    check(s.doc() && s.doc()->key() == "a", "slideshow stop: lands on a, the frame shown");
    s.close();
#endif
}

// The pending frame is not the frame on screen: a stop while b is still
// decoding must open a, which is what the operator is looking at.
void test_session_slideshow_stop_on_screen() {
#ifndef _WIN32
    Fixture f = make_dataset("slideshow_on_screen", 64, 48, {"a", "b", "c"});
    const fs::path fifo = f.masks / "b.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "on screen: fixture: b's mask is a FIFO");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "on screen: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 400 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 32, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    bool in_flight = false;
    for (int i = 0; i < 3000 && !in_flight; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { in_flight = true; close(fd); }
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    const bool due = s.slideshow_tick(100.2, 32, pic);
    check(shown && in_flight && !due, "on screen: fixture: a shown, b due and still decoding");
    check(s.slide_index() == 0 && s.slider_index() == 0, "on screen: a is still the frame shown");
    s.stop_slideshow();
    settle(s);
    check(s.doc() && s.doc()->key() == "a", "on screen: stop opens a, not b still decoding");
    for (int i = 0; i < 300; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    s.close();
#endif
}

// Stop opens the frame at the file's size through load_frame, never the
// slideshow's picture: the editor paints on the whole frame.
void test_session_slideshow_stop_reloads_full_resolution() {
    Fixture f = make_dataset("slideshow_full_res", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "full res: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 400 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 32, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    check(shown && pic.w == 32 && pic.h == 24, "full res: fixture: the slideshow shows a at 32x24");
    s.stop_slideshow();
    settle(s);
    const auto px = s.frame_pixels();
    check(px && px->size() == (size_t)64 * 48 * 3 && px->size() != pic.rgb.size(),
          "full res: stop installs the whole 64x48 frame, not the 32x24 picture");
    check(s.doc() && s.doc()->width() == 64 && s.doc()->height() == 48,
          "full res: the document is loaded at the file's size");
    s.close();
}

// The decoder budget follows the dataset's largest frame, not the one Play
// starts on: two 1080p frames in the root, one 8K frame under cam1.
void test_session_slideshow_threads_from_largest_frame() {
    const fs::path root = scratch("slideshow_mixed");
    const fs::path images = root / "images", masks = root / "masks";
    const std::vector<uint8_t> hd((size_t)1920 * 1080 * 3, 90), big((size_t)7680 * 3840 * 3, 90);
    write_jpg_rgb(images / "a.jpg", 1920, 1080, hd);
    write_jpg_rgb(images / "b.jpg", 1920, 1080, hd);
    write_jpg_rgb(images / "cam1" / "c.jpg", 7680, 3840, big);
    std::error_code ec;
    fs::create_directories(masks, ec);
    const unsigned hw = std::thread::hardware_concurrency();
    const int want = mk::slide_threads(7680, 3840, hw);
    check(want != mk::slide_threads(1920, 1080, hw),
          "mixed sizes: fixture: this machine budgets 8K and 1080p differently");
    mk::MaskSession s;
    std::string err;
    check(s.open(root.string(), images.string(), masks.string(), false, err), "mixed sizes: open: " + err);
    settle(s);
    const auto px = s.frame_pixels();
    check(s.doc() && s.doc()->width() == 1920 && px && px->size() == (size_t)1920 * 1080 * 3,
          "mixed sizes: fixture: Play starts on a 1080p frame");
    s.start_slideshow();
    check(s.slideshow_playing() && s.slide_threads() == want,
          "mixed sizes: Play on a 1080p frame budgets for the 8K one: " + std::to_string(s.slide_threads()));
    s.stop_slideshow();
    settle(s);
    s.close();
}

// An undecodable frame is counted as shown and skipped: the
// picture on screen stays the last good one, and playback moves on.
void test_session_slideshow_skips_undecodable() {
    Fixture f = make_dataset("slideshow_skip", 64, 48, {"a", "b", "c"});
    {
        std::FILE* bad = std::fopen((f.images / "b.jpg").string().c_str(), "wb");
        std::fputs("not a jpeg", bad);
        std::fclose(bad);
    }
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "skip: open: " + err);
    settle(s);
    s.start_slideshow();
    gui::Picture pic;
    auto tick_at = [&](double at) {
        bool got = false;
        for (int i = 0; i < 400 && !got; i++) {
            got = s.slideshow_tick(at, 32, pic);
            if (!got) std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return got;
    };
    check(tick_at(100.0) && !pic.empty() && s.slide_index() == 0, "skip: a shown");
    check(tick_at(100.1) && pic.empty() && s.slide_index() == 0,
          "skip: b cannot be decoded: counted, and a stays on screen");
    check(tick_at(100.2) && !pic.empty() && s.slide_index() == 2, "skip: c follows the skipped frame");
    s.stop_slideshow();
    settle(s);
    s.close();
}

// Closing mid-playback (the OS window, not ImGui input) must end playback: a
// reopen must not come up in the slideshow branch against the old pool.
void test_session_slideshow_close_while_playing() {
    Fixture f = make_dataset("slideshow_close", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close playing: open: " + err);
    settle(s);
    s.start_slideshow();
    check(s.slideshow_playing(), "close playing: fixture: playing");
    s.close();
    check(!s.slideshow_playing() && !s.animating(), "close playing: close ends playback");
    check(!s.slide_pool_running(), "close playing: the decode pool is stopped");
    check(s.slide_join_ms() >= 0.0, "close playing: close timed its join: " + std::to_string(s.slide_join_ms()));
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close playing: reopen: " + err);
    settle(s);
    check(!s.slideshow_playing() && s.doc() != nullptr, "close playing: a reopen opens a frame, not a slideshow");
    s.close();
}

// Play queues the edited frame's save; a decode beating it plays the pre-edit
// composite. The save is held on the worker until the ticks have seen it, and
// both boxes paint the opposite of the file's state, so a stale picture fails both.
void test_session_slideshow_plays_the_edit(bool flipped) {
    const std::string arm = flipped ? "plays the edit (flipped): " : "plays the edit: ";
    Fixture f = make_dataset(flipped ? "slideshow_edit_f" : "slideshow_edit", 4000, 3000,
                             {"a", "b", "c", "d"});
    std::atomic<bool> hold{false}, held{false};
    mk::MaskSession s;
    s.set_worker_hook_for_test([&](bool end) {
        if (end || !hold.load()) return;
        held = true;
        while (hold.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err), arm + "open: " + err);
    settle(s);
    mk::Mapping m;
    gui::ShapeStroke centre, corner;
    centre.kind = corner.kind = gui::ShapeKind::Box;
    centre.pts = std::vector<float>{1800.0f, 1300.0f, 2200.0f, 1700.0f};
    corner.pts = std::vector<float>{0.0f, 0.0f, 300.0f, 300.0f};
    s.commit_stroke(centre, flipped ? mk::Paint::ForceKeep : mk::Paint::ForceDrop, m);
    s.commit_stroke(corner, flipped ? mk::Paint::ForceDrop : mk::Paint::ForceKeep, m);
    hold = true;
    s.start_slideshow();
    // Seconds, not a spin count: TSan at -O1 took longer than 2000 x 2 ms.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    for (int i = 0; i < 30000 && !held.load(); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    gui::Picture pic;
    bool shown = false;
    for (int i = 0; i < 500 && !shown; i++) {
        shown = s.slideshow_tick(100.0, 400, pic);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(held.load() && s.slideshow_playing() && !shown,
          arm + "no frame is shown while Play's save is held on the worker");
    hold = false;
    while (!shown && std::chrono::steady_clock::now() < deadline) {
        shown = s.slideshow_tick(100.0, 400, pic);
        if (!shown) std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    check(shown && pic.w == 400 && pic.h == 300, arm + "first frame shown at 400 x 300");
    // Tinted is r/3+150 >= 150; synth_rgb's red is 127 at the centre, 3 at the corner.
    const size_t mid = ((size_t)150 * 400 + 200) * 3, cor = ((size_t)5 * 400 + 5) * 3;
    const bool painted = !pic.empty() && pic.rgb.size() > std::max(mid, cor);
    const bool mid_tinted = painted && pic.rgb[mid] >= 150;
    const bool cor_tinted = painted && pic.rgb[cor] >= 150;
    check(painted && mid_tinted == !flipped,
          arm + "the centre plays as painted: red " + std::to_string(painted ? pic.rgb[mid] : -1));
    check(painted && cor_tinted == flipped,
          arm + "the corner plays as painted: red " + std::to_string(painted ? pic.rgb[cor] : -1));
    s.stop_slideshow();
    settle(s);
    s.close();
}

// A load still on the worker would install a document mid-playback.
void test_session_slideshow_waits_for_worker() {
#ifndef _WIN32
    Fixture f = make_dataset("slideshow_busy", 64, 48, {"a", "b"});
    const fs::path fifo = f.masks / "b.png";
    fs::remove(fifo);
    check(mkfifo(fifo.c_str(), 0600) == 0, "slideshow busy: fixture: b's mask is a FIFO");
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "slideshow busy: open: " + err);
    settle(s);
    s.go_to(1);
    bool blocked = false;
    for (int i = 0; i < 3000 && !blocked; i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { blocked = true; close(fd); }
        else std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    check(blocked && !s.idle(), "slideshow busy: fixture: b's load is on the worker");
    s.start_slideshow();
    check(!s.slideshow_playing(), "slideshow busy: Play waits for the worker");
    for (int i = 0; i < 3000 && !s.idle(); i++) {
        const int fd = open(fifo.c_str(), O_WRONLY | O_NONBLOCK);
        if (fd >= 0) close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    settle(s);
    s.stop_slideshow();
    s.close();
#endif
}

}  // namespace

// ---------------------------------------------------------------------------
// SAM assist: lifetime and teardown
// ---------------------------------------------------------------------------

// Build, then read the flag, then publish: an Esc landing while the stencil is
// built must beat the result it would otherwise hand over.
void test_publish_unless_cancelled() {
    bool cancel = false;
    int built = 0, published = 0;
    std::string got;
    auto build = [&] {
        built++;
        mk::SamResult r;
        r.frame_key = "k";
        return r;
    };
    auto publish = [&](mk::SamResult r) {
        published++;
        got = r.frame_key;
    };
    const bool ok = mk::publish_unless_cancelled(build, [&] { return cancel; }, publish);
    check(ok && built == 1 && published == 1 && got == "k",
          "publish tail: an uncancelled job builds and publishes once");
    built = published = 0;
    const bool late = mk::publish_unless_cancelled(
        [&] {
            cancel = true;
            return build();
        },
        [&] { return cancel; }, publish);
    check(!late && built == 1 && published == 0,
          "publish tail: an Esc during the build wins over the result");
}

// Closing forgets every piece of SAM state: the clicks name frames of a session
// that is gone, and the next open must start from nothing.
void test_session_close_forgets_sam() {
    Fixture f = make_dataset("sam_close_forgets", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close sam: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam_prompt().dilate_ratio = 0.3f;
    s.sam_prompt_point(21.0f, 22.0f, mk::Paint::ForceDrop);
    s.sam_prompt_started(20.0f, 20.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 20.0f, 20.0f, 6.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 7.0);
    s.sam_pump();
    s.sam().refuse("a stale error");
    check(s.sam_click_x() == 21.0f && s.sam_click_y() == 22.0f,
          "close sam: the fixture has a last click point");
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 40.0f, 20.0f, 4.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 7.0);
    check(s.sam_click_count() == 1 && s.sam_results() == 1 && s.sam_last_area() > 0 &&
              s.sam_last_detections() == 1 && s.sam_last_job_ms() == 7.0 &&
              s.sam_held_bytes() > 0 && s.sam_margin() == 0.3f && !s.sam_error().empty() &&
              s.sam_model_path() == "/m/a.ggml",
          "close sam: the fixture has clicks, a result, a held add, a margin and an error");
    s.close();
    check(s.sam_click_count() == 0 && s.sam_object_count() == 0,
          "close sam: the editor's clicks go with it");
    check(s.sam_margin() == -1.0f, "close sam: the editor's own prompt state goes with it");
    check(s.sam_results() == 0 && s.sam_dropped() == 0 && s.sam_last_area() == 0 &&
              s.sam_last_detections() == 0 && s.sam_last_ms() == 0.0 &&
              s.sam_last_job_ms() == 0.0 && s.sam_last_score() == 0.0f &&
              s.sam_empty_note() == nullptr,
          "close sam: the result counters start again at zero");
    check(s.sam_model_path().empty() && !s.sam_has_model(),
          "close sam: the model is forgotten until the next open pushes it");
    check(s.sam_error().empty() && s.sam_status().empty() && s.sam_vram_mib() == -1.0,
          "close sam: no status, error or session survives");
    check(!s.sam_margin_reapplies() && s.sam_held_bytes() == 0,
          "close sam: nothing is left to re-apply the margin to");
    check(s.sam_click_x() == -1.0f && s.sam_click_y() == -1.0f,
          "close sam: the last click point goes with it");
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close sam: reopen: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    check(s.sam_model_path() == "/m/a.ggml" && s.sam_click_count() == 0 && s.sam_results() == 0,
          "close sam: a reopen takes the model again, with no clicks");
}

// Undo takes the add off the top and redo puts back the same step, so the
// margin slider must reach it again; an edit in its place ends that for good.
void test_session_sam_margin_after_undo_redo() {
    Fixture f = make_dataset("sam_margin_undo_redo", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "undo redo margin: open: " + err);
    settle(s);
    const mk::AddRegion g = disc_region(64, 48, 30.0f, 24.0f, 6.0f);
    s.sam_prompt().dilate_ratio = 0.0f;
    sam_add(s, g, 0);
    const int h0 = s.doc()->history_size();
    const int64_t tight = s.sam_last_area();
    s.undo();
    s.sam_pump();
    check(!s.sam_margin_reapplies() && s.sam_held_bytes() > 0,
          "undo redo margin: an undone add takes no margin, but keeps its detection for a redo");
    s.redo();
    s.sam_pump();
    s.sam_prompt().dilate_ratio = 0.4f;
    check(s.sam_margin_reapplies(), "undo redo margin: the redone add can be re-applied");
    reapply(s);
    std::vector<mk::AddRegion> want{g};
    gui::Stencil st;
    mk::Rect b;
    int64_t grown = 0;
    mk::build_add_stencil(want, 64, 48, st, b, grown, 0.4f);
    check(grown > tight && s.sam_last_area() == grown && s.doc()->history_size() == h0,
          "undo redo margin: the slider re-applies to the redone add, in place");
    s.undo();
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    s.sam_pump();
    check(s.sam_held_bytes() == 0,
          "undo redo margin: an edit in the undone add's place lets go of its detection");
}

// "Matched nothing" is only true when nothing matched: a match the exception
// chips cleared entirely says so instead.
void test_session_sam_vetoed_all() {
    namespace em = spirula::i18n::msg::maskedit;
    Fixture f = make_dataset("sam_vetoed_all", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "vetoed: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    check(s.sam_empty_note() == nullptr, "vetoed: no note before any result");
    mk::AddRegion all;
    all.w = 64;
    all.h = 48;
    all.mask.assign((size_t)64 * 48, 255);
    const mk::AddRegion none = disc_region(64, 48, 20.0f, 20.0f, 0.0f);
    const std::string key = s.sam_frame_stamp();
    s.sam().post_result(key, {disc_region(64, 48, 20.0f, 20.0f, 6.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0, {all});
    s.sam_pump();
    check(s.sam_results() == 1 && s.sam_last_area() == 0 && s.sam_last_detections() == 1,
          "vetoed: the fixture matched once and the veto left nothing");
    check(s.sam_empty_note() == &em::sam_vetoed_all,
          "vetoed: a match the exceptions cleared entirely says the exceptions took it");
    s.sam().post_result(key, {none}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.sam_empty_note() == &em::sam_empty, "vetoed: a prompt that matched nothing says so");
    s.sam().post_result(key, {none}, 64, 48, mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0, {all});
    s.sam_pump();
    check(s.sam_empty_note() == &em::sam_empty,
          "vetoed: an exception over an empty match is still nothing matched");
    s.sam().post_result(key, {disc_region(64, 48, 20.0f, 20.0f, 6.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.sam_last_area() > 0 && s.sam_empty_note() == nullptr,
          "vetoed: a result that painted takes no note");
}

// Stands in for MaskSam's busy() and release(): `busy` is a job still inside a
// stage; each release records whether it came while busy.
struct FakeSamOps {
    bool busy = false;
    int releases = 0, releases_while_busy = 0;
    int sleep_ms = 0;
    mk::SamOps ops() {
        return {[this](mk::MaskSam&) { return busy; },
                [this](mk::MaskSam&) {
                    releases++;
                    releases_while_busy += busy ? 1 : 0;
                    if (sleep_ms) std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
                    return false;
                }};
    }
};

// A close mid-job must not join it on the UI thread: it parks the job, and the
// per-frame poll releases it only once the stage it is in has ended.
void test_session_close_parks_busy_job() {
    Fixture f = make_dataset("sam_close_parks", 64, 48, {"a"});
    FakeSamOps fake;
    mk::MaskSession s;
    s.set_sam_ops(fake.ops());
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "retire: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam();
    fake.busy = true;
    s.close();
    check(fake.releases == 0 && s.sam_retiring(),
          "retire: close never releases a busy job on the UI thread, it parks it");
    s.sam_poll_retiring();
    check(fake.releases == 0 && s.sam_retiring(), "retire: the poll waits while the job still runs");
    fake.busy = false;
    s.sam_poll_retiring();
    check(fake.releases == 1 && !s.sam_retiring(), "retire: the poll releases once the job is idle");
    s.sam_poll_retiring();
    check(fake.releases == 1, "retire: the poll releases once, not every frame");
}

// A SAM job, or a margin re-apply waiting to start, can still land on the
// source after propagate snapshots it, so propagate waits. Undo never reads
// the source, so it does not.
void test_propagate_waits_for_sam() {
    Fixture f = make_dataset("prop_sam", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png";
    const std::vector<uint8_t> original_b = file_bytes(b_png);
    FakeSamOps fake;
    mk::MaskSession s;
    s.set_sam_ops(fake.ops());
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "prop sam: open: " + err);
    settle(s);
    s.sam();   // a SAM half exists, as after a first prompt
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    fake.busy = true;
    check(s.sam_work_pending(), "prop sam: a running job is pending work");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(file_bytes(b_png) == original_b && !fs::exists(f.layer / "cam0" / "b.drop.png") &&
              !s.can_undo_propagate(),
          "prop sam: nothing propagates while a SAM job runs");
    fake.busy = false;
    s.sam_margin_changed();
    check(s.sam_work_pending(), "prop sam: a margin re-apply waiting to start is pending work");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(file_bytes(b_png) == original_b, "prop sam: nothing propagates while a re-apply waits");
    s.sam_pump();   // no add is held, so the waiting re-apply is dropped
    check(!s.sam_work_pending(), "prop sam: fixture: nothing is pending after the pump");
    s.propagate(mk::PropagateScope::Camera, 0, 0);
    settle(s);
    check(file_bytes(b_png) != original_b && s.can_undo_propagate(),
          "prop sam: with nothing pending it propagates");
    fake.busy = true;
    s.undo_propagate();
    settle(s);
    check(file_bytes(b_png) == original_b, "prop sam: a SAM job does not hold Undo propagate back");
    fake.busy = false;
    s.close();
}

// A slideshow and a SAM session never hold memory
// at once. A job or a waiting re-apply refuses Play; otherwise Play releases.
void test_slideshow_releases_sam() {
    Fixture f = make_dataset("slide_sam", 64, 48, {"a", "b", "c"});
    FakeSamOps fake;
    mk::MaskSession s;
    s.set_sam_ops(fake.ops());
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "slideshow vs SAM: open: " + err);
    settle(s);
    s.sam();   // a SAM half exists, as after a first prompt
    fake.busy = true;
    s.start_slideshow();
    check(!s.slideshow_playing() && fake.releases == 0 && s.doc() != nullptr,
          "slideshow vs SAM: a running SAM job refuses Play, and nothing is released");
    fake.busy = false;
    s.sam_margin_changed();
    s.start_slideshow();
    check(!s.slideshow_playing(), "slideshow vs SAM: a re-apply waiting to start refuses Play too");
    s.sam_pump();   // no add is held, so the waiting re-apply is dropped
    s.start_slideshow();
    check(s.slideshow_playing() && fake.releases == 1 && fake.releases_while_busy == 0,
          "slideshow vs SAM: starting a slideshow releases the SAM session: " + std::to_string(fake.releases));
    s.stop_slideshow();
    settle(s);
    s.close();
}

// An idle session is released by close() itself, not left to ~MaskSam, and a
// later close with no SAM reports 0 ms.
void test_session_close_releases_idle() {
    Fixture f = make_dataset("sam_close_idle", 64, 48, {"a"});
    FakeSamOps fake;
    fake.sleep_ms = 5;
    mk::MaskSession s;
    s.set_sam_ops(fake.ops());
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close idle: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam();
    s.close();
    check(fake.releases == 1 && !s.sam_retiring(),
          "close idle: close releases an idle session itself, not ~MaskSam");
    check(s.sam_close_ms() >= 5.0, "close idle: sam_close_ms covers that release");
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "close idle: reopen: " + err);
    settle(s);
    s.close();
    check(s.sam_close_ms() == 0.0 && fake.releases == 1,
          "close idle: a close with no SAM reports 0 ms, not the last close's");
}

// Every other inference user drains the parked job first, even mid-stage: when
// sam_yield(), a reopen or destruction returns, no MaskSam is left unreleased.
void test_session_retiring_drains() {
    Fixture f = make_dataset("sam_retire_drains", 64, 48, {"a"});
    std::string err;
    {
        FakeSamOps fake;
        mk::MaskSession s;
        s.set_sam_ops(fake.ops());
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
              "drain: open: " + err);
        settle(s);
        s.set_sam_model("/m/a.ggml", true);
        s.sam();
        fake.busy = true;
        s.close();
        s.sam_yield();
        check(fake.releases == 1 && fake.releases_while_busy == 1 && !s.sam_retiring(),
              "drain: sam_yield releases the parked job even mid-stage");
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
              "drain: reopen: " + err);
        settle(s);
        s.set_sam_model("/m/a.ggml", true);
        s.sam();
        s.close();
        check(s.sam_retiring() && fake.releases == 1, "drain: a second busy close parks again");
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
              "drain: reopen 2: " + err);
        check(fake.releases == 2 && !s.sam_retiring(),
              "drain: a reopen releases the parked job before its own session starts");
        settle(s);
        s.set_sam_model("/m/a.ggml", true);
        s.sam();
        s.sam_yield();
        check(fake.releases == 3, "drain: sam_yield releases the live session as well");
    }
    FakeSamOps fake;
    {
        mk::MaskSession s;
        s.set_sam_ops(fake.ops());
        check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
              "drain: open 3: " + err);
        settle(s);
        s.set_sam_model("/m/a.ggml", true);
        s.sam();
        fake.busy = true;
        s.close();
        check(s.sam_retiring(), "drain: the fixture parked a job before destruction");
    }
    check(fake.releases == 1, "drain: destroying the session releases the parked job");
}

// MaskDoc serials restart per document, so another frame's first edit can carry
// the add's serial: only the stamp tells them apart.
void test_session_sam_held_other_frame() {
    Fixture f = make_dataset("sam_held_other_frame", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "held other frame: open: " + err);
    settle(s);
    sam_add(s, disc_region(64, 48, 30.0f, 24.0f, 6.0f), 0);
    const uint64_t add_step = s.doc()->top_step();
    check(s.sam_held_bytes() > 0, "held other frame: the add holds its detection");
    s.go_to(1);
    settle(s);
    s.doc()->paint(mk::Paint::ForceKeep, box_stencil(64, 48, 0, 0, 4, 4), mk::Rect{0, 0, 4, 4});
    check(s.doc()->key() == "b" && s.doc()->top_step() == add_step,
          "held other frame: fixture: frame b's first edit has the add's serial");
    s.sam_pump();
    check(s.sam_held_bytes() == 0,
          "held other frame: another frame's edit with the same serial does not keep it");
}

// A Clear while a job runs: the job's result lands as a plain add, so the next
// click on the same number adds beside it instead of replacing it for good.
void test_session_sam_clear_in_flight() {
    Fixture f = make_dataset("sam_clear_in_flight", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "clear in flight: open: " + err);
    settle(s);
    const size_t first = (size_t)20 * 64 + 12, second = (size_t)24 * 64 + 48;
    const int h0 = s.doc()->history_size();
    s.sam_prompt_started(12.0f, 20.0f, true);
    s.sam_objects_edited();
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 12.0f, 20.0f, 5.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->drop()[first] == 255, "clear in flight: the in-flight result still lands");
    s.sam_prompt_started(48.0f, 24.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 48.0f, 24.0f, 5.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    s.sam_pump();
    check(s.doc()->history_size() == h0 + 2 && s.doc()->drop()[first] == 255 &&
              s.doc()->drop()[second] == 255,
          "clear in flight: the next click on the same number adds, it does not replace");
    s.doc()->undo();
    check(s.doc()->drop()[first] == 255 && s.doc()->drop()[second] == 0,
          "clear in flight: one undo takes the new add and leaves the first drop");
}

// Every prompt asks GuiApp's device gate first, and loads on what it names; a
// failed freeze refuses with its own sentence, and a paused prompt never asks.
void test_session_sam_device_gate() {
    namespace em = spirula::i18n::msg::maskedit;
    Fixture f = make_dataset("sam_device_gate", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "device gate: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    int asked = 0;
    bool frozen = false;
    s.set_sam_device_gate([&](std::string& device, std::string& error) {
        asked++;
        if (!frozen) {
            error = "device conflict";
            return false;
        }
        device = "uuid:0123";
        return true;
    });
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) &&
              s.sam_error() == "device conflict" && asked == 1 && s.sam_click_count() == 0,
          "device gate: a failed freeze refuses the click with its own sentence");
    check(!s.sam_prompt_text("door") && s.sam_error() == "device conflict" && asked == 2,
          "device gate: a failed freeze refuses a phrase too");
    s.set_sam_blocker(em::sam_blocked_run.get());
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    check(asked == 2, "device gate: a paused prompt never freezes the device");
    s.set_sam_blocker("");
    frozen = true;
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    check(asked == 3 && s.sam().device() == "uuid:0123" &&
              s.sam_error() == em::sam_unavailable_build.get(),
          "device gate: a frozen device is handed to SAM before the job starts");
}

// Between a model change and its release no prompt may start: its job would
// run on the new model and its result be counted as the old one's, dropped.
void test_session_sam_release_pending_refuses() {
    Fixture f = make_dataset("sam_release_pending", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "release pending: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam();
    s.set_sam_model("/m/b.ggml", true);
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) && s.sam_error().empty() &&
              s.sam_click_count() == 0,
          "release pending: a click before the release never reaches the job");
    check(!s.sam_prompt_text("door") && s.sam_error().empty(),
          "release pending: a phrase before the release never reaches the job");
    s.sam_pump();
    check(!s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop) &&
              s.sam_error() == spirula::i18n::msg::maskedit::sam_unavailable_build.get(),
          "release pending: once released, the click reaches the job");
}

// A slider release while SAM is paused re-applies the margin and leaves the
// pause's reason standing in the error line.
void test_session_sam_margin_keeps_pause() {
    namespace em = spirula::i18n::msg::maskedit;
    Fixture f = make_dataset("sam_margin_pause", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "margin pause: open: " + err);
    settle(s);
    s.set_sam_model("/m/a.ggml", true);
    s.sam_prompt().dilate_ratio = 0.0f;
    sam_add(s, disc_region(64, 48, 30.0f, 24.0f, 6.0f), 0);
    const int64_t tight = s.sam_last_area();
    const std::string run = em::sam_blocked_run.get();
    s.set_sam_blocker(run);
    s.sam_prompt_point(1.0f, 1.0f, mk::Paint::ForceDrop);
    check(s.sam_error() == run, "margin pause: the paused click put its reason in error");
    s.sam_prompt().dilate_ratio = 0.4f;
    reapply(s);
    check(s.sam_last_area() > tight, "margin pause: the margin re-applied while paused");
    check(s.sam_error() == run, "margin pause: the re-apply leaves the pause's reason standing");
}

// An undo while a margin re-apply is in flight drops its landing; the redo that
// brings the add back must find its detections again.
void test_session_sam_redo_after_dropped_margin() {
    Fixture f = make_dataset("sam_redo_dropped_margin", 64, 48, {"a"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "dropped margin: open: " + err);
    settle(s);
    s.sam_prompt().dilate_ratio = 0.0f;
    sam_add(s, disc_region(64, 48, 30.0f, 24.0f, 6.0f), 0);
    s.sam_prompt().dilate_ratio = 0.4f;
    s.sam_margin_changed();
    s.sam_pump();
    check(s.sam_held_bytes() == 0, "dropped margin: the detections went into the margin job");
    const int dropped = s.sam_dropped();
    s.undo();
    s.sam_pump();
    check(s.sam_dropped() == dropped + 1, "dropped margin: the landing after an undo is dropped");
    s.redo();
    s.sam_pump();
    check(s.sam_held_bytes() > 0 && s.sam_margin_reapplies(),
          "dropped margin: the redone add has its detections back and can re-apply");
}

// As for revert, so for propagate and its undo: a target's layers
// are replaced, so its clicks would re-prompt SAM with the correction just lost.
void propagate_forgets_clicks_arm(bool flipped) {
    const std::string tag = flipped ? "propagate clicks (flipped): " : "propagate clicks: ";
    Fixture f = make_dataset(flipped ? "prop_clicks_f" : "prop_clicks", 64, 48,
                             {"cam0/a", "cam0/b", "cam0/c"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), flipped, err),
          tag + "open: " + err);
    settle(s);
    gui::MaskSettings& p = s.sam_prompt();
    for (int i : {1, 2, 0}) {
        s.go_to(i);
        settle(s);
        s.sam_prompt_started(10.0f + (float)i, 12.0f, true);
    }
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    check(s.frame_index() == 0 && clicks_on(p, 0, 0) == 1 && clicks_on(p, 1, 0) == 1 &&
              clicks_on(p, 2, 0) == 1,
          tag + "fixture: one click on each of a, b and c, a open and edited");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    check(s.can_undo_propagate(), tag + "fixture: a propagated onto b");
    check(clicks_on(p, 1, 0) == 0, tag + "propagate forgets the target's clicks");
    check(clicks_on(p, 0, 0) == 1 && clicks_on(p, 2, 0) == 1 && p.clicks.size() == 2,
          tag + "propagate keeps the source's and other frames' clicks");
    // No UI path puts a click on b now (opening b drops the record), so the
    // undo's own erase is pinned with a planted one.
    gui::MaskClick planted = p.clicks.front();
    planted.frame = 1;
    p.clicks.push_back(planted);
    s.undo_propagate();
    settle(s);
    check(!s.can_undo_propagate() && s.error().empty(), tag + "fixture: the undo ran: " + s.error());
    check(clicks_on(p, 1, 0) == 0, tag + "undo propagate forgets the target's clicks");
    check(clicks_on(p, 0, 0) == 1 && clicks_on(p, 2, 0) == 1 && p.clicks.size() == 2,
          tag + "undo propagate keeps the source's and other frames' clicks");
    s.close();
}

void test_propagate_forgets_clicks() {
    propagate_forgets_clicks_arm(false);
    propagate_forgets_clicks_arm(true);
}

// A result published but not yet taken is SAM work: Play would discard it and
// propagate would copy the source without it.
void test_sam_result_ready_is_pending() {
    Fixture f = make_dataset("sam_ready", 64, 48, {"cam0/a", "cam0/b"});
    const fs::path b_png = f.masks / "cam0" / "b.png";
    const std::vector<uint8_t> original_b = file_bytes(b_png);
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "sam ready: open: " + err);
    settle(s);
    s.sam_prompt_started(30.0f, 24.0f, true);
    s.sam().post_result(s.sam_frame_stamp(), {disc_region(64, 48, 30.0f, 24.0f, 6.0f)}, 64, 48,
                        mk::Paint::ForceDrop, 0.0f, 0.9f, 1.0);
    check(!s.sam_busy() && s.sam().has_result(), "sam ready: fixture: a result waits, no job runs");
    check(s.sam_work_pending(), "sam ready: a result not yet taken is pending work");
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    check(file_bytes(b_png) == original_b && !s.can_undo_propagate(),
          "sam ready: propagate is refused while it waits");
    s.start_slideshow();
    check(!s.slideshow_playing() && s.sam().has_result(),
          "sam ready: Play is refused and the result is kept");
    s.stop_slideshow();   // a no-op unless Play wrongly started
    settle(s);
    const int h0 = s.doc() ? s.doc()->history_size() : -1;
    s.sam_pump();
    check(!s.sam_work_pending() && s.doc() && s.doc()->history_size() == h0 + 1,
          "sam ready: once taken it lands and nothing is pending");
    s.close();
}

// Propagate's own save of the source is a save: a failure outlives the loads
// after it, as save()'s does, and close() still reports it.
void test_propagate_source_save_failure_sticks() {
    Fixture f = make_dataset("prop_src_fail", 64, 48, {"cam0/a", "cam0/b"});
    mk::MaskSession s;
    std::string err, logged;
    s.set_log([&logged](const std::string& t) { logged = t; });
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "prop source fail: open: " + err);
    settle(s);
    wait_scanned(s);
    for (int i = 0; i < 2000 && !fs::exists(f.layer / mk::kKeptFileName); i++)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::error_code ec;
    fs::remove_all(f.layer, ec);
    fs::create_directories(f.layer.parent_path(), ec);
    const std::vector<uint8_t> one = {'x'};
    check(mk::write_file_atomic(f.layer.string(), one.data(), one.size()),
          "prop source fail: fixture: the layer root is a file");
    mk::Mapping m;
    gui::ShapeStroke box;
    box.kind = gui::ShapeKind::Box;
    box.pts = std::vector<float>{4.0f, 4.0f, 14.0f, 14.0f};
    s.commit_stroke(box, mk::Paint::ForceDrop, m);
    s.propagate(mk::PropagateScope::Next, 0, 0);
    settle(s);
    const std::string failed = s.error();
    check(failed.find(f.layer.string()) != std::string::npos && !s.can_undo_propagate(),
          "prop source fail: fixture: the source's save failed, nothing propagated: " + failed);
    fs::remove(f.layer, ec);   // writable again, so only the load below can clear it
    fs::create_directories(f.layer, ec);
    s.revert_open_frame();
    settle(s);
    check(s.doc() && !s.doc()->dirty() && s.error() == failed,
          "prop source fail: the failure outlives a later load: " + s.error());
    s.close();
    check(logged == failed, "prop source fail: close() reports it: " + logged);
}

// The Polygon half of Play's gate is the panel's (EditTool needs ImGui); the
// pen's is the session's own.
void test_slideshow_refuses_open_path() {
    Fixture f = make_dataset("slide_path", 64, 48, {"a", "b"});
    mk::MaskSession s;
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "slide path: open: " + err);
    settle(s);
    gui::ViewportInput in;
    in.hovered = in.clicked = true;
    in.x = 10.0f;
    in.y = 12.0f;
    std::vector<float> out;
    bool consumed = false;
    s.path_for_test().update(in, out, consumed);
    check(s.path_anchors() == 1, "slide path: fixture: one anchor down, the pen path open");
    s.start_slideshow();
    check(!s.slideshow_playing() && s.doc() != nullptr,
          "slide path: Play is refused with a pen path open");
    s.path_for_test().cancel();
    s.start_slideshow();
    check(s.slideshow_playing(), "slide path: with the path gone it plays");
    s.stop_slideshow();
    settle(s);
    s.close();
}

// While the scan counts, "none that way" is not yet known; an inverted Range
// is not "no other frame of this camera".
void test_workflow_messages_say_why() {
    Fixture f = make_dataset("why_msgs", 64, 48, {"cam0/a", "cam0/b", "cam0/c"});
    std::atomic<bool> hold{true}, held{false};
    mk::MaskSession s;
    s.set_scan_hook_for_test([&](int i, bool read) {
        if (read || i != 1) return;
        held = true;
        for (int k = 0; k < 20000 && hold.load(); k++)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    std::string err;
    check(s.open(f.root.string(), f.images.string(), f.masks.string(), false, err),
          "why: open: " + err);
    settle(s);
    for (int k = 0; k < 5000 && !held.load(); k++) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(held.load() && s.scanned_count() == 1, "why: fixture: the scan is held after frame 1 of 3");
    check(!s.go_to_missing(+1) &&
              s.error() == spirula::i18n::msg::maskedit::find_none_scanning.get(),
          "why: M mid-scan says the scan is still running: " + s.error());
    hold = false;
    wait_scanned(s);
    check(!s.go_to_missing(+1) && s.error() == spirula::i18n::msg::maskedit::find_none.get(),
          "why: M after the scan says there is none: " + s.error());
    s.propagate(mk::PropagateScope::Range, 2, 1);
    check(s.error() == spirula::i18n::msg::maskedit::prop_range_empty.get(),
          "why: an inverted Range says the range is empty: " + s.error());
    s.propagate(mk::PropagateScope::Range, 0, 0);
    check(s.error() == spirula::i18n::msg::maskedit::prop_no_targets.get(),
          "why: a Range holding only the source still says no other frame: " + s.error());
    s.close();
}

int main() {
    test_fnv();
    test_composite_truth_table();
    test_keys_and_paths();
    test_png_and_atomic_write();
    test_temp_write_path_unique();
    test_index_roundtrip();
    test_save_and_layers_roundtrip();
    test_fingerprint_decides();
    test_revert_is_byte_exact();
    test_save_without_mask();
    test_revert_reports_removal_failure();
    test_recomposite_all_continues_past_failure();
    test_revert_all_continues_past_failure();
    test_revert_all_guards_empty_mask_root();
    test_doc_load_and_paint();
    test_doc_without_mask();
    test_read_write_rect_roundtrip();
    test_noop_paint_does_not_dirty();
    test_undo_redo();
    test_byte_cap_eviction();
    test_orientation_mapping();
    test_view_math();
    test_derive_window();
    test_window_styles();
    test_derive_window_frame_scale();
    test_view_math_non_square_pane();
    test_session();
    test_propagate_targets();
    test_propagate_to();
    test_snapshot_restore();
    test_snapshot_restore_flipped();
    test_restore_failure_keeps_base();
    test_restore_rollback_keeps_snapshot_layers();
    test_restore_writes_no_mask();
    test_snapshot_restore_edges();
    test_session_propagate();
    test_propagate_cancel();
    test_session_propagate_failures();
    test_session_propagate_flipped();
    test_propagate_forgotten_on_reopen();
    test_undo_propagate_failure();
    test_revert_all_drops_propagate_record();
    test_undo_propagate_failure_no_entry();
    test_undo_propagate_one_layer_resists();
    test_revert_all_drops_queued_record();
    test_undo_propagate_index_save_fails();
    test_propagate_camera_many();
    test_propagate_names_unrestored();
    test_propagate_unrestored_after_a_write();
    test_undo_propagate_unreadable_layer();
    test_propagate_refuses_stray_base();
    test_undo_propagate_regenerated_target();
    test_session_eraser();
    test_session_size_mismatch();
    test_session_flipped_polarity();
    test_session_other_mask_root_refused();
    test_session_reencoded_mask_is_not_regenerated();
    test_session_corrupt_index_refuses();
    test_session_close_reports_a_failed_save();
    test_session_close_resets_paths();
    test_session_exif_turn();
    test_path_fill_parity();
    test_livewire_features();
    test_livewire_mapping();
    test_livewire_edge_path();
    test_livewire_reanchor();
    test_livewire_diagonal();
    test_livewire_once();
    test_path_tool_basic();
    test_path_tool_mode_latch();
    test_path_tool_livewire();
    test_path_tool_space();
    test_pen_tool_basic();
    test_pen_tool_modifiers();
    test_pen_tool_space();
    test_to_displayed_float();
    test_livewire_corner_path();
    test_livewire_sign_alignment();
    test_add_stencil_same_size();
    test_add_stencil_resamples();
    test_add_stencil_resample_matches_masker();
    test_add_stencil_unions_three();
    test_add_stencil_rejects_empty();
    test_add_stencil_is_one_undo_step();
    test_add_history_bytes();
    test_mask_sam_stub_refuses();
    test_mask_sam_clicks();
    test_mask_sam_negative_clicks();
    test_mask_sam_click_filters();
    test_session_frame_pixels_outlive_navigation();
    test_session_model_sync();
    test_session_sam_stamp();
    test_session_sam_blocker();
    test_mask_sam_release_forgets();
    test_mask_sam_run_guarded();
    test_session_sam_result_stamp();
    test_session_sam_blocker_clears_its_own_error();
    test_canvas_mode_is_exclusive();
    test_mask_picker_row();
    test_strip_reserve();
    test_click_maps_by_shown_layout();
    test_click_maps_by_clicked_pane();
    test_pane_at();
    test_bind_pane();
    test_pen_across_panes();
    test_switch_view();
    test_pen_survives_view_switch();
    test_pane_style_for();
    test_plan_derive();
    test_session_sam_paint_modes();
    test_session_sam_clear_is_local();
    test_margin_radius();
    test_add_stencil_drop_margin();
    test_drop_margin_modes();
    test_editor_margin_is_its_own();
    test_prompt_point_off_frame();
    test_session_sam_add_replaces();
    test_session_sam_negative_mode();
    test_mask_sam_prompt_points();
    test_session_refused_click_leaves_no_dot();
    test_held_region_roundtrip();
    test_session_sam_margin_reapply();
    test_session_sam_undo_click();
    test_session_sam_noop_replace();
    test_session_sam_clear_forgets_add();
    test_session_sam_prompt_bookkeeping();
    test_session_sam_margin_after_prompt();
    test_veto_regions();
    test_add_stencil_veto_beats_margin();
    test_add_stencil_box_sizes_margin();
    test_session_sam_click_mode();
    test_session_sam_redo_then_refine();
    test_session_sam_undo_then_refine();
    test_session_sam_noop_fresh_keeps_redo();
    test_session_sam_click_mode_shift();
    test_session_sam_refine_at_cap();
    test_session_sam_empty_phrase();
    test_session_sam_revert_forgets_clicks();
    test_session_sam_text_gate();
    test_publish_unless_cancelled();
    test_session_close_forgets_sam();
    test_session_sam_margin_after_undo_redo();
    test_session_sam_vetoed_all();
    test_session_close_parks_busy_job();
    test_propagate_waits_for_sam();
    test_session_close_releases_idle();
    test_session_retiring_drains();
    test_session_sam_held_other_frame();
    test_session_sam_clear_in_flight();
    test_session_sam_device_gate();
    test_session_sam_release_pending_refuses();
    test_session_sam_margin_keeps_pause();
    test_session_sam_redo_after_dropped_margin();
    test_load_picture_matches_make_picture();
    test_load_picture_reuses_its_buffer();
    test_load_picture_mask_other_size();
    test_load_picture_turned_mask();
    test_slide_prefetch();
    test_slide_threads();
    test_slide_prefetch_keeps_buffers();
    test_slide_prefetch_evicts_farthest();
    test_slide_prefetch_discards_stale();
    test_slide_prefetch_halt();
    test_session_slideshow();
    test_session_slideshow_stop_reloads_full_resolution();
    test_session_slideshow_threads_from_largest_frame();
    test_session_slideshow_flipped();
    test_session_slideshow_window();
    test_session_slideshow_stop_does_not_join();
    test_session_slideshow_stop_on_screen();
    test_session_slideshow_skips_undecodable();
    test_session_slideshow_close_while_playing();
    test_session_slideshow_plays_the_edit(false);
    test_session_slideshow_plays_the_edit(true);
    test_session_slideshow_waits_for_worker();
    test_slideshow_releases_sam();
    test_kept_cache();
    test_missing_predicate();
    test_session_find_missing();
    test_scan_save_failure();
    test_session_find_missing_flipped();
    test_scan_yields_to_a_write();
    test_scan_skips_a_fifo();
    test_scan_follows_rebase();
    test_find_missing_unreadable();
    test_find_missing_layer_without_mask();
    test_fix_then_leave();
    test_scan_waits_out_a_running_job();
    test_close_mid_scan();
    test_undo_refresh_after_index_failure();
    test_band_edit();
    test_kept_cache_refuses_a_relative_root();
    test_propagate_forgets_clicks();
    test_sam_result_ready_is_pending();
    test_propagate_source_save_failure_sticks();
    test_slideshow_refuses_open_path();
    test_workflow_messages_say_why();
    if (const char* b = std::getenv("SS_MASK_BENCH")) bench_8k(b);
    if (const char* b = std::getenv("SS_MASK_BENCH")) bench_propagate(b);
    if (const char* b = std::getenv("SS_MASK_BENCH")) bench_livewire(b);
    if (std::getenv("SS_MASK_BENCH")) bench_add_history();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
