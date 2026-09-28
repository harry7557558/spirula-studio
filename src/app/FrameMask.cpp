// FrameMask.cpp -- see FrameMask.h.

#include "app/FrameMask.h"

#include "app/FrameLook.h"
#include "core/ExrImage.h"
#include "core/PolygonFill.h"

#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "nn/core/Parallel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <tuple>
#include <utility>

namespace app {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) a++;
    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

bool parse_four(const std::string& s, float v[4]) {
    const char* p = s.c_str();
    for (int i = 0; i < 4; i++) {
        char* end = nullptr;
        v[i] = std::strtof(p, &end);
        if (end == p) return false;
        p = end;
        while (*p == ' ') p++;
        if (i < 3) {
            if (*p != ',') return false;
            p++;
        }
    }
    return trim(p).empty();
}

// Comma-separated floats, any count; false on junk, an empty list or a
// trailing comma.
bool parse_floats(const std::string& s, std::vector<float>& out) {
    out.clear();
    const char* p = s.c_str();
    while (true) {
        char* end = nullptr;
        const float v = std::strtof(p, &end);
        if (end == p) return false;
        out.push_back(v);
        p = end;
        while (*p == ' ') p++;
        if (*p != ',') break;
        p++;
        while (*p == ' ') p++;
    }
    return trim(p).empty();
}

// Appends exactly what vsnprintf would produce, at any length: measures the
// needed size first, so no fixed buffer can truncate a huge float.
void append_printf(std::string& out, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    va_list probe;
    va_copy(probe, args);
    const int n = std::vsnprintf(nullptr, 0, fmt, probe);
    va_end(probe);
    if (n > 0) {
        std::vector<char> buf((size_t)n + 1);
        std::vsnprintf(buf.data(), buf.size(), fmt, args);
        out.append(buf.data(), (size_t)n);
    }
    va_end(args);
}

// ---------------------------------------------------------------------------
// Detection
// ---------------------------------------------------------------------------

struct Gray {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
};

Gray load_gray(const std::string& path) {
    Gray g;
    int comp = 0;
    if (exr::is_exr(path)) {
        exr::Info info;
        exr::Options opt;
        opt.channels = 1;
        if (!exr::decode_srgb8(path, opt, info, g.px).empty()) return Gray{};
        g.w = info.width;
        g.h = info.height;
        return g;
    }
    unsigned char* d = stbi_load(path.c_str(), &g.w, &g.h, &comp, 1);
    if (!d) return Gray{};
    g.px.assign(d, d + (size_t)g.w * g.h);
    stbi_image_free(d);
    // Displayed, not stored: a stencil's shapes were drawn on the picture the
    // way up it is shown, and the border fit has to agree with them.
    turn_pixels(photo_turn(path), 1, g.px, g.w, g.h);
    return g;
}

// |dI/dx| + |dI/dy| on a 2-pixel stencil; the frame border stays 0.
void gradient_magnitude(const Gray& g, std::vector<float>& out) {
    out.assign((size_t)g.w * g.h, 0.0f);
    for (int y = 1; y + 1 < g.h; y++) {
        const uint8_t* row = &g.px[(size_t)y * g.w];
        const uint8_t* up = row - g.w;
        const uint8_t* dn = row + g.w;
        float* o = &out[(size_t)y * g.w];
        for (int x = 1; x + 1 < g.w; x++)
            o[x] = (float)(std::abs((int)row[x + 1] - (int)row[x - 1]) +
                           std::abs((int)dn[x] - (int)up[x]));
    }
}

// Separable running mean, edge-clamped.
void box_blur(std::vector<float>& a, int w, int h, int r) {
    if (r <= 0) return;
    const float inv = 1.0f / (float)(2 * r + 1);
    std::vector<float> tmp((size_t)w * h);
    for (int y = 0; y < h; y++) {
        const float* in = &a[(size_t)y * w];
        float* out = &tmp[(size_t)y * w];
        double sum = 0;
        for (int i = -r; i <= r; i++) sum += in[std::clamp(i, 0, w - 1)];
        for (int x = 0; x < w; x++) {
            out[x] = (float)(sum * inv);
            sum += in[std::clamp(x + r + 1, 0, w - 1)] - in[std::clamp(x - r, 0, w - 1)];
        }
    }
    for (int x = 0; x < w; x++) {
        double sum = 0;
        for (int i = -r; i <= r; i++) sum += tmp[(size_t)std::clamp(i, 0, h - 1) * w + x];
        for (int y = 0; y < h; y++) {
            a[(size_t)y * w + x] = (float)(sum * inv);
            sum += tmp[(size_t)std::clamp(y + r + 1, 0, h - 1) * w + x] -
                   tmp[(size_t)std::clamp(y - r, 0, h - 1) * w + x];
        }
    }
}

float percentile(std::vector<float> v, float q) {
    if (v.empty()) return 0.0f;
    const size_t k = (size_t)((float)(v.size() - 1) * q);
    std::nth_element(v.begin(), v.begin() + (long)k, v.end());
    return v[k];
}

// Pixels, so a circle stays one on a frame that is not square.
struct Circle {
    float cx = 0, cy = 0, r = 0;
};

