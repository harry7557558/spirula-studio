// Homography estimation (src/sfm/README.md).
//
//   estimateHomography  normalized DLT, >= 4 points (minimal sampler = 4,
//                       also the refit solver since DLT takes any N >= 4).
//   symmetricTransferSq symmetric transfer error (px^2), the RANSAC residual.
#pragma once

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include "sfm/geometry/Essential.h"
#include "sfm/geometry/LinAlg.h"

namespace sfm {

// Hartley-normalized DLT homography H mapping image-1 points to image-2 points.
inline std::vector<Mat3> estimateHomography(const std::vector<Vec2>& p1,
                                            const std::vector<Vec2>& p2,
                                            const std::vector<int>& idx) {
    if (idx.size() < 4) return {};
    std::vector<Vec2> a, b;
    Mat3 T1 = hartleyNormalize(p1, idx, a);
    Mat3 T2 = hartleyNormalize(p2, idx, b);

    size_t n = idx.size();
    std::vector<double> A(2 * n * 9, 0.0);
    for (size_t i = 0; i < n; i++) {
        double x1 = a[i].x, y1 = a[i].y, x2 = b[i].x, y2 = b[i].y;
        double* r0 = &A[(2 * i) * 9];
        double* r1 = &A[(2 * i + 1) * 9];
        r0[0] = -x1; r0[1] = -y1; r0[2] = -1;
        r0[6] = x2 * x1; r0[7] = x2 * y1; r0[8] = x2;
        r1[3] = -x1; r1[4] = -y1; r1[5] = -1;
        r1[6] = y2 * x1; r1[7] = y2 * y1; r1[8] = y2;
    }
    auto nv = nullVectors9(A, (int)(2 * n), 1);
    if (nv.empty()) return {};
    Mat3 Hn;
    for (int i = 0; i < 9; i++) Hn[i] = nv[0][i];
    // denormalize: H = T2^-1 Hn T1
    Mat3 H = mul(mul(inverse3(T2), Hn), T1);
    if (std::fabs(H[8]) > 1e-12)
        for (double& v : H) v /= H[8];
    return {H};
}

// Homography on unit bearings (D45). A world plane maps rays to rays --
// b2 ~ H b1 with H = R + t n^T / d -- so the DLT rows are the same, written
// for homogeneous 3-vectors. No Hartley normalization: see estimateEpipolar*.
//
// The overall sign of H is fixed so that most of the fitted sample transfers
// *forwards* (positive dot). Without that, `angularTransferSq` would have to
// treat a ray and its opposite as equal, and a plane behind the camera would
// score as well as the real one.
inline std::vector<Mat3> estimateHomographyBearing(const std::vector<Vec3>& p1,
                                                   const std::vector<Vec3>& p2,
                                                   const std::vector<int>& idx) {
    size_t n = idx.size();
    if (n < 4) return {};
    std::vector<double> A(2 * n * 9, 0.0);
    for (size_t i = 0; i < n; i++) {
        const Vec3& a = p1[idx[i]];
        const Vec3& b = p2[idx[i]];
        double* r0 = &A[(2 * i) * 9];
        double* r1 = &A[(2 * i + 1) * 9];
        r0[0] = -b.z * a.x; r0[1] = -b.z * a.y; r0[2] = -b.z * a.z;
        r0[6] = b.x * a.x;  r0[7] = b.x * a.y;  r0[8] = b.x * a.z;
        r1[3] = -b.z * a.x; r1[4] = -b.z * a.y; r1[5] = -b.z * a.z;
        r1[6] = b.y * a.x;  r1[7] = b.y * a.y;  r1[8] = b.y * a.z;
    }
    auto nv = nullVectors9(A, (int)(2 * n), 1);
    if (nv.empty()) return {};
    Mat3 H;
    for (int i = 0; i < 9; i++) H[i] = nv[0][i];
    int forward = 0;
    for (size_t i = 0; i < n; i++)
        forward += p2[idx[i]].dot(mul(H, p1[idx[i]])) > 0 ? 1 : -1;
    if (forward < 0) for (double& v : H) v = -v;
    return {H};
}

// Symmetric angular transfer error on the sphere. A ray transferred to the
// opposite hemisphere lands past 90 deg and is rejected, which is the point of
// the sign convention above.
//
// The value is tan^2 of each transfer angle, not the angle squared. The two
// agree to seven digits over the thresholds this is ever compared against (a
// few milliradians -- `--max-error` pixels over a focal length), they are
// monotone in each other everywhere short of 90 deg, and MSAC clamps everything
// past the threshold anyway, so the inlier sets and the scores are the same.
// What it buys is the arithmetic: |u x v|^2 / (u.v)^2 is scale-invariant in
// both arguments, so this runs with no transcendental and no square root at
// all, where the angle form needed two atan2 and four sqrt. It is evaluated
// hundreds of times per RANSAC trial on every pair of a capture.
inline double angularTransferSq(const Mat3& H, const Mat3& Hinv, const Vec3& a, const Vec3& b) {
    const Vec3 hb = mul(H, a), ha = mul(Hinv, b);
    const double d1 = hb.dot(b), d2 = ha.dot(a);
    if (d1 <= 0 || d2 <= 0) return 1e30;  // transferred to the far hemisphere
    const Vec3 c1 = hb.cross(b), c2 = ha.cross(a);
    return c1.dot(c1) / (d1 * d1) + c2.dot(c2) / (d2 * d2);
}

inline Vec2 applyH(const Mat3& H, const Vec2& p) {
    Vec3 q = mul(H, Vec3{p.x, p.y, 1});
    if (std::fabs(q.z) < 1e-30) return {1e15, 1e15};
    return {q.x / q.z, q.y / q.z};
}

// Symmetric transfer error: |b - H a|^2 + |a - H^-1 b|^2.
inline double symmetricTransferSq(const Mat3& H, const Mat3& Hinv, const Vec2& a, const Vec2& b) {
    Vec2 hb = applyH(H, a);
    Vec2 ha = applyH(Hinv, b);
    double d1 = (hb.x - b.x) * (hb.x - b.x) + (hb.y - b.y) * (hb.y - b.y);
    double d2 = (ha.x - a.x) * (ha.x - a.x) + (ha.y - a.y) * (ha.y - a.y);
    return d1 + d2;
}

struct HomographyPoseCandidate {
    Mat3 R = mat3Identity();
    Vec3 t;
    Vec3 normal;
};

inline double homographyOppositeMinor(const Mat3& matrix, int row, int col) {
    const int col1 = col == 0 ? 1 : 0;
    const int col2 = col == 2 ? 1 : 2;
    const int row1 = row == 0 ? 1 : 0;
    const int row2 = row == 2 ? 1 : 2;
    return matrix[3 * row1 + col2] * matrix[3 * row2 + col1] -
           matrix[3 * row1 + col1] * matrix[3 * row2 + col2];
}

inline std::vector<HomographyPoseCandidate> decomposeHomographyBearing(const Mat3& H) {
    for (double value : H) if (!std::isfinite(value)) return {};
    Svd3 svd = svd3(H);
    if (!(svd.s.y > 1e-12) || !std::isfinite(svd.s.y)) return {};
    Mat3 h = H;
    for (double& value : h) value /= svd.s.y;
    if (det3(h) < 0) for (double& value : h) value = -value;

    Mat3 S = mul(transpose(h), h);
    for (int i = 0; i < 3; i++) S[3 * i + i] -= 1;
    double max_abs = 0;
    for (double value : S) max_abs = std::max(max_abs, std::fabs(value));
    if (max_abs < 1e-3) return {{{nearestRotation(h), {0, 0, 0}, {0, 0, 0}}}};

    const double M00 = homographyOppositeMinor(S, 0, 0);
    const double M11 = homographyOppositeMinor(S, 1, 1);
    const double M22 = homographyOppositeMinor(S, 2, 2);
    const double rtM00 = std::sqrt(std::max(M00, 0.0));
    const double rtM11 = std::sqrt(std::max(M11, 0.0));
    const double rtM22 = std::sqrt(std::max(M22, 0.0));
    const double M01 = homographyOppositeMinor(S, 0, 1);
    const double M12 = homographyOppositeMinor(S, 1, 2);
    const double M02 = homographyOppositeMinor(S, 0, 2);
    const double diag_abs[3] = {std::fabs(S[0]), std::fabs(S[4]), std::fabs(S[8])};
    int axis = 0;
    if (diag_abs[1] > diag_abs[axis]) axis = 1;
    if (diag_abs[2] > diag_abs[axis]) axis = 2;
    const auto sign = [](double value) { return value < 0 ? -1.0 : value > 0 ? 1.0 : 0.0; };

    Vec3 np1, np2;
    if (axis == 0) {
        np1 = {S[0], S[1] + rtM22, S[2] + sign(M12) * rtM11};
        np2 = {S[0], S[1] - rtM22, S[2] - sign(M12) * rtM11};
    } else if (axis == 1) {
        np1 = {S[1] + rtM22, S[4], S[5] - sign(M02) * rtM00};
        np2 = {S[1] - rtM22, S[4], S[5] + sign(M02) * rtM00};
    } else {
        np1 = {S[2] + sign(M01) * rtM11, S[5] + rtM00, S[8]};
        np2 = {S[2] - sign(M01) * rtM11, S[5] - rtM00, S[8]};
    }
    if (!(np1.norm() > 1e-12) || !(np2.norm() > 1e-12)) return {};
    const Vec3 n1 = np1.normalized(), n2 = np2.normalized();
    const double trace = S[0] + S[4] + S[8];
    const double v = 2 * std::sqrt(std::max(1 + trace - M00 - M11 - M22, 0.0));
    if (!(v > 1e-12)) return {};
    const double r = std::sqrt(std::max(2 + trace + v, 0.0));
    const double nt = std::sqrt(std::max(2 + trace - v, 0.0));
    const double signed_r = sign(S[3 * axis + axis]) * r;
    const Vec3 t1star = (n2 * signed_r - n1 * nt) * (0.5 * nt);
    const Vec3 t2star = (n1 * signed_r - n2 * nt) * (0.5 * nt);
    const auto make = [&](const Vec3& tstar, const Vec3& normal) {
        Mat3 correction = mat3Identity();
        for (int row = 0; row < 3; row++)
            for (int col = 0; col < 3; col++)
                correction[3 * row + col] -= 2 * (&tstar.x)[row] * (&normal.x)[col] / v;
        const Mat3 rotation = nearestRotation(mul(h, correction));
        return HomographyPoseCandidate{rotation, mul(rotation, tstar), normal};
    };
    const HomographyPoseCandidate first = make(t1star, n1);
    const HomographyPoseCandidate second = make(t2star, n2);
    return {first, {first.R, first.t * -1, first.normal * -1},
            second, {second.R, second.t * -1, second.normal * -1}};
}

inline bool recoverHomographyPoseBearing(const std::vector<Vec3>& b1, const std::vector<Vec3>& b2,
                                         const std::vector<int>& indices, const Mat3& H,
                                         HomographyPoseCandidate& pose, int& best_count,
                                         std::vector<char>* mask = nullptr) {
    best_count = 0;
    double best_error = std::numeric_limits<double>::infinity();
    std::vector<char> best_mask;
    for (const HomographyPoseCandidate& candidate : decomposeHomographyBearing(H)) {
        const double baseline = candidate.t.norm();
        if (!(baseline > 1e-12) || !std::isfinite(baseline)) continue;
        int count = 0;
        double error = 0;
        std::vector<char> candidate_mask(indices.size(), 0);
        for (size_t k = 0; k < indices.size(); k++) {
            const int i = indices[k];
            const Vec3 ray = mul(candidate.R, b1[i]);
            const double cosine = ray.dot(b2[i]);
            const double denominator = 1 - cosine * cosine;
            if (!(denominator > 1e-8)) continue;
            const double ray_t = ray.dot(candidate.t);
            const double target_t = b2[i].dot(candidate.t);
            const double depth1 = (-ray_t + cosine * target_t) / denominator;
            const double depth2 = (-cosine * ray_t + target_t) / denominator;
            if (!(depth1 > 0) || !(depth2 > 0)) continue;
            const Vec3 projected = ray * depth1 + candidate.t;
            const double projected_norm = projected.norm();
            if (!(projected_norm > 1e-12)) continue;
            const double alignment = std::max(-1.0, std::min(1.0, projected.dot(b2[i]) / projected_norm));
            error += 1 - alignment;
            candidate_mask[k] = 1;
            count++;
        }
        if (count > best_count || (count == best_count && error < best_error)) {
            best_count = count;
            best_error = error;
            pose = candidate;
            best_mask.swap(candidate_mask);
        }
    }
    if (mask) *mask = std::move(best_mask);
    return best_count > 0;
}

}  // namespace sfm
