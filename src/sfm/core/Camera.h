// Camera intrinsics + the single source of truth for camera-model metadata.
//
// Supported models (host mirror of sfm/shaders/common/camera.slang's BA models):
//   SimplePinhole -> COLMAP SIMPLE_PINHOLE (0): (f, cx, cy)
//   Pinhole       -> COLMAP PINHOLE (1):        (fx, fy, cx, cy)
//   Radial        -> COLMAP RADIAL (3):         (f, cx, cy, k1, k2)
//   OpenCV        -> COLMAP OPENCV (4):         (fx, fy, cx, cy, k1, k2, p1, p2)
//   OpenCVFisheye -> COLMAP OPENCV_FISHEYE (5), FullOpenCV -> FULL_OPENCV (6),
//   ThinPrismFisheye -> THIN_PRISM_FISHEYE (10)
//   Equirect      -> COLMAP EQUIRECTANGULAR (17): (w, h)
// Fisheye (Kannala-Brandt / >180 deg) is D29 phase C and needs the geometry core
// on unit bearings first (phase B). Equirectangular is spherical rather than
// perspective: the whole sphere projects, and its two parameters are the image
// dimensions, so it has no focal length and nothing for BA to refine (D49).
//
// The forward `project`/`unproject` are written once, using all of (fx,fy,k1,k2,
// p1,p2); models with fewer parameters just leave the rest zero (and keep
// fy==fx), so the same code serves every model and the pinhole-radial path stays
// numerically identical to before.
//
// Everything model-specific -- the COLMAP id, the BA shader-model index, the
// parameter count, the CLI name, and how Camera fields map to/from the flat
// parameter array -- lives in the kCamModelInfo table + pack()/unpack() below,
// so adding a model touches exactly one host table row (plus the shader model and
// its sfm/ba/Problem.h registry entry). BA parameter order is identical to COLMAP's for
// every model, so one pack/unpack serves both the solver and cameras.bin IO.
#pragma once

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "sfm/geometry/LinAlg.h"

namespace sfm {

enum class CamModel {
    SimplePinhole, Pinhole, Radial, OpenCV, OpenCVFisheye, FullOpenCV, ThinPrismFisheye,
    Equirect
};

struct Camera {
    uint32_t id = 0;
    int width = 0, height = 0;
    CamModel model = CamModel::Radial;
    double fx = 0, fy = 0, cx = 0, cy = 0;
    double k1 = 0, k2 = 0;   // radial
    double p1 = 0, p2 = 0;   // tangential (OpenCV / FullOpenCV / ThinPrismFisheye)
    double k3 = 0, k4 = 0;   // extra radial (fisheye k3,k4; FullOpenCV k3 + denominator k4)
    double k5 = 0, k6 = 0;   // FullOpenCV rational denominator (with k4)
    double sx1 = 0, sy1 = 0; // thin-prism (ThinPrismFisheye only)

    // How many of this camera's pixels one *measurement* pixel is worth: the
    // source image's size over the size the extractor actually ran SIFT at
    // (FeatureSet::pixelScale). 1 when nothing was downscaled.
    //
    // Every pixel threshold in the pipeline -- `--max-error`, the mapper's
    // `--max-error`, the BA loss delta, the merger's inlier radius -- is
    // specified in *extraction* pixels and converted with this, because that is
    // the frame feature-localization noise lives in: SIFT localizes to a third
    // of a pixel of the image it was given, whatever that image's relation to
    // the file on disk. Without it, `--max-error 4` silently means something
    // different at every `--quality` preset and for every camera in a
    // mixed-resolution capture (D47).
    //
    // Runtime only: it says nothing about the lens, so it is never written to
    // cameras.bin. A model read back from disk has 1.0 and the mapper restores
    // the real value from the features.
    double pixel_scale = 1.0;

    // A pixel threshold in this camera's own pixels, and as an angle. `px` is
    // always in extraction pixels; these are the only two conversions.
    double errPx(double px) const { return px * pixel_scale; }
    double errRad(double px) const { return px * pixel_scale / std::max(1e-6, focal()); }