// Least squares on x^2 + y^2 = a x + b y + c, about the points' mean.
bool fit_circle(const std::vector<float>& xs, const std::vector<float>& ys,
                const std::vector<size_t>& idx, Circle& out) {
    if (idx.size() < 3) return false;
    double mx = 0, my = 0;
    for (size_t i : idx) { mx += xs[i]; my += ys[i]; }
    mx /= (double)idx.size();
    my /= (double)idx.size();
    double M[3][4] = {};
    for (size_t i : idx) {
        const double u = xs[i] - mx, v = ys[i] - my;
        const double row[3] = {u, v, 1.0};
        for (int a = 0; a < 3; a++) {
            for (int b = 0; b < 3; b++) M[a][b] += row[a] * row[b];
            M[a][3] += row[a] * (u * u + v * v);
        }
    }
    for (int c = 0; c < 3; c++) {
        int piv = c;
        for (int r = c + 1; r < 3; r++)
            if (std::fabs(M[r][c]) > std::fabs(M[piv][c])) piv = r;
        if (std::fabs(M[piv][c]) < 1e-9) return false;
        if (piv != c) std::swap(M[piv], M[c]);
        for (int r = 0; r < 3; r++) {
            if (r == c) continue;
            const double f = M[r][c] / M[c][c];
            for (int k = c; k < 4; k++) M[r][k] -= f * M[c][k];
        }
    }
    const double cu = 0.5 * M[0][3] / M[0][0], cv = 0.5 * M[1][3] / M[1][1];
    const double r2 = M[2][3] / M[2][2] + cu * cu + cv * cv;
    if (!(r2 > 0)) return false;
    out = {(float)(mx + cu), (float)(my + cv), (float)std::sqrt(r2)};
    return true;
}

float radial_residual(const Circle& c, float x, float y) {
    return std::hypot(x - c.cx, y - c.cy) / c.r - 1.0f;
}

// Where the valid region ends on each ray out of (cx, cy): the first radius
// followed by `run` invalid pixels. Taking the FIRST transition rather than
// the last valid pixel is what keeps a flare island out in the border from
// dragging the boundary outwards -- the image circle is star-convex about its
// centre and nothing past the first crossing belongs to it.
//
// A ray that leaves the frame while still valid is dropped: the circle runs
// past the frame edge there, which is the normal case for a 360 camera.
void ray_edges(const std::vector<uint8_t>& valid, int w, int h, float cx, float cy,
               int nrays, std::vector<float>& xs, std::vector<float>& ys) {
    const int run = std::max(3, w / 200);
    const float rmax = std::hypot((float)w, (float)h);
    xs.clear();
    ys.clear();
    for (int k = 0; k < nrays; k++) {
        const float th = 6.2831853f * (float)k / (float)nrays;
        const float dx = std::cos(th), dy = std::sin(th);
        int streak = 0;
        float edge = -1.0f;
        for (float r = 0.0f; r < rmax; r += 1.0f) {
            const int x = (int)(cx + dx * r + 0.5f), y = (int)(cy + dy * r + 0.5f);
            if (x < 0 || y < 0 || x >= w || y >= h) break;
            if (valid[(size_t)y * w + x]) {
                streak = 0;
            } else {
                if (streak == 0) edge = r;
                if (++streak >= run) break;
            }
        }
        if (streak >= run && edge > 0.0f) {
            xs.push_back(cx + dx * edge);
            ys.push_back(cy + dy * edge);
        }
    }
}

// Consensus, not a trimmed least-squares fit: on OSV a sky leaves 60-70% of
// the rays wrong, and a fit to all of them never finds the lens again. Black
// (bx, by) scores too -- see `score` -- and `prior` bounds the centre.
bool consensus_circle(const std::vector<float>& xs, const std::vector<float>& ys,
                      const std::vector<float>& bx, const std::vector<float>& by,
                      const float* prior, int w, int h, float tol, Circle& best,
                      std::vector<size_t>& inliers) {
    const size_t n = xs.size();
    if (n < 8) return false;
    const float rmin = 0.2f * (float)std::min(w, h);
    const float rmax = std::hypot((float)w, (float)h);
    // Where radial_centre and a fit to the rays both hold, they agree within
    // 1.7% of the frame (OSV, Insta360, PortalCam); a fit to the half arc a
    // sky leaves was 4.5% out.
    const float reach = 0.025f * (float)std::max(w, h);
    auto plausible = [&](const Circle& c) {
        if (prior && std::hypot(c.cx - prior[0], c.cy - prior[1]) > reach) return false;
        return c.r >= rmin && c.r <= rmax && c.cx >= 0 && c.cy >= 0 &&
               c.cx <= (float)w && c.cy <= (float)h;
    };
    auto collect = [&](const Circle& c, std::vector<size_t>& out) {
        out.clear();
        for (size_t i = 0; i < n; i++)
            if (std::fabs(radial_residual(c, xs[i], ys[i])) <= tol) out.push_back(i);
    };
    uint32_t state = 0x9E3779B9u;   // fixed, so a Look again gives the same answer
    auto next = [&] {
        state = state * 1664525u + 1013904223u;
        return (size_t)(state >> 8) % n;
    };
    // Black begins on a second circle round the same centre, a rim's width
    // out: its densest ring outside `c` counts for it, black inside against.
    // It holds the centre where a lit rim on one side pulls the scene's edge.
    std::vector<float> dist;
    auto score = [&](const Circle& c, size_t on) {
        long inside = 0;
        dist.clear();
        for (size_t i = 0; i < bx.size(); i++) {
            const float d = std::hypot(bx[i] - c.cx, by[i] - c.cy);
            if (d < c.r * (1.0f - tol)) inside++;
            else dist.push_back(d);
        }
        std::sort(dist.begin(), dist.end());
        size_t ring = 0;
        for (size_t a = 0, b = 0; b < dist.size(); b++) {
            while (dist[b] > dist[a] * (1.0f + 2.0f * tol)) a++;
            ring = std::max(ring, b - a + 1);
        }
        return (long)on + (long)ring - 2 * inside;
    };
    std::vector<size_t> pick(3), cur;
    size_t best_n = 0;
    long best_s = 0;
    for (int it = 0; it < 600; it++) {
        pick[0] = next();
        pick[1] = next();
        pick[2] = next();
        if (pick[0] == pick[1] || pick[1] == pick[2] || pick[0] == pick[2]) continue;
        Circle c;
        if (!fit_circle(xs, ys, pick, c) || !plausible(c)) continue;
        collect(c, cur);
        const long sc = score(c, cur.size());
        if (cur.size() >= 8 && (best_n == 0 || sc > best_s)) {
            best_s = sc;
            best_n = cur.size();
            best = c;
            inliers = cur;
        }
    }
    if (best_n < 8) return false;
    for (int it = 0; it < 3; it++) {
        Circle c;
        if (!fit_circle(xs, ys, inliers, c) || !plausible(c)) break;
        collect(c, cur);
        if (cur.size() < 8) break;
        best = c;
        inliers.swap(cur);
    }
    return true;
}

