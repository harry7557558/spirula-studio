// CPU reference solver for global camera and point positioning.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include "sfm/ba/CpuCamera.h"
#include "sfm/core/Cancel.h"
#include "sfm/map/GlobalPositionCost.h"

namespace sfm {

struct GlobalPositionObservation {
    uint32_t camera = 0, point = 0;
    Vec3 ray;
    double weight = 1;
    Vec3 camera_offset;
};

struct GlobalPositionPrior {
    uint32_t camera = 0;
    Vec3 camera_offset;
    Mat3 A = mat3Identity();
    Vec3 b;
    Vec3 sigma{1, 1, 1};
    double cauchy = 0;
};

struct GlobalPositionProblem {
    std::vector<Vec3> cameras, points;
    std::vector<GlobalPositionObservation> observations;
    std::vector<GlobalPositionPrior> position_priors;
    std::vector<double> log_scales;
    bool metric_scale = false;
};

struct GlobalPositionOptions {
    int max_iterations = 150, cg_iterations = 200;
    double huber = 0.1, initial_damping = 1e-3;
    double cg_tolerance = 1e-4, relative_tolerance = 1e-8;
    uint32_t seed = 1;
};

struct GlobalPositionResult {
    double initial_cost = 0, final_cost = 0;
    int iterations = 0, accepted = 0;
    bool converged = false;
};

inline Vec3 globalPositionPriorResidual(const GlobalPositionPrior& p, const Vec3& camera) {
    const Vec3 r = mul(p.A, camera + p.camera_offset) - p.b;
    return {p.sigma.x > 0 ? r.x / p.sigma.x : 0,
            p.sigma.y > 0 ? r.y / p.sigma.y : 0,
            p.sigma.z > 0 ? r.z / p.sigma.z : 0};
}

inline Mat3 globalPositionPriorJacobian(const GlobalPositionPrior& p) {
    Mat3 out = p.A;
    const double inv[3] = {p.sigma.x > 0 ? 1.0 / p.sigma.x : 0,
                           p.sigma.y > 0 ? 1.0 / p.sigma.y : 0,
                           p.sigma.z > 0 ? 1.0 / p.sigma.z : 0};
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) out[3 * r + c] *= inv[r];
    return out;
}

inline double globalPositionPriorCost(double squared, double cauchy, double huber) {
    if (cauchy > 0) return cauchy * cauchy * std::log1p(squared / (cauchy * cauchy));
    return bacpu::HuberLoss::cost(squared, huber);
}

inline double globalPositionPriorWeight(double squared, double cauchy, double huber) {
    if (cauchy > 0) return 1.0 / (1.0 + squared / (cauchy * cauchy));
    return bacpu::HuberLoss::weight(squared, huber);
}

inline void initializeGlobalPosition(GlobalPositionProblem& p, uint32_t seed = 1) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(-1, 1);
    auto random = [&] { return Vec3{dist(rng), dist(rng), dist(rng)}; };
    for (Vec3& c : p.cameras) c = random();
    for (Vec3& x : p.points) x = random();
    if (!p.cameras.empty()) {
        const Vec3 origin = p.cameras.front();
        for (Vec3& c : p.cameras) c = c - origin;
        for (Vec3& x : p.points) x = x - origin;
    }
    p.log_scales.assign(p.observations.size(), 0);
}

inline double globalPositionCost(const GlobalPositionProblem& p, double huber) {
    double cost = 0;
    for (size_t i = 0; i < p.observations.size(); i++) {
        const auto& o = p.observations[i];
        const Vec3 r = globalPositionLinearize(o.ray, p.points[o.point],
                                              p.cameras[o.camera] + o.camera_offset,
                                              std::exp(p.log_scales[i])).residual;
        cost += 0.5 * o.weight * bacpu::HuberLoss::cost(r.dot(r), huber);
    }
    for (const GlobalPositionPrior& q : p.position_priors) {
        const Vec3 r = globalPositionPriorResidual(q, p.cameras[q.camera]);
        cost += 0.5 * globalPositionPriorCost(r.dot(r), q.cauchy, huber);
    }
    return cost;
}

