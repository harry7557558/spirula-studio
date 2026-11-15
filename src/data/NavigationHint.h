#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dsparse {

struct NavigationHint {
    bool orbit = true;
    bool pivot_focus = false;   // orbit about `focus`, else about the camera median
    std::array<double, 3> focus{};
    double inward = 0;          // distance-weighted mean cosine, view vs way to the camera median
    double aimed = 0;           // fraction of views within 40 degrees of `focus`
};

// OpenGL c2w, row-major 3x4. No poses retains orbit; ambiguous captures use fly.
// Thresholds and the captures that set them: docs/notes/viewer-navigation.md.
template<class T>
NavigationHint navigation_hint(const T* c2w, int64_t n) {
    NavigationHint out;
    if (!c2w || n <= 0) return out;
    out.orbit = false;
    if (n < 4) return out;
    struct Ray { std::array<double, 3> p, d; double weight = 1; };
    std::vector<Ray> rays;
    const int64_t step = std::max<int64_t>(1, (n + 2047) / 2048);
    for (int64_t i = 0; i < n; i += step) {
        Ray ray;
        double norm = 0;
        bool valid = true;
        for (int k = 0; k < 3; k++) {
            ray.p[k] = c2w[12*i + 4*k + 3];
            ray.d[k] = -c2w[12*i + 4*k + 2];
            valid &= std::isfinite(ray.p[k]) && std::isfinite(ray.d[k]);
            norm += ray.d[k]*ray.d[k];
        }
        if (!valid || norm < 1e-20) continue;
        for (double& d : ray.d) d /= std::sqrt(norm);
        rays.push_back(ray);
    }
    if (rays.size() < 4) return out;
    std::array<double, 3> origin{};
    std::vector<double> values;
    auto median = [](std::vector<double>& v) {
        auto mid = v.begin() + v.size()/2;
        std::nth_element(v.begin(), mid, v.end());
        return *mid;
    };
    for (int k = 0; k < 3; k++) {
        values.clear();
        for (auto& ray : rays) values.push_back(ray.p[k]);
        origin[k] = median(values);
    }
    double scale = 0;
    values.clear();
    for (auto& ray : rays) {
        double d2 = 0;
        for (int k = 0; k < 3; k++) {
            ray.p[k] -= origin[k];
            d2 += ray.p[k]*ray.p[k];
        }
        values.push_back(std::sqrt(d2));
    }
    scale = median(values);
    if (!(scale > 1e-12)) return out;
    for (auto& ray : rays) for (double& p : ray.p) p /= scale;
    // A view's way to the median is undefined at the median, so weight by distance.
    double inward = 0, total = 0;
    for (const auto& ray : rays) {
        double dist = 0, along = 0;
        for (int k = 0; k < 3; k++) {
            dist += ray.p[k]*ray.p[k];
            along -= ray.p[k]*ray.d[k];
        }
        dist = std::sqrt(dist);
        if (!(dist > 1e-12)) continue;
        inward += std::min(dist, 1.0)*along/dist;
        total += std::min(dist, 1.0);
    }
    out.inward = total > 0 ? inward/total : 0;
    out.orbit = out.inward >= 0.5;
    std::array<double, 3> focus{};
    for (int pass = 0; pass < 8; pass++) {
        double a[3][4] = {};
        for (const auto& ray : rays)
            for (int r = 0; r < 3; r++)
                for (int c = 0; c < 3; c++) {
                    double m = ray.weight * ((r == c ? 1.0 : 0.0) - ray.d[r]*ray.d[c]);
                    a[r][c] += m;
                    a[r][3] += m*ray.p[c];
                }
        for (int k = 0; k < 3; k++) {
            int pivot = k;
            for (int r = k+1; r < 3; r++)
                if (std::abs(a[r][k]) > std::abs(a[pivot][k])) pivot = r;
            if (std::abs(a[pivot][k]) < 1e-6*rays.size()) return out;
            for (int c = 0; c < 4; c++) std::swap(a[k][c], a[pivot][c]);
            const double div = a[k][k];
            for (int c = k; c < 4; c++) a[k][c] /= div;
            for (int r = 0; r < 3; r++) if (r != k) {
                const double f = a[r][k];
                for (int c = k; c < 4; c++) a[r][c] -= f*a[k][c];
            }
        }
        for (int k = 0; k < 3; k++) focus[k] = a[k][3];
        for (auto& ray : rays) {
            double depth = 0, d2 = 0;
            for (int k = 0; k < 3; k++) {
                double v = focus[k] - ray.p[k];
                depth += v*ray.d[k]; d2 += v*v;
            }
            double residual = std::sqrt(std::max(0.0, d2-depth*depth));
            ray.weight = std::min(1.0, 0.15/std::max(residual, 1e-12));
        }
    }
    int aimed = 0;
    std::array<double, 3> mean{};
    for (const auto& ray : rays) {
        double depth = 0, d2 = 0;
        for (int k = 0; k < 3; k++) {
            double v = focus[k] - ray.p[k];
            depth += v*ray.d[k]; d2 += v*v;
        }
        if (depth > 0 && depth*depth > 0.58682409*d2) {   // cos^2(40 deg)
            aimed++;
            for (int k = 0; k < 3; k++) mean[k] += ray.d[k];
        }
    }
    double coherence = 0;
    for (double v : mean) coherence += v*v;
    out.aimed = (double)aimed / rays.size();
    // At least ~23 degrees of directional spread among the aimed views.
    out.pivot_focus = out.aimed >= 0.75 && coherence < 0.96*aimed*aimed;
    out.orbit |= out.pivot_focus;
    for (int k = 0; k < 3; k++) out.focus[k] = origin[k] + scale*focus[k];
    return out;
}

} // namespace dsparse
