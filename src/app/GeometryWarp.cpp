#include "app/GeometryWarp.h"

#include "data/CameraMath.h"
#include "data/SourceCamera.h"
#include "nn/core/Parallel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace app {
namespace {

constexpr float  kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr double kPi  = 3.14159265358979323846;
constexpr double kDeg = kPi / 180.0;

// The front face reaches 1.1x its 45 degrees, leaving the ring a strip to
// cross-fade over; a panorama's cube faces reach 1.25x theirs.
constexpr double kFrontOverlap = 1.1;
constexpr double kCubeOverlap  = 1.25;

// The ring reaches in to 40 degrees, under the front face's edge, and 3
// degrees past what the lens holds.
constexpr double kRingInner  = 40.0 * kDeg;
constexpr double kRingMargin = 3.0 * kDeg;

// A ring face's half field of view: as tall as its band needs, within these.
// 30 degrees is where a 180-degree lens takes 8 faces; 40 keeps the corners
// of a wider lens's faces inside what a network has seen.
constexpr double kRingHalfMin = 30.0 * kDeg;
constexpr double kRingHalfMax = 40.0 * kDeg;

// Neighbours in the ring share this fraction of their azimuth.
constexpr double kRingOverlap = 0.15;

// A face cropped to its band keeps at least this much of its width.
constexpr double kMinAspect = 0.75;

// Rings stack outward, each reaching this far under the previous one's edge,
// until the boundary is reached; a lens never needs more than this many.
constexpr double kRingStack = 10.0 * kDeg;
constexpr int    kMaxRings  = 3;

// A face side no `--max-size` was asked for still stops here: past it the
// plan's maps alone are gigabytes.
constexpr int kMaxFaceSide = 4096;

// The cross-fade ramps over this fraction of a face's half extent.
constexpr double kBlend = 0.2;

struct Frame {
    double ax[3], ay[3], az[3];   // unit rows
    double ex = 1.0, ey = 1.0;    // half extents, tangent units
};

double dot3(const double* a, const double* b) {
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}
double len3(const double* a) { return std::sqrt(dot3(a, a)); }

void cross3(const double* a, const double* b, double out[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

camhost::Camera host_camera(const GeometryCamera& cam) {
    camhost::Camera c;
    c.model = cam.model;
    c.tier = cam.distortion;
    c.width = cam.width;
    c.height = cam.height;
    c.fx = cam.fx; c.fy = cam.fy; c.cx = cam.cx; c.cy = cam.cy;
    std::copy(std::begin(cam.dist), std::end(cam.dist), std::begin(c.dist));
    c.source_model = cam.source_model;
    std::copy(std::begin(cam.source_params), std::end(cam.source_params),
              std::begin(c.source_params));
    return c;
}

// The frame looking along `az` whose image up is the camera's up (-y) as far
// as the tilt allows -- the networks carry a gravity prior -- and `down`
// where `az` is the camera's up itself.
Frame upright_frame(const double az[3], const double down[3]) {
    Frame f;
    for (int m = 0; m < 3; ++m) f.az[m] = az[m];
    const double up[3] = {0.0, -1.0, 0.0};
    const double d = dot3(up, az);
    double u[3];
    for (int m = 0; m < 3; ++m) u[m] = up[m] - d * az[m];
    const double l = len3(u);
    if (l > 1e-6) for (int m = 0; m < 3; ++m) f.ay[m] = -u[m] / l;
    else          for (int m = 0; m < 3; ++m) f.ay[m] = down[m];
    cross3(f.ay, f.az, f.ax);   // ax x ay = az
    return f;
}

// Pixels per radian the source has along the radial direction at (theta,
// phi) off its axis -- what a face pointing there is sized to keep. A
// panorama's is a constant, and measuring it across its seam is not.
double source_density(const camhost::Camera& cam, double theta, double phi) {
    if (cam.model == (int)CameraModelType::EQUIRECTANGULAR)
        return cam.width / (2.0 * kPi);
    const double d = 0.5 * kDeg;
    double p0[2], p1[2];
    const double a = std::fmax(theta - d, 0.0), b = theta + d;
    const double r0[3] = {std::sin(a) * std::cos(phi), std::sin(a) * std::sin(phi), std::cos(a)};
    const double r1[3] = {std::sin(b) * std::cos(phi), std::sin(b) * std::sin(phi), std::cos(b)};
    if (!camhost::ray_in_frame(cam, r0, p0) || !camhost::ray_in_frame(cam, r1, p1))
        return std::fmax(cam.fx, 1.0);
    return std::fmax(std::hypot(p1[0] - p0[0], p1[1] - p0[1]) / (b - a), 1.0);
}

// Solve az + tx*ax + ty*ay = t*r by Cramer. t <= 0 is the direction behind
// the camera, which meets the same plane at the same point.
bool face_coords(const double* a, const double r[3], double& tx, double& ty) {
    double c[3];
    cross3(a, a + 3, c);                       // ax x ay
    const double det = dot3(c, r);
    if (std::fabs(det) < 1e-12) return false;
    const double inv = 1.0 / det;
    if (dot3(c, a + 6) * inv <= 0.0) return false;
    double c1[3], c2[3];
    cross3(r, a + 3, c1);                      // r x ay
    cross3(a, r, c2);                          // ax x r
    tx = dot3(c1, a + 6) * inv;
    ty = dot3(c2, a + 6) * inv;
    return std::fabs(tx) < 1.0 && std::fabs(ty) < 1.0;
}

// 1 over the face's middle, ramping to 0 at its edge over the outer kBlend.
float face_weight(double tx, double ty) {
    const double w = std::fmin(std::fmin((1.0 - std::fabs(tx)) / kBlend,
                                         (1.0 - std::fabs(ty)) / kBlend), 1.0);
    if (!(w > 0.0)) return 0.0f;
    return (float)(w * w * (3.0 - 2.0 * w));   // smoothstep: C1 across the seam
}

// Bilinear sample of a [h, w, C] map at a pixel coordinate whose 0.5 is the
// first pixel's centre, edges clamped.
void sample(const float* img, int w, int h, int C, float px, float py, float* out) {
    const float x = px - 0.5f, y = py - 0.5f;
    int x0 = (int)std::floor(x), y0 = (int)std::floor(y);
    const float fx = x - x0, fy = y - y0;
    const int x1 = std::min(std::max(x0 + 1, 0), w - 1);
    const int y1 = std::min(std::max(y0 + 1, 0), h - 1);
    x0 = std::min(std::max(x0, 0), w - 1);
    y0 = std::min(std::max(y0, 0), h - 1);
    const float* p00 = img + ((size_t)y0 * w + x0) * C;
    const float* p01 = img + ((size_t)y0 * w + x1) * C;
    const float* p10 = img + ((size_t)y1 * w + x0) * C;
    const float* p11 = img + ((size_t)y1 * w + x1) * C;
    for (int c = 0; c < C; ++c)
        out[c] = (p00[c] * (1 - fx) + p01[c] * fx) * (1 - fy) +
                 (p10[c] * (1 - fx) + p11[c] * fx) * fy;
}

int snap(double px, int patch) {
    return std::max(patch, (int)std::lround(px / patch) * patch);
}

// One ring of upright square faces, tilted so each reaches its own sector's
// boundary, as many as their width needs to go round. Reaches in to `inner`
// and out to `outer`; returns false when the boundary leaves it nothing.
bool plan_one_ring(const std::vector<double>& tmax, double inner, double outer,
                   std::vector<Frame>& frames) {
    const int M = (int)tmax.size();
    const double front_reach = std::atan(kFrontOverlap);
    const double beta = std::clamp(0.5 * (outer - inner), kRingHalfMin, kRingHalfMax);
    const double alpha_g = outer - beta;
    // Azimuth a face spans at an edge is atan(sin beta / sin theta): the
    // narrower of its two edges is what has to go round.
    auto edge_hw = [&](double theta) {
        return std::atan2(std::sin(beta), std::fmax(std::sin(theta), 1e-6));
    };
    const double hw = std::fmin(edge_hw(alpha_g + beta), edge_hw(alpha_g - beta));
    const int N = std::clamp((int)std::ceil(kPi / (hw * (1.0 - kRingOverlap))), 4, 12);
    const double sector = kPi / N * (1.0 + kRingOverlap);

    bool any = false;
    for (int k = 0; k < N; ++k) {
        const double phi = 2.0 * kPi * k / N;
        double tout = 0.0;
        for (int m = 0; m < M; ++m) {
            double d = std::fmod(std::fabs(2.0 * kPi * m / M - phi), 2.0 * kPi);
            d = std::fmin(d, 2.0 * kPi - d);
            if (d <= sector) tout = std::fmax(tout, tmax[(size_t)m]);
        }
        tout = std::fmin(tout + kRingMargin, outer);
        if (tout <= std::fmax(inner, front_reach) + 1.0 * kDeg) continue;

        const double br = std::clamp(0.5 * (tout - inner),
                                     std::atan(kMinAspect * std::tan(beta)), beta);
        const double alpha = tout - br;
        const double az[3] = {std::sin(alpha) * std::cos(phi),
                              std::sin(alpha) * std::sin(phi), std::cos(alpha)};
        const double radial[3] = {std::cos(alpha) * std::cos(phi),
                                  std::cos(alpha) * std::sin(phi), -std::sin(alpha)};
        Frame f = upright_frame(az, radial);
        f.ex = f.ey = std::tan(beta);
        // Cropped to its band where the band runs along an image axis; a
        // diagonal face stays square, and its rotated square covers more.
        const double dx = std::fabs(dot3(radial, f.ax)), dy = std::fabs(dot3(radial, f.ay));
        double* rad = dx > 0.99 ? &f.ex : dy > 0.99 ? &f.ey : nullptr;
        double* tan_ = dx > 0.99 ? &f.ey : dy > 0.99 ? &f.ex : nullptr;
        if (rad) {
            *rad = std::tan(br);
            const double need = std::tan(kPi / N * (1.0 + kRingOverlap)) *
                                std::fmax(std::sin(tout), 1e-6) / std::cos(br);
            *tan_ = std::fmax(*tan_, need);
        }
        frames.push_back(f);
        any = true;
    }
    return any;
}

// The rings around the front face: the first reaches in under its edge, each
// next one in under the last, out to where the lens stops.
void plan_ring(const camhost::Camera& hc, std::vector<Frame>& frames) {
    constexpr int M = 360;
    const std::vector<double> tmax = camhost::visible_boundary(hc, M);
    double tout_g = 0.0;
    for (double t : tmax) tout_g = std::fmax(tout_g, t);
    tout_g = std::fmin(tout_g + kRingMargin, kPi - 1e-3);

    double inner = kRingInner;
    for (int ring = 0; ring < kMaxRings && tout_g > inner + 1.0 * kDeg; ++ring) {
        const double outer = std::fmin(tout_g, inner + 2.0 * kRingHalfMax);
        if (!plan_one_ring(tmax, inner, outer, frames) || outer >= tout_g) break;
        inner = outer - kRingStack;
    }
}

}  // namespace

std::vector<float> resize_area(const uint8_t* src, int sw, int sh, int channels,
                               int dw, int dh) {
    std::vector<float> out((size_t)dw * dh * 3, 0.0f);
    if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0 || channels < 3)
        return out;
    nn::parallel_for(dh, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y) {
            const int sy0 = (int)((double)y * sh / dh);
            const int sy1 = std::max(sy0 + 1, (int)((double)(y + 1) * sh / dh));
            for (int x = 0; x < dw; ++x) {
                const int sx0 = (int)((double)x * sw / dw);
                const int sx1 = std::max(sx0 + 1, (int)((double)(x + 1) * sw / dw));
                double acc[3] = {0, 0, 0};
                for (int sy = sy0; sy < sy1; ++sy)
                    for (int sx = sx0; sx < sx1; ++sx) {
                        const uint8_t* p = &src[((size_t)sy * sw + sx) * channels];
                        for (int c = 0; c < 3; ++c) acc[c] += p[c];
                    }
                const double n = (double)(sy1 - sy0) * (sx1 - sx0) * 255.0;
                for (int c = 0; c < 3; ++c)
                    out[((size_t)y * dw + x) * 3 + c] = (float)(acc[c] / n);
            }
        }
    });
    return out;
}

