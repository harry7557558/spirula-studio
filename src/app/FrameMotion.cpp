// FrameMotion.cpp -- see app/FrameMotion.h.

#include "app/FrameMotion.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <thread>

namespace app {

namespace {

constexpr double kPi = 3.14159265358979323846;

// Residual flow weighs this much against content leaving the view. A tenth of
// the frame of unexplained disparity is a harder match than a tenth of the
// frame of pan, and carries the triangulation the pan does not.
constexpr float kParallaxWeight = 2.0f;

constexpr int kWin = 5;                      // 11x11 tracking window
constexpr int kWinArea = (2 * kWin + 1) * (2 * kWin + 1);
constexpr int kRansac = 64;

// ---------------------------------------------------------------------------
// Grey pyramids and Lucas-Kanade
// ---------------------------------------------------------------------------

struct Pyramid {
    std::vector<std::vector<uint8_t>> level;
    std::vector<int> w, h;
};

void build_pyramid(const uint8_t* src, int w, int h, Pyramid& p) {
    p.w.assign(1, w);
    p.h.assign(1, h);
    p.level.resize(1);
    p.level[0].assign(src, src + (size_t)w * h);
    while (p.level.size() < 5 && p.w.back() >= 64 && p.h.back() >= 64) {
        const int pw = p.w.back();
        const int nw = pw / 2, nh = p.h.back() / 2;
        p.level.resize(p.level.size() + 1);
        const std::vector<uint8_t>& up = p.level[p.level.size() - 2];
        std::vector<uint8_t>& d = p.level.back();
        d.resize((size_t)nw * nh);
        for (int y = 0; y < nh; y++)
            for (int x = 0; x < nw; x++) {
                const uint8_t* q = up.data() + (size_t)(2 * y) * pw + 2 * x;
                d[(size_t)y * nw + x] =
                    (uint8_t)((q[0] + q[1] + q[pw] + q[pw + 1] + 2) / 4);
            }
        p.w.push_back(nw);
        p.h.push_back(nh);
    }
    p.level.resize(p.w.size());
}

float sample(const uint8_t* im, int w, int h, float x, float y) {
    x = std::min(std::max(x, 0.0f), (float)w - 1.001f);
    y = std::min(std::max(y, 0.0f), (float)h - 1.001f);
    const int x0 = (int)x, y0 = (int)y;
    const float ax = x - x0, ay = y - y0;
    const uint8_t* p = im + (size_t)y0 * w + x0;
    const float t = p[0] + (p[1] - p[0]) * ax;
    const float b = p[w] + (p[w + 1] - p[w]) * ax;
    return t + (b - t) * ay;
}

// The displacement of one point from `a` to `b`, coarse to fine. Inverse
// compositional: the normal matrix comes from `a` and is built once per level.
bool track_point(const Pyramid& a, const Pyramid& b, float x, float y,
                 float& dx, float& dy) {
    float gx[kWinArea], gy[kWinArea], ref[kWinArea];
    float d0 = 0, d1 = 0, err = 0;
    bool fitted = false;
    const int top = (int)a.level.size() - 1;
    for (int L = top; L >= 0; --L) {
        if (L != top) {
            d0 *= 2;
            d1 *= 2;
        }
        const int w = a.w[(size_t)L], h = a.h[(size_t)L];
        const float sc = 1.0f / (float)(1 << L);
        const float px = x * sc, py = y * sc;
        if (px < kWin + 2 || py < kWin + 2 || px > w - kWin - 3 ||
            py > h - kWin - 3)
            continue;
        const uint8_t* ia = a.level[(size_t)L].data();
        const uint8_t* ib = b.level[(size_t)L].data();
        float gxx = 0, gxy = 0, gyy = 0;
        int k = 0;
        for (int j = -kWin; j <= kWin; j++)
            for (int i = -kWin; i <= kWin; i++, k++) {
                const float sx = px + i, sy = py + j;
                gx[k] = 0.5f * (sample(ia, w, h, sx + 1, sy) -
                                sample(ia, w, h, sx - 1, sy));
                gy[k] = 0.5f * (sample(ia, w, h, sx, sy + 1) -
                                sample(ia, w, h, sx, sy - 1));
                ref[k] = sample(ia, w, h, sx, sy);
                gxx += gx[k] * gx[k];
                gxy += gx[k] * gy[k];
                gyy += gy[k] * gy[k];
            }
        const float det = gxx * gyy - gxy * gxy;
        const float lo =
            0.5f * (gxx + gyy -
                    std::sqrt(std::max(0.0f, (gxx - gyy) * (gxx - gyy) +
                                                 4.0f * gxy * gxy)));
        if (det <= 1e-6f || lo < 4.0f * kWinArea) continue;

        for (int it = 0; it < 8; it++) {
            float bx = 0, by = 0;
            err = 0;
            k = 0;
            for (int j = -kWin; j <= kWin; j++)
                for (int i = -kWin; i <= kWin; i++, k++) {
                    const float di =
                        ref[k] - sample(ib, w, h, px + i + d0, py + j + d1);
                    bx += gx[k] * di;
                    by += gy[k] * di;
                    err += std::fabs(di);
                }
            const float n0 = (gyy * bx - gxy * by) / det;
            const float n1 = (gxx * by - gxy * bx) / det;
            d0 += n0;
            d1 += n1;
            if (n0 * n0 + n1 * n1 < 0.01f) break;
        }
        fitted = true;
    }
    if (!fitted || err / kWinArea > 22.0f) return false;
    dx = d0;
    dy = d1;
    return true;
}

// ---------------------------------------------------------------------------
// Global models
// ---------------------------------------------------------------------------

// b = A * [a 1], by least squares. False when the points are collinear.
bool solve_affine(const std::vector<float>& ax, const std::vector<float>& ay,
                  const std::vector<float>& bx, const std::vector<float>& by,
                  const int* pick, int n, float A[6]) {
    double m[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    double ru[3] = {0, 0, 0}, rv[3] = {0, 0, 0};
    for (int k = 0; k < n; k++) {
        const size_t i = (size_t)(pick ? pick[k] : k);
        const double x = ax[i], y = ay[i], u = bx[i], v = by[i];
        const double t[3] = {x, y, 1.0};
        for (int r = 0; r < 3; r++) {
            for (int c = 0; c < 3; c++) m[r][c] += t[r] * t[c];
            ru[r] += u * t[r];
            rv[r] += v * t[r];
        }
    }
    const double det =
        m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
        m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
        m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if (std::fabs(det) < 1e-9) return false;
    double inv[3][3];
    inv[0][0] = (m[1][1] * m[2][2] - m[1][2] * m[2][1]) / det;
    inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) / det;
    inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) / det;
    inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) / det;
    inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) / det;
    inv[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) / det;
    inv[2][0] = (m[1][0] * m[2][1] - m[1][1] * m[2][0]) / det;
    inv[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) / det;
    inv[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) / det;
    for (int r = 0; r < 3; r++) {
        A[r] = (float)(inv[r][0] * ru[0] + inv[r][1] * ru[1] + inv[r][2] * ru[2]);
        A[3 + r] = (float)(inv[r][0] * rv[0] + inv[r][1] * rv[1] + inv[r][2] * rv[2]);
    }
    return true;
}

