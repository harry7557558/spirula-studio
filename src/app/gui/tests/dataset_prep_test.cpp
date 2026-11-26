// dataset_prep_test -- where DatasetPrep (app/gui/DatasetPrep.h) meets the
// folders beside images/: a re-run re-applies the mask editor's corrections
// over the masks it rewrites, the camera scan never takes mask_edits/ or
// feature_masks/ for a camera, and the sky's 128 band survives every fold.
// Real DatasetPrep::run, no model: the re-mask is the frame stencil, which is
// also checked per camera folder and with a lens circle shared between inputs.
// Built without SS_BUILD_SAM, so it also checks that asking for a model fails;
// that `sparse/0` is not a dataset; what becomes of the pictures a blur
// threshold leaves out; and that the frames reel lists every image.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/DatasetRecord.h"
#include "app/gui/FilmReel.h"
#include "app/gui/PickSheet.h"
#include "app/gui/mask/MaskLayer.h"
#include "core/BufferedFile.h"
#include "core/Sha256.h"
#include "core/SourcePath.h"
#include "data/ImageDiscovery.h"
#include "external/stb_image_write.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/MaskEdit.h"
#include "sfm/core/Exif.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;
namespace mk = gui::mask;

namespace {

std::function<void(const fs::path&, uintmax_t)> source_hash_read_hook;

// The blur rule at `px` (0 off), `by_hand` with the pause.
void skip_blurry(gui::PrepJob& job, float px, bool by_hand = false) {
    job.skip_blurry = px > 0.0f;
    job.blur_px = px > 0.0f ? px : job.blur_px;
    job.blur_review = by_hand;
}

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

fs::path scratch(const char* name) {
    const fs::path d = fs::temp_directory_path() / "spirula_dataset_prep_test" / name;
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

void write_jpg(const fs::path& p, int w, int h, int seed) {
    std::vector<uint8_t> px((size_t)w * h * 3);
    for (size_t i = 0; i < px.size(); i++) px[i] = (uint8_t)((i * 7 + (size_t)seed * 31) & 255);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    stbi_write_jpg(p.string().c_str(), w, h, 3, px.data(), 90);
}

void write_png(const fs::path& p, int w, int h, uint8_t v) {
    std::vector<uint8_t> px((size_t)w * h, v);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    stbi_write_png(p.string().c_str(), w, h, 1, px.data(), w);
}

// One box of 255 on 0, [x0, x1) x [y0, y1).
std::vector<uint8_t> box(int w, int h, int x0, int y0, int x1, int y1) {
    std::vector<uint8_t> px((size_t)w * h, 0);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) px[(size_t)y * w + x] = 255;
    return px;
}

uint8_t at(const std::vector<uint8_t>& px, int w, int x, int y) { return px[(size_t)y * w + x]; }

bool run_prep(const gui::PrepJob& job, gui::RunProgress& prog, std::string& error,
              gui::PickSheet* picks = nullptr, gui::FilmReel* frames = nullptr,
              gui::FilmReel* masks = nullptr) {
    std::atomic<bool> cancel{false};
    gui::RunFilms films;
    films.picks = picks;
    films.frames = frames;
    films.masks = masks;
    gui::DatasetPrep prep(&prog, films, cancel);
    gui::PrepResult out;
    return prep.run(job, out, error);
}

bool logged(gui::RunProgress& prog, const std::string& line) {
    for (const gui::RunLine& l : prog.drain())
        if (l.text == line) return true;
    return false;
}

// The re-mask is DatasetPrep::run's stencil pass, which folds the stencil into
// the masks already there -- so it re-drops the band a hand "keep" had put
// back, and only the re-apply after it restores that keep.
void test_rerun_reapplies_corrections() {
    const int W = 64, H = 48;
    const fs::path root = scratch("rerun");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(photos / (std::string(1, (char)('a' + i)) + ".jpg"), W, H, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::InPlace;
    gui::PrepInput in;
    in.path = photos.string();
    std::string err;
    check(app::parse_mask_shapes("-rect 0,0.5,1,0.75", in.stencil.mask.shapes, err),
          "fixture: the stencil drops rows 24..35: " + err);
    job.inputs = {in};
    gui::RunProgress prog;
    check(run_prep(job, prog, err), "first run: " + err);
    const fs::path mask_a = ws / "masks" / "a.png";
    std::vector<uint8_t> run1;
    int w = 0, h = 0;
    check(app::load_stencil(mask_a.string(), w, h, run1) && w == W && h == H,
          "first run wrote masks/a.png at 64x48");
    if (run1.size() != (size_t)W * H) return;
    check(at(run1, W, 15, 30) == 0 && at(run1, W, 45, 8) == 255,
          "fixture: the stencil band is dropped, the top is kept");

    // What the editor's save writes: keep a box inside the band, drop one above it.
    const std::string layer_root = (ws / mk::kLayerDirName).string();
    const std::string mask_root = mk::normalize_dir((ws / "masks").string());
    const std::vector<uint8_t> keep = box(W, H, 10, 28, 20, 34);
    const std::vector<uint8_t> drop = box(W, H, 40, 5, 50, 12);
    mk::LayerIndex idx;
    idx.mask_root = mask_root;
    check(mk::save_frame(layer_root, mask_root, "a", W, H, run1.data(), drop.data(), keep.data(),
                         true, idx, err),
          "the correction saves: " + err);
    std::vector<uint8_t> saved;
    app::load_stencil(mask_a.string(), w, h, saved);
    check(at(saved, W, 15, 30) == 255 && at(saved, W, 45, 8) == 0,
          "fixture: the saved composite carries the keep and the drop");

    prog.drain();
    check(run_prep(job, prog, err), "second run: " + err);
    const std::string one = spirula::i18n::format(spirula::i18n::msg::maskedit::log_recomposited,
                                                  {1LL});
    check(logged(prog, one), "the second run logs one frame re-applied");
    std::vector<uint8_t> after;
    check(app::load_stencil(mask_a.string(), w, h, after) && after.size() == (size_t)W * H,
          "masks/a.png readable after the re-run");
    if (after.size() != (size_t)W * H) return;
    check(at(after, W, 15, 30) == 255,
          "re-run: the hand keep inside the stencil band survives the re-mask");
    // Not a discriminator: the stencil's fold is an intersection, so it keeps a drop by itself.
    check(at(after, W, 45, 8) == 0, "re-run: the hand drop is still dropped");
    check(at(after, W, 30, 30) == 0 && at(after, W, 5, 3) == 255,
          "re-run: pixels no correction covers are the stencil's");
    std::vector<uint8_t> b;
    app::load_stencil((ws / "masks" / "b.png").string(), w, h, b);
    check(b.size() == run1.size() && at(b, W, 15, 30) == 0,
          "re-run: a's keep is not applied to uncorrected frame b");
    job.resume = true;
    job.make_frames = false;
    gui::FilmReel frames, masks;
    check(run_prep(job, prog, err, nullptr, &frames, &masks), "resume previews: " + err);
    check(frames.size() == 3 && masks.size() == 3, "reused frames and masks populate their preview reels");
}

// A sibling holding the same PNGs under another name IS a camera, so only
// the name guard keeps mask_edits/ and an older run's feature_masks/ out.
void test_camera_scan_skips_mask_edits() {
    const fs::path root = scratch("scan");
    write_jpg(root / "cam0" / "f0.jpg", 16, 12, 0);
    write_jpg(root / "cam1" / "f0.jpg", 16, 12, 1);
    for (const char* dir : {mk::kLayerDirName, gui::kLegacyFeatureMaskDirName, "lookalike"})
        for (const char* f : {"cam0/f0.base.png", "cam0/f0.drop.png", "cam0/f0.keep.png"})
            write_png(root / dir / f, 16, 12, 255);
    write_png(root / image_discovery::kProgressiveCache / "images" / "4x3" / "cached.png", 4, 3, 127);
    write_png(root / "cam0" / image_discovery::kProgressiveCache / "external" / "cached.png", 4, 3, 127);
    const std::vector<std::string> cams = gui::camera_subfolders(root.string());
    std::string listed;
    for (const std::string& c : cams) listed += c + " ";
    check(std::find(cams.begin(), cams.end(), "lookalike/cam0") != cams.end(),
          "fixture: the layer PNGs under another name are taken for a camera: " + listed);
    bool edits = false;
    for (const std::string& c : cams)
        edits |= c.rfind(mk::kLayerDirName, 0) == 0 || c.rfind(gui::kLegacyFeatureMaskDirName, 0) == 0;
    check(!edits, "camera scan: nothing under mask_edits/ or feature_masks/: " + listed);
    check(!gui::folder_has_images((root / image_discovery::kProgressiveCache).string()),
          "a cache-only folder is not a photo source");
    const auto groups = app::group_frames_by_camera(root.string());
    bool cached_group = false;
    for (const auto& group : groups)
        cached_group |= image_discovery::generated_cache(group.first);
    check(!cached_group, "frame masking does not create cameras from generated levels");
    check(cams.size() == 3 && cams[0] == "cam0" && cams[1] == "cam1",
          "camera scan: cam0, cam1 and the lookalike, nothing else: " + listed);
    check(gui::is_mask_edits_folder((root / mk::kLayerDirName).string()) &&
              !gui::is_mask_edits_folder((root / "lookalike").string()),
          "is_mask_edits_folder: by name");
}

// An input whose cameras have their own stencils: cam1's replaces the
// input's for its frames, and cam0 keeps the input's.
void test_per_camera_stencil() {
    const int W = 40, H = 20;
    const fs::path root = scratch("percamera");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (const char* f : {"f.jpg", "g.jpg"}) {
        write_jpg(photos / "cam0" / f, W, H, 0);
        write_jpg(photos / "cam1" / f, W, H, 1);
    }
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::InPlace;
    gui::PrepInput in;
    in.path = photos.string();
    std::string err;
    app::parse_mask_shapes("-rect 0,0,0.5,1", in.stencil.mask.shapes, err);
    app::parse_mask_shapes("-rect 0.5,0,1,1", in.stencil.cameras["cam1"].mask.shapes, err);
    job.inputs = {in};
    gui::RunProgress prog;
    check(run_prep(job, prog, err), "per camera: run: " + err);
    std::vector<uint8_t> m0, m1;
    int w = 0, h = 0;
    const bool read = app::load_stencil((ws / "masks" / "cam0" / "f.png").string(), w, h, m0) &&
                      app::load_stencil((ws / "masks" / "cam1" / "f.png").string(), w, h, m1) &&
                      m0.size() == (size_t)W * H && m1.size() == m0.size();
    check(read, "per camera: a mask per camera folder");
    if (!read) return;
    check(at(m0, W, 5, 10) == 0 && at(m0, W, 35, 10) == 255,
          "per camera: cam0 has the input's shapes, the left half out");
    check(at(m1, W, 5, 10) == 255 && at(m1, W, 35, 10) == 0,
          "per camera: cam1 has its own, the right half out");
}

// A lit, changing circle of `radius` (of the frame) on black.
void write_fisheye(const fs::path& p, int size, int seed, float radius) {
    std::vector<uint8_t> px((size_t)size * size * 3, 0);
    const float c = 0.5f * (float)size, r = radius * (float)size;
    uint32_t s = 2654435761u * (uint32_t)(seed + 1);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++) {
            s = s * 1664525u + 1013904223u;
            const float dx = (float)x + 0.5f - c, dy = (float)y + 0.5f - c;
            if (dx * dx + dy * dy > r * r) continue;
            for (int k = 0; k < 3; k++) px[((size_t)y * size + x) * 3 + k] = (uint8_t)(40 + (s >> 25));
        }
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    stbi_write_png(p.string().c_str(), size, size, 3, px.data(), size * 3);
}

// Pixels where two masks of one size disagree.
size_t mask_diff(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    size_t n = a.size() == b.size() ? 0 : a.size() + b.size();
    for (size_t i = 0; i < a.size() && i < b.size(); i++) n += a[i] != b[i];
    return n;
}

// Two captures of one lens (issue #420): sharing its circle, both are cut by
// one fit over all their frames, though the second, dimmer at the rim, alone
// would fit a smaller one.
void test_shared_border() {
    const int S = 256;
    const fs::path root = scratch("shared_border");
    const fs::path a = root / "a", b = root / "b";
    for (int k = 0; k < 8; k++) {
        const std::string f = "f" + std::to_string(k) + ".png";
        write_fisheye(a / "cam0" / f, S, k, 0.42f);
        write_fisheye(b / "cam0" / f, S, 100 + k, 0.34f);
    }
    // A dataset each: masks already written would be intersected with.
    auto run = [&](bool share, std::vector<uint8_t>& ma, std::vector<uint8_t>& mb) {
        const fs::path ws = root / (share ? "shared" : "own");
        gui::PrepJob job;
        job.workspace = ws.string();
        gui::PrepInput one, two;
        one.path = a.string();
        one.subdir = "a";
        two.path = b.string();
        two.subdir = "b";
        for (gui::PrepInput* in : {&one, &two}) {
            in->stencil.detect_border = true;
            in->stencil.shrink = 0.0f;
            in->stencil_follow["cam0"].share = share ? 3 : 0;
        }
        job.inputs = {one, two};
        gui::RunProgress prog;
        std::string err;
        int w = 0, h = 0;
        const bool ok = run_prep(job, prog, err) &&
                        app::load_stencil((ws / "masks" / "a" / "cam0" / "f0.png").string(), w, h, ma) &&
                        app::load_stencil((ws / "masks" / "b" / "cam0" / "f0.png").string(), w, h, mb);
        check(ok, std::string("shared border: run, ") + (share ? "shared: " : "own: ") + err);
        bool line = false;
        for (const gui::RunLine& l : prog.drain())
            line = line || l.text.rfind("a/cam0, b/cam0: ", 0) == 0;
        return line;
    };
    std::vector<uint8_t> own_a, own_b, shared_a, shared_b;
    const bool own_line = run(false, own_a, own_b);
    check(!own_line && mask_diff(own_a, own_b) > (size_t)S * S / 20,
          "shared border fixture: alone, the second capture fits a smaller circle");
    const bool shared_line = run(true, shared_a, shared_b);
    check(shared_line, "shared border: one fit, logged for both rows");
    check(!shared_a.empty() && shared_a == shared_b, "shared border: both captures get the same mask");
    check(mask_diff(shared_a, own_a) < (size_t)S * S / 50,
          "shared border: the circle is the one the whole lens shows");
}

// A lone input keeps images/ to itself, and a scan's photographs join it in a
// folder of their own: each folder gets its own input's stencil, not both.
void test_nested_input_stencils() {
    const int W = 40, H = 20;
    const fs::path root = scratch("nested");
    const fs::path a = root / "video", b = root / "scan", ws = root / "dataset";
    for (const char* f : {"f.jpg", "g.jpg", "h.jpg"}) {
        write_jpg(a / f, W, H, 0);
        write_jpg(b / f, W, H, 1);
    }
    gui::PrepJob job;
    job.workspace = ws.string();
    gui::PrepInput lone, scan;
    lone.path = a.string();
    scan.path = b.string();
    scan.subdir = "scan_b";
    std::string err;
    app::parse_mask_shapes("-rect 0,0,0.5,1", lone.stencil.mask.shapes, err);
    app::parse_mask_shapes("-rect 0.5,0,1,1", scan.stencil.mask.shapes, err);
    job.inputs = {lone, scan};
    gui::RunProgress prog;
    check(run_prep(job, prog, err), "nested: run: " + err);
    std::vector<uint8_t> mine, theirs;
    int w = 0, h = 0;
    const bool read =
        app::load_stencil((ws / "masks" / "f.png").string(), w, h, mine) &&
        app::load_stencil((ws / "masks" / "scan_b" / "f.png").string(), w, h, theirs) &&
        mine.size() == (size_t)W * H && theirs.size() == mine.size();
    check(read, "nested: a mask for each input's photographs");
    if (!read) return;
    check(at(mine, W, 5, 10) == 0 && at(mine, W, 35, 10) == 255,
          "nested: the lone input's photographs have its shape");
    check(at(theirs, W, 5, 10) == 255 && at(theirs, W, 35, 10) == 0,
          "nested: the folder inside it has its own input's shape only");
}

// The sky is masks/' 128 band. An older run's feature_masks/ is cleared but
// not resumed; COLMAP gets a binary copy -- of the sky alone without the
// checkbox -- and a stencil fold keeps the band.
void test_segment_masks_workspace() {
    const int W = 8, H = 4;
    const fs::path ws = scratch("segmasks");
    write_jpg(ws / "images" / "cam0" / "a.jpg", W, H, 0);
    write_jpg(ws / "images" / "cam0" / "b.jpg", W, H, 1);
    auto png = [&](const fs::path& p, const std::vector<uint8_t>& px) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        stbi_write_png(p.string().c_str(), W, H, 1, px.data(), W);
    };
    png(ws / gui::kLegacyFeatureMaskDirName / "cam0" / "a.png", box(W, H, 0, 2, W, H));
    const gui::WorkspaceState st = gui::probe_workspace(ws.string(), {});
    const std::vector<std::string> arts = gui::workspace_artifacts(ws.string(), {});
    check(!st.masks, "probe: a legacy feature_masks/ is not masks to resume");
    check(std::find(arts.begin(), arts.end(),
                    (ws / gui::kLegacyFeatureMaskDirName).string()) != arts.end(),
          "artifacts: a legacy feature_masks/ is the run's to clear");

