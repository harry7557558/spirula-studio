#pragma once

#include <cstddef>

namespace gui {

enum class SfmPartitionMode { Auto = 0, Off = 1, On = 2 };

enum class SfmPartitionReason {
    Enabled,
    UserDisabled,
    BelowThreshold,
    NoGps,
    PartialGps,
    PendingGps,
    UnsupportedInput,
    Empty,
};

inline constexpr std::size_t kSfmPartitionAutoThreshold = 5000;
inline constexpr int kSfmRecommendedBlockSize = 2000;

struct SfmPartitionRecommendation {
    int photos = kSfmRecommendedBlockSize;
    int cache_mb = 512;
};

constexpr SfmPartitionRecommendation sfm_partition_recommendation(
    std::size_t total_bytes, std::size_t available_bytes,
    std::size_t features_per_photo = 8192,
    std::size_t descriptor_bytes_per_feature = 128) {
    if (!total_bytes) return {};
    if (!features_per_photo) features_per_photo = 8192;
    if (!descriptor_bytes_per_feature) descriptor_bytes_per_feature = 128;
    const auto budget = total_bytes / 2;
    // Reserve model/solver copies and overlap in addition to feature storage.
    const long double per_photo = static_cast<long double>(features_per_photo) *
        (static_cast<long double>(descriptor_bytes_per_feature) + 32) * 4;
    const long double estimate = budget / per_photo;
    const int photos = estimate >= 20000 ? 20000 : estimate < 2 ? 2
        : estimate < 500 ? static_cast<int>(estimate)
        : static_cast<int>(estimate) / 500 * 500;
    const long double descriptors = static_cast<long double>(photos) *
        features_per_photo * descriptor_bytes_per_feature;
    const auto available_cache = available_bytes ? available_bytes / 4 : std::size_t{512} * 1024 * 1024;
    const auto cache_budget = budget / 2 < available_cache ? budget / 2 : available_cache;
    const long double cache_bytes = descriptors < cache_budget ? descriptors : cache_budget;
    const long double cache_mib = cache_bytes / (1024 * 1024);
    const int cache_mb = cache_mib >= 4096 ? 4096 : cache_mib < 1 ? 1
        : static_cast<int>(cache_mib);
    return {photos, cache_mb};
}

struct SfmPartitionDecision {
    bool available = false;
    bool enabled = false;
    SfmPartitionReason reason = SfmPartitionReason::Empty;
};

constexpr SfmPartitionDecision sfm_partition_decision(
    int mode, std::size_t photos, std::size_t gps, bool ready, bool photos_only) {
    if (!photos_only) return {false, false, SfmPartitionReason::UnsupportedInput};
    if (!photos) return {false, false, SfmPartitionReason::Empty};
    if (!ready) return {false, false, SfmPartitionReason::PendingGps};
    if (!gps) return {false, false, SfmPartitionReason::NoGps};
    if (gps != photos) return {false, false, SfmPartitionReason::PartialGps};
    if (mode == static_cast<int>(SfmPartitionMode::On))
        return {true, true, SfmPartitionReason::Enabled};
    if (mode != static_cast<int>(SfmPartitionMode::Auto))
        return {true, false, SfmPartitionReason::UserDisabled};
    if (photos < kSfmPartitionAutoThreshold)
        return {true, false, SfmPartitionReason::BelowThreshold};
    return {true, true, SfmPartitionReason::Enabled};
}

}  // namespace gui
