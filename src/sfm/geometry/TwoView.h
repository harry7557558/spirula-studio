// Two-view verification on pixels or unit bearings, with planar-model competition.
#pragma once

#include <vector>

#include "sfm/geometry/Essential.h"
#include "sfm/geometry/EssentialFivePoint.h"
#include "sfm/geometry/KnownRotation.h"
#include "sfm/geometry/Fundamental.h"
#include "sfm/geometry/Homography.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/optim/Ransac.h"

namespace sfm {

enum class TwoViewConfig {
    Undefined = 0,
    Degenerate,          // too few inliers
    Uncalibrated,        // F best explains it
    PlanarOrPanoramic,   // H best explains it (planar scene or pure rotation)
};

inline const char* twoViewConfigName(TwoViewConfig c) {
    switch (c) {
        case TwoViewConfig::Degenerate: return "degenerate";
        case TwoViewConfig::Uncalibrated: return "uncalibrated";
        case TwoViewConfig::PlanarOrPanoramic: return "planar/panoramic";
        default: return "undefined";
    }
}

struct TwoViewOptions {
    // Error is measured in extraction pixels; bearing callers convert it to radians.
    RansacOptions ransac;
    int min_num_inliers = 15;
    double max_H_inlier_ratio = 0.8;
    // Seed search can reuse verification's planar verdict.
    bool estimate_homography = true;
    // Optional calibrated pose recovery from the F inliers.
    bool recover_pose = false;
    bool calibrated_essential = false;
    Mat3 K1 = mat3Identity(), K2 = mat3Identity();
    TwoViewOptions() {
        ransac.max_error = 3.0;
        ransac.min_inlier_ratio = 0.0;
    }
};

struct TwoViewGeometry {
    TwoViewConfig config = TwoViewConfig::Undefined;
    std::vector<char> inlier_mask;  // over the input correspondences
    int num_inliers = 0;
    // In the pixel path these are the fundamental and the homography, in
    // pixels. In the bearing path (D45) they are the same two maps expressed
    // on unit rays -- an essential matrix up to the two extra degrees of
    // freedom the 7-point fit leaves free, and a ray-to-ray homography.
    Mat3 F = {}, H = {};
    bool has_pose = false;
    Pose pose;  // relative pose (camera1 -> camera2), unit translation
};

namespace detail {
struct HModel {
    Mat3 H = mat3Identity(), Hinv = mat3Identity();
};

// Shared tail of both paths: pick F or H by COLMAP's H-inlier-ratio rule and
// copy the winner's inliers out.
inline void selectTwoViewModel(TwoViewGeometry& g, const RansacReport<Mat3>& fRep,
                               const RansacReport<HModel>& hRep, const TwoViewOptions& opt) {
    g.F = fRep.model;
    g.H = hRep.model.H;
    if (std::max(fRep.num_inliers, hRep.num_inliers) < opt.min_num_inliers) {
        g.config = TwoViewConfig::Degenerate;
        return;
    }
    // A high H-inlier ratio means a planar scene or pure rotation, where the
    // epipolar geometry is degenerate (COLMAP max_H_inlier_ratio = 0.8).
    double hRatio = opt.estimate_homography
                        ? (double)hRep.num_inliers / std::max(1, fRep.num_inliers)
                        : 0.0;
    if (hRatio > opt.max_H_inlier_ratio) {
        g.config = TwoViewConfig::PlanarOrPanoramic;
        g.inlier_mask = hRep.inlier_mask;
        g.num_inliers = hRep.num_inliers;
    } else {
        g.config = TwoViewConfig::Uncalibrated;
        g.inlier_mask = fRep.inlier_mask;
        g.num_inliers = fRep.num_inliers;
    }
}
}  // namespace detail

// Unit-ray verification; ransac.max_error is in radians, including backward fisheye rays.
inline TwoViewGeometry estimateTwoViewBearing(const std::vector<Vec3>& b1,
                                              const std::vector<Vec3>& b2,
                                              const TwoViewOptions& opt) {
    TwoViewGeometry g;
    int n = (int)b1.size();
    if (n < opt.min_num_inliers) return g;

    auto fFit = [&](const std::vector<int>& sample) {
        return opt.calibrated_essential ? estimateEssential5Bearing(b1, b2, sample)
                                        : estimateEpipolar7Bearing(b1, b2, sample);
    };
    auto fRefit = [&](const std::vector<int>& sample, const Mat3& incumbent) {
        if (!opt.calibrated_essential) return estimateEpipolar8Bearing(b1, b2, sample);
        std::vector<char> mask(n, 0);
        for (int i : sample) mask[i] = 1;
        Pose pose = decomposeEssential(incumbent).front();
        known_rotation_detail::refineNearRotation(b1, b2, mask, pose.R, 0,
                                                  opt.ransac.max_error, pose, 8);
        return std::vector<Mat3>{mul(crossMatrix(pose.t), pose.R)};
    };
    struct EssentialResidual {
        const std::vector<Vec3>& b1;
        const std::vector<Vec3>& b2;
        mutable Mat3 Et{};
        void prepare(const Mat3& E) const { Et = transpose(E); }
        double operator()(const Mat3& E, int i) const { return sampsonSqBearing(E, Et, b1[i], b2[i]); }
    };
    EssentialResidual fRes{b1, b2};
    RansacReport<Mat3> fRep = loransac<Mat3>(n, opt.calibrated_essential ? 5 : 7,
                                           fFit, fRefit, fRes, opt.ransac);

    RansacReport<detail::HModel> hRep;
    if (opt.estimate_homography) {
        auto hFit = [&](const std::vector<int>& s) {
            std::vector<detail::HModel> out;
            for (const Mat3& H : estimateHomographyBearing(b1, b2, s))
                out.push_back({H, inverse3(H)});
            return out;
        };
        auto hRes = [&](const detail::HModel& m, int i) {
            return angularTransferSq(m.H, m.Hinv, b1[i], b2[i],
                                     opt.ransac.max_error * opt.ransac.max_error);
        };
        hRep = loransac<detail::HModel>(n, 4, hFit, hFit, hRes, opt.ransac);
    }

    detail::selectTwoViewModel(g, fRep, hRep, opt);
    if (g.config == TwoViewConfig::Degenerate || g.config == TwoViewConfig::Undefined) return g;

    if (opt.recover_pose && g.config == TwoViewConfig::Uncalibrated && g.num_inliers >= 5) {
        std::vector<int> idx;
        for (int i = 0; i < n; i++)
            if (g.inlier_mask[i]) idx.push_back(i);
        int cnt = 0;
        g.pose = recoverRelativePoseBearing(b1, b2, idx, projectToEssential(g.F), cnt);
        g.has_pose = cnt > 0;
    }
    return g;
}

// Verify one image pair. p1[k] <-> p2[k] are matched keypoint pixel coords.
inline TwoViewGeometry estimateTwoView(const std::vector<Vec2>& p1, const std::vector<Vec2>& p2,
                                       const TwoViewOptions& opt) {
    TwoViewGeometry g;
    int n = (int)p1.size();
    if (n < opt.min_num_inliers) return g;

    // --- Fundamental ---
    struct FundamentalResidual {
        const std::vector<Vec2>& p1;
        const std::vector<Vec2>& p2;
        mutable Mat3 Ft{};
        void prepare(const Mat3& F) const { Ft = transpose(F); }
        double operator()(const Mat3& F, int i) const { return sampsonSq(F, Ft, p1[i], p2[i]); }
    };
    FundamentalResidual fRes{p1, p2};
    auto fFit = [&](const std::vector<int>& s) { return estimateFundamental7(p1, p2, s); };
    auto fRefit = [&](const std::vector<int>& s) { return estimateFundamental8(p1, p2, s); };
    RansacReport<Mat3> fRep = loransac<Mat3>(n, 7, fFit, fRefit, fRes, opt.ransac);

    // --- Homography ---
    RansacReport<detail::HModel> hRep;
    if (opt.estimate_homography) {
        auto hFit = [&](const std::vector<int>& s) {
            std::vector<detail::HModel> out;
            for (const Mat3& H : estimateHomography(p1, p2, s)) out.push_back({H, inverse3(H)});
            return out;
        };
        auto hRes = [&](const detail::HModel& m, int i) {
            return symmetricTransferSq(m.H, m.Hinv, p1[i], p2[i]);
        };
        hRep = loransac<detail::HModel>(n, 4, hFit, hFit, hRes, opt.ransac);
    }

    detail::selectTwoViewModel(g, fRep, hRep, opt);
    if (g.config == TwoViewConfig::Degenerate || g.config == TwoViewConfig::Undefined) return g;

    // Optional calibrated relative pose from the F inliers.
    if (opt.recover_pose && g.config == TwoViewConfig::Uncalibrated && g.num_inliers >= 5) {
        Mat3 K1inv = inverse3(opt.K1), K2inv = inverse3(opt.K2);
        std::vector<Vec2> n1(n), n2(n);
        std::vector<int> idx;
        for (int i = 0; i < n; i++) {
            Vec3 a = mul(K1inv, Vec3{p1[i].x, p1[i].y, 1});
            Vec3 b = mul(K2inv, Vec3{p2[i].x, p2[i].y, 1});
            n1[i] = {a.x / a.z, a.y / a.z};
            n2[i] = {b.x / b.z, b.y / b.z};
            if (g.inlier_mask[i]) idx.push_back(i);
        }
        Mat3 E = essentialFromFundamental(g.F, opt.K1, opt.K2);
        int cnt = 0;
        g.pose = recoverRelativePose(n1, n2, idx, E, cnt);
        g.has_pose = cnt > 0;
    }
    return g;
}

}  // namespace sfm
