// SfmRunner.cpp -- see SfmRunner.h.

#include "app/gui/SfmRunner.h"

#include "app/gui/SfmInProcess.h"

#include "sfm/core/Manifest.h"

#include <fstream>

#include "app/gui/ReconStamp.h"

#include "i18n/Locale.h"
#include "i18n/catalog/Log.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#ifdef SS_TOOL_SFM
// For the stage tags the child prints; a build without the module has no child
// to read (see availability()).
#include "sfm/core/Log.h"
#include "i18n/catalog/Sfm.h"
#endif

#if defined(SS_TOOL_SFM) && defined(SS_HAVE_ALIKED) && SS_HAVE_ALIKED
#include "aliked/model/Fetch.h"
#include "loma/Loma.h"
#include "loma/model/Fetch.h"
#include "nn/io/Fetch.h"
#endif

#ifndef _WIN32
#include <ftw.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;
using spirula::i18n::format;

// Shorthand: every log line that carries a path or a count is a format()
// call, and there are enough of them here to be worth a short name.
inline std::string fmt(const spirula::i18n::Msg& m,
                       std::initializer_list<spirula::i18n::Arg> a) {
    return format(m, a);
}

namespace gui {

namespace {

const char* kQuality[] = {"low", "medium", "high", "extreme"};
const char* kDataType[] = {"individual", "video", "internet"};
const char* kCameraMode[] = {"single", "folder", "image"};
const char* kPairs[] = {"auto", "exhaustive", "sequential", "prefilter"};
const char* kMapper[] = {"flat", "bottom-up"};
const char* kFeatures[] = {"sift", "aliked-n16rot", "aliked-n32", "loma-b128",
                           "loma-b"};
const char* kMetricGps[] = {"none", "horizontal", "full"};
const char* kSensorGauge[] = {"none", "up", "auto"};

template <int N>
const char* pick(const char* const (&table)[N], int i, int fallback = 0) {
    return table[(i >= 0 && i < N) ? i : fallback];
}

// The matcher combo is two entries -- brute force, or "the learned matcher for
// this frontend" -- because a learned matcher only reads the descriptors it
// was trained on. Which one that is follows --features.
const char* matcher_for(int features, int matcher) {
    if (features == 0 || matcher != 1) return "bruteforce";
    const std::string f = pick(kFeatures, features);
    return f.rfind("loma", 0) == 0 ? pick(kFeatures, features) : "lightglue";
}

// A failed Vulkan call is the child's own English diagnostic -- a result name
// and a source line -- not interface copy, so it is matched as one.
bool child_line_is_gpu_failure(const std::string& l) {
    return l.find("VK_ERROR_DEVICE_LOST") != std::string::npos ||
           l.find("VK_ERROR_OUT_OF_DEVICE_MEMORY") != std::string::npos;
}

// Worth showing to somebody who is not debugging: the child's own [run] block
// and anything it flagged. Both halves are asked of the catalog the child
// printed from, so a `--lang ja` run classifies as well as an English one.
bool child_line_is_notable(const std::string& l) {
#ifndef SS_TOOL_SFM
    (void)l;
    return false;
#else
    const std::string run = sfm::slog::prefix(sfm::slog::Tag::Run);
    if (l.compare(0, run.size(), run) == 0) return true;
    for (const char* word : {spirula::i18n::msg::sfm::word_warning.get(),
                             spirula::i18n::msg::sfm::word_error.get()}) {
        const size_t at = l.find(word);
        // After the tag column, not anywhere: a path with "error" in it is not
        // a warning.
        if (at != std::string::npos && at <= run.size() + 2) return true;
    }
    return false;
#endif
}

// NOT std::filesystem::remove_all -- on the torch build libtorch.so interposes
// an ABI-incompatible copy (see AGENTS.md gotchas).
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

// The mapper only writes a model when it finishes, so any model on disk is
// from a completed run.
bool has_model(const fs::path& sparse) {
    std::error_code ec;
    for (fs::directory_iterator it(sparse, ec), end; !ec && it != end;
         it.increment(ec))
        if (fs::exists(it->path() / "cameras.bin", ec)) return true;
    return false;
}

}  // namespace

std::vector<PendingDownload> sfm_feature_downloads(int features, int matcher) {
    std::vector<PendingDownload> out;
#if defined(SS_TOOL_SFM) && defined(SS_HAVE_ALIKED) && SS_HAVE_ALIKED
    auto take = [&](const nn::FetchFile& f) {
        const std::string dest = nn::cached_path(f);
        if (!file_is_cached(dest, f.bytes)) out.push_back({f.url, dest, f.bytes});
    };
    auto want_aliked = [&](const char* id) {
        if (const aliked::ModelSource* src = aliked::find_model_source(id)) take(src->onnx);
    };
    auto want_loma = [&](const std::string& id) {
        if (const loma::ModelSource* src = loma::find_model_source(id)) take(src->onnx);
    };
    if (features <= 0) return out;

    const std::string f = pick(kFeatures, features);
    if (f.rfind("loma", 0) == 0) {
        // Three files, not one: the detector is shared, the descriptor follows
        // the variant, and the matcher is only wanted if it is selected.
        want_loma("loma-dad");
        want_loma(loma::descriptor_for_matcher(f));
        if (matcher == 1) want_loma(f);
    } else {
        want_aliked(f.c_str());
        if (matcher == 1) want_aliked("aliked-lightglue");
    }
#else
    (void)features;
    (void)matcher;
#endif
    return out;
}

bool sfm_features_cached(int features, int matcher) {
    return sfm_feature_downloads(features, matcher).empty();
}

std::string SfmRunner::availability() {
#ifndef SS_TOOL_SFM
    return lmsg::err_no_sfm_module.get();
#else
    if (app::exe_path().empty())
        return lmsg::err_no_exe_path.get();
    return "";
#endif
}

SfmRunner::~SfmRunner() {
    cancel();
    if (_worker.joinable()) _worker.join();
}

void SfmRunner::start(const SfmJob& job, RunFilms films) {
    if (_state.load() == State::Running) return;
    if (_worker.joinable()) _worker.join();
    _cancel = false;
    _partial = false;
    _not_metric = false;
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
        _progress_dir.clear();
        _features_dir.clear();
        _matches_path.clear();
        _sfm_image_dir.clear();
        _sfm_mask_dir.clear();
        _sweep_dir.clear();
        _live = job;
    }
    _state = State::Running;
    _worker = std::thread([this, job] { run(job); });
}

