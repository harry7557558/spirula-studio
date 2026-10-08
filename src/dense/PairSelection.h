#pragma once

#include "sfm/geometry/LinAlg.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace spirula::dense {

using ImagePair = std::pair<uint32_t, uint32_t>;
enum class PairMode { Automatic, Sequential, Exhaustive, Explicit };

// One matchable view. Faces split from one image share source_image and are never paired.
struct PairImage {
    int64_t source_image = -1;
    int face = 0;
    std::string name;
    sfm::Vec3 center, forward{0, 0, 1};
    sfm::Vec3 up{0, 1, 0};
    double half_fov_radians = 0.8;
    std::vector<uint64_t> visible_points;
    // Paired only through shared sparse points, never by pose alone.
    bool shared_points_only = false;
};

struct PairOptions {
    PairMode mode = PairMode::Automatic;
    int neighbors = 8, sequence_window = 8;
    // Automatic mode: in capture order, a view is a reference only while a tenth of its sparse
    // point cells are seen by fewer than this many earlier references; 0 makes every view one.
    int reference_coverage = 0;
    uint64_t max_pairs = 0;
    std::vector<ImagePair> explicit_pairs;
    bool directed = false;
    std::vector<uint32_t> references;
    void validate() const;
};

struct PairStatistics {
    uint64_t emitted = 0, rejected_baseline = 0, references = 0;
};

bool shares_points(const std::vector<uint64_t>& a, const std::vector<uint64_t>& b);
std::vector<uint32_t> select_references(const std::vector<PairImage>& images, double fraction, uint64_t seed,
                                      const std::function<void()>& check_cancel = {});

// Exhaustive emits incrementally; automatic stores at most N*neighbors candidates and, given
// the sparse point positions, prefers neighbours that see shared points from a wider angle.
PairStatistics select_pairs(const std::vector<PairImage>& images, const PairOptions& options,
                               const std::function<void(ImagePair)>& emit,
                               const std::function<void()>& check_cancel = {},
                               const std::vector<sfm::Vec3>& points = {});

}  // namespace spirula::dense
