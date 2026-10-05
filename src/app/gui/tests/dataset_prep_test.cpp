// dataset_prep_test -- where DatasetPrep (app/gui/DatasetPrep.h) meets the
// folders beside images/: a re-run re-applies the mask editor's corrections
// over the masks it rewrites, the camera scan never takes mask_edits/ or
// feature_masks/ for a camera, and COLMAP's one mask tree is the two ANDed.
// Real DatasetPrep::run, no model: the re-mask is the frame stencil, which is
// also checked per camera folder. Built without SS_BUILD_SAM, so it also
// checks that asking for a model fails.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/mask/MaskLayer.h"
#include "core/SourcePath.h"
#include "external/stb_image_write.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/MaskEdit.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace mk = gui::mask;

namespace {

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

bool run_prep(const gui::PrepJob& job, gui::RunProgress& prog, std::string& error) {
    std::atomic<bool> cancel{false};
    gui::DatasetPrep prep(&prog, gui::RunFilms{}, cancel);
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
}

// A sibling holding the same PNGs under another name IS a camera, so only
// the name guard keeps mask_edits/ and feature_masks/ out of the list.
void test_camera_scan_skips_mask_edits() {
    const fs::path root = scratch("scan");
    write_jpg(root / "cam0" / "f0.jpg", 16, 12, 0);
    write_jpg(root / "cam1" / "f0.jpg", 16, 12, 1);
    for (const char* dir : {mk::kLayerDirName, gui::kFeatureMaskDirName, "lookalike"})
        for (const char* f : {"cam0/f0.base.png", "cam0/f0.drop.png", "cam0/f0.keep.png"})
            write_png(root / dir / f, 16, 12, 255);
    const std::vector<std::string> cams = gui::camera_subfolders(root.string());
    std::string listed;
    for (const std::string& c : cams) listed += c + " ";
    check(std::find(cams.begin(), cams.end(), "lookalike/cam0") != cams.end(),
          "fixture: the layer PNGs under another name are taken for a camera: " + listed);
    bool edits = false;
    for (const std::string& c : cams)
        edits |= c.rfind(mk::kLayerDirName, 0) == 0 || c.rfind(gui::kFeatureMaskDirName, 0) == 0;
    check(!edits, "camera scan: nothing under mask_edits/ or feature_masks/: " + listed);
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

// feature_masks/ is the run's own, so a workspace holding only it is one to
// resume and one "clear this project" empties; and COLMAP, which reads one
// mask tree, gets it intersected with masks/ -- the flipped one read flipped.
void test_feature_masks_workspace() {
    const int W = 8, H = 4;
    const fs::path ws = scratch("featmasks");
    write_jpg(ws / "images" / "cam0" / "a.jpg", W, H, 0);
    write_jpg(ws / "images" / "cam0" / "b.jpg", W, H, 1);
    write_jpg(ws / "images" / "cam0" / "c.jpg", W, H, 2);
    auto png = [&](const fs::path& p, const std::vector<uint8_t>& px) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
        stbi_write_png(p.string().c_str(), W, H, 1, px.data(), W);
    };
    png(ws / gui::kFeatureMaskDirName / "cam0" / "a.png", box(W, H, 0, 2, W, H));
    png(ws / gui::kFeatureMaskDirName / "cam0" / "b.png", box(W, H, 0, 0, W, H));
    const gui::WorkspaceState st = gui::probe_workspace(ws.string(), {});
    const std::vector<std::string> arts = gui::workspace_artifacts(ws.string(), {});
    check(st.masks && st.resumable(), "probe: feature_masks/ alone is resumable");
    check(std::find(arts.begin(), arts.end(), (ws / gui::kFeatureMaskDirName).string()) !=
              arts.end(),
          "artifacts: feature_masks/ is the run's to clear");

    // masks/ marks what to REMOVE here: the left half of a, all of c.
    png(ws / "masks" / "cam0" / "a.png", box(W, H, 0, 0, W / 2, H));
    png(ws / "masks" / "cam0" / "c.png", box(W, H, 0, 0, W, H));
    std::string err;
    const fs::path out = ws / ".colmap_masks";
    const int64_t n = app::intersect_mask_trees(
        (ws / "images").string(), (ws / "masks").string(), /*flip_a=*/true,
        (ws / gui::kFeatureMaskDirName).string(), out.string(), nullptr, err);
    check(n == 3, "intersect: one mask per image that has either: " + std::to_string(n));
    int w = 0, h = 0;
    std::vector<uint8_t> a, b, c;
    app::load_stencil((out / "cam0" / "a.png").string(), w, h, a);
    app::load_stencil((out / "cam0" / "b.png").string(), w, h, b);
    app::load_stencil((out / "cam0" / "c.png").string(), w, h, c);
    check(a.size() == (size_t)W * H && at(a, W, 6, 3) == 255 && at(a, W, 1, 3) == 0 &&
              at(a, W, 6, 0) == 0,
          "intersect: a keeps only the right half of its lower rows");
    check(b.size() == (size_t)W * H && at(b, W, 0, 0) == 255 && at(b, W, 7, 3) == 255,
          "intersect: b, with no masks/ file, is its feature mask");
    check(c.size() == (size_t)W * H && at(c, W, 3, 2) == 0,
          "intersect: c, with no feature mask, is its flipped mask");
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

int main() {
    test_rerun_reapplies_corrections();
    test_camera_scan_skips_mask_edits();
    test_per_camera_stencil();
    test_feature_masks_workspace();
    test_model_masking_needs_segmentation();
    test_heif_folder();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