    bool isFisheye() const {
        return model == CamModel::OpenCVFisheye || model == CamModel::ThinPrismFisheye;
    }
    // Spherical (omnidirectional) projection: the whole 4*pi is representable,
    // there is no focal length and no distortion. COLMAP's IsSpherical().
    bool isSpherical() const { return model == CamModel::Equirect; }
    // Whether a pixel's viewing ray must go through bearing() rather than the
    // z=1 normalized coordinates unproject() returns. True for anything that
    // can see past 90 degrees, which is the property every geometry call cares
    // about -- not "is it a fisheye" (D49).
    bool wideFov() const { return isFisheye() || isSpherical(); }

    // A sensible default when EXIF is unavailable. Pinhole: COLMAP's 1.2*max(w,h).
    // Fisheye: the equidistant estimate for a ~180 deg diagonal FOV, f = diag/pi
    // (half-diagonal = f*(pi/2)); the 1.2*max guess is ~4x too long for a fisheye
    // and feeds the focal<->distortion degeneracy, so a physical init matters (D33).
    // Equirectangular: not a guess at all -- the image *is* the calibration
    // (2*pi of azimuth over w pixels, pi of elevation over h), so any --focal or
    // EXIF focal is ignored.
    static Camera defaultFor(uint32_t id, int w, int h, double focal = 0,
                             CamModel m = CamModel::Radial) {
        Camera c;
        c.id = id;
        c.width = w;
        c.height = h;
        c.model = m;
        if (m == CamModel::Equirect) {
            c.fx = w / (2.0 * M_PI);   // pixels per radian of azimuth
            c.fy = h / M_PI;           // pixels per radian of elevation
            c.cx = w * 0.5;
            c.cy = h * 0.5;
            return c;
        }
        if (focal > 0)
            c.fx = c.fy = focal;
        else if (c.isFisheye())
            c.fx = c.fy = std::sqrt((double)w * w + (double)h * h) / M_PI;
        else
            c.fx = c.fy = 1.2 * std::max(w, h);
        c.cx = w * 0.5;
        c.cy = h * 0.5;
        return c;
    }

    // Isotropic focal, for code that wants a single scalar (RANSAC error scaling,
    // the mapper's focal search). fx==fy until BA refines a two-focal model.
    // For Equirect this is w/2pi -- pixels per radian, which is exactly what the
    // callers want it for (errRad below, and COLMAP's CamFromImgThreshold for
    // the spherical models does the same conversion).
    double focal() const { return 0.5 * (fx + fy); }
    void setFocal(double f) { fx = fy = f; }

    // Kannala-Brandt radial polynomial theta_d(theta) and its derivative, and
    // the Newton inversion theta(theta_d). Shared by the fisheye project/bearing.
    double kbThetaD(double th) const {
        double t2 = th * th;
        return th * (1.0 + t2 * (k1 + t2 * (k2 + t2 * (k3 + t2 * k4))));
    }
    double kbThetaFromThetaD(double thd) const {
        double th = thd;  // seed: theta_d ~= theta for small distortion
        for (int i = 0; i < 10; i++) {
            double t2 = th * th;
            double f = th * (1.0 + t2 * (k1 + t2 * (k2 + t2 * (k3 + t2 * k4)))) - thd;
            double fp = 1.0 + t2 * (3 * k1 + t2 * (5 * k2 + t2 * (7 * k3 + t2 * 9 * k4)));
            double next = th - f / fp;
            // Exact early-out, not a tolerance: once the update no longer moves
            // th, every remaining iteration recomputes the same f and fp and
            // lands on the same value, so stopping here returns the identical
            // double the fixed ten iterations would have. Newton gets there in
            // three or four, and this inversion runs once per observation per
            // filtering pass on a fisheye capture.
            if (next == th) return th;
            th = next;
        }
        return th;
    }