    // a: left column ignored, top row sky, the rest kept. b has no mask.
    std::vector<uint8_t> a((size_t)W * H, app::kMaskKeep);
    std::vector<uint8_t> sky = box(W, H, 0, 1, W, H);
    app::fold_segment(a, W, H, sky, W, H);
    for (int y = 0; y < H; y++) a[(size_t)y * W] = app::kMaskIgnore;
    app::fold_segment(a, W, H, sky, W, H);
    check(at(a, W, 0, 0) == 0 && at(a, W, 3, 0) == 128 && at(a, W, 3, 2) == 255,
          "fold: the sky marks only what is still kept");
    png(ws / "masks" / "cam0" / "a.png", a);
    std::string err;
    const fs::path out = ws / ".colmap_masks";
    for (const bool segment_only : {false, true}) {
        const int64_t n = app::binary_mask_tree((ws / "images").string(),
                                                (ws / "masks").string(), false, segment_only,
                                                out.string(), nullptr, err);
        int w = 0, h = 0;
        std::vector<uint8_t> got;
        app::load_stencil((out / "cam0" / "a.png").string(), w, h, got);
        const uint8_t col0 = segment_only ? 255 : 0;
        check(n == 1 && got.size() == a.size() && at(got, W, 0, 2) == col0 &&
                  at(got, W, 3, 0) == 0 && at(got, W, 3, 2) == 255,
              std::string("colmap copy, segment_only=") + (segment_only ? "1" : "0"));
    }
    const std::vector<uint8_t> flipped =
        app::read_mask_classes((ws / "masks" / "cam0" / "a.png").string(), W, H, true);
    check(at(flipped, W, 0, 2) == 255 && at(flipped, W, 3, 2) == 0,
          "classes: a flipped mask reads as binary remove-masks");

