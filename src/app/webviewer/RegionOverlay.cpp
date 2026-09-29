// RegionOverlay.cpp -- see RegionOverlay.h.

#include "app/webviewer/RegionOverlay.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace spirula {

namespace {

constexpr float kFillFront = 0.25f, kFillHidden = 0.05f;
constexpr float kLineFront = 0.9f, kLineHidden = 0.12f;
constexpr int kDashOn = 7, kDashPeriod = 12;

bool faces_toward(const RegionOverlay::Layer& l, int32_t t, const float eye[3]) {
    const float* a = &l.xyz[l.tri[(size_t)t * 3] * 3];
    const float* b = &l.xyz[l.tri[(size_t)t * 3 + 1] * 3];
    const float* c = &l.xyz[l.tri[(size_t)t * 3 + 2] * 3];
    const float u[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const float v[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    const float n[3] = {u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]};
    return n[0] * (eye[0] - a[0]) + n[1] * (eye[1] - a[1]) + n[2] * (eye[2] - a[2]) > 0;
}

void blend(uint8_t* px, const float rgb[3], float a) {
    for (int k = 0; k < 3; k++)
        px[k] = (uint8_t)std::lround(std::clamp(px[k] * (1.0f - a) + 255.0f * rgb[k] * a, 0.0f, 255.0f));
}

}  // namespace

void RegionOverlay::add(const RegionMesh& m, const float rgb[3], const float to_frame[12]) {
    if (m.empty()) return;
    Layer l;
    l.xyz.resize(m.xyz.size());
    for (size_t v = 0; v < m.xyz.size(); v += 3) {
        const float* p = &m.xyz[v];
        for (int r = 0; r < 3; r++)
            l.xyz[v + r] = to_frame ? to_frame[r * 4] * p[0] + to_frame[r * 4 + 1] * p[1] +
                                          to_frame[r * 4 + 2] * p[2] + to_frame[r * 4 + 3]
                                    : p[r];
    }
    l.tri = m.tri;
    for (int k = 0; k < 3; k++) l.rgb[k] = rgb[k];
    // Surface nets leave a bump per cell, and every bump puts a silhouette
    // edge in the outline; a few rounds of neighbour averaging take them out.
    const size_t n_v = l.xyz.size() / 3;
    std::vector<float> sum(n_v * 3);
    std::vector<int> cnt(n_v);
    for (int round = 0; round < 6; round++) {
        std::fill(sum.begin(), sum.end(), 0.0f);
        std::fill(cnt.begin(), cnt.end(), 0);
        for (size_t t = 0; t < l.tri.size(); t += 3)
            for (int e = 0; e < 3; e++) {
                const uint32_t a = l.tri[t + e], b = l.tri[t + (e + 1) % 3];
                for (int r = 0; r < 3; r++) {
                    sum[a * 3 + r] += l.xyz[b * 3 + r];
                    sum[b * 3 + r] += l.xyz[a * 3 + r];
                }
                cnt[a]++;
                cnt[b]++;
            }
        for (size_t v = 0; v < n_v; v++)
            if (cnt[v] > 0)
                for (int r = 0; r < 3; r++)
                    l.xyz[v * 3 + r] = 0.5f * l.xyz[v * 3 + r] + 0.5f * sum[v * 3 + r] / (float)cnt[v];
    }
    std::unordered_map<uint64_t, uint32_t> seen;
    for (size_t t = 0; t < l.tri.size() / 3; t++)
        for (int e = 0; e < 3; e++) {
            uint32_t a = l.tri[t * 3 + e], b = l.tri[t * 3 + (e + 1) % 3];
            if (a > b) std::swap(a, b);
            const uint64_t key = (uint64_t)a << 32 | b;
            auto [it, fresh] = seen.emplace(key, (uint32_t)(l.edge.size() / 2));
            if (fresh) {
                l.edge.insert(l.edge.end(), {a, b});
                l.face.insert(l.face.end(), {(int32_t)t, -1});
            } else {
                l.face[(size_t)it->second * 2 + 1] = (int32_t)t;
            }
        }
    layers.push_back(std::move(l));
}

void region_outline(const RegionOverlay::Layer& l, const float eye[3], std::vector<float>& out) {
    out.clear();
    const size_t n_tri = l.tri.size() / 3;
    std::vector<uint8_t> toward(n_tri);
    for (size_t t = 0; t < n_tri; t++) toward[t] = faces_toward(l, (int32_t)t, eye);
    for (size_t e = 0; e < l.edge.size() / 2; e++) {
        const int32_t f0 = l.face[e * 2], f1 = l.face[e * 2 + 1];
        if (f1 >= 0 && toward[(size_t)f0] == toward[(size_t)f1]) continue;
        for (int k = 0; k < 2; k++) {
            const float* p = &l.xyz[l.edge[e * 2 + k] * 3];
            out.insert(out.end(), p, p + 3);
        }
    }
}

void draw_region_overlay(const RegionOverlay& ov, uint8_t* rgb, int W, int H, const float* ray_depth,
                         const float c2w[12], float fx, float fy, float cx, float cy) {
    if (ov.empty() || W <= 0 || H <= 0) return;
    // CV camera axes are the OpenGL ones with y and z negated.
    float R[3][3];
    for (int k = 0; k < 3; k++) {
        R[0][k] = c2w[k * 4 + 0];
        R[1][k] = -c2w[k * 4 + 1];
        R[2][k] = -c2w[k * 4 + 2];
    }
    const float eye[3] = {c2w[3], c2w[7], c2w[11]};
    auto to_cam = [&](const float* p, float c[3]) {
        const float d[3] = {p[0] - eye[0], p[1] - eye[1], p[2] - eye[2]};
        for (int r = 0; r < 3; r++) c[r] = R[r][0] * d[0] + R[r][1] * d[1] + R[r][2] * d[2];
    };
    // Scene distance of a pixel, converted to depth along the view axis.
    const size_t npx = (size_t)W * H;
    std::vector<float> scene_z(npx), ray_scale(npx);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            const size_t i = (size_t)y * W + x;
            const float u = (x + 0.5f - cx) / fx, v = (y + 0.5f - cy) / fy;
            ray_scale[i] = std::sqrt(1.0f + u * u + v * v);
            const float d = ray_depth ? ray_depth[i] : 0.0f;
            scene_z[i] = d > 0 && std::isfinite(d) ? d / ray_scale[i] : std::numeric_limits<float>::infinity();
        }
    constexpr float kNear = 1e-4f;

