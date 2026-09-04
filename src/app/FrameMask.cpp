// FrameMask.cpp -- see FrameMask.h.

#include "app/FrameMask.h"

#include "core/ExrImage.h"

#include "external/stb_image.h"
#include "external/stb_image_write.h"
#include "nn/core/Parallel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
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

struct Ellipse {
    float cx = 0, cy = 0, rx = 0, ry = 0;
};

// Least squares on A u^2 + B v^2 + C u + D v = 1, in normalized coordinates.
// The sign of A and B is not constrained: the constant term of a circle
// through the middle of the frame is positive, so normalizing it to -1 makes
// both negative, and rejecting that rejects every well-centred fit.
bool fit_ellipse(const std::vector<float>& xs, const std::vector<float>& ys,
                 int w, int h, Ellipse& out) {
    if (xs.size() < 8) return false;
    double M[4][5] = {};
    for (size_t i = 0; i < xs.size(); i++) {
        const double u = xs[i] / w, v = ys[i] / h;
        const double row[4] = {u * u, v * v, u, v};
        for (int a = 0; a < 4; a++) {
            for (int b = 0; b < 4; b++) M[a][b] += row[a] * row[b];
            M[a][4] += row[a];
        }
    }
    for (int c = 0; c < 4; c++) {
        int piv = c;
        for (int r = c + 1; r < 4; r++)
            if (std::fabs(M[r][c]) > std::fabs(M[piv][c])) piv = r;
        if (std::fabs(M[piv][c]) < 1e-12) return false;
        if (piv != c) std::swap(M[piv], M[c]);
        for (int r = 0; r < 4; r++) {
            if (r == c) continue;
            const double f = M[r][c] / M[c][c];
            for (int k = c; k < 5; k++) M[r][k] -= f * M[c][k];
        }
    }
    const double A = M[0][4] / M[0][0], B = M[1][4] / M[1][1];
    const double C = M[2][4] / M[2][2], D = M[3][4] / M[3][3];
    if (A == 0 || B == 0) return false;
    const double cx = -C / (2 * A), cy = -D / (2 * B);
    const double k = 1 + A * cx * cx + B * cy * cy;
    if (k / A <= 0 || k / B <= 0) return false;
    out = {(float)cx, (float)cy, (float)std::sqrt(k / A), (float)std::sqrt(k / B)};
    return true;
}

