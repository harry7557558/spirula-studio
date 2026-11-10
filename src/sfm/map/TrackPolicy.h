#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "sfm/core/Experiment.h"

namespace sfm {

struct AssociationVote {
    uint64_t point;
    uint32_t image;
    size_t track_length;
    bool nearby;
};

struct TrackAssociation {
    uint64_t point = std::numeric_limits<uint64_t>::max();
    size_t support = 0, alternatives = 0;
    bool nearby = false, tied = false;
};

struct PointAssociation {
    uint64_t point;
    uint32_t feature;
    size_t support;
    bool nearby;
};

inline std::vector<size_t> uniquePointAssociations(const std::vector<PointAssociation>& candidates) {
    std::vector<size_t> order(candidates.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t i, size_t j) {
        const auto& a = candidates[i];
        const auto& b = candidates[j];
        if (a.point != b.point) return a.point < b.point;
        if (a.nearby != b.nearby) return a.nearby > b.nearby;
        if (a.support != b.support) return a.support > b.support;
        return a.feature < b.feature;
    });
    order.erase(std::unique(order.begin(), order.end(), [&](size_t i, size_t j) {
        return candidates[i].point == candidates[j].point;
    }), order.end());
    std::sort(order.begin(), order.end());
    return order;
}

inline std::vector<size_t> bestPointObservations(const std::vector<uint64_t>& points,
                                                const std::vector<double>& errors) {
    std::vector<size_t> order;
    for (size_t i = 0; i < points.size(); ++i)
        if (std::isfinite(errors[i])) order.push_back(i);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        if (points[a] != points[b]) return points[a] < points[b];
        if (errors[a] != errors[b]) return errors[a] < errors[b];
        return a < b;
    });
    order.erase(std::unique(order.begin(), order.end(), [&](size_t a, size_t b) {
        return points[a] == points[b];
    }), order.end());
    std::sort(order.begin(), order.end());
    return order;
}

inline TrackAssociation supportedAssociation(std::vector<AssociationVote>& votes) {
    std::sort(votes.begin(), votes.end(), [](const auto& a, const auto& b) {
        if (a.point != b.point) return a.point < b.point;
        return a.image < b.image;
    });
    TrackAssociation best;
    size_t best_length = 0;
    for (size_t i = 0; i < votes.size();) {
        const size_t begin = i;
        size_t support = 0;
        bool nearby = false;
        for (; i < votes.size() && votes[i].point == votes[begin].point; ++i) {
            if (i == begin || votes[i].image != votes[i - 1].image) ++support;
            nearby = nearby || votes[i].nearby;
        }
        ++best.alternatives;
        const bool same = nearby == best.nearby && support == best.support;
        const bool better = (nearby && !best.nearby) ||
                            (nearby == best.nearby && support > best.support);
        if (better || (same && votes[begin].track_length > best_length)) {
            best.point = votes[begin].point;
            best.support = support;
            best.nearby = nearby;
            best_length = votes[begin].track_length;
            best.tied = same;
        } else if (same) best.tied = true;
    }
    return best;
}

struct TrackPolicy {
    int completion = 3;  // 0: legacy, 1: final, 2: every, 3: established final
    int association = 2; // 0: first, 1: supporting views, 2: defer tied support
    size_t merge_max_track = 20; // 0: unbounded
    bool refine_points = false;
    bool refine_fusion = false;
    int unique_points = 0; // 1: before PnP, 2: best residual after PnP
    bool ignore_two_view = false;

    void readExperiments() {
        completion = experimentInt("SFM_TRACK_COMPLETION", completion, 3);
        association = experimentInt("SFM_TRACK_ASSOCIATION", association, 2);
        merge_max_track = static_cast<size_t>(experimentInt(
            "SFM_TRACK_MERGE_CAP", static_cast<int>(merge_max_track), 1000000));
        refine_points = experimentInt("SFM_TRACK_REFINE", refine_points, 1) != 0;
        refine_fusion = experimentInt("SFM_TRACK_REFINE_FUSION", refine_fusion, 1) != 0;
        unique_points = experimentInt("SFM_TRACK_UNIQUE", unique_points, 2);
        ignore_two_view = experimentInt("SFM_IGNORE_TWO_VIEW", ignore_two_view, 1) != 0;
    }

    bool allowsMerge(size_t a, size_t b) const {
        return !merge_max_track || (a <= merge_max_track && b <= merge_max_track - a);
    }
};

} // namespace sfm