void SfmRunner::update(const SfmJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    _live = job;
}

void SfmRunner::take_reconstruction(SfmJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    job.quality = _live.quality;
    job.data_type = _live.data_type;
    job.camera_model = _live.camera_model;
    job.camera_mode = _live.camera_mode;
    job.pairs = _live.pairs;
    job.overlap = _live.overlap;
    job.loop_closure = _live.loop_closure;
    job.init_focal_px = _live.init_focal_px;
    job.init_distortion = _live.init_distortion;
    job.distortion_refine = _live.distortion_refine;
    job.final_per_image_intrinsics = _live.final_per_image_intrinsics;
    job.final_free_rig = _live.final_free_rig;
    job.max_features = _live.max_features;
    job.max_image_size = _live.max_image_size;
    job.mapper = _live.mapper;
    job.features = _live.features;
    job.matcher = _live.matcher;
    job.metric_gps = _live.metric_gps;
    job.sensor_gauge = _live.sensor_gauge;
    job.keep_intermediate = _live.keep_intermediate;
    job.ba_cpu = _live.ba_cpu;
    job.extra_args = _live.extra_args;
    // The lens is a reconstruction setting that happens to be stored on the
    // input it describes. The list itself cannot change while a run is live.
    for (size_t i = 0; i < job.prep.inputs.size() &&
                       i < _live.prep.inputs.size(); i++) {
        job.prep.inputs[i].camera_model = _live.prep.inputs[i].camera_model;
        job.prep.inputs[i].focal_factor = _live.prep.inputs[i].focal_factor;
        job.prep.inputs[i].subcameras = _live.prep.inputs[i].subcameras;
    }
}

