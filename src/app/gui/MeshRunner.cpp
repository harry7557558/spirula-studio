// MeshRunner.cpp -- see MeshRunner.h.

#include "app/gui/MeshRunner.h"

#include "app/AppPaths.h"
#include "app/gui/Subprocess.h"
#include "i18n/Locale.h"
#include "i18n/catalog/Log.h"
#include "mesh/MeshLog.h"

#include <algorithm>
#include <cctype>
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

// The caption under the bar for each stage the child names. Not English: the
// child runs with this process's --lang, so its stage column is compared with
// the message it printed (meshing::mlog::stage_label).
struct StageCaption {
    meshing::mlog::Stage stage;
    const spirula::i18n::Msg* caption;
};
using MStage = meshing::mlog::Stage;
const StageCaption kStages[] = {
    {MStage::Loading,    &lmsg::mesh_stage_loading},
    {MStage::PointCloud, &lmsg::mesh_stage_point_cloud},
    {MStage::Delaunay,   &lmsg::mesh_stage_delaunay},
    {MStage::Occupancy,  &lmsg::mesh_stage_occupancy},
    {MStage::CutEdges,   &lmsg::mesh_stage_cut_edges},
    {MStage::Bisection,  &lmsg::mesh_stage_bisection},
    {MStage::Marching,   &lmsg::mesh_stage_marching_tets},
    {MStage::Merge,      &lmsg::mesh_stage_merge},
    {MStage::Cleanup,    &lmsg::mesh_stage_cleanup},
    {MStage::Cull,       &lmsg::mesh_stage_cull_unseen},
    {MStage::Quality,    &lmsg::mesh_stage_quality},
    {MStage::Orient,     &lmsg::mesh_stage_orient},
    {MStage::Color,      &lmsg::mesh_stage_color},
    {MStage::Uv,         &lmsg::mesh_stage_uv},
    {MStage::Bake,       &lmsg::mesh_stage_bake},
    {MStage::Texture,    &lmsg::mesh_stage_texture},
    {MStage::Stats,      &lmsg::mesh_stage_stats},
    {MStage::Wrote,      &lmsg::mesh_stage_wrote},
};

}  // namespace

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
        _outputs.clear();
        _log.clear();
    }
    _verts = 0;
    _faces = 0;
    _cancel = false;
    const uint64_t id = _run_id.fetch_add(1) + 1;
    {
        std::lock_guard<std::mutex> lk(_mu);
        _progress_dir = (fs::path(app::cache_dir()) / "mesh-progress" /
                         std::to_string((uint64_t)(progress_now() * 1e3) * 1000 + id % 1000))
                            .string();
    }
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

std::vector<std::string> MeshRunner::output_paths() {
    std::lock_guard<std::mutex> lk(_mu);
    return _outputs;
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

void MeshRunner::poll() {
    const uint64_t id = _run_id.load();
    const double now = progress_now();
    if (id != _polled_run) {
        _polled_run = id;
        _clock.start(now);
        _memory.reset();
    }
    const State st = _state.load();
    if (st == State::Done) _clock.finish(now);
    if (st != State::Running) return;
    std::string dir;
    {
        std::lock_guard<std::mutex> lk(_mu);
        dir = _progress_dir;
    }
    _memory.poll_report(dir);
    if (_memory.reported_work() >= 0.0) _clock.work(now, _memory.reported_work());
}

void MeshRunner::note_line(const std::string& line) {
    const std::string tag = meshing::mlog::tag_prefix();
    const size_t at = line.find(tag);
    if (at == std::string::npos) return;
    const std::string rest = line.substr(at + tag.size());

    for (const StageCaption& s : kStages) {
        const std::string label = meshing::mlog::stage_label(s.stage);
        if (rest.rfind(label, 0) != 0) continue;
        std::lock_guard<std::mutex> lk(_mu);
        // Resolved here rather than at draw time so the caption is settled on
        // the thread that noticed the stage change; the language cannot move
        // between the two, and the UI thread does no lookup.
        _stage = s.caption->get();
        break;
    }

    std::string tail = rest;
    const size_t colon = rest.find(": ");
    if (colon != std::string::npos) tail = rest.substr(colon + 2);

    // The final count, from the one line that knows both.
    std::vector<std::string> got;
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
    std::string colors;
    for (int i = 0; i < kNumMeshColorModes; ++i)
        if (job.colors[i])
            colors += (colors.empty() ? "" : ",") + std::string(kMeshColorModes[i]);
    if (colors.empty()) colors = "vertex";

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
        "--color", colors,
        "--output", job.output,
    };
    std::string progress_dir;
    {
        std::lock_guard<std::mutex> lk(_mu);
        progress_dir = _progress_dir;
    }
    argv.push_back("--progress-dir");
    argv.push_back(progress_dir);
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
    if (job.texture_size > 0 && job.wants_color(2)) {
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
#ifdef SS_BACKEND_VULKAN
    if (!job.device_uuid.empty()) {
        argv.push_back("--device");
        argv.push_back(job.device_uuid);
    }
#else
    if (job.cuda_device >= 0) {
        argv.push_back("--device");
        argv.push_back(std::to_string(job.cuda_device));
    }
#endif

    std::string cmd;
    for (const auto& a : argv) cmd += (cmd.empty() ? "$ " : " ") + a;
    log(cmd);

    const int rc = run_process(argv, "", [this](const std::string& l) {
        log(l);
        note_line(l);
    }, _cancel);
    {
        std::error_code ec;
        fs::remove_all(progress_dir, ec);
    }

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
        // Which file the preview opens: decided from the job rather than read
        // back out of the log, and the first one actually on disk wins -- so a
        // combination the child skipped never becomes the preview.
        std::error_code ec;
        const std::vector<std::string> all = mesh_job_outputs(job);
        std::string best = all.empty() ? std::string() : all.front();
        for (const std::string& cand : all)
            if (fs::is_regular_file(cand, ec)) { best = cand; break; }
        std::lock_guard<std::mutex> lk(_mu);
        _output = best;
        _outputs.clear();
        for (const std::string& cand : all)
            if (fs::is_regular_file(cand, ec)) _outputs.push_back(cand);
    }
    _state = State::Done;
}

}  // namespace gui