void GeometryWarp::plan(const GeometryCamera& cam, int out_w, int out_h, bool split,
                        int patch, int max_face) {
    out_w_ = out_w;
    out_h_ = out_h;
    faces_.clear();
    axes_.clear();

    const camhost::Camera hc = host_camera(cam);
    const bool equirect = cam.model == (int)CameraModelType::EQUIRECTANGULAR;

    // ---- the faces: where each points and how much it spans ---------------
    std::vector<Frame> frames;
    if (split && equirect) {
        for (int k = 0; k < 6; ++k) {
            const double* a = camhost::equirect_face_axes() + 9 * k;
            Frame f;
            for (int m = 0; m < 3; ++m) { f.ax[m] = a[m]; f.ay[m] = a[3 + m]; f.az[m] = a[6 + m]; }
            f.ex = f.ey = kCubeOverlap;
            frames.push_back(f);
        }
    } else if (split) {
        // The front face, cropped (still centred) to what the lens holds.
        const double I[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
        const double cell[4] = {-kFrontOverlap, kFrontOverlap, -kFrontOverlap, kFrontOverlap};
        double bb[4];
        if (camhost::visible_bbox(hc, I, cell, bb)) {
            Frame f;
            for (int m = 0; m < 3; ++m) { f.ax[m] = I[m]; f.ay[m] = I[3 + m]; f.az[m] = I[6 + m]; }
            f.ex = std::fmin(kFrontOverlap, std::fmax(-bb[0], bb[1]));
            f.ey = std::fmin(kFrontOverlap, std::fmax(-bb[2], bb[3]));
            frames.push_back(f);
        }
        plan_ring(hc, frames);
    }
    if (split && frames.empty())
        throw std::runtime_error("no part of this camera's frame reaches a pinhole "
                                 "face; check the camera model and its coefficients");

    // ---- pixel sizes: the lens's own density where a face points ----------
    // The forward map reads at the sampling resolution, so a face at native
    // density resamples the frame once rather than through a smaller copy.
    const double sx = (double)out_w / cam.width, sy = (double)out_h / cam.height;
    double fs = 0.0;
    if (frames.empty()) {
        Face f;
        f.w = out_w;
        f.h = out_h;
        f.fx = cam.fx * sx;
        f.fy = cam.fy * sy;
        f.cx = cam.cx * sx;
        f.cy = cam.cy * sy;
        faces_.push_back(std::move(f));
        fs = 1.0;
    } else {
        for (const Frame& fr : frames) {
            const double theta = std::acos(std::clamp(fr.az[2], -1.0, 1.0));
            const double phi = std::atan2(fr.az[1], fr.az[0]);
            const double dens = source_density(hc, theta, phi);
            double focal = dens;
            const double longest = 2.0 * focal * std::fmax(fr.ex, fr.ey);
            const int cap = max_face > 0 ? std::min(max_face, kMaxFaceSide) : kMaxFaceSide;
            if (longest > cap) focal *= cap / longest;
            Face f;
            f.w = snap(2.0 * focal * fr.ex, patch);
            f.h = snap(2.0 * focal * fr.ey, patch);
            f.fx = f.fy = focal;
            f.cx = 0.5 * f.w;
            f.cy = 0.5 * f.h;
            // The extents follow the pixel grid so the pixels stay square.
            const double ex = f.w / (2.0 * focal), ey = f.h / (2.0 * focal);
            for (int m = 0; m < 3; ++m) {
                axes_.push_back(fr.ax[m] * ex);
            }
            for (int m = 0; m < 3; ++m) axes_.push_back(fr.ay[m] * ey);
            for (int m = 0; m < 3; ++m) axes_.push_back(fr.az[m]);
            faces_.push_back(std::move(f));
            fs = std::fmax(fs, focal / dens);
        }
        fs = std::fmin(fs, 1.0);
    }
    sw_ = std::max(1, (int)std::lround(cam.width * fs));
    sh_ = std::max(1, (int)std::lround(cam.height * fs));
    const double gx = (double)sw_ / cam.width, gy = (double)sh_ / cam.height;

    // A camera the parser had to fit reads through its TRUE lens; only that
    // direction has a closed form, so the inverse below still goes through the
    // fit. The one approximate step here, and sub-pixel.
    const bool from_source = cam.source_model >= 0;
    auto ray_to_pixel = [&](const double r[3], double& px, double& py) -> bool {
        double uv[2];
        if (from_source) {
            double u, v;
            if (!srccam::project(cam.source_model, cam.source_params, r[0], r[1], r[2],
                                 &u, &v))
                return false;
            px = u * gx;
            py = v * gy;
            return true;
        }
        if (!camhost::project_ray(r, cam.model, cam.distortion, cam.dist, uv))
            return false;
        px = (uv[0] * cam.fx + cam.cx) * gx;
        py = (uv[1] * cam.fy + cam.cy) * gy;
        return true;
    };

    // ---- forward: where each face pixel reads from ------------------------
    const int K = (int)faces_.size();
    for (int k = 0; k < K; ++k) {
        Face& f = faces_[(size_t)k];
        f.to_src.assign((size_t)f.w * f.h * 2, kNaN);
        f.valid.assign((size_t)f.w * f.h, 0.0f);
        nn::parallel_for(f.h, [&](int64_t y0, int64_t y1) {
            for (int64_t y = y0; y < y1; ++y)
                for (int x = 0; x < f.w; ++x) {
                    // One face keeps the camera's own axis and intrinsics; a
                    // split face is its own pinhole inside its frame.
                    double r[3];
                    const double u = ((double)x + 0.5 - f.cx) / f.fx;
                    const double v = ((double)y + 0.5 - f.cy) / f.fy;
                    if (axes_.empty()) {
                        r[0] = u; r[1] = v; r[2] = 1.0;
                    } else {
                        const double* a = axes_.data() + (size_t)k * 9;
                        const double ex = len3(a), ey = len3(a + 3);
                        for (int i = 0; i < 3; ++i)
                            r[i] = a[6 + i] + u / ex * a[i] + v / ey * a[3 + i];
                    }
                    double px = 0, py = 0;
                    if (!ray_to_pixel(r, px, py)) continue;
                    const size_t i = (size_t)y * f.w + x;
                    f.to_src[i * 2 + 0] = (float)px;
                    f.to_src[i * 2 + 1] = (float)py;
                    if (px >= 0 && py >= 0 && px <= sw_ && py <= sh_) f.valid[i] = 1.0f;
                }
        });
    }

    // ---- inverse: which faces reach each output pixel -----------------------
    const double ofx = cam.fx * sx, ofy = cam.fy * sy;
    const double ocx = cam.cx * sx, ocy = cam.cy * sy;
    const size_t n = (size_t)out_w * out_h;
    // Staged a row at a time: a slot per face per pixel would be many times
    // what this keeps, most of it empty.
    std::vector<std::vector<Contrib>> rows((size_t)out_h);
    std::vector<int32_t> count(n, 0);
    nn::parallel_for(out_h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y) {
            std::vector<Contrib>& row = rows[(size_t)y];
            row.reserve((size_t)out_w);
            for (int x = 0; x < out_w; ++x) {
                const size_t i = (size_t)y * out_w + x;
                const double u = ((double)x + 0.5 - ocx) / ofx;
                const double v = ((double)y + 0.5 - ocy) / ofy;
                double r[3];
                if (!camhost::generate_ray(u, v, cam.model, cam.distortion, cam.dist, r))
                    continue;

                if (axes_.empty()) {
                    // The pinhole face shares the optical axis, so the normal
                    // needs no rotation and the depth is already this camera's
                    // z. Ray depth is the secant of the undistorted angle.
                    if (r[2] <= 1e-9) continue;
                    const Face& f = faces_[0];
                    const double un = r[0] / r[2], vn = r[1] / r[2];
                    const double px = un * f.fx + f.cx, py = vn * f.fy + f.cy;
                    if (px < 0 || py < 0 || px > f.w || py > f.h) continue;
                    Contrib c;
                    c.face = 0;
                    c.px = (float)px;
                    c.py = (float)py;
                    c.w = 1.0f;
                    c.sz = 1.0f;
                    c.sr = (float)std::sqrt(un * un + vn * vn + 1.0);
                    row.push_back(c);
                    count[i] = 1;
                    continue;
                }

                for (int k = 0; k < K; ++k) {
                    const double* a = axes_.data() + (size_t)k * 9;
                    double tx = 0, ty = 0;
                    if (!face_coords(a, r, tx, ty)) continue;
                    const float w = face_weight(tx, ty);
                    if (w <= 0.0f) continue;
                    const Face& f = faces_[(size_t)k];
                    Contrib c;
                    c.face = k;
                    // 0.5-centred, which is what `sample` takes.
                    c.px = (float)((tx + 1.0) * 0.5 * f.w);
                    c.py = (float)((ty + 1.0) * 0.5 * f.h);
                    c.w = w;
                    // A point at face z-depth d sits at d * this vector, so
                    // its z and its length are the two conversions.
                    double rr[3];
                    for (int m = 0; m < 3; ++m)
                        rr[m] = a[6 + m] + tx * a[m] + ty * a[3 + m];
                    c.sz = (float)rr[2];
                    c.sr = (float)len3(rr);
                    row.push_back(c);
                    ++count[i];
                }
            }
        }
    });

    contrib_off_.assign(n + 1, 0);
    for (size_t i = 0; i < n; ++i) contrib_off_[i + 1] = contrib_off_[i] + count[i];
    contrib_.resize((size_t)contrib_off_[n]);
    if (contrib_.empty())
        throw std::runtime_error("no part of this camera's frame reaches a pinhole "
                                 "face; check the camera model and its coefficients");
    // A row's pixels are consecutive, so its stage lands in one run.
    nn::parallel_for(out_h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y)
            std::copy(rows[(size_t)y].begin(), rows[(size_t)y].end(),
                      contrib_.begin() + contrib_off_[(size_t)y * out_w]);
    });
}

