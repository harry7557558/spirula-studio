#include "app/ScanDepth.h"

#include "core/CameraModel.h"
#include "data/CameraMath.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace app {
namespace {

constexpr float kNone = std::numeric_limits<float>::infinity();
const float kNoDistortion[kCameraDistortionParams] = {};

// A far point seen through a gap in a nearer surface is further than its
// window's nearest by more than this; a floor at grazing incidence 30 m out
// changes by ~3% across the window of a 1600 px map.
constexpr float kBleedRatio = 1.05f;
constexpr float kBleedSlack = 0.01f;   // metres
// A hole is filled only from neighbours on one surface, and only from four or
// more of the eight: a straight edge has three, so a silhouette does not creep
// into the sky behind it. Four passes close the holes a sparse surface leaves.
constexpr float kFillSpread = 1.05f;
constexpr int kFillNeighbours = 4;
constexpr int kFillPasses = 4;
// A normal is taken only where its stencil stays on one surface.
constexpr float kNormalSpread = 0.1f;
// A point's footprint, in pixels either side, is at most this: past it a
// silhouette grows into the background by more than it hides.
constexpr int kMaxFootprint = 3;

}  // namespace


ScanCloud build_scan_cloud(const std::vector<double>& xyz, const std::vector<uint8_t>& rgb,
                           double cell_edge) {
    ScanCloud c;
    const size_t n = xyz.size() / 3;
    if (n == 0) return c;
    double lo[3], hi[3];
    for (int a = 0; a < 3; a++) {
        lo[a] = std::numeric_limits<double>::infinity();
        hi[a] = -lo[a];
    }
    for (size_t i = 0; i < n; i++)
        for (int a = 0; a < 3; a++) {
            lo[a] = std::min(lo[a], xyz[i * 3 + a]);
            hi[a] = std::max(hi[a], xyz[i * 3 + a]);
        }
    for (int a = 0; a < 3; a++) c.origin[a] = 0.5 * (lo[a] + hi[a]);

    constexpr uint64_t kAxis = (uint64_t(1) << 21) - 1;
    std::vector<std::pair<uint64_t, uint32_t>> order(n);
    for (size_t i = 0; i < n; i++) {
        uint64_t k = 0;
        for (int a = 0; a < 3; a++)
            k = (k << 21) | std::min(kAxis, (uint64_t)((xyz[i * 3 + a] - lo[a]) / cell_edge));
        order[i] = {k, (uint32_t)i};
    }
    std::sort(order.begin(), order.end());
    c.xyz.resize(n * 3);
    c.rgb.resize(n * 3);
    for (size_t j = 0; j < n; j++) {
        const size_t i = order[j].second;
        for (int a = 0; a < 3; a++) {
            c.xyz[j * 3 + a] = (float)(xyz[i * 3 + a] - c.origin[a]);
            c.rgb[j * 3 + a] = rgb[i * 3 + a];
        }
    }
    for (size_t b = 0; b < n;) {
        size_t e = b;
        float mn[3] = {kNone, kNone, kNone}, mx[3] = {-kNone, -kNone, -kNone};
        for (; e < n && order[e].first == order[b].first; e++)
            for (int a = 0; a < 3; a++) {
                mn[a] = std::min(mn[a], c.xyz[e * 3 + a]);
                mx[a] = std::max(mx[a], c.xyz[e * 3 + a]);
            }
        ScanCloud::Cell cell;
        cell.begin = (uint32_t)b;
        cell.end = (uint32_t)e;
        float r2 = 0;
        for (int a = 0; a < 3; a++) {
            cell.center[a] = 0.5f * (mn[a] + mx[a]);
            r2 += 0.25f * (mx[a] - mn[a]) * (mx[a] - mn[a]);
        }
        cell.radius = std::sqrt(r2);
        // A surface crossing a cube of edge `e` holds ~e^2 of area.
        cell.spacing = (float)(cell_edge / std::sqrt((double)(e - b)));
        c.cells.push_back(cell);
        b = e;
    }
    return c;
}

