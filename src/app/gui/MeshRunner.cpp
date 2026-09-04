// MeshRunner.cpp -- see MeshRunner.h.

#include "app/gui/MeshRunner.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Log.h"
#include "mesh/MeshLog.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace lmsg = spirula::i18n::msg::log;
using spirula::i18n::format;

namespace gui {

namespace {

// The pipeline's stages, in the order the child prints them, and how far
// through the run each one is. The child's own `[meshing] <stage>: ...` lines
// drive this -- no separate protocol to keep in sync.
// Each phase announces itself when it STARTS, so `at` is where the bar sits
// on entry and the next row's `at` is where it may creep to.
//
// Nothing here is English. The child runs with the same --lang this process
// is in, so `stage` is compared against the very message the child printed
// (meshing::mlog::stage_label), and the counts come out of the bodies with
// i18n::scan(), which matches a line against the message it came from. The
// caption under the bar is a separate string, a phrase rather than a column.
struct StageMark {
    meshing::mlog::Stage stage;        // what the child prints after the tag
    float at;                          // fraction complete when it appears
    const spirula::i18n::Msg* caption; // what the GUI shows while it lasts
};
using MStage = meshing::mlog::Stage;
const StageMark kStages[] = {
    {MStage::Loading,     0.00f,  &lmsg::mesh_stage_loading},
    {MStage::PointCloud,  0.04f,  &lmsg::mesh_stage_point_cloud},
    {MStage::Delaunay,    0.10f,  &lmsg::mesh_stage_delaunay},
    {MStage::Occupancy,   0.34f,  &lmsg::mesh_stage_occupancy},
    {MStage::CutEdges,    0.60f,  &lmsg::mesh_stage_cut_edges},
    {MStage::Bisection,   0.64f,  &lmsg::mesh_stage_bisection},
    {MStage::Marching,    0.74f,  &lmsg::mesh_stage_marching_tets},
    {MStage::Merge,       0.78f,  &lmsg::mesh_stage_merge},
    {MStage::Cleanup,     0.82f,  &lmsg::mesh_stage_cleanup},
    {MStage::Cull,        0.85f,  &lmsg::mesh_stage_cull_unseen},
    {MStage::Quality,     0.88f,  &lmsg::mesh_stage_quality},
    {MStage::Orient,      0.90f,  &lmsg::mesh_stage_orient},
    {MStage::Color,       0.91f,  &lmsg::mesh_stage_color},
    {MStage::Uv,          0.94f,  &lmsg::mesh_stage_uv},
    {MStage::Bake,        0.96f,  &lmsg::mesh_stage_bake},
    {MStage::Texture,     0.98f,  &lmsg::mesh_stage_texture},
    {MStage::Stats,       0.99f,  &lmsg::mesh_stage_stats},
    {MStage::Wrote,       0.995f, &lmsg::mesh_stage_wrote},
};
constexpr int kNumStages = (int)(sizeof(kStages) / sizeof(kStages[0]));

// How long a phase is assumed to take when it reports nothing (seconds). The
// creep is 1 - exp(-t/tau), so it approaches the next mark without ever
// reaching or passing it -- a bar that moves but never lies about finishing.
constexpr double kCreepTau = 25.0;

// Whichever of the requested formats carries the most: a textured GLB is
// self-contained, a glTF needs its sidecars, OBJ has no vertex colors, PLY has
// no texture. Preview quality follows that order.
const char* kPreviewOrder[] = {"glb", "gltf", "obj", "ply"};

double now_seconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}  // namespace

std::string MeshJob::preview_path() const {
    if (output.empty()) return {};
    for (const char* want : kPreviewOrder)
        for (int i = 0; i < kNumMeshFormats; ++i)
            if (formats[i] && std::strcmp(kMeshFormats[i], want) == 0)
                return output + "." + want;
    return {};
}

MeshRunner::~MeshRunner() {
    _cancel = true;
    if (_worker.joinable()) _worker.join();
}

void MeshRunner::start(const MeshJob& job) {
    if (_worker.joinable()) _worker.join();
    {
        std::lock_guard<std::mutex> lk(_mu);
        _error.clear();
        _stage.clear();
        _output.clear();
        _log.clear();
    }
    _verts = 0;
    _faces = 0;
    _stage_lo = -1.0f;
    _stage_hi = -1.0f;
    _stage_frac = -1.0f;
    _stage_at = now_seconds();
    _cancel = false;
    _run_id.fetch_add(1);
    _state = State::Running;
    _worker = std::thread([this, job] { run(job); });
}

void MeshRunner::cancel() { _cancel = true; }

std::string MeshRunner::stage() {
    std::lock_guard<std::mutex> lk(_mu);
    return _stage;
}
std::string MeshRunner::error() {
    std::lock_guard<std::mutex> lk(_mu);
    return _error;
}
std::string MeshRunner::output_path() {
    std::lock_guard<std::mutex> lk(_mu);
    return _output;
}

void MeshRunner::log(const std::string& line) {
    std::lock_guard<std::mutex> lk(_mu);
    _log.push_back(line);
    if (_log.size() > 4000) _log.erase(_log.begin(), _log.begin() + 1000);
}

std::vector<std::string> MeshRunner::drain_log() {
    std::lock_guard<std::mutex> lk(_mu);
    std::vector<std::string> out;
    out.swap(_log);
    return out;
}

float MeshRunner::progress() const {
    const float lo = _stage_lo.load();
    if (lo < 0.0f) return -1.0f;
#if 1
    return std::pow(lo, 2.0f);
#else
    const float hi = _stage_hi.load();
    const float sub = _stage_frac.load();
    if (sub >= 0.0f) return lo + (hi - lo) * std::min(std::max(sub, 0.0f), 1.0f);
    const double dt = std::max(0.0, now_seconds() - _stage_at.load());
    return lo + (hi - lo) * (float)(1.0 - std::exp(-dt / kCreepTau));
#endif
}

void MeshRunner::note_line(const std::string& line) {
    const std::string tag = meshing::mlog::tag_prefix();
    const size_t at = line.find(tag);
    if (at == std::string::npos) return;
    const std::string rest = line.substr(at + tag.size());

    // "<stage>: <body>" -- the stage moves the bar, the body may say how far
    // through the stage the child is.
    for (int i = 0; i < kNumStages; ++i) {
        const std::string label = meshing::mlog::stage_label(kStages[i].stage);
        if (rest.rfind(label, 0) != 0) continue;
        _stage_lo = kStages[i].at;
        _stage_hi = i + 1 < kNumStages ? kStages[i + 1].at : 1.0f;
        _stage_frac = -1.0f;
        _stage_at = now_seconds();
        std::lock_guard<std::mutex> lk(_mu);
        // Resolved here rather than at draw time so the caption is settled on
        // the thread that noticed the stage change; the language cannot move
        // between the two, and the UI thread does no lookup.
        _stage = kStages[i].caption->get();
        break;
    }

    // The "<stage>: " column comes off first -- including for the stages the
    // table above does not track, which still report their own progress.
    std::string tail = rest;
    const size_t colon = rest.find(": ");
    if (colon != std::string::npos) tail = rest.substr(colon + 2);

    // "cameras rendered: 12/120" -- a phase reporting its own fraction.
    // Matched against the message itself, so no digit anywhere else in the
    // line (a path, a timing) can be mistaken for progress. Monotone within a
    // bracket: the bisection loop sweeps the camera set once per iteration,
    // and a bar that runs backwards reads as a bug.
    std::vector<std::string> got;
    if (_stage_lo.load() >= 0.0f &&
        spirula::i18n::scan(spirula::i18n::msg::mesh::cameras_rendered, tail,
                            got) &&
        got.size() == 2) {
        const double done = std::atof(got[0].c_str());
        const double total = std::atof(got[1].c_str());
        if (total > 0.0) {
            const float f = (float)(done / total);
            if (f > _stage_frac.load()) _stage_frac = f;
        }
    }

    // The final count, from the one line that knows both.
    if (spirula::i18n::scan(spirula::i18n::msg::mesh::done_summary, tail, got) &&
        got.size() == 3) {
        _verts = std::atoll(got[0].c_str());
        _faces = std::atoll(got[1].c_str());
    }
}

void MeshRunner::run(MeshJob job) {
    auto fail = [&](const std::string& why) {
        std::lock_guard<std::mutex> lk(_mu);
        _error = why;
        _state = _cancel.load() ? State::Cancelled : State::Failed;
    };

    std::string formats;
    for (int i = 0; i < kNumMeshFormats; ++i)
        if (job.formats[i])
            formats += (formats.empty() ? "" : ",") + std::string(kMeshFormats[i]);
    if (formats.empty()) formats = "ply";

    if (job.output.empty()) {
        // Beside the checkpoint, as the CLI's own default does -- and for a
        // FILE source, `<name>_mesh` rather than `<name>`: the latter writes
        // `splat.ply` when meshing `splat.ply`. The child refuses that
        // outright (check_mesh_outputs_safe); this is the reason it never
        // has to.
        std::error_code ec;
        fs::path base(job.checkpoint);
        if (fs::is_regular_file(base, ec))
            base.replace_filename(base.stem().string() + "_mesh");
        else
            base = base / "mesh";
        job.output = base.string();
    }

    std::vector<std::string> argv = {
        // The child is this same executable, so it has the same thirteen
        // languages -- tell it which one, or its output lands in the log in
        // whatever the machine's locale is.
        app::exe_path(), "--lang",
        spirula::i18n::code(spirula::i18n::current()),
        "mesh", job.checkpoint,
        "--format", formats,
        "--color", kMeshColorModes[job.color],
        "--output", job.output,
    };
    if (!job.use_data) {
        argv.push_back("--no-data");
    } else if (!job.data_dir.empty()) {
        argv.push_back("--data");
        argv.push_back(job.data_dir);
    }
    if (job.max_cameras > 0) {
        argv.push_back("--max-cameras");
        argv.push_back(std::to_string(job.max_cameras));
    }
    if (job.texture_size > 0 && job.color == 2) {
        argv.push_back("--texture-size");
        argv.push_back(std::to_string(job.texture_size));
    }
    if (job.iso > 0.0f) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", job.iso);
        argv.push_back("--iso");
        argv.push_back(buf);
    }
    if (job.bisection_iters != 3) {
        argv.push_back("--bisection-iters");
        argv.push_back(std::to_string(job.bisection_iters));
    }
    if (job.merge_factor != 1.0f) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%g", job.merge_factor);
        argv.push_back("--merge-factor");
        argv.push_back(buf);
    }
    if (job.quality_iters != 3) {
        argv.push_back("--quality-iters");
        argv.push_back(std::to_string(job.quality_iters));
    }
    if (job.floater_min_faces != 10) {
        argv.push_back("--floater-min-faces");
        argv.push_back(std::to_string(job.floater_min_faces));
    }
    if (job.carve_k != 1) {
        argv.push_back("--carve-k");
        argv.push_back(std::to_string(job.carve_k));
    }
    if (!job.cull_unseen) argv.push_back("--no-cull-unseen");
    for (const std::string& a : split_args(job.extra_args)) argv.push_back(a);

    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);

    const int rc = run_process(argv, "", [this](const std::string& l) {
        log(l);
        note_line(l);
    }, _cancel);

    if (rc == kCancelled) {
        _state = State::Cancelled;
        std::lock_guard<std::mutex> lk(_mu);
        _error = lmsg::err_cancelled.get();
        return;
    }
    if (rc == kSpawnFailed)
        return fail(format(lmsg::err_spawn_recon, {argv[0]}));
    if (rc != 0) return fail(lmsg::err_mesh_failed.get());

    {
        // Which file the preview opens. The child writes every requested
        // format, so this is decided from the job rather than read back out of
        // the log: the preview wants the one that carries the most (a textured
        // GLB over a bare PLY), and falls back down that order to whatever is
        // actually on disk.
        std::error_code ec;
        std::string best = job.preview_path();
        if (!fs::is_regular_file(best, ec)) {
            for (const char* want : kPreviewOrder) {
                const std::string cand = job.output + "." + want;
                if (fs::is_regular_file(cand, ec)) { best = cand; break; }
            }
        }
        std::lock_guard<std::mutex> lk(_mu);
        _output = best;
    }
    _stage_lo = 1.0f;
    _stage_hi = 1.0f;
    _stage_frac = -1.0f;
    _state = State::Done;
}

}  // namespace gui