// Where the edges of the frame-averaged picture point: the rim's are all that
// survive averaging, and all radial, even under a sky. Reweighted (Cauchy) least
// squares over the strongest 2%, so a selfie stick's edges fall away.
bool radial_centre(const std::vector<float>& mean, int w, int h, float& cx, float& cy) {
    std::vector<float> gx(mean.size(), 0.0f), gy(mean.size(), 0.0f), m(mean.size(), 0.0f);
    for (int y = 1; y + 1 < h; y++)
        for (int x = 1; x + 1 < w; x++) {
            const size_t p = (size_t)y * w + x;
            gx[p] = mean[p + 1] - mean[p - 1];
            gy[p] = mean[p + w] - mean[p - w];
            m[p] = std::hypot(gx[p], gy[p]);
        }
    const float thr = percentile(m, 0.98f);
    if (!(thr > 0.0f)) return false;
    struct Line { float x, y, tx, ty, w; };
    std::vector<Line> lines;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const size_t p = (size_t)y * w + x;
            if (m[p] <= thr) continue;
            lines.push_back({(float)x, (float)y, -gy[p] / m[p], gx[p] / m[p], m[p]});
        }
    if (lines.size() < 64) return false;
    double c[2] = {0.5 * w, 0.5 * h};
    double scale = 0.05 * w;
    for (int it = 0; it < 10; it++) {
        double A00 = 0, A01 = 0, A11 = 0, b0 = 0, b1 = 0;
        for (const Line& l : lines) {
            const double d = (l.tx * (c[0] - l.x) + l.ty * (c[1] - l.y)) / scale;
            const double wt = l.w / (1.0 + d * d);
            const double proj = l.tx * l.x + l.ty * l.y;
            A00 += wt * l.tx * l.tx;
            A01 += wt * l.tx * l.ty;
            A11 += wt * l.ty * l.ty;
            b0 += wt * l.tx * proj;
            b1 += wt * l.ty * proj;
        }
        const double det = A00 * A11 - A01 * A01;
        if (std::fabs(det) < 1e-9) return false;
        c[0] = (A11 * b0 - A01 * b1) / det;
        c[1] = (A00 * b1 - A01 * b0) / det;
        scale = std::max(0.005 * w, scale * 0.7);
    }
    cx = (float)c[0];
    cy = (float)c[1];
    return cx >= 0 && cy >= 0 && cx <= (float)w && cy <= (float)h;
}

struct CircleFit {
    Circle c;
    float support = 0.0f;   // rays on the circle, fraction of all rays cast
    float rms = 0.0f;       // over those, fraction of the radius
};

// Per ray, where the averaged picture steps down to the lens barrel -- on OSV a
// darker band 5% of the radius wide before the black: the innermost drop within
// 8% inside the deepest and 40% as deep. The deepest alone is often the band's end.
void mean_edges(const std::vector<float>& mean, int w, int h, float cx, float cy,
                float r0, int nrays, std::vector<float>& xs, std::vector<float>& ys) {
    constexpr float kStep = 0.5f;
    constexpr int kSigma = 4, kTaps = 3 * kSigma, kFan = 5;
    float kernel[2 * kTaps + 1], ksum = 0;
    for (int i = -kTaps; i <= kTaps; i++)
        ksum += kernel[i + kTaps] = std::exp(-0.5f * (float)(i * i) / (kSigma * kSigma));
    std::vector<float> v, sm, dv;
    xs.clear();
    ys.clear();
    for (int k = 0; k < nrays; k++) {
        const float th = 6.2831853f * (float)k / (float)nrays;
        const float dx = std::cos(th), dy = std::sin(th);
        const float r_lo = 0.85f * r0;
        v.clear();
        // Across the ray's own slot too: an edge that is a circle survives
        // being averaged along itself, the scene's noise does not.
        for (float r = r_lo; r < 1.08f * r0; r += kStep) {
            float acc = 0;
            int n = 0;
            for (int f = 0; f < kFan; f++) {
                const float a = th + 6.2831853f / (float)nrays * ((float)f / (kFan - 1) - 0.5f);
                const int x = (int)std::lround(cx + std::cos(a) * r);
                const int y = (int)std::lround(cy + std::sin(a) * r);
                if (x < 0 || y < 0 || x >= w || y >= h) continue;
                acc += mean[(size_t)y * w + x];
                n++;
            }
            if (n < kFan) break;
            v.push_back(acc / (float)n);
        }
        const int n = (int)v.size();
        if (n < 20) continue;
        sm.assign(n, 0.0f);
        for (int i = 0; i < n; i++) {
            float acc = 0;
            for (int t = -kTaps; t <= kTaps; t++)
                acc += kernel[t + kTaps] * v[std::clamp(i + t, 0, n - 1)];
            sm[i] = acc / ksum;
        }
        dv.assign(n, 0.0f);
        for (int i = 1; i + 1 < n; i++) dv[i] = 0.5f * (sm[i + 1] - sm[i - 1]);
        int deepest = 3;
        for (int i = 3; i < n - 3; i++)
            if (dv[i] < dv[deepest]) deepest = i;
        if (dv[deepest] > -1.0f) continue;   // luma per half pixel
        int edge = deepest;
        const int band = (int)(0.08f * r0 / kStep);
        for (int i = std::max(3, deepest - band); i < deepest; i++)
            if (dv[i] <= 0.4f * dv[deepest] && dv[i] <= dv[i - 1] && dv[i] <= dv[i + 1]) {
                edge = i;
                break;
            }
        const float r = r_lo + kStep * (float)edge;
        xs.push_back(cx + dx * r);
        ys.push_back(cy + dy * r);
    }
}