void render_front(const ScanCloud& cloud, const MapCamera& cam, bool ray_depth,
                  std::vector<uint32_t>& index, std::vector<float>& depth) {
    const int W = cam.width, H = cam.height;
    index.assign((size_t)W * H, UINT32_MAX);
    std::vector<float> best((size_t)W * H, kNone);

    // p_cam = R^T (origin + local - t)
    const double* M = cam.c2w;
    double R[9], b[3], d[3];
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) R[r * 3 + c] = M[c * 4 + r];
    for (int a = 0; a < 3; a++) d[a] = cloud.origin[a] - M[a * 4 + 3];
    for (int r = 0; r < 3; r++) b[r] = R[r * 3] * d[0] + R[r * 3 + 1] * d[1] + R[r * 3 + 2] * d[2];

    const bool pinhole = cam.model == (int)CameraModelType::PINHOLE;
    const bool wraps = cam.model == (int)CameraModelType::EQUIRECTANGULAR;
    const double tx = std::max(cam.cx, W - cam.cx) / cam.fx;
    const double ty = std::max(cam.cy, H - cam.cy) / cam.fy;
    const double sx = std::sqrt(1 + tx * tx), sy = std::sqrt(1 + ty * ty);
    for (const ScanCloud::Cell& cell : cloud.cells) {
        if (pinhole) {
            const float* q = cell.center;
            double c[3];
            for (int r = 0; r < 3; r++)
                c[r] = R[r * 3] * q[0] + R[r * 3 + 1] * q[1] + R[r * 3 + 2] * q[2] + b[r];
            const double rad = cell.radius;
            if (c[2] < -rad) continue;
            if (c[0] - tx * c[2] > rad * sx || -c[0] - tx * c[2] > rad * sx) continue;
            if (c[1] - ty * c[2] > rad * sy || -c[1] - ty * c[2] > rad * sy) continue;
        }
        for (uint32_t i = cell.begin; i < cell.end; i++) {
            const float* q = &cloud.xyz[(size_t)i * 3];
            double p[3];
            for (int r = 0; r < 3; r++)
                p[r] = R[r * 3] * q[0] + R[r * 3 + 1] * q[1] + R[r * 3 + 2] * q[2] + b[r];
            if (pinhole && p[2] < 0.01) continue;
            double uv[2];
            if (!camhost::project_ray(p, cam.model, (int)CameraDistortionType::None,
                                      kNoDistortion, uv))
                continue;
            const double px = cam.fx * uv[0] + cam.cx, py = cam.fy * uv[1] + cam.cy;
            if (!(px >= 0 && py >= 0 && px < W && py < H)) continue;
            const double range = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
            const float dist = ray_depth ? (float)range : (float)p[2];
            const int r = std::min(kMaxFootprint,
                                   (int)(0.5 * cam.fx * cell.spacing / range));
            const int x0 = (int)px, y0 = (int)py;
            for (int y = std::max(0, y0 - r); y <= std::min(H - 1, y0 + r); y++)
                for (int dx = -r; dx <= r; dx++) {
                    int x = x0 + dx;
                    if (wraps) x = (x % W + W) % W;
                    else if (x < 0 || x >= W) continue;
                    const size_t k = (size_t)y * W + (size_t)x;
                    if (dist < best[k]) {
                        best[k] = dist;
                        index[k] = i;
                    }
                }
        }
    }
    depth.resize(best.size());
    for (size_t k = 0; k < best.size(); k++) depth[k] = best[k] == kNone ? 0.0f : best[k];
}

