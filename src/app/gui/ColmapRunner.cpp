// ColmapRunner.cpp -- see ColmapRunner.h. CLI flags mirror
// reference/scripts/run_colmap.bash (COLMAP >= 4.x; use_gpu-style flags are
// gone).

#include "app/gui/ColmapRunner.h"

#include "i18n/catalog/Log.h"
#include "app/AppPaths.h"
#include "app/gui/DatasetPrep.h"
#include "app/gui/Subprocess.h"

#ifndef _WIN32
#include <ftw.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;

namespace gui {

namespace {

// NOT std::filesystem::remove_all -- on the torch build libtorch.so
// interposes an ABI-incompatible copy (see app/README.md gotchas).
void remove_tree(const fs::path& p) {
#ifndef _WIN32
    nftw(p.string().c_str(),
         [](const char* f, const struct stat*, int, struct FTW*) {
             return ::remove(f);
         }, 16, FTW_DEPTH | FTW_PHYS);
#else
    std::error_code ec;
    std::filesystem::remove_all(p, ec);
#endif
}

const char* kVocabTreeName = "vocab_tree_faiss_flickr100K_words256K.bin";
const char* kVocabTreeUrl =
    "https://github.com/colmap/colmap/releases/download/3.11.1/"
    "vocab_tree_faiss_flickr100K_words256K.bin";

// Initial ImageReader.camera_params for a model: given focal length and a
// centered principal point, all distortion coefficients zero. Order follows
// COLMAP's camera model definitions.
std::string compose_camera_params(const std::string& model, double f,
                                  double cx, double cy) {
    struct M { const char* name; int n_focal; int n_extra; };
    static const M kModels[] = {
        {"SIMPLE_PINHOLE", 1, 0},        {"PINHOLE", 2, 0},
        {"SIMPLE_RADIAL", 1, 1},         {"RADIAL", 1, 2},
        {"OPENCV", 2, 4},                {"FULL_OPENCV", 2, 8},
        {"OPENCV_FISHEYE", 2, 4},        {"THIN_PRISM_FISHEYE", 2, 8},
        {"SIMPLE_RADIAL_FISHEYE", 1, 1}, {"RADIAL_FISHEYE", 1, 2},
    };
    for (const M& m : kModels) {
        if (model != m.name) continue;
        char buf[64];
        std::string out;
        for (int i = 0; i < m.n_focal; i++) {
            std::snprintf(buf, sizeof buf, "%.4f,", f);
            out += buf;
        }
        std::snprintf(buf, sizeof buf, "%.4f,%.4f", cx, cy);
        out += buf;
        for (int i = 0; i < m.n_extra; i++) out += ",0";
        return out;
    }
    return "";
}

bool is_fisheye_model(const std::string& m) {
    return m.find("FISHEYE") != std::string::npos;
}

// The images one camera can cover: the folder --camera-mode asked for, split
// by frame size, since a principal point is not shared across sizes.
struct CameraGroup {
    std::string folder;   // empty under one shared camera
    int w = 0, h = 0;
    std::vector<std::string> names;
};

std::vector<CameraGroup> camera_groups(
        const std::vector<DatasetPrep::ImageSize>& images, bool per_folder) {
    std::vector<CameraGroup> out;
    for (const DatasetPrep::ImageSize& im : images) {
        std::string folder;
        if (per_folder) {
            const size_t slash = im.name.find_last_of('/');
            if (slash != std::string::npos) folder = im.name.substr(0, slash);
        }
        CameraGroup* g = nullptr;
        for (CameraGroup& c : out)
            if (c.folder == folder && c.w == im.w && c.h == im.h) { g = &c; break; }
        if (!g) {
            out.push_back(CameraGroup{folder, im.w, im.h, {}});
            g = &out.back();
        }
        g->names.push_back(im.name);
    }
    return out;
}

// Registered-image count of a COLMAP model dir (uint64 head of images.bin;
// same trick as ColmapParser's largest-model pick).
int64_t model_num_images(const fs::path& dir) {
    FILE* f = std::fopen((dir / "images.bin").string().c_str(), "rb");
    if (!f) return 0;
    uint64_t n = 0;
    size_t got = std::fread(&n, sizeof n, 1, f);
    std::fclose(f);
    return got == 1 ? (int64_t)n : 0;
}

}  // namespace

ColmapRunner::~ColmapRunner() {
    cancel();
    if (_worker.joinable()) _worker.join();
}

void ColmapRunner::start(const ColmapJob& job, RunFilms films) {
    if (_state.load() == State::Running) return;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    _films = films;
    _prog.reset();
    if (_films.frames) _films.frames->clear();
    if (_films.masks) _films.masks->clear();
    if (_films.geometry) _films.geometry->clear();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _dataset_dir.clear();
        _image_dir.clear();
        _mask_dir.clear();
        _live = job;
    }
    _state = State::Running;
    _worker = std::thread([this, job] { run(job); });
}