void SfmRunner::take_geometry(SfmJob& job) {
    std::lock_guard<std::mutex> lk(_mu);
    job.geometry = _live.geometry;
}

void SfmRunner::take_masking(PrepJob& prep) {
    std::lock_guard<std::mutex> lk(_mu);
    prep.mask_enable = _live.prep.mask_enable;
    prep.mask_prompt = _live.prep.mask_prompt;
    prep.mask_negative_prompt = _live.prep.mask_negative_prompt;
    prep.mask_keep_subject = _live.prep.mask_keep_subject;
    prep.mask_max_image_size = _live.prep.mask_max_image_size;
    prep.mask_dilate_ratio = _live.prep.mask_dilate_ratio;
    prep.mask_threshold = _live.prep.mask_threshold;
    prep.mask_nms = _live.prep.mask_nms;
    prep.mask_memory = _live.prep.mask_memory;
    prep.mask_detect_every = _live.prep.mask_detect_every;
    prep.mask_memory_frames = _live.prep.mask_memory_frames;
    prep.mask_clicks = _live.prep.mask_clicks;
    prep.mask_model_path = _live.prep.mask_model_path;
    prep.mask_model_name = _live.prep.mask_model_name;
    prep.force_external_masking = _live.prep.force_external_masking;
    prep.python_exe = _live.prep.python_exe;
}

void SfmRunner::cancel() { _cancel = true; }

std::string SfmRunner::stage() {
    return _prog.stage(_prog.current()).detail;
}
float SfmRunner::progress() const {
    return _prog.stage(_prog.current()).fraction;
}
std::string SfmRunner::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}
std::string SfmRunner::dataset_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _dataset_dir;
}
std::string SfmRunner::image_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _image_dir;
}
bool SfmRunner::mask_flipped() const { return _mask_flipped.load(); }

std::string SfmRunner::mask_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _mask_dir;
}
std::string SfmRunner::progress_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _progress_dir;
}

std::string SfmRunner::thumbs_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _progress_dir.empty() ? std::string()
                                : (fs::path(_progress_dir) / "thumbs").string();
}

std::string SfmRunner::live_matches_path() {
    std::lock_guard<std::mutex> lk(_mu);
    return _progress_dir.empty() ? std::string()
                                : (fs::path(_progress_dir) / "live_matches.bin").string();
}
std::string SfmRunner::features_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _features_dir;
}
std::string SfmRunner::matches_path() {
    std::lock_guard<std::mutex> lk(_mu);
    return _matches_path;
}
std::string SfmRunner::sfm_image_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _sfm_image_dir;
}
std::string SfmRunner::sfm_mask_dir() {
    std::lock_guard<std::mutex> lk(_mu);
    return _sfm_mask_dir;
}

void SfmRunner::sweep_intermediates() {
    if (_state.load() == State::Running) return;
    std::string ws;
    {
        std::lock_guard<std::mutex> lk(_mu);
        ws.swap(_sweep_dir);
    }
    if (ws.empty()) return;
    const fs::path dir(ws);
    remove_tree(dir / ".progress");
    // Recursive: the feature files MIRROR the image tree, so a capture with
    // camera folders puts them in features/cam0/... and a single-level sweep
    // removed nothing and left the directory.
    remove_tree(dir / "features");
    std::error_code ec;
    fs::remove(dir / "matches.bin", ec);
}
void SfmRunner::log(const std::string& line, bool detail) {
    _prog.note(line, detail);
}

void SfmRunner::set_stage_if_new(Stage st, const char* s) {
    if (_prog.current() == st && _prog.stage(st).detail == s) return;
    set_stage(st, s);
}

void SfmRunner::set_stage(Stage st, const std::string& s) {
    _prog.enter(st, s);
    // One line per step, and the skeleton the default log view is read as:
    // without it a step whose own output is all detail looks like nothing
    // happening at all.
    log("==== " + s + " ====", /*detail=*/false);
}