void render_depth_normal(const ScanCloud& cloud, const MapCamera& cam, bool ray_depth,
                         std::vector<float>& depth, std::vector<float>& normal) {
    std::vector<uint32_t> index;
    render_front(cloud, cam, ray_depth, index, depth);
    const int W = cam.width, H = cam.height;
    const bool wraps = cam.model == (int)CameraModelType::EQUIRECTANGULAR;
    const int r = std::max(1, (int)std::lround(std::max(W, H) / 800.0));
    auto col = [&](int x) { return wraps ? (x % W + W) % W : std::clamp(x, 0, W - 1); };
    auto at = [&](const std::vector<float>& m, int x, int y) {
        return m[(size_t)std::clamp(y, 0, H - 1) * W + col(x)];
    };

    // Nearest depth over the window, 0 counted as nothing, rows then columns.
    std::vector<float> rows((size_t)W * H), nearest((size_t)W * H);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float m = kNone;
            for (int k = -r; k <= r; k++) {
                const float v = at(depth, x + k, y);
                if (v > 0) m = std::min(m, v);
            }
            rows[(size_t)y * W + x] = m;
        }
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            float m = kNone;
            for (int k = -r; k <= r; k++) m = std::min(m, at(rows, x, y + k));
            nearest[(size_t)y * W + x] = m;
        }
    for (size_t k = 0; k < depth.size(); k++)
        if (depth[k] > nearest[k] * kBleedRatio + kBleedSlack) depth[k] = 0;

    for (int pass = 0; pass < kFillPasses; pass++) {
        std::vector<float> filled = depth;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                if (depth[(size_t)y * W + x] > 0) continue;
                float lo = kNone, hi = 0, sum = 0;
                int n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (y + dy < 0 || y + dy >= H) continue;
                        const float v = at(depth, x + dx, y + dy);
                        if (!(v > 0)) continue;
                        lo = std::min(lo, v);
                        hi = std::max(hi, v);
                        sum += v;
                        n++;
                    }
                if (n >= kFillNeighbours && hi <= lo * kFillSpread)
                    filled[(size_t)y * W + x] = sum / n;
            }
        depth.swap(filled);
    }

    std::vector<float> rays((size_t)W * H * 3);
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++) {
            double ray[3] = {0, 0, 0};
            camhost::generate_ray((x + 0.5 - cam.cx) / cam.fx, (y + 0.5 - cam.cy) / cam.fy,
                                  cam.model, (int)CameraDistortionType::None, kNoDistortion, ray);
            // Planar depth scales the ray to z = 1.
            const double s = ray_depth ? 1.0 : (ray[2] > 1e-6 ? 1.0 / ray[2] : 0.0);
            for (int a = 0; a < 3; a++) rays[((size_t)y * W + x) * 3 + a] = (float)(ray[a] * s);
        }
    auto point = [&](int x, int y, float out[3]) {
        const size_t k = (size_t)y * W + col(x);
        for (int a = 0; a < 3; a++) out[a] = rays[k * 3 + a] * depth[k];
    };
    normal.assign((size_t)W * H * 3, 0.0f);
    for (int y = r; y < H - r; y++)
        for (int x = wraps ? 0 : r; x < (wraps ? W : W - r); x++) {
            const float d0 = depth[(size_t)y * W + x];
            if (!(d0 > 0)) continue;
            const float ds[4] = {at(depth, x - r, y), at(depth, x + r, y),
                                 at(depth, x, y - r), at(depth, x, y + r)};
            bool ok = true;
            for (float v : ds) ok = ok && v > 0 && std::fabs(v - d0) <= kNormalSpread * d0;
            if (!ok) continue;
            float p[4][3], c[3];
            point(x - r, y, p[0]);
            point(x + r, y, p[1]);
            point(x, y - r, p[2]);
            point(x, y + r, p[3]);
            point(x, y, c);
            const float dx[3] = {p[1][0] - p[0][0], p[1][1] - p[0][1], p[1][2] - p[0][2]};
            const float dy[3] = {p[2][0] - p[3][0], p[2][1] - p[3][1], p[2][2] - p[3][2]};
            float n[3] = {dx[1] * dy[2] - dx[2] * dy[1], dx[2] * dy[0] - dx[0] * dy[2],
                          dx[0] * dy[1] - dx[1] * dy[0]};
            const float len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
            if (!(len > 0)) continue;
            const float face = n[0] * c[0] + n[1] * c[1] + n[2] * c[2] > 0 ? -1.0f : 1.0f;
            for (int a = 0; a < 3; a++) normal[((size_t)y * W + x) * 3 + a] = face * n[a] / len;
        }
}

}  // namespace app
