#pragma once

// What a front end hands a run, as a file rather than as a command line.
//
// The camera-group settings used to travel as `--camera-model PREFIX=MODEL`
// and `--focal DIR=PX`, re-resolved against the filesystem in the child; a
// manifest states them once, in a file a person can read and edit. It is also
// where per-image priors go when there is something producing them -- GPS,
// IMU gravity, range data -- so a new sensor is a new key, not a new flag.
//
// Read as YAML or JSON (data/Yaml.h takes both); written as either.

#include "sfm/SfmConfig.h"
#include "sfm/core/Rig.h"

#include <set>
#include <string>
#include <vector>

namespace sfm {

// One camera group: the images it covers, and what is known about the lens.
struct ManifestCamera {
    // Path prefix under the image directory: "cam0" covers cam0/00017 and
    // cam0/sub/x, and nothing else. Empty is the dataset-wide default.
    std::string prefix;
    std::string model;               // empty = the run's --camera-model
    double focal = 0;                // pixels; 0 = no prior
    std::vector<double> distortion;  // the model's BA order; empty = zeros
};

// One source video and the telemetry it carries. `telemetry` is the video
// itself or a file the reader knows; frames under `prefix` are timed by the
// source frame index in their stem over `fps` (0 = the file's own rate).
struct ManifestCapture {
    std::string prefix;
    std::string telemetry;
    double fps = 0;
    double time_offset = 0;   // seconds added to every frame time
};

struct Manifest {
    // Exactly as the file spells them; `manifest_apply` resolves a relative
    // one against `base_dir`, so a manifest and the capture beside it move
    // together and a round trip through the writer does not rewrite the path.
    std::string image_dir;
    std::string mask_dir;
    // The manifest's own directory, set by manifest_read. Not written out.
    std::string base_dir;
    bool mask_flipped = false;
    bool has_mask_flipped = false;

    std::string camera_mode;         // "single" | "folder" | "image"; empty = default
    std::vector<ManifestCamera> cameras;
    std::vector<ManifestCapture> captures;
    // Rigs: members are path prefixes like the cameras', frames are the
    // images sharing a path under them; a member may carry its cam_from_rig
    // (quaternion w,x,y,z and a translation) when it is known.
    std::vector<RigDef> rigs;

    std::string image_gamut;         // empty = leave the run's own
    int image_linear = -1;           // -1 unset, 0 no, 1 yes
};

// Throws std::runtime_error naming the file and line on a malformed one.
Manifest manifest_read(const std::string& path);

// YAML unless `json`. Round trips through manifest_read.
std::string manifest_write(const Manifest& m, bool json = false);

// Fold into a config. `seen` names the flags the command line already set;
// those win, exactly as an explicit flag beats a preset. Returns "" or why
// the manifest cannot be applied.
std::string manifest_apply(const Manifest& m, SfmConfig& cfg,
                           const std::set<std::string>& seen,
                           std::string& image_dir);

}  // namespace sfm
