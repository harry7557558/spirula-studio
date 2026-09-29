#pragma once

// The display size of a dataset's camera frusta, from the camera positions
// alone. Chosen so the distinct cameras together cover ~1% of a 90-degree
// view from a typical viewing distance; docs/notes/frustum-size.md derives
// it and lists the measurements behind each constant.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace camhost {

namespace frustum_size_detail {

constexpr double kScreenFraction = 0.01;
constexpr double kHomeFovTan = 1.0;          // tan(90 deg / 2): all three viewers open at 90
// Eyes are spread over a shell of 0.5 to 2 cloud radii around the cloud centre:
// the zoom range a turntable session actually uses. The estimate moves less
// than 2x for any shell within [0.25, 3].
constexpr double kShellInner = 0.5, kShellOuter = 2.0;
// Positions closer than this fraction of the cloud radius are one camera.
// A 5-bracket exposure set jitters 0.05% between brackets with shots 40%
// apart; a 2000-frame walk merges 7% of its frames at 1%, 37% at 2%.
constexpr double kMergeTolerance = 0.01;
constexpr double kMaxSizeOverRadius = 0.15;  // a handful of cameras must not become billboards
constexpr double kFallbackSize = 0.2;        // no spread to measure: one distinct position
// A camera further than this many median distances out is a failed
// registration; kept, it sizes every frustum to itself. Must match
// dsparse::kStrayCameraThreshold (data/DatasetParser.h).
constexpr double kStrayOverMedian = 20.0;

// Mean of 1/|c - e|^2 over eyes e uniform in a ball of radius rho, for a
// camera c at distance r from the ball's centre. Finite everywhere.
inline double ball_mean_inv_dist2(double rho, double r) {
    double rho2 = rho * rho;
    if (r < 1e-9 * rho) return 3.0 / rho2;
    double gap = std::fabs(rho - r);
    if (gap < 1e-9 * rho) return 1.5 / rho2;
    return 1.5 / rho2 + 3.0 * (rho2 - r * r) / (4.0 * rho2 * rho * r) * std::log((rho + r) / gap);
}

inline double shell_mean_inv_dist2(double r_in, double r_out, double r) {
    double vi = r_in * r_in * r_in, vo = r_out * r_out * r_out;
    return (vo * ball_mean_inv_dist2(r_out, r) - vi * ball_mean_inv_dist2(r_in, r)) / (vo - vi);
}

using P3 = std::array<double, 3>;

inline double dist2(const P3& a, const P3& b) {
    double dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return dx * dx + dy * dy + dz * dz;
}

inline P3 centroid(const std::vector<P3>& p) {
    P3 c{0, 0, 0};
    for (const P3& q : p) for (int k = 0; k < 3; k++) c[k] += q[k];
    for (double& v : c) v /= (double)p.size();
    return c;
}

inline std::vector<P3> drop_strays(std::vector<P3> p) {
    if (p.size() < 3) return p;
    P3 m;
    std::vector<double> v(p.size());
    for (int k = 0; k < 3; k++) {
        for (size_t i = 0; i < p.size(); i++) v[i] = p[i][k];
        std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
        m[k] = v[v.size() / 2];
    }
    for (size_t i = 0; i < p.size(); i++) v[i] = dist2(p[i], m);
    std::vector<double> d = v;
    std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
    const double lim = kStrayOverMedian * kStrayOverMedian * d[d.size() / 2];
    if (!(lim > 0.0)) return p;
    std::vector<P3> kept;
    for (size_t i = 0; i < p.size(); i++)
        if (v[i] <= lim) kept.push_back(p[i]);
    return kept;
}

inline double max_radius(const std::vector<P3>& p, const P3& c) {
    double r2 = 0.0;
    for (const P3& q : p) r2 = std::fmax(r2, dist2(q, c));
    return std::sqrt(r2);
}

// Keeps the first of every group of positions within eps of each other, via a
// hash grid of cell size eps (27-cell probe), so a 20k-camera set stays O(N).
inline std::vector<P3> merge_within(const std::vector<P3>& p, double eps) {
    std::vector<P3> kept;
    std::unordered_map<uint64_t, std::vector<int>> cells;
    auto cell = [eps](double v) { return (int64_t)std::floor(v / eps); };
    auto key = [](int64_t x, int64_t y, int64_t z) {
        return (uint64_t)x * 73856093ull ^ (uint64_t)y * 19349663ull ^ (uint64_t)z * 83492791ull;
    };
    double eps2 = eps * eps;
    for (const P3& q : p) {
        int64_t cx = cell(q[0]), cy = cell(q[1]), cz = cell(q[2]);
        bool dup = false;
        for (int dx = -1; dx <= 1 && !dup; dx++)
            for (int dy = -1; dy <= 1 && !dup; dy++)
                for (int dz = -1; dz <= 1 && !dup; dz++) {
                    auto it = cells.find(key(cx + dx, cy + dy, cz + dz));
                    if (it == cells.end()) continue;
                    for (int i : it->second)
                        if (dist2(kept[i], q) < eps2) { dup = true; break; }
                }
        if (dup) continue;
        cells[key(cx, cy, cz)].push_back((int)kept.size());
        kept.push_back(q);
    }
    return kept;
}

}  // namespace frustum_size_detail

// c2w: [n, 3, 4] camera-to-world rows; only the translation column is read,
// so the flipped and unflipped conventions both work. The result is in the
// positions' units.
inline double frustum_display_size(const float* c2w, int64_t n) {
    using namespace frustum_size_detail;
    std::vector<P3> pos;
    pos.reserve((size_t)std::max<int64_t>(n, 0));
    for (int64_t i = 0; i < n; i++) {
        P3 q{c2w[i * 12 + 3], c2w[i * 12 + 7], c2w[i * 12 + 11]};
        if (std::isfinite(q[0]) && std::isfinite(q[1]) && std::isfinite(q[2])) pos.push_back(q);
    }
    pos = drop_strays(std::move(pos));
    if (pos.empty()) return kFallbackSize;
    double radius = max_radius(pos, centroid(pos));
    if (!(radius > 0.0)) return kFallbackSize;

    std::vector<P3> uniq = merge_within(pos, kMergeTolerance * radius);
    P3 c = centroid(uniq);
    radius = max_radius(uniq, c);
    if (uniq.size() < 2 || !(radius > 0.0)) return kFallbackSize;

    double q = 0.0;
    for (const P3& u : uniq)
        q += shell_mean_inv_dist2(kShellInner * radius, kShellOuter * radius, std::sqrt(dist2(u, c)));
    q /= (double)uniq.size();

    // N * (s * F / D)^2 = p * W^2 with F = W / (2 tan(fov/2)), solved for s.
    double s = 2.0 * kHomeFovTan * std::sqrt(kScreenFraction / (double)uniq.size()) / std::sqrt(q);
    return std::fmin(s, kMaxSizeOverRadius * radius);
}

}  // namespace camhost
