// Lossless in-memory removal of feature rows no stored match references.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"

namespace sfm {

using StoredFeatureIndex = decltype(FeatureMatch::idx1);
static_assert(std::is_same<StoredFeatureIndex, uint32_t>::value,
              "feature compaction must follow the stored match index type");

constexpr StoredFeatureIndex kUnusedFeature =
    std::numeric_limits<StoredFeatureIndex>::max();

inline std::runtime_error compactionError(const std::string& what) {
    return std::runtime_error("unused-feature compaction: " + what);
}

namespace feature_compaction_detail {

// UINT32_MAX is the unused sentinel, so retained endpoint indices end one below it.
inline StoredFeatureIndex checkedCompactIndex(uint64_t index) {
    if (index >= uint64_t(kUnusedFeature))
        throw compactionError("compact feature index " + std::to_string(index) +
                              " exceeds the stored match index range");
    return static_cast<StoredFeatureIndex>(index);
}

}  // namespace feature_compaction_detail

struct FeatureCompactionStats {
    uint64_t original_features = 0;
    uint64_t compact_features = 0;
    uint64_t correspondences = 0;
    uint64_t pairs = 0;
    uint64_t images = 0;
    uint64_t zero_feature_images = 0;

    uint64_t removedFeatures() const { return original_features - compact_features; }
};

struct FeatureCompactionPlan {
    FeatureCompactionStats stats;

    // Per original feature row: compact index, or UINT32_MAX when unreferenced.
    std::vector<std::vector<StoredFeatureIndex>> old_to_new;
    std::vector<uint32_t> compact_counts;

