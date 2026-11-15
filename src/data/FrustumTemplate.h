#pragma once

// A camera's frustum as every viewer draws it: the image border through the
// lens, a wire dome over a wide one, a globe for equirectangular, and the
// points the apex is joined to. Size 1, CV camera space (+Z forward, +Y
// down); scale by the display size (data/FrustumSize.h). The web viewer's
// frustumTemplate (viewer/js/dataset.js) is the reference this follows.

#include "data/CameraMath.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace camhost {

constexpr int kFrustumSeg = 16;         // segments per image edge
constexpr int kFrustumAnchorSeg = 4;    // per line from the apex

struct FrustumPoint { float x, y, z; };
struct FrustumLine {
    std::vector<FrustumPoint> pts;
    bool closed = false;
    bool dim = false;                   // a gridline, drawn fainter
};
struct FrustumShape {
    std::vector<FrustumLine> lines;
    std::vector<FrustumPoint> anchors;
};

inline FrustumShape frustum_template(int model, int tier, int w, int h, float fx, float fy,
                                     float cx, float cy, const float* dist) {
    FrustumShape out;
    if (w < 1) w = std::max(1, (int)std::lround(2 * cx));
    if (h < 1) h = std::max(1, (int)std::lround(2 * cy));
    const double a = std::sqrt((double)fx * fy / ((double)w * h));
    const double sxy = std::sqrt(a), sz = std::sqrt(a);
    const double corners[4][2] = {{0, 0}, {(double)w, 0}, {(double)w, (double)h}, {0, (double)h}};
    if (model == 4) {
        std::vector<FrustumPoint> near_border, far_border;
        for (const auto& xy : corners) {
            const float x = float((xy[0] - cx) / fx * sxy);
            const float y = float((xy[1] - cy) / fy * sxy);
            near_border.push_back({x, y, 0});
            far_border.push_back({x, y, float(sz)});
            out.lines.push_back({{near_border.back(), far_border.back()}, false, false});
        }
        out.lines.push_back({std::move(near_border), true, false});
        out.lines.push_back({std::move(far_border), true, false});
        return out;
    }
    const bool wide = model == 1 || model == 2 || model == 3;
    const double shell = std::sqrt((2 * sxy * sxy + sz * sz) / 3);
    auto place = [&](const double d[3]) -> FrustumPoint {
        if (wide) return {(float)(d[0] * shell), (float)(d[1] * shell), (float)(d[2] * shell)};
        if (std::fabs(d[2]) < 1e-6) return {0, 0, 0};
        return {(float)(d[0] * sxy / d[2]), (float)(d[1] * sxy / d[2]), (float)sz};
    };
    // Past where the lens folds: the last point toward the middle that works.
    auto ray = [&](double u, double v, double d[3]) {
        if (generate_ray(u, v, model, tier, dist, d)) return;
        double lo = 0, hi = 1, best[3] = {0, 0, 1};
        for (int k = 0; k < 12; k++) {
            const double s = 0.5 * (lo + hi);
            double r[3];
            if (generate_ray(u * s, v * s, model, tier, dist, r)) {
                lo = s;
                best[0] = r[0]; best[1] = r[1]; best[2] = r[2];
            } else {
                hi = s;
            }
        }
        d[0] = best[0]; d[1] = best[1]; d[2] = best[2];
    };
    auto sample = [&](double x0, double y0, double x1, double y1, int n) {
        std::vector<FrustumPoint> pts;
        pts.reserve((size_t)n + 1);
        for (int i = 0; i <= n; i++) {
            const double t = (double)i / n;
            double d[3];
            ray((x0 + (x1 - x0) * t - cx) / fx, (y0 + (y1 - y0) * t - cy) / fy, d);
            pts.push_back(place(d));
        }
        return pts;
    };
    // An equirectangular border row is a pole.
    auto degenerate = [&](const std::vector<FrustumPoint>& pts) {
        const FrustumPoint& p0 = pts[0];
        for (const FrustumPoint& p : pts)
            if (std::hypot(p.x - p0.x, p.y - p0.y, p.z - p0.z) >= 1e-5 * shell + 1e-12)
                return false;
        return true;
    };
    constexpr int N = kFrustumSeg;
    if (model == 3) {
        for (int i = 0; i < 8; i++) {
            auto m = sample(w * i / 8.0, 0, w * i / 8.0, h, 2 * N);
            if (!degenerate(m)) out.lines.push_back({std::move(m), false, i != 0});
        }
        for (int i = 1; i < 4; i++) {
            auto p = sample(0, h * i / 4.0, w, h * i / 4.0, 2 * N);
            if (!degenerate(p)) out.lines.push_back({std::move(p), false, i != 2});
        }
        double d[3];
        ray((w / 2.0 - cx) / fx, (h / 2.0 - cy) / fy, d);
        out.anchors.push_back(place(d));
        return out;
    }
    std::vector<FrustumPoint> border;
    for (int e = 0; e < 4; e++) {
        auto edge = sample(corners[e][0], corners[e][1], corners[(e + 1) % 4][0],
                           corners[(e + 1) % 4][1], N);
        border.insert(border.end(), edge.begin(), edge.begin() + N);
    }
    out.anchors = {border[0], border[N], border[2 * N], border[3 * N]};
    out.lines.push_back({std::move(border), true, false});
    if (wide)
        for (int i = 1; i <= 3; i++) {
            out.lines.push_back({sample(w * i / 4.0, 0, w * i / 4.0, h, 2 * N), false, true});
            out.lines.push_back({sample(0, h * i / 4.0, w, h * i / 4.0, 2 * N), false, true});
        }
    return out;
}

}  // namespace camhost