    // Camera-frame 3D point -> pixel.
    //  - pinhole family: Brown-Conrady radial+tangential; FullOpenCV divides the
    //    radial term by the rational denominator 1 + k4 r^2 + k5 r^4 + k6 r^6.
    //  - OpenCV_FISHEYE: Kannala-Brandt, radial-only in theta.
    //  - THIN_PRISM_FISHEYE: KB radial + tangential + thin-prism, all in the
    //    equidistant coords (uf,vf) = theta*(x,y)/r.
    // Both fisheye models are valid for rays past 90 deg (p.z <= 0).
    Vec2 project(const Vec3& p) const {
        if (model == CamModel::Equirect) {
            // azimuth from +z toward +x, elevation from the equator toward -y
            // (up). Matches COLMAP EQUIRECTANGULAR with (fx,fy,cx,cy) =
            // (w/2pi, h/pi, w/2, h/2); valid for every direction.
            double theta = std::atan2(p.x, p.z);
            double phi = std::atan2(-p.y, std::hypot(p.x, p.z));
            return {fx * theta + cx, cy - fy * phi};
        }
        if (model == CamModel::OpenCVFisheye) {
            double r = std::hypot(p.x, p.y);
            double theta = std::atan2(r, p.z);  // [0, pi], smooth at the axis
            double thd = kbThetaD(theta);
            double scale = r > 1e-12 ? thd / r : 0.0;  // on-axis -> principal point
            return {fx * scale * p.x + cx, fy * scale * p.y + cy};
        }
        if (model == CamModel::ThinPrismFisheye) {
            double r = std::hypot(p.x, p.y);
            double theta = std::atan2(r, p.z);
            double s = r > 1e-12 ? theta / r : 0.0;
            double uf = s * p.x, vf = s * p.y;   // equidistant coords, |uf,vf| = theta
            double rf2 = theta * theta;
            double radial = rf2 * (k1 + rf2 * (k2 + rf2 * (k3 + rf2 * k4)));
            double du = uf * radial + 2.0 * p1 * uf * vf + p2 * (rf2 + 2.0 * uf * uf) + sx1 * rf2;
            double dv = vf * radial + p1 * (rf2 + 2.0 * vf * vf) + 2.0 * p2 * uf * vf + sy1 * rf2;
            return {fx * (uf + du) + cx, fy * (vf + dv) + cy};
        }
        double xp = p.x / p.z, yp = p.y / p.z;
        double r2 = xp * xp + yp * yp;
        double radial = (model == CamModel::FullOpenCV) ?
            (1.0 + r2 * (k1 + r2 * (k2 + r2 * k3))) / (1.0 + r2 * (k4 + r2 * (k5 + r2 * k6))) :
            (1.0 + r2 * (k1 + r2 * (k2 + r2 * (k3 + r2 * k4))));
        double dx = xp * radial + 2.0 * p1 * xp * yp + p2 * (r2 + 2.0 * xp * xp);
        double dy = yp * radial + p1 * (r2 + 2.0 * yp * yp) + 2.0 * p2 * xp * yp;
        return {fx * dx + cx, fy * dy + cy};
    }

    // Pixel -> normalized (undistorted) image coordinates (x/z, y/z at z=1), by
    // fixed-point inversion of the distortion (few terms; distortion is small in
    // practice). A no-op for the undistorted pinhole models. For fisheye this is
    // only valid for the forward hemisphere (theta < 90 deg); wide rays must use
    // bearing() instead.
    Vec2 unproject(const Vec2& px) const {
        if (wideFov()) {  // wide rays need the bearing (z=1 coords blow up past 90 deg)
            Vec3 b = bearing(px);
            return {b.x / b.z, b.y / b.z};
        }
        double u = (px.x - cx) / fx, v = (px.y - cy) / fy;
        if (k1 == 0 && k2 == 0 && k3 == 0 && k4 == 0 && k5 == 0 && k6 == 0 &&
            p1 == 0 && p2 == 0)
            return {u, v};
        // 5 fixed-point steps: a contraction for the distortion magnitudes we
        // see, and exactly what the radial-only path did before (so the RADIAL
        // model stays bit-identical -- the tangential/k3 terms are literally zero
        // there). Revisit the count only if a strongly-distorted camera shows
        // unconverged undistortion.
        const bool rational = model == CamModel::FullOpenCV;
        double xu = u, yu = v;
        for (int i = 0; i < 5; i++) {
            double r2 = xu * xu + yu * yu;
            double radial = rational ?
                (1.0 + r2 * (k1 + r2 * (k2 + r2 * k3))) / (1.0 + r2 * (k4 + r2 * (k5 + r2 * k6))) :
                (1.0 + r2 * (k1 + r2 * (k2 + r2 * (k3 + r2 * k4))));
            double dtx = 2.0 * p1 * xu * yu + p2 * (r2 + 2.0 * xu * xu);
            double dty = p1 * (r2 + 2.0 * yu * yu) + 2.0 * p2 * xu * yu;
            xu = (u - dtx) / radial;
            yu = (v - dty) / radial;
        }
        return {xu, yu};
    }