void apply_affine(const float A[6], float x, float y, float& u, float& v) {
    u = A[0] * x + A[1] * y + A[2];
    v = A[3] * x + A[4] * y + A[5];
}

// A convex polygon of at most eight corners: the frame, or the frame carried
// over by the fitted model and cut back to it.
struct Poly {
    float x[12], y[12];
    int n = 0;
};

float poly_area(const Poly& p) {
    double a = 0;
    for (int i = 0, j = p.n - 1; i < p.n; j = i++)
        a += (double)p.x[j] * p.y[i] - (double)p.x[i] * p.y[j];
    return (float)std::fabs(a) * 0.5f;
}

Poly clip_to_frame(const Poly& in, float w, float h) {
    auto side = [&](int e, float x, float y) -> float {
        if (e == 0) return x;
        if (e == 1) return w - x;
        if (e == 2) return y;
        return h - y;
    };
    Poly a = in, b;
    for (int e = 0; e < 4 && a.n > 0; e++) {
        b.n = 0;
        for (int i = 0, j = a.n - 1; i < a.n; j = i++) {
            const float dj = side(e, a.x[j], a.y[j]);
            const float di = side(e, a.x[i], a.y[i]);
            if ((di >= 0) != (dj >= 0) && b.n < 12) {
                const float t = dj / (dj - di);
                b.x[b.n] = a.x[j] + t * (a.x[i] - a.x[j]);
                b.y[b.n] = a.y[j] + t * (a.y[i] - a.y[j]);
                b.n++;
            }
            if (di >= 0 && b.n < 12) {
                b.x[b.n] = a.x[i];
                b.y[b.n] = a.y[i];
                b.n++;
            }
        }
        a = b;
    }
    return a;
}