inline void validateGlobalPosition(const GlobalPositionProblem& p,
                                   const GlobalPositionOptions& opt) {
    const size_t nc = p.cameras.size(), np = p.points.size(), no = p.observations.size();
    if (nc == 0 || (nc < 2 && !p.metric_scale) || np == 0 || no == 0 || p.log_scales.size() != no ||
        opt.max_iterations < 1 || opt.cg_iterations < 1 || !(opt.huber > 0) ||
        !(opt.initial_damping > 0) || !(opt.cg_tolerance > 0) ||
        !(opt.relative_tolerance > 0) || !std::isfinite(opt.huber) ||
        !std::isfinite(opt.initial_damping) || !std::isfinite(opt.cg_tolerance) ||
        !std::isfinite(opt.relative_tolerance))
        throw std::invalid_argument("global positioning: invalid problem or options");
    auto finite = [](const Vec3& v) {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    };
    for (const Vec3& v : p.cameras)
        if (!finite(v)) throw std::invalid_argument("global positioning: invalid camera position");
    for (const Vec3& v : p.points)
        if (!finite(v)) throw std::invalid_argument("global positioning: invalid point position");
    std::vector<size_t> parent(nc + np);
    std::iota(parent.begin(), parent.end(), 0);
    std::vector<Vec3> first_offset(nc);
    std::vector<uint8_t> has_offset(nc, 0);
    bool baseline = false;
    std::set<uint32_t> metric_prior_cameras;
    auto root = [&](size_t x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    for (size_t i = 0; i < no; i++) {
        const auto& o = p.observations[i];
        if (o.camera >= nc || o.point >= np || !(o.weight > 0) ||
            !std::isfinite(o.weight) || !std::isfinite(o.ray.x) || !std::isfinite(o.ray.y) ||
            !std::isfinite(o.ray.z) || !finite(o.camera_offset) ||
            std::fabs(o.ray.norm() - 1) > 1e-6 ||
            !std::isfinite(p.log_scales[i]) || std::fabs(p.log_scales[i]) > 20)
            throw std::invalid_argument("global positioning: invalid observation");
        const size_t a = root(o.camera), b = root(nc + o.point);
        if (a != b) parent[b] = a;
        if (!has_offset[o.camera]) {
            first_offset[o.camera] = o.camera_offset;
            has_offset[o.camera] = 1;
        } else if ((first_offset[o.camera] - o.camera_offset).norm() > 1e-12) {
            baseline = true;
        }
    }
    for (const GlobalPositionPrior& q : p.position_priors) {
        bool finite_matrix = true;
        for (double value : q.A) finite_matrix = finite_matrix && std::isfinite(value);
        const bool active = q.sigma.x > 0 || q.sigma.y > 0 || q.sigma.z > 0;
        if (q.camera >= nc || !finite(q.camera_offset) || !finite(q.b) || !finite_matrix ||
            q.sigma.x < 0 || q.sigma.y < 0 || q.sigma.z < 0 || !active ||
            !std::isfinite(q.sigma.x) || !std::isfinite(q.sigma.y) ||
            !std::isfinite(q.sigma.z) || q.cauchy < 0 || !std::isfinite(q.cauchy))
            throw std::invalid_argument("global positioning: invalid position prior");
        metric_prior_cameras.insert(q.camera);
    }
    const size_t component = root(0);
    for (size_t i = 1; i < parent.size(); i++)
        if (root(i) != component)
            throw std::invalid_argument("global positioning: disconnected observations");
    if (p.metric_scale && !baseline && metric_prior_cameras.size() < 2)
        throw std::invalid_argument("global positioning: metric scale has no rig baseline");
    if (!p.metric_scale && !p.position_priors.empty())
        throw std::invalid_argument("global positioning: position priors need metric scale");
}

inline GlobalPositionResult solveGlobalPosition(GlobalPositionProblem& p,
                                                const GlobalPositionOptions& opt = {}) {
    validateGlobalPosition(p, opt);
    const size_t nc = p.cameras.size(), np = p.points.size(), no = p.observations.size();
    const bool free_camera_gauge = !p.position_priors.empty();
    const size_t po = 3 * (nc - (free_camera_gauge ? 0 : 1)), so = po + 3 * np, dim = so + no;
    const bool has_offsets = std::any_of(p.observations.begin(), p.observations.end(),
        [](const GlobalPositionObservation& o) { return o.camera_offset.norm() > 0; });
    auto get = [](const std::vector<double>& v, size_t i) { return Vec3{v[i], v[i + 1], v[i + 2]}; };
    auto add = [](std::vector<double>& v, size_t i, const Vec3& x) {
        v[i] += x.x; v[i + 1] += x.y; v[i + 2] += x.z;
    };
    auto camera_offset = [&](uint32_t camera) {
        return free_camera_gauge ? 3 * (size_t)camera : 3 * ((size_t)camera - 1);
    };
    auto dot = [](const std::vector<double>& a, const std::vector<double>& b) {
        double value = 0;
        for (size_t i = 0; i < a.size(); i++) value += a[i] * b[i];
        return value;
    };
    std::vector<double> rhs(dim), diag(dim), weight(no), scale(no), damping_diag(dim);
    std::vector<Vec3> delta(no);
    double cost = globalPositionCost(p, opt.huber);
    if (!std::isfinite(cost)) throw std::invalid_argument("global positioning: non-finite initial cost");
    GlobalPositionResult result;
    result.initial_cost = cost;
    double damping = opt.initial_damping;
    std::vector<Vec3> trial_cameras(nc), trial_points(np);
    std::vector<double> trial_scales(no);
    std::vector<double> step(dim), r(dim), z(dim), direction(dim), product(dim);
    auto trialCost = [&] {
        double value = 0;
        for (size_t i = 0; i < no; i++) {
            const auto& o = p.observations[i];
            const Vec3 r = o.ray - (trial_points[o.point] - trial_cameras[o.camera] - o.camera_offset) *
                                     std::exp(trial_scales[i]);
            value += 0.5 * o.weight * bacpu::HuberLoss::cost(r.dot(r), opt.huber);
        }
        for (const GlobalPositionPrior& q : p.position_priors) {
            const Vec3 r = globalPositionPriorResidual(q, trial_cameras[q.camera]);
            value += 0.5 * globalPositionPriorCost(r.dot(r), q.cauchy, opt.huber);
        }
        return value;
    };
    for (int iteration = 0; iteration < opt.max_iterations; iteration++) {
        cancel::check();
        std::fill(rhs.begin(), rhs.end(), 0);
        std::fill(diag.begin(), diag.end(), 0);
        for (size_t i = 0; i < no; i++) {
            const auto& o = p.observations[i];
            scale[i] = std::exp(p.log_scales[i]);
            delta[i] = p.points[o.point] - p.cameras[o.camera] - o.camera_offset;
            const Vec3 residual = o.ray - delta[i] * scale[i];
            weight[i] = o.weight * bacpu::HuberLoss::weight(residual.dot(residual), opt.huber);
            const double ws = weight[i] * scale[i];
            const double diagonal = ws * scale[i];
            const size_t pi = po + 3 * (size_t)o.point;
            add(rhs, pi, residual * ws);
            for (int k = 0; k < 3; k++) diag[pi + k] += diagonal;
            if (free_camera_gauge || o.camera) {
                const size_t ci = camera_offset(o.camera);
                add(rhs, ci, residual * -ws);
                for (int k = 0; k < 3; k++) diag[ci + k] += diagonal;
            }
            rhs[so + i] = ws * delta[i].dot(residual);
            diag[so + i] = diagonal * delta[i].dot(delta[i]);
        }
        for (const GlobalPositionPrior& q : p.position_priors) {
            const Mat3 J = globalPositionPriorJacobian(q);
            const Vec3 residual = globalPositionPriorResidual(q, p.cameras[q.camera]);
            const double weight = globalPositionPriorWeight(residual.dot(residual), q.cauchy, opt.huber);
            const size_t ci = camera_offset(q.camera);
            add(rhs, ci, mul(transpose(J), residual) * -weight);
            for (int c = 0; c < 3; c++) {
                double d = 0;
                for (int r = 0; r < 3; r++) d += J[3 * r + c] * J[3 * r + c];
                diag[ci + c] += weight * d;
            }
        }
        for (size_t i = 0; i < dim; i++) damping_diag[i] = std::max(diag[i], 1e-8);
        auto multiply = [&](const std::vector<double>& x, std::vector<double>& y) {
            for (size_t j = 0; j < dim; j++) y[j] = damping * damping_diag[j] * x[j];
            for (size_t i = 0; i < no; i++) {
                const auto& o = p.observations[i];
                const size_t pi = po + 3 * (size_t)o.point;
                Vec3 step = get(x, pi) * -1.0 - delta[i] * x[so + i];
                if (free_camera_gauge || o.camera) step = step + get(x, camera_offset(o.camera));
                const Vec3 jt = step * (weight[i] * scale[i] * scale[i]);
                add(y, pi, jt * -1.0);
                if (free_camera_gauge || o.camera) add(y, camera_offset(o.camera), jt);
                y[so + i] -= delta[i].dot(jt);
            }
            for (const GlobalPositionPrior& q : p.position_priors) {
                const Mat3 J = globalPositionPriorJacobian(q);
                const size_t ci = camera_offset(q.camera);
                const Vec3 residual = globalPositionPriorResidual(q, p.cameras[q.camera]);
                const double weight = globalPositionPriorWeight(residual.dot(residual), q.cauchy, opt.huber);
                add(y, ci, mul(transpose(J), mul(J, get(x, ci))) * weight);
            }
        };
        std::fill(step.begin(), step.end(), 0);
        r = rhs;
        for (size_t j = 0; j < dim; j++) z[j] = r[j] / (diag[j] + damping * damping_diag[j]);
        direction = z;
        double rz = dot(r, z), initial = std::sqrt(dot(r, r));
        if (!(initial > 0) || !std::isfinite(initial)) {
            result.converged = initial == 0;
            break;
        }
        for (int cg = 0; cg < opt.cg_iterations; cg++) {
            if (cg % 10 == 0) cancel::check();
            multiply(direction, product);
            const double denom = dot(direction, product);
            if (!(denom > 0) || !std::isfinite(denom)) break;
            const double alpha = rz / denom;
            for (size_t j = 0; j < dim; j++) {
                step[j] += alpha * direction[j];
                r[j] -= alpha * product[j];
            }
            if (std::sqrt(dot(r, r)) <= opt.cg_tolerance * initial) break;
            for (size_t j = 0; j < dim; j++) z[j] = r[j] / (diag[j] + damping * damping_diag[j]);
            const double next = dot(r, z);
            if (!(next >= 0) || !std::isfinite(next)) break;
            const double beta = next / rz;
            for (size_t j = 0; j < dim; j++) direction[j] = z[j] + beta * direction[j];
            rz = next;
        }
        trial_cameras = p.cameras;
        trial_points = p.points;
        for (size_t i = free_camera_gauge ? 0 : 1; i < nc; i++)
            trial_cameras[i] = trial_cameras[i] + get(step, camera_offset((uint32_t)i));
        for (size_t i = 0; i < np; i++) trial_points[i] = trial_points[i] + get(step, po + 3 * i);
        for (size_t i = 0; i < no; i++)
            trial_scales[i] = std::clamp(p.log_scales[i] +
                                          std::clamp(step[so + i], -2.0, 2.0), -20.0, 20.0);
        const double next = trialCost();
        result.iterations = iteration + 1;
        if (std::isfinite(next) && next < cost) {
            const double change = (cost - next) / std::max(cost, 1e-30);
            p.cameras.swap(trial_cameras);
            p.points.swap(trial_points);
            p.log_scales.swap(trial_scales);
            double norm = 0;
            for (const Vec3& c : p.cameras) norm += (c - p.cameras[0]).dot(c - p.cameras[0]);
            norm = nc > 1 ? std::sqrt(norm / (nc - 1)) : 0;
            if (!has_offsets && !p.metric_scale && !free_camera_gauge && norm > 1e-12 &&
                std::isfinite(norm)) {
                const double shift = std::log(norm);
                bool safe = true;
                for (double s : p.log_scales) safe = safe && std::abs(s + shift) <= 20;
                if (safe) {
                    const Vec3 origin = p.cameras[0];
                    for (Vec3& c : p.cameras) c = origin + (c - origin) * (1.0 / norm);
                    for (Vec3& x : p.points) x = origin + (x - origin) * (1.0 / norm);
                    for (double& s : p.log_scales) s += shift;
                }
            }
            cost = next;
            result.accepted++;
            damping = std::max(1e-12, damping * 0.5);
            if (change < opt.relative_tolerance) { result.converged = true; break; }
        } else {
            damping = std::min(1e12, damping * 4.0);
        }
    }
    result.final_cost = cost;
    return result;
}

}  // namespace sfm
