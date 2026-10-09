#pragma once

#include "sfm/core/FeatureCompaction.h"
#include "sfm/feature/SpatialBlocks.h"
#include "sfm/map/Atoms.h"

namespace sfm::regional {
struct Region { std::vector<uint32_t> core, images; };
struct RegionInputOverBudget : std::length_error {
    RegionInputOverBudget() : std::length_error("regional matches and graph exceed input budget") {}
};

inline bool expandAlignmentContext(const MatchesIndex& index, Region& region,
                                   const std::set<uint32_t>& unresolved,
                                   const std::set<uint32_t>& anchors, size_t limit) {
    std::set<uint32_t> included(region.images.begin(), region.images.end());
    std::map<uint32_t, uint64_t> votes;
    for (const auto& pair : index.pairs) {
        auto vote = [&](uint32_t a, uint32_t b) {
            if (unresolved.count(a) && anchors.count(b) && !included.count(b)) votes[b] += pair.count;
        };
        vote(pair.image1, pair.image2); vote(pair.image2, pair.image1);
    }
    std::vector<std::pair<uint64_t, uint32_t>> ranked;
    for (const auto& kv : votes) if (kv.second) ranked.emplace_back(kv.second, kv.first);
    std::sort(ranked.begin(), ranked.end(), [](auto a, auto b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    size_t count = std::min(limit, ranked.size());
    for (size_t k = 0; k < count; ++k) region.images.push_back(ranked[k].second);
    std::sort(region.images.begin(), region.images.end());
    return count != 0;
}

inline std::vector<Region> planRegions(const MatchesIndex& index, const std::vector<Vec3>& gps,
                                     size_t requested) {
    if (gps.empty() || gps.size() != index.images.size() || requested < 2)
        throw std::invalid_argument("invalid regional mapping plan");
    SpatialBlockPlan partition = SpatialBlockIndex(gps).plan(requested, 1, 0);
    std::vector<Region> out(partition.blocks);
    for (uint32_t i = 0; i < gps.size(); ++i) out[partition.owner[i]].core.push_back(i);
    std::vector<std::map<uint32_t, uint64_t>> votes(out.size());
    for (const auto& pair : index.pairs) {
        uint32_t a = partition.owner[pair.image1], b = partition.owner[pair.image2];
        if (a == b) continue;
        votes[a][pair.image2] += pair.count; votes[b][pair.image1] += pair.count;
    }
    for (size_t r = 0; r < out.size(); ++r) {
        out[r].images = out[r].core;
        std::vector<std::pair<uint64_t, uint32_t>> halo;
        for (const auto& kv : votes[r]) halo.emplace_back(kv.second, kv.first);
        std::sort(halo.begin(), halo.end(), [](auto a, auto b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        const size_t count = std::min(halo.size(), std::max<size_t>(16, out[r].core.size() / 4));
        for (size_t k = 0; k < count; ++k) out[r].images.push_back(halo[k].second);
        std::sort(out[r].images.begin(), out[r].images.end());
    }
    return out;
}

struct RegionInput {
    MatchesDatabase db;
    std::vector<FeatureSet> features;
    std::vector<uint32_t> global;
    std::vector<std::vector<uint32_t>> original;
};

inline RegionInput loadRegion(const MatchesIndex& index, const Region& region,
                              const std::filesystem::path& matches, const std::filesystem::path& features,
                              size_t budget) {
    RegionInput out; out.global = region.images;
    std::vector<uint32_t> local(index.images.size(), UINT32_MAX);
    uint64_t input = 0;
    for (uint32_t id : out.global) input += uint64_t(index.images.at(id).num_features) * 31;
    for (uint32_t i = 0; i < out.global.size(); ++i) local.at(out.global[i]) = i;
    for (const auto& pair : index.pairs)
        if (local.at(pair.image1) != UINT32_MAX && local.at(pair.image2) != UINT32_MAX)
            input += uint64_t(pair.count) * 32;
    if (input > budget / 2) throw RegionInputOverBudget();
    const auto& metadata = index.metadata;
    out.db.cameras = metadata.cameras; out.db.focal_prior = metadata.focal_prior;
    out.db.focal_measured = metadata.focal_measured;
    for (uint32_t g : out.global) {
        out.db.images.push_back(index.images[g]); out.db.camera_ids.push_back(metadata.camera_ids.at(g));
    }
    std::ifstream in(matches, std::ios::binary);
    for (const auto& pair : index.pairs) {
        uint32_t a = local.at(pair.image1), b = local.at(pair.image2);
        if (a == UINT32_MAX || b == UINT32_MAX) continue;
        cancel::check(); TwoViewMatches p; p.image1 = a; p.image2 = b; p.config = pair.config;
        if (!readPairMatches(in, pair, p.matches.mut())) throw std::runtime_error("cannot read regional match pair");
        out.db.pairs.push_back(std::move(p));
    }
    FeatureCompactionPlan compact = buildFeatureCompactionPlan(out.db);
    out.original.resize(out.global.size()); out.features.reserve(out.global.size());
    for (uint32_t i = 0; i < out.global.size(); ++i) {
        cancel::check();
        for (uint32_t f = 0; f < compact.old_to_new[i].size(); ++f)
            if (compact.old_to_new[i][f] != kUnusedFeature) out.original[i].push_back(f);
        FeatureSet fs = readFeatures((features / (index.images[out.global[i]].name + ".bin")).string(), false);
        out.features.push_back(compactFeatureSet(std::move(fs), compact.old_to_new[i], compact.compact_counts[i]));
    }
    remapMatches(out.db, compact, out.features);
    return out;
}

inline void restoreFeatureIds(Reconstruction& rec, const RegionInput& input, const MatchesIndex& index) {
    for (auto& kv : rec.points3D)
        for (TrackElement& e : kv.second.track) e.point2D_idx = input.original.at(e.image_id).at(e.point2D_idx);
    for (auto& kv : rec.images) {
        Image& im = kv.second; if (!im.registered) continue;
        uint32_t global = input.global.at(kv.first);
        std::vector<Vec2> xy(index.images.at(global).num_features);
        std::vector<uint64_t> ids(xy.size(), kInvalidPoint3D);
        for (uint32_t f = 0; f < im.points2D.size(); ++f) {
            uint32_t old = input.original.at(kv.first).at(f); xy.at(old) = im.points2D[f]; ids.at(old) = im.point3D_ids.at(f);
        }
        im.points2D = std::move(xy); im.point3D_ids = std::move(ids);
    }
    detail::toGlobalIds(rec, input.global);
}
} // namespace sfm::regional
