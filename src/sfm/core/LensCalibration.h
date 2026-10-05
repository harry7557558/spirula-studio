#pragma once

// A video's factory lens calibration (Telemetry.h LensCalibration) as #119's
// per-camera COLMAP params: one CameraOverride per lens folder, only where no
// setting a person gave already covers it. docs/notes/imu-gps-for-sfm.md §2.3.

#include "sfm/SfmConfig.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/Image.h"
#include "sfm/core/Telemetry.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace sfm {

struct LensFit {
    std::vector<double> params;   // COLMAP order of the model asked for
    double max_px = 0;            // the radial refit's worst error, pixels
    double theta_max = 0;         // refitted over [0, theta_max], radians
};

// The fisheye models stop at k4 and the lens has a k5: the radial curve is
// refitted, focal free, to the inscribed circle; 1.2-2.0 px worst, +0.1 px on
// real matches (imu-gps-for-sfm.md §2.3). False for a non-fisheye model.
inline bool lensParams(const LensCalibration& l, CamModel model, LensFit& out) {
    out = LensFit();
    if (model != CamModel::ThinPrismFisheye && model != CamModel::OpenCVFisheye) return false;
    if (!(l.fx > 0) || l.width <= 0 || l.height <= 0) return false;
    auto thetaD = [&](double th) {
        const double t2 = th * th;
        return th * (1 + t2 * (l.k[0] + t2 * (l.k[1] + t2 * (l.k[2] + t2 * (l.k[3] + t2 * l.k[4])))));
    };
    const double edge = 0.5 * std::min(l.width, l.height) / l.fx;
    double lo = 0, hi = 110 * M_PI / 180;
    for (int i = 0; i < 60; i++) {
        const double mid = 0.5 * (lo + hi);
        (thetaD(mid) < edge ? lo : hi) = mid;
    }
    out.theta_max = lo;

    // Least squares of theta_d on theta, theta^3 .. theta^9: normal equations.
    constexpr int kN = 1001, kP = 5;
    double A[kP][kP + 1] = {};
    for (int s = 0; s < kN; s++) {
        const double th = out.theta_max * s / (kN - 1), t2 = th * th;
        double b[kP];
        b[0] = th;
        for (int j = 1; j < kP; j++) b[j] = b[j - 1] * t2;
        const double y = thetaD(th);
        for (int i = 0; i < kP; i++) {
            for (int j = 0; j < kP; j++) A[i][j] += b[i] * b[j];
            A[i][kP] += b[i] * y;
        }
    }
    for (int c = 0; c < kP; c++) {
        int piv = c;
        for (int r = c + 1; r < kP; r++)
            if (std::fabs(A[r][c]) > std::fabs(A[piv][c])) piv = r;
        for (int j = 0; j <= kP; j++) std::swap(A[c][j], A[piv][j]);
        if (A[c][c] == 0) return false;
        for (int r = 0; r < kP; r++) {
            if (r == c) continue;
            const double f = A[r][c] / A[c][c];
            for (int j = c; j <= kP; j++) A[r][j] -= f * A[c][j];
        }
    }
    double a[kP];
    for (int i = 0; i < kP; i++) a[i] = A[i][kP] / A[i][i];
    if (!(a[0] > 0)) return false;
    for (int s = 0; s < kN; s++) {
        const double th = out.theta_max * s / (kN - 1), t2 = th * th;
        const double fit = th * (a[0] + t2 * (a[1] + t2 * (a[2] + t2 * (a[3] + t2 * a[4]))));
        out.max_px = std::max(out.max_px, l.fx * std::fabs(fit - thetaD(th)));
    }

    // The focal takes the fit's scale; p acts on unscaled equidistant coordinates.
    const double s = a[0], fx = l.fx * s, fy = l.fy * s;
    const double k1 = a[1] / s, k2 = a[2] / s, k3 = a[3] / s, k4 = a[4] / s;
    if (model == CamModel::ThinPrismFisheye)
        out.params = {fx, fy, l.cx, l.cy, k1, k2, l.p1 / s, l.p2 / s, k3, k4, 0, 0};
    else
        out.params = {fx, fy, l.cx, l.cy, k1, k2, k3, k4};
    return true;
}

