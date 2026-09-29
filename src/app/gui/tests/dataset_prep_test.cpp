// dataset_prep_test -- where DatasetPrep (app/gui/DatasetPrep.h) meets the mask
// editor's layer folder, and the colour record it writes for the trainer. Real
// DatasetPrep::run, no model: the re-mask is the frame stencil.

#include "app/FrameMask.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/mask/MaskLayer.h"
#include "core/SourcePath.h"
#include "data/DatasetColor.h"
#include "external/stb_image_write.h"
#include "i18n/catalog/Log.h"
#include "i18n/catalog/MaskEdit.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <cstdio>
#include <filesystem>
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
// the name guard keeps mask_edits/ out of the list.
void test_camera_scan_skips_mask_edits() {
    const fs::path root = scratch("scan");
    write_jpg(root / "cam0" / "f0.jpg", 16, 12, 0);
    write_jpg(root / "cam1" / "f0.jpg", 16, 12, 1);
    for (const char* dir : {mk::kLayerDirName, "lookalike"})
        for (const char* f : {"cam0/f0.base.png", "cam0/f0.drop.png", "cam0/f0.keep.png"})
            write_png(root / dir / f, 16, 12, 255);
    const std::vector<std::string> cams = gui::camera_subfolders(root.string());
    std::string listed;
    for (const std::string& c : cams) listed += c + " ";
    check(std::find(cams.begin(), cams.end(), "lookalike/cam0") != cams.end(),
          "fixture: the layer PNGs under another name are taken for a camera: " + listed);
    bool edits = false;
    for (const std::string& c : cams) edits |= c.rfind(mk::kLayerDirName, 0) == 0;
    check(!edits, "camera scan: nothing under mask_edits/ is listed: " + listed);
    check(cams.size() == 3 && cams[0] == "cam0" && cams[1] == "cam1",
          "camera scan: cam0, cam1 and the lookalike, nothing else: " + listed);
    check(gui::is_mask_edits_folder((root / mk::kLayerDirName).string()) &&
              !gui::is_mask_edits_folder((root / "lookalike").string()),
          "is_mask_edits_folder: by name");
}

bool has_color_record(const fs::path& ws) {
    return fs::exists(ws / spirula::kDatasetColorFile);
}

// Photos carry no profile, so a run over them must not leave an earlier
// dataset's D-Log M record behind for the trainer to read.
void test_run_rewrites_color_record() {
    const fs::path root = scratch("color");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(photos / ("p" + std::to_string(i) + ".jpg"), 32, 24, i);
    fs::create_directories(ws);
    std::ofstream(ws / spirula::kDatasetColorFile) << "dlogm 19 dvtm_oq101.proto old.OSV\n";
    check(spirula::summarize_dataset_color(spirula::read_dataset_color(ws.string())).verdict ==
              spirula::DatasetColorVerdict::DlogM,
          "fixture: a stale D-Log M record is in the dataset");
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "photos run: " + err);
    check(!has_color_record(ws), "run: a photos-only prep removes the stale colour record");
    const std::vector<std::string> arts = gui::workspace_artifacts(ws.string(), job.inputs);
    std::ofstream(ws / spirula::kDatasetColorFile) << "dlogm 19 dvtm_oq101.proto old.OSV\n";
    const std::vector<std::string> arts2 = gui::workspace_artifacts(ws.string(), job.inputs);
    const std::string rec = (ws / spirula::kDatasetColorFile).string();
    check(std::find(arts.begin(), arts.end(), rec) == arts.end() &&
              std::find(arts2.begin(), arts2.end(), rec) != arts2.end(),
          "clear project: the colour record is one of the run's artifacts");
}

// Review M8: an input dropped from the job leaves no frames to be trained on.
void test_removed_input_frames_go() {
    const fs::path root = scratch("removed");
    const fs::path ws = root / "dataset";
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    for (const char* name : {"a", "b"}) {
        for (int i = 0; i < 3; i++)
            write_jpg(root / name / (std::string(name) + std::to_string(i) + ".jpg"), 32, 24, i);
        gui::PrepInput in;
        in.path = (root / name).string();
        in.subdir = name;
        job.inputs.push_back(in);
    }
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "two inputs: " + err);
    check(fs::is_directory(ws / "images" / "b"), "fixture: input b's frames are in images/b");

    job.inputs.pop_back();   // as the GUI hands it over: a lone input gets images/ itself
    job.inputs[0].subdir.clear();
    check(run_prep(job, prog, err), "one input: " + err);
    check(!fs::exists(ws / "images" / "b"), "removed input: its frames are gone from images/");
    check(fs::exists(ws / "images" / "a0.jpg"), "removed input: the remaining input's frames are there");
}

