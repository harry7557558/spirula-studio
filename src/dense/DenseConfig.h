#pragma once

#include "dense/Geometry.h"
#include "dense/PairSelection.h"
#include "roma/Roma.h"

#include <optional>
#include <string>

namespace spirula::dense {

// Fast, balanced and high set matching size, pairing and density together; turbo,
// base and precise are the older matcher-only presets that saved settings still name.
inline constexpr const char* kDensePresets[] = {"fast", "balanced", "high"};
inline bool is_dense_preset(const std::string& name) {
    for (const char* preset : kDensePresets) if (name == preset) return true;
    return false;
}

struct DenseConfig {
    DenseConfig() { apply_preset(preset); }
    roma::MatchOptions match;
    PairOptions pairs;
    GeometryOptions geometry;
    std::string preset = "balanced", checkpoint = "romav2.0.1";
    std::string image_dir = "images", recon_dir, pair_list;
    std::string mask_dir = "masks", feature_mask_dir = "feature_masks";
    bool use_masks = true;
    bool training_masks = true, alpha_masks = true, feature_masks = true;
    bool invert_masks = false;
    double mask_threshold = 0.5;
    int mask_boundary = 0;
    std::string exif_orientation = "none";
    std::string metashape_xml, metashape_psx;
    int metashape_component = -1;
    bool split_views = true;
    bool sparse_face_pairs = true;
    int max_face_size = 1280;
    std::string matching_space = "rectified";
    double reference_fraction = 0.8, source_reprojection_error = 1;   // matcher cells
    uint64_t samples_per_reference = 10000, sampling_seed = 0;
    int stride = 1;
    uint64_t point_limit = 0;
    double min_overlap = 0.5, max_cycle_error = 1;
    bool cycle_check = true;
    double voxel_size = 0;
    bool remove_outliers = false;
    int outlier_neighbors = 16;
    double outlier_stddev = 2;
    uint64_t image_cache_bytes = 0;
    int cpu_workers = 0;
    // Predictions stay on disk until the run succeeds, so a cancelled run resumes;
    // keep_cache keeps them afterwards too (about 10 MB a pair at 640, 80 MB at 1280 both ways).
    bool resume = true, rebuild = false, keep_cache = false;
    std::string device, image_gamut, image_exposure;
    std::optional<bool> image_is_linear;
    void apply_preset(const std::string& name);
    // True while every setting a preset controls still holds that preset's value.
    bool matches_preset(const std::string& name) const;
    void apply_source_workflow();
    GeometryOptions resolved_geometry() const;
    void validate() const;
    void validate_run() const;
    uint64_t resolved_image_cache_bytes() const;
    bool effective_cycle_check() const { return cycle_check && match.bidirectional; }
};

}  // namespace spirula::dense