    // A stencil folded over masks/ keeps the band where it still keeps.
    app::FrameStencilRun run;
    run.image_dir = (ws / "images").string();
    run.mask_dir = (ws / "masks").string();
    run.skip_dirs = {run.mask_dir};
    app::MaskShape half;
    half.kind = app::MaskShape::Kind::Rect;
    half.cx = 0.0f; half.cy = 0.0f; half.rx = 0.5f; half.ry = 1.0f;
    run.stencil.mask.shapes.push_back(half);
    app::FrameStencilSinks sinks;
    check(app::apply_frame_stencil(run, sinks, err) == 2, "stencil: both frames written: " + err);
    int w = 0, h = 0;
    std::vector<uint8_t> merged;
    app::load_stencil((ws / "masks" / "cam0" / "a.png").string(), w, h, merged);
    check(merged.size() == a.size() && at(merged, W, 1, 0) == 128 && at(merged, W, 1, 2) == 255 &&
              at(merged, W, 0, 2) == 0 && at(merged, W, 6, 0) == 0,
          "stencil: the sky survives a fold, outside the stencil is ignored");
}

void test_model_masking_needs_segmentation() {
    const fs::path root = scratch("nosam");
    const fs::path photos = root / "photos";
    for (int i = 0; i < 3; i++) write_jpg(photos / (std::string(1, (char)('a' + i)) + ".jpg"), 64, 48, i);
    gui::PrepJob job;
    job.workspace = (root / "dataset").string();
    job.photo_import = gui::PhotoImport::InPlace;
    job.mask_enable = true;
    job.mask_prompt = "person";
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    const bool ran = run_prep(job, prog, err);
    check(!ran && err == spirula::i18n::msg::log::err_no_builtin_segmentation.get(),
          "no segmentation module: a model-masking run is refused: " + err);
}

}  // namespace

