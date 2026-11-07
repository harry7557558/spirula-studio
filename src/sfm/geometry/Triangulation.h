// Triangulation and triangulation-angle checks (src/sfm/README.md).
//
// DLT triangulation from two 3x4 projection matrices, plus the parallax angle
// between the two viewing rays (COLMAP's min_tri_angle filter, default 1.5 deg).
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "sfm/core/Experiment.h"
#include "sfm/geometry/LinAlg.h"

namespace sfm {

using Mat34 = std::array<double, 12>;  // 3x4, row-major

// Fill the two DLT rows contributed by one unit bearing `b` and its 3x4
// projection matrix `P`. For a forward ray (b.z above the threshold) this is the
// classic perspective pair x*P3-P1, y*P3-P2 (x=b.x/b.z) -- bit-identical to the
// pre-bearing pinhole code, and the only path any rectilinear lens ever takes
// (its rays never reach ~84 deg). A wide/fisheye ray (small or negative b.z)
// cannot be perspective-divided, so it uses the general cross-product form
// b x (P X) = 0, whose two independent rows are well-conditioned for any FOV.
inline void dltRows(double* r0, double* r1, const Vec3& b, const Mat34& P) {
    if (std::fabs(b.z) > 0.1) {
        double x = b.x / b.z, y = b.y / b.z;
        for (int c = 0; c < 4; c++) {
            r0[c] = x * P[8 + c] - P[0 + c];
            r1[c] = y * P[8 + c] - P[4 + c];
        }
    } else {
        for (int c = 0; c < 4; c++) {
            r0[c] = b.y * P[8 + c] - b.z * P[4 + c];   // (b x PX) component 0
            r1[c] = b.z * P[0 + c] - b.x * P[8 + c];   // (b x PX) component 1
        }
    }
}

// Homogeneous DLT triangulation from two unit bearings (viewing rays in each
// camera frame) and their 3x4 projection matrices. Returns the inhomogeneous
// 3D point. FOV-agnostic: forward rays stay bit-identical to the pre-bearing
// code, wide/fisheye rays use the ray-native form (see dltRows).
inline Vec3 triangulateDLT(const Mat34& P1, const Mat34& P2, const Vec3& b1, const Vec3& b2) {
    double A[4][4];
    dltRows(A[0], A[1], b1, P1);
    dltRows(A[2], A[3], b2, P2);
    // On the stack: retriangulation calls this millions of times a pass.
    double AtA[16], w[4], V[16];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            double s = 0;
            for (int r = 0; r < 4; r++) s += A[r][i] * A[r][j];
            AtA[i * 4 + j] = s;
        }
    jacobiEigenSymmetric(AtA, 4, w, V);
    int mi = 0;
    for (int i = 1; i < 4; i++)
        if (w[i] < w[mi]) mi = i;
    double h[4];
    for (int r = 0; r < 4; r++) h[r] = V[r * 4 + mi];
    if (std::fabs(h[3]) < 1e-30) return {0, 0, 0};
    return {h[0] / h[3], h[1] / h[3], h[2] / h[3]};
}

struct PointObservation {
    Mat34 projection;
    Vec3 bearing;
    double angular_error;
};

inline double pointAngularCost(const Vec3& X, const std::vector<PointObservation>& obs,
                               Mat3* normal = nullptr, Vec3* gradient = nullptr) {
    if (normal) *normal = {};
    if (gradient) *gradient = {};
    double cost = 0;
    for (const auto& o : obs) {
        const auto& P = o.projection;
        const Vec3 p{P[0]*X.x + P[1]*X.y + P[2]*X.z + P[3],
                     P[4]*X.x + P[5]*X.y + P[6]*X.z + P[7],
                     P[8]*X.x + P[9]*X.y + P[10]*X.z + P[11]};
        const double depth = p.norm();
        if (!(depth > 1e-15) || p.dot(o.bearing) <= 0) {
            cost += 1e6;
            continue;
        }
        const Vec3 ray = p * (1.0 / depth), residual = ray - o.bearing;
        const double scale = std::max(1e-12, o.angular_error);
        const double r = residual.norm() / scale;
        cost += r <= 1 ? 0.5 * r * r : r - 0.5;
        if (!normal || !gradient) continue;
        const double weight = (r > 1 ? 1.0 / r : 1.0) / (scale * scale);
        double J[9];
        const double u[] = {ray.x, ray.y, ray.z};
        const double e[] = {residual.x, residual.y, residual.z};
        for (int col = 0; col < 3; ++col) {
            double dot = 0;
            for (int row = 0; row < 3; ++row) dot += u[row] * P[4 * row + col];
            for (int row = 0; row < 3; ++row)
                J[3 * row + col] = (P[4 * row + col] - u[row] * dot) / depth;
        }
        double g[3] = {};
        for (int i = 0; i < 3; ++i) {
            for (int row = 0; row < 3; ++row) g[i] += weight * J[3 * row + i] * e[row];
            for (int j = 0; j < 3; ++j)
                for (int row = 0; row < 3; ++row)
                    (*normal)[3 * i + j] += weight * J[3 * row + i] * J[3 * row + j];
        }
        *gradient = *gradient + Vec3{g[0], g[1], g[2]};
    }
    return cost;
}

// Unit-ray residuals keep refinement valid for fisheye observations behind the image plane.
inline Vec3 refineTriangulatedPoint(Vec3 X, const std::vector<PointObservation>& obs,
                                   int iterations = 10) {
    if (obs.size() < 2) return X;
    double damping = 1e-4;
    for (int iteration = 0; iteration < iterations; ++iteration) {
        Mat3 normal;
        Vec3 gradient;
        const double cost = pointAngularCost(X, obs, &normal, &gradient);
        for (int i = 0; i < 3; ++i)
            normal[4 * i] += damping * std::max(1e-12, normal[4 * i]);
        bool invertible;
        const Mat3 inverse = inverse3(normal, &invertible);
        if (!invertible) break;
        const Vec3 step = mul(inverse, gradient);
        const Vec3 candidate = X - step;
        const double next = pointAngularCost(candidate, obs);
        if (std::isfinite(next) && next < cost) {
            X = candidate;
            damping = std::max(1e-10, damping * 0.25);
            if (cost - next <= 1e-10 * std::max(1.0, cost)) break;
        } else {
            damping *= 10;
        }
    }
    return X;
}

// Opposed rays also leave depth unobservable; the acute policy folds them onto zero.
inline double triangulationAngle(const Vec3& X, const Vec3& c1, const Vec3& c2) {
    static const bool acute = experimentInt("SFM_ACUTE_PARALLAX", 0, 1) != 0;
    Vec3 r1 = (X - c1).normalized();
    Vec3 r2 = (X - c2).normalized();
    double c = std::max(-1.0, std::min(1.0, r1.dot(r2)));
    return std::acos(acute ? std::fabs(c) : c);
}

}  // namespace sfm
