// Fundamental matrix estimation (src/sfm/README.md).
//
//   estimateFundamental7  minimal 7-point solver (up to 3 real solutions),
//                         RANSAC's minimal sampler for F.
//   estimateFundamental8  normalized 8-point (Hartley), the >= 8 point solver
//                         used for local optimization / refit.
//   sampsonSq             first-order geometric (Sampson) reprojection error.
//
// Both solvers Hartley-normalize the sampled points first. Coordinates are in
// pixels; residuals come back in squared pixels.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "sfm/geometry/LinAlg.h"

namespace sfm {

// Hartley normalization: centroid to origin, mean distance sqrt(2). Returns T
// and the transformed points (indexed by `idx`).
inline Mat3 hartleyNormalize(const std::vector<Vec2>& pts, const std::vector<int>& idx,
                             std::vector<Vec2>& out) {
    double cx = 0, cy = 0;
    for (int i : idx) { cx += pts[i].x; cy += pts[i].y; }
    cx /= idx.size();
    cy /= idx.size();
    double meanDist = 0;
    for (int i : idx) meanDist += std::hypot(pts[i].x - cx, pts[i].y - cy);
    meanDist /= idx.size();
    double scale = meanDist > 1e-12 ? std::sqrt(2.0) / meanDist : 1.0;
    Mat3 T = {scale, 0, -scale * cx, 0, scale, -scale * cy, 0, 0, 1};
    out.resize(idx.size());
    for (size_t k = 0; k < idx.size(); k++)
        out[k] = {scale * (pts[idx[k]].x - cx), scale * (pts[idx[k]].y - cy)};
    return T;
}

// Build the n x 9 epipolar constraint matrix rows x2^T F x1 = 0.
inline std::vector<double> epipolarMatrix(const std::vector<Vec2>& a, const std::vector<Vec2>& b) {
    size_t n = a.size();
    std::vector<double> A(n * 9);
    for (size_t i = 0; i < n; i++) {
        double x1 = a[i].x, y1 = a[i].y, x2 = b[i].x, y2 = b[i].y;
        double* r = &A[i * 9];
        r[0] = x2 * x1; r[1] = x2 * y1; r[2] = x2;
        r[3] = y2 * x1; r[4] = y2 * y1; r[5] = y2;
        r[6] = x1;      r[7] = y1;      r[8] = 1;
    }
    return A;
}

inline Mat3 enforceRank2(const Mat3& F) {
    Svd3 s = svd3(F);
    Vec3 d = {s.s.x, s.s.y, 0.0};
    Mat3 D = {d.x, 0, 0, 0, d.y, 0, 0, 0, d.z};
    return mul(mul(s.U, D), transpose(s.V));
}

inline std::vector<Mat3> estimateFundamental8(const std::vector<Vec2>& p1,
                                              const std::vector<Vec2>& p2,
                                              const std::vector<int>& idx) {
    if (idx.size() < 8) return {};
    std::vector<Vec2> a, b;
    Mat3 T1 = hartleyNormalize(p1, idx, a);
    Mat3 T2 = hartleyNormalize(p2, idx, b);
    std::vector<double> A = epipolarMatrix(a, b);
    auto nv = nullVectors9(A, (int)idx.size(), 1);
    if (nv.empty()) return {};
    Mat3 Fn;
    for (int i = 0; i < 9; i++) Fn[i] = nv[0][i];
    Fn = enforceRank2(Fn);
    // denormalize: F = T2^T Fn T1
    Mat3 F = mul(mul(transpose(T2), Fn), T1);
    return {F};
}

// Real roots of a cubic c3 x^3 + c2 x^2 + c1 x + c0 (returns 1..3 roots).
inline std::vector<double> solveCubicReal(double c3, double c2, double c1, double c0) {
    std::vector<double> roots;
    if (std::fabs(c3) < 1e-14) {  // quadratic
        double a = c2, b = c1, c = c0;
        if (std::fabs(a) < 1e-14) {
            if (std::fabs(b) > 1e-14) roots.push_back(-c / b);
            return roots;
        }
        double disc = b * b - 4 * a * c;
        if (disc >= 0) {
            double sq = std::sqrt(disc);
            roots.push_back((-b + sq) / (2 * a));
            roots.push_back((-b - sq) / (2 * a));
        }
        return roots;
    }
    // normalize to x^3 + a x^2 + b x + c
    double a = c2 / c3, b = c1 / c3, c = c0 / c3;
    double q = (a * a - 3 * b) / 9.0;
    double r = (2 * a * a * a - 9 * a * b + 27 * c) / 54.0;
    double q3 = q * q * q;
    if (r * r < q3) {
        double theta = std::acos(std::max(-1.0, std::min(1.0, r / std::sqrt(q3))));
        double m = -2 * std::sqrt(q);
        roots.push_back(m * std::cos(theta / 3) - a / 3);
        roots.push_back(m * std::cos((theta + 2 * M_PI) / 3) - a / 3);
        roots.push_back(m * std::cos((theta - 2 * M_PI) / 3) - a / 3);
    } else {
        double e = std::cbrt(std::fabs(r) + std::sqrt(r * r - q3));
        if (r > 0) e = -e;
        double f = (std::fabs(e) < 1e-300) ? 0.0 : q / e;
        roots.push_back((e + f) - a / 3);
    }
    return roots;
}

inline std::vector<Mat3> estimateFundamental7(const std::vector<Vec2>& p1,
                                              const std::vector<Vec2>& p2,
                                              const std::vector<int>& idx) {
    if (idx.size() != 7) return {};
    std::vector<Vec2> a, b;
    Mat3 T1 = hartleyNormalize(p1, idx, a);
    Mat3 T2 = hartleyNormalize(p2, idx, b);
    std::vector<double> A = epipolarMatrix(a, b);
    auto nv = nullVectors9(A, 7, 2);
    if (nv.size() < 2) return {};
    Mat3 F1, F2;
    for (int i = 0; i < 9; i++) { F1[i] = nv[0][i]; F2[i] = nv[1][i]; }

    // det(alpha F1 + (1-alpha) F2) = 0, cubic in alpha. Sample det at 4 alphas
    // and fit the cubic (robust and simple).
    auto detAt = [&](double al) {
        Mat3 F;
        for (int i = 0; i < 9; i++) F[i] = al * F1[i] + (1 - al) * F2[i];
        return det3(F);
    };
    double d0 = detAt(0.0), d1 = detAt(1.0), d2 = detAt(2.0), dm1 = detAt(-1.0);
    // fit p(a)=c3 a^3 + c2 a^2 + c1 a + c0 through (0,d0),(1,d1),(2,d2),(-1,dm1)
    double c0 = d0;
    double c3 = (d2 - 3 * d1 + 3 * d0 - dm1) / 6.0;
    double c2 = (d1 + dm1 - 2 * d0) / 2.0;
    double c1 = d1 - d0 - c2 - c3;
    std::vector<Mat3> out;
    for (double al : solveCubicReal(c3, c2, c1, c0)) {
        Mat3 Fn;
        for (int i = 0; i < 9; i++) Fn[i] = al * F1[i] + (1 - al) * F2[i];
        Mat3 F = mul(mul(transpose(T2), Fn), T1);
        out.push_back(F);
    }
    return out;
}

// ---- the same algebra on unit bearings (D45) ----------------------------
//
// The epipolar constraint b2^T E b1 = 0 holds for *viewing rays*, and only
// coincides with x2^T F x1 = 0 on pixels when the lens is a pinhole. A fisheye
// keypoint 100 deg off axis has no pinhole pixel at all, so a pixel-space F
// cannot explain it and verification silently throws those correspondences
// away (docs/notes/sfm-design.md D45). Given a camera model we can hand the same
// linear algebra unit bearings instead and get geometry that is valid over the
// whole field of view.
//
// What is estimated is a general rank-2 3x3, not a metric essential matrix:
// exactly the uncalibrated F story, one coordinate system down. That costs two
// spurious degrees of freedom and buys tolerance to an imperfect focal prior,
// which at verification time we certainly have. `projectToEssential` imposes
// the missing constraint later, where the prior has been refined.

inline std::vector<double> epipolarMatrix3(const std::vector<Vec3>& a, const std::vector<Vec3>& b) {
    size_t n = a.size();
    std::vector<double> A(n * 9);
    for (size_t i = 0; i < n; i++) {
        const Vec3& p = a[i];
        const Vec3& q = b[i];
        double* r = &A[i * 9];
        r[0] = q.x * p.x; r[1] = q.x * p.y; r[2] = q.x * p.z;
        r[3] = q.y * p.x; r[4] = q.y * p.y; r[5] = q.y * p.z;
        r[6] = q.z * p.x; r[7] = q.z * p.y; r[8] = q.z * p.z;
    }
    return A;
}

// Unit bearings need no Hartley normalization: they already have norm 1 and
// span the sphere, so the constraint matrix is well conditioned by
// construction. (Normalizing them like image points would move them off the
// sphere and break the geometry.)
inline std::vector<Mat3> estimateEpipolar8Bearing(const std::vector<Vec3>& b1,
                                                  const std::vector<Vec3>& b2,
                                                  const std::vector<int>& idx) {
    if (idx.size() < 8) return {};
    std::vector<Vec3> a(idx.size()), b(idx.size());
    for (size_t k = 0; k < idx.size(); k++) { a[k] = b1[idx[k]]; b[k] = b2[idx[k]]; }
    std::vector<double> A = epipolarMatrix3(a, b);
    auto nv = nullVectors9(A, (int)idx.size(), 1);
    if (nv.empty()) return {};
    Mat3 E;
    for (int i = 0; i < 9; i++) E[i] = nv[0][i];
    return {enforceRank2(E)};
}

inline std::vector<Mat3> estimateEpipolar7Bearing(const std::vector<Vec3>& b1,
                                                  const std::vector<Vec3>& b2,
                                                  const std::vector<int>& idx) {
    if (idx.size() != 7) return {};
    std::vector<Vec3> a(7), b(7);
    for (int k = 0; k < 7; k++) { a[k] = b1[idx[k]]; b[k] = b2[idx[k]]; }
    std::vector<double> A = epipolarMatrix3(a, b);
    auto nv = nullVectors9(A, 7, 2);
    if (nv.size() < 2) return {};
    Mat3 E1, E2;
    for (int i = 0; i < 9; i++) { E1[i] = nv[0][i]; E2[i] = nv[1][i]; }
    auto detAt = [&](double al) {
        Mat3 E;
        for (int i = 0; i < 9; i++) E[i] = al * E1[i] + (1 - al) * E2[i];
        return det3(E);
    };
    double d0 = detAt(0.0), d1 = detAt(1.0), d2 = detAt(2.0), dm1 = detAt(-1.0);
    double c0 = d0;
    double c3 = (d2 - 3 * d1 + 3 * d0 - dm1) / 6.0;
    double c2 = (d1 + dm1 - 2 * d0) / 2.0;
    double c1 = d1 - d0 - c2 - c3;
    std::vector<Mat3> out;
    for (double al : solveCubicReal(c3, c2, c1, c0)) {
        Mat3 E;
        for (int i = 0; i < 9; i++) E[i] = al * E1[i] + (1 - al) * E2[i];
        out.push_back(E);
    }
    return out;
}

// Nearest essential matrix: singular values (1,1,0). Valid to apply once the
// bearings are trusted -- it is the constraint the 7-point fit above omits.
inline Mat3 projectToEssential(const Mat3& E) {
    Svd3 s = svd3(E);
    double m = 0.5 * (s.s.x + s.s.y);
    Mat3 D = {m, 0, 0, 0, m, 0, 0, 0, 0};
    return mul(mul(s.U, D), transpose(s.V));
}

// Sampson error on the sphere, in squared radians. Same first-order geometric
// approximation as sampsonSq, but the perturbation lives in each bearing's
// tangent plane (2 DOF on the sphere) rather than on the z=1 image plane --
// which is what makes it meaningful for a ray pointing sideways.
inline double sampsonSqBearing(const Mat3& E, const Mat3& Et, const Vec3& b1, const Vec3& b2) {
    Vec3 Eb1 = mul(E, b1);
    Vec3 Etb2 = mul(Et, b2);
    double num = b2.dot(Eb1);
    // project each gradient onto the tangent plane of its own bearing
    Vec3 g1 = Etb2 - b1 * b1.dot(Etb2);
    Vec3 g2 = Eb1 - b2 * b2.dot(Eb1);
    double den = g1.dot(g1) + g2.dot(g2);
    if (den < 1e-30) return 1e30;
    return num * num / den;
}

inline double sampsonSqBearing(const Mat3& E, const Vec3& b1, const Vec3& b2) {
    return sampsonSqBearing(E, transpose(E), b1, b2);
}

// Squared Sampson distance for correspondence a (image 1) <-> b (image 2).
inline double sampsonSq(const Mat3& F, const Mat3& Ft, const Vec2& a, const Vec2& b) {
    Vec3 x1 = {a.x, a.y, 1}, x2 = {b.x, b.y, 1};
    Vec3 Fx1 = mul(F, x1);
    Vec3 Ftx2 = mul(Ft, x2);
    double num = x2.dot(Fx1);
    double den = Fx1.x * Fx1.x + Fx1.y * Fx1.y + Ftx2.x * Ftx2.x + Ftx2.y * Ftx2.y;
    if (den < 1e-30) return 1e30;
    return num * num / den;
}

inline double sampsonSq(const Mat3& F, const Vec2& a, const Vec2& b) {
    return sampsonSq(F, transpose(F), a, b);
}

}  // namespace sfm