    // Outside, greyed and darkened: tested every kStep pixels, where there is
    // a surface to test.
    if (ov.region) {
        constexpr int kStep = 3;
        const int gw = (W + kStep - 1) / kStep, gh = (H + kStep - 1) / kStep;
        std::vector<int8_t> out((size_t)gw * gh, -1);
#pragma omp parallel for schedule(dynamic, 8)
        for (int gy = 0; gy < gh; gy++)
            for (int gx = 0; gx < gw; gx++) {
                const int x = std::min(W - 1, gx * kStep + kStep / 2), y = std::min(H - 1, gy * kStep + kStep / 2);
                const float z = scene_z[(size_t)y * W + x];
                if (!std::isfinite(z)) continue;
                const float cvx = (x + 0.5f - cx) / fx * z, cvy = (y + 0.5f - cy) / fy * z;
                double p[3];
                for (int r = 0; r < 3; r++)
                    p[r] = eye[r] + R[0][r] * cvx + R[1][r] * cvy + R[2][r] * z + ov.shift[r];
                out[(size_t)gy * gw + gx] = ov.region->contains(p) ? 0 : 1;
            }
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                if (out[(size_t)(y / kStep) * gw + x / kStep] != 1) continue;
                uint8_t* px = &rgb[((size_t)y * W + x) * 3];
                const float g = 0.45f * (0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2]);
                for (int k = 0; k < 3; k++) px[k] = (uint8_t)std::lround(0.2f * px[k] + 0.8f * g);
            }
    }

    for (const RegionOverlay::Layer& l : ov.layers) {
        const size_t n_v = l.xyz.size() / 3;
        std::vector<float> cam(n_v * 3);
        for (size_t v = 0; v < n_v; v++) to_cam(&l.xyz[v * 3], &cam[v * 3]);
        // Nearest surface per pixel as 1/z, 0 for none.
        std::vector<float> inv_z(npx, 0.0f);
        for (size_t t = 0; t < l.tri.size() / 3; t++) {
            float sx[3], sy[3], iz[3];
            bool ok = true;
            for (int k = 0; k < 3 && ok; k++) {
                const float* c = &cam[l.tri[t * 3 + k] * 3];
                ok = c[2] > kNear;
                if (!ok) break;
                iz[k] = 1.0f / c[2];
                sx[k] = fx * c[0] * iz[k] + cx;
                sy[k] = fy * c[1] * iz[k] + cy;
            }
            if (!ok) continue;
            const float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
            if (std::fabs(area) < 1e-12f) continue;
            const int x0 = std::max(0, (int)std::floor(std::min({sx[0], sx[1], sx[2]})));
            const int x1 = std::min(W - 1, (int)std::ceil(std::max({sx[0], sx[1], sx[2]})));
            const int y0 = std::max(0, (int)std::floor(std::min({sy[0], sy[1], sy[2]})));
            const int y1 = std::min(H - 1, (int)std::ceil(std::max({sy[0], sy[1], sy[2]})));
            for (int y = y0; y <= y1; y++)
                for (int x = x0; x <= x1; x++) {
                    const float px = x + 0.5f, py = y + 0.5f;
                    float w[3];
                    for (int k = 0; k < 3; k++) {
                        const int a = (k + 1) % 3, b = (k + 2) % 3;
                        w[k] = ((sx[b] - sx[a]) * (py - sy[a]) - (sy[b] - sy[a]) * (px - sx[a])) / area;
                    }
                    if (w[0] < 0 || w[1] < 0 || w[2] < 0) continue;
                    const float z = w[0] * iz[0] + w[1] * iz[1] + w[2] * iz[2];
                    float& dst = inv_z[(size_t)y * W + x];
                    dst = std::max(dst, z);
                }
        }
        for (size_t i = 0; i < npx; i++) {
            if (inv_z[i] <= 0) continue;
            const bool front = 1.0f / inv_z[i] <= scene_z[i] * 1.01f;
            if (!ov.region) blend(&rgb[i * 3], l.rgb, front ? kFillFront : kFillHidden);
        }

        std::vector<float> seg;
        region_outline(l, eye, seg);
        for (size_t s = 0; s + 6 <= seg.size(); s += 6) {
            float a[3], b[3];
            to_cam(&seg[s], a);
            to_cam(&seg[s + 3], b);
            if (a[2] <= kNear || b[2] <= kNear) continue;
            const float ax = fx * a[0] / a[2] + cx, ay = fy * a[1] / a[2] + cy;
            const float bx = fx * b[0] / b[2] + cx, by = fy * b[1] / b[2] + cy;
            const int steps = (int)std::ceil(std::max(std::fabs(bx - ax), std::fabs(by - ay)));
            if (steps <= 0 || steps > 4 * (W + H)) continue;
            for (int k = 0; k <= steps; k++) {
                if (k % kDashPeriod >= kDashOn) continue;
                const float f = (float)k / (float)steps;
                const int x = (int)(ax + f * (bx - ax)), y = (int)(ay + f * (by - ay));
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                const float z = 1.0f / ((1 - f) / a[2] + f / b[2]);
                const size_t i = (size_t)y * W + x;
                blend(&rgb[i * 3], l.rgb, z <= scene_z[i] * 1.01f ? kLineFront : kLineHidden);
                if (x + 1 < W) blend(&rgb[(i + 1) * 3], l.rgb, 0.5f * (z <= scene_z[i] * 1.01f ? kLineFront : kLineHidden));
            }
        }
    }
}

}  // namespace spirula