// Where the run is, from the snapshot the child writes (status.bin) rather
// than from its translated stdout. Cheap to call per output line: one stat,
// and a read only when the file actually moved.
void SfmRunner::poll_status() {
    RunStatus st;
    if (read_status(_progress_dir, _status_mtime, st)) apply_status(st);
}

// One consumer for both transports: the child's status.bin and the in-process
// event fold say the same thing in the same shape.
void SfmRunner::apply_status(const RunStatus& st) {
    switch (st.stage) {
        case 0: set_stage_if_new(Stage::Features, lmsg::stage_finding_features.get());
                _prog.count(Stage::Features, st.done, st.total); break;
        case 1: set_stage_if_new(Stage::Matching, lmsg::stage_matching_images.get());
                _prog.count(Stage::Matching, st.done, st.total); break;
        case 2: case 3: case 4:
                set_stage_if_new(Stage::Mapping, lmsg::stage_reconstructing.get());
                _prog.count(Stage::Mapping, st.done, st.total); break;
        default: break;
    }
    if (st.finished) {
        _partial = st.partial;
        _not_metric = !st.metric;
        _have_status = true;
    }
}

// The panel's per-input rows become the manifest's camera groups, keyed on the
// sub-folder each input's frames went into. The focal is a fraction of the
// width up to here: the width is not known until the frames exist.
sfm::Manifest SfmRunner::build_manifest(const SfmJob& job, const PrepResult& prep) {
    sfm::Manifest man;
    man.image_dir = prep.image_dir;
    if (!prep.mask_dir.empty()) {
        man.mask_dir = prep.mask_dir;
        man.has_mask_flipped = true;
        man.mask_flipped = prep.mask_dir_flipped;
    }
    // The rows the panel showed, and the model each one resolved to once "same
    // as above" was followed through the list (camera_group_models).
    const std::vector<CameraGroup> groups = camera_groups(job.prep.inputs);
    const std::vector<std::string> models =
        camera_group_models(job.prep.inputs, groups, job.camera_model);

    for (size_t i = 0; i < groups.size(); i++) {
        const CameraGroup& g = groups[i];
        const float focal = group_focal(job.prep.inputs, g);
        sfm::ManifestCamera c;
        c.prefix = g.rel;
        // For the whole capture the panel's own "Camera / lens" is the single
        // source of truth and is already in the argv; only a named group adds one.
        if (!g.rel.empty() && !models[i].empty()) c.model = models[i];
        if (focal > 0 && !(g.rel.empty() && job.init_focal_px > 0)) {
            const std::string dir =
                (g.rel.empty() ? fs::path(prep.image_dir)
                               : fs::path(prep.image_dir) / g.rel).string();
            int W = 0, H = 0;
            if (!DatasetPrep::first_image_dims(dir, W, H)) {
                log(fmt(lmsg::sfm_focal_unreadable, {dir}));
            } else {
                c.focal = (double)focal * W;
                char buf[32];
                std::snprintf(buf, sizeof buf, "%g", c.focal);
                log(fmt(lmsg::sfm_initial_focal,
                        {g.rel.empty() ? lmsg::sfm_the_capture.get() : g.rel.c_str(),
                         buf, focal, (long long)W}));
            }
        }
        if (!c.model.empty() || c.focal > 0) man.cameras.push_back(std::move(c));
    }
    for (const PrepCapture& pc : prep.captures) {
        sfm::ManifestCapture c;
        c.prefix = pc.subdir;
        c.telemetry = pc.path;
        c.fps = pc.fps;
        man.captures.push_back(std::move(c));
    }
    man.rigs = build_rigs(job.prep);
    return man;
}