// Edges and a fit, three times, re-centring each round. With `mean` the edges
// are its steps (mean_edges); without, where `valid` ends.
bool fit_valid_region(const std::vector<uint8_t>& valid, const std::vector<uint8_t>* lit,
                      const std::vector<float>* mean, const float* prior,
                      int w, int h, int nrays, float tol, CircleFit& out) {
    size_t n_valid = 0;
    double sx = 0, sy = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (valid[(size_t)y * w + x]) { n_valid++; sx += x; sy += y; }
    if (n_valid < (size_t)w * h / 20) return false;
    float cx = (float)(sx / (double)n_valid), cy = (float)(sy / (double)n_valid);
    if (prior) {
        cx = prior[0];
        cy = prior[1];
    }

    std::vector<float> xs, ys, bx, by;
    std::vector<size_t> inl;
    // Where to look for the steps: round the median ray's end, black's if
    // there is any. Fixed, not following the fit -- one round that settles
    // on a ring inside the scene would otherwise walk the window in after it.
    float r0 = 0.5f * (float)std::min(w, h);
    if (mean) {
        const std::vector<uint8_t>& ends = lit ? *lit : valid;
        ray_edges(ends, w, h, cx, cy, nrays, bx, by);
        if (bx.size() < (size_t)nrays / 8 && lit)
            ray_edges(valid, w, h, cx, cy, nrays, bx, by);
        if (bx.size() >= (size_t)nrays / 8) {
            std::vector<float> d(bx.size());
            for (size_t i = 0; i < bx.size(); i++) d[i] = std::hypot(bx[i] - cx, by[i] - cy);
            r0 = percentile(d, 0.5f);
        }
    }
    bool ok = false;
    for (int it = 0; it < 3; it++) {
        if (mean)
            mean_edges(*mean, w, h, cx, cy, r0, nrays, xs, ys);
        else
            ray_edges(valid, w, h, cx, cy, nrays, xs, ys);
        if (xs.size() < (size_t)nrays / 4) break;
        if (lit) ray_edges(*lit, w, h, cx, cy, nrays, bx, by);
        Circle c;
        if (!consensus_circle(xs, ys, bx, by, prior, w, h, tol, c, inl)) break;
        double s = 0;
        for (size_t i : inl) {
            const double r = radial_residual(c, xs[i], ys[i]);
            s += r * r;
        }
        out.c = c;
        out.support = (float)inl.size() / (float)nrays;
        out.rms = (float)std::sqrt(s / (double)inl.size());
        ok = true;
        cx = c.cx;
        cy = c.cy;
    }
    return ok;
}

// How much of the frame past 1.1 r is valid: little for a lens, most of it
// for a circle a pinhole frame happened to support. A lit barrel is inside 1.1 r.
float valid_outside(const std::vector<uint8_t>& valid, int w, int h, const Circle& c) {
    size_t out = 0, hit = 0;
    const float r2 = 1.21f * c.r * c.r;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            const float dx = (float)x + 0.5f - c.cx, dy = (float)y + 0.5f - c.cy;
            if (dx * dx + dy * dy <= r2) continue;
            out++;
            hit += valid[(size_t)y * w + x];
        }
    return out ? (float)hit / (float)out : 0.0f;
}

MaskShape to_shape(const Circle& c, float shrink, int w, int h) {
    MaskShape s;
    s.kind = MaskShape::Kind::Ellipse;
    s.remove = false;
    s.cx = (c.cx + 0.5f) / (float)w;
    s.cy = (c.cy + 0.5f) / (float)h;
    s.rx = c.r / (float)w;
    s.ry = c.r / (float)h;
    return shrink_border(s, shrink);
}

float kept_fraction(const MaskShape& s, int w, int h) {
    FrameMask m;
    m.shapes.push_back(s);
    std::vector<uint8_t> px;
    std::string err;
    if (!rasterize_frame_mask(m, w, h, px, err)) return 1.0f;
    size_t keep = 0;
    for (uint8_t v : px) keep += v ? 1 : 0;
    return (float)keep / (float)std::max<size_t>(px.size(), 1);
}

// A round-capped polyline as the union of capsules, measured in units of the
// stroke's half-width on each axis: the brush becomes a unit circle.
void fill_stroke(const MaskShape& s, int W, int H, uint8_t* out) {
    const size_t n = s.pts.size() / 2;
    if (n == 0 || !(s.rx > 0.0f) || !(s.ry > 0.0f)) return;
    const float rx = s.rx * (float)W, ry = s.ry * (float)H;
    const float kx = 1.0f / rx, ky = 1.0f / ry;
    for (size_t i = 0; i < n; i++) {
        const size_t j = i + 1 < n ? i + 1 : i;
        const float ax = s.pts[2 * i] * (float)W, ay = s.pts[2 * i + 1] * (float)H;
        const float bx = s.pts[2 * j] * (float)W, by = s.pts[2 * j + 1] * (float)H;
        const int x0 = std::max(0, (int)std::floor(std::min(ax, bx) - rx));
        const int x1 = std::min(W - 1, (int)std::ceil(std::max(ax, bx) + rx));
        const int y0 = std::max(0, (int)std::floor(std::min(ay, by) - ry));
        const int y1 = std::min(H - 1, (int)std::ceil(std::max(ay, by) + ry));
        const float dx = (bx - ax) * kx, dy = (by - ay) * ky;
        const float len2 = dx * dx + dy * dy;
        for (int y = y0; y <= y1; y++)
            for (int x = x0; x <= x1; x++) {
                const float qx = ((float)x + 0.5f - ax) * kx, qy = ((float)y + 0.5f - ay) * ky;
                const float t = len2 > 0.0f ? std::clamp((qx * dx + qy * dy) / len2, 0.0f, 1.0f)
                                            : 0.0f;
                const float ex = qx - t * dx, ey = qy - t * dy;
                if (ex * ex + ey * ey <= 1.0f) out[(size_t)y * W + x] = 1;
            }
    }
}

}  // namespace