// What the frame carried over by `A` leaves behind: the share of it that left
// the frame, or of the frame it no longer covers, whichever is larger. A camera
// backing away loses nothing and still sees a scene it has not seen.
float model_coverage(const float A[6], float w, float h) {
    Poly moved;
    for (int k = 0; k < 4; k++) {
        const float x = (k == 1 || k == 2) ? w : 0.0f;
        const float y = (k >= 2) ? h : 0.0f;
        apply_affine(A, x, y, moved.x[k], moved.y[k]);
    }
    moved.n = 4;
    const float a_moved = poly_area(moved);
    const float a_shared = poly_area(clip_to_frame(moved, w, h));
    const float lost = a_moved > 0 ? 1.0f - a_shared / a_moved : 1.0f;
    const float fresh = 1.0f - a_shared / (w * h);
    return std::min(1.0f, std::max(lost, fresh));
}

struct Vec3 {
    float x = 0, y = 0, z = 0;
};

Vec3 normalize(Vec3 v) {
    const float n = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (n <= 0) return {0, 0, 1};
    return {v.x / n, v.y / n, v.z / n};
}

Vec3 mul(const float R[9], const Vec3& v) {
    return {R[0] * v.x + R[1] * v.y + R[2] * v.z,
            R[3] * v.x + R[4] * v.y + R[5] * v.z,
            R[6] * v.x + R[7] * v.y + R[8] * v.z};
}

float angle_between(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    const float c = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
    return 2.0f * std::asin(std::min(1.0f, c));
}

// The rotation two correspondences fix exactly: each pair spans an orthonormal
// frame, and one frame carried onto the other is the rotation.
bool rotation_from_two(const Vec3& a0, const Vec3& a1, const Vec3& b0,
                       const Vec3& b1, float R[9]) {
    auto frame = [](const Vec3& u, const Vec3& v, float F[9]) {
        const Vec3 e0 = normalize(u);
        const float d = e0.x * v.x + e0.y * v.y + e0.z * v.z;
        const Vec3 t{v.x - d * e0.x, v.y - d * e0.y, v.z - d * e0.z};
        if (t.x * t.x + t.y * t.y + t.z * t.z < 1e-6f) return false;
        const Vec3 e1 = normalize(t);
        const Vec3 e2{e0.y * e1.z - e0.z * e1.y, e0.z * e1.x - e0.x * e1.z,
                      e0.x * e1.y - e0.y * e1.x};
        F[0] = e0.x; F[1] = e1.x; F[2] = e2.x;
        F[3] = e0.y; F[4] = e1.y; F[5] = e2.y;
        F[6] = e0.z; F[7] = e1.z; F[8] = e2.z;
        return true;
    };
    float Fa[9], Fb[9];
    if (!frame(a0, a1, Fa) || !frame(b0, b1, Fb)) return false;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) {
            float s = 0;
            for (int k = 0; k < 3; k++) s += Fb[3 * r + k] * Fa[3 * c + k];
            R[3 * r + c] = s;
        }
    return true;
}