void ColmapRunner::update(const ColmapJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    _live = job;
}

void ColmapRunner::take_reconstruction(ColmapJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    // Everything from "Cameras" down in ColmapJob: what feature extraction and
    // everything after it reads. The inputs, the workspace and the video
    // settings are the run and stay as they were started.
    const std::vector<PrepInput> inputs = job.inputs;
    const std::string workspace = job.workspace;
    const bool resume = job.resume;
    const float fps = job.video_fps;
    const int sharp = job.sharp_window, maxf = job.max_frames;
    job = _live;
    job.inputs = inputs;
    job.workspace = workspace;
    job.resume = resume;
    job.video_fps = fps;
    job.sharp_window = sharp;
    job.max_frames = maxf;
}

void ColmapRunner::take_geometry(ColmapJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    job.geometry = _live.geometry;
}

void ColmapRunner::take_masking(PrepJob& prep) {
    std::lock_guard<std::mutex> lk(_mu);
    prep.mask_enable = _live.mask_enable;
    prep.mask_prompt = _live.mask_prompt;
    prep.mask_negative_prompt = _live.mask_negative_prompt;
    prep.mask_keep_subject = _live.mask_keep_subject;
    prep.mask_max_image_size = _live.mask_max_image_size;
    prep.mask_threshold = _live.mask_threshold;
    prep.mask_nms = _live.mask_nms;
    prep.mask_memory = _live.mask_memory;
    prep.mask_detect_every = _live.mask_detect_every;
    prep.mask_memory_frames = _live.mask_memory_frames;
    prep.mask_clicks = _live.mask_clicks;
    prep.image_gamut = _live.image_gamut;
    prep.image_is_linear = _live.image_is_linear;
    prep.mask_model_path = _live.mask_model_path;
    prep.mask_model_name = _live.mask_model;
    prep.force_external_masking = _live.force_external_masking;
    prep.python_exe = _live.python_exe;
}

void ColmapRunner::cancel() { _cancel = true; }

std::string ColmapRunner::stage() {
    return _prog.stage(_prog.current()).detail;
}
std::string ColmapRunner::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}
std::string ColmapRunner::dataset_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _dataset_dir;
}
std::string ColmapRunner::image_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _image_dir;
}
bool ColmapRunner::mask_flipped() const { return _mask_flipped.load(); }

std::string ColmapRunner::mask_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _mask_dir;
}
void ColmapRunner::log(const std::string& line, bool detail) {
    _prog.note(line, detail);
}

void ColmapRunner::set_stage(Stage st, const std::string& s) {
    _prog.enter(st, s);
    log("==== " + s + " ====", /*detail=*/false);
}

int ColmapRunner::exec(const std::vector<std::string>& argv) {
    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);
    return run_process(argv, "", [this](const std::string& l) { log(l); }, _cancel);
}