MaskShape shrink_border(MaskShape shape, float shrink) {
    const float scale = 1.0f - std::clamp(shrink, -0.5f, 0.9f);
    shape.rx *= scale;
    shape.ry *= scale;
    return shape;
}

bool edit_detected_border(FrameStencil& stencil, const BorderDetect& border) {
    if (!stencil.detect_border || !border.found) return false;
    stencil.mask.shapes.insert(stencil.mask.shapes.begin(),
                              shrink_border(border.shape, stencil.shrink));
    stencil.detect_border = false;
    return true;
}

// ---------------------------------------------------------------------------
// Shapes
// ---------------------------------------------------------------------------

bool parse_mask_shapes(const std::string& spec, std::vector<MaskShape>& out,
                       std::string& error) {
    out.clear();
    size_t start = 0;
    while (start <= spec.size()) {
        const size_t sep = spec.find(';', start);
        std::string piece = trim(spec.substr(
            start, sep == std::string::npos ? std::string::npos : sep - start));
        start = sep == std::string::npos ? spec.size() + 1 : sep + 1;
        if (piece.empty()) continue;

        MaskShape s;
        if (piece[0] == '-' || piece[0] == '!') {
            s.remove = true;
            piece = trim(piece.substr(1));
        } else if (piece[0] == '+') {
            piece = trim(piece.substr(1));
        }
        const size_t sp = piece.find_first_of(" \t");
        if (sp == std::string::npos) {
            error = piece;
            return false;
        }
        const std::string kind = piece.substr(0, sp);
        const std::string nums = trim(piece.substr(sp + 1));
        if (kind == "path") {
            s.kind = MaskShape::Kind::Path;
            if (!parse_floats(nums, s.pts) || s.pts.size() < 6 || s.pts.size() % 2) {
                error = piece;
                return false;
            }
            out.push_back(s);
            continue;
        }
        if (kind == "stroke") {
            s.kind = MaskShape::Kind::Stroke;
            if (!parse_floats(nums, s.pts) || s.pts.size() < 4 || s.pts.size() % 2 ||
                !(s.pts[0] > 0.0f) || !(s.pts[1] > 0.0f)) {
                error = piece;
                return false;
            }
            s.rx = s.pts[0];
            s.ry = s.pts[1];
            s.pts.erase(s.pts.begin(), s.pts.begin() + 2);
            out.push_back(s);
            continue;
        }
        float v[4];
        if (!parse_four(nums, v)) {
            error = piece;
            return false;
        }
        if (kind == "ellipse") s.kind = MaskShape::Kind::Ellipse;
        else if (kind == "rect") s.kind = MaskShape::Kind::Rect;
        else { error = piece; return false; }
        s.cx = v[0]; s.cy = v[1]; s.rx = v[2]; s.ry = v[3];
        if (s.kind == MaskShape::Kind::Ellipse && (s.rx <= 0.0f || s.ry <= 0.0f)) {
            error = piece;
            return false;
        }
        out.push_back(s);
    }
    if (out.empty()) {
        error = spec;
        return false;
    }
    return true;
}

std::string format_mask_shapes(const std::vector<MaskShape>& shapes) {
    std::string out;
    char buf[96];
    for (const MaskShape& s : shapes) {
        std::string piece = s.remove ? "-" : "";
        if (s.kind == MaskShape::Kind::Path || s.kind == MaskShape::Kind::Stroke) {
            const bool stroke = s.kind == MaskShape::Kind::Stroke;
            piece += stroke ? "stroke " : "path ";
            if (stroke) append_printf(piece, "%.5f,%.5f", s.rx, s.ry);
            for (size_t i = 0; i < s.pts.size(); i++) {
                std::snprintf(buf, sizeof buf, "%s%.4f", i || stroke ? "," : "", s.pts[i]);
                piece += buf;
            }
        } else {
            append_printf(piece, "%s %.4f,%.4f,%.4f,%.4f",
                          s.kind == MaskShape::Kind::Rect ? "rect" : "ellipse",
                          s.cx, s.cy, s.rx, s.ry);
        }
        if (!out.empty()) out += "; ";
        out += piece;
    }
    return out;
}

bool image_size(const std::string& path, int& width, int& height) {
    int comp = 0;
    if (exr::is_exr(path)) {
        exr::Info info;
        if (!exr::probe(path, info).empty()) return false;
        width = info.width;
        height = info.height;
        return true;
    }
    return stbi_info(path.c_str(), &width, &height, &comp) != 0;
}

bool load_rgb(const std::string& path, int& width, int& height,
              std::vector<uint8_t>& out) {
    int comp = 0;
    if (exr::is_exr(path)) {
        exr::Info info;
        if (!exr::decode_srgb8(path, exr::Options(), info, out).empty()) return false;
        width = info.width;
        height = info.height;
        return true;
    }
    unsigned char* d = stbi_load(path.c_str(), &width, &height, &comp, 3);
    if (!d) return false;
    out.assign(d, d + (size_t)width * height * 3);
    stbi_image_free(d);
    return true;
}

bool load_stencil(const std::string& path, int& width, int& height,
                  std::vector<uint8_t>& out) {
    Gray g = load_gray(path);
    if (g.px.empty()) return false;
    width = g.w;
    height = g.h;
    out.resize(g.px.size());
    for (size_t i = 0; i < g.px.size(); i++) out[i] = g.px[i] > 127 ? 255 : 0;
    return true;
}

