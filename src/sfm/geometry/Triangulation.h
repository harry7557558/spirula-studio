// Triangulation and triangulation-angle checks (src/sfm/README.md).
//
// DLT triangulation from two 3x4 projection matrices, plus the parallax angle
// between the two viewing rays (COLMAP's min_tri_angle filter, default 1.5 deg).
#pragma once

#include <array>
#include <cmath>
#include <vector>

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

// Angle (radians) at X between rays to camera centers c1 and c2.
inline double triangulationAngle(const Vec3& X, const Vec3& c1, const Vec3& c2) {
    Vec3 r1 = (X - c1).normalized();
    Vec3 r2 = (X - c2).normalized();
    double c = std::max(-1.0, std::min(1.0, r1.dot(r2)));
    return std::acos(c);
}

}  // namespace sfm
