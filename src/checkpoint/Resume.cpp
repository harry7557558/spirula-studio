// Resume.cpp -- see Resume.h.

#include "checkpoint/Resume.h"

#include "config/TrainConfigJson.h"
#include "core/CheckpointIO.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace ckpt {

namespace {

// Read state.tar once and hand back both its member index and the stream.
// Callers want either state.json or the member name list, usually both.
struct TarView {
    std::ifstream in;
    std::vector<TarMember> members;
};

TarView open_state_tar(const fs::path& ckpt_dir) {
    fs::path tarpath = resolve_checkpoint(ckpt_dir).tar_path;
    TarView v;
    v.in.open(tarpath.string(), std::ios::binary);
    if (!v.in)
        throw std::runtime_error("cannot open " + tarpath.string() +
                                 " (not a checkpoint directory)");
    v.members = tar_index(v.in);
    return v;
}

std::string read_member(TarView& v, const std::string& name) {
    for (const auto& m : v.members)
        if (m.name == name) return tar_read_member(v.in, m);
    return {};
}

}  // namespace


ResolvedCheckpoint resolve_checkpoint(const fs::path& path) {
    if (fs::is_regular_file(path) && (path.extension() == ".tar" || path.extension() == ".ply")) {
        fs::path tar = path;
        tar.replace_extension(".tar");
        if (!fs::is_regular_file(tar) && path.extension() == ".ply")
            tar = path.parent_path() / "state.tar";
        if (!fs::is_regular_file(tar)) throw std::runtime_error("no checkpoint archive for " + path.string());
        const auto dir = tar.parent_path();
        return {dir.extension() == ".ckpt" ? dir.parent_path() : dir, dir, tar};
    }
    if (fs::is_regular_file(path / "state.tar"))
        return {path.extension() == ".ckpt" || !fs::exists(path / "config.json")
                    ? path.parent_path() : path, path, path / "state.tar"};

    std::vector<fs::path> candidates;
    if (fs::is_directory(path)) {
        for (const auto& e : fs::directory_iterator(path)) {
            const auto name = e.path().filename().string();
            if (e.is_regular_file() && e.path().extension() == ".tar")
                candidates.push_back(e.path());
            if (e.is_directory() && name.rfind("step-", 0) == 0 && e.path().extension() == ".ckpt" &&
                fs::is_regular_file(e.path() / "state.tar"))
                candidates.push_back(e.path() / "state.tar");
        }
    }
    fs::path latest;
    int64_t latest_step = -1;
    for (const auto& tar : candidates) {
        JsonValue state;
        try { state = read_state_json(tar); }
        catch (const std::runtime_error&) { continue; }
        const auto* step = state.find("step");
        const int64_t n = step ? step->as_int() : 0;
        if (n > latest_step || (n == latest_step && tar > latest)) {
            latest = tar;
            latest_step = n;
        }
    }
    if (latest.empty()) throw std::runtime_error("no checkpoint archive found under " + path.string());
    return {path, latest.parent_path(), latest};
}


JsonValue read_state_json(const fs::path& ckpt_dir) {
    TarView v = open_state_tar(ckpt_dir);
    std::string sj = read_member(v, "state.json");
    if (sj.empty())
        throw std::runtime_error("state.json missing in " +
                                 ckpt_dir.string());
    return json_parse(sj);
}


void check_resumable(const fs::path& ckpt_dir) {
    TarView v = open_state_tar(ckpt_dir);
    std::string sj = read_member(v, "state.json");
    JsonValue state = sj.empty() ? JsonValue() : json_parse(sj);

    bool has_world = false;
    for (const auto& m : v.members)
        if (m.name == "world.means.npy" || m.name == "world.opacities.npy")
            has_world = true;

    const JsonValue* full = state.find("full_resume");
    if ((full && full->as_int(1) == 0) || !has_world)
        throw std::runtime_error(
            "checkpoint '" + ckpt_dir.string() + "' is NOT resumable: it was "
            "saved without save_full_checkpoint, so it holds only the "
            "inference/appearance params -- not the world "
            "parameters and optimizer state needed to continue training. "
            "Re-run the source training with --save-full-checkpoint 1 so its "
            "checkpoints are resumable; this one is usable for inference and "
            "meshing with its PLY only.");
}


TrainConfig config_from_json(const fs::path& config_json) {
    const JsonValue root = json_parse_file(config_json.string());
    TrainConfig c;
    // A run from before --scene-center trained uncentred; `auto` could move it.
    if (!root.find("scene_center")) c.scene_center = "none";
    train_config_from_json(root, c);
    return c;
}


TrainConfig build_resume_config(const TrainConfig& cli,
                                 const std::string& preset,
                                 const std::set<std::string>& explicit_flags) {
    ResolvedCheckpoint r = resolve_checkpoint(cli.resume);
    check_resumable(r.tar_path);

    fs::path run_dir = fs::absolute(r.run_dir);
    fs::path cfg_path = run_dir / "config.json";
    if (!fs::is_regular_file(cfg_path))
        throw std::runtime_error("no config.json in " + run_dir.string() +
                                 " (needed to reconstruct the run's config)");

    TrainConfig base = config_from_json(cfg_path);
    base.resume = cli.resume;
    // This invocation's alone: a run folder from elsewhere must not run one.
    base.finish_command = cli.finish_command;

    // Continue writing into the checkpoint's own run folder, so new
    // checkpoints, eval images and logs land beside the old ones. An explicit
    // --output-dir-* below overrides this.
    base.output_dir_prefix = run_dir.parent_path().string();
    base.output_dir_name   = run_dir.filename().string();

    // A preset named on the resume command line re-imposes its deviations on
    // top of the checkpoint (e.g. resuming a 3dgs run as `synthetic` turns
    // bilagrid/PPISP off). Applying nothing for a same-preset resume is
    // automatic: the preset assigns exactly the fields it overrides.
    if (!preset.empty() && !train_apply_preset(base, preset))
        throw std::runtime_error("unknown preset: " + preset);

    // Explicit flags win over both.
#define SS_APPLY_EXPLICIT(type, member, default_, section, tier, choices)     \
    if (explicit_flags.count(#member)) base.member = cli.member;
    SS_CONFIG_FIELDS(SS_APPLY_EXPLICIT)
#undef SS_APPLY_EXPLICIT

    return base;
}

}  // namespace ckpt
