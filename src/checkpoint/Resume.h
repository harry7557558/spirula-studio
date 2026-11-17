#pragma once

// Flat named PLY/TAR checkpoints and legacy step directories share this resolver.

#include "config/TrainConfig.h"
#include "data/Json.h"

#include <filesystem>
#include <set>
#include <string>

namespace ckpt {

struct ResolvedCheckpoint {
    std::filesystem::path run_dir;    // holds config.json
    std::filesystem::path ckpt_dir;
    std::filesystem::path tar_path;
};

// Accepts a run, legacy checkpoint directory, or an explicit TAR/paired PLY.
ResolvedCheckpoint resolve_checkpoint(const std::filesystem::path& path);

// The state.json member of the resolved archive.
JsonValue read_state_json(const std::filesystem::path& ckpt_dir);

// Throws with an actionable message if the checkpoint holds only the Always
// (inference/appearance) buffers -- i.e. was saved without
// save_full_checkpoint -- and so cannot restore training.
void check_resumable(const std::filesystem::path& ckpt_dir);

// A run's config.json -> TrainConfig. Keys absent from the file keep the
// field's default, so a config.json written by an older build still loads.
TrainConfig config_from_json(const std::filesystem::path& config_json);

// Explicit flags override the preset, which overrides the saved configuration.
TrainConfig build_resume_config(const TrainConfig& cli,
                                 const std::string& preset,
                                 const std::set<std::string>& explicit_flags);

}  // namespace ckpt
