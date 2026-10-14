// dataset_prep_test -- where DatasetPrep (app/gui/DatasetPrep.h) meets the
// folders beside images/: a re-run re-applies the mask editor's corrections
// over the masks it rewrites, the camera scan never takes mask_edits/ or
// feature_masks/ for a camera, and COLMAP's one mask tree is the two ANDed.
// Real DatasetPrep::run, no model: the re-mask is the frame stencil, which is
// also checked per camera folder. Built without SS_BUILD_SAM, so it also
// checks that asking for a model fails; that `sparse/0` is not a dataset; and
// what becomes of the pictures a blur threshold leaves out.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/PickSheet.h"
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

bool run_prep(const gui::PrepJob& job, gui::RunProgress& prog, std::string& error,
              gui::PickSheet* picks = nullptr) {
    std::atomic<bool> cancel{false};
    gui::RunFilms films;
    films.picks = picks;
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
        gui::set_blur_mode(job, gui::kBlurStrict);
        job.keep_skipped = c.keep;
        gui::PrepInput in;
        in.path = photos.string();
        job.inputs = {in};
        gui::RunProgress prog;
        std::string err;
        const std::string n = c.name;
        gui::PickSheet sheet;
        check(run_prep(job, prog, err, &sheet), n + ": run: " + err);
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
        gui::set_blur_mode(again, gui::kBlurKeep);
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
    gui::set_blur_mode(sort, gui::kBlurStrict);
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
    gui::set_blur_mode(sort, gui::kBlurStrict);
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

int main() {
    test_bare_model_is_not_a_dataset();
    test_rerun_reapplies_corrections();
    test_camera_scan_skips_mask_edits();
    test_per_camera_stencil();
    test_feature_masks_workspace();
    test_model_masking_needs_segmentation();
    test_heif_folder();
    test_skipped_photos();
    test_pick_sheet_discards();
    test_sort_in_place();
    test_sort_rig_in_place();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
