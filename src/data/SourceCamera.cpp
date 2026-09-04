#include "data/SourceCamera.h"

#include <cmath>

// Transcribed from colmap/src/colmap/sensor/models.h (FOVCameraModel,
// SimpleDivisionCameraModel, DivisionCameraModel, EUCMCameraModel,
// RadTanThinPrismFisheyeModel::Distortion) and, for kSkewed, from
// shaders/camera_source.slang. Only the forward direction is needed: the
// fitter samples it and the re-distort kernel re-evaluates it.

namespace srccam {

namespace {

bool project_fov(const float* prm, double X, double Y, double Z,
                 double* u, double* v) {
    if (Z < 1e-12) return false;
    double x = X / Z, y = Y / Z, omega = prm[4];
    double r2 = x*x + y*y, om2 = omega * omega;
    constexpr double kEps = 1e-4;
    double factor;
    if (om2 < kEps) {
        factor = (om2 * r2) / 3.0 - om2 / 12.0 + 1.0;
    } else if (r2 < kEps) {
        double t = std::tan(omega / 2.0);
        factor = (-2.0 * t * (4.0 * r2 * t * t - 3.0)) / (3.0 * omega);
    } else {
        double r = std::sqrt(r2);
        factor = std::atan(r * 2.0 * std::tan(omega / 2.0)) / (r * omega);
    }
    *u = prm[0] * x * factor + prm[2];
    *v = prm[1] * y * factor + prm[3];
    return true;
}

bool project_division(const float* prm, bool simple, double X, double Y, double Z,
                      double* u, double* v) {
    double fx = prm[0], fy = simple ? prm[0] : prm[1];
    double cx = simple ? prm[1] : prm[2], cy = simple ? prm[2] : prm[3];
    double k  = simple ? prm[3] : prm[4];
    double rho2 = X*X + Y*Y;
    double disc = Z*Z - 4.0 * rho2 * k;
    if (disc < 0.0) return false;
    double r = 2.0 / (Z + std::sqrt(disc));
    *u = fx * r * X + cx;
    *v = fy * r * Y + cy;
    return true;
}

bool project_eucm(const float* prm, double X, double Y, double Z,
                  double* u, double* v) {
    if (Z < 1e-12) return false;
    double alpha = prm[4], beta = prm[5];
    double rho2 = beta * (X*X + Y*Y) + Z*Z;
    if (rho2 < 0.0) return false;
    double den = alpha * std::sqrt(rho2) + (1.0 - alpha) * Z;
    if (den < 1e-12) return false;
    *u = prm[0] * X / den + prm[2];
    *v = prm[1] * Y / den + prm[3];
    return true;
}

// Aria Fisheye624. Unlike THIN_PRISM_FISHEYE, the tangential and thin-prism
// terms act on the ALREADY radially distorted point.
bool project_rad_tan_thin_prism(const float* prm, double X, double Y, double Z,
                                double* u, double* v) {
    if (Z < 1e-12) return false;
    double xn = X / Z, yn = Y / Z;
    double r = std::sqrt(xn*xn + yn*yn);
    double uu = xn, vv = yn;
    if (r > 1e-12) {
        double th = std::atan(r);
        uu *= th / r; vv *= th / r;
    }
    double th2 = uu*uu + vv*vv, pw = 1.0, rad = 1.0;
    for (int i = 0; i < 6; i++) { pw *= th2; rad += prm[4 + i] * pw; }
    double x = rad * uu, y = rad * vv;
    double p0 = prm[10], p1 = prm[11];
    double s0 = prm[12], s1 = prm[13], s2 = prm[14], s3 = prm[15];
    double x2 = x*x, y2 = y*y, xy = x*y, r2 = x2 + y2, r4 = r2*r2;
    double xd = x + 2.0*p1*xy + p0*(r2 + 2.0*x2) + s0*r2 + s1*r4;
    double yd = y + 2.0*p0*xy + p1*(r2 + 2.0*y2) + s2*r2 + s3*r4;
    *u = prm[0] * xd + prm[2];
    *v = prm[1] * yd + prm[3];
    return true;
}

bool project_skewed(const float* p, double X, double Y, double Z,
                    double* u, double* v) {
    double qx, qy;
    const int base = (int)p[13];
    if (base == (int)kSkewBasePerspective) {
        if (Z < 1e-12) return false;
        qx = X / Z; qy = Y / Z;
    } else {
        double r = std::sqrt(X*X + Y*Y);
        double theta = std::atan2(r, Z), k;
        if (base == (int)kSkewBaseFisheye)
            k = theta < 1e-3 ? (1.0 - theta*theta/3.0) / Z : theta / r;
        else
            k = r < 1e-6 ? (1.0 - theta*theta/24.0) / Z
                         : (2.0 * std::sin(0.5 * theta)) / r;
        qx = X * k; qy = Y * k;
    }
    double r2 = qx*qx + qy*qy, dx, dy;
    if (p[14] != 0.0f) {
        double radial = (1.0 + r2*(p[5] + r2*(p[6] + r2*p[7])))
                      / (1.0 + r2*(p[8] + r2*(p[9] + r2*p[10])));
        dx = qx * radial + 2.0*p[11]*qx*qy + p[12]*(r2 + 2.0*qx*qx);
        dy = qy * radial + 2.0*p[12]*qx*qy + p[11]*(r2 + 2.0*qy*qy);
    } else {
        double radial = 1.0 + r2*(p[5] + r2*(p[6] + r2*(p[7] + r2*p[8])));
        dx = qx * radial + 2.0*p[9]*qx*qy + p[10]*(r2 + 2.0*qx*qx) + p[11]*r2;
        dy = qy * radial + 2.0*p[10]*qx*qy + p[9]*(r2 + 2.0*qy*qy) + p[12]*r2;
    }
    *u = p[0] * dx + p[4] * dy + p[2];
    *v = p[1] * dy + p[3];
    return true;
}

// The model's own formula, with no check that it still describes a lens.
bool project_raw(int model_id, const float* params,
                 double X, double Y, double Z, double* u, double* v) {
    switch (model_id) {
        case kSkewed:
            return project_skewed(params, X, Y, Z, u, v);
        case kColmapFOV:
            return project_fov(params, X, Y, Z, u, v);
        case kColmapSimpleDivision:
            return project_division(params, true, X, Y, Z, u, v);
        case kColmapDivision:
            return project_division(params, false, X, Y, Z, u, v);
        case kColmapEUCM:
            return project_eucm(params, X, Y, Z, u, v);
        case kColmapRadTanThinPrismFisheye:
            return project_rad_tan_thin_prism(params, X, Y, Z, u, v);
        default:
            return false;
    }
}

// source_unfolded mirror; camera_source.slang has the why.
bool unfolded(int model_id, const float* params, double X, double Y, double Z,
              double u, double v) {
    double l = std::sqrt(X*X + Y*Y + Z*Z);
    if (!(l > 0.0)) return false;
    double n[3] = {X / l, Y / l, Z / l};
    double t[3] = {1.0, 0.0, 0.0};
    if (!(std::abs(n[0]) < 0.9)) { t[0] = 0.0; t[1] = 1.0; }
    double e1[3] = {t[1]*n[2] - t[2]*n[1], t[2]*n[0] - t[0]*n[2],
                    t[0]*n[1] - t[1]*n[0]};
    double e1l = std::sqrt(e1[0]*e1[0] + e1[1]*e1[1] + e1[2]*e1[2]);
    for (double& c : e1) c /= e1l;
    double e2[3] = {n[1]*e1[2] - n[2]*e1[1], n[2]*e1[0] - n[0]*e1[2],
                    n[0]*e1[1] - n[1]*e1[0]};
    constexpr double kStep = 1e-3;  // radians
    double au, av, bu, bv;
    if (!project_raw(model_id, params, n[0] + kStep*e1[0], n[1] + kStep*e1[1],
                     n[2] + kStep*e1[2], &au, &av)) return false;
    if (!project_raw(model_id, params, n[0] + kStep*e2[0], n[1] + kStep*e2[1],
                     n[2] + kStep*e2[2], &bu, &bv)) return false;
    return (au - u) * (bv - v) - (av - v) * (bu - u) > 0.0;
}

}  // namespace

// Everything not touched here is on the normalized plane and scale-free: FOV's
// omega, the division models' k, EUCM's alpha/beta, the radial and tangential
// coefficients.
void rescale(int model_id, float* params, double sx, double sy) {
    if (!(sx > 0.0) || !(sy > 0.0) || (sx == 1.0 && sy == 1.0)) return;
    if (model_id == kColmapSimpleDivision) {
        // One focal length for both axes, so a non-square resize cannot be
        // represented; sx keeps the horizontal field right. cx/cy shift down a slot.
        params[0] = (float)(params[0] * sx);
        params[1] = (float)(params[1] * sx);
        params[2] = (float)(params[2] * sy);
        return;
    }
    params[0] = (float)(params[0] * sx);   // fx
    params[1] = (float)(params[1] * sy);   // fy
    params[2] = (float)(params[2] * sx);   // cx
    params[3] = (float)(params[3] * sy);   // cy
    if (model_id == kSkewed)
        params[4] = (float)(params[4] * sx);   // x-pixels per unit normalized y
}

bool project(int model_id, const float* params,
             double X, double Y, double Z, double* u, double* v) {
    return project_raw(model_id, params, X, Y, Z, u, v) &&
           unfolded(model_id, params, X, Y, Z, *u, *v);
}

}  // namespace srccam