// A HEIC folder is never read in place, and a run with nothing that can read
// it names the decoder it needed and leaves no half-written JPEG behind.
void test_heif_folder() {
    const fs::path root = scratch("heif");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(photos / (std::string(1, (char)('a' + i)) + ".jpg"), 16, 12, i);
    check(!gui::folder_has_heif(photos.string()), "a folder of JPEGs holds no HEIC");
    std::ofstream(photos / "d.HEIC", std::ios::binary) << "not a photo";
    check(gui::folder_has_heif(photos.string()) && gui::is_image_file(photos / "d.HEIC"),
          "a .HEIC is a photo, whatever its case");

    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::InPlace;
    job.ffmpeg_exe = (root / "no-such-ffmpeg").string();
    gui::PrepInput in;
    in.path = photos.string();
    in.heif = true;
    job.inputs = {in};
    check(!gui::reads_photos_in_place(job.inputs, job.photo_import),
          "a HEIC folder is not read in place");
    job.inputs[0].heif = false;   // run() asks the disk itself
    gui::RunProgress prog;
    std::string err;
    const bool ran = run_prep(job, prog, err);
    check(!ran && err == spirula::i18n::format(spirula::i18n::msg::log::err_ffmpeg_missing,
                                               {job.ffmpeg_exe}),
          "without a decoder the run names ffmpeg: " + err);
    std::error_code ec;
    check(!fs::exists(ws / "images" / "d.jpg", ec) &&
              !fs::exists(ws / "images" / "d.jpg.part", ec),
          "no half-written JPEG is left for a resumed run to keep");
}

