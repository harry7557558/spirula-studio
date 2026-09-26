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
    fs::path tarpath = ckpt_dir / "state.tar";
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
    if (fs::is_regular_file(path / "state.tar"))
        return {path.parent_path(), path};

    // Checkpoint dirs are step-%09d.ckpt, so lexicographic order is numeric.
    std::vector<fs::path> ckpts;
    if (fs::is_directory(path))
        for (const auto& e : fs::directory_iterator(path)) {
            std::string b = e.path().filename().string();
            if (b.rfind("step-", 0) == 0 && b.find(".ckpt") != std::string::npos)
                ckpts.push_back(e.path());
        }
    if (ckpts.empty())
        throw std::runtime_error("no state.tar or step-*.ckpt found under " +
                                 path.string());
    std::sort(ckpts.begin(), ckpts.end());
    return {path, ckpts.back()};
}


JsonValue read_state_json(const fs::path& ckpt_dir) {
    TarView v = open_state_tar(ckpt_dir);
    std::string sj = read_member(v, "state.json");
    if (sj.empty())
        throw std::runtime_error("state.json missing in " +
                                 (ckpt_dir / "state.tar").string());
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
            "inference/appearance params and splat.ply -- not the world "
            "parameters and optimizer state needed to continue training. "
            "Re-run the source training with --save-full-checkpoint 1 so its "
            "checkpoints are resumable; this one is usable for inference and "
            "meshing (splat.ply) only.");
}


TrainConfig config_from_json(const fs::path& config_json) {
    TrainConfig c;
    const JsonValue root = json_parse_file(config_json.string());
    train_config_from_json(root, c);
    // What `--image-color-log auto` settled on, so a resume never re-detects.
    if (const JsonValue* v = root.find("image_color_log_resolved"))
        json_field::assign(c.image_color_log_resolved, *v);
    return c;
}


TrainConfig build_resume_config(const TrainConfig& cli,
                                 const std::string& preset,
                                 const std::set<std::string>& explicit_flags) {
    ResolvedCheckpoint r = resolve_checkpoint(cli.resume);
    check_resumable(r.ckpt_dir);

    fs::path run_dir = fs::absolute(r.run_dir);
    fs::path cfg_path = run_dir / "config.json";
    if (!fs::is_regular_file(cfg_path))
        throw std::runtime_error("no config.json in " + run_dir.string() +
                                 " (needed to reconstruct the run's config)");

    TrainConfig base = config_from_json(cfg_path);
    base.resume = cli.resume;

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
