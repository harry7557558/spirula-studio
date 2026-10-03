// A COLMAP model whose cameras and poses a run must not change: read once with
// each pose's bytes kept, triangulated against, written back from those bytes,
// then re-read and compared. docs/notes/fixed-poses.md has the rules.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Model.h"
#include "sfm/core/Pose.h"

namespace sfm {

struct FixedImage {
    uint32_t id = 0, camera_id = 0;
    std::string name;
    // qw qx qy qz tx ty tz as the file stores them. Never through arithmetic:
    // a quaternion -> matrix -> quaternion round trip moves the last bits.
    uint64_t pose_bits[7] = {};
    Pose pose() const;
};

struct FixedPoses {
    std::string cameras_bin;               // the whole file, written back as is
    std::map<uint32_t, Camera> cameras;    // parsed, for triangulating only
    std::vector<FixedImage> images;        // in file order
};

// cameras.bin and images.bin under `dir`; points3D.bin is not read. Throws
// std::runtime_error saying which file and what is wrong with it.
FixedPoses readFixedPoses(const std::string& dir);

// A model's image name as the feature database spells it: '/' separators, no
// extension.
std::string fixedImageStem(const std::string& name);

// For each image of `fp`, its index in `db` by stem, or -1.
std::vector<int64_t> matchFixedImages(const FixedPoses& fp, const MatchesDatabase& db);

// What the mapper adopts: database image ids, the file's camera ids, each
// camera's pixel_scale from the features of the images that use it.
struct FixedSetup {
    Reconstruction model;
    std::vector<uint32_t> camera_ids;      // per database image
};
FixedSetup fixedSetup(const FixedPoses& fp, const std::vector<int64_t>& db_index,
                      const std::vector<FeatureSet>& feats);

// `rec` in database ids, written to `dir` with the file's image ids, names and
// pose bytes and its cameras.bin. Removes a stale gauge.txt / rigs.txt there.
void writeFixedModel(const std::string& dir, const FixedPoses& fp,
                     const std::vector<int64_t>& db_index, const Reconstruction& rec);

// Re-reads what writeFixedModel wrote. "" when cameras.bin and every image's
// id, camera, name and pose bytes match `fp`; otherwise what differs.
std::string checkFixedModel(const std::string& dir, const FixedPoses& fp);

}  // namespace sfm
