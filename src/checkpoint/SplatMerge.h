#pragma once

// Models returned to dataset coordinates and merged by spatial ownership.
// File output streams bounded chunks.

#include "checkpoint/SplatPly.h"
#include "data/ScenePartition.h"

#include <functional>
#include <string>
#include <vector>

namespace spirula {

struct MergePartStats {
    std::string ply;      // the file read; "" when the part had none
    int64_t read = 0;
    int64_t kept = 0;
    int sh_degree = 0;
    std::string region_camera_hash;
};

struct MergeStats {
    std::vector<MergePartStats> parts;
    int64_t total = 0;
    int sh_degree = 0;
    int sb_lobes = 0;
    std::string primitive;
};

// `part_paths[k]` is part k's run folder, checkpoint folder or splat.ply; an
// empty entry skips the part (its region stays unfilled). Throws when a named
// path holds no splats.
SplatCloud merge_partition_splats(const ScenePartition& p, const std::vector<std::string>& part_paths,
                                  MergeStats& stats,
                                  const std::function<void(const std::string&)>& log = {});

// Two passes over bounded chunks; the output is published after its final close.
void merge_partition_splats_file(const ScenePartition& p, const std::vector<std::string>& part_paths,
                                 const std::string& output, MergeStats& stats,
                                 const std::function<void(const std::string&)>& log = {});

// Every run under `outputs_dir` whose config.json names `partition_json` (by
// canonical path): part index -> newest run folder. A part with no run gets "".
std::vector<std::string> find_partition_runs(const std::string& outputs_dir,
                                             const std::string& partition_json, int num_parts);

}  // namespace spirula
