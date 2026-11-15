// Essential-matrix decomposition and bearing-based cheirality selection.
#pragma once

#include <cmath>
#include <vector>

#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/Triangulation.h"

namespace sfm {

struct Pose {
    Mat3 R = mat3Identity();  // world -> camera rotation
    Vec3 t;                   // world -> camera translation (unit-scale for relative pose)
};

inline Mat3 essentialFromFundamental(const Mat3& F, const Mat3& K1, const Mat3& K2) {
    return mul(mul(transpose(K2), F), K1);
}

// Force an approximately-orthonormal matrix to a proper rotation (det +1).
inline Mat3 nearestRotation(const Mat3& R) {
    Svd3 s = svd3(R);
    Mat3 out = mul(s.U, transpose(s.V));
    if (det3(out) < 0)
        for (int c = 0; c < 3; c++) { out[c] = -out[c]; out[3 + c] = -out[3 + c]; out[6 + c] = -out[6 + c]; }
    return out;
}

// Four (R,t) candidates from an essential matrix.
inline std::vector<Pose> decomposeEssential(const Mat3& E) {
    Svd3 s = svd3(E);
    Mat3 U = s.U, V = s.V;
    if (det3(U) < 0) for (int c = 0; c < 3; c++) { U[c] = -U[c]; U[3 + c] = -U[3 + c]; U[6 + c] = -U[6 + c]; }
    if (det3(V) < 0) for (int c = 0; c < 3; c++) { V[c] = -V[c]; V[3 + c] = -V[3 + c]; V[6 + c] = -V[6 + c]; }
    Mat3 W = {0, -1, 0, 1, 0, 0, 0, 0, 1};
    Mat3 R1 = mul(mul(U, W), transpose(V));
    Mat3 R2 = mul(mul(U, transpose(W)), transpose(V));
    Vec3 t = {U[2], U[5], U[8]};  // third column of U
    return {{R1, t}, {R1, {-t.x, -t.y, -t.z}}, {R2, t}, {R2, {-t.x, -t.y, -t.z}}};
}

inline Mat34 poseToP(const Pose& p) {
    return {p.R[0], p.R[1], p.R[2], p.t.x, p.R[3], p.R[4], p.R[5], p.t.y,
            p.R[6], p.R[7], p.R[8], p.t.z};
}

// Choose the pose with the most correspondences in front of both cameras.
// `n1`/`n2` are normalized image coordinates (K^-1 applied). Returns the count
// of cheirality-valid points and, in `mask`, which correspondences were valid.
inline Pose recoverRelativePose(const std::vector<Vec2>& n1, const std::vector<Vec2>& n2,
                                const std::vector<int>& idx, const Mat3& E, int& bestCount,
                                std::vector<char>* mask = nullptr) {
    Mat34 P1 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    Pose best;
    bestCount = -1;
    for (const Pose& cand : decomposeEssential(E)) {
        Mat34 P2 = poseToP(cand);
        int count = 0;
        std::vector<char> m(idx.size(), 0);
        for (size_t k = 0; k < idx.size(); k++) {
            int i = idx[k];
            // n1/n2 are z=1 normalized coords; lift to unit bearings for the
            // (now bearing-native) triangulator. Bit-identical for these forward
            // rays. Fisheye two-view init is D29 phase D.
            Vec3 b1 = Vec3{n1[i].x, n1[i].y, 1.0}.normalized();
            Vec3 b2 = Vec3{n2[i].x, n2[i].y, 1.0}.normalized();
            Vec3 X = triangulateDLT(P1, P2, b1, b2);
            double z1 = X.z;
            Vec3 Xc2 = mul(cand.R, X) + cand.t;
            if (z1 > 0 && Xc2.z > 0) { count++; m[k] = 1; }
        }
        if (count > bestCount) { bestCount = count; best = cand; if (mask) *mask = m; }
    }
    return best;
}

// Bearing-native relative pose (D45). Same four-way cheirality vote as above,
// but the observations are unit rays, so it stays correct past 90 deg where
// z=1 normalized coordinates do not exist. A point is in front of a camera
// when it lies along the *observed* ray, i.e. dot(X_cam, b) > 0 -- for a
// forward ray that is the z > 0 test, for a sideways fisheye ray it is not.
inline Pose recoverRelativePoseBearing(const std::vector<Vec3>& b1, const std::vector<Vec3>& b2,
                                       const std::vector<int>& idx, const Mat3& E, int& bestCount,
                                       std::vector<char>* mask = nullptr) {
    Mat34 P1 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    Pose best;
    bestCount = -1;
    for (const Pose& cand : decomposeEssential(E)) {
        Mat34 P2 = poseToP(cand);
        int count = 0;
        std::vector<char> m(idx.size(), 0);
        for (size_t k = 0; k < idx.size(); k++) {
            int i = idx[k];
            Vec3 X = triangulateDLT(P1, P2, b1[i], b2[i]);
            Vec3 Xc2 = mul(cand.R, X) + cand.t;
            if (X.dot(b1[i]) > 0 && Xc2.dot(b2[i]) > 0) { count++; m[k] = 1; }
        }
        if (count > bestCount) { bestCount = count; best = cand; if (mask) *mask = m; }
    }
    return best;
}

}  // namespace sfm