    // Pixel -> unit viewing ray (bearing) in camera coordinates. For the
    // pinhole family this is normalize(unproject(px), 1), i.e. a forward ray
    // (z>0). This is the geometry core's interchange type (D31): a fisheye model
    // (D29 phase C) returns a ray that can point sideways or backward (z<=0),
    // which z=1 normalized coordinates cannot represent -- that is the whole
    // reason PnP / triangulation / relative pose work in bearings.
    Vec3 bearing(const Vec2& px) const {
        if (model == CamModel::Equirect) {
            double theta = (px.x - cx) / fx;
            double phi = (cy - px.y) / fy;
            double cp = std::cos(phi);
            return {cp * std::sin(theta), -std::sin(phi), cp * std::cos(theta)};
        }
        if (model == CamModel::OpenCVFisheye) {
            double u = (px.x - cx) / fx, v = (px.y - cy) / fy;
            double rd = std::hypot(u, v);  // = theta_d
            if (rd < 1e-12) return {0, 0, 1};
            double theta = kbThetaFromThetaD(rd);
            double s = std::sin(theta);
            return {s * u / rd, s * v / rd, std::cos(theta)};  // unit; z<0 past 90 deg
        }
        if (model == CamModel::ThinPrismFisheye) {
            double u = (px.x - cx) / fx, v = (px.y - cy) / fy;
            // Forward: u = theta_d*dir + tangential+prism, theta_d = theta*(1+k1
            // theta^2+...). Strip the small tangential/prism, invert the dominant
            // KB radial with the robust 1D Newton, and iterate. (A naive 2D
            // fixed-point diverges once the KB radial exceeds 1, i.e. past ~85 deg.)
            double uf = 0, vf = 0, theta = 0, dtx = 0, dty = 0;
            for (int it = 0; it < 6; it++) {
                double u2 = u - dtx, v2 = v - dty;
                double rd = std::hypot(u2, v2);  // = theta_d
                if (rd < 1e-12) { theta = uf = vf = 0; break; }
                theta = kbThetaFromThetaD(rd);   // theta_d -> theta (1D)
                uf = theta * u2 / rd;
                vf = theta * v2 / rd;
                double rf2 = theta * theta;
                double ndtx = 2.0 * p1 * uf * vf + p2 * (rf2 + 2.0 * uf * uf) + sx1 * rf2;
                double ndty = p1 * (rf2 + 2.0 * vf * vf) + 2.0 * p2 * uf * vf + sy1 * rf2;
                // Same exact fixed-point argument as the inner Newton: an
                // unchanged correction reproduces this iteration exactly.
                if (ndtx == dtx && ndty == dty) break;
                dtx = ndtx;
                dty = ndty;
            }
            if (theta < 1e-12) return {0, 0, 1};
            double s = std::sin(theta) / theta;
            return {s * uf, s * vf, std::cos(theta)};  // unit; z<0 past 90 deg
        }
        Vec2 n = unproject(px);
        return Vec3{n.x, n.y, 1.0}.normalized();
    }

