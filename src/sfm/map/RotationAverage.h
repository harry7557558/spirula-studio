// Robust rotation averaging on relative-view edges, with one gauge per component.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <vector>

#include "sfm/core/Pose.h"
#include "sfm/core/Rig.h"
#include "sfm/map/RelativeViewGraph.h"

namespace sfm {

struct RotationAverageOptions {
    int max_iterations = 500;
    int l1_iterations = 5;
    int irls_iterations = 100;
    int l1_admm_initial_iterations = 10;
    int l1_admm_max_iterations = 100;
    int cg_iterations = 1000;
    double l1_admm_rho = 1.0;
    double l1_admm_alpha = 1.0;
    double l1_admm_absolute_tolerance = 1e-4;
    double l1_admm_relative_tolerance = 1e-2;
    double l1_step_tolerance = 1e-3;
    double irls_sigma_radians = 0.08726646259971647;
    double irls_step_tolerance = 1e-3;
    double ridge_regularization = 1e-9;
    double max_error_radians = 0.20;
    double cg_tolerance = 1e-6;
};

struct RotationAverageResult {
    std::vector<Mat3> rotations;
    // Isolated images have no component; all others use their smallest image index.
    std::vector<uint32_t> component;
    std::vector<uint8_t> accepted;
    bool converged = false;
};

struct FixedRigFrames {
    std::vector<uint32_t> frame;
    std::vector<Pose> sensor_from_rig;
};

inline FixedRigFrames fixedRigFrames(size_t n, const RigTable* rigs) {
    if (n >= UINT32_MAX) throw std::invalid_argument("rotation averaging: too many images");
    FixedRigFrames out;
    out.frame.resize(n);
    std::iota(out.frame.begin(), out.frame.end(), 0);
    out.sensor_from_rig.assign(n, Pose{mat3Identity(), {0, 0, 0}});
    if (!rigs) return out;
    std::map<std::pair<uint32_t, uint32_t>, uint32_t> frame_id;
    for (uint32_t image = 0; image < n; image++) {
        const RigSlot slot = rigs->slot(image);
        if (!slot.valid()) continue;
        if (slot.rig >= rigs->rigs.size() || slot.member >= rigs->rigs[slot.rig].members.size() ||
            slot.frame >= rigs->rigs[slot.rig].frames.size())
            throw std::invalid_argument("rotation averaging: invalid rig slot");
        const RigMemberDef& member = rigs->rigs[slot.rig].members[slot.member];
        if (!member.has_ext || !member.ext_fixed) continue;
        for (double v : member.ext.R)
            if (!std::isfinite(v)) throw std::invalid_argument("rotation averaging: invalid rig rotation");
        if (!std::isfinite(member.ext.t.x) || !std::isfinite(member.ext.t.y) ||
            !std::isfinite(member.ext.t.z))
            throw std::invalid_argument("rotation averaging: invalid rig translation");
        out.frame[image] = frame_id.emplace(std::make_pair(slot.rig, slot.frame), image).first->second;
        out.sensor_from_rig[image] = member.ext;
    }
    return out;
}

inline RotationAverageResult averageViewRotations(const MatchesDatabase& db,
                                                   const std::vector<RelativeViewEdge>& views,
                                                   const RotationAverageOptions& opt = {}) {
    if (views.size() != db.pairs.size() || db.images.size() >= UINT32_MAX ||
        opt.max_iterations < 1 || opt.l1_iterations < 0 || opt.irls_iterations < 0 ||
        opt.l1_iterations + opt.irls_iterations < 1 || opt.l1_admm_initial_iterations < 1 ||
        opt.l1_admm_max_iterations < opt.l1_admm_initial_iterations || opt.cg_iterations < 1 ||
        !(opt.l1_admm_rho > 0) || !(opt.l1_admm_alpha > 0) ||
        !(opt.l1_admm_alpha < 2) || !(opt.l1_admm_absolute_tolerance >= 0) ||
        !(opt.l1_admm_relative_tolerance >= 0) || !(opt.l1_step_tolerance > 0) ||
        !(opt.irls_sigma_radians > 0) || !(opt.irls_step_tolerance > 0) ||
        !(opt.ridge_regularization >= 0) || !(opt.max_error_radians > 0) ||
        !(opt.cg_tolerance > 0) || !std::isfinite(opt.l1_admm_rho) ||
        !std::isfinite(opt.l1_admm_alpha) || !std::isfinite(opt.l1_admm_absolute_tolerance) ||
        !std::isfinite(opt.l1_admm_relative_tolerance) || !std::isfinite(opt.l1_step_tolerance) ||
        !std::isfinite(opt.irls_sigma_radians) || !std::isfinite(opt.irls_step_tolerance) ||
        !std::isfinite(opt.ridge_regularization) ||
        !std::isfinite(opt.max_error_radians) || !std::isfinite(opt.cg_tolerance))
        throw std::invalid_argument("rotation averaging: invalid inputs");
    const uint32_t n = (uint32_t)db.images.size();
    RotationAverageResult out;
    out.rotations.assign(n, mat3Identity());
    out.component.assign(n, UINT32_MAX);
    out.accepted.assign(views.size(), 0);
    std::vector<size_t> order;
    for (size_t i = 0; i < views.size(); i++) {
        if (views[i].status != RelativeViewStatus::Accepted) continue;
        const auto& p = db.pairs[i];
        if (p.image1 >= n || p.image2 >= n || p.image1 == p.image2)
            throw std::invalid_argument("rotation averaging: invalid accepted pair");
        for (double v : views[i].pose.R)
            if (!std::isfinite(v))
                throw std::invalid_argument("rotation averaging: non-finite rotation");
        out.accepted[i] = 1;
        order.push_back(i);
    }
    struct Adjacent { uint32_t other; size_t edge; };
    std::vector<std::vector<Adjacent>> adjacency(n);
    for (size_t e : order) {
        const auto& p = db.pairs[e];
        adjacency[p.image1].push_back({p.image2, e});
        adjacency[p.image2].push_back({p.image1, e});
    }
    for (auto& list : adjacency)
        std::sort(list.begin(), list.end(), [](const Adjacent& a, const Adjacent& b) {
            return a.other != b.other ? a.other < b.other : a.edge < b.edge;
        });
    auto fromTo = [&](uint32_t from, size_t e) {
        return db.pairs[e].image1 == from ? views[e].pose.R : transpose(views[e].pose.R);
    };
    std::vector<double> consistency(views.size(), 0.5);
    for (size_t e : order) {
        const auto& p = db.pairs[e];
        const auto& a = adjacency[p.image1];
        const auto& b = adjacency[p.image2];
        size_t ia = 0, ib = 0;
        int checked = 0, agreed = 0;
        while (ia < a.size() && ib < b.size() && checked < 32) {
            if (a[ia].other < b[ib].other) { ia++; continue; }
            if (b[ib].other < a[ia].other) { ib++; continue; }
            const uint32_t common = a[ia].other;
            const Mat3 implied = mul(transpose(fromTo(p.image2, b[ib].edge)),
                                     fromTo(p.image1, a[ia].edge));
            const double error = rotationToAngleAxis(mul(implied, transpose(views[e].pose.R))).norm();
            agreed += error <= opt.max_error_radians;
            checked++;
            while (ia < a.size() && a[ia].other == common) ia++;
            while (ib < b.size() && b[ib].other == common) ib++;
        }
        if (checked) consistency[e] = (double)agreed / checked;
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return consistency[a] != consistency[b] ? consistency[a] > consistency[b]
                                               : views[a].inliers > views[b].inliers;
    });
    auto solve = [&]() {
        std::vector<uint32_t> parent(n);
        std::iota(parent.begin(), parent.end(), 0);
        auto root = [&](uint32_t x) {
            while (parent[x] != x) {
                parent[x] = parent[parent[x]];
                x = parent[x];
            }
            return x;
        };
        std::vector<std::vector<size_t>> tree(n);
        for (size_t e : order) {
            if (!out.accepted[e]) continue;
            const auto& p = db.pairs[e];
            uint32_t a = root(p.image1), b = root(p.image2);
            if (a == b) continue;
            parent[b] = a;
            tree[p.image1].push_back(e);
            tree[p.image2].push_back(e);
        }
        std::fill(out.component.begin(), out.component.end(), UINT32_MAX);
        std::fill(out.rotations.begin(), out.rotations.end(), mat3Identity());
        for (uint32_t start = 0; start < n; start++) {
            if (tree[start].empty() || out.component[start] != UINT32_MAX) continue;
            std::queue<uint32_t> q;
            out.component[start] = start;
            q.push(start);
            while (!q.empty()) {
                const uint32_t u = q.front();
                q.pop();
                for (size_t e : tree[u]) {
                    const auto& p = db.pairs[e];
                    const bool forward = p.image1 == u;
                    const uint32_t v = forward ? p.image2 : p.image1;
                    if (out.component[v] != UINT32_MAX) continue;
                    out.rotations[v] = mul(forward ? views[e].pose.R : transpose(views[e].pose.R),
                                           out.rotations[u]);
                    out.component[v] = start;
                    q.push(v);
                }
            }
        }
        std::vector<Vec3> rhs(n), step(n), residual(n), preconditioned(n), direction(n), product(n);
        std::vector<double> diagonal(n), unit_weight(views.size(), 1.0);
        std::vector<Mat3> candidate(n);
        auto anchored = [&](uint32_t i) {
            return out.component[i] == UINT32_MAX || out.component[i] == i;
        };
        auto dot = [&](const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
            double sum = 0;
            for (uint32_t i = 0; i < n; i++) sum += a[i].dot(b[i]);
            return sum;
        };
        enum class Stage { L1, GemanMcClure };
        auto objective = [&](const std::vector<Mat3>& rotations, Stage stage) {
            double value = 0;
            for (size_t e : order) {
                if (!out.accepted[e]) continue;
                const auto& pair = db.pairs[e];
                const Vec3 r = rotationToAngleAxis(mul(mul(transpose(rotations[pair.image2]),
                                                           views[e].pose.R), rotations[pair.image1]));
                const double squared = r.dot(r);
                if (stage == Stage::L1)
                    value += std::abs(r.x) + std::abs(r.y) + std::abs(r.z);
                else
                    value += squared / (squared + opt.irls_sigma_radians * opt.irls_sigma_radians);
            }
            return value;
        };
        auto solveNormal = [&](const std::vector<Vec3>& target, const std::vector<double>& weights) {
            std::fill(rhs.begin(), rhs.end(), Vec3{});
            std::fill(diagonal.begin(), diagonal.end(), 0.0);
            for (size_t e : order) {
                if (!out.accepted[e]) continue;
                const auto& p = db.pairs[e];
                const double w = weights[e];
                if (!(w > 0) || !std::isfinite(w)) return false;
                rhs[p.image1] = rhs[p.image1] - target[e] * w;
                rhs[p.image2] = rhs[p.image2] + target[e] * w;
                diagonal[p.image1] += w;
                diagonal[p.image2] += w;
            }
            std::fill(step.begin(), step.end(), Vec3{});
            for (uint32_t i = 0; i < n; i++) {
                if (anchored(i)) {
                    residual[i] = preconditioned[i] = direction[i] = {};
                    continue;
                }
                const double d = diagonal[i] + opt.ridge_regularization;
                if (!(d > 0) || !std::isfinite(d)) return false;
                residual[i] = rhs[i];
                preconditioned[i] = residual[i] * (1.0 / d);
                direction[i] = preconditioned[i];
            }
            double rz = dot(residual, preconditioned);
            const double initial = dot(residual, residual);
            if (initial == 0) return true;
            if (!(rz > 0) || !std::isfinite(rz) || !std::isfinite(initial)) return false;
            for (int cg = 0; cg < opt.cg_iterations; cg++) {
                if (cg % 32 == 0) cancel::check();
                std::fill(product.begin(), product.end(), Vec3{});
                for (size_t e : order) {
                    if (!out.accepted[e]) continue;
                    const auto& p = db.pairs[e];
                    const Vec3 v = (direction[p.image2] - direction[p.image1]) * weights[e];
                    product[p.image1] = product[p.image1] - v;
                    product[p.image2] = product[p.image2] + v;
                }
                for (uint32_t i = 0; i < n; i++) {
                    if (anchored(i)) product[i] = {};
                    else product[i] = product[i] + direction[i] * opt.ridge_regularization;
                }
                const double denom = dot(direction, product);
                if (!(denom > 0) || !std::isfinite(denom)) return false;
                const double alpha = rz / denom;
                for (uint32_t i = 0; i < n; i++) {
                    step[i] = step[i] + direction[i] * alpha;
                    residual[i] = residual[i] - product[i] * alpha;
                }
                if (dot(residual, residual) <= opt.cg_tolerance * opt.cg_tolerance * initial)
                    return true;
                for (uint32_t i = 0; i < n; i++) {
                    if (anchored(i)) {
                        preconditioned[i] = {};
                        continue;
                    }
                    const double d = diagonal[i] + opt.ridge_regularization;
                    preconditioned[i] = residual[i] * (1.0 / d);
                }
                const double next = dot(residual, preconditioned);
                if (!(next > 0) || !std::isfinite(next)) return false;
                const double beta = next / rz;
                for (uint32_t i = 0; i < n; i++)
                    direction[i] = preconditioned[i] + direction[i] * beta;
                rz = next;
            }
            return std::isfinite(dot(residual, residual));
        };
        auto acceptStep = [&](Stage stage, bool& converged) {
            double sum = 0;
            size_t count = 0;
            for (uint32_t i = 0; i < n; i++)
                if (!anchored(i)) {
                    sum += step[i].norm();
                    count++;
                }
            const double mean = count ? sum / count : 0;
            const double tolerance = stage == Stage::L1 ? opt.l1_step_tolerance
                                                         : opt.irls_step_tolerance;
            if (mean < tolerance) {
                converged = true;
                return true;
            }
            const double before = objective(out.rotations, stage);
            for (double scale = 1; scale >= 1.0 / 1024; scale *= 0.5) {
                for (uint32_t i = 0; i < n; i++)
                    candidate[i] = mul(out.rotations[i], angleAxisToRotation(step[i] * scale));
                if (objective(candidate, stage) <= before + 1e-15 * std::max(1.0, before)) {
                    out.rotations.swap(candidate);
                    converged = mean * scale < tolerance;
                    return true;
                }
            }
            converged = true;
            return true;
        };
        auto solveL1 = [&]() {
            std::vector<Vec3> target(views.size()), z(views.size()), z_old(views.size()),
                dual(views.size());
            int admm_iterations = opt.l1_admm_initial_iterations;
            const int iterations = std::min(opt.max_iterations, opt.l1_iterations);
            for (int iter = 0; iter < iterations; iter++) {
                cancel::check();
                std::fill(z.begin(), z.end(), Vec3{});
                std::fill(dual.begin(), dual.end(), Vec3{});
                std::vector<Vec3> errors(views.size());
                size_t constraints = 0;
                for (size_t e : order) {
                    if (!out.accepted[e]) continue;
                    const auto& p = db.pairs[e];
                    errors[e] = rotationToAngleAxis(mul(mul(transpose(out.rotations[p.image2]),
                                                               views[e].pose.R), out.rotations[p.image1]));
                    constraints++;
                }
                bool converged = false;
                for (int admm = 0; admm < admm_iterations; admm++) {
                    if (admm % 8 == 0) cancel::check();
                    for (size_t e : order)
                        if (out.accepted[e]) target[e] = errors[e] + z[e] - dual[e];
                    if (!solveNormal(target, unit_weight)) return false;
                    std::swap(z, z_old);
                    double error_norm = 0, ax_norm = 0, z_norm = 0, primal_squared = 0;
                    std::fill(residual.begin(), residual.end(), Vec3{});
                    std::fill(product.begin(), product.end(), Vec3{});
                    for (size_t e : order) {
                        if (!out.accepted[e]) continue;
                        const auto& p = db.pairs[e];
                        const Vec3 ax = step[p.image2] - step[p.image1];
                        const Vec3 ax_hat = ax * opt.l1_admm_alpha +
                            (z_old[e] + errors[e]) * (1.0 - opt.l1_admm_alpha);
                        auto shrink = [&](double v) {
                            return std::copysign(std::max(0.0, std::abs(v) - 1.0 / opt.l1_admm_rho), v);
                        };
                        z[e] = {shrink(ax_hat.x - errors[e].x + dual[e].x),
                                shrink(ax_hat.y - errors[e].y + dual[e].y),
                                shrink(ax_hat.z - errors[e].z + dual[e].z)};
                        dual[e] = dual[e] + ax_hat - z[e] - errors[e];
                        const Vec3 primal = ax - z[e] - errors[e];
                        const Vec3 dual_step = (z[e] - z_old[e]) * opt.l1_admm_rho;
                        const Vec3 dual_value = dual[e] * opt.l1_admm_rho;
                        residual[p.image1] = residual[p.image1] - dual_step;
                        residual[p.image2] = residual[p.image2] + dual_step;
                        product[p.image1] = product[p.image1] - dual_value;
                        product[p.image2] = product[p.image2] + dual_value;
                        error_norm += errors[e].dot(errors[e]);
                        ax_norm += ax.dot(ax);
                        z_norm += z[e].dot(z[e]);
                        primal_squared += primal.dot(primal);
                    }
                    const double primal_norm = std::sqrt(primal_squared);
                    const double dual_norm = std::sqrt(dot(residual, residual));
                    const double eps_primal = std::sqrt(3.0 * constraints) * opt.l1_admm_absolute_tolerance +
                        opt.l1_admm_relative_tolerance * std::max({std::sqrt(error_norm), std::sqrt(ax_norm), std::sqrt(z_norm)});
                    const double eps_dual = std::sqrt(3.0 * n) * opt.l1_admm_absolute_tolerance +
                        opt.l1_admm_relative_tolerance * std::sqrt(dot(product, product));
                    if (!std::isfinite(primal_norm) || !std::isfinite(dual_norm) ||
                        !std::isfinite(eps_primal) || !std::isfinite(eps_dual)) return false;
                    if (primal_norm <= eps_primal && dual_norm <= eps_dual) {
                        converged = true;
                        break;
                    }
                }
                if (!converged && admm_iterations < opt.l1_admm_max_iterations)
                    admm_iterations = std::min(2 * admm_iterations, opt.l1_admm_max_iterations);
                bool done = false;
                if (!acceptStep(Stage::L1, done)) return false;
                if (done) return true;
            }
            return true;
        };
        auto solveIrls = [&]() {
            std::vector<Vec3> target(views.size());
            std::vector<double> weights(views.size());
            const int iterations = std::min(opt.max_iterations, opt.irls_iterations);
            for (int iter = 0; iter < iterations; iter++) {
                cancel::check();
                for (size_t e : order) {
                    if (!out.accepted[e]) continue;
                    const auto& p = db.pairs[e];
                    target[e] = rotationToAngleAxis(mul(mul(transpose(out.rotations[p.image2]),
                                                               views[e].pose.R), out.rotations[p.image1]));
                    const double squared = target[e].dot(target[e]);
                    const double denom = squared + opt.irls_sigma_radians * opt.irls_sigma_radians;
                    weights[e] = opt.irls_sigma_radians * opt.irls_sigma_radians / (denom * denom);
                }
                bool done = false;
                if (!solveNormal(target, weights) || !acceptStep(Stage::GemanMcClure, done)) return false;
                if (done) return true;
            }
            return true;
        };
        const bool l1_converged = solveL1();
        const bool irls_converged = solveIrls();
        return l1_converged && irls_converged;
    };
    const bool first_converged = solve();
    bool removed = false;
    for (size_t e : order) {
        const auto& p = db.pairs[e];
        const Mat3 predicted = mul(out.rotations[p.image2], transpose(out.rotations[p.image1]));
        const double error = rotationToAngleAxis(mul(predicted, transpose(views[e].pose.R))).norm();
        if (error > opt.max_error_radians) {
            out.accepted[e] = 0;
            removed = true;
        }
    }
    out.converged = removed ? solve() : first_converged;
    return out;
}