float radial_residual(const Ellipse& e, float x, float y, int w, int h) {
    const float u = x / w, v = y / h;
    return std::hypot((u - e.cx) / e.rx, (v - e.cy) / e.ry) - 1.0f;
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

// Ray-march, fit, drop the outliers, refit -- four times, re-centring on the
// fit each round so a badly-placed starting centroid does not bias the edges
// it finds. `rms` comes back over the surviving inliers.
bool fit_valid_region(const std::vector<uint8_t>& valid, int w, int h, int nrays,
                      Ellipse& out, float& rms) {
    size_t n_valid = 0;
    double sx = 0, sy = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (valid[(size_t)y * w + x]) { n_valid++; sx += x; sy += y; }
    if (n_valid < (size_t)w * h / 20) return false;
    float cx = (float)(sx / (double)n_valid), cy = (float)(sy / (double)n_valid);

    std::vector<float> xs, ys, res, a, kx, ky;
    for (int it = 0; it < 4; it++) {
        ray_edges(valid, w, h, cx, cy, nrays, xs, ys);
        if (xs.size() < (size_t)nrays / 4) return false;
        Ellipse e;
        if (!fit_ellipse(xs, ys, w, h, e)) return false;

        // To a fixed point, not once: a ray stopping on a still region inside
        // the circle lands 20% short, and one round takes its tolerance from a
        // fit those rays are in -- MAD 0.14 on the OSV captures, keeping them all.
        for (int trim = 0; trim < 6; trim++) {
            res.resize(xs.size());
            for (size_t i = 0; i < xs.size(); i++)
                res[i] = radial_residual(e, xs[i], ys[i], w, h);
            a.resize(res.size());
            const float med = percentile(res, 0.5f);
            for (size_t i = 0; i < res.size(); i++) a[i] = std::fabs(res[i] - med);
            const float tol = std::max(3.0f * 1.4826f * percentile(a, 0.5f), 0.004f);
            kx.clear();
            ky.clear();
            for (size_t i = 0; i < xs.size(); i++)
                if (std::fabs(res[i]) <= tol) { kx.push_back(xs[i]); ky.push_back(ys[i]); }
            if (kx.size() < (size_t)nrays * 2 / 5) break;
            Ellipse e2;
            if (!fit_ellipse(kx, ky, w, h, e2)) break;
            const bool converged = kx.size() == xs.size();
            e = e2;
            xs.swap(kx);
            ys.swap(ky);
            if (converged) break;
        }
        res.resize(xs.size());
        for (size_t i = 0; i < xs.size(); i++)
            res[i] = radial_residual(e, xs[i], ys[i], w, h);
        out = e;
        cx = e.cx * w;
        cy = e.cy * h;
    }
    if (res.empty()) return false;
    double s = 0;
    for (float r : res) s += (double)r * r;
    rms = (float)std::sqrt(s / (double)res.size());
    return true;
}

MaskShape to_shape(const Ellipse& e, float shrink) {
    const float k = 1.0f - std::clamp(shrink, -0.5f, 0.9f);
    MaskShape s;
    s.kind = MaskShape::Kind::Ellipse;
    s.remove = false;
    s.cx = e.cx;
    s.cy = e.cy;
    s.rx = e.rx * k;
    s.ry = e.ry * k;
    return s;
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

}  // namespace

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
        float v[4];
        if (!parse_four(trim(piece.substr(sp + 1)), v)) {
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
    for (const MaskShape& s : shapes) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s%s %.4f,%.4f,%.4f,%.4f",
                      s.remove ? "-" : "",
                      s.kind == MaskShape::Kind::Rect ? "rect" : "ellipse",
                      s.cx, s.cy, s.rx, s.ry);
        if (!out.empty()) out += "; ";
        out += buf;
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

    out.assign((size_t)width * height, 255);
    for (int y = 0; y < height; y++) {
        const float v = ((float)y + 0.5f) / (float)height;
        uint8_t* row = &out[(size_t)y * width];
        for (int x = 0; x < width; x++) {
            const float u = ((float)x + 0.5f) / (float)width;
            bool keep = base;
            for (const MaskShape& s : m.shapes) {
                bool inside;
                if (s.kind == MaskShape::Kind::Ellipse) {
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

struct BorderAccumulator::Impl {
    int w = 0, h = 0, n = 0;
    std::vector<uint8_t> maxluma;
    std::vector<float> gsum, gsum2;
    std::vector<float> g;
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
        s.maxluma.assign((size_t)width * height, 0);
        s.gsum.assign(s.maxluma.size(), 0.0f);
        s.gsum2.assign(s.maxluma.size(), 0.0f);
    } else if (width != s.w || height != s.h) {
        return;   // a camera is one frame size; anything else is not ours
    }
    s.gray.w = width;
    s.gray.h = height;
    s.gray.px.resize(s.maxluma.size());
    if (channels == 1) {
        std::memcpy(s.gray.px.data(), px, s.gray.px.size());
    } else {
        for (size_t i = 0; i < s.gray.px.size(); i++) {
            const uint8_t* p = px + i * (size_t)channels;
            s.gray.px[i] = (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
        }
    }
    gradient_magnitude(s.gray, s.g);
    for (size_t p = 0; p < s.gray.px.size(); p++) {
        s.maxluma[p] = std::max(s.maxluma[p], s.gray.px[p]);
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
    const int w = s.w, h = s.h;
    d.width = w;
    d.height = h;

    const size_t n_px = s.maxluma.size();
    std::vector<uint8_t> dark(n_px);
    size_t n_dark = 0;
    for (size_t p = 0; p < n_px; p++) {
        dark[p] = s.maxluma[p] <= (uint8_t)std::clamp(o.dark, 0, 255) ? 1 : 0;
        n_dark += dark[p];
    }
    d.dark_fraction = (float)n_dark / (float)n_px;

    auto accept = [&](const std::vector<uint8_t>& valid, BorderCue cue) {
        Ellipse e;
        float rms = 0.0f;
        if (!fit_valid_region(valid, w, h, std::max(o.rays, 32), e, rms)) return false;
        if (rms > o.max_residual) return false;
        const MaskShape shape = to_shape(e, o.shrink);
        const float kept = kept_fraction(shape, w, h);
        if (kept > 0.99f) return false;   // nothing worth masking
        d.found = true;
        d.shape = shape;
        d.cue = cue;
        d.residual = rms;
        d.kept_fraction = kept;
        return true;
    };

    // Where the frame never resolves anything, blurred so that a flat wall does
    // not read as border. It leads: black finds only where the light stops, and
    // a flare ring costs 6% of the radius on PortalCam, 9% on OSV.
    std::vector<float> act(n_px);
    for (size_t p = 0; p < n_px; p++) {
        const float mean = s.gsum[p] / (float)s.n;
        act[p] = std::sqrt(std::max(s.gsum2[p] / (float)s.n - mean * mean, 0.0f));
    }
    box_blur(act, w, h, std::max(4, w / 48));
    const float thr = 0.25f * percentile(act, 0.75f);
    std::vector<uint8_t> valid(n_px);
    size_t n_still = 0;
    for (size_t p = 0; p < n_px; p++) {
        valid[p] = (act[p] <= thr || dark[p]) ? 0 : 1;
        n_still += valid[p] ? 0 : 1;
    }
    if ((float)n_still / (float)n_px >= 0.02f && accept(valid, BorderCue::Activity))
        return d;

    // Too still a capture for that, or what it saw is not an ellipse.
    if (d.dark_fraction >= 0.02f) {
        for (size_t p = 0; p < n_px; p++) valid[p] = dark[p] ? 0 : 1;
        accept(valid, BorderCue::Dark);
    }
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
        // once per distinct frame size.
        struct Sized {
            std::vector<uint8_t> px;
            std::string first;
        };
        std::map<std::pair<int, int>, Sized> cache;
        struct Item { std::string dst, src; int w, h; };
        std::vector<Item> items;
        items.reserve(files.size());
        for (const std::string& f : files) {
            if (sinks.cancel && sinks.cancel->load()) return written;
            int w = 0, h = 0;
            if (!image_size(f, w, h)) continue;
            Sized& s = cache[{w, h}];
            if (s.px.empty() && !rasterize_frame_mask(fm, w, h, s.px, error)) return -1;
            const std::string name = fs::path(f).stem().string() + ".png";
            const fs::path dst = mask_root / rel / name;
            fs::create_directories(dst.parent_path(), ec);
            const fs::path src = merge_root.empty() ? dst : merge_root / rel / name;
            const bool have_src = (merge_root.empty() ? !run.replace : true) &&
                                  fs::exists(src, ec);
            items.push_back({dst.string(), have_src ? src.string() : std::string(),
                             w, h});
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
                std::vector<uint8_t> px = cache.at({it.w, it.h}).px;
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
            Sized& s = cache.at({it.w, it.h});
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