// An interrupted prep leaves frames but no .spirula-frames; the next run must
// still prune them.
void test_interrupted_prep_prunes() {
    const fs::path root = scratch("interrupted");
    const fs::path ws = root / "dataset";
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    for (const char* name : {"a", "b"}) {
        for (int i = 0; i < 3; i++)
            write_jpg(root / name / (std::string(name) + std::to_string(i) + ".jpg"), 32, 24, i);
        gui::PrepInput in;
        in.path = (root / name).string();
        in.subdir = name;
        job.inputs.push_back(in);
    }
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "interrupted: first run: " + err);
    fs::remove(ws / gui::kFramesStampFile);   // killed before the stamp was written
    check(fs::is_directory(ws / "images" / "b") && !fs::exists(ws / gui::kFramesStampFile),
          "fixture: frames of a and b, no frames stamp");

    job.inputs.pop_back();   // as the GUI hands it over: a lone input gets images/ itself
    job.inputs[0].subdir.clear();
    prog.drain();
    check(run_prep(job, prog, err), "interrupted: re-prep of a alone: " + err);
    check(!fs::exists(ws / "images" / "b"),
          "interrupted prep: the re-prep prunes the removed input's frames");
    check(fs::exists(ws / "images" / "a0.jpg"),
          "interrupted prep: the remaining input's frames are there");
}

// A lone input's own camera folders survive an unstamped prune, others go: the
// frames are not called stale, so a resume can still keep finished inputs.
void test_interrupted_lone_input_keeps_cameras() {
    const fs::path root = scratch("lone");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (const char* cam : {"cam0", "cam1"})
        for (int i = 0; i < 3; i++)
            write_jpg(photos / cam / ("p" + std::to_string(i) + ".jpg"), 32, 24, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "lone: first run: " + err);
    const fs::path sentinel = ws / "images" / "cam0" / "kept.txt";
    std::ofstream(sentinel) << "x";
    write_jpg(ws / "images" / "old" / "o0.jpg", 32, 24, 9);
    fs::remove(ws / gui::kFramesStampFile);
    check(fs::is_directory(ws / "images" / "cam1"), "fixture: the input's cameras are cam0/ and cam1/");
    check(run_prep(job, prog, err), "lone: re-prep: " + err);
    check(!fs::exists(ws / "images" / "old") && fs::exists(sentinel),
          "interrupted lone input: its camera folders stay, a leftover folder goes");
}

// The same for a lone video, whose cameras are cam<k>/. Pruning runs before
// extraction, so the run failing here for want of a decoder does not matter.
void test_interrupted_lone_video_keeps_cameras() {
    const fs::path root = scratch("lonevideo");
    const fs::path ws = root / "dataset", images = ws / "images";
    write_jpg(images / "cam0" / "f0.jpg", 32, 24, 1);
    write_jpg(images / "cam1" / "f0.jpg", 32, 24, 2);
    write_jpg(images / "old" / "o0.jpg", 32, 24, 3);
    write_jpg(images / "r0.jpg", 32, 24, 5);   // a single-track frame, also its own
    write_jpg(root / "clip.OSV", 32, 24, 4);   // never decoded
    gui::PrepJob job;
    job.workspace = ws.string();
    gui::PrepInput in;
    in.path = (root / "clip.OSV").string();
    in.is_video = true;
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    run_prep(job, prog, err);
    check(!fs::exists(images / "old") && fs::exists(images / "cam0" / "f0.jpg") &&
              fs::exists(images / "cam1" / "f0.jpg") && fs::exists(images / "r0.jpg"),
          "interrupted lone video: its cam<k>/ folders stay, a leftover folder goes");
}

// A removed input's folder can be a link into a raw capture: the link goes,
// what it points at is never touched.
void test_stale_dir_symlink_unlinked() {
    const fs::path root = scratch("symlink");
    const fs::path ws = root / "dataset", raw = root / "raw";
    for (int i = 0; i < 3; i++) write_jpg(raw / ("r" + std::to_string(i) + ".jpg"), 32, 24, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    for (const char* name : {"a", "b"}) {
        for (int i = 0; i < 3; i++)
            write_jpg(root / name / (std::string(name) + std::to_string(i) + ".jpg"), 32, 24, i);
        gui::PrepInput in;
        in.path = (root / name).string();
        in.subdir = name;
        job.inputs.push_back(in);
    }
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "symlink: first run: " + err);
    std::error_code ec;
    fs::remove_all(ws / "images" / "b", ec);
    fs::create_directory_symlink(raw, ws / "images" / "b", ec);
    check(!ec && fs::is_symlink(ws / "images" / "b"), "fixture: images/b is a link to the raw folder");

    job.inputs.pop_back();
    prog.drain();
    check(run_prep(job, prog, err), "symlink: re-prep of a alone: " + err);
    check(!fs::exists(fs::symlink_status(ws / "images" / "b")),
          "stale dir symlink: the link itself is removed");
    check(fs::exists(raw / "r0.jpg") && fs::exists(raw / "r2.jpg"),
          "stale dir symlink: its target is untouched");
    const std::string line = spirula::i18n::format(spirula::i18n::msg::log::frames_link_removed,
                                                   {std::string("b"), raw.string()});
    check(logged(prog, line), "stale dir symlink: the removal is logged with its target");
}

