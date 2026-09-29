// GeometryRunner.cpp -- see GeometryRunner.h.

#include "app/gui/GeometryRunner.h"

#include "app/FrameMask.h"
#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#include "core/ModelMirror.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Dataset.h"
#include "i18n/catalog/Geometry.h"
#include "i18n/catalog/Log.h"

#ifdef SS_TOOL_GEOMETRY
#include "metric3d/model/Fetch.h"
#include "moge/model/Fetch.h"
#include "nn/Device.h"
#include "nn/io/Fetch.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
namespace dmsg = spirula::i18n::msg::dataset;
namespace lmsg = spirula::i18n::msg::log;
namespace gmsg = spirula::i18n::msg::geometry;

using spirula::i18n::format;

namespace gui {

namespace {

const char* kTri[] = {"auto", "yes", "no"};

// A whole download in bytes, both files where there are two, so the prompt
// can say what a checkpoint costs before it is started.
constexpr uint64_t kSmallBytes = 75778144ull;
constexpr uint64_t kLargeBytes = 825204259ull;
constexpr uint64_t kGiantBytes = 1401125724ull + 1355808768ull;
constexpr uint64_t kMogeSBytes = 140852051ull;
constexpr uint64_t kMogeBBytes = 419411850ull;
constexpr uint64_t kMogeLBytes = 1324265014ull;

std::string trim_right(const std::string& s) {
    size_t n = s.size();
    while (n && (s[n - 1] == ' ' || s[n - 1] == '\t')) n--;
    return s.substr(0, n);
}

}  // namespace

const std::vector<GeometryModel>& geometry_models() {
    static const std::vector<GeometryModel> kModels = {
        {"moge2-vits", &dmsg::geom_model_moge_s, &dmsg::geom_model_moge_s_blurb,
         kMogeSBytes},
        {"moge2-vitb", &dmsg::geom_model_moge_b, &dmsg::geom_model_moge_b_blurb,
         kMogeBBytes},
        {"moge2-vitl", &dmsg::geom_model_moge_l, &dmsg::geom_model_moge_l_blurb,
         kMogeLBytes},
        {"metric3d-vit-small", &dmsg::geom_model_small,
         &dmsg::geom_model_small_blurb, kSmallBytes},
        {"metric3d-vit-large", &dmsg::geom_model_large,
         &dmsg::geom_model_large_blurb, kLargeBytes},
        {"metric3d-vit-giant2", &dmsg::geom_model_giant,
         &dmsg::geom_model_giant_blurb, kGiantBytes},
    };
    return kModels;
}

std::vector<PendingDownload> geometry_model_downloads(const std::string& id) {
    std::vector<PendingDownload> out;
#ifdef SS_TOOL_GEOMETRY
    auto want = [&](const nn::FetchFile& f) {
        if (!f.file) return;
        const std::string dest = nn::cached_path(f);
        if (!file_is_cached(dest, f.bytes)) out.push_back({f.url, dest, f.bytes, spirula::model_mirror_url(f.file)});
    };
    if (const moge::ModelSource* m = moge::find_model_source(id)) {
        want(m->onnx);
    } else if (const metric3d::ModelSource* src = metric3d::find_model_source(id)) {
        want(src->onnx);
        want(src->data);
    }
#else
    (void)id;
#endif
    return out;
}

bool geometry_model_cached(const std::string& id) {
#ifdef SS_TOOL_GEOMETRY
    if (!moge::find_model_source(id) && !metric3d::find_model_source(id)) {
        std::error_code ec;
        return fs::is_regular_file(id, ec);   // a file the user pointed at
    }
    return geometry_model_downloads(id).empty();
#else
    (void)id;
    return false;
#endif
}

std::string geometry_availability() {
#ifndef SS_TOOL_GEOMETRY
    return lmsg::err_no_geometry_module.get();
#else
    if (app::exe_path().empty()) return lmsg::err_no_exe_path.get();
    return "";
#endif
}

bool run_geometry_step(const GeometryJob& job, const std::string& dataset,
                       const std::string& images, RunProgress& prog,
                       FilmReel* reel, const std::atomic<bool>& cancel,
                       std::string& error) {
    if (std::string why = geometry_availability(); !why.empty()) {
        error = why;
        return false;
    }
    if (!job.want_depth && !job.want_normal) return true;

    prog.enter(Stage::Geometry, lmsg::stage_geometry.get());
    auto log = [&](const std::string& s, bool detail) { prog.note(s, detail); };

    std::vector<std::string> argv = {
        app::exe_path(), "--lang", spirula::i18n::code(spirula::i18n::current()),
        "geometry", dataset,
        "--image-dir", images.empty() ? std::string("images") : images,
        "--model", job.model,
        "--max-size", std::to_string(job.max_size),
        "--num-tokens", std::to_string(job.num_tokens),
        "--normal-format", job.normal_jpg ? "jpg" : "png",
        "--jpeg-quality", std::to_string(job.jpeg_quality),
        "--depth-units", job.depth_mm ? "mm" : "relative",
        "--ray-depth", kTri[std::clamp(job.ray_depth, 0, 2)],
        "--split", kTri[std::clamp(job.split, 0, 2)],
    };
    if (job.want_depth) argv.push_back("--depth");
    if (!job.want_normal) argv.push_back("--no-normal");
    if (job.overwrite) argv.push_back("--overwrite");
    // Carry the frozen UUID so an inherited environment cannot redirect the child.
    std::string device = job.device_uuid;
#ifdef SS_TOOL_GEOMETRY
    if (device.empty()) device = nn::configured_device_selector();
#endif
    if (!device.empty()) {
        argv.push_back("--device");
        argv.push_back(device);
    }
    if (!job.image_gamut.empty()) {
        argv.push_back("--image-gamut");
        argv.push_back(job.image_gamut);
    }
    if (job.image_is_linear.has_value())
        argv.push_back(*job.image_is_linear ? "--image-linear" : "--no-image-linear");

    std::string cmd;
    for (const std::string& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd, true);

    // The normals are what a reconstruction usually wants and so what the run
    // is watched by; a depth-only run has only the other folder.
    const fs::path root(dataset);
    OutputWatch watch(reel, root / (job.want_normal ? "normals" : "depths"),
                      images, job.want_depth ? root / "depths" : fs::path());
    const int rc = run_process(argv, "", [&](const std::string& raw) {
        const std::string line = trim_right(raw);
        if (line.empty()) return;
        std::vector<std::string> got;
        if (spirula::i18n::scan(gmsg::log_progress, line, got) && got.size() >= 2) {
            prog.count(Stage::Geometry, std::atoll(got[0].c_str()),
                       std::atoll(got[1].c_str()));
            prog.detail(Stage::Geometry, line);
            watch.poll();
            return;
        }
        // The dataset line and the per-camera warp lines are what a user who
        // is not debugging wants: how many images, and whether a wide lens was
        // split into faces.
        log(line, !spirula::i18n::scan(gmsg::log_dataset, line, got) &&
                      !spirula::i18n::scan(gmsg::log_done, line, got));
        watch.poll();
    }, cancel);
    watch.poll(/*flush=*/true);

    if (rc == kCancelled) {
        error = lmsg::err_cancelled.get();
        return false;
    }
    if (rc == kSpawnFailed) {
        error = format(lmsg::err_spawn_geometry, {argv[0]});
        return false;
    }
    if (rc != 0) {
        error = lmsg::err_geometry_failed.get();
        return false;
    }
    prog.mark(Stage::Geometry, StageStatus::Done);
    return true;
}

}  // namespace gui
