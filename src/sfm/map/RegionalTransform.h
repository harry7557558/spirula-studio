#pragma once

#include "sfm/map/RegionalLandmarks.h"
#include <numeric>
#include <optional>

namespace sfm::regional {

inline Sim3 composeShared(const Sim3& a, const Sim3& b) {
    return {a.scale * b.scale, mul(a.R, b.R), mul(a.R, b.t) * a.scale + a.t};
}
inline Sim3 inverseShared(const Sim3& a) {
    Mat3 r = transpose(a.R); return {1. / a.scale, r, mul(r, a.t) * (-1. / a.scale)};
}
struct SharedTransformResult {
    std::vector<Sim3> transforms;
    size_t edges = 0, pair_count = 0;
    double initial_cost = 0, final_cost = 0, rms = 0;
    double uniform_reference_cost = 0, candidate_reference_cost = 0, reference_gate = 0;
    size_t reference_pairs = 0;
    bool used_uniform = false;
    bool candidate_available = true;
};

inline SharedTransformResult alignSharedBlocks(SharedIndex& index, size_t minimum_common = 50,
                                             size_t memory_budget = 256u << 20) {
    SharedTransformResult out; out.transforms.resize(index.models);
    if (index.models <= 1) return out;
    Vec3 origin; size_t count = 0;
    for (const auto& p : index.landmarks) if (p.valid) { origin = origin + p.xyz; ++count; }
    if (!count) throw std::runtime_error("regional coordination: no shared landmarks");
    origin = origin * (1. / count); double extent = 0;
    for (const auto& p : index.landmarks) if (p.valid) extent += (p.xyz - origin).dot(p.xyz - origin);
    extent = std::max(1e-6, std::sqrt(extent / count));
    struct Pair { Vec3 a, b; };
    struct Edge { uint32_t a = 0, b = 0; std::vector<Pair> pairs; Sim3 b_to_a; };
    std::map<std::pair<uint32_t, uint32_t>, Edge> pairs;
    size_t pair_bytes = 0;
    for (const auto& p : index.landmarks) if (p.valid) {
        for (size_t i = 0; i < p.members.size(); ++i) for (size_t j = i + 1; j < p.members.size(); ++j) {
            auto a = p.members[i], b = p.members[j]; if (a.model > b.model) std::swap(a, b);
            if (pair_bytes > memory_budget || 4 * sizeof(Pair) > memory_budget - pair_bytes)
                throw std::runtime_error("regional block transform exceeds memory budget; reduce regional-landmarks");
            pair_bytes += 4 * sizeof(Pair);
            auto& e = pairs[{a.model, b.model}]; e.a = a.model; e.b = b.model;
            e.pairs.push_back({(a.xyz - origin) * (1. / extent), (b.xyz - origin) * (1. / extent)});
        }
    }
    std::vector<Edge> edges;
    for (auto& item : pairs) {
        cancel::check(); auto& e = item.second;
        if (e.pairs.size() < minimum_common) continue;
        std::vector<Vec3> a, b;
        for (const auto& p : e.pairs) { a.push_back(p.a); b.push_back(p.b); }
        auto noncollinear = [](const std::vector<Vec3>& v) {
            Vec3 mean; for (const auto& x : v) mean = mean + x; mean = mean * (1. / v.size());
            Mat3 cov{};
            for (const auto& x : v) { auto d = x - mean; double c[3] = {d.x, d.y, d.z};
                for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) cov[3 * i + j] += c[i] * c[j]; }
            auto s = svd3(cov); return s.s.x > 1e-12 && s.s.y > s.s.x * 1e-8;
        };
        if (!noncollinear(a) || !noncollinear(b) || !estimateSim3(b, a, e.b_to_a)) continue;
        std::vector<double> distances;
        for (const auto& p : e.pairs) distances.push_back((transformPoint(e.b_to_a, p.b) - p.a).norm());
        auto sorted = distances; std::sort(sorted.begin(), sorted.end());
        double gate = std::max(1e-9, 40 * sorted[sorted.size() / 2]);
        std::vector<Pair> retained; a.clear(); b.clear();
        for (size_t i = 0; i < e.pairs.size(); ++i) if (distances[i] <= gate) {
            retained.push_back(e.pairs[i]); a.push_back(e.pairs[i].a); b.push_back(e.pairs[i].b);
        }
        if (retained.size() * 5 < e.pairs.size() * 4 || retained.size() < minimum_common ||
            !noncollinear(a) || !noncollinear(b) || !estimateSim3(b, a, e.b_to_a)) continue;
        e.pairs = std::move(retained); out.edges += e.pairs.size(); edges.push_back(std::move(e));
    }
    out.pair_count = edges.size();
    std::vector<uint8_t> connected(index.models); connected[0] = 1;
    size_t reached = 1;
    for (;;) {
        size_t before = reached;
        for (const auto& e : edges) {
            if (connected[e.a] && !connected[e.b]) {
                out.transforms[e.b] = composeShared(out.transforms[e.a], e.b_to_a); connected[e.b] = 1; ++reached;
            } else if (connected[e.b] && !connected[e.a]) {
                out.transforms[e.a] = composeShared(out.transforms[e.b], inverseShared(e.b_to_a)); connected[e.a] = 1; ++reached;
            }
        }
        if (reached == before) break;
    }
    if (reached != index.models) throw std::runtime_error("regional coordination: shared-landmark block graph is disconnected or degenerate");
    std::vector<double> norms;
    for (const auto& e : edges) for (const auto& p : e.pairs)
        norms.push_back((transformPoint(out.transforms[e.a], p.a) - transformPoint(out.transforms[e.b], p.b)).norm());
    auto middle = norms.begin() + norms.size() / 2; std::nth_element(norms.begin(), middle, norms.end());
    const double huber = std::max(1e-7, *middle * 2);
    auto cost = [&](const std::vector<Sim3>& t) {
        double sum = 0;
        for (const auto& e : edges) for (const auto& p : e.pairs) {
            double d = (transformPoint(t[e.a], p.a) - transformPoint(t[e.b], p.b)).norm();
            sum += d <= huber ? d * d : huber * (2 * d - huber);
        }
        return sum;
    };
    double current = out.initial_cost = cost(out.transforms), lambda = 1e-3;
    const size_t dim = 7 * (index.models - 1);
    using Block = std::array<double, 49>;
    for (int iteration = 0; iteration < 30; ++iteration) {
        cancel::check(); std::map<std::pair<uint32_t, uint32_t>, Block> blocks;
        std::vector<double> g(dim), diag(dim);
        for (const auto& e : edges) for (const auto& p : e.pairs) {
            Vec3 ya = mul(out.transforms[e.a].R, p.a) * out.transforms[e.a].scale;
            Vec3 yb = mul(out.transforms[e.b].R, p.b) * out.transforms[e.b].scale;
            Vec3 residual = ya + out.transforms[e.a].t - yb - out.transforms[e.b].t;
            double w = residual.norm() > huber ? huber / residual.norm() : 1;
            double r[3] = {residual.x, residual.y, residual.z};
            auto jac = [](Vec3 y, double sign) {
                std::array<double, 21> j{0,-y.z,y.y,1,0,0,y.x, y.z,0,-y.x,0,1,0,y.y, -y.y,y.x,0,0,0,1,y.z};
                for (auto& v : j) v *= sign; return j;
            };
            auto ja = jac(ya, 1), jb = jac(yb, -1);
            // Left rotation has derivative -[y]x.
            for (int row = 0; row < 3; ++row) for (int k = 0; k < 3; ++k) {
                ja[7 * row + k] *= -1; jb[7 * row + k] *= -1;
            }
            auto add = [&](uint32_t a, uint32_t b, const auto& x, const auto& y) {
                if (!a || !b) return;
                auto& block = blocks[{a - 1, b - 1}];
                for (int i = 0; i < 7; ++i) for (int j = 0; j < 7; ++j)
                    for (int k = 0; k < 3; ++k) block[7 * i + j] += w * x[7 * k + i] * y[7 * k + j];
            };
            add(e.a,e.a,ja,ja); add(e.b,e.b,jb,jb); add(e.a,e.b,ja,jb);
            for (auto side : {0,1}) {
                uint32_t m = side ? e.b : e.a; if (!m) continue; const auto& j = side ? jb : ja;
                for (int k = 0; k < 7; ++k) for (int row = 0; row < 3; ++row) g[7 * (m - 1) + k] += w * j[7 * row + k] * r[row];
            }
        }
        double grad = 0;
        for (size_t i = 0; i < dim; ++i) { diag[i] = std::max(1e-12, blocks[{uint32_t(i / 7), uint32_t(i / 7)}][7 * (i % 7) + i % 7]); grad = std::max(grad, std::abs(g[i])); }
        if (grad < 1e-10) break;
        auto product = [&](const std::vector<double>& x) {
            std::vector<double> y(dim);
            for (const auto& item : blocks) {
                size_t a = 7 * item.first.first, b = 7 * item.first.second;
                for (int i = 0; i < 7; ++i) for (int j = 0; j < 7; ++j) {
                    y[a + i] += item.second[7 * i + j] * x[b + j];
                    if (a != b) y[b + j] += item.second[7 * i + j] * x[a + i];
                }
            }
            for (size_t i = 0; i < dim; ++i) y[i] += lambda * diag[i] * x[i]; return y;
        };
        std::vector<double> step(dim), r(dim), z(dim), direction(dim);
        double rz = 0, initial_norm = 0;
        for (size_t i = 0; i < dim; ++i) { r[i] = -g[i]; direction[i] = z[i] = r[i] / (diag[i] * (1 + lambda)); rz += r[i] * z[i]; initial_norm += r[i] * r[i]; }
        for (size_t cg = 0; cg < std::min<size_t>(600, dim * 2 + 20); ++cg) {
            auto y = product(direction); double denom = 0;
            for (size_t i = 0; i < dim; ++i) denom += direction[i] * y[i];
            if (!(denom > 0) || !std::isfinite(denom)) break;
            double alpha = rz / denom, norm = 0, next_rz = 0;
            for (size_t i = 0; i < dim; ++i) { step[i] += alpha * direction[i]; r[i] -= alpha * y[i]; norm += r[i] * r[i]; z[i] = r[i] / (diag[i] * (1 + lambda)); next_rz += r[i] * z[i]; }
            if (norm <= initial_norm * 1e-16) break;
            for (size_t i = 0; i < dim; ++i) direction[i] = z[i] + (next_rz / rz) * direction[i]; rz = next_rz;
        }
        auto next = out.transforms; double alpha = 1;
        for (size_t m = 1; m < index.models; ++m) {
            const double* s = step.data() + 7 * (m - 1);
            alpha = std::min(alpha, .25 / std::max(.25, Vec3{s[0],s[1],s[2]}.norm()));
            alpha = std::min(alpha, .25 / std::max(.25, std::abs(s[6])));
        }
        for (size_t m = 1; m < index.models; ++m) {
            const double* s = step.data() + 7 * (m - 1);
            next[m].R = mul(angleAxisToRotation({alpha*s[0],alpha*s[1],alpha*s[2]}), next[m].R);
            next[m].t = next[m].t + Vec3{alpha*s[3],alpha*s[4],alpha*s[5]}; next[m].scale *= std::exp(alpha*s[6]);
        }
        double value = cost(next);
        if (std::isfinite(value) && value < current) {
            double delta = current - value; out.transforms = std::move(next); current = value; lambda = std::max(1e-9, lambda / 3);
            if (delta < std::max(1e-16, out.initial_cost * 1e-8)) break;
        } else { lambda *= 10; if (lambda > 1e9) throw std::runtime_error("regional coordination: joint block transform did not converge"); }
    }
    out.final_cost = current;
    double squared = 0;
    for (const auto& e : edges) for (const auto& p : e.pairs) {
        auto r = transformPoint(out.transforms[e.a],p.a)-transformPoint(out.transforms[e.b],p.b); squared += r.dot(r);
    }
    out.rms = extent * std::sqrt(squared / std::max<size_t>(1,out.edges));
    for (auto& t : out.transforms) t.t = origin + t.t * extent - mul(t.R,origin) * t.scale;
    for (auto& p : index.landmarks) if (p.valid) {
        p.xyz = {};
        for (auto& m : p.members) { m.xyz = transformPoint(out.transforms[m.model],m.xyz); p.xyz = p.xyz + m.xyz; }
        p.xyz = p.xyz * (1. / p.members.size());
    }
    return out;
}