// A chequerboard smeared sideways across `smear` pixels: 10 px of edge sharp,
// 40 px at a smear of 30. A PNG, since JPEG's ringing on a pattern this clean
// outranks the smeared edges and leaves the picture unmeasured (and so kept).
void write_squares(const fs::path& p, int smear) {
    const int W = 640, H = 480;
    std::vector<uint8_t> px((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            int sum = 0;
            for (int k = 0; k < smear; k++) {
                const int cx = (x + k) / 40, cy = y / 40;
                sum += (cx + cy) % 2 ? 110 + 15 * ((cx + 3 * cy) % 8) : 30;
            }
            for (int c = 0; c < 3; c++) px[((size_t)y * W + x) * 3 + c] = (uint8_t)(sum / smear);
        }
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    stbi_write_png(p.string().c_str(), W, H, 3, px.data(), W * 3);
}

int files_in(const fs::path& dir) {
    std::error_code ec;
    int n = 0;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        n += it->is_regular_file(ec) ? 1 : 0;
    return n;
}

// Eight photographs, two of them smeared. Left out, they are deleted from
// images/ unless asked for -- and never when images/ holds the only copy.
void test_skipped_photos() {
    struct Case { const char* name; gui::PhotoImport import; bool keep; int skipped; };
    const Case cases[] = {{"blur_drop", gui::PhotoImport::Copy, false, 0},
                          {"blur_keep", gui::PhotoImport::Copy, true, 2},
                          {"blur_move", gui::PhotoImport::Move, false, 2}};
    for (const Case& c : cases) {
        const fs::path root = scratch(c.name);
        const fs::path photos = root / "photos", ws = root / "dataset";
        for (int i = 0; i < 8; i++)
            write_squares(photos / ("p" + std::to_string(i) + ".png"), i == 2 || i == 5 ? 30 : 1);
        gui::PrepJob job;
        job.workspace = ws.string();
        job.photo_import = c.import;
        skip_blurry(job, 5.0f);
        job.keep_skipped = c.keep;
        gui::PrepInput in;
        in.path = photos.string();
        job.inputs = {in};
        gui::RunProgress prog;
        std::string err;
        const std::string n = c.name;
        gui::PickSheet sheet;
        gui::FilmReel reel;
        check(run_prep(job, prog, err, &sheet, &reel), n + ": run: " + err);
        check(reel.size() == 6, n + ": the frames reel lists the 6 left in images/");
        // Something to look at for the one left out: its file where it was
        // kept, and otherwise the photograph it was copied from.
        const fs::path seen(sheet.path(0, 2));
        std::error_code at;
        check(sheet.picks(0).size() == 8 && fs::exists(seen, at) &&
                  seen.parent_path().filename() == (c.skipped ? "images_skipped" : "photos"),
              n + ": the sheet shows the smeared one from " + seen.parent_path().string());
        std::error_code ec;
        check(files_in(ws / "images") == 6 && !fs::exists(ws / "images" / "p2.png", ec) &&
                  !fs::exists(ws / "images" / "p5.png", ec),
              n + ": the two smeared photographs are out of images/");
        check(files_in(ws / "images_skipped") == c.skipped,
              n + ": images_skipped/ holds " + std::to_string(c.skipped));
        check(files_in(photos) + (c.import == gui::PhotoImport::Move ? 8 : 0) == 8,
              n + ": the photographs given are where the import mode leaves them");
        if (c.import == gui::PhotoImport::Move) continue;
        // A later run that keeps the frames gathers the folder again, and must
        // not let the two back in -- whatever the rule says by then.
        gui::PrepJob again = job;
        again.resume = true;
        again.make_frames = false;
        skip_blurry(again, 0.0f);
        gui::RunProgress prog2;
        check(run_prep(again, prog2, err) && files_in(ws / "images") == 6 &&
                  !fs::exists(ws / "images" / "p5.png", ec),
              n + ": a run that keeps the frames leaves them sorted as they were");
        again.make_frames = true;
        check(run_prep(again, prog2, err) && files_in(ws / "images") == 8 &&
                  !fs::exists(ws / gui::kSkippedListFile, ec),
              n + ": a run that makes them again with the rule off brings both back");
    }
}

// One reel entry per image in images/: each lens of a rig and each half of a
// dual-lens photograph, across several inputs.
void test_frames_reel_counts_images() {
    const fs::path root = scratch("frames_reel");
    const fs::path ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(root / "phone" / ("p" + std::to_string(i) + ".jpg"), 16, 12, i);
    for (int i = 0; i < 2; i++)
        for (const char* cam : {"cam0", "cam1"})
            write_jpg(root / "rig" / cam / ("r" + std::to_string(i) + ".jpg"), 16, 12, i);
    for (int i = 0; i < 2; i++) write_jpg(root / "dual" / ("d" + std::to_string(i) + ".insp"), 32, 16, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    for (const char* name : {"phone", "rig", "dual"}) {
        gui::PrepInput in;
        in.path = (root / name).string();
        in.subdir = name;
        job.inputs.push_back(in);
    }
    gui::RunProgress prog;
    std::string err;
    gui::FilmReel reel;
    check(run_prep(job, prog, err, nullptr, &reel), "reel: run: " + err);
    check(files_in(ws / "images" / "dual") == 4, "reel: the dual-lens photographs are split in two");
    check(files_in(ws / "images") == 11 && reel.size() == 11,
          "reel: lists all 11 images, has " + std::to_string(reel.size()));
    job.resume = true;
    job.make_frames = false;
    reel.clear();
    check(run_prep(job, prog, err, nullptr, &reel), "reel: resume: " + err);
    check(reel.size() == 11, "reel: reused frames remain available for preview");
}

// One lens of each .insp is written, and which one is a setting, not a source
// that changed: the plan refuses to keep frames when the identity moves.
void test_packed_photo_one_lens() {
    const fs::path root = scratch("one_lens");
    const fs::path ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(root / "dual" / ("d" + std::to_string(i) + ".insp"), 32, 16, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = (root / "dual").string();
    in.packed_lenses = 2;
    in.active_lens = 1;
    job.inputs.push_back(in);
    gui::RunProgress prog;
    std::string err;
    const bool ran = run_prep(job, prog, err);
    check(ran, "one lens: run: " + err);
    std::error_code ec;
    check(!fs::exists(ws / "images" / "cam0", ec) && files_in(ws / "images" / "cam1") == 3,
          "one lens: only the lens kept is written");
    const std::string identity = gui::frame_source_identity(job, job.inputs[0], false);
    job.inputs[0].active_lens = 0;
    check(!identity.empty() && gui::frame_source_identity(job, job.inputs[0], false) == identity,
          "one lens: another lens is the same source");
}

// An .insp as a 360 camera writes one: its EXIF holds the exposure, the
// whole frame's 35 mm focal and a GPS fix (43.25 N, 79.5 W).
void write_insp_with_exif(const fs::path& p) {
    std::vector<uint8_t> t;
    auto u16 = [&t](uint32_t v) { t.push_back((uint8_t)v); t.push_back((uint8_t)(v >> 8)); };
    auto u32 = [&](uint32_t v) { u16(v & 0xFFFF); u16(v >> 16); };
    auto entry = [&](uint32_t tag, uint32_t type, uint32_t count, uint32_t value) {
        u16(tag); u16(type); u32(count); u32(value);
    };
    const uint32_t exif_ifd = 8 + 2 + 2 * 12 + 4, gps_ifd = exif_ifd + 2 + 3 * 12 + 4;
    const uint32_t data = gps_ifd + 2 + 4 * 12 + 4;
    t = {'I', 'I'};
    u16(42); u32(8);
    u16(2); entry(0x8769, 4, 1, exif_ifd); entry(0x8825, 4, 1, gps_ifd); u32(0);
    u16(3); entry(0x829A, 5, 1, data); entry(0x8827, 3, 1, 166); entry(0xA405, 3, 1, 6); u32(0);
    u16(4);
    entry(0x0001, 2, 2, 'N'); entry(0x0002, 5, 3, data + 8);
    entry(0x0003, 2, 2, 'W'); entry(0x0004, 5, 3, data + 32);
    u32(0);
    u32(1); u32(25);
    for (uint32_t v : {43u, 15u, 0u, 79u, 30u, 0u}) { u32(v); u32(1); }
    std::vector<uint8_t> seg = {'E', 'x', 'i', 'f', 0, 0};
    seg.insert(seg.end(), t.begin(), t.end());

    std::vector<uint8_t> px(32 * 16 * 3, 128), jpeg;
    stbi_write_jpg_to_func([](void* c, void* d, int n) {
        auto* v = (std::vector<uint8_t>*)c;
        v->insert(v->end(), (uint8_t*)d, (uint8_t*)d + n);
    }, &jpeg, 32, 16, 3, px.data(), 90);
    sfm::jpegInsertApp1(jpeg, seg);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream(p, std::ios::binary).write((const char*)jpeg.data(), (std::streamsize)jpeg.size());
}

// Each lens of an .insp keeps the photo's GPS and exposure, which SfM's
// position prior and PPISP's exposure seeds read, but not its focal.
void test_packed_photo_keeps_exif() {
    const fs::path root = scratch("insp_exif");
    const fs::path ws = root / "dataset";
    for (const char* name : {"IMG_0001.insp", "IMG_0002.insp"})
        write_insp_with_exif(root / "dual" / name);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = (root / "dual").string();
    in.packed_lenses = 2;
    job.inputs.push_back(in);
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "insp exif: run: " + err);
    for (const char* cam : {"cam0", "cam1"}) {
        const sfm::ExifData e = sfm::readExif((ws / "images" / cam / "IMG_0001.jpg").string());
        const std::string tag = std::string("insp exif: ") + cam;
        check(e.has_gps && std::fabs(e.lat_deg - 43.25) < 1e-9 && std::fabs(e.lon_deg + 79.5) < 1e-9,
              tag + " keeps the GPS fix");
        check(std::fabs(e.exposure_time - 0.04) < 1e-12 && e.iso == 166, tag + " keeps the exposure");
        check(!e.hasFocal(), tag + " drops the whole frame's focal");
    }
}

// A review's sheet: a looser threshold brings a frame back by a rename, and
// what is still left out is deleted without touching anything else there.
void test_pick_sheet_discards() {
    const fs::path root = scratch("picksheet");
    const fs::path keep = root / "images", skip = root / "images_skipped" / "clip";
    gui::PickSheet sheet;
    sheet.reset({"clip"});
    sheet.set_dirs(0, keep.string(), skip.string());
    const float widths[] = {3, 3, 20, 3, 14, 3};
    app::PickRule ran{8.0f, true, 0};
    for (int i = 0; i < 6; i++) {
        app::FramePick p;
        p.index = i;
        p.width = widths[i];
        const bool kept = ran.judge(p) == app::PickVerdict::Kept;
        const fs::path f = (kept ? keep : skip) / (std::to_string(i) + ".jpg");
        write_jpg(f, 8, 8, i);
        sheet.add(0, p, std::vector<uint8_t>(), f.string());
    }
    std::error_code ec;
    check(files_in(keep) == 4 && files_in(skip) == 2, "sheet: 20 and 14 px are out at 8 px");
    check(sheet.resort(0, app::PickRule{16.0f, true, 0}) == 1 && fs::exists(keep / "4.jpg", ec) &&
              fs::exists(skip / "2.jpg", ec),
          "sheet: at 16 px the 14 px frame is renamed back, the 20 px one stays out");
    std::ofstream(root / "images_skipped" / "mine.txt") << "not the run's";
    check(sheet.discard_skipped(0) == 1 && !fs::exists(skip, ec) && files_in(keep) == 5,
          "sheet: the frame left out is deleted with its folder, the kept ones stay");
    check(fs::exists(root / "images_skipped" / "mine.txt", ec),
          "sheet: a file the sheet did not write is not touched");
}

// Photographs gathered with the rule off, then sorted where they lie: the
// smeared ones move aside, and come back when the rule is turned off again.
void test_sort_in_place() {
    const fs::path root = scratch("sort_in_place");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 8; i++)
        write_squares(photos / ("p" + std::to_string(i) + ".png"), i == 2 || i == 5 ? 30 : 1);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err) && files_in(ws / "images") == 8, "sort: made with the rule off");
    gui::PrepJob sort = job;
    sort.resume = true;
    sort.make_frames = false;
    sort.sort_frames = true;
    skip_blurry(sort, 5.0f);
    std::error_code ec;
    check(run_prep(sort, prog, err) && files_in(ws / "images") == 6 &&
              files_in(ws / "images_skipped") == 2 && fs::exists(ws / "images_skipped" / "p2.png", ec),
          "sort: the two smeared ones move to images_skipped, nothing is deleted");
    check(gui::probe_workspace(ws.string(), sort.inputs).sorted_rule == gui::blur_rule_text(sort),
          "sort: the folder says what it is sorted by");
    sort.sort_frames = false;
    check(run_prep(sort, prog, err) && files_in(ws / "images") == 6,
          "sort: a later run that changes nothing leaves them aside");
    gui::PrepJob off = job;
    off.resume = true;
    off.make_frames = false;
    off.sort_frames = true;
    check(run_prep(off, prog, err) && files_in(ws / "images") == 8 &&
              files_in(ws / "images_skipped") == 0 &&
              gui::probe_workspace(ws.string(), off.inputs).sorted_rule.empty(),
          "sort: the rule turned off brings both back");
}

