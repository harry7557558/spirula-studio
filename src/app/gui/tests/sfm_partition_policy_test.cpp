#include "app/gui/SfmPartition.h"

#include <cstdio>
#include <initializer_list>

using namespace gui;

namespace {

int failures = 0;

void expect(bool ok, const char* what) {
    std::printf("%s  %s\n", ok ? "ok " : "BAD", what);
    if (!ok) ++failures;
}

SfmPartitionDecision decision(SfmPartitionMode mode, std::size_t photos,
                             std::size_t gps, bool ready = true,
                             bool photos_only = true) {
    return sfm_partition_decision(static_cast<int>(mode), photos, gps,
                                  ready, photos_only);
}

}  // namespace

int main() {
    const auto belowThreshold = decision(SfmPartitionMode::Auto, 4999, 4999);
    expect(belowThreshold.available && !belowThreshold.enabled &&
               belowThreshold.reason == SfmPartitionReason::BelowThreshold,
           "automatic mode disables a 4999-photo GPS project");
    const auto threshold = decision(SfmPartitionMode::Auto, 5000, 5000);
    expect(threshold.available && threshold.enabled &&
               threshold.reason == SfmPartitionReason::Enabled,
           "automatic mode enables at exactly 5000 GPS photos");
    expect(decision(SfmPartitionMode::Auto, 26749, 26749).enabled,
           "automatic mode enables large GPS projects");

    for (const auto mode : {SfmPartitionMode::Auto, SfmPartitionMode::On}) {
        const auto missing = decision(mode, 5000, 0);
        expect(!missing.available && !missing.enabled &&
                   missing.reason == SfmPartitionReason::NoGps,
               "GPS-free photos cannot enable partitioning");
        const auto partial = decision(mode, 5000, 4999);
        expect(!partial.available && !partial.enabled &&
                   partial.reason == SfmPartitionReason::PartialGps,
               "one photo without GPS prevents partitioning");
        const auto pending = decision(mode, 5000, 5000, false);
        expect(!pending.available && !pending.enabled &&
                   pending.reason == SfmPartitionReason::PendingGps,
               "unconfirmed GPS coverage cannot enable partitioning");
        const auto video = decision(mode, 5000, 5000, true, false);
        expect(!video.available && !video.enabled &&
                   video.reason == SfmPartitionReason::UnsupportedInput,
               "video and mixed inputs cannot enable photo partitioning");
        const auto empty = decision(mode, 0, 0);
        expect(!empty.available && !empty.enabled &&
                   empty.reason == SfmPartitionReason::Empty,
               "empty projects cannot enable partitioning");
    }

    expect(decision(SfmPartitionMode::On, 128, 128).enabled,
           "manual mode can enable a belowThreshold GPS project");
    const auto manual_off = decision(SfmPartitionMode::Off, 26749, 26749);
    expect(manual_off.available && !manual_off.enabled &&
               manual_off.reason == SfmPartitionReason::UserDisabled,
           "manual mode can disable a large GPS project");
    expect(!sfm_partition_decision(-1, 5000, 5000, true, true).enabled &&
               !sfm_partition_decision(3, 5000, 5000, true, true).enabled,
           "invalid persisted modes conservatively disable partitioning");
    expect(!decision(SfmPartitionMode::On, 5000, 5001).enabled,
           "inconsistent GPS counts cannot enable partitioning");
    constexpr std::size_t gib = std::size_t{1024} * 1024 * 1024;
    const auto ram32 = sfm_partition_recommendation(32 * gib, 16 * gib);
    expect(ram32.photos == 3000 && ram32.cache_mb == 3000,
           "half of 32 GiB RAM recommends 3000 photos including working reserves");
    const auto ram64 = sfm_partition_recommendation(64 * gib, 32 * gib);
    expect(ram64.photos == 6500 && ram64.cache_mb == 4096,
           "64 GiB RAM permits a larger block with a bounded cache");
    const auto pressure = sfm_partition_recommendation(32 * gib, 4 * gib);
    expect(pressure.photos == ram32.photos && pressure.cache_mb == 1024,
           "available memory changes cache but not the half-total-RAM photo recommendation");
    const auto high_features = sfm_partition_recommendation(
        32 * gib, 16 * gib, 16384);
    expect(high_features.photos == 1500,
           "more features per photo lower the recommended block size");
    const auto float_descriptors = sfm_partition_recommendation(
        32 * gib, 16 * gib, 8192, 512);
    expect(float_descriptors.photos == 500,
           "float descriptors lower the recommended block size");
    const auto unknown = sfm_partition_recommendation(0, 0);
    expect(unknown.photos == 2000 && unknown.cache_mb == 512,
           "unknown total memory uses a 2000-photo and 512 MiB fallback");
    const auto unknown_available = sfm_partition_recommendation(32 * gib, 0);
    expect(unknown_available.photos == ram32.photos && unknown_available.cache_mb == 512,
           "known total RAM still determines photos when available memory is unknown");
    const auto tiny = sfm_partition_recommendation(gib, gib / 8);
    const auto large = sfm_partition_recommendation(1024 * gib, 512 * gib);
    expect(tiny.photos == 102 && tiny.cache_mb == 32 &&
               large.photos == 20000 && large.cache_mb == 4096,
           "belowThreshold total memory reduces photo counts and large machines retain the upper bound");
    const auto exhausted = sfm_partition_recommendation(32 * gib, gib / 16);
    expect(exhausted.photos == ram32.photos && exhausted.cache_mb == 16,
           "very low free memory reduces cache while preserving the hardware recommendation");
    const auto minimum = sfm_partition_recommendation(gib, 1);
    expect(minimum.photos == tiny.photos && minimum.cache_mb == 1,
           "minimum cache is kept independently of the total-RAM photo recommendation");
    const auto zero_features = sfm_partition_recommendation(32 * gib, 16 * gib, 0, 0);
    expect(zero_features.photos == ram32.photos &&
               zero_features.cache_mb == ram32.cache_mb,
           "unspecified feature dimensions use the default estimate");
    return failures ? 1 : 0;
}