    // 3x3 intrinsic matrix (ignoring distortion), for E/F conversions.
    Mat3 K() const { return {fx, 0, cx, 0, fy, cy, 0, 0, 1}; }
};

// ---- per-model metadata: the single source of truth -----------------------
// Adding a model = add a CamModel value, a row here, the shader struct, its
// ba.slang entry points and its sfm/ba/Problem.h registry row. Nothing else.
struct CamModelInfo {
    CamModel model;
    int colmap_id;         // COLMAP cameras.bin model id
    int ba_model;          // index into sfm/ba/Problem.h kModels
    int ba_params;         // params in the packIntrinsics layout BA reads
    int ba_focal;          // of those, leading params that are the focal length
    int ba_pp;             // of those, trailing params that are the principal point
    bool ba_refinable;     // false = BA reads the params but never changes them
    int colmap_params;     // params written to cameras.bin (packColmap layout)
    const char* cli_name;  // --camera-model value
};

// ba_model indices MUST match the order of kModels[] in sfm/ba/Problem.h.
// The BA layout is (focal, extra params, cx, cy) -- COLMAP's order with the
// principal point moved last, so "free" is always a prefix (D50).
static constexpr CamModelInfo kCamModelInfo[] = {
    {CamModel::SimplePinhole,    0, 4,  3, 1, 2, true,   3, "simple-pinhole"},
    {CamModel::Pinhole,          1, 5,  4, 2, 2, true,   4, "pinhole"},
    {CamModel::Radial,           3, 2,  5, 1, 2, true,   5, "radial"},
    {CamModel::OpenCV,           4, 3,  8, 2, 2, true,   8, "opencv"},
    {CamModel::OpenCVFisheye,    5, 6,  8, 2, 2, true,   8, "opencv-fisheye"},
    {CamModel::FullOpenCV,       6, 7, 12, 2, 2, true,  12, "full-opencv"},
    {CamModel::ThinPrismFisheye, 10, 8, 12, 2, 2, true,  12, "thin-prism-fisheye"},
    {CamModel::Equirect,         17, 9,  2, 2, 0, false,  2, "equirectangular"},
};

inline const CamModelInfo& camInfo(CamModel m) {
    for (const CamModelInfo& i : kCamModelInfo)
        if (i.model == m) return i;
    throw std::runtime_error("unknown camera model");
}
inline int camNumParams(CamModel m) { return camInfo(m).ba_params; }  // BA layout count
// How many leading BA parameters bundle adjustment may change: COLMAP's
// refine_focal_length / refine_extra_params / refine_principal_point (D50, D72).
// Held distortion pins the principal point -- it sits behind it in the prefix.
inline int camNumFreeParams(CamModel m, bool refine_pp = false, bool refine_extra = true) {
    const CamModelInfo& i = camInfo(m);
    if (!i.ba_refinable) return 0;
    if (!refine_extra) return i.ba_focal;
    return refine_pp ? i.ba_params : i.ba_params - i.ba_pp;
}
// Distortion coefficients this model carries, in the BA layout's middle block.
inline int camNumExtraParams(CamModel m) {
    const CamModelInfo& i = camInfo(m);
    return i.ba_params - i.ba_focal - i.ba_pp;
}
inline int camColmapParams(CamModel m) { return camInfo(m).colmap_params; }
inline int camBaModel(CamModel m) { return camInfo(m).ba_model; }
inline int camColmapId(CamModel m) { return camInfo(m).colmap_id; }

// Map a COLMAP model id to ours; throws on an unsupported model (the caller is
// reading a cameras.bin we cannot represent, which should be surfaced, not
// silently coerced).
inline CamModel camFromColmapId(int id) {
    for (const CamModelInfo& i : kCamModelInfo)
        if (i.colmap_id == id) return i.model;
    throw std::runtime_error("unsupported COLMAP camera model id " + std::to_string(id));
}
inline bool parseCamModelName(const std::string& s, CamModel& out) {
    for (const CamModelInfo& i : kCamModelInfo)
        if (s == i.cli_name) { out = i.model; return true; }
    return false;
}

// Camera fields <-> flat parameter array in the *BA* layout. `d` must have
// camNumParams(c.model) slots. The single place that knows each model's BA
// parameter order; sfm/map/Bundle.h packs/unpacks the solver's intrinsics with these.
//
// BA order is COLMAP's with the principal point moved to the end:
//   focal length(s), extra parameters, cx, cy
// so that holding the principal point fixed -- COLMAP's default, and ours --
// is expressible as "the group owns the first n columns" (D50). packColmap
// below is the translation back to COLMAP's own order for cameras.bin.
inline void packIntrinsics(const Camera& c, double* d) {
    switch (c.model) {
        case CamModel::SimplePinhole: d[0] = c.focal();
                                      d[1] = c.cx; d[2] = c.cy; break;
        case CamModel::Pinhole:       d[0] = c.fx; d[1] = c.fy;
                                      d[2] = c.cx; d[3] = c.cy; break;
        case CamModel::Radial:        d[0] = c.focal(); d[1] = c.k1; d[2] = c.k2;
                                      d[3] = c.cx; d[4] = c.cy; break;
        case CamModel::OpenCV:        d[0] = c.fx; d[1] = c.fy;
                                      d[2] = c.k1; d[3] = c.k2; d[4] = c.p1; d[5] = c.p2;
                                      d[6] = c.cx; d[7] = c.cy; break;
        case CamModel::OpenCVFisheye: d[0] = c.fx; d[1] = c.fy;
                                      d[2] = c.k1; d[3] = c.k2; d[4] = c.k3; d[5] = c.k4;
                                      d[6] = c.cx; d[7] = c.cy; break;
        case CamModel::FullOpenCV:    d[0] = c.fx; d[1] = c.fy;
                                      d[2] = c.k1; d[3] = c.k2; d[4] = c.p1; d[5] = c.p2;
                                      d[6] = c.k3; d[7] = c.k4; d[8] = c.k5; d[9] = c.k6;
                                      d[10] = c.cx; d[11] = c.cy; break;
        case CamModel::ThinPrismFisheye:
                                      d[0] = c.fx; d[1] = c.fy;
                                      d[2] = c.k1; d[3] = c.k2; d[4] = c.p1; d[5] = c.p2;
                                      d[6] = c.k3; d[7] = c.k4; d[8] = c.sx1; d[9] = c.sy1;
                                      d[10] = c.cx; d[11] = c.cy; break;
        // (w, h): the angular scales fx = w/2pi, fy = h/pi in COLMAP's spelling.
        // There is no principal point to hold or refine.
        case CamModel::Equirect:      d[0] = 2.0 * M_PI * c.fx; d[1] = M_PI * c.fy; break;
    }
}
inline void unpackIntrinsics(Camera& c, const double* d) {  // c.model set by caller
    c.k1 = c.k2 = c.p1 = c.p2 = c.k3 = c.k4 = c.k5 = c.k6 = c.sx1 = c.sy1 = 0;
    switch (c.model) {
        case CamModel::SimplePinhole: c.setFocal(d[0]);
                                      c.cx = d[1]; c.cy = d[2]; break;
        case CamModel::Pinhole:       c.fx = d[0]; c.fy = d[1];
                                      c.cx = d[2]; c.cy = d[3]; break;
        case CamModel::Radial:        c.setFocal(d[0]); c.k1 = d[1]; c.k2 = d[2];
                                      c.cx = d[3]; c.cy = d[4]; break;
        case CamModel::OpenCV:        c.fx = d[0]; c.fy = d[1];
                                      c.k1 = d[2]; c.k2 = d[3]; c.p1 = d[4]; c.p2 = d[5];
                                      c.cx = d[6]; c.cy = d[7]; break;
        case CamModel::OpenCVFisheye: c.fx = d[0]; c.fy = d[1];
                                      c.k1 = d[2]; c.k2 = d[3]; c.k3 = d[4]; c.k4 = d[5];
                                      c.cx = d[6]; c.cy = d[7]; break;
        case CamModel::FullOpenCV:    c.fx = d[0]; c.fy = d[1];
                                      c.k1 = d[2]; c.k2 = d[3]; c.p1 = d[4]; c.p2 = d[5];
                                      c.k3 = d[6]; c.k4 = d[7]; c.k5 = d[8]; c.k6 = d[9];
                                      c.cx = d[10]; c.cy = d[11]; break;
        case CamModel::ThinPrismFisheye:
                                      c.fx = d[0]; c.fy = d[1];
                                      c.k1 = d[2]; c.k2 = d[3]; c.p1 = d[4]; c.p2 = d[5];
                                      c.k3 = d[6]; c.k4 = d[7]; c.sx1 = d[8]; c.sy1 = d[9];
                                      c.cx = d[10]; c.cy = d[11]; break;
        case CamModel::Equirect:      c.fx = d[0] / (2.0 * M_PI); c.fy = d[1] / M_PI;
                                      c.cx = d[0] * 0.5; c.cy = d[1] * 0.5; break;
    }
}

// Set the distortion coefficients from a flat list in this model's BA order
// (`--distortion`). Missing entries are zeroed, surplus ones ignored, so a list
// written for one model does not silently mean something else on another.
inline void setExtraParams(Camera& c, const std::vector<double>& v) {
    const int n = camNumExtraParams(c.model);
    if (n <= 0) return;
    double d[12];
    packIntrinsics(c, d);
    const int off = camInfo(c.model).ba_focal;
    for (int i = 0; i < n; i++) d[off + i] = i < (int)v.size() ? v[i] : 0.0;
    unpackIntrinsics(c, d);
}

// Camera fields -> the *COLMAP* cameras.bin layout (camColmapParams slots):
// focal length(s), cx, cy, then the extra parameters.
inline void packColmap(const Camera& c, double* d) {
    switch (c.model) {
        case CamModel::SimplePinhole: d[0] = c.focal(); d[1] = c.cx; d[2] = c.cy; break;
        case CamModel::Pinhole:       d[0] = c.fx; d[1] = c.fy; d[2] = c.cx; d[3] = c.cy; break;
        case CamModel::Radial:        d[0] = c.focal(); d[1] = c.cx; d[2] = c.cy;
                                      d[3] = c.k1; d[4] = c.k2; break;
        case CamModel::OpenCV:        d[0] = c.fx; d[1] = c.fy; d[2] = c.cx; d[3] = c.cy;
                                      d[4] = c.k1; d[5] = c.k2; d[6] = c.p1; d[7] = c.p2; break;
        case CamModel::OpenCVFisheye: d[0] = c.fx; d[1] = c.fy; d[2] = c.cx; d[3] = c.cy;
                                      d[4] = c.k1; d[5] = c.k2; d[6] = c.k3; d[7] = c.k4; break;
        case CamModel::FullOpenCV:    d[0] = c.fx; d[1] = c.fy; d[2] = c.cx; d[3] = c.cy;
                                      d[4] = c.k1; d[5] = c.k2; d[6] = c.p1; d[7] = c.p2;
                                      d[8] = c.k3; d[9] = c.k4; d[10] = c.k5; d[11] = c.k6; break;
        case CamModel::ThinPrismFisheye:
                                      d[0] = c.fx; d[1] = c.fy; d[2] = c.cx; d[3] = c.cy;
                                      d[4] = c.k1; d[5] = c.k2; d[6] = c.p1; d[7] = c.p2;
                                      d[8] = c.k3; d[9] = c.k4; d[10] = c.sx1; d[11] = c.sy1; break;
        // Not 2*pi*fx: that round trip misses the integer width by an ulp.
        case CamModel::Equirect:      d[0] = c.width; d[1] = c.height; break;
    }
}
// COLMAP layout -> fields.
inline void unpackColmap(Camera& c, const double* d) {
    c.k1 = c.k2 = c.p1 = c.p2 = c.k3 = c.k4 = c.k5 = c.k6 = c.sx1 = c.sy1 = 0;
    switch (c.model) {
        case CamModel::SimplePinhole: c.setFocal(d[0]); c.cx = d[1]; c.cy = d[2]; break;
        case CamModel::Pinhole:       c.fx = d[0]; c.fy = d[1]; c.cx = d[2]; c.cy = d[3]; break;
        case CamModel::Radial:        c.setFocal(d[0]); c.cx = d[1]; c.cy = d[2];
                                      c.k1 = d[3]; c.k2 = d[4]; break;
        case CamModel::OpenCV:        c.fx = d[0]; c.fy = d[1]; c.cx = d[2]; c.cy = d[3];
                                      c.k1 = d[4]; c.k2 = d[5]; c.p1 = d[6]; c.p2 = d[7]; break;
        case CamModel::OpenCVFisheye: c.fx = d[0]; c.fy = d[1]; c.cx = d[2]; c.cy = d[3];
                                      c.k1 = d[4]; c.k2 = d[5]; c.k3 = d[6]; c.k4 = d[7]; break;
        case CamModel::FullOpenCV:    c.fx = d[0]; c.fy = d[1]; c.cx = d[2]; c.cy = d[3];
                                      c.k1 = d[4]; c.k2 = d[5]; c.p1 = d[6]; c.p2 = d[7];
                                      c.k3 = d[8]; c.k4 = d[9]; c.k5 = d[10]; c.k6 = d[11]; break;
        case CamModel::ThinPrismFisheye:
                                      c.fx = d[0]; c.fy = d[1]; c.cx = d[2]; c.cy = d[3];
                                      c.k1 = d[4]; c.k2 = d[5]; c.p1 = d[6]; c.p2 = d[7];
                                      c.k3 = d[8]; c.k4 = d[9]; c.sx1 = d[10]; c.sy1 = d[11]; break;
        case CamModel::Equirect:      c.fx = d[0] / (2.0 * M_PI); c.fy = d[1] / M_PI;
                                      c.cx = d[0] * 0.5; c.cy = d[1] * 0.5; break;
    }
}

}  // namespace sfm