// A COLMAP model with nothing beside it is a reconstruction to view, not a
// dataset, so a dropped `sparse/0` does not land on the trainer.
void test_bare_model_is_not_a_dataset() {
    const fs::path root = scratch("bare_model");
    auto model = [](const fs::path& d) {
        std::error_code ec;
        fs::create_directories(d, ec);
        for (const char* f : {"cameras.bin", "images.bin", "points3D.bin"})
            std::ofstream(d / f, std::ios::binary) << "";
    };
    model(root / "sparse" / "0");
    write_jpg(root / "images" / "a.jpg", 16, 12, 0);
    check(gui::folder_looks_like_dataset(root.string()), "a folder with sparse/ is a dataset");
    check(!gui::folder_looks_like_dataset((root / "sparse" / "0").string()),
          "sparse/0 alone is not a dataset");
    model(root / "flat");
    write_jpg(root / "flat" / "images" / "a.jpg", 16, 12, 0);
    check(gui::folder_looks_like_dataset((root / "flat").string()),
          "a model beside images/ is a dataset");
    model(root / "loose");
    write_jpg(root / "loose" / "a.jpg", 16, 12, 0);
    check(gui::folder_looks_like_dataset((root / "loose").string()),
          "a model beside its photos is a dataset");
}

// Two camera folders: a frame is the files of one name in both, and goes or
// stays whole -- here on the strength of the second lens alone.
void test_sort_rig_in_place() {
    const fs::path root = scratch("sort_rig");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 8; i++) {
        const std::string name = "p" + std::to_string(i) + ".png";
        write_squares(photos / "cam0" / name, 1);
        write_squares(photos / "cam1" / name, i == 3 ? 30 : 1);
    }
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err) && files_in(ws / "images") == 16, "rig: made with the rule off");
    gui::PrepJob sort = job;
    sort.resume = true;
    sort.make_frames = false;
    sort.sort_frames = true;
    skip_blurry(sort, 5.0f);
    gui::PickSheet sheet;
    std::error_code ec;
    // As a two-lens video leaves it: a frame the rule left out before the
    // lenses were cut apart, whole, beside the camera folders.
    write_squares(ws / "images_skipped" / "whole.png", 1);
    check(run_prep(sort, prog, err, &sheet) && files_in(ws / "images") == 14 &&
              fs::exists(ws / "images_skipped" / "cam0" / "p3.png", ec) &&
              fs::exists(ws / "images_skipped" / "cam1" / "p3.png", ec),
          "rig: the frame smeared in one lens moves aside in both");
    // Sorted again by a rule nothing fails, over a reconstruction that placed
    // the first lens only: the list alone must bring the second lens's file
    // back, before anything is judged.
    {
        fs::create_directories(ws / "sparse" / "0", ec);
        std::ofstream model(ws / "sparse" / "0" / "images.bin", std::ios::binary);
        const uint64_t count = 8, none = 0;
        model.write((const char*)&count, 8);
        for (int i = 0; i < 8; i++) {
            const int32_t id = i + 1, camera = 1;
            const double pose[7] = {1, 0, 0, 0, 0, 0, 0};
            const std::string name = "cam0/p" + std::to_string(i) + ".png";
            model.write((const char*)&id, 4);
            model.write((const char*)pose, sizeof pose);
            model.write((const char*)&camera, 4);
            model.write(name.c_str(), (std::streamsize)name.size() + 1);
            model.write((const char*)&none, 8);
        }
    }
    gui::PrepJob eased = sort;
    eased.blur_px = 39.0f;
    gui::PickSheet again;
    check(run_prep(eased, prog, err, &again) && files_in(ws / "images") == 16 &&
              files_in(ws / "images_skipped") == 1,
          "rig: a second sort brings back both lenses' files of the frame the first moved");
    check(run_prep(sort, prog, err, &sheet) && files_in(ws / "images") == 14, "rig: and a third moves it again");
    check(sheet.picks(0).size() == 8 && sheet.resort(0, app::PickRule{}) == 1 &&
              files_in(ws / "images") == 16 && files_in(ws / "images_skipped") == 1 &&
              fs::exists(ws / "images_skipped" / "whole.png", ec),
          "rig: eased in the review, both lenses' files come back and the whole frame stays");
}

