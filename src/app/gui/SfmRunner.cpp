// SfmRunner.cpp -- see SfmRunner.h.

#include "app/gui/SfmRunner.h"

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
    job.max_features = _live.max_features;
    job.max_image_size = _live.max_image_size;
    job.mapper = _live.mapper;
    job.features = _live.features;
    job.matcher = _live.matcher;
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

// spirula-sfm's own progress lines. Reading them is a little grubby, but it is
// the one thing a child process cannot hand over structurally.
//
// What it keys on is the stage TAG, and the tag is translated -- so this asks
// the same catalog the child printed from (sfm/core/Log.h), rather than
// matching English text that a `--lang ja` run will never emit. The numbers
// are the first "a/b" on the line, which every progress message puts there and
// no other message on those stages has.
void SfmRunner::note_progress(const std::string& l) {
#ifdef SS_TOOL_SFM
    using sfm::slog::Tag;
    auto tagged = [&l](Tag t) {
        const std::string p = sfm::slog::prefix(t);
        return l.compare(0, p.size(), p) == 0;
    };
    // First "<digits>/<digits>" anywhere in the line.
    auto fraction = [&l](unsigned long& a, unsigned long& b) {
        for (size_t i = 0; i + 2 < l.size(); i++) {
            if (!std::isdigit((unsigned char)l[i])) continue;
            if (i && std::isdigit((unsigned char)l[i - 1])) continue;
            if (std::sscanf(l.c_str() + i, "%lu/%lu", &a, &b) == 2 && b > 0) return true;
            while (i + 1 < l.size() && std::isdigit((unsigned char)l[i + 1])) i++;
        }
        return false;
    };
    unsigned long a = 0, b = 0;
    if (tagged(Tag::Extract)) {
        if (!fraction(a, b)) return;
        set_stage_if_new(Stage::Features, lmsg::stage_finding_features.get());
        _prog.count(Stage::Features, (int64_t)a, (int64_t)b);
        return;
    }
    if (tagged(Tag::Match)) {
        if (!fraction(a, b)) return;
        set_stage_if_new(Stage::Matching, lmsg::stage_matching_images.get());
        _prog.count(Stage::Matching, (int64_t)a, (int64_t)b);
        return;
    }
    if (tagged(Tag::Map) || tagged(Tag::Merge) || tagged(Tag::Orient)) {
        // The mapper counts registrations rather than working towards a
        // total, so this is a stage change and a spinner, not a fraction.
        set_stage_if_new(Stage::Mapping, lmsg::stage_reconstructing.get());
    }
#else
    (void)l;
#endif
}

// A capture shot as several inputs is several cameras, and the lens is a
// property of the input rather than of the dataset. `spirula sfm` says that with
// PREFIX=VALUE, where the prefix matches an image name by path prefix -- which
// is exactly the sub-folder each input's frames went into. A dual-lens video's
// cam0/ and cam1/ both sit under that folder, so one prefix covers both, and
// they still group as two cameras because grouping is per folder.
//
// The focal length is carried as a fraction of the image width rather than in
// pixels, because the width is not known until the frames exist (an Insta360
// X5 is fx = fy ~ 0.269 * width whatever it was shot at). A lone input has no
// prefix, so its value IS the dataset-wide one -- and an explicit focal typed
// into the panel wins over a preset-derived one.
void SfmRunner::append_camera_overrides(const SfmJob& job, const PrepResult& prep,
                                        std::vector<std::string>& argv) {
    // One group per row the panel showed: an input, or a camera folder inside
    // one. `rel` is the group's path under the image directory, which is what
    // `--camera-model PREFIX=MODEL` matches an image name against; empty means
    // the whole capture, and only a lone input with no camera folders is that.
    struct Group {
        std::string rel;
        std::string camera_model;
        float focal_factor = 0.0f;
    };
    std::vector<Group> groups;
    for (const PrepInput& in : job.prep.inputs) {
        if (in.subcameras.empty()) {
            groups.push_back({in.subdir, in.camera_model, in.focal_factor});
            continue;
        }
        for (const SubCamera& sc : in.subcameras) {
            const std::string rel =
                in.subdir.empty() ? sc.rel
                                  : (fs::path(in.subdir) / sc.rel).generic_string();
            groups.push_back({rel, sc.camera_model.empty() ? in.camera_model
                                                           : sc.camera_model,
                              sc.focal_factor > 0 ? sc.focal_factor
                                                  : in.focal_factor});
        }
    }

    for (const Group& g : groups) {
        const std::string prefix = g.rel.empty() ? "" : g.rel + "=";
        // For the whole capture the panel's own "Camera / lens" is the single
        // source of truth and has already been passed; only a named group adds
        // one.
        if (!g.rel.empty() && !g.camera_model.empty()) {
            argv.push_back("--camera-model");
            argv.push_back(prefix + g.camera_model);
        }
        if (!(g.focal_factor > 0)) continue;
        if (g.rel.empty() && job.init_focal_px > 0) continue;
        const std::string dir =
            (g.rel.empty() ? fs::path(prep.image_dir)
                           : fs::path(prep.image_dir) / g.rel).string();
        int W = 0, H = 0;
        if (!DatasetPrep::first_image_dims(dir, W, H)) {
            log(fmt(lmsg::sfm_focal_unreadable, {dir}));
            continue;
        }
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", (double)g.focal_factor * W);
        argv.push_back("--focal");
        argv.push_back(prefix + buf);
        log(fmt(lmsg::sfm_initial_focal,
                {g.rel.empty() ? lmsg::sfm_the_capture.get() : g.rel.c_str(),
                 buf, g.focal_factor, (long long)W}));
    }
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
    if (job.ba_cpu) {
        argv.push_back("--ba-real");
        argv.push_back("cpu");
        argv.push_back("--ba-real-coarse");
        argv.push_back("cpu");
    }
    append_camera_overrides(job, prep, argv);
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
            std::vector<std::string> argv = {
                // The child is this same executable, so it has the same
                // thirteen languages -- tell it which one, or its output
                // lands in the log in whatever the machine's locale is.
                app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
                "sfm", "auto", prep.image_dir,
                "-o", ws.string(),
                "--progress-dir", (ws / ".progress").string(),
            };
            argv.insert(argv.end(), now.args.begin(), now.args.end());

            std::string cmd;
            for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
            log(cmd);
            // What to advise on failure depends on which stage lost the
            // device: a CPU bundle adjustment is no answer to one lost while
            // matching.
            bool mapping = false, gpu_failure = false;
            const int rc = run_process(argv, "", [&](const std::string& l) {
                log(l, !child_line_is_notable(l));
                note_progress(l);
#ifdef SS_TOOL_SFM
                const std::string map = sfm::slog::prefix(sfm::slog::Tag::Map);
                if (l.compare(0, map.size(), map) == 0) mapping = true;
#endif
                if (mapping && child_line_is_gpu_failure(l)) gpu_failure = true;
            }, _cancel);
            if (rc == kCancelled) return fail(lmsg::err_cancelled.get());
            if (rc == kSpawnFailed)
                return fail(fmt(lmsg::err_spawn_recon, {argv[0]}));
            // `auto` spends exit code 2 on "nothing reconstructed" and 3 on
            // "reconstructed, but under half the images registered or the
            // reprojection error is high" (src/sfm/README.md). 3 is a warning
            // here, not a failure: a partial model still trains, and throwing
            // it away over a threshold would be worse than saying so.
            if (rc == 3) {
                _partial = true;
                log(lmsg::sfm_partial.get());
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

        if (reads_photos_in_place(job.prep.inputs))
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