bool rasterize_frame_mask(const FrameMask& m, int width, int height,
                          std::vector<uint8_t>& out, std::string& error) {
    if (width <= 0 || height <= 0) {
        error = std::to_string(width) + "x" + std::to_string(height);
        return false;
    }
    const bool base = m.shapes.empty() || m.shapes.front().remove;

    // A path or stroke is filled once into its own plane; the pixel loop then
    // reads it like any other inside test, so the ordering rule is untouched.
    std::vector<std::vector<uint8_t>> paths(m.shapes.size());
    std::vector<float> px;
    for (size_t k = 0; k < m.shapes.size(); k++) {
        const MaskShape& s = m.shapes[k];
        if (s.kind == MaskShape::Kind::Stroke) {
            paths[k].assign((size_t)width * height, 0);
            fill_stroke(s, width, height, paths[k].data());
            continue;
        }
        if (s.kind != MaskShape::Kind::Path) continue;
        paths[k].assign((size_t)width * height, 0);
        px.resize(s.pts.size());
        for (size_t i = 0; i + 1 < s.pts.size(); i += 2) {
            px[i] = s.pts[i] * (float)width;
            px[i + 1] = s.pts[i + 1] * (float)height;
        }
        polyfill::fill_even_odd(px.data(), px.size() / 2, width, height, paths[k].data(), 1);
    }

    out.assign((size_t)width * height, 255);
    for (int y = 0; y < height; y++) {
        const float v = ((float)y + 0.5f) / (float)height;
        uint8_t* row = &out[(size_t)y * width];
        for (int x = 0; x < width; x++) {
            const float u = ((float)x + 0.5f) / (float)width;
            bool keep = base;
            for (size_t k = 0; k < m.shapes.size(); k++) {
                const MaskShape& s = m.shapes[k];
                bool inside;
                if (s.kind == MaskShape::Kind::Path || s.kind == MaskShape::Kind::Stroke) {
                    inside = paths[k][(size_t)y * width + x] != 0;
                } else if (s.kind == MaskShape::Kind::Ellipse) {
                    if (s.rx <= 0.0f || s.ry <= 0.0f) continue;
                    const float du = (u - s.cx) / s.rx, dv = (v - s.cy) / s.ry;
                    inside = du * du + dv * dv <= 1.0f;
                } else {
                    inside = u >= std::min(s.cx, s.rx) && u <= std::max(s.cx, s.rx) &&
                             v >= std::min(s.cy, s.ry) && v <= std::max(s.cy, s.ry);
                }
                if (inside) keep = !s.remove;
            }
            row[x] = keep ? 255 : 0;
        }
    }

    if (!m.image.empty()) {
        int iw = 0, ih = 0;
        std::vector<uint8_t> stencil;
        if (!load_stencil(m.image, iw, ih, stencil)) {
            error = m.image;
            return false;
        }
        for (int y = 0; y < height; y++) {
            const int sy = std::min(ih - 1, (int)((float)y / height * (float)ih));
            for (int x = 0; x < width; x++) {
                const int sx = std::min(iw - 1, (int)((float)x / width * (float)iw));
                if (!stencil[(size_t)sy * iw + sx]) out[(size_t)y * width + x] = 0;
            }
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Border detection
// ---------------------------------------------------------------------------

// Frames are box-averaged down to this before anything is fitted: at full
// resolution sensor noise speckles the black past `dark`.
constexpr int kFitSize = 1024;

struct BorderAccumulator::Impl {
    int w = 0, h = 0, n = 0;
    int f = 1, sw = 0, sh = 0;   // box factor and the size the fit sees
    std::vector<uint8_t> maxluma;
    std::vector<float> gsum, gsum2;
    std::vector<float> lsum;
    std::vector<float> g;
    std::vector<uint32_t> box;
    Gray gray;
};

BorderAccumulator::BorderAccumulator() : impl_(std::make_unique<Impl>()) {}
BorderAccumulator::~BorderAccumulator() = default;
int BorderAccumulator::frames() const { return impl_->n; }

void BorderAccumulator::add(const uint8_t* px, int width, int height, int channels) {
    Impl& s = *impl_;
    if (!px || width <= 0 || height <= 0 || channels < 1) return;
    if (s.w == 0) {
        s.w = width;
        s.h = height;
        s.f = std::max(1, (std::max(width, height) + kFitSize - 1) / kFitSize);
        s.sw = std::max(1, width / s.f);
        s.sh = std::max(1, height / s.f);
        s.maxluma.assign((size_t)s.sw * s.sh, 0);
        s.gsum.assign(s.maxluma.size(), 0.0f);
        s.gsum2.assign(s.maxluma.size(), 0.0f);
        s.lsum.assign(s.maxluma.size(), 0.0f);
    } else if (width != s.w || height != s.h) {
        return;   // a camera is one frame size; anything else is not ours
    }
    s.box.assign(s.maxluma.size(), 0);
    const int f = s.f;
    for (int y = 0; y < s.sh * f; y++) {
        const uint8_t* row = px + (size_t)y * width * channels;
        uint32_t* out = &s.box[(size_t)(y / f) * s.sw];
        for (int x = 0; x < s.sw * f; x++) {
            const uint8_t* p = row + (size_t)x * channels;
            out[x / f] += channels == 1 ? 256u * p[0]
                                        : 77u * p[0] + 150u * p[1] + 29u * p[2];
        }
    }
    s.gray.w = s.sw;
    s.gray.h = s.sh;
    s.gray.px.resize(s.maxluma.size());
    const uint32_t div = 256u * (uint32_t)(f * f);
    for (size_t i = 0; i < s.gray.px.size(); i++)
        s.gray.px[i] = (uint8_t)(s.box[i] / div);
    gradient_magnitude(s.gray, s.g);
    for (size_t p = 0; p < s.gray.px.size(); p++) {
        s.maxluma[p] = std::max(s.maxluma[p], s.gray.px[p]);
        s.lsum[p] += (float)s.gray.px[p];
        s.gsum[p] += s.g[p];
        s.gsum2[p] += s.g[p] * s.g[p];
    }
    s.n++;
}

BorderDetect BorderAccumulator::finish(const BorderDetectOptions& o) const {
    const Impl& s = *impl_;
    BorderDetect d;
    d.frames = s.n;
    if (s.n < 2) return d;
    const int w = s.sw, h = s.sh;
    d.width = s.w;
    d.height = s.h;

    const size_t n_px = s.maxluma.size();
    std::vector<uint8_t> dark(n_px);
    size_t n_dark = 0;
    for (size_t p = 0; p < n_px; p++) {
        dark[p] = s.maxluma[p] <= (uint8_t)std::clamp(o.dark, 0, 255) ? 1 : 0;
        n_dark += dark[p];
    }
    d.dark_fraction = (float)n_dark / (float)n_px;

    // Where the frame never resolves anything, blurred so that a flat wall does
    // not read as border. It decides whether there is a lens at all; the edge
    // itself comes off the averaged picture, `mean`.
    std::vector<float> act(n_px);
    for (size_t p = 0; p < n_px; p++) {
        const float mean = s.gsum[p] / (float)s.n;
        act[p] = std::sqrt(std::max(s.gsum2[p] / (float)s.n - mean * mean, 0.0f));
    }
    box_blur(act, w, h, std::max(4, w / 48));
    const float thr = 0.25f * percentile(act, 0.75f);
    std::vector<uint8_t> valid(n_px), lit(n_px);
    size_t n_still = 0;
    for (size_t p = 0; p < n_px; p++) {
        valid[p] = (act[p] <= thr || dark[p]) ? 0 : 1;
        n_still += valid[p] ? 0 : 1;
        lit[p] = dark[p] ? 0 : 1;
    }
    act = {};
    std::vector<float> mean(n_px);
    for (size_t p = 0; p < n_px; p++) mean[p] = s.lsum[p] / (float)s.n;

    auto fit = [&](const std::vector<uint8_t>& region, const std::vector<uint8_t>* bound,
                   const std::vector<float>* steps, const float* prior, CircleFit& f) {
        if (!fit_valid_region(region, bound, steps, prior, w, h, std::max(o.rays, 32),
                              o.tolerance, f))
            return false;
        return f.support >= o.min_support &&
               valid_outside(region, w, h, f.c) <= o.max_outside;
    };

    float centre[2];
    const bool centred = radial_centre(mean, w, h, centre[0], centre[1]);
    CircleFit f;
    d.cue = BorderCue::Activity;
    bool found = (float)n_still / (float)n_px >= 0.02f &&
                 fit(valid, &lit, &mean, centred ? centre : nullptr, f);
    // Too still a capture for that, or what it saw is not a circle.
    if (!found && d.dark_fraction >= 0.02f) {
        d.cue = BorderCue::Dark;
        found = fit(lit, nullptr, nullptr, nullptr, f);
    }
    const MaskShape shape = to_shape(f.c, o.shrink, w, h);
    const float kept = found ? kept_fraction(shape, w, h) : 1.0f;
    if (kept > 0.99f) {   // nothing worth masking
        d.cue = BorderCue::None;
        return d;
    }
    d.found = true;
    d.shape = shape;
    d.residual = f.rms;
    d.kept_fraction = kept;
    return d;
}

BorderDetect detect_fisheye_border(const std::vector<std::string>& files,
                                   const BorderDetectOptions& o) {
    if (files.empty()) return BorderDetect{};
    const int want = std::max(2, o.samples);
    std::vector<size_t> pick;
    if ((int)files.size() <= want) {
        for (size_t i = 0; i < files.size(); i++) pick.push_back(i);
    } else {
        for (int i = 0; i < want; i++)
            pick.push_back((size_t)((double)i * (files.size() - 1) / (want - 1) + 0.5));
        pick.erase(std::unique(pick.begin(), pick.end()), pick.end());
    }
    BorderAccumulator acc;
    for (size_t i : pick) {
        Gray f = load_gray(files[i]);
        if (!f.px.empty()) acc.add(f.px.data(), f.w, f.h, 1);
    }
    return acc.finish(o);
}

// ---------------------------------------------------------------------------
// Applying a stencil to a folder of frames
// ---------------------------------------------------------------------------

namespace {

namespace fs = std::filesystem;

bool is_image_file(const fs::path& p) {
    std::string e = p.extension().string();
    for (char& c : e) c = (char)std::tolower((unsigned char)c);
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".bmp" ||
           e == ".tif" || e == ".tiff" || e == ".webp" || e == ".exr";
}

void intersect_with_file(std::vector<uint8_t>& px, int w, int h,
                         const std::string& path, bool flip) {
    int mw = 0, mh = 0;
    std::vector<uint8_t> m;
    if (!load_stencil(path, mw, mh, m)) return;
    for (int y = 0; y < h; y++) {
        const int sy = mh == h ? y : std::min(mh - 1, (int)((float)y / h * mh));
        for (int x = 0; x < w; x++) {
            const int sx = mw == w ? x : std::min(mw - 1, (int)((float)x / w * mw));
            if ((m[(size_t)sy * mw + sx] != 0) == flip) px[(size_t)y * w + x] = 0;
        }
    }
}

}  // namespace

std::map<std::string, std::vector<std::string>> group_frames_by_camera(
    const std::string& dir, const std::string& skip_dir) {
    std::map<std::string, std::vector<std::string>> groups;
    std::error_code ec;
    const std::string skip =
        skip_dir.empty() ? std::string()
                         : fs::path(skip_dir).lexically_normal().generic_string() + "/";
    const fs::path root(dir);
    for (fs::recursive_directory_iterator it(
             root, fs::directory_options::skip_permission_denied |
                       fs::directory_options::follow_directory_symlink, ec), end;
         !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || !is_image_file(it->path())) continue;
        if (!skip.empty() &&
            it->path().lexically_normal().generic_string().rfind(skip, 0) == 0)
            continue;
        // Lexical: fs::relative resolves symlinks, and an images/ of links
        // into a raw capture then relativizes to "../../<capture>/...", which
        // puts the masks in the folder the run was only asked to read.
        fs::path rel = it->path().lexically_relative(root);
        if (rel.empty() || *rel.begin() == "..") rel = it->path().filename();
        groups[rel.parent_path().generic_string()].push_back(it->path().string());
    }
    for (auto& [k, v] : groups) std::sort(v.begin(), v.end());
    return groups;
}

int64_t apply_frame_stencil(const FrameStencilRun& run,
                            const FrameStencilSinks& sinks, std::string& error) {
    const auto groups = group_frames_by_camera(run.image_dir, run.skip_dir);
    if (groups.empty()) {
        error = run.image_dir;
        return -1;
    }
    int64_t total = 0;
    for (const auto& [rel, files] : groups) total += (int64_t)files.size();

    std::error_code ec;
    const fs::path image_root(run.image_dir), mask_root(run.mask_dir);
    const fs::path merge_root(run.merge_dir);
    int64_t written = 0, seen = 0;
    for (const auto& [rel, files] : groups) {
        if (sinks.cancel && sinks.cancel->load()) return written;
        if (sinks.camera) sinks.camera(rel, (int64_t)files.size());

        FrameMask fm = run.stencil.mask;
        BorderDetect border;
        if (run.stencil.detect_border) {
            BorderDetectOptions o = run.detect;
            o.shrink = run.stencil.shrink;
            border = detect_fisheye_border(files, o);
            // First, so the shapes drawn on top are applied to it in order.
            if (border.found) fm.shapes.insert(fm.shapes.begin(), border.shape);
        }
        if (sinks.resolved) sinks.resolved(rel, fm, border);
        // Nothing to draw AND nothing to fold in. A camera whose border was not
        // found still has to be walked when there are masks to read: leaving it
        // out is what leaves half a dataset in the other convention.
        if (run.dry_run || (fm.empty() && merge_root.empty())) {
            seen += (int64_t)files.size();
            continue;
        }

        // Every frame of one camera gets the same stencil, so it is rasterized
        // once per distinct frame size -- and per distinct turn, the shapes
        // being drawn in the displayed frame and written in the stored one.
        struct Sized {
            std::vector<uint8_t> px;
            std::string first;
        };
        std::map<std::tuple<int, int, int>, Sized> cache;
        struct Item { std::string dst, src; int w, h; int key_turn; };
        std::vector<Item> items;
        items.reserve(files.size());
        for (const std::string& f : files) {
            if (sinks.cancel && sinks.cancel->load()) return written;
            int w = 0, h = 0;
            if (!image_size(f, w, h)) continue;
            const sfm::ExifTransform t = photo_turn(f);
            const int key_turn = t.turns_cw | (t.mirror ? 4 : 0);
            Sized& s = cache[{w, h, key_turn}];
            if (s.px.empty()) {
                int dw = w, dh = h;
                spirula::oriented_size(t.turns_cw, dw, dh);
                if (!rasterize_frame_mask(fm, dw, dh, s.px, error)) return -1;
                turn_pixels(inverse_turn(t), 1, s.px, dw, dh);
            }
            const std::string name = fs::path(f).stem().string() + ".png";
            const fs::path dst = mask_root / rel / name;
            fs::create_directories(dst.parent_path(), ec);
            const fs::path src = merge_root.empty() ? dst : merge_root / rel / name;
            const bool have_src = (merge_root.empty() ? !run.replace : true) &&
                                  fs::exists(src, ec);
            items.push_back({dst.string(), have_src ? src.string() : std::string(),
                             w, h, key_turn});
        }

        // A frame with a mask to fold in cannot share the stencil image: it
        // costs a PNG decode and a re-encode, 131 ms per 2880-square fisheye
        // frame. They are independent, so they go on every core.
        std::vector<size_t> merges;
        for (size_t i = 0; i < items.size(); i++)
            if (!items[i].src.empty()) merges.push_back(i);
        std::atomic<bool> failed{false};
        std::atomic<int64_t> done{0};
        std::mutex sink_mu;
        std::string merge_error;
        nn::parallel_for((int64_t)merges.size(), [&](int64_t lo, int64_t hi) {
            for (int64_t k = lo; k < hi; k++) {
                if (failed.load() ||
                    (sinks.cancel && sinks.cancel->load())) return;
                const Item& it = items[merges[(size_t)k]];
                std::vector<uint8_t> px = cache.at({it.w, it.h, it.key_turn}).px;
                intersect_with_file(px, it.w, it.h, it.src, run.flip_merge);
                // Masks gathered next to the images are HARD LINKS to the ones
                // the photos came with; writing over one would edit the user's
                // file. Unlink first, so this only ever adds a file.
                std::error_code rm;
                fs::remove(it.dst, rm);
                if (!stbi_write_png(it.dst.c_str(), it.w, it.h, 1, px.data(), it.w)) {
                    std::lock_guard<std::mutex> lk(sink_mu);
                    merge_error = it.dst;
                    failed = true;
                    return;
                }
                if (sinks.progress) {
                    std::lock_guard<std::mutex> lk(sink_mu);
                    sinks.progress(seen + (++done), total);
                }
            }
        }, 1);
        if (failed.load()) {
            error = merge_error;
            return -1;
        }
        written += (int64_t)merges.size();
        seen += (int64_t)merges.size();

        // The rest share one stencil image: encode it once, copy the file for
        // every other frame of that size. A 1920-square gray mask costs ~150 ms
        // to encode -- a quarter of an hour over a two-track 2800-frame capture.
        for (const Item& it : items) {
            if (!it.src.empty()) continue;
            if (sinks.cancel && sinks.cancel->load()) return written;
            if (sinks.progress) sinks.progress(++seen, total);
            Sized& s = cache.at({it.w, it.h, it.key_turn});
            fs::remove(it.dst, ec);
            if (!s.first.empty()) {
                fs::copy_file(s.first, it.dst, fs::copy_options::overwrite_existing, ec);
                if (!ec) { written++; continue; }
            }
            if (!stbi_write_png(it.dst.c_str(), it.w, it.h, 1, s.px.data(), it.w)) {
                error = it.dst;
                return -1;
            }
            written++;
            if (s.first.empty()) s.first = it.dst;
        }
    }
    return written;
}

}  // namespace app
