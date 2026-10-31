// Relative poses for a verified view graph; pair indices refer to MatchesDatabase.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

#include "sfm/core/Features.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Matches.h"
#include "sfm/geometry/TwoView.h"

namespace sfm {

enum class RelativeViewStatus : uint8_t {
    Accepted, InvalidPair, Unverified, Degenerate, TooFewMatches, InvalidFeature,
    InvalidCamera, NoPose
};

struct RelativeViewEdge {
    Pose pose;
    uint32_t inliers = 0;
    // Bits index the original pair's matches, including when matches are spilled.
    std::vector<uint64_t> match_mask;
    RelativeViewStatus status = RelativeViewStatus::InvalidPair;

    bool isInlier(size_t index) const {
        return index / 64 < match_mask.size() && (match_mask[index / 64] & (1ull << (index % 64)));
    }
};

struct RelativeViewOptions {
    TwoViewOptions geometry;
    int threads = 0;
    double min_rotation_only_ratio = 0.9;
};

namespace relative_view_detail {

inline Mat3 fitRotation(const std::vector<Vec3>& a, const std::vector<Vec3>& b,
                        const std::vector<int>& indices) {
    Mat3 covariance{};
    for (int index : indices) {
        const Vec3& x = a[index];
        const Vec3& y = b[index];
        covariance[0] += y.x * x.x; covariance[1] += y.x * x.y; covariance[2] += y.x * x.z;
        covariance[3] += y.y * x.x; covariance[4] += y.y * x.y; covariance[5] += y.y * x.z;
        covariance[6] += y.z * x.x; covariance[7] += y.z * x.y; covariance[8] += y.z * x.z;
    }
    Svd3 svd = svd3(covariance);
    Mat3 rotation = mul(svd.U, transpose(svd.V));
    if (det3(rotation) < 0) {
        svd.U[2] = -svd.U[2];
        svd.U[5] = -svd.U[5];
        svd.U[8] = -svd.U[8];
        rotation = mul(svd.U, transpose(svd.V));
    }
    return rotation;
}

inline bool estimateRotationOnly(const std::vector<Vec3>& a, const std::vector<Vec3>& b,
                                 const TwoViewOptions& geometry, double min_ratio,
                                 RelativeViewEdge& edge) {
    const int n = (int)a.size();
    auto fit = [&](const std::vector<int>& indices) {
        return std::vector<Mat3>{fitRotation(a, b, indices)};
    };
    auto residual = [&](const Mat3& rotation, int i) {
        return 2.0 * std::max(0.0, 1.0 - b[i].dot(mul(rotation, a[i])));
    };
    RansacReport<Mat3> report = loransac<Mat3>(n, 3, fit, fit, residual, geometry.ransac);
    if (!report.success || report.num_inliers < geometry.min_num_inliers ||
        report.num_inliers < min_ratio * n)
        return false;
    edge.pose = {report.model, {0, 0, 0}};
    edge.inliers = (uint32_t)report.num_inliers;
    edge.match_mask.assign((report.inlier_mask.size() + 63) / 64, 0);
    for (size_t i = 0; i < report.inlier_mask.size(); i++)
        if (report.inlier_mask[i]) edge.match_mask[i / 64] |= 1ull << (i % 64);
    edge.status = RelativeViewStatus::Accepted;
    return true;
}

inline bool estimatePlanarTranslation(const std::vector<Vec3>& a, const std::vector<Vec3>& b,
                                      TwoViewOptions geometry, RelativeViewEdge& edge) {
    geometry.estimate_homography = true;
    geometry.recover_pose = false;
    const TwoViewGeometry fit = estimateTwoViewBearing(a, b, geometry);
    if (fit.config != TwoViewConfig::PlanarOrPanoramic ||
        fit.num_inliers < geometry.min_num_inliers)
        return false;
    std::vector<int> indices;
    indices.reserve(fit.num_inliers);
    for (size_t i = 0; i < fit.inlier_mask.size(); i++)
        if (fit.inlier_mask[i]) indices.push_back((int)i);
    HomographyPoseCandidate pose;
    int cheirality_count = 0;
    std::vector<char> cheirality;
    if (!recoverHomographyPoseBearing(a, b, indices, fit.H, pose, cheirality_count, &cheirality) ||
        cheirality_count < geometry.min_num_inliers ||
        cheirality_count * 10 < (int)indices.size() * 7)
        return false;
    edge.pose = {pose.R, pose.t.normalized()};
    edge.inliers = (uint32_t)cheirality_count;
    edge.match_mask.assign((a.size() + 63) / 64, 0);
    for (size_t k = 0; k < indices.size(); k++)
        if (cheirality[k]) edge.match_mask[indices[k] / 64] |= 1ull << (indices[k] % 64);
    edge.status = RelativeViewStatus::Accepted;
    return true;
}

}  // namespace relative_view_detail

inline RelativeViewEdge estimateRelativeView(const TwoViewMatches& pair,
                                             const std::vector<FeatureSet>& features,
                                             const std::vector<Camera>& cameras,
                                             const RelativeViewOptions& options) {
    RelativeViewEdge edge;
    const uint32_t a = pair.image1, b = pair.image2;
    if (!(options.min_rotation_only_ratio > 0 && options.min_rotation_only_ratio <= 1) ||
        a >= features.size() || b >= features.size() || a >= cameras.size() ||
        b >= cameras.size() || a == b) return edge;
    if (pair.config == (int)TwoViewConfig::Undefined) {
        edge.status = RelativeViewStatus::Unverified;
        return edge;
    }
    const bool planar_or_panoramic = pair.config == (int)TwoViewConfig::PlanarOrPanoramic;
    if (pair.config != (int)TwoViewConfig::Uncalibrated && !planar_or_panoramic) {
        edge.status = RelativeViewStatus::Degenerate;
        return edge;
    }
    if (pair.matches.size() < (size_t)std::max(8, options.geometry.min_num_inliers)) {
        edge.status = RelativeViewStatus::TooFewMatches;
        return edge;
    }
    const Camera& ca = cameras[a];
    const Camera& cb = cameras[b];
    const auto validCamera = [](const Camera& c) {
        return std::isfinite(c.fx) && std::isfinite(c.fy) &&
               std::isfinite(c.cx) && std::isfinite(c.cy) && c.fx > 0 && c.fy > 0;
    };
    if (!validCamera(ca) || !validCamera(cb)) {
        edge.status = RelativeViewStatus::InvalidCamera;
        return edge;
    }
    std::vector<Vec3> ba, bb;
    ba.reserve(pair.matches.size());
    bb.reserve(pair.matches.size());
    for (const FeatureMatch& m : pair.matches) {
        if (m.idx1 >= features[a].keypoints.size() || m.idx2 >= features[b].keypoints.size()) {
            edge.status = RelativeViewStatus::InvalidFeature;
            return edge;
        }
        const Keypoint& ka = features[a].keypoints[m.idx1];
        const Keypoint& kb = features[b].keypoints[m.idx2];
        Vec3 va = ca.bearing({ka.x, ka.y}), vb = cb.bearing({kb.x, kb.y});
        if (!std::isfinite(va.x) || !std::isfinite(va.y) || !std::isfinite(va.z) ||
            !std::isfinite(vb.x) || !std::isfinite(vb.y) || !std::isfinite(vb.z) ||
            va.norm() < 1e-12 || vb.norm() < 1e-12) {
            edge.status = RelativeViewStatus::InvalidFeature;
            return edge;
        }
        ba.push_back(va.normalized());
        bb.push_back(vb.normalized());
    }
    TwoViewOptions opt = options.geometry;
    opt.estimate_homography = false;
    opt.recover_pose = true;
    const double scale = 0.5 * (features[a].pixelScale() + features[b].pixelScale());
    opt.ransac.max_error *= scale / std::max(1.0, 0.5 * (ca.focal() + cb.focal()));
    if (planar_or_panoramic) {
        opt.recover_pose = false;
        if (relative_view_detail::estimateRotationOnly(ba, bb, opt,
                                                       options.min_rotation_only_ratio, edge))
            return edge;
        if (!relative_view_detail::estimatePlanarTranslation(ba, bb, opt, edge))
            edge.status = RelativeViewStatus::Degenerate;
        return edge;
    }
    TwoViewGeometry g = estimateTwoViewBearing(ba, bb, opt);
    if (!g.has_pose || g.config != TwoViewConfig::Uncalibrated) {
        edge.status = RelativeViewStatus::NoPose;
        return edge;
    }
    edge.pose = g.pose;
    edge.inliers = (uint32_t)g.num_inliers;
    edge.match_mask.assign((g.inlier_mask.size() + 63) / 64, 0);
    const Mat3 inverse = transpose(g.pose.R);
    const Vec3 centre = mul(inverse, g.pose.t) * -1.0;
    for (size_t k = 0; k < g.inlier_mask.size(); k++) {
        if (!g.inlier_mask[k]) continue;
        const Vec3 ray = mul(inverse, bb[k]);
        const double cosine = ba[k].dot(ray);
        const double denominator = 1.0 - cosine * cosine;
        if (denominator < 1e-6) continue;
        const double a = ba[k].dot(centre), b = ray.dot(centre);
        if ((a - cosine * b) <= 0 || (cosine * a - b) <= 0) continue;
        edge.match_mask[k / 64] |= 1ull << (k % 64);
    }
    edge.status = RelativeViewStatus::Accepted;
    return edge;
}

inline std::vector<RelativeViewEdge> buildRelativeViewGraph(
    const MatchesDatabase& db, const std::vector<FeatureSet>& features,
    const std::vector<Camera>& cameras, const RelativeViewOptions& options = {}) {
    if (features.size() != db.images.size() || cameras.size() != db.images.size())
        throw std::invalid_argument("relative view graph: image, feature and camera counts differ");
    std::vector<RelativeViewEdge> edges(db.pairs.size());
    std::atomic<size_t> next{0};
    const unsigned hc = std::thread::hardware_concurrency();
    const size_t requested = options.threads > 0 ? (size_t)options.threads : (hc ? hc : 1);
    const size_t count = std::max<size_t>(1, std::min(requested, db.pairs.size()));
    auto worker = [&] {
        for (size_t i = next.fetch_add(1); i < db.pairs.size() && !cancel::requested(); i = next.fetch_add(1))
            edges[i] = estimateRelativeView(db.pairs[i], features, cameras, options);
    };
    std::vector<std::thread> pool;
    for (size_t i = 1; i < count; i++) pool.emplace_back(worker);
    worker();
    for (std::thread& thread : pool) thread.join();
    cancel::check();
    return edges;
}

}  // namespace sfm