    // Snapshots reject mutation between planning and endpoint remapping.
    std::vector<std::string> image_names;
    std::vector<uint32_t> image_feature_counts;
    std::vector<Camera> cameras;
    std::vector<uint32_t> camera_ids;
    std::vector<uint8_t> focal_prior;
    std::vector<uint32_t> pair_image1;
    std::vector<uint32_t> pair_image2;
    std::vector<int32_t> pair_config;
    std::vector<size_t> pair_match_counts;
};

inline bool sameCameraExact(const Camera& a, const Camera& b) {
    return a.id == b.id && a.width == b.width && a.height == b.height &&
           a.model == b.model && a.fx == b.fx && a.fy == b.fy && a.cx == b.cx &&
           a.cy == b.cy && a.k1 == b.k1 && a.k2 == b.k2 && a.p1 == b.p1 &&
           a.p2 == b.p2 && a.k3 == b.k3 && a.k4 == b.k4 && a.sx1 == b.sx1 &&
           a.sy1 == b.sy1 && a.pixel_scale == b.pixel_scale;
}

inline FeatureCompactionPlan buildFeatureCompactionPlan(const MatchesDatabase& db) {
    FeatureCompactionPlan plan;
    plan.stats.images = db.images.size();
    plan.stats.pairs = db.pairs.size();
    plan.old_to_new.resize(db.images.size());
    plan.compact_counts.resize(db.images.size(), 0);
    plan.cameras = db.cameras;
    plan.camera_ids = db.camera_ids;
    plan.focal_prior = db.focal_prior;
    plan.image_names.reserve(db.images.size());
    plan.image_feature_counts.reserve(db.images.size());
    plan.pair_image1.reserve(db.pairs.size());
    plan.pair_image2.reserve(db.pairs.size());
    plan.pair_config.reserve(db.pairs.size());
    plan.pair_match_counts.reserve(db.pairs.size());

    for (size_t i = 0; i < db.images.size(); i++) {
        const ImageEntry& image = db.images[i];
        plan.stats.original_features += image.num_features;
        plan.old_to_new[i].assign(image.num_features, kUnusedFeature);
        plan.image_names.push_back(image.name);
        plan.image_feature_counts.push_back(image.num_features);
    }

    for (size_t p = 0; p < db.pairs.size(); p++) {
        const TwoViewMatches& pair = db.pairs[p];
        if (pair.image1 >= db.images.size())
            throw compactionError("pair " + std::to_string(p) + ": image1 " +
                                  std::to_string(pair.image1) + " does not exist");
        if (pair.image2 >= db.images.size())
            throw compactionError("pair " + std::to_string(p) + ": image2 " +
                                  std::to_string(pair.image2) + " does not exist");

        plan.stats.correspondences += pair.matches.size();
        plan.pair_image1.push_back(pair.image1);
        plan.pair_image2.push_back(pair.image2);
        plan.pair_config.push_back(pair.config);
        plan.pair_match_counts.push_back(pair.matches.size());

        std::vector<StoredFeatureIndex>& a = plan.old_to_new[pair.image1];
        std::vector<StoredFeatureIndex>& b = plan.old_to_new[pair.image2];
        for (size_t k = 0; k < pair.matches.size(); k++) {
            const FeatureMatch& match = pair.matches[k];
            if (match.idx1 >= a.size())
                throw compactionError("pair " + std::to_string(p) + ", match " +
                                      std::to_string(k) + ": idx1 " +
                                      std::to_string(match.idx1) + " is outside image " +
                                      std::to_string(pair.image1));
            if (match.idx2 >= b.size())
                throw compactionError("pair " + std::to_string(p) + ", match " +
                                      std::to_string(k) + ": idx2 " +
                                      std::to_string(match.idx2) + " is outside image " +
                                      std::to_string(pair.image2));
            a[match.idx1] = 0;
            b[match.idx2] = 0;
        }
    }

    for (size_t i = 0; i < plan.old_to_new.size(); i++) {
        uint64_t next = 0;
        for (StoredFeatureIndex& mapped : plan.old_to_new[i]) {
            if (mapped == kUnusedFeature) continue;
            mapped = feature_compaction_detail::checkedCompactIndex(next);
            next++;
        }
        // Each input feature count is uint32_t, so next cannot exceed UINT32_MAX.
        if (next > uint64_t(std::numeric_limits<uint32_t>::max()))
            throw compactionError("compact feature count exceeds the file-format range");
        plan.compact_counts[i] = static_cast<uint32_t>(next);
        plan.stats.compact_features += next;
        if (next == 0) plan.stats.zero_feature_images++;
    }
    return plan;
}

inline FeatureSet compactFeatureSet(FeatureSet input,
                                    const std::vector<StoredFeatureIndex>& old_to_new,
                                    uint32_t compact_count) {
    if (input.keypoints.size() != old_to_new.size())
        throw compactionError("feature-file count " +
                              std::to_string(input.keypoints.size()) +
                              " does not match matches.bin count " +
                              std::to_string(old_to_new.size()));

    const bool have_colors = input.hasColors();
    if (!input.colors.empty() && !have_colors)
        throw compactionError("feature colors are not one RGB row per keypoint");
    const size_t descriptor_row = size_t(input.dim) * dtypeSize(input.dtype);
    const bool have_descriptors = !input.descriptors.empty();
    if (have_descriptors &&
        input.descriptors.size() != input.keypoints.size() * descriptor_row)
        throw compactionError("descriptor data is not one row per keypoint");

    bool identity = compact_count == input.keypoints.size();
    uint64_t expected = 0;
    for (size_t old = 0; old < old_to_new.size(); old++) {
        const StoredFeatureIndex mapped = old_to_new[old];
        if (mapped == kUnusedFeature) {
            identity = false;
            continue;
        }
        if (mapped != feature_compaction_detail::checkedCompactIndex(expected++))
            throw compactionError("old-to-new table is not stable and contiguous");
        if (mapped != old) identity = false;
    }
    if (expected != compact_count)
        throw compactionError("compact feature count does not match old-to-new table");
    if (identity) return input;

    FeatureSet out;
    out.width = input.width;
    out.height = input.height;
    out.extract_width = input.extract_width;
    out.extract_height = input.extract_height;
    out.exif_focal = input.exif_focal;
    out.exif_camera = input.exif_camera;
    out.exif_orientation = input.exif_orientation;
    out.dim = input.dim;
    out.dtype = input.dtype;
    out.keypoints.reserve(compact_count);
    if (have_colors) out.colors.reserve(size_t(compact_count) * 3);
    if (have_descriptors) out.descriptors.reserve(size_t(compact_count) * descriptor_row);

    for (size_t old = 0; old < old_to_new.size(); old++) {
        if (old_to_new[old] == kUnusedFeature) continue;
        out.keypoints.push_back(input.keypoints[old]);
        if (have_colors)
            out.colors.insert(out.colors.end(), input.colors.begin() + old * 3,
                              input.colors.begin() + old * 3 + 3);
        if (have_descriptors)
            out.descriptors.insert(out.descriptors.end(),
                                   input.descriptors.begin() + old * descriptor_row,
                                   input.descriptors.begin() + (old + 1) * descriptor_row);
    }

    if (out.count() != compact_count)
        throw compactionError("compacted FeatureSet has the wrong feature count");
    return out;
}

inline void remapMatches(MatchesDatabase& db, const FeatureCompactionPlan& plan,
                         const std::vector<FeatureSet>& compact_features) {
    if (db.images.size() != plan.old_to_new.size() ||
        db.images.size() != plan.compact_counts.size() ||
        compact_features.size() != db.images.size())
        throw compactionError("image count changed while applying the plan");
    if (db.pairs.size() != plan.stats.pairs)
        throw compactionError("pair count changed while applying the plan");
    if (plan.image_names.size() != db.images.size() ||
        plan.image_feature_counts.size() != db.images.size())
        throw compactionError("image snapshot has the wrong size");
    for (size_t i = 0; i < db.images.size(); i++) {
        if (db.images[i].name != plan.image_names[i])
            throw compactionError("image name or order changed while applying the plan");
        if (db.images[i].num_features != plan.image_feature_counts[i])
            throw compactionError("image feature count changed while applying the plan");
    }
    if (db.camera_ids != plan.camera_ids || db.focal_prior != plan.focal_prior ||
        db.cameras.size() != plan.cameras.size())
        throw compactionError("camera setup changed while applying the plan");
    for (size_t i = 0; i < db.cameras.size(); i++)
        if (!sameCameraExact(db.cameras[i], plan.cameras[i]))
            throw compactionError("camera setup changed while applying the plan");
    if (plan.pair_image1.size() != db.pairs.size() ||
        plan.pair_image2.size() != db.pairs.size() ||
        plan.pair_config.size() != db.pairs.size() ||
        plan.pair_match_counts.size() != db.pairs.size())
        throw compactionError("pair snapshot has the wrong size");

    uint64_t correspondences = 0;
    for (size_t p = 0; p < db.pairs.size(); p++) {
        TwoViewMatches& pair = db.pairs[p];
        if (pair.image1 != plan.pair_image1[p] || pair.image2 != plan.pair_image2[p] ||
            pair.config != plan.pair_config[p] ||
            pair.matches.size() != plan.pair_match_counts[p])
            throw compactionError("pair identity, order, config or match count changed");
        if (pair.image1 >= db.images.size() || pair.image2 >= db.images.size())
            throw compactionError("pair " + std::to_string(p) + " has an invalid image id");

        correspondences += pair.matches.size();
        const std::vector<StoredFeatureIndex>& a = plan.old_to_new[pair.image1];
        const std::vector<StoredFeatureIndex>& b = plan.old_to_new[pair.image2];
        for (FeatureMatch& match : pair.matches) {
            if (match.idx1 >= a.size() || match.idx2 >= b.size())
                throw compactionError("match index changed before remapping");
            const StoredFeatureIndex mapped1 = a[match.idx1];
            const StoredFeatureIndex mapped2 = b[match.idx2];
            if (mapped1 == kUnusedFeature || mapped2 == kUnusedFeature)
                throw compactionError("a stored match endpoint was marked unused");
            if (mapped1 >= compact_features[pair.image1].count() ||
                mapped2 >= compact_features[pair.image2].count())
                throw compactionError("a remapped match endpoint is out of bounds");
            match.idx1 = mapped1;
            match.idx2 = mapped2;
        }
    }
    if (correspondences != plan.stats.correspondences)
        throw compactionError("correspondence count changed while applying the plan");

    for (size_t i = 0; i < db.images.size(); i++) {
        if (compact_features[i].count() != plan.compact_counts[i])
            throw compactionError("compacted FeatureSet count disagrees with its plan");
        db.images[i].num_features = plan.compact_counts[i];
    }
}

}  // namespace sfm