// I1: a record that cannot be replaced or removed would be read as current.
void test_stale_record_fails_prep() {
    const fs::path root = scratch("stale");
    const fs::path photos = root / "photos", ws = root / "dataset";
    for (int i = 0; i < 3; i++) write_jpg(photos / ("p" + std::to_string(i) + ".jpg"), 32, 24, i);
    fs::create_directories(ws / spirula::kDatasetColorFile / "blocker");
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = photos.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    const bool ok = run_prep(job, prog, err);
    check(!ok && err.find(spirula::kDatasetColorFile) != std::string::npos,
          "run: an unremovable colour record fails the prep and names it");
}

// The prune must never touch an input's own photos, which can be images/.
void test_prune_spares_input_folder() {
    const fs::path root = scratch("spare");
    const fs::path ws = root / "dataset", images = ws / "images";
    for (int i = 0; i < 3; i++) write_jpg(images / ("p" + std::to_string(i) + ".jpg"), 32, 24, i);
    gui::PrepJob job;
    job.workspace = ws.string();
    job.photo_import = gui::PhotoImport::Copy;
    gui::PrepInput in;
    in.path = images.string();
    job.inputs = {in};
    gui::RunProgress prog;
    std::string err;
    check(run_prep(job, prog, err), "photos in images/: first run: " + err);
    job.redo_frames = true;   // frames stale, as a changed input list makes them
    run_prep(job, prog, err);
    check(fs::exists(images / "p0.jpg") && fs::exists(images / "p2.jpg"),
          "prune: an input read from images/ itself is never removed");
}

// What each file's djmd would say; the real reader (sfm::video_color) is
// sfm_telemetry_test's, on synthetic and real clips.
sfm::VideoColor fake_djmd(const std::string& path) {
    sfm::VideoColor c;
    const std::string f = fs::path(path).filename().string();
    if (f == "dlogm.OSV")  { c.mode = sfm::VideoColorMode::DlogM; c.code = 19; }
    if (f == "normal.OSV") { c.mode = sfm::VideoColorMode::Normal; c.code = 0; }
    if (f == "dlog2.OSV")  { c.mode = sfm::VideoColorMode::OtherLog; c.code = 22; }
    if (f == "wa530.OSV") {
        c.mode = sfm::VideoColorMode::Unknown;
        c.issue = sfm::VideoColorIssue::UnknownLayout;
    }
    if (f != "plain.mp4") c.proto = f == "wa530.OSV" ? "dvtm_wa530.proto" : "dvtm_oq101.proto";
    return c;
}

void test_clip_colors_mapping() {
    const fs::path root = scratch("mapping");
    struct Case { const char* file; spirula::ClipColor want; int code; };
    const Case cases[] = {
        {"dlogm.OSV", spirula::ClipColor::DlogM, 19},
        {"normal.OSV", spirula::ClipColor::Normal, 0},
        {"dlog2.OSV", spirula::ClipColor::OtherLog, 22},
        {"wa530.OSV", spirula::ClipColor::Unknown, -1},
        {"plain.mp4", spirula::ClipColor::NotRecorded, -1},
    };
    std::vector<gui::PrepInput> inputs;
    for (const Case& c : cases) {
        gui::PrepInput in;
        in.path = (root / c.file).string();
        in.is_video = true;
        inputs.push_back(in);
    }
    gui::PrepInput photos;
    photos.path = (root / "dlogm.OSV").string();   // same name, but a folder of photos
    inputs.push_back(photos);
    std::vector<std::string> notes;
    const spirula::DatasetColor d = gui::clip_colors(inputs, &notes, fake_djmd);
    bool all = d.clips.size() == 6;
    for (size_t i = 0; all && i < 5; i++)
        all = d.clips[i].mode == cases[i].want && d.clips[i].code == cases[i].code &&
              d.clips[i].source == cases[i].file;
    check(all, "clip_colors: D-Log M, Normal, D-Log2, unknown-layout and plain videos map to their modes");
    check(d.clips.size() == 6 && d.clips[3].proto == "dvtm_wa530.proto" && d.clips[4].proto.empty(),
          "clip_colors: the metadata layout is recorded");
    check(d.clips.size() == 6 && d.clips[5].mode == spirula::ClipColor::NotRecorded,
          "clip_colors: a photo folder is never read as a video");
    check(notes.size() == 1 && notes[0].find("wa530.OSV") != std::string::npos,
          "clip_colors: the unreadable input is logged");
}

}  // namespace

int main() {
    test_rerun_reapplies_corrections();
    test_camera_scan_skips_mask_edits();
    test_run_rewrites_color_record();
    test_removed_input_frames_go();
    test_stale_record_fails_prep();
    test_interrupted_prep_prunes();
    test_interrupted_lone_input_keeps_cameras();
    test_interrupted_lone_video_keeps_cameras();
    test_stale_dir_symlink_unlinked();
    test_prune_spares_input_folder();
    test_clip_colors_mapping();
    std::printf("%s: %d failure(s)\n", SS_FILE, g_failures);
    return g_failures;
}