inline RotationAverageResult averageRigViewRotations(const MatchesDatabase& db,
                                                      const std::vector<RelativeViewEdge>& views,
                                                      const RigTable* rigs,
                                                      const RotationAverageOptions& opt = {}) {
    if (!rigs || rigs->empty()) return averageViewRotations(db, views, opt);
    if (views.size() != db.pairs.size() || db.images.size() >= UINT32_MAX)
        throw std::invalid_argument("rotation averaging: invalid inputs");
    const size_t n = db.images.size();
    const FixedRigFrames layout = fixedRigFrames(n, rigs);
    const auto& frame = layout.frame;
    MatchesDatabase frames;
    frames.images.resize(n);
    frames.pairs.resize(db.pairs.size());
    std::vector<RelativeViewEdge> frame_views(views.size());
    for (size_t i = 0; i < views.size(); i++) {
        const auto& pair = db.pairs[i];
        if (views[i].status != RelativeViewStatus::Accepted) continue;
        if (pair.image1 >= n || pair.image2 >= n || pair.image1 == pair.image2)
            throw std::invalid_argument("rotation averaging: invalid accepted pair");
        const uint32_t a = pair.image1, b = pair.image2;
        frames.pairs[i].image1 = frame[a];
        frames.pairs[i].image2 = frame[b];
        if (frame[a] == frame[b]) continue;
        frame_views[i].status = RelativeViewStatus::Accepted;
        frame_views[i].inliers = views[i].inliers;
        frame_views[i].pose.R = mul(mul(transpose(layout.sensor_from_rig[b].R), views[i].pose.R),
                                   layout.sensor_from_rig[a].R);
    }
    RotationAverageResult out = averageViewRotations(frames, frame_views, opt);
    const auto frame_rotations = out.rotations;
    const auto frame_components = out.component;
    for (uint32_t image = 0; image < n; image++) {
        out.rotations[image] = mul(layout.sensor_from_rig[image].R, frame_rotations[frame[image]]);
        out.component[image] = frame_components[frame[image]];
    }
    for (size_t i = 0; i < views.size(); i++) {
        if (views[i].status != RelativeViewStatus::Accepted) continue;
        const uint32_t a = db.pairs[i].image1, b = db.pairs[i].image2;
        if (frame[a] != frame[b] || out.component[a] == UINT32_MAX) continue;
        const Mat3 expected = mul(layout.sensor_from_rig[b].R, transpose(layout.sensor_from_rig[a].R));
        out.accepted[i] = rotationToAngleAxis(mul(expected, transpose(views[i].pose.R))).norm()
                          <= opt.max_error_radians;
    }
    return out;
}

}  // namespace sfm