enum class LensUse { Used, Override, DatasetWide, Model, Size, NoImages };

struct LensPlan {
    std::string prefix;          // the lens's folder under the image directory
    std::string source;          // the file the calibration came from
    LensCalibration lens;
    int width = 0, height = 0;   // of the folder's first frame; 0 = none read
    LensUse use = LensUse::Used;
    CamModel model = CamModel::OpenCV;
    LensFit fit;
};

// Decides each plan and appends an override for each used one. Any matching
// override that sets a focal, extra or params wins whole, whichever its prefix:
// a factory override for the folder would outrank it by prefix length.
inline void applyLensPlans(CameraSetupOptions& cam, std::vector<LensPlan>& plans) {
    for (LensPlan& p : plans) {
        bool given = false;
        CameraOverride* exact = nullptr;
        const CameraOverride* model_from = nullptr;
        for (CameraOverride& o : cam.overrides) {
            if (!detail::cameraPrefixMatches(p.prefix + "/", o.prefix)) continue;
            if (o.has_focal || o.has_extra || !o.params.empty()) given = true;
            if (o.has_model && (!model_from || o.prefix.size() > model_from->prefix.size())) model_from = &o;
            if (o.prefix == p.prefix && !exact) exact = &o;
        }
        p.model = model_from ? model_from->model : cam.model;
        if (given)
            p.use = LensUse::Override;
        else if (cam.focal > 0 || !cam.extra.empty() || !cam.params.empty())
            p.use = LensUse::DatasetWide;
        else if (p.width <= 0 || p.height <= 0)
            p.use = LensUse::NoImages;
        else if (p.width != p.lens.width || p.height != p.lens.height)
            p.use = LensUse::Size;
        else if (!lensParams(p.lens, p.model, p.fit))
            p.use = LensUse::Model;
        else
            p.use = LensUse::Used;
        if (p.use != LensUse::Used) continue;
        if (exact) {
            exact->has_model = true;
            exact->model = p.model;
            exact->params = p.fit.params;
            continue;
        }
        CameraOverride o;
        o.prefix = p.prefix;
        o.has_model = true;
        o.model = p.model;
        o.params = p.fit.params;
        cam.overrides.push_back(o);
    }
}

// The plans for every telemetry input's lenses: capture prefix + camN, sized by
// the folder's first readable frame.
using LensReader = std::vector<LensCalibration> (*)(const std::string&);
inline std::vector<LensPlan> collectLensPlans(const std::vector<TelemetryInput>& inputs,
                                              const std::string& image_dir,
                                              LensReader read = video_lenses) {
    namespace fs = std::filesystem;
    std::vector<LensPlan> out;
    for (const TelemetryInput& in : inputs) {
        for (const LensCalibration& l : read(in.path)) {
            LensPlan p;
            const std::string cam = "cam" + std::to_string(l.track);
            p.prefix = in.prefix.empty() ? cam : in.prefix + "/" + cam;
            p.source = in.path;
            p.lens = l;
            std::error_code ec;
            std::vector<fs::path> files;
            for (fs::directory_iterator it(fs::path(image_dir) / p.prefix, ec), end; !ec && it != end;
                 it.increment(ec))
                if (it->is_regular_file(ec)) files.push_back(it->path());
            std::sort(files.begin(), files.end());
            for (const fs::path& f : files) {
                if (imageSize(f.string(), p.width, p.height) && p.width > 0 && p.height > 0) break;
                p.width = p.height = 0;
            }
            out.push_back(p);
        }
    }
    return out;
}

}  // namespace sfm