// Photographs in folders of their own names are one sequence, not a rig: a
// smeared one goes alone, and nothing is matched across folders by name.
void test_sort_folders_not_rig() {
    const fs::path root = scratch("sort_days");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 6; i++) {
        write_squares(photos / "day1" / ("a" + std::to_string(i) + ".png"), 1);
        write_squares(photos / "day2" / ("b" + std::to_string(i) + ".png"), i == 3 ? 30 : 1);
    }
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err) && files_in(ws / "images") == 12,
          "days: made with the rule off");
    gui::PrepJob sort = job;
    sort.resume = true;
    sort.make_frames = false;
    sort.sort_frames = true;
    skip_blurry(sort, 5.0f);
    std::error_code ec;
    check(run_prep(sort, prog, err) && files_in(ws / "images") == 11 &&
              fs::exists(ws / "images_skipped" / "day2" / "b3.png", ec),
          "days: the one smeared photograph moves aside, alone");
}

namespace gui {
void source_hash_before_read(const fs::path& file, uintmax_t offset) {
    if (source_hash_read_hook) source_hash_read_hook(file, offset);
}
}  // namespace gui

static void test_source_hash_overlap() {
    constexpr size_t chunk = spirula::kFileBufferBytes;
    const fs::path root = scratch("source_hash_overlap").lexically_normal();
    const fs::path source = root / "source.mp4", first = root / "first.mp4";
    const fs::path lut = root / "look.cube";
    std::vector<uint8_t> bytes(40u * (1u << 20) + 3);
    for (size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = (uint8_t)(i ^ (i >> 8) ^ (i >> 16) ^ (i >> 24));

    const auto write = [&](const fs::path& file, size_t size) {
        std::ofstream out(file, std::ios::binary);
        out.write(reinterpret_cast<const char*>(bytes.data()), (std::streamsize)size);
        out.close();
        if (!out) throw std::runtime_error("cannot write hash fixture");
    };
    const auto record = [&](const std::string& name, size_t size) {
        spirula::Sha256 hash;
        hash.update(bytes.data(), size);
        return name + '\0' + std::to_string(size) + '\0' + hash.hex() + '\0';
    };
    const auto digest = [](const std::string& text) {
        spirula::Sha256 hash;
        hash.update(reinterpret_cast<const uint8_t*>(text.data()), text.size());
        return hash.hex();
    };
    gui::PrepJob job;
    job.workspace = (root / "dataset").string();
    gui::PrepInput input;
    input.path = source.string();
    input.is_video = true;
    input.lut = gui::kLutNone;
    job.inputs = {input};
    for (size_t size : {size_t{0}, size_t{1}, chunk - 1, chunk, chunk + 1, bytes.size()}) {
        write(source, size);
        check(gui::frame_source_identity(job, job.inputs[0], true) == digest(record(".", size)),
              "legacy source digest at " + std::to_string(size) + " bytes");
    }

    write(source, chunk + 1);
    write(first, 1);
    write(lut, 65);
    gui::PrepInput first_input = input;
    first_input.path = first.string();
    first_input.lut = lut.string();
    job.inputs = {first_input, input};
    const std::vector<std::string> expected = {
        digest(record(".", 1) + record("../look.cube", 65)), digest(record(".", chunk + 1))};
    std::string error;
    check(gui::snapshot_frame_sources(job, error) && job.source_content == expected,
          "snapshot retains input and file-backed LUT order");

    const auto reject_mutation = [&](auto action, const char* what) {
        bool changed = false;
        source_hash_read_hook = [&](const fs::path& file, uintmax_t offset) {
            if (file == source && offset == chunk) {
                action(file);
                changed = true;
            }
        };
        const bool rejected = gui::frame_source_identity(job, job.inputs[1], true).empty();
        source_hash_read_hook = {};
        check(changed && rejected, what);
    };
    const auto stamp = fs::last_write_time(source);
    reject_mutation([&](const fs::path& file) {
        fs::last_write_time(file, stamp - std::chrono::hours(1));
    }, "mtime change between chunk reads rejects the identity");
    reject_mutation([&](const fs::path& file) {
        fs::resize_file(file, chunk);
    }, "truncation between chunk reads rejects a prefix digest");
    write(source, chunk + 1);

    struct ReaderError {};
    source_hash_read_hook = [&](const fs::path& file, uintmax_t offset) {
        if (file == source && offset == chunk) throw ReaderError{};
    };
    bool propagated = false;
    try {
        gui::frame_source_identity(job, job.inputs[1], true);
    } catch (const ReaderError&) {
        propagated = true;
    }
    source_hash_read_hook = {};
    check(propagated, "reader exceptions reach the identity caller");

    write(first, 2);

#ifdef _WIN32
    const HANDLE handle = CreateFileW(source.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    OVERLAPPED offset{};
    offset.Offset = (DWORD)chunk;
    const bool unreadable = handle != INVALID_HANDLE_VALUE &&
        LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &offset);
#else
    bool unreadable = false;
    source_hash_read_hook = [&](const fs::path& file, uintmax_t offset) {
        if (file == source && offset == 0) unreadable = fs::remove(file);
    };
#endif
    const bool rejected = !gui::snapshot_frame_sources(job, error);
#ifdef _WIN32
    if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
#else
    source_hash_read_hook = {};
#endif
    check(unreadable && rejected && error.find(input.path) != std::string::npos &&
              job.source_content == expected,
          "unreadable source rejects the snapshot without partial identities");
    fs::remove_all(root);
}

void test_source_aware_video_resume() {
    const fs::path root = scratch("source_identity");
    const fs::path source = root / "source.mp4", ws = root / "dataset";
    { std::ofstream file(source, std::ios::binary); file << "first video bytes"; }
    for (int i = 1; i <= 3; ++i)
        write_jpg(ws / "images" / ("frame_00000" + std::to_string(i) + ".jpg"), 16, 12, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    gui::PrepInput input;
    input.path = source.string();
    input.is_video = true;
    job.inputs = {input};
    job.make_frames = false;
    job.recorded_captures = {{"", source.string(), 1.0, false}};
    std::string error;
    check(gui::snapshot_frame_sources(job, error), "source resume: snapshot succeeds");
    gui::StepRecord frames;
    frames.present = frames.complete = true;
    frames.id = "fixture";
    frames.fields = {{"source_content", ".", job.source_content[0]}};
    gui::write_step_record(job.workspace, gui::Step::Frames, frames);
    gui::RunProgress progress;
    const bool reused = run_prep(job, progress, error);
    check(reused, "unchanged video reuses its existing frames: " + error);
    const auto stamp = fs::last_write_time(source);
    { std::ofstream file(source, std::ios::binary); file << "other video bytes"; }
    fs::last_write_time(source, stamp);
    std::atomic<bool> cancel{false};
    gui::DatasetPrep prep(&progress, {}, cancel);
    gui::PrepResult result;
    bool completed = false;
    check(!prep.run(job, result, error, {},
                    [&](gui::Stage, const gui::PrepJob&) { completed = true; }) &&
              !completed && error.find("changed") != std::string::npos,
          "source changed after planning never publishes frame completion");
    job.source_content.clear();
    error.clear();
    check(!run_prep(job, progress, error) && error.find("identity") != std::string::npos,
          "same-size same-timestamp replacement cannot reuse old video frames");
    check(fs::exists(ws / "images" / "frame_000001.jpg"),
          "identity mismatch preserves existing frames until explicit rebuild");
    frames.fields.clear();
    gui::write_step_record(job.workspace, gui::Step::Frames, frames);
    error.clear();
    check(!run_prep(job, progress, error), "legacy record without source identity fails closed");
    check(fs::exists(source), "resume failure preserves the source");
    gui::PrepJob photos;
    photos.workspace = ws.string();
    photos.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput photo;
    photo.path = (root / "photos").string();
    photos.inputs = {photo};
    write_jpg(root / "photos" / "a.jpg", 16, 12, 1);
    const std::string inventory = gui::frame_source_identity(photos, photos.inputs[0], true);
    fs::rename(root / "photos" / "a.jpg", root / "photos" / "b.jpg");
    check(!inventory.empty() &&
              inventory != gui::frame_source_identity(photos, photos.inputs[0], true),
          "photo inventory identity includes relative names, not just image bytes");
    const fs::path lut = root / "look.cube";
    { std::ofstream file(lut); file << "first LUT"; }
    job.inputs[0].lut = lut.string();
    const std::string with_lut = gui::frame_source_identity(job, job.inputs[0], true);
    { std::ofstream file(lut); file << "other LUT"; }
    check(!with_lut.empty() && with_lut != gui::frame_source_identity(job, job.inputs[0], true),
          "file-backed LUT content is a frame source dependency");
}

void test_mask_output_ownership() {
    const fs::path root = scratch("mask_owners") / std::string(96, 'p');
    const fs::path masks = root / "masks";
    fs::create_directories(masks);
    const fs::path output = masks / "frame.png";
    { std::ofstream file(output); file << "existing mask"; }
    gui::MaskOutputOwners owners;
    auto rejected = [&](const fs::path& path) {
        try { owners.claim(path); }
        catch (const std::runtime_error&) { return true; }
        return false;
    };
    const fs::path old_owner = root / ".spirula-mask-attempts" / "attempt" / "owners" /
        fs::absolute(output).relative_path();
    check(old_owner.native().size() > 260, "ownership regression exceeds Windows scratch path limit");
    check(!rejected(output), "long destination claims ownership without mirroring its path on disk");
    check(rejected(output), "two images with the same mask stem are rejected");
    check(rejected(masks / "." / "frame.png"), "lexical aliases cannot own the same mask");
    check(rejected(fs::relative(output, fs::current_path())), "relative and absolute owners collide");
    check(!rejected(root / "other_masks" / "frame.png"), "another folder's mask is not a collision");
    check(!fs::exists(root / ".spirula-mask-attempts"), "ownership validation creates no scratch tree");
    std::string contents;
    { std::ifstream file(output); std::getline(file, contents); }
    check(contents == "existing mask", "ownership validation preserves existing masks");
    std::error_code ec;
    fs::create_directory_symlink(masks, root / "alias", ec);
    if (!ec)
        check(rejected(root / "alias" / "frame.png"), "directory symlink aliases cannot own the same mask");
#ifdef _WIN32
    check(rejected(masks / "FRAME.PNG"), "Windows ownership remains case-insensitive");
    check(!rejected(masks / fs::path(L"\u00c4.png")) &&
          rejected(masks / fs::path(L"\u00e4.png")), "Windows Unicode case aliases collide");
    const auto absolute = fs::absolute(output).native();
    if (absolute.rfind(L"\\\\", 0) != 0)
        check(rejected(fs::path(L"\\\\?\\" + absolute)), "Windows extended and ordinary paths collide");
#else
    check(!rejected(masks / "FRAME.PNG"), "case-distinct POSIX outputs remain separate");
#endif
}

void test_geometry_reel_outputs() {
    const auto root = scratch("geometry-reel");
    const auto maps = root / "normals";
    write_png(maps / "frame.png", 16, 16, 128);
    std::ofstream(maps / "frame.png.geometry-complete") << "completion record";
    std::ofstream(maps / "frame.png.part-1") << "unfinished image";
    gui::FilmReel reel;
    gui::OutputWatch watch(&reel, maps, {}, {}, false, true);
    watch.poll();
    check(reel.size() == 1, "atomic geometry appears immediately without markers or temporary files");
    watch.poll(true);
    check(reel.size() == 1, "geometry flush does not duplicate published frames");
}

int main() {
    test_geometry_reel_outputs();
    test_mask_output_ownership();
    test_source_hash_overlap();
    test_source_aware_video_resume();
    test_bare_model_is_not_a_dataset();
    test_rerun_reapplies_corrections();
    test_camera_scan_skips_mask_edits();
    test_per_camera_stencil();
    test_shared_border();
    test_nested_input_stencils();
    test_segment_masks_workspace();
    test_model_masking_needs_segmentation();
    test_heif_folder();
    test_skipped_photos();
    test_frames_reel_counts_images();
    test_packed_photo_one_lens();
    test_packed_photo_keeps_exif();
    test_pick_sheet_discards();
    test_sort_in_place();
    test_sort_rig_in_place();
    test_sort_folders_not_rig();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