// COLMAP >= 4 required: 3.x still had the SiftExtraction.use_gpu-era CLI and
// misses flags this pipeline passes (run_colmap.bash targets 4.x).
bool ColmapRunner::check_colmap_version(const ColmapJob& job, std::string& err) {
    if (!command_exists(job.colmap_exe)) {
        err = "colmap not found ('" + job.colmap_exe +
              "'); install COLMAP 4.x or set its path under Tool Locations";
        return false;
    }
    int major = -1, minor = -1;
    run_process({job.colmap_exe, "help"}, "",
                [&](const std::string& l) {
                    if (major >= 0) return;
                    const char* p = std::strstr(l.c_str(), "COLMAP ");
                    if (p) std::sscanf(p, "COLMAP %d.%d", &major, &minor);
                },
                _cancel);
    if (major < 0) {
        log("warning: could not detect the COLMAP version; continuing anyway");
        return true;
    }
    log("Detected COLMAP " + std::to_string(major) + "." + std::to_string(minor));
    if (major < 4) {
        err = "COLMAP " + std::to_string(major) + "." + std::to_string(minor) +
              " found, but version 4.x or newer is required (the command-line "
              "options changed). Please upgrade COLMAP.";
        return false;
    }
    return true;
}

// Resolve the vocab-tree file: explicit path > found near the workspace or
// in the app cache > downloaded into the cache (curl).
std::string ColmapRunner::resolve_vocab_tree(const ColmapJob& job) {
    std::error_code ec;
    if (!job.vocab_tree_path.empty()) {
        if (fs::exists(job.vocab_tree_path, ec)) return job.vocab_tree_path;
        log("vocab tree not found at '" + job.vocab_tree_path + "'");
        return "";
    }
    fs::path ws = job.workspace;
    for (const fs::path& dir : {ws, ws.parent_path(), fs::path(app::cache_dir())}) {
        if (!fs::is_directory(dir, ec)) continue;
        for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
             it.increment(ec)) {
            std::string name = it->path().filename().string();
            if (name.rfind("vocab_tree", 0) == 0 &&
                it->path().extension() == ".bin") {
                log("Found vocabulary tree: " + it->path().string());
                return it->path().string();
            }
        }
    }
    // Download into the cache.
    fs::path dst = fs::path(app::cache_dir()) / kVocabTreeName;
    set_stage(Stage::Matching, lmsg::stage_vocab_download.get());
    if (!command_exists("curl")) {
        log("curl not found -- download it manually:");
        log(std::string("  ") + kVocabTreeUrl + " -> " + dst.string());
        return "";
    }
    fs::path tmp = dst;
    tmp += ".part";
    int rc = exec({"curl", "-L", "-f", "--progress-bar",
                   "-o", tmp.string(), kVocabTreeUrl});
    if (rc != 0) {
        fs::remove(tmp, ec);
        log("vocabulary tree download failed");
        return "";
    }
    fs::rename(tmp, dst, ec);
    return ec ? "" : dst.string();
}

double ColmapRunner::model_reproj_error(const ColmapJob& job,
                                        const std::string& model) {
    double err = 1e30;
    run_process({job.colmap_exe, "model_analyzer", "--path", model}, "",
                [&](const std::string& l) {
                    const char* key = "Mean reprojection error:";
                    size_t p = l.find(key);
                    if (p == std::string::npos) return;
                    try { err = std::stod(l.substr(p + std::strlen(key))); }
                    catch (...) {}
                },
                _cancel);
    return err;
}

