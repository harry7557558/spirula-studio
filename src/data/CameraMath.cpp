#include "data/CameraMath.h"

#include "core/CameraModel.h"
#include "data/SourceCamera.h"

#include <algorithm>
#include <cmath>

namespace camhost {
namespace {

enum { M_PINHOLE = 0, M_FISHEYE = 1, M_EQUISOLID = 2, M_EQUIRECT = 3 };

constexpr double kPi = 3.14159265358979323846;   // MSVC has no M_PI by default

void distort_jac(double qx, double qy, int tier, const float* d, double J[4]) {
    const double e = 1e-5;
    double f[2], fx[2], fy[2];
    distort_lens(qx, qy, tier, d, f);
    distort_lens(qx + e, qy, tier, d, fx);
    distort_lens(qx, qy + e, tier, d, fy);
    J[0] = (fx[0]-f[0])/e; J[1] = (fx[1]-f[1])/e;   // j00 j10
    J[2] = (fy[0]-f[0])/e; J[3] = (fy[1]-f[1])/e;   // j01 j11
}

bool norm3(double x, double y, double z, double out[3]) {
    double l = std::sqrt(x*x + y*y + z*z);
    if (l < 1e-12) l = 1;
    out[0] = x/l; out[1] = y/l; out[2] = z/l;
    return true;
}

}  // namespace

bool has_distortion(int tier, const float* d) {
    if (tier == (int)CameraDistortionType::None) return false;
    for (int i = 0; i < kCameraDistortionParams; i++) if (d[i] != 0.0f) return true;
    return false;
}

// Host mirror of the tier structs in shaders/projection_utils.slang; the slot
// order differs per tier, which is why the tier travels with the coefficients.
void distort_lens(double u, double v, int tier, const float* d, double out[2]) {
    double r2 = u*u + v*v;
    if (tier == (int)CameraDistortionType::None) {
        out[0] = u; out[1] = v;
        return;
    }
    if (tier == (int)CameraDistortionType::OpenCV) {
        double radial = 1 + r2*(d[0] + r2*d[1]);
        out[0] = u*radial + 2*d[2]*u*v + d[3]*(r2 + 2*u*u);
        out[1] = v*radial + 2*d[3]*u*v + d[2]*(r2 + 2*v*v);
        return;
    }
    if (tier == (int)CameraDistortionType::Rational) {
        double radial = (1 + r2*(d[0] + r2*(d[1] + r2*d[2])))
                      / (1 + r2*(d[3] + r2*(d[4] + r2*d[5])));
        out[0] = u*radial + 2*d[6]*u*v + d[7]*(r2 + 2*u*u);
        out[1] = v*radial + 2*d[7]*u*v + d[6]*(r2 + 2*v*v);
        return;
    }
    double radial = 1 + r2*(d[0] + r2*(d[1] + r2*(d[2] + r2*d[3])));
    out[0] = u*radial + 2*d[4]*u*v + d[5]*(r2 + 2*u*u) + d[6]*r2;
    out[1] = v*radial + 2*d[5]*u*v + d[4]*(r2 + 2*v*v) + d[7]*r2;
}

// Newton solve distort(q) = (u,v), with undistort_point_0's failure
// conditions (well-posed forward Jacobian + re-distorts within 0.01).
bool undistort_lens(double u, double v, int tier, const float* d, double out[2]) {
    double qx = u, qy = v;
    for (int it = 0; it < 8; it++) {
        double f[2], J[4];
        distort_lens(qx, qy, tier, d, f);
        distort_jac(qx, qy, tier, d, J);
        double det = J[0]*J[3] - J[2]*J[1];
        if (std::fabs(det) < 1e-12) break;
        double rx = f[0] - u, ry = f[1] - v, inv = 1/det;
        qx -= ( J[3]*rx - J[2]*ry)*inv;
        qy -= (-J[1]*rx + J[0]*ry)*inv;
    }
    if (!std::isfinite(qx) || !std::isfinite(qy)) return false;
    double J[4], f[2];
    distort_jac(qx, qy, tier, d, J);
    double det = J[0]*J[3] - J[2]*J[1];
    if (std::fmin(det, std::fmin(J[0], J[3])) <= 0) return false;   // folded
    distort_lens(qx, qy, tier, d, f);
    if (std::hypot(f[0]-u, f[1]-v) >= 0.01) return false;
    out[0] = qx; out[1] = qy;
    return true;
}

bool valid_distortion(double u, double v, int tier, const float* d) {
    if (tier == (int)CameraDistortionType::None) return true;
    double J[4], f[2];
    distort_jac(u, v, tier, d, J);
    const double det = J[0]*J[3] - J[2]*J[1];
    const double jd = std::fmin(det, std::fmin(J[0], J[3]));
    distort_lens(u, v, tier, d, f);
    return jd > 0.25 && jd < 4.0 && (u*f[0] + v*f[1]) >= 0.0;
}

void distort_point(double u, double v, int model, int tier, const float* d,
                   double out[2]) {
    if (model == M_EQUIRECT) {
        out[0] = u; out[1] = v;
        return;
    }
    if (model == M_FISHEYE || model == M_EQUISOLID) {
        const double r = std::hypot(u, v);
        const double theta = std::atan(r);
        double k;
        if (model == M_FISHEYE)
            k = (r < 1e-3) ? (1.0 - theta*theta/6.0) : (theta / r);
        else
            k = (r < 1e-3) ? (1.0 - theta*theta/24.0)
                           : (2.0 * std::sin(0.5*theta) / r);
        u *= k; v *= k;
    }
    distort_lens(u, v, tier, d, out);
}

bool undistort_point(double u, double v, int model, int tier, const float* d,
                     double out[2]) {
    double ray[3];
    if (!generate_ray(u, v, model, tier, d, ray)) return false;
    if (ray[2] <= 1e-12) return false;
    out[0] = ray[0] / ray[2];
    out[1] = ray[1] / ray[2];
    return true;
}

bool generate_ray(double u, double v, int model, int tier, const float* d,
                  double out[3]) {
    if (model == M_EQUIRECT) {
        if (std::fabs(u) > kPi || std::fabs(v) > kPi/2) return false;
        double cl = std::cos(v);
        out[0] = cl*std::sin(u); out[1] = std::sin(v); out[2] = cl*std::cos(u);
        return true;
    }
    double uu = u, vv = v;
    if (has_distortion(tier, d)) {
        double q[2];
        if (!undistort_lens(u, v, tier, d, q)) return false;
        uu = q[0]; vv = q[1];
    }
    double r = std::hypot(uu, vv);
    if (model == M_FISHEYE) {                        // equidistant: theta = r
        if (r >= kPi) return false;
        double s = r < 1e-3 ? (1 - r*r/6) : (std::sin(r)/r);
        return norm3(uu*s, vv*s, std::cos(r), out);
    }
    if (model == M_EQUISOLID) {                      // r = 2 sin(theta/2)
        if (r >= 2.0) return false;
        double k = std::sqrt(std::fmax(0.0, 1 - 0.25*r*r));
        return norm3(uu*k, vv*k, 1 - 0.5*r*r, out);
    }
    return norm3(uu, vv, 1.0, out);                  // pinhole
}

bool project_ray(const double ray[3], int model, int tier, const float* d,
                 double out[2]) {
    if (model == M_EQUIRECT) {
        out[0] = std::atan2(ray[0], ray[2]);
        out[1] = std::atan2(ray[1], std::hypot(ray[0], ray[2]));
        return true;
    }
    const double rz = ray[2];
    const double rxy = std::hypot(ray[0], ray[1]);
    if (model == M_PINHOLE) {
        if (rz <= 1e-12) return false;
        const double u = ray[0]/rz, v = ray[1]/rz;
        if (!valid_distortion(u, v, tier, d)) return false;
        distort_lens(u, v, tier, d, out);
        return true;
    }
    // Both wide models are angular, so theta comes straight off the ray. This
    // does NOT go through distort_point: that one recovers theta as atan(r),
    // which is the wrong branch past 90 degrees -- the half a split keeps.
    const double theta = std::atan2(rxy, rz);
    if (theta >= kPi) return false;
    const double r_ang = (model == M_FISHEYE) ? theta : 2.0 * std::sin(0.5 * theta);
    const double s = (rxy < 1e-12) ? 0.0 : r_ang / rxy;
    const double u = ray[0] * s, v = ray[1] * s;
    if (!valid_distortion(u, v, tier, d)) return false;
    distort_lens(u, v, tier, d, out);
    return true;
}

double pinhole_coverage(int model, int width, int height, double fx, double fy) {
    if (model == M_EQUIRECT) return 0.0;
    if (model != M_FISHEYE && model != M_EQUISOLID) return 1.0;
    if (!(fx > 0) || !(fy > 0) || width <= 0 || height <= 0) return 1.0;

    // Both images share an axis and intrinsics, so at azimuth phi the frame
    // reaches radius rect(phi) in the wide one and tan(theta) = rect(phi) in
    // the pinhole -- the same expression, hence no root finding.
    const double U = 0.5 * width / fx, V = 0.5 * height / fy;
    const auto kept = [&](double phi) {
        const double c = std::cos(phi), s = std::sin(phi);
        const double rect = std::fmin(U / std::fmax(c, 1e-300),
                                      V / std::fmax(s, 1e-300));
        const double theta = std::atan(rect);
        const double r = (model == M_FISHEYE) ? theta : 2.0 * std::sin(0.5 * theta);
        return r * r;
    };
    // Simpson on each side of the corner, where the frame's radius has a kink.
    const double corner = std::atan2(V, U);
    double area = 0.0;
    for (int half = 0; half < 2; ++half) {
        const double a = half ? corner : 0.0, b = half ? 0.5 * kPi : corner;
        const int n = 128;                       // even, for Simpson
        const double h = (b - a) / n;
        double s = kept(a) + kept(b);
        for (int i = 1; i < n; ++i) s += kept(a + i * h) * (i & 1 ? 4 : 2);
        area += s * h / 3.0;
    }
    // Against 2 * (the quadrant's area): both are integrals of r^2/2 over a
    // quarter turn, so the leading half cancels.
    return std::fmin(1.0, area / (2.0 * U * V));
}

bool splits_to_pinhole_faces(int model, int width, int height, double fx,
                             double fy) {
    // A quarter of the frame is what one undistortion may throw away.
    return pinhole_coverage(model, width, height, fx, fy) <= 0.75;
}


// ===========================================================================
// Visibility
// ===========================================================================

bool ray_in_frame(const Camera& cam, const double ray[3], double px[2]) {
    if (cam.source_model >= 0) {
        if (!srccam::project(cam.source_model, cam.source_params,
                             ray[0], ray[1], ray[2], &px[0], &px[1]))
            return false;
    } else {
        double uv[2];
        if (!project_ray(ray, cam.model, cam.tier, cam.dist, uv)) return false;
        px[0] = uv[0] * cam.fx + cam.cx;
        px[1] = uv[1] * cam.fy + cam.cy;
    }
    // A pixel of slack at the panorama's poles, where a half-row's rounding
    // would otherwise cut them off.
    const double tol = cam.model == M_EQUIRECT ? 1.0 : 0.0;
    return px[0] >= 0.0 && px[0] <= cam.width &&
           px[1] >= -tol && px[1] <= cam.height + tol;
}

bool visible_bbox(const Camera& cam, const double R[9], const double cell[4],
                  double bbox[4], double* fraction) {
    constexpr int n = 64;
    const double lo[2] = {cell[0], cell[2]}, hi[2] = {cell[1], cell[3]};
    auto at = [&](int axis, double t) { return lo[axis] + (hi[axis] - lo[axis]) * t; };
    auto seen = [&](double x, double y) {
        double r[3], px[2];
        for (int m = 0; m < 3; ++m) r[m] = R[6 + m] + x * R[m] + y * R[3 + m];
        return ray_in_frame(cam, r, px);
    };
    int imin[2] = {n, n}, imax[2] = {-1, -1}, hits = 0;
    for (int j = 0; j < n; ++j)
        for (int i = 0; i < n; ++i)
            if (seen(at(0, (i + 0.5) / n), at(1, (j + 0.5) / n))) {
                imin[0] = std::min(imin[0], i); imax[0] = std::max(imax[0], i);
                imin[1] = std::min(imin[1], j); imax[1] = std::max(imax[1], j);
                ++hits;
            }
    if (fraction) *fraction = (double)hits / (n * n);
    if (imax[0] < 0) return false;

    // Whether the line across the cell at parameter t of `axis` meets anything.
    auto line_seen = [&](int axis, double t) {
        for (int i = 0; i < n; ++i) {
            const double o = at(1 - axis, (i + 0.5) / n);
            if (axis == 0 ? seen(at(0, t), o) : seen(o, at(1, t))) return true;
        }
        return false;
    };
    // Each edge: the outermost sample line that saw something, bisected
    // against the next one out (or the cell's edge), then a quarter step of
    // slack so a rounded pixel never lands on a ray the grid proved visible.
    for (int axis = 0; axis < 2; ++axis)
        for (int side = 0; side < 2; ++side) {
            const int k = side ? imax[axis] : imin[axis];
            double t_in = (k + 0.5) / n;
            double t_out = std::clamp(side ? (k + 1.5) / n : (k - 0.5) / n, 0.0, 1.0);
            if (line_seen(axis, t_out)) {
                t_in = t_out;
            } else {
                for (int it = 0; it < 10; ++it) {
                    const double m = 0.5 * (t_in + t_out);
                    (line_seen(axis, m) ? t_in : t_out) = m;
                }
            }
            const double t = t_in + (side ? 0.25 : -0.25) / n;
            bbox[axis * 2 + side] = at(axis, std::clamp(t, 0.0, 1.0));
        }
    return true;
}

std::vector<double> visible_boundary(const Camera& cam, int samples) {
    std::vector<double> out((size_t)std::max(samples, 0), 0.0);
    constexpr int coarse = 72;   // 2.5-degree steps, then bisection
    for (int k = 0; k < samples; ++k) {
        const double phi = 2.0 * kPi * k / samples;
        const double cphi = std::cos(phi), sphi = std::sin(phi);
        auto seen = [&](double theta) {
            const double r[3] = {std::sin(theta) * cphi, std::sin(theta) * sphi,
                                 std::cos(theta)};
            double px[2];
            return ray_in_frame(cam, r, px);
        };
        double lo = 0.0, hi = -1.0;
        if (!seen(1e-6)) continue;
        for (int i = 1; i <= coarse; ++i) {
            const double th = kPi * i / coarse;
            if (th >= kPi) { hi = kPi; break; }
            if (!seen(th)) { hi = th; break; }
            lo = th;
        }
        if (hi < 0.0) hi = kPi;
        for (int it = 0; it < 16; ++it) {
            const double m = 0.5 * (lo + hi);
            (seen(m) ? lo : hi) = m;
        }
        out[(size_t)k] = lo;
    }
    return out;
}


// ===========================================================================
// The trainer's split
// ===========================================================================

namespace {

// Side frames all take ay = +z: the lens boundary crops them along ay. The
// back frame only ever holds a ray of a lens past 270 degrees.
const double kFisheyeAxes[6][3][3] = {
    {{ 1, 0, 0}, { 0, 1, 0}, { 0, 0, 1}},
    {{ 0, 1, 0}, { 0, 0, 1}, { 1, 0, 0}},
    {{-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0}},
    {{ 0,-1, 0}, { 0, 0, 1}, {-1, 0, 0}},
    {{ 1, 0, 0}, { 0, 0, 1}, { 0,-1, 0}},
    {{ 1, 0, 0}, { 0,-1, 0}, { 0, 0,-1}},
};
// The four equator faces all take ay = +y (the panorama's down), so a partial
// vertical field of view crops them along ay.
const double kEquirectAxes[6][3][3] = {
    {{ 1, 0, 0}, { 0, 1, 0}, { 0, 0, 1}},
    {{ 0, 0,-1}, { 0, 1, 0}, { 1, 0, 0}},
    {{-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0}},
    {{ 0, 0, 1}, { 0, 1, 0}, {-1, 0, 0}},
    {{ 1, 0, 0}, { 0, 0, 1}, { 0,-1, 0}},
    {{-1, 0, 0}, { 0, 1, 0}, { 0, 0,-1}},
};

// Face sides round up to this, so near-equal crops share a size and a pass.
constexpr int kSizeStep = 32;

// A frame under this share of the image's rays is noise, not a face (what
// --check calls a hole). A fisheye's back frame is opt-in and then needs a
// quarter of its own 90 degrees: past 135 degrees a real lens is folding.
constexpr double kMinShare       = 0.002;
constexpr double kMinVisibleBack = 0.25;

struct Crop {
    int face;
    int px0[2], px1[2];   // pixel bounds of the visible box, per axis
};

// The side of a face over a crop of `ext` pixels: never under half the frame,
// so a side band is at least the 45..90 degree ring and no face is a sliver.
int face_side(int ext, int half) {
    const int want = (ext + kSizeStep - 1) / kSizeStep * kSizeStep;
    return std::min(2 * half, std::max(want, half));
}

// Pixel origin of a face of `side` along `axis`: centred on the crop and kept
// inside the frame's own 90 degrees.
int face_origin(const Crop& c, int axis, int side, int half) {
    const int ext = c.px1[axis] - c.px0[axis];
    const int p = c.px0[axis] - (side - ext) / 2;
    return std::clamp(p, -half, std::max(-half, half - side));
}

}  // namespace

const double* fisheye_face_axes()  { return &kFisheyeAxes[0][0][0]; }
const double* equirect_face_axes() { return &kEquirectAxes[0][0][0]; }

std::vector<SplitFace> plan_split_faces(const Camera& cam_in, FaceFit fit,
                                        bool back_face) {
    Camera cam = cam_in;
    const bool equi = cam.model == M_EQUIRECT;
    if (equi) {
        cam.fx = cam.fy = cam.width / (2.0 * kPi);
        cam.cx = cam.width / 2.0;
        cam.cy = cam.height / 2.0;
        cam.tier = (int)CameraDistortionType::None;
        cam.source_model = -1;
    }
    // The density of the uncropped split: 5 faces for a fisheye, whose back
    // frame is empty for any real lens, 6 for a panorama.
    const int K = equi ? 6 : 5;
    const double* axes = equi ? equirect_face_axes() : fisheye_face_axes();
    const int S = (int)std::ceil(std::sqrt((double)cam.width * cam.height / K));
    const int half = (S + 1) / 2;
    const double f = half;

    // Every frame's cell is the same solid angle, so a frame's share of the
    // image is its visible fraction over the sum of them.
    const double cell[4] = {-1.0, 1.0, -1.0, 1.0};
    double bbox[6][4], fraction[6] = {}, total = 0.0;
    for (int k = 0; k < 6; ++k) {
        if (!visible_bbox(cam, axes + 9 * k, cell, bbox[k], &fraction[k]))
            fraction[k] = 0.0;
        total += fraction[k];
    }
    std::vector<Crop> crops;
    for (int k = 0; k < 6; ++k) {
        if (fraction[k] < kMinShare * total) continue;
        if (k == 5 && !equi && (!back_face || fraction[k] < kMinVisibleBack))
            continue;
        Crop c;
        c.face = k;
        for (int a = 0; a < 2; ++a) {
            c.px0[a] = std::max(-half, (int)std::floor(bbox[k][a * 2] * f));
            c.px1[a] = std::min(half, (int)std::ceil(bbox[k][a * 2 + 1] * f));
        }
        crops.push_back(c);
    }
    // A camera that sees nothing is broken; render it uncropped rather than not at all.
    if (crops.empty())
        for (int k = 0; k < K; ++k)
            crops.push_back(Crop{k, {-half, -half}, {half, half}});

    // One face per frame. Uniform takes the largest side per axis, so every
    // face still lies inside its own frame and the batch renders in one pass.
    std::vector<SplitFace> out;
    int side[2] = {0, 0};
    for (const Crop& cr : crops) {
        SplitFace sf;
        sf.face = cr.face;
        sf.fx = sf.fy = f;
        sf.crop_w = cr.px1[0] - cr.px0[0];
        sf.crop_h = cr.px1[1] - cr.px0[1];
        sf.width  = face_side(sf.crop_w, half);
        sf.height = face_side(sf.crop_h, half);
        side[0] = std::max(side[0], sf.width);
        side[1] = std::max(side[1], sf.height);
        out.push_back(sf);
    }
    for (size_t i = 0; i < out.size(); ++i) {
        SplitFace& sf = out[i];
        if (fit == FaceFit::Uniform) {
            sf.width  = side[0];
            sf.height = side[1];
        }
        sf.cx = -face_origin(crops[i], 0, sf.width, half);
        sf.cy = -face_origin(crops[i], 1, sf.height, half);
    }
    // Equal sizes adjacent: a pass covers one run of the face axis, so the
    // order is what decides how many passes a camera costs.
    if (fit == FaceFit::PerFace)
        std::stable_sort(out.begin(), out.end(),
                         [](const SplitFace& a, const SplitFace& b) {
                             if (a.height != b.height) return a.height > b.height;
                             return a.width > b.width;
                         });
    return out;
}

}  // namespace camhost