inline SharedTransformResult guardedSharedAlignment(SharedIndex& index, size_t minimum_common = 50,
                                                   size_t memory_budget = 256u << 20) {
    SharedIndex reference; reference.models = index.models;
    reference.landmarks = std::move(index.alignment_reference);
    std::optional<SharedTransformResult> uniform;
    if (!reference.landmarks.empty()) {
        try { uniform = alignSharedBlocks(reference, minimum_common, memory_budget); }
        catch (const Cancelled&) { throw; }
        catch (const std::runtime_error& e) {
            slog::diag(slog::Tag::Map, "[regional] uniform alignment unavailable; protected seam sample required: %s\n", e.what());
        }
    }
    SharedTransformResult candidate;
    try { candidate = alignSharedBlocks(index, minimum_common, memory_budget); }
    catch (const Cancelled&) { throw; }
    catch (const std::runtime_error& e) {
        if (!uniform) throw;
        slog::diag(slog::Tag::Map, "[regional] protected alignment unavailable; retained uniform transform: %s\n", e.what());
        for (auto& p : index.landmarks) if (p.valid) {
            p.xyz = {};
            for (auto& m : p.members) { m.xyz = transformPoint(uniform->transforms[m.model], m.xyz); p.xyz = p.xyz + m.xyz; }
            p.xyz = p.xyz * (1. / p.members.size());
        }
        uniform->used_uniform = true; uniform->candidate_available = false; return *uniform;
    }
    if (!uniform) return candidate;
    std::vector<Sim3> inverse_uniform;
    for (const auto& t : uniform->transforms) inverse_uniform.push_back(inverseShared(t));
    std::vector<double> distances;
    for (const auto& p : reference.landmarks) if (p.valid)
        for (size_t a = 0; a < p.members.size(); ++a) for (size_t b = a + 1; b < p.members.size(); ++b) {
            if (distances.size() >= memory_budget / (16 * sizeof(double)))
                throw std::length_error("regional alignment reference exceeds memory budget");
            distances.push_back((p.members[a].xyz - p.members[b].xyz).norm());
        }
    if (distances.empty()) return candidate;
    auto middle = distances.begin() + distances.size() / 2;
    std::nth_element(distances.begin(), middle, distances.end());
    const double gate = std::max(1e-6, 2 * *middle);
    double baseline_cost = 0, candidate_cost = 0;
    auto rho = [&](double e) { return e <= gate ? e * e : gate * (2 * e - gate); };
    for (const auto& p : reference.landmarks) if (p.valid)
        for (size_t a = 0; a < p.members.size(); ++a) for (size_t b = a + 1; b < p.members.size(); ++b) {
            const auto& x = p.members[a]; const auto& y = p.members[b];
            Vec3 raw_x = transformPoint(inverse_uniform[x.model], x.xyz), raw_y = transformPoint(inverse_uniform[y.model], y.xyz);
            baseline_cost += rho((x.xyz - y.xyz).norm());
            candidate_cost += rho((transformPoint(candidate.transforms[x.model], raw_x) - transformPoint(candidate.transforms[y.model], raw_y)).norm());
        }
    if (!std::isfinite(baseline_cost) || !std::isfinite(candidate_cost)) throw std::runtime_error("nonfinite alignment reference cost");
    bool fallback = candidate_cost > baseline_cost * (1 + 1e-8) + 1e-8;
    auto out = fallback ? *uniform : candidate;
    out.used_uniform = fallback; out.uniform_reference_cost = baseline_cost; out.candidate_reference_cost = candidate_cost;
    out.reference_gate = gate; out.reference_pairs = distances.size();
    if (fallback) {
        std::vector<Sim3> correction;
        for (size_t m = 0; m < index.models; ++m) correction.push_back(composeShared(out.transforms[m], inverseShared(candidate.transforms[m])));
        for (auto& p : index.landmarks) if (p.valid) {
            p.xyz = {};
            for (auto& m : p.members) { m.xyz = transformPoint(correction[m.model], m.xyz); p.xyz = p.xyz + m.xyz; }
            p.xyz = p.xyz * (1. / p.members.size());
        }
    }
    return out;
}

} // namespace sfm::regional