// The rows' rig choices as definitions (sfm/core/Rig.h): "this input's
// lenses" is a rig per input; a shared letter joins rows across inputs, as
// captures of one rig when every input contributes the same lens folders.
std::vector<sfm::RigDef> SfmRunner::build_rigs(const PrepJob& prep) {
    std::vector<sfm::RigDef> out;
    auto join = [](const std::string& a, const std::string& b) {
        return a.empty() ? b : b.empty() ? a : a + "/" + b;
    };
    // Per input, the lens folders under its subdir: the sub-camera rows, or a
    // video's tracks / views.
    std::vector<std::vector<std::string>> lenses(prep.inputs.size());
    for (size_t i = 0; i < prep.inputs.size(); i++) {
        const PrepInput& in = prep.inputs[i];
        if (!in.subcameras.empty()) continue;
        lenses[i] = lens_dirs(prep, in);
    }
    // "This input's lenses": one rig per input.
    for (size_t i = 0; i < prep.inputs.size(); i++) {
        const PrepInput& in = prep.inputs[i];
        std::vector<std::string> members;
        if (in.subcameras.empty()) {
            if (in.rig == kRigOwn) members = lenses[i];
        } else {
            for (const SubCamera& sc : in.subcameras)
                if (sc.rig == kRigOwn) members.push_back(sc.rel);
        }
        if (members.size() < 2) continue;
        sfm::RigDef d;
        d.name = in.subdir.empty() ? std::string("rig") : in.subdir;
        for (const std::string& m : members) {
            sfm::RigMemberDef md;
            md.prefix = join(in.subdir, m);
            d.members.push_back(md);
        }
        out.push_back(std::move(d));
    }
    // The shared letters.
    for (int letter = 0; letter < kRigShared; letter++) {
        const int id = kRigFirstShared + letter;
        // input -> the lens folders it contributes under this letter
        std::vector<std::pair<size_t, std::vector<std::string>>> parts;
        for (size_t i = 0; i < prep.inputs.size(); i++) {
            const PrepInput& in = prep.inputs[i];
            std::vector<std::string> mine;
            if (in.subcameras.empty()) {
                if (in.rig == id) mine = lenses[i].empty() ? std::vector<std::string>{""} : lenses[i];
            } else {
                for (const SubCamera& sc : in.subcameras)
                    if (sc.rig == id) mine.push_back(sc.rel);
            }
            if (!mine.empty()) parts.push_back({i, std::move(mine)});
        }
        if (parts.empty()) continue;
        sfm::RigDef d;
        d.name = std::string(1, (char)('A' + letter));
        bool same = parts.size() > 1 && parts[0].second.size() > 1;
        for (const auto& p : parts) same = same && p.second == parts[0].second;
        if (same) {
            // One rig behind several inputs: captures, and members relative
            // to each.
            for (const auto& p : parts) d.captures.push_back(prep.inputs[p.first].subdir);
            for (const std::string& m : parts[0].second) {
                sfm::RigMemberDef md;
                md.prefix = m;
                d.members.push_back(md);
            }
        } else {
            for (const auto& p : parts)
                for (const std::string& m : p.second) {
                    sfm::RigMemberDef md;
                    md.prefix = join(prep.inputs[p.first].subdir, m);
                    d.members.push_back(md);
                }
        }
        if (d.members.size() < 2) continue;
        out.push_back(std::move(d));
    }
    return out;
}

// The flags that describe the MODEL rather than where it goes. The command
// line and the workspace's stamp are both made from this, so they cannot drift.
std::vector<std::string> SfmRunner::recon_args(const SfmJob& job,
                                               const PrepResult& prep) {
    std::vector<std::string> argv = {
        "--quality", pick(kQuality, job.quality, 2),
        "--data-type", pick(kDataType, job.data_type),
        "--camera-model", job.camera_model,
        "--camera-mode", pick(kCameraMode, job.camera_mode, 1),
        "--mapper", pick(kMapper, job.mapper),
        "--features", pick(kFeatures, job.features),
        // A learned matcher only exists for the learned descriptors; asking
        // for one with SIFT selected is a usage error.
        "--matcher", matcher_for(job.features, job.matcher),
    };
    if (job.pairs > 0) {
        argv.push_back("--pairs");
        argv.push_back(pick(kPairs, job.pairs));
        if (job.pairs == 2) {
            argv.push_back("--overlap");
            argv.push_back(std::to_string(job.overlap));
        }
    }
    // Sequential is what `auto` resolves to for video, so this has to be
    // passed whenever sequential is reachable, not only when it was named. It
    // is a no-op under the other pair modes.
    if (!job.loop_closure) argv.push_back("--no-loop-closure");
    if (job.init_focal_px > 0) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", job.init_focal_px);
        argv.push_back("--focal");
        argv.push_back(buf);
    }
    if (!job.init_distortion.empty()) {
        argv.push_back("--distortion");
        argv.push_back(job.init_distortion);
    }
    // Two flags, three states: hold during mapping, and hold in the finishing
    // pass as well.
    if (job.distortion_refine >= 1) argv.push_back("--no-refine-extra-params");
    if (job.distortion_refine >= 2) argv.push_back("--no-final-extra-params");
    if (job.final_per_image_intrinsics)
        argv.push_back("--final-per-image-intrinsics");
    if (job.final_free_rig) argv.push_back("--final-free-rig");
    if (job.ba_cpu) {
        argv.push_back("--ba-real");
        argv.push_back("cpu");
        argv.push_back("--ba-real-coarse");
        argv.push_back("cpu");
    }
    // Not flags any more: the groups go in the manifest. Its text joins the
    // stamp so that changing a lens still counts as a different model
    // (recon_stamp_change), which is the whole point of this list.
