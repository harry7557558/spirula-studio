// Focal calibration from fundamental matrices over the view graph.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

#include "sfm/core/Camera.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Features.h"
#include "sfm/core/Matches.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/TwoView.h"

namespace sfm {

struct FocalGraphEdge {
    uint32_t camera1 = 0, camera2 = 0;
    Mat3 F{};
};

struct FocalGraphOptions {
    int max_iterations = 100, cg_iterations = 200;
    double cauchy_scale = 0.01, max_ratio = 10, tolerance = 1e-6;
    size_t min_edges = 3;
};

struct FocalGraphResult {
    std::map<uint32_t, Camera> cameras;
    std::set<uint32_t> calibrated;
    size_t edges = 0;
    double initial_cost = 0, final_cost = 0;
};

namespace focal_graph_detail {

using Coefficients = std::array<double, 4>;
// Fetzer's cross-product equations have a trivial solution at both focals = 1.
constexpr double focal_scale = 32;

inline Coefficients coefficients(const Vec3& a, const Vec3& b,
                                 const Vec3& c, const Vec3& d, int u, int v) {
    const double av[3] = {a.x, a.y, a.z}, bv[3] = {b.x, b.y, b.z};
    const double cv[3] = {c.x, c.y, c.z}, dv[3] = {d.x, d.y, d.z};
    return {av[u] * cv[v] - av[v] * cv[u], av[u] * dv[v] - av[v] * dv[u],
            bv[u] * cv[v] - bv[v] * cv[u], bv[u] * dv[v] - bv[v] * dv[u]};
}

struct Constraint {
    size_t a = 0, b = 0;
    Coefficients d01{}, d12{};
};

inline bool decompose(const Mat3& F, const Camera& a, const Camera& b, Constraint& c) {
    const Mat3 K0{a.focal() / focal_scale, 0, a.cx, 0, a.focal() / focal_scale, a.cy, 0, 0, 1};
    const Mat3 K1{b.focal() / focal_scale, 0, b.cx, 0, b.focal() / focal_scale, b.cy, 0, 0, 1};
    Mat3 G = mul(mul(transpose(K1), F), K0);
    double magnitude = 0;
    for (double x : G) {
        if (!std::isfinite(x)) return false;
        magnitude = std::max(magnitude, std::abs(x));
    }
    if (!(magnitude > 0) || !std::isfinite(magnitude)) return false;
    for (double& x : G) x /= magnitude;
    const Svd3 svd = svd3(G);
    if (!(svd.s.y > 1e-7 * svd.s.x)) return false;
    const Vec3 v0{svd.V[0], svd.V[3], svd.V[6]}, v1{svd.V[1], svd.V[4], svd.V[7]};
    const Vec3 u0{svd.U[0], svd.U[3], svd.U[6]}, u1{svd.U[1], svd.U[4], svd.U[7]};
    const double s0 = svd.s.x, s1 = svd.s.y;
    const Vec3 ai{s0 * s0 * (v0.x * v0.x + v0.y * v0.y),
                  s0 * s1 * (v0.x * v1.x + v0.y * v1.y),
                  s1 * s1 * (v1.x * v1.x + v1.y * v1.y)};
    const Vec3 bi{s0 * s0 * v0.z * v0.z, s0 * s1 * v0.z * v1.z, s1 * s1 * v1.z * v1.z};
    const Vec3 aj{u1.x * u1.x + u1.y * u1.y, -(u0.x * u1.x + u0.y * u1.y),
                  u0.x * u0.x + u0.y * u0.y};
    const Vec3 bj{u1.z * u1.z, -u0.z * u1.z, u0.z * u0.z};
    c.d01 = coefficients(ai, bi, aj, bj, 1, 0);
    c.d12 = coefficients(ai, bi, aj, bj, 2, 1);
    return true;
}

struct Linearization {
    double residual[2]{}, ja[2]{}, jb[2]{};
};

inline bool linearize(const Constraint& c, double log_a, double log_b, Linearization& out) {
    const double x = focal_scale * focal_scale * std::exp(2 * log_a);
    const double y = focal_scale * focal_scale * std::exp(2 * log_b);
    const auto& p = c.d01;
    const auto& q = c.d12;
    const double da = y * p[0] + p[1], db = x * q[0] + q[2];
    const double sa = std::max({std::abs(y * p[0]), std::abs(p[1]), 1e-30});
    const double sb = std::max({std::abs(x * q[0]), std::abs(q[2]), 1e-30});
    if (std::abs(da) < 1e-12 * sa || std::abs(db) < 1e-12 * sb) return false;
    const double ra = (y * p[2] + p[3]) / (x * da);
    const double rb = (x * q[1] + q[3]) / (y * db);
    out.residual[0] = 1 + ra;
    out.residual[1] = 1 + rb;
    out.ja[0] = -2 * ra;
    out.jb[0] = 2 * y * (p[2] * p[1] - p[0] * p[3]) / (x * da * da);
    out.ja[1] = 2 * x * (q[1] * q[2] - q[0] * q[3]) / (y * db * db);
    out.jb[1] = -2 * rb;
    for (int k = 0; k < 2; k++)
        if (!std::isfinite(out.residual[k]) || !std::isfinite(out.ja[k]) || !std::isfinite(out.jb[k]))
            return false;
    return true;
}

}  // namespace focal_graph_detail

inline FocalGraphResult calibrateFocalGraph(const std::map<uint32_t, Camera>& cameras,
                                           const std::set<uint32_t>& fixed,
                                           const std::vector<FocalGraphEdge>& edges,
                                           const FocalGraphOptions& opt = {}) {
    using namespace focal_graph_detail;
    FocalGraphResult result;
    result.cameras = cameras;
    if (opt.max_iterations < 1 || opt.cg_iterations < 1 || !(opt.cauchy_scale > 0) ||
        !(opt.max_ratio > 1 && opt.max_ratio < focal_scale) || !(opt.tolerance > 0) ||
        !std::isfinite(opt.cauchy_scale) ||
        !std::isfinite(opt.max_ratio) || !std::isfinite(opt.tolerance))
        throw std::invalid_argument("view graph calibration: invalid options");
    std::vector<uint32_t> ids;
    std::map<uint32_t, size_t> index;
    for (const auto& item : cameras) {
        if (item.second.isFisheye() || item.second.isSpherical() ||
            !(item.second.focal() > 0) || !std::isfinite(item.second.focal()) ||
            !std::isfinite(item.second.cx) || !std::isfinite(item.second.cy)) continue;
        index[item.first] = ids.size();
        ids.push_back(item.first);
    }
    const size_t n = ids.size();
    std::vector<Constraint> constraints;
    std::vector<size_t> counts(n, 0);
    for (const auto& edge : edges) {
        const auto a = index.find(edge.camera1), b = index.find(edge.camera2);
        if (a == index.end() || b == index.end()) continue;
        Constraint c;
        c.a = a->second; c.b = b->second;
        if (!decompose(edge.F, cameras.at(edge.camera1), cameras.at(edge.camera2), c)) continue;
        Linearization l;
        if (!linearize(c, 0, 0, l)) continue;
        constraints.push_back(c);
        counts[c.a]++;
        if (c.a != c.b) counts[c.b]++;
    }
    result.edges = constraints.size();
    std::vector<uint8_t> free(n);
    for (size_t i = 0; i < n; i++) free[i] = !fixed.count(ids[i]) && counts[i] >= opt.min_edges;
    std::vector<double> x(n, 0), rhs(n), diag(n), step(n), r(n), z(n), p(n), Ap(n), trial(n);
    struct Term { size_t a, b; double aa, ab, bb; };
    std::vector<Term> terms;
    const double c2 = opt.cauchy_scale * opt.cauchy_scale;
    auto objective = [&](const std::vector<double>& state) {
        double cost = 0;
        for (const Constraint& c : constraints) {
            Linearization l;
            if (!linearize(c, state[c.a], state[c.b], l))
                return std::numeric_limits<double>::infinity();
            const double sq = l.residual[0] * l.residual[0] + l.residual[1] * l.residual[1];
            cost += 0.5 * c2 * std::log1p(sq / c2);
        }
        return cost;
    };
    double cost = objective(x), damping = 1e-3;
    result.initial_cost = cost;
    auto dot = [&](const std::vector<double>& a, const std::vector<double>& b) {
        double v = 0;
        for (size_t i = 0; i < n; i++) v += a[i] * b[i];
        return v;
    };
    const double bound = std::log(opt.max_ratio);
    for (int iter = 0; iter < opt.max_iterations && std::isfinite(cost); iter++) {
        cancel::check();
        std::fill(rhs.begin(), rhs.end(), 0);
        std::fill(diag.begin(), diag.end(), 0);
        terms.clear();
        for (const Constraint& c : constraints) {
            Linearization l;
            if (!linearize(c, x[c.a], x[c.b], l)) continue;
            const double sq = l.residual[0] * l.residual[0] + l.residual[1] * l.residual[1];
            const double w = 1 / (1 + sq / c2);
            Term t{c.a, c.b, 0, 0, 0};
            for (int k = 0; k < 2; k++) {
                const double ja = free[c.a] ? l.ja[k] : 0;
                const double jb = free[c.b] ? l.jb[k] : 0;
                rhs[c.a] -= w * ja * l.residual[k];
                rhs[c.b] -= w * jb * l.residual[k];
                t.aa += w * ja * ja; t.ab += w * ja * jb; t.bb += w * jb * jb;
            }
            diag[c.a] += t.aa; diag[c.b] += t.bb;
            if (c.a == c.b) diag[c.a] += 2 * t.ab;
            terms.push_back(t);
        }
        std::fill(step.begin(), step.end(), 0);
        for (size_t i = 0; i < n; i++) {
            r[i] = rhs[i];
            z[i] = free[i] ? r[i] / (diag[i] + damping * std::max(diag[i], 1e-12)) : 0;
            p[i] = z[i];
        }
        double rz = dot(r, z), initial = dot(r, r);
        if (!(rz > 0) || !std::isfinite(rz)) break;
        for (int cg = 0; cg < opt.cg_iterations; cg++) {
            std::fill(Ap.begin(), Ap.end(), 0);
            for (const Term& t : terms) {
                Ap[t.a] += t.aa * p[t.a] + t.ab * p[t.b];
                Ap[t.b] += t.ab * p[t.a] + t.bb * p[t.b];
            }
            for (size_t i = 0; i < n; i++) Ap[i] += damping * std::max(diag[i], 1e-12) * p[i];
            const double denominator = dot(p, Ap);
            if (!(denominator > 0) || !std::isfinite(denominator)) break;
            const double alpha = rz / denominator;
            for (size_t i = 0; i < n; i++) { step[i] += alpha * p[i]; r[i] -= alpha * Ap[i]; }
            if (dot(r, r) < 1e-12 * initial) break;
            for (size_t i = 0; i < n; i++)
                z[i] = free[i] ? r[i] / (diag[i] + damping * std::max(diag[i], 1e-12)) : 0;
            const double next = dot(r, z);
            if (!(next > 0) || !std::isfinite(next)) break;
            const double beta = next / rz;
            for (size_t i = 0; i < n; i++) p[i] = z[i] + beta * p[i];
            rz = next;
        }
        for (size_t i = 0; i < n; i++) trial[i] = std::clamp(x[i] + step[i], -bound, bound);
        const double next = objective(trial);
        if (std::isfinite(next) && next < cost) {
            const double change = (cost - next) / std::max(cost, 1e-30);
            x.swap(trial);
            cost = next;
            damping = std::max(1e-12, damping * 0.5);
            if (change < opt.tolerance) break;
        } else {
            damping = std::min(1e12, damping * 4);
        }
    }
    result.final_cost = cost;
    std::vector<size_t> support(n, 0);
    std::vector<double> information(n, 0);
    for (const Constraint& c : constraints) {
        Linearization l;
        if (!linearize(c, x[c.a], x[c.b], l)) continue;
        const double sq = l.residual[0] * l.residual[0] + l.residual[1] * l.residual[1];
        if (sq > 4) continue;
        const double w = 1 / (1 + sq / c2);
        support[c.a]++;
        if (c.a != c.b) support[c.b]++;
        for (int k = 0; k < 2; k++) {
            if (c.a == c.b) {
                const double j = l.ja[k] + l.jb[k];
                information[c.a] += w * j * j;
            } else {
                information[c.a] += w * l.ja[k] * l.ja[k];
                information[c.b] += w * l.jb[k] * l.jb[k];
            }
        }
    }
    for (size_t i = 0; i < n; i++) {
        if (!free[i] || support[i] < opt.min_edges || information[i] <= 1e-12 ||
            !std::isfinite(x[i]) || std::abs(x[i]) >= bound - 1e-6) continue;
        Camera& camera = result.cameras[ids[i]];
        const double ratio = std::exp(x[i]);
        camera.fx *= ratio; camera.fy *= ratio;
        result.calibrated.insert(ids[i]);
    }
    return result;
}

inline FocalGraphResult calibrateViewGraph(
    const MatchesDatabase& db, const std::vector<FeatureSet>& features,
    const std::vector<uint32_t>& camera_ids, const std::map<uint32_t, Camera>& cameras,
    const std::set<uint32_t>& fixed, const TwoViewOptions& geometry, int threads,
    const FocalGraphOptions& options = {}) {
    if (features.size() != db.images.size() || camera_ids.size() != db.images.size())
        throw std::invalid_argument("view graph calibration: input counts differ");
    bool has_unknown = false;
    for (const auto& item : cameras)
        has_unknown = has_unknown || (!fixed.count(item.first) && !item.second.isFisheye() &&
                                     !item.second.isSpherical());
    if (!has_unknown) return calibrateFocalGraph(cameras, fixed, {}, options);
    std::vector<FocalGraphEdge> estimated(db.pairs.size());
    std::vector<uint8_t> valid(db.pairs.size(), 0);
    std::atomic<size_t> next{0};
    const unsigned hc = std::thread::hardware_concurrency();
    const size_t requested = threads > 0 ? (size_t)threads : (hc ? hc : 1);
    const size_t count = std::max<size_t>(1, std::min(requested, db.pairs.size()));
    auto worker = [&] {
        for (size_t i = next.fetch_add(1); i < db.pairs.size() && !cancel::requested();
             i = next.fetch_add(1)) {
            const TwoViewMatches& pair = db.pairs[i];
            if (pair.config != (int)TwoViewConfig::Uncalibrated ||
                pair.image1 >= features.size() || pair.image2 >= features.size() ||
                pair.image1 == pair.image2 || pair.matches.size() > (size_t)std::numeric_limits<int>::max() ||
                pair.matches.size() < (size_t)std::max(30, geometry.min_num_inliers)) continue;
            const uint32_t ca = camera_ids[pair.image1], cb = camera_ids[pair.image2];
            const auto a = cameras.find(ca), b = cameras.find(cb);
            if (a == cameras.end() || b == cameras.end() ||
                (fixed.count(ca) && fixed.count(cb)) || a->second.isFisheye() ||
                b->second.isFisheye() || a->second.isSpherical() || b->second.isSpherical()) continue;
            std::vector<Vec2> p1, p2;
            p1.reserve(pair.matches.size()); p2.reserve(pair.matches.size());
            for (const FeatureMatch& match : pair.matches) {
                if (match.idx1 >= features[pair.image1].keypoints.size() ||
                    match.idx2 >= features[pair.image2].keypoints.size()) break;
                const Keypoint& x = features[pair.image1].keypoints[match.idx1];
                const Keypoint& y = features[pair.image2].keypoints[match.idx2];
                p1.push_back({x.x, x.y}); p2.push_back({y.x, y.y});
            }
            if (p1.size() != pair.matches.size()) continue;
            TwoViewOptions opt = geometry;
            opt.estimate_homography = false;
            opt.recover_pose = false;
            opt.ransac.max_error *= 0.5 * (features[pair.image1].pixelScale() +
                                           features[pair.image2].pixelScale());
            const TwoViewGeometry g = estimateTwoView(p1, p2, opt);
            if (g.config != TwoViewConfig::Uncalibrated ||
                g.num_inliers < std::max(30, geometry.min_num_inliers)) continue;
            estimated[i] = {ca, cb, g.F};
            valid[i] = 1;
        }
    };
    std::vector<std::thread> pool;
    for (size_t i = 1; i < count; i++) pool.emplace_back(worker);
    worker();
    for (std::thread& thread : pool) thread.join();
    cancel::check();
    std::vector<FocalGraphEdge> edges;
    for (size_t i = 0; i < estimated.size(); i++) if (valid[i]) edges.push_back(estimated[i]);
    return calibrateFocalGraph(cameras, fixed, edges, options);
}

}  // namespace sfm