void GeometryWarp::sampleFace(int k, const float* src, std::vector<float>& dst) const {
    const Face& f = faces_[(size_t)k];
    dst.assign((size_t)f.w * f.h * 3, 0.5f);
    nn::parallel_for(f.h, [&](int64_t y0, int64_t y1) {
        for (int64_t y = y0; y < y1; ++y)
            for (int x = 0; x < f.w; ++x) {
                const size_t i = (size_t)y * f.w + x;
                if (f.valid[i] == 0.0f) continue;   // outside the frame stays grey
                sample(src, sw_, sh_, 3, f.to_src[i * 2 + 0], f.to_src[i * 2 + 1],
                       &dst[i * 3]);
            }
    });
}

// Per-face log offsets reconciling the median log depth ratio over every
// pair's shared strip -- a graph Laplacian on faces() nodes. Median because
// the disagreements are worst where one face resolves an edge and one does not.
std::vector<double> GeometryWarp::alignFaces(
    const std::vector<std::vector<float>>& depth) const {
    const int K = faces();
    std::vector<double> out((size_t)K, 0.0);
    if (K < 2) return out;

    std::vector<std::vector<float>> pairs((size_t)K * K);
    const size_t n = (size_t)out_w_ * out_h_;
    std::vector<double> lv;
    std::vector<int> fk;
    for (size_t i = 0; i < n; ++i) {
        if (contrib_off_[i + 1] - contrib_off_[i] < 2) continue;
        lv.clear();
        fk.clear();
        for (int64_t c = contrib_off_[i]; c < contrib_off_[i + 1]; ++c) {
            const Contrib& t = contrib_[(size_t)c];
            if (t.w <= 0.0f || depth[(size_t)t.face].empty()) continue;
            const Face& f = faces_[(size_t)t.face];
            float ok = 0, d = 0;
            // What a face predicted over its mid-grey fill measures nothing.
            sample(f.valid.data(), f.w, f.h, 1, t.px, t.py, &ok);
            if (ok < 0.999f) continue;
            sample(depth[(size_t)t.face].data(), f.w, f.h, 1, t.px, t.py, &d);
            if (!(d > 0.0f)) continue;
            // As a distance from the camera: the one quantity two faces
            // agree on the meaning of.
            lv.push_back(std::log((double)d * t.sr));
            fk.push_back(t.face);
        }
        for (size_t a = 0; a < lv.size(); ++a)
            for (size_t b = a + 1; b < lv.size(); ++b) {
                int p = fk[a], q = fk[b];
                double d = lv[b] - lv[a];
                if (p > q) { std::swap(p, q); d = -d; }
                pairs[(size_t)p * K + q].push_back((float)d);
            }
    }

    // L s = rhs, where s[p] - s[q] should be the median log ratio.
    std::vector<double> L((size_t)K * K, 0.0), rhs((size_t)K, 0.0);
    bool any = false;
    for (int p = 0; p < K; ++p)
        for (int q = p + 1; q < K; ++q) {
            std::vector<float>& v = pairs[(size_t)p * K + q];
            // A handful of shared pixels is a corner, not a seam.
            if (v.size() < 256) continue;
            std::nth_element(v.begin(), v.begin() + (long)(v.size() / 2), v.end());
            const double d = v[v.size() / 2];
            L[(size_t)p * K + p] += 1.0;  L[(size_t)q * K + q] += 1.0;
            L[(size_t)p * K + q] -= 1.0;  L[(size_t)q * K + p] -= 1.0;
            rhs[(size_t)p] += d;          rhs[(size_t)q] -= d;
            any = true;
        }
    if (!any) return out;
    // Singular along the constant: the ridge picks the smallest solution and
    // the mean below leaves the frame's overall scale alone.
    for (int p = 0; p < K; ++p) L[(size_t)p * K + p] += 1e-3;

    auto at = [&](int r, int c) -> double& { return L[(size_t)r * K + c]; };
    for (int p = 0; p < K; ++p) {
        int piv = p;
        for (int q = p + 1; q < K; ++q)
            if (std::fabs(at(q, p)) > std::fabs(at(piv, p))) piv = q;
        if (std::fabs(at(piv, p)) < 1e-12) return out;
        std::swap_ranges(&at(p, 0), &at(p, 0) + K, &at(piv, 0));
        std::swap(rhs[(size_t)p], rhs[(size_t)piv]);
        for (int q = p + 1; q < K; ++q) {
            const double f = at(q, p) / at(p, p);
            for (int m = p; m < K; ++m) at(q, m) -= f * at(p, m);
            rhs[(size_t)q] -= f * rhs[(size_t)p];
        }
    }
    for (int p = K - 1; p >= 0; --p) {
        double s = rhs[(size_t)p];
        for (int m = p + 1; m < K; ++m) s -= at(p, m) * out[(size_t)m];
        out[(size_t)p] = s / at(p, p);
    }

    double mean = 0;
    for (double v : out) mean += v;
    mean /= K;
    // Past 4x is not a scale disagreement, it is a face that saw something
    // else; leave that one where the network put it.
    for (double& v : out) v = std::fmin(std::fmax(v - mean, -1.386), 1.386);
    return out;
}