// Horn's quaternion fit over the inliers, its largest eigenvector reached by
// shifted power iteration -- the matrix is 4x4, so an eigen solver would be
// more code than the thirty multiplies this is.
void rotation_from_many(const std::vector<Vec3>& a, const std::vector<Vec3>& b,
                        const std::vector<int>& pick, float R[9]) {
    double S[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
    for (int i : pick) {
        const float av[3] = {a[(size_t)i].x, a[(size_t)i].y, a[(size_t)i].z};
        const float bv[3] = {b[(size_t)i].x, b[(size_t)i].y, b[(size_t)i].z};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) S[r][c] += (double)av[r] * bv[c];
    }
    const double N[4][4] = {
        {S[0][0] + S[1][1] + S[2][2], S[1][2] - S[2][1], S[2][0] - S[0][2],
         S[0][1] - S[1][0]},
        {S[1][2] - S[2][1], S[0][0] - S[1][1] - S[2][2], S[0][1] + S[1][0],
         S[2][0] + S[0][2]},
        {S[2][0] - S[0][2], S[0][1] + S[1][0], -S[0][0] + S[1][1] - S[2][2],
         S[1][2] + S[2][1]},
        {S[0][1] - S[1][0], S[2][0] + S[0][2], S[1][2] + S[2][1],
         -S[0][0] - S[1][1] + S[2][2]}};
    double shift = 0;
    for (int r = 0; r < 4; r++) {
        double row = 0;
        for (int c = 0; c < 4; c++) row += std::fabs(N[r][c]);
        shift = std::max(shift, row);
    }
    double q[4] = {1, 0, 0, 0};
    for (int it = 0; it < 40; it++) {
        double n[4];
        for (int r = 0; r < 4; r++) {
            n[r] = shift * q[r];
            for (int c = 0; c < 4; c++) n[r] += N[r][c] * q[c];
        }
        const double len =
            std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] + n[3] * n[3]);
        if (len < 1e-12) break;
        for (int r = 0; r < 4; r++) q[r] = n[r] / len;
    }
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    R[0] = (float)(1 - 2 * (y * y + z * z));
    R[1] = (float)(2 * (x * y - w * z));
    R[2] = (float)(2 * (x * z + w * y));
    R[3] = (float)(2 * (x * y + w * z));
    R[4] = (float)(1 - 2 * (x * x + z * z));
    R[5] = (float)(2 * (y * z - w * x));
    R[6] = (float)(2 * (x * z - w * y));
    R[7] = (float)(2 * (y * z + w * x));
    R[8] = (float)(1 - 2 * (x * x + y * y));
}

uint32_t next_random(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

float percentile(std::vector<float>& v, float p) {
    if (v.empty()) return 0.0f;
    const size_t k = (size_t)std::max(
        0.0, std::min((double)v.size() - 1, p * (double)(v.size() - 1)));
    std::nth_element(v.begin(), v.begin() + (ptrdiff_t)k, v.end());
    return v[k];
}

}  // namespace

// ---------------------------------------------------------------------------
// The tracker
// ---------------------------------------------------------------------------

struct MotionTracker::Impl {
    MotionOptions o;
    float diag = 1.0f;
    std::vector<float> gx, gy;      // the grid tracked, in frame pixels
    Pyramid prev, cur;
    bool primed = false;
    std::vector<float> cost;
    std::vector<MotionStep> step;
    std::vector<int64_t> end;
    int weak = 0;
    bool done = false;

    struct Pair {
        std::vector<float> ax, ay, bx, by;
    };

    bool sphere() const { return o.view != MotionView::Planar; }
    float rad_per_pixel() const;
    bool to_direction(float x, float y, Vec3& d) const;
    float step_cost(const Pair& p, MotionStep& out) const;
    float sphere_cost(const Pair& p) const;
    float planar_cost(const Pair& p, MotionStep& out) const;
};

// How far apart two neighbouring grey pixels point, which is the floor every
// angular threshold here has to clear.
float MotionTracker::Impl::rad_per_pixel() const {
    if (o.view == MotionView::Packed360) {
        const float face = (float)o.width * (float)o.eac.face /
                           std::max(1.0f, (float)o.eac.track_w);
        return (float)(kPi * 0.5) / std::max(face, 1.0f);
    }
    return 0.5f * o.circle_fov / std::max(o.circle_r, 1.0f);
}

bool MotionTracker::Impl::to_direction(float x, float y, Vec3& d) const {
    if (o.view == MotionView::Packed360) {
        // The grey frame is the whole track scaled down, strips and all, so
        // the layout's own coordinates are what the mapping wants back.
        const float sx = x * (float)o.eac.track_w / (float)o.width;
        const float sy = y * (float)o.eac.track_h / (float)o.height;
        float v[3];
        if (!pano360_direction(o.eac, 0, sx, sy, v)) return false;
        d = {v[0], v[1], v[2]};
        return true;
    }
    const float rx = x - o.circle_x, ry = y - o.circle_y;
    const float r = std::sqrt(rx * rx + ry * ry) / std::max(o.circle_r, 1.0f);
    if (r > 1.0f) return false;
    const float th = r * 0.5f * o.circle_fov, ph = std::atan2(ry, rx);
    d = {std::sin(th) * std::cos(ph), std::sin(th) * std::sin(ph), std::cos(th)};
    return true;
}