void ColmapRunner::run(ColmapJob job) {
    auto fail = [&](const std::string& why) {
        _prog.finish(_cancel.load() ? StageStatus::Skipped : StageStatus::Failed);
        std::lock_guard<std::mutex> lk(_mu);
        _error = why;
        _state = _cancel.load() ? State::Cancelled : State::Failed;
    };
    try {
        const fs::path ws = job.workspace;
        fs::create_directories(ws);

        // Resume reuses what the last run completed, or a clean folder is
        // insisted on; a model already there is reused whoever made it, and the
        // input's own images are not leftovers (see SfmRunner).
        const WorkspaceState prior = probe_workspace(ws.string(), job.inputs);
        const bool reuse_model = prior.model && !job.redo_model;
        if (prior.resumable() && !job.resume)
            return fail("the workspace already contains an unfinished run "
                        "(database.db / extracted frames / masks); enable "
                        "\"Resume previous run\" to reuse it, or choose "
                        "another folder");
        if (prior.resumable())
            log("Resuming previous run in " + ws.string() +
                " (completed stages are reused)");
        if (reuse_model)
            log(spirula::i18n::format(lmsg::sfm_reusing_model, {ws.string()}));

        std::string err;
        if (!reuse_model && !check_colmap_version(job, err)) return fail(err);

        // ---- 1. frames and masks (shared with the built-in SfM path) -------
        PrepResult prep;
        {
            PrepJob pj;
            pj.inputs = job.inputs;
            pj.workspace = job.workspace;
            pj.resume = job.resume;
            pj.redo_frames = job.redo_frames;
            pj.redo_masks = job.redo_masks;
            pj.video_fps = job.video_fps;
            pj.sharp_window = job.sharp_window;
            pj.max_frames = job.max_frames;
            pj.ffmpeg_exe = job.ffmpeg_exe;
            pj.force_external_decode = job.force_external_decode;
            pj.mask_enable = job.mask_enable;
            pj.mask_prompt = job.mask_prompt;
            pj.mask_negative_prompt = job.mask_negative_prompt;
            pj.mask_keep_subject = job.mask_keep_subject;
            pj.mask_max_image_size = job.mask_max_image_size;
            pj.mask_threshold = job.mask_threshold;
            pj.mask_nms = job.mask_nms;
            pj.mask_memory = job.mask_memory;
            pj.mask_detect_every = job.mask_detect_every;
            pj.mask_memory_frames = job.mask_memory_frames;
            pj.mask_clicks = job.mask_clicks;
            pj.mask_model_path = job.mask_model_path;
            pj.mask_model_name = job.mask_model;
            pj.force_external_masking = job.force_external_masking;
            pj.python_exe = job.python_exe;

            DatasetPrep dp(&_prog, _films, _cancel);
            if (!dp.run(pj, prep, err, [this](PrepJob& p) { take_masking(p); }))
                return fail(err);
        }
        const std::string images = prep.image_dir;
        const std::string image_dir_cfg = prep.image_dir_cfg;
        const int n_images = prep.n_images;
        const bool have_masks = !prep.mask_dir.empty();
        const std::string mask_dir_cfg = prep.mask_dir_cfg;
        _mask_flipped = prep.mask_dir_flipped;
        if (prep.per_folder_cameras && job.camera_mode == 0) {
            log(lmsg::one_camera_per_folder.get());
            job.camera_mode = 1;
        }

        // Everything COLMAP does, skipped whole for a run pointed at a
        // finished dataset: extracting and matching into a database that is not
        // there costs an hour to arrive at the model already sitting there.
        if (!reuse_model) {
            // Quality knobs (per run_colmap.bash: fewer features = much faster
            // matching; O(n^2) in feature count). ALIKED extracts far fewer,
            // higher-quality keypoints than SIFT.
            bool aliked = job.feature_type == 1;
            int features = job.max_num_features > 0 ? job.max_num_features
                         : aliked ? (job.quality == 0 ? 1024
                                   : job.quality == 1 ? 2048 : 4096)
                         : (job.quality == 0 ? 4096
                          : job.quality == 1 ? 8192 : 16384);
            const std::string db = (ws / "database.db").string();

            // An explicit ImageReader.camera_params wins; otherwise compose one
            // from the focal factor per frame size -- the factor is a fraction
            // of the width, so it describes a size and not the run.
            auto add_camera_params = [&](std::vector<std::string>& fe,
                                         int W, int H) {
                std::string p = job.camera_params;
                if (p.empty() && job.init_focal_factor > 0) {
                    if (W <= 0) {
                        log("warning: could not read an image size; skipping "
                            "the initial focal length");
                        return;
                    }
                    p = compose_camera_params(job.camera_model,
                                              (double)job.init_focal_factor * W,
                                              0.5 * W, 0.5 * H);
                    if (p.empty()) {
                        log("warning: no camera_params template for " +
                            job.camera_model +
                            "; skipping the initial focal length");
                        return;
                    }
                    log("Initial camera (" + job.camera_model + ", " +
                        std::to_string(W) + "x" + std::to_string(H) + "): " + p);
                }
                if (p.empty()) return;
                fe.push_back("--ImageReader.camera_params");
                fe.push_back(p);
            };

            // COLMAP's ImageReader drops every image whose frame size differs
            // from the first in its camera group -- one warning per image, exit
            // code 0, half a capture missing. Split by size here instead.
            std::vector<CameraGroup> groups;
            size_t folders = 0;
            if (job.camera_mode == 0 || job.camera_mode == 1) {
                groups = camera_groups(
                    DatasetPrep::image_sizes(images, prep.mask_dir),
                    job.camera_mode == 1);
                std::set<std::string> seen;
                for (const CameraGroup& g : groups) seen.insert(g.folder);
                folders = seen.size();
            }
            const bool split_sizes = groups.size() > folders;
            if (split_sizes)
                log(spirula::i18n::format(lmsg::colmap_split_frame_sizes,
                                          {(long long)groups.size()}));

            // ---- 3. feature extraction -----------------------------------------
            take_reconstruction(job);
            set_stage(Stage::Features,
                      aliked ? lmsg::stage_colmap_features_aliked.get()
                             : lmsg::stage_colmap_features.get());
            std::vector<std::string> shared = {job.colmap_exe, "feature_extractor",
                "--database_path", db,
                "--image_path", images,
                "--ImageReader.camera_model", job.camera_model};
            if (aliked) {
                shared.push_back("--FeatureExtraction.type");
                shared.push_back("ALIKED");
                shared.push_back("--AlikedExtraction.max_num_features");
                shared.push_back(std::to_string(features));
            } else {
                shared.push_back("--SiftExtraction.max_num_features");
                shared.push_back(std::to_string(features));
            }
            int size_cap = job.max_image_size > 0 ? job.max_image_size
                         : job.quality == 0 ? 2000 : 0;
            if (size_cap > 0) {
                shared.push_back("--FeatureExtraction.max_image_size");
                shared.push_back(std::to_string(size_cap));
            }
            if (job.estimate_affine_shape && !aliked) {
                shared.push_back("--SiftExtraction.estimate_affine_shape");
                shared.push_back("1");
            }
            if (have_masks) {
                shared.push_back("--ImageReader.mask_path");
                shared.push_back(prep.mask_dir);
            }
            int rc = 0;
            std::vector<std::vector<std::string>> passes;
            if (!split_sizes) {
                std::vector<std::string> fe = shared;
                int W = 0, H = 0;
                if (job.camera_params.empty() && job.init_focal_factor > 0)
                    DatasetPrep::first_image_dims(images, W, H);
                add_camera_params(fe, W, H);
                if (job.camera_mode == 0) {
                    fe.push_back("--ImageReader.single_camera");
                    fe.push_back("1");
                } else if (job.camera_mode == 1) {
                    fe.push_back("--ImageReader.single_camera_per_folder");
                    fe.push_back("1");
                }
                passes.push_back(std::move(fe));
            } else {
                const fs::path lists = ws / ".camera_groups";
                fs::create_directories(lists);
                for (size_t i = 0; i < groups.size(); i++) {
                    const fs::path lf =
                        lists / ("group" + std::to_string(i) + ".txt");
                    std::ofstream f(lf);
                    for (const std::string& n : groups[i].names) f << n << "\n";
                    f.close();
                    if (!f) return fail("could not write " + lf.string());
                    std::vector<std::string> fe = shared;
                    add_camera_params(fe, groups[i].w, groups[i].h);
                    fe.push_back("--ImageReader.single_camera");
                    fe.push_back("1");
                    fe.push_back("--image_list_path");
                    fe.push_back(lf.string());
                    passes.push_back(std::move(fe));
                }
            }
            for (const std::vector<std::string>& fe : passes) {
                rc = exec(fe);
                if (rc == kCancelled) return fail("cancelled");
                if (rc != 0) return fail("colmap feature_extractor failed (see log)");
            }

            // ---- 4. matching -----------------------------------------------------
            // An explicit choice: the GUI presets sequential for video and
            // exhaustive for photos. The vocabulary tree is SIFT-only.
            bool any_video = false;
            for (const PrepInput& in : job.inputs) any_video = any_video || in.is_video;
            int matcher = job.matcher;
            if (matcher < 1 || matcher > 3)
                matcher = any_video ? 2 : (n_images <= 400 ? 1 : 3);
            std::string match_type;   // FeatureMatching.type ("" = COLMAP default)
            if (aliked)
                match_type = job.lightglue ? "ALIKED_LIGHTGLUE" : "ALIKED_BRUTEFORCE";
            else if (job.lightglue)
                match_type = "SIFT_LIGHTGLUE";
            std::vector<std::string> ma;
            if (matcher == 3) {
                if (aliked)
                    return fail("vocabulary-tree matching requires SIFT features "
                                "(the tree indexes SIFT descriptors); pick the "
                                "sequential or exhaustive matcher for ALIKED");
                std::string vt = resolve_vocab_tree(job);
                if (vt.empty())
                    return fail("no vocabulary tree available (see log)");
                set_stage(Stage::Matching, lmsg::stage_match_vocab.get());
                ma = {job.colmap_exe, "vocab_tree_matcher",
                      "--database_path", db,
                      "--VocabTreeMatching.vocab_tree_path", vt};
            }
            if (matcher == 2) {
                std::string vt;
                if (job.seq_loop_closure) {
                    if (aliked)
                        log("note: loop detection needs SIFT descriptors "
                            "(vocabulary tree); relying on quadratic overlap "
                            "for loop closure");
                    else if ((vt = resolve_vocab_tree(job)).empty())
                        log("warning: no vocabulary tree; loop detection disabled");
                }
                set_stage(Stage::Matching, lmsg::stage_match_sequential.get());
                ma = {job.colmap_exe, "sequential_matcher",
                      "--database_path", db,
                      "--SequentialMatching.overlap", std::to_string(job.seq_overlap),
                      "--SequentialMatching.quadratic_overlap",
                      job.seq_quadratic_overlap ? "1" : "0"};
                if (!vt.empty()) {
                    ma.push_back("--SequentialMatching.loop_detection");
                    ma.push_back("1");
                    ma.push_back("--SequentialMatching.vocab_tree_path");
                    ma.push_back(vt);
                }
            } else if (matcher == 1) {
                set_stage(Stage::Matching, lmsg::stage_match_exhaustive.get());
                ma = {job.colmap_exe, "exhaustive_matcher", "--database_path", db};
            }
            if (!match_type.empty()) {
                ma.push_back("--FeatureMatching.type");
                ma.push_back(match_type);
            }
            if (job.estimate_affine_shape && !aliked) {
                ma.push_back("--FeatureMatching.guided_matching");
                ma.push_back("1");
            }
            // Wrong-match suppression (repetitive scenes).
            if (job.match_max_ratio > 0 && !aliked) {
                char b[16];
                std::snprintf(b, sizeof b, "%g", job.match_max_ratio);
                ma.push_back("--SiftMatching.max_ratio");
                ma.push_back(b);
            }
            if (job.min_inliers_per_pair > 0) {
                ma.push_back("--TwoViewGeometry.min_num_inliers");
                ma.push_back(std::to_string(job.min_inliers_per_pair));
            }
            rc = exec(ma);
            if (rc == kCancelled) return fail("cancelled");
            if (rc != 0) return fail("colmap matcher failed (see log)");

            // ---- 5. sparse reconstruction ----------------------------------------
            set_stage(Stage::Mapping, lmsg::stage_colmap_mapper.get());
            fs::create_directories(ws / "sparse");
            bool fisheye = is_fisheye_model(job.camera_model);
            bool ba_gpu = job.ba_use_gpu;
            if (ba_gpu && fisheye) {
                ba_gpu = false;
                log("note: COLMAP's GPU bundle adjustment does not support "
                    "fisheye camera models; using the CPU backend");
            }
            // Low-distortion perspective models map more stably with the
            // coefficients FIXED, the final pass recovering them; fisheye models
            // need theirs refined during mapping (run_colmap.bash's advice).
            bool no_extra = job.camera_model == "PINHOLE" ||
                            job.camera_model == "SIMPLE_PINHOLE";
            bool fix_extra = !no_extra &&
                             (job.mapper_extra_params == 2 ||
                              (job.mapper_extra_params == 0 && !fisheye));
            std::vector<std::string> mp = {job.colmap_exe, "mapper",
                       "--database_path", db,
                       "--image_path", images,
                       "--output_path", (ws / "sparse").string(),
                       "--Mapper.ba_use_gpu", ba_gpu ? "1" : "0",
                       "--Mapper.structure_less_registration_fallback", "0"};
            if (fix_extra) {
                mp.push_back("--Mapper.ba_refine_extra_params");
                mp.push_back("0");
                log(job.final_bundle_adjust
                    ? "Distortion held fixed during mapping; the final "
                      "refinement pass recovers it"
                    : "warning: distortion held fixed during mapping but the "
                      "final refinement pass is disabled -- coefficients stay "
                      "at their initial values");
            }
            if (job.min_num_matches > 0) {
                mp.push_back("--Mapper.min_num_matches");
                mp.push_back(std::to_string(job.min_num_matches));
            }
            // Stricter image registration (repetitive scenes).
            char fbuf[16];
            if (job.abs_pose_min_num_inliers > 0) {
                mp.push_back("--Mapper.abs_pose_min_num_inliers");
                mp.push_back(std::to_string(job.abs_pose_min_num_inliers));
            }
            if (job.abs_pose_min_inlier_ratio > 0) {
                std::snprintf(fbuf, sizeof fbuf, "%g", job.abs_pose_min_inlier_ratio);
                mp.push_back("--Mapper.abs_pose_min_inlier_ratio");
                mp.push_back(fbuf);
            }
            if (job.abs_pose_max_error > 0) {
                std::snprintf(fbuf, sizeof fbuf, "%g", job.abs_pose_max_error);
                mp.push_back("--Mapper.abs_pose_max_error");
                mp.push_back(fbuf);
            }
            // COLMAP may emit several models (sparse/0, sparse/1, ...); the
            // trainer auto-picks the one with the most registered images.
            auto enumerate_models = [&]() {
                std::vector<std::pair<int64_t, fs::path>> ms;   // (-count, dir)
                std::error_code ec;
                for (fs::directory_iterator it(ws / "sparse", ec), end;
                     !ec && it != end; it.increment(ec))
                    if (it->path().filename().string().rfind(".", 0) != 0 &&
                        fs::exists(it->path() / "cameras.bin"))
                        ms.push_back({-model_num_images(it->path()), it->path()});
                std::sort(ms.begin(), ms.end());
                return ms;
            };
            // The mapper only writes models on completion, so any existing
            // one is from a FINISHED run -- reuse it. An interrupted mapper
            // leaves nothing and simply reruns.
            std::vector<std::pair<int64_t, fs::path>> models;
            if (job.resume && !job.redo_model &&
                !(models = enumerate_models()).empty()) {
                log("Resume: " + std::to_string(models.size()) +
                    " existing model(s) under sparse/; skipping the mapper "
                    "(delete sparse/ to re-reconstruct)");
            } else {
                rc = exec(mp);
                if (rc == kCancelled) return fail("cancelled");
                if (rc != 0) return fail("colmap mapper failed (see log)");
                models = enumerate_models();
            }
            if (models.empty())
                return fail("mapper produced no reconstruction (too few matches? "
                            "try higher quality or more overlapping images)");
            fs::path best = models[0].second;
            if (models.size() > 1) {
                std::string counts;
                for (auto& m : models)
                    counts += (counts.empty() ? "" : ", ") + std::to_string(-m.first);
                log("Note: mapper produced " + std::to_string(models.size()) +
                    " partial models (" + counts + " images)");
            }

            // ---- 6. merge partial models (best effort) ---------------------------
            // model_merger needs two models sharing registered frames, which
            // partials usually lack -- but it recovers a broken-apart one.
            if (models.size() > 1 && job.merge_models) {
                set_stage(Stage::Mapping, lmsg::stage_merge_models.get());
                fs::path cur = best;
                int64_t cur_n = -models[0].first;
                fs::path acc = ws / "sparse" / ".merge_acc";
                fs::path tmp = ws / "sparse" / ".merge_tmp";
                int merged = 0;
                for (size_t i = 1; i < models.size(); i++) {
                    remove_tree(tmp);
                    fs::create_directories(tmp);
                    rc = exec({job.colmap_exe, "model_merger",
                               "--input_path1", cur.string(),
                               "--input_path2", models[i].second.string(),
                               "--output_path", tmp.string()});
                    if (rc == kCancelled) return fail("cancelled");
                    int64_t n = rc == 0 ? model_num_images(tmp) : 0;
                    if (n > cur_n) {
                        remove_tree(acc);
                        std::error_code ec;
                        fs::rename(tmp, acc, ec);
                        if (ec) break;
                        cur = acc;
                        cur_n = n;
                        merged++;
                        log("Merged " + models[i].second.filename().string() +
                            " -> " + std::to_string(n) + " images");
                    } else {
                        log("Could not merge " +
                            models[i].second.filename().string() +
                            " (models share no registered frames); skipped");
                    }
                }
                remove_tree(tmp);
                if (merged > 0) {
                    // Persist as the next sparse/<N> so the trainer's
                    // largest-model auto-pick finds it.
                    int next = 0;
                    while (fs::exists(ws / "sparse" / std::to_string(next))) next++;
                    fs::path dst = ws / "sparse" / std::to_string(next);
                    std::error_code ec;
                    fs::rename(acc, dst, ec);
                    if (!ec) {
                        best = dst;
                        log("Merged model written to sparse/" +
                            std::to_string(next) + " (" + std::to_string(cur_n) +
                            " images); the trainer auto-picks it");
                    }
                }
            }

            // ---- 7. bundle-adjustment refinement ---------------------------------
            // Releases what the mapper fixed, but NOT a fisheye's principal
            // point: with the 8 thin-prism coefficients it diverges.
            if (job.final_bundle_adjust) {
                set_stage(Stage::Finishing, lmsg::stage_bundle_adjust.get());
                fs::path tmp = ws / "sparse" / ".ba_tmp";
                remove_tree(tmp);
                fs::create_directories(tmp);
                rc = exec({job.colmap_exe, "bundle_adjuster",
                           "--input_path", best.string(),
                           "--output_path", tmp.string(),
                           "--BundleAdjustment.refine_focal_length", "1",
                           "--BundleAdjustment.refine_principal_point",
                           fisheye ? "0" : "1",
                           "--BundleAdjustment.refine_extra_params", "1"});
                if (rc == kCancelled) return fail("cancelled");
                double before = model_reproj_error(job, best.string());
                double after = rc == 0 ? model_reproj_error(job, tmp.string()) : 1e30;
                log(spirula::i18n::format(
                    spirula::i18n::msg::log::colmap_reproj_error,
                    {before, after}));
                if (rc == 0 && std::isfinite(after) &&
                    (after <= before || !std::isfinite(before))) {
                    std::error_code ec;
                    for (const char* f : {"cameras.bin", "images.bin",
                                          "points3D.bin", "frames.bin",
                                          "rigs.bin"}) {
                        if (!fs::exists(tmp / f, ec)) continue;
                        fs::rename(tmp / f, best / f, ec);
                    }
                    log("Refinement kept");
                } else {
                    log("warning: bundle_adjuster " +
                        std::string(rc != 0 ? "failed" : "made the model worse") +
                        "; keeping the mapper result");
                }
                remove_tree(tmp);
            }
        }

        // ---- depth and normals ---------------------------------------------
        take_geometry(job);
        if (job.geometry.enable) {
            std::string gerr;
            if (!run_geometry_step(job.geometry, ws.string(), images, _prog,
                                   _films.geometry, _cancel, gerr))
                return fail(gerr);
        }

        // No marker file is written: the image dir is handed to the GUI
        // in-memory for the immediate open; on later re-opens the parser
        // default applies (video datasets use images/ anyway) and photo-in-
        // place datasets need data.image_dir set in the dataparser options.
        if (reads_photos_in_place(job.inputs))
            log("Note: images are referenced in place; when re-opening this "
                "dataset later, set image_dir to " + image_dir_cfg +
                " under the dataset-parsing options");

        set_stage(Stage::Finishing, lmsg::stage_done.get());
        _prog.finish(StageStatus::Done);
        {
            std::lock_guard<std::mutex> lk(_mu);
            _dataset_dir = ws.string();
            _image_dir = image_dir_cfg;
            _mask_dir = mask_dir_cfg;
        }
        _state = State::Done;
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

}  // namespace gui
