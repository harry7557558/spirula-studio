#include "dense/DenseConfig.h"
#include "core/HostMemory.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace spirula::dense {

// The matching stage plans about 1.4x the budget (planned_host_bytes), held to under half of
// free RAM and under a third of free commit, which RoMa's device memory also draws on Windows.
uint64_t DenseConfig::resolved_image_cache_bytes() const {
    if (image_cache_bytes) return image_cache_bytes;
    uint64_t budget = 0;
    for (const uint64_t limit : {(uint64_t)spirula::totalRamBytes() / 8, (uint64_t)spirula::availableRamBytes() / 3,
                                 (uint64_t)spirula::availableCommitBytes() / 5})
        if (limit) budget = budget ? std::min(budget, limit) : limit;
    if (!budget) throw std::runtime_error("cannot determine dense host memory budget; set image_cache_bytes explicitly");
    return budget;
}

namespace {

// `neighbors` and `coverage` see each surface of Mip-NeRF 360 about as often as the pairing
// that `was_neighbors` and `was_coverage` set before pairing followed the surface (docs/dense.md).
struct Preset { const char* name; int low, high; bool bidirectional; int neighbors, stride, coverage, was_neighbors, was_coverage; };
constexpr Preset kPresets[] = {
    {"fast", 512, 0, false, 2, 2, 12, 4, 2},
    {"balanced", 640, 0, false, 3, 2, 12, 6, 3},
    {"high", 800, 1280, true, 4, 1, 16, 8, 0},
};

const Preset* find_preset(const std::string& name) {
    for (const auto& preset : kPresets) if (name == preset.name) return &preset;
    return nullptr;
}

}  // namespace

bool DenseConfig::migrate_pairing() {
    for (const auto& p : kPresets)
        if (match.low_width == p.low && match.low_height == p.low && match.high_width == p.high && match.high_height == p.high &&
            match.bidirectional == p.bidirectional && stride == p.stride &&
            pairs.neighbors == p.was_neighbors && pairs.reference_coverage == p.was_coverage) {
            pairs.neighbors = p.neighbors; pairs.reference_coverage = p.coverage;
            return true;
        }
    return false;
}

void DenseConfig::apply_preset(const std::string& name) {
    if (const auto* p = find_preset(name)) {
        match.low_width = match.low_height = p->low;
        match.high_width = match.high_height = p->high;
        match.bidirectional = p->bidirectional; match.overlap_saturation = -1;
        pairs.neighbors = p->neighbors; pairs.reference_coverage = p->coverage; stride = p->stride;
    } else if (name != "custom") {
        const auto dimensions = roma::MatchOptions::preset(name);
        match.low_width = dimensions.low_width; match.low_height = dimensions.low_height;
        match.high_width = dimensions.high_width; match.high_height = dimensions.high_height;
        match.bidirectional = dimensions.bidirectional;
        match.overlap_saturation = dimensions.overlap_saturation;
    }
    preset = name;
}

bool DenseConfig::matches_preset(const std::string& name) const {
    const auto* p = find_preset(name);
    return p && match.low_width == p->low && match.low_height == p->low && match.high_width == p->high &&
        match.high_height == p->high && match.bidirectional == p->bidirectional && match.overlap_saturation == -1 &&
        pairs.neighbors == p->neighbors && pairs.reference_coverage == p->coverage && stride == p->stride;
}

void DenseConfig::apply_source_workflow() {
    matching_space = "source"; reference_fraction = 0.8; pairs.neighbors = 3;
    samples_per_reference = 10000; sampling_seed = 0; source_reprojection_error = 1;
}

GeometryOptions DenseConfig::resolved_geometry() const {
    auto out = geometry;
    if (matching_space == "source") out.max_reprojection_error = source_reprojection_error;
    return out;
}

void DenseConfig::validate() const {
    match.validate(); pairs.validate(); geometry.validate();
    if (preset != "custom" && !find_preset(preset)) roma::MatchOptions::preset(preset);
    auto positive = [](double v) { return std::isfinite(v) && v > 0; };
    auto probability = [](double v) { return std::isfinite(v) && v >= 0 && v <= 1; };
    if ((matching_space != "rectified" && matching_space != "source") ||
        !positive(reference_fraction) || reference_fraction > 1 || !positive(source_reprojection_error))
        throw std::runtime_error("invalid dense matching space, reference fraction, or source pixel tolerance");
    if (!probability(min_overlap) || !probability(mask_threshold) || !positive(max_cycle_error) ||
        !std::isfinite(voxel_size) || voxel_size < 0 || !positive(outlier_stddev))
        throw std::runtime_error("invalid dense filtering, mask, or fusion threshold");
    if ((two_image_points != "auto" && two_image_points != "off") || !std::isfinite(max_depth_error_per_cell) ||
        !std::isfinite(max_baseline))
        throw std::runtime_error("invalid dense two-image, depth precision, or baseline filter");
    if (stride < 1 || outlier_neighbors < 2 || cpu_workers < 0 || max_face_size < 16 || metashape_component < -1)
        throw std::runtime_error("invalid dense sampling, view, worker, or mask limits");
    if (exif_orientation != "none" && exif_orientation != "orient" && exif_orientation != "apply")
        throw std::runtime_error("unknown dense EXIF orientation policy");
    if (checkpoint.empty() || image_dir.empty()) throw std::runtime_error("dense checkpoint and image directory must not be empty");
    if (pairs.mode == PairMode::Explicit && pair_list.empty() && pairs.explicit_pairs.empty())
        throw std::runtime_error("dense explicit pair mode needs a pair list");
}

void DenseConfig::validate_run() const {
    validate();
    if (pairs.mode == PairMode::Automatic && (int64_t)pairs.neighbors + 1 < geometry.min_source_images)
        throw std::runtime_error("dense automatic neighbors cannot provide the required distinct-image support; increase neighbors or reduce min_source_images");
}

}  // namespace spirula::dense