// Turning a camera that already sees every direction shows nothing new, so
// what the rotation leaves behind is the whole of the cost.
float MotionTracker::Impl::sphere_cost(const Pair& p) const {
    const int n = (int)p.ax.size();
    std::vector<Vec3> a, b;
    a.reserve((size_t)n);
    b.reserve((size_t)n);
    for (int i = 0; i < n; i++) {
        Vec3 da, db;
        const size_t k = (size_t)i;
        if (!to_direction(p.ax[k], p.ay[k], da)) continue;
        if (!to_direction(p.bx[k], p.by[k], db)) continue;
        a.push_back(da);
        b.push_back(db);
    }
    const int m = (int)a.size();
    if (m < 16) return -1.0f;

    const float thr = 3.0f * rad_per_pixel();
    uint32_t rng = 0x85ebca6bu;
    std::vector<int> inliers, trial;
    float best[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    int best_in = 0;
    for (int it = 0; it < kRansac; it++) {
        const int i0 = (int)(next_random(rng) % (uint32_t)m);
        const int i1 = (int)(next_random(rng) % (uint32_t)m);
        float R[9];
        if (i0 == i1 || !rotation_from_two(a[(size_t)i0], a[(size_t)i1],
                                           b[(size_t)i0], b[(size_t)i1], R))
            continue;
        trial.clear();
        for (int i = 0; i < m; i++)
            if (angle_between(mul(R, a[(size_t)i]), b[(size_t)i]) < thr)
                trial.push_back(i);
        if ((int)trial.size() > best_in) {
            best_in = (int)trial.size();
            inliers = trial;
            std::memcpy(best, R, sizeof best);
        }
    }
    if (best_in < 8) return -1.0f;
    rotation_from_many(a, b, inliers, best);

    std::vector<float> res;
    res.reserve((size_t)m);
    const float clamp = 0.35f * std::max(o.out_fov, 0.25f);
    for (int i = 0; i < m; i++)
        res.push_back(std::min(
            clamp, angle_between(mul(best, a[(size_t)i]), b[(size_t)i])));
    return kParallaxWeight * percentile(res, 0.75f) /
           std::max(o.out_fov, 0.25f);
}

float MotionTracker::Impl::planar_cost(const Pair& p, MotionStep& out) const {
    const int n = (int)p.ax.size();
    if (n < 16) return -1.0f;
    const float thr = 0.004f * diag + 0.5f;
    uint32_t rng = 0x9e3779b9u;
    float best[6] = {1, 0, 0, 0, 1, 0};
    int best_in = 0;
    std::vector<int> inliers, trial;
    for (int it = 0; it < kRansac; it++) {
        int pick[3];
        for (int k = 0; k < 3; k++)
            pick[k] = (int)(next_random(rng) % (uint32_t)n);
        float A[6];
        if (!solve_affine(p.ax, p.ay, p.bx, p.by, pick, 3, A)) continue;
        trial.clear();
        for (int i = 0; i < n; i++) {
            float u, v;
            apply_affine(A, p.ax[(size_t)i], p.ay[(size_t)i], u, v);
            const float ex = u - p.bx[(size_t)i], ey = v - p.by[(size_t)i];
            if (ex * ex + ey * ey < thr * thr) trial.push_back(i);
        }
        if ((int)trial.size() > best_in) {
            best_in = (int)trial.size();
            inliers = trial;
            std::memcpy(best, A, sizeof best);
        }
    }
    if (best_in < 8) return -1.0f;
    float A[6];
    if (solve_affine(p.ax, p.ay, p.bx, p.by, inliers.data(),
                     (int)inliers.size(), A))
        std::memcpy(best, A, sizeof best);

    std::vector<float> res;
    res.reserve((size_t)n);
    const float clamp = 0.35f * diag;
    for (int i = 0; i < n; i++) {
        float u, v;
        apply_affine(best, p.ax[(size_t)i], p.ay[(size_t)i], u, v);
        const float ex = u - p.bx[(size_t)i], ey = v - p.by[(size_t)i];
        res.push_back(std::min(clamp, std::sqrt(ex * ex + ey * ey)));
    }
    const float parallax = percentile(res, 0.75f) / diag;

    const float w = (float)o.width, h = (float)o.height;
    // The same affine over a unit-square frame, which is what a plan composes.
    out.turn[0] = best[0];
    out.turn[1] = best[1] * h / w;
    out.turn[2] = best[2] / w;
    out.turn[3] = best[3] * w / h;
    out.turn[4] = best[4];
    out.turn[5] = best[5] / h;
    out.drift = kParallaxWeight * parallax;
    return model_coverage(best, w, h) + out.drift;
}

float MotionTracker::Impl::step_cost(const Pair& p, MotionStep& out) const {
    if (!sphere()) return planar_cost(p, out);
    // A rotation of the sphere carries no coverage, so the whole of the cost
    // is a residual and the step composes to nothing.
    const float c = sphere_cost(p);
    out.drift = c > 0 ? c : 0.0f;
    return c;
}

MotionTracker::MotionTracker(const MotionOptions& options) : impl_(new Impl{}) {
    Impl& s = *impl_;
    s.o = options;
    if (s.o.circle_r <= 0) {
        s.o.circle_x = 0.5f * (float)s.o.width;
        s.o.circle_y = 0.5f * (float)s.o.height;
        s.o.circle_r = 0.5f * (float)std::min(s.o.width, s.o.height);
    }
    s.diag = std::sqrt((float)(s.o.width * s.o.width + s.o.height * s.o.height));
    // About 500 points: enough for a stable fit, few enough that a step costs
    // a millisecond or two of one core.
    const int cols = std::max(8, (int)std::lround(s.o.width / 18.0));
    const int rows = std::max(8, (int)std::lround(s.o.height / 18.0));
    for (int j = 0; j < rows; j++)
        for (int i = 0; i < cols; i++) {
            s.gx.push_back((i + 0.5f) / (float)cols * (float)s.o.width);
            s.gy.push_back((j + 0.5f) / (float)rows * (float)s.o.height);
        }
}

MotionTracker::~MotionTracker() = default;

void MotionTracker::track(const uint8_t* gray, int64_t index) {
    Impl& s = *impl_;
    if (s.o.width <= 0 || s.o.height <= 0 || s.done) return;
    build_pyramid(gray, s.o.width, s.o.height, s.cur);
    if (s.primed) {
        // The points are independent and are most of the cost of a step, so
        // they are split across the machine: the extraction this runs in front
        // of has the GPU busy and the cores idle.
        const size_t n = s.gx.size();
        const int threads = std::max(
            1, std::min((int)std::thread::hardware_concurrency(), (int)(n / 64)));
        std::vector<Impl::Pair> part((size_t)threads);
        const size_t span = (n + threads - 1) / (size_t)threads;
        auto run = [&](int t) {
            Impl::Pair& q = part[(size_t)t];
            for (size_t k = t * span; k < std::min(n, (t + 1) * span); k++) {
                float dx, dy;
                if (!track_point(s.prev, s.cur, s.gx[k], s.gy[k], dx, dy)) continue;
                q.ax.push_back(s.gx[k]);
                q.ay.push_back(s.gy[k]);
                q.bx.push_back(s.gx[k] + dx);
                q.by.push_back(s.gy[k] + dy);
            }
        };
        std::vector<std::thread> pool;
        for (int t = 1; t < threads; t++) pool.emplace_back(run, t);
        run(0);
        for (std::thread& th : pool) th.join();
        Impl::Pair p;
        for (const Impl::Pair& q : part) {
            p.ax.insert(p.ax.end(), q.ax.begin(), q.ax.end());
            p.ay.insert(p.ay.end(), q.ay.begin(), q.ay.end());
            p.bx.insert(p.bx.end(), q.bx.begin(), q.bx.end());
            p.by.insert(p.by.end(), q.by.begin(), q.by.end());
        }
        MotionStep ms;
        const float c = s.step_cost(p, ms);
        if (c < 0) s.weak++;
        s.cost.push_back(c);
        s.step.push_back(ms);
        s.end.push_back(index);
    }
    s.prev.level.swap(s.cur.level);
    s.prev.w = s.cur.w;
    s.prev.h = s.cur.h;
    s.primed = true;
}

void MotionTracker::finish() {
    Impl& s = *impl_;
    if (s.done) return;
    s.done = true;
    // A step nothing could be tracked across is not a still one: the typical
    // step is a better guess than zero, which would hoard frames there.
    std::vector<float> good;
    for (float c : s.cost)
        if (c >= 0) good.push_back(c);
    const float fill = good.empty() ? 0.0f : percentile(good, 0.5f);
    for (size_t i = 0; i < s.cost.size(); i++)
        if (s.cost[i] < 0) {
            s.cost[i] = fill;
            s.step[i] = MotionStep{};   // nothing to compose through
            s.step[i].drift = fill;
        }
}

const std::vector<float>& MotionTracker::costs() const { return impl_->cost; }
const std::vector<MotionStep>& MotionTracker::steps() const { return impl_->step; }
const std::vector<int64_t>& MotionTracker::ends() const { return impl_->end; }
int MotionTracker::weak_steps() const { return impl_->weak; }

float motion_out_fov(const std::vector<Pano360View>& views) {
    if (views.empty()) return 1.5708f;
    const Pano360View& v = views[0];
    if (v.fov <= 0.0f) return 3.14159f;   // equirectangular
    const double focal = v.width * 0.5 / std::tan(v.fov * kPi / 360.0);
    const double diag =
        std::sqrt((double)v.width * v.width + (double)v.height * v.height);
    return (float)(2.0 * std::atan(0.5 * diag / focal));
}

void motion_frame_size(MotionView view, int src_w, int src_h, int& w, int& h) {
    w = h = 0;
    if (src_w <= 0 || src_h <= 0) return;
    const double want = view == MotionView::Planar ? 320.0 * 240.0 : 640.0 * 480.0;
    const double s = std::min(1.0, std::sqrt(want / ((double)src_w * src_h)));
    w = std::max(64, 2 * (int)std::lround(src_w * s * 0.5));
    h = std::max(64, 2 * (int)std::lround(src_h * s * 0.5));
}

// ---------------------------------------------------------------------------
// Planning
// ---------------------------------------------------------------------------

namespace {

// One video's running total of view change, and the two gaps its own rate puts
// bounds on. Built once so that bisecting the step re-walks arithmetic only.
struct PlanTrack {
    const MotionPlanInput* in = nullptr;
    size_t at = 0;                  // which of the caller's inputs this is
    std::vector<double> sum;
    double total = 0;
    int64_t min_gap = 1, max_gap = 1, want = 1;
};

// One number means the same thing in two videos only when the same model
// measured it against the same angle -- and a sphere's, with no coverage term
// in it at all, never means what a flat capture's does.
bool same_scale(const MotionPlanInput& a, const MotionPlanInput& b) {
    return a.view == b.view && std::fabs(a.out_fov - b.out_fov) < 1e-4f;
}

// The steps since the last kept frame, carried forward one at a time. The
// coverage of what they compose to, not the sum of each one's, is what a
// wobble that comes back has to cost nothing.
struct Since {
    double turn[6] = {1, 0, 0, 0, 1, 0};
    double drift = 0;
};

void advance(Since& s, const MotionStep& m) {
    const double b[6] = {s.turn[0], s.turn[1], s.turn[2],
                         s.turn[3], s.turn[4], s.turn[5]};
    s.turn[0] = m.turn[0] * b[0] + m.turn[1] * b[3];
    s.turn[1] = m.turn[0] * b[1] + m.turn[1] * b[4];
    s.turn[2] = m.turn[0] * b[2] + m.turn[1] * b[5] + m.turn[2];
    s.turn[3] = m.turn[3] * b[0] + m.turn[4] * b[3];
    s.turn[4] = m.turn[3] * b[1] + m.turn[4] * b[4];
    s.turn[5] = m.turn[3] * b[2] + m.turn[4] * b[5] + m.turn[5];
    s.drift += m.drift;
}

double since_cost(const Since& s) {
    const float A[6] = {(float)s.turn[0], (float)s.turn[1], (float)s.turn[2],
                        (float)s.turn[3], (float)s.turn[4], (float)s.turn[5]};
    return model_coverage(A, 1.0f, 1.0f) + s.drift;
}

std::vector<int64_t> walk_track(const PlanTrack& t, double step) {
    const MotionPlanInput& in = *t.in;
    const bool compose = in.step.size() == in.cost.size();
    std::vector<int64_t> got;
    int64_t last = -1;
    double last_sum = 0;
    Since cur, prev;            // composed up to sample i, and up to i - 1
    for (size_t i = 0; i < in.cost.size(); i++) {
        prev = cur;
        if (compose) advance(cur, in.step[i]);
        const int64_t at = in.ends[i];
        if (at < in.window - 1) continue;
        size_t pick = i;
        if (last >= 0) {
            if (at - last < t.min_gap) continue;
            const double now = compose ? since_cost(cur) : t.sum[i] - last_sum;
            if (now < step && at - last < t.max_gap) continue;
            // The step falls BETWEEN two samples, and always taking the one
            // past it lands the whole plan late.
            const double before =
                i == 0 ? 0.0
                       : (compose ? since_cost(prev) : t.sum[i - 1] - last_sum);
            if (i > 0 && now > step && in.ends[i - 1] > last &&
                in.ends[i - 1] - last >= t.min_gap &&
                in.ends[i - 1] >= in.window - 1 && now - step > step - before)
                pick = i - 1;
        }
        got.push_back(in.ends[pick]);
        last = in.ends[pick];
        last_sum = t.sum[pick];
        cur = Since{};
        i = pick;   // the sample stepped over is still the next candidate
        if (in.max_frames > 0 && (int)got.size() >= in.max_frames) break;
    }
    return got;
}

}  // namespace

std::vector<std::vector<int64_t>> plan_by_motion(
        const std::vector<MotionPlanInput>& in, float range) {
    std::vector<std::vector<int64_t>> out(in.size());
    if (range < 1.0f) range = 1.0f;

    std::vector<PlanTrack> tracks;
    for (size_t i = 0; i < in.size(); i++) {
        const MotionPlanInput& m = in[i];
        if (m.cost.empty() || m.cost.size() != m.ends.size() || m.skip < 1) continue;
        PlanTrack t;
        t.in = &m;
        t.at = i;
        t.sum.resize(m.cost.size());
        double run = 0;
        for (size_t j = 0; j < m.cost.size(); j++) {
            run += std::max(0.0f, m.cost[j]);
            t.sum[j] = run;
        }
        t.total = run;
        // Never closer than one sharpness window: two windows that overlap can
        // choose the same frame, and one of the two kept frames then vanishes.
        t.min_gap = std::max<int64_t>(std::max(1, m.window),
                                      (int64_t)((double)m.skip / range));
        t.max_gap = std::max<int64_t>(t.min_gap, (int64_t)((double)m.skip * range));
        t.want = std::max<int64_t>(1, m.frames / m.skip);
        if (m.max_frames > 0) t.want = std::min<int64_t>(t.want, m.max_frames);
        tracks.push_back(std::move(t));
    }

    // Each scale is planned against the frames its own inputs would have had,
    // so a budget moves between videos only where the numbers moving it are
    // the same measurement.
    std::vector<bool> done(tracks.size(), false);
    for (size_t first = 0; first < tracks.size(); first++) {
        if (done[first]) continue;
        std::vector<PlanTrack*> pool;
        double total = 0;
        int64_t want = 0;
        for (size_t k = first; k < tracks.size(); k++)
            if (!done[k] && same_scale(*tracks[k].in, *tracks[first].in)) {
                done[k] = true;
                pool.push_back(&tracks[k]);
                total += tracks[k].total;
                want += tracks[k].want;
            }

        auto walk_all = [&](double step) {
            std::vector<std::vector<int64_t>> got(pool.size());
            int64_t n = 0;
            for (size_t k = 0; k < pool.size(); k++) {
                got[k] = walk_track(*pool[k], step);
                n += (int64_t)got[k].size();
            }
            return std::make_pair(n, std::move(got));
        };

        // Bisected rather than total/want, because a burst of motion swallows
        // several steps' budget in one sample and spends one frame of it. The
        // floor stops a still capture being answered with every frame of itself.
        const double floor_step = 0.01;
        double lo = floor_step, hi = std::max(floor_step * 2.0, total);
        auto best = walk_all(lo);
        if (best.first > want) {
            for (int it = 0; it < 32; it++) {
                const double mid = 0.5 * (lo + hi);
                auto got = walk_all(mid);
                if (got.first > want) {
                    lo = mid;
                } else {
                    hi = mid;
                    best = std::move(got);
                }
            }
        }
        for (size_t k = 0; k < pool.size(); k++)
            out[pool[k]->at] = std::move(best.second[k]);
    }
    return out;
}

std::vector<int64_t> plan_by_motion(const std::vector<float>& cost,
                                    const std::vector<int64_t>& ends,
                                    int64_t frames, int skip, int window,
                                    float range, int max_frames) {
    MotionPlanInput in;
    in.cost = cost;
    in.ends = ends;
    in.frames = frames;
    in.skip = skip;
    in.window = window;
    in.max_frames = max_frames;
    std::vector<std::vector<int64_t>> got = plan_by_motion({in}, range);
    return got.empty() ? std::vector<int64_t>() : std::move(got[0]);
}

}  // namespace app