#ifdef SS_TOOL_SFM
    const std::string manifest = sfm::manifest_write(build_manifest(job, prep));
    if (!manifest.empty()) {
        argv.push_back("--manifest");
        argv.push_back(manifest);
    }
#endif
    if (job.max_features > 0) {
        // Each frontend has its own count flag: the budgets are not comparable,
        // a learned detector emitting a few thousand better-localized points
        // where SIFT wants tens of thousands. One spinner, routed to the live one.
        argv.push_back(job.features == 0     ? "--max-features"
                       : job.features >= 3   ? "--loma-max-features"
                                             : "--aliked-max-features");
        argv.push_back(std::to_string(job.max_features));
    }
    if (job.max_image_size > 0) {
        argv.push_back("--max-image-size");
        argv.push_back(std::to_string(job.max_image_size));
    }
    if (job.metric_gps > 0) {
        argv.push_back("--metric-gps");
        argv.push_back(pick(kMetricGps, job.metric_gps));
    }
    if (job.sensor_gauge != 2) {
        argv.push_back("--sensor-gauge");
        argv.push_back(pick(kSensorGauge, job.sensor_gauge, 2));
    }
    if (!job.image_gamut.empty()) {
        argv.push_back("--image-gamut");
        argv.push_back(job.image_gamut);
    }
    if (job.image_is_linear.has_value())
        argv.push_back(*job.image_is_linear ? "--image-linear"
                                            : "--no-image-linear");
    if (job.point_color_in_image_space) {
        argv.push_back("--point-color");
        argv.push_back("image");
    }
    if (!prep.mask_dir.empty()) {
        argv.push_back("--masks");
        argv.push_back(prep.mask_dir);
        // Only masks the run handed on untouched are still the other way
        // round; anything it wrote is in the usual convention.
        if (prep.mask_dir_flipped) argv.push_back("--flip-mask");
    } else {
        // Otherwise `auto` picks up a stale masks/ sitting beside the images
        // from an earlier run with masking on.
        argv.push_back("--no-masks");
    }
    for (const std::string& a : split_args(job.extra_args))
        argv.push_back(a);
    return argv;
}