void GeometryWarp::gather(const std::vector<std::vector<float>>& depth,
                          const std::vector<std::vector<float>>& normal, bool ray_depth,
                          std::vector<float>* out_depth,
                          std::vector<float>* out_normal) const {
    const size_t n = (size_t)out_w_ * out_h_;
    const bool do_depth = out_depth && !depth.empty();
    const bool do_normal = out_normal && !normal.empty();
    if (out_depth) out_depth->assign(n, 0.0f);
    if (out_normal) out_normal->assign(n * 3, 0.0f);
    if (!do_depth && !do_normal) return;

    const int K = faces();
    const std::vector<double> scale =
        do_depth ? alignFaces(depth) : std::vector<double>((size_t)K, 0.0);

    // Unit frames: the axis lengths are the face's extent, not a rotation.
    std::vector<double> frame((size_t)K * 9, 0.0);
    for (int k = 0; k < K; ++k)
        for (int r = 0; r < 3; ++r) {
            const double* a = axes_.empty() ? nullptr : axes_.data() + (size_t)k * 9 + r * 3;
            double* f = &frame[(size_t)k * 9 + r * 3];
            if (!a) { f[r] = 1.0; continue; }
            const double l = len3(a);
            for (int m = 0; m < 3; ++m) f[m] = a[m] / (l > 1e-12 ? l : 1.0);
        }

    nn::parallel_for(out_h_, [&](int64_t y0, int64_t y1) {
        for (size_t i = (size_t)y0 * out_w_; i < (size_t)y1 * out_w_; ++i) {
            double wd = 0, ld = 0, wn = 0, an[3] = {0, 0, 0};
            for (int64_t c = contrib_off_[i]; c < contrib_off_[i + 1]; ++c) {
                const Contrib& t = contrib_[(size_t)c];
                const size_t k = (size_t)t.face;
                const Face& fc = faces_[k];
                float ok = 0;
                sample(fc.valid.data(), fc.w, fc.h, 1, t.px, t.py, &ok);
                // Where the face saw the mid-grey fill instead of the frame,
                // the network still answered; it just answered about nothing.
                const double w = (double)t.w * ok;
                if (w <= 0.0) continue;

                // Linear depth is undefined past 90 degrees; the trainer's
                // own warp drops those rather than divide by a zero cosine.
                const double g = ray_depth ? t.sr : t.sz;
                if (do_depth && !depth[k].empty() && g > 1e-6) {
                    float d = 0;
                    sample(depth[k].data(), fc.w, fc.h, 1, t.px, t.py, &d);
                    if (d > 0.0f) {
                        ld += w * (std::log((double)d * g) + scale[k]);
                        wd += w;
                    }
                }
                if (do_normal && !normal[k].empty()) {
                    float nf[3];
                    sample(normal[k].data(), fc.w, fc.h, 3, t.px, t.py, nf);
                    const double* f = &frame[k * 9];
                    for (int m = 0; m < 3; ++m)
                        an[m] += w * (nf[0]*f[m] + nf[1]*f[3+m] + nf[2]*f[6+m]);
                    wn += w;
                }
            }
            // Blended in the log, where a scale error lives and where the
            // trainer's depth loss reads it.
            if (wd > 0.0) (*out_depth)[i] = (float)std::exp(ld / wd);
            if (wn > 0.0) {
                const double l = std::sqrt(an[0]*an[0] + an[1]*an[1] + an[2]*an[2]);
                if (l > 1e-12)
                    for (int m = 0; m < 3; ++m)
                        (*out_normal)[i * 3 + m] = (float)(an[m] / l);
            }
        }
    });
}

}  // namespace app