void SfmRunner::run(SfmJob job) {
    auto fail = [&](const std::string& why) {
        _prog.finish(_cancel.load() ? StageStatus::Skipped : StageStatus::Failed);
        std::lock_guard<std::mutex> lk(_mu);
        _error = why;
        _state = _cancel.load() ? State::Cancelled : State::Failed;
    };

    try {
        const fs::path ws = job.prep.workspace;
        std::error_code ec;
        fs::create_directories(ws, ec);

        if (std::string why = availability(); !why.empty()) return fail(why);

        // What was there before this run touched anything. Not a reason to
        // refuse: neither an existing model nor the input's own images, which
        // the dataset is deliberately written next to.
        const WorkspaceState prior = probe_workspace(ws.string(), job.prep.inputs);
        if (prior.resumable() && !job.prep.resume)
            return fail(lmsg::err_unfinished_run.get());
        if (prior.resumable())
            log(fmt(lmsg::sfm_resuming, {ws.string()}), /*detail=*/false);

        // Where the child will write its snapshots, and the two files it
        // leaves behind. Published before the stages that produce them, so the
        // screen is already watching when the first one lands.
        {
            std::lock_guard<std::mutex> lk(_mu);
            _progress_dir = (ws / ".progress").string();
            _features_dir = (ws / "features").string();
            _matches_path = (ws / "matches.bin").string();
        }

        // ---- 1. frames and masks ------------------------------------------
        PrepResult prep;
        {
            DatasetPrep dp(&_prog, _films, _cancel);
            std::string err;
            if (!dp.run(job.prep, prep, err,
                        [this](PrepJob& p) { take_masking(p); }))
                return fail(err);
        }
        {
            // The folders the previews draw from, published as soon as they
            // exist rather than beside the reconstruction: a run that finds a
            // model already there skips that step, and the screen still wants
            // to show the images it kept.
            std::lock_guard<std::mutex> lk(_mu);
            _sfm_image_dir = prep.image_dir;
            _sfm_mask_dir = prep.mask_dir;
        }
        if (prep.per_folder_cameras && job.camera_mode == 0) {
            log(lmsg::one_camera_per_folder.get());
            job.camera_mode = 1;
        }

        // ---- 2. reconstruction --------------------------------------------
        take_reconstruction(job);
        // The model, as the flags that make it. A copy stays in the workspace
        // beside it (ReconStamp.h) so that a later run can tell whether the
        // one already there still answers what the panel is asking for.
        ReconStamp now;
        now.present = true;
        now.engine = "builtin";
        now.args = recon_args(job, prep);
        const std::string changed =
            recon_stamp_change(read_recon_stamp(ws.string()), now);

        // A model already there is reused whoever made it, which is how a
        // finished dataset gets masks and geometry. Not one this panel would now
        // build differently; one with no stamp says nothing and is reused still.
        const bool reuse_model = prior.model && !job.redo_model && changed.empty();
        if (reuse_model) {
            log(fmt(lmsg::sfm_reusing_model, {ws.string()}), /*detail=*/false);
        } else {
            if (prior.model && !changed.empty())
                log(fmt(lmsg::sfm_settings_changed, {changed}), /*detail=*/false);
            set_stage(Stage::Features, lmsg::stage_reconstructing_features.get());
            // Matching reads every .bin under features/, so one an interrupted
            // run left for an image this one no longer has would join it as a
            // phantom view. Nothing here is reused; start from none of it.
            remove_tree(ws / "features");
            remove_tree(ws / ".progress");
            fs::remove(ws / "matches.bin", ec);
            // From here the intermediates are this run's, however it ends: a
            // cancelled run leaves the same ones a finished one does, and the
            // screen goes on reading both until it is done with them.
            {
                std::lock_guard<std::mutex> lk(_mu);
                _sweep_dir = job.keep_intermediate ? "" : ws.string();
            }
            // What the run is asked for, the same list either way. The
            // manifest travels as TEXT in the stamp, because that is what
            // defines the model; a run wants a file.
            std::vector<std::string> settings = {
                prep.image_dir, "-o", ws.string(),
                "--progress-dir", (ws / ".progress").string(),
            };
            for (size_t k = 0; k < now.args.size(); k++) {
                settings.push_back(now.args[k]);
                if (now.args[k] != "--manifest" || k + 1 >= now.args.size()) continue;
                // Dotted and prefixed, like .spirula_mask.py: the workspace
                // is the user's, and a plain manifest.yaml there could be theirs.
                const fs::path mf = ws / ".spirula_manifest.yaml";
                std::ofstream(mf, std::ios::binary | std::ios::trunc) << now.args[++k];
                settings.push_back(mf.string());
            }

            // What to advise on failure depends on which stage lost the
            // device: a CPU bundle adjustment is no answer to one lost while
            // matching.
            bool mapping = false, gpu_failure = false;
            auto note_line = [&](const std::string& l) {
                mapping = mapping || _prog.current() == Stage::Mapping;
                if (mapping && child_line_is_gpu_failure(l)) gpu_failure = true;
            };
            int rc = 0;
            if (job.subprocess) {
                std::vector<std::string> argv = {
                    // The child is this same executable, so it has the same
                    // thirteen languages -- tell it which one, or its output
                    // lands in the log in whatever the machine's locale is.
                    app::exe_path(), "--lang",
                    spirula::i18n::code(spirula::i18n::current()), "sfm", "auto",
                };
                argv.insert(argv.end(), settings.begin(), settings.end());
                std::string cmd;
                for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
                log(cmd);
                rc = run_process(argv, "", [&](const std::string& l) {
                    log(l, !child_line_is_notable(l));
                    poll_status();
                    note_line(l);
                }, _cancel);
                // The last snapshot the child wrote, which is the one carrying
                // its verdict; the loop above may have missed it between lines.
                _status_mtime = 0;
                poll_status();
                if (rc == kSpawnFailed)
                    return fail(fmt(lmsg::err_spawn_recon, {argv[0]}));
            } else {
                const InProcessResult r = run_sfm_in_process(
                    settings,
                    [&](const std::string& l) {
                        log(l, !child_line_is_notable(l));
                        note_line(l);
                    },
                    [this](const RunStatus& st) { apply_status(st); }, _cancel);
                rc = r.exit_code;
                if (r.cancelled) rc = kCancelled;
                if (!r.error.empty()) return fail(r.error);
            }
            if (rc == kCancelled || _cancel.load())
                return fail(lmsg::err_cancelled.get());
            // Neither 3 (under half the images registered, or a high
            // reprojection error) nor 4 (no metric frame) is a failure here:
            // the model still trains, and it cost an hour (src/sfm/README.md).
            if (rc == 3 || rc == 4) {
                // status.bin reports both facts; the exit code can carry only
                // one of them, and 3 wins when a run is partial AND unscaled.
                if (!_have_status) {
                    _partial = rc == 3;
                    _not_metric = rc == 4;
                }
                if (_partial) log(lmsg::sfm_partial.get());
                if (_not_metric) log(lmsg::sfm_not_metric.get());
            } else if (rc != 0) {
                return fail(gpu_failure
                                ? fmt(lmsg::err_recon_gpu,
                                      {spirula::i18n::msg::dataset::sfm_ba_cpu.get()})
                                : lmsg::err_recon_failed.get());
            }
        }

        // Only for a run that reconstructed: a reused model may be a
        // transforms.json or a Metashape export, which has no sparse/ at all.
        if (!reuse_model && !has_model(ws / "sparse"))
            return fail(lmsg::err_no_reconstruction.get());
        if (!reuse_model) write_recon_stamp(ws.string(), now);

        // ---- 3. depth and normals -------------------------------------------
        take_geometry(job);
        if (job.geometry.enable) {
            std::string err;
            if (!run_geometry_step(job.geometry, ws.string(), prep.image_dir,
                                   _prog, _films.geometry, _cancel, err))
                return fail(err);
        }

        // ---- 4. tidy up ----------------------------------------------------
        // Swept by sweep_intermediates(), not here: the screen goes on
        // reading the snapshots and matches.bin after the run ends.
        {
            std::lock_guard<std::mutex> lk(_mu);
            _sweep_dir = job.keep_intermediate ? "" : ws.string();
        }

        if (reads_photos_in_place(job.prep.inputs, job.prep.photo_import))
            log(fmt(lmsg::photos_referenced_in_place, {prep.image_dir_cfg}));

        set_stage(Stage::Finishing, lmsg::stage_done.get());
        _prog.finish(StageStatus::Done);
        {
            std::lock_guard<std::mutex> lk(_mu);
            _dataset_dir = ws.string();
            _image_dir = prep.image_dir_cfg;
            _mask_dir = prep.mask_dir_cfg;
            _mask_flipped = prep.mask_dir_flipped;
        }
        _state = State::Done;
    } catch (const std::exception& e) {
        fail(e.what());
    }
}

}  // namespace gui
