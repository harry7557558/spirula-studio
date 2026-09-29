// Rigid pose conversions shared by the mapper, BA, and COLMAP model IO.
//
// A pose is stored as (R, t) with the world->camera convention x_cam = R x + t
// (geometry::Pose in sfm/geometry/Essential.h). This header adds the parameterization
// conversions the rest of the pipeline needs:
//   - quaternion (w,x,y,z), COLMAP's images.bin storage,
//   - angle-axis, the BA solver's pose[0..2] (pose[3..5] = t).
#pragma once

#include <array>
#include <cmath>

#include "sfm/geometry/Essential.h"  // Pose
#include "sfm/geometry/LinAlg.h"

namespace sfm {

using Quat = std::array<double, 4>;  // (w, x, y, z)

inline Quat rotationToQuaternion(const Mat3& R) {
    double tr = R[0] + R[4] + R[8];
    Quat q;
    if (tr > 0) {
        double s = std::sqrt(tr + 1.0) * 2;
        q = {0.25 * s, (R[7] - R[5]) / s, (R[2] - R[6]) / s, (R[3] - R[1]) / s};
    } else if (R[0] > R[4] && R[0] > R[8]) {
        double s = std::sqrt(1.0 + R[0] - R[4] - R[8]) * 2;
        q = {(R[7] - R[5]) / s, 0.25 * s, (R[1] + R[3]) / s, (R[2] + R[6]) / s};
    } else if (R[4] > R[8]) {
        double s = std::sqrt(1.0 + R[4] - R[0] - R[8]) * 2;
        q = {(R[2] - R[6]) / s, (R[1] + R[3]) / s, 0.25 * s, (R[5] + R[7]) / s};
    } else {
        double s = std::sqrt(1.0 + R[8] - R[0] - R[4]) * 2;
        q = {(R[3] - R[1]) / s, (R[2] + R[6]) / s, (R[5] + R[7]) / s, 0.25 * s};
    }
    double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (n > 0) for (double& v : q) v /= n;
    return q;
}

inline Mat3 quaternionToRotation(const Quat& q) {
    double w = q[0], x = q[1], y = q[2], z = q[3];
    double n = std::sqrt(w * w + x * x + y * y + z * z);
    if (n > 0) { w /= n; x /= n; y /= n; z /= n; }
    return {1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y),
            2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
            2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y)};
}

// Through the quaternion everywhere: the skew-part formula divides rounding
// noise by sin(angle) within ~1e-6 rad of pi, which turned a .360 view's
// exact 180-degree rotation into the identity.
inline Vec3 rotationToAngleAxis(const Mat3& R) {
    Quat q = rotationToQuaternion(R);
    if (q[0] < 0)
        for (double& v : q) v = -v;
    const double vn = std::sqrt(q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (vn < 1e-12) return {0, 0, 0};
    const double a = 2.0 * std::atan2(vn, q[0]);
    return {q[1] / vn * a, q[2] / vn * a, q[3] / vn * a};
}

inline Mat3 angleAxisToRotation(const Vec3& aa) {
    double theta = aa.norm();
    if (theta < 1e-12) return mat3Identity();
    Vec3 k = aa * (1.0 / theta);
    double c = std::cos(theta), s = std::sin(theta), v = 1 - c;
    return {c + k.x * k.x * v,       k.x * k.y * v - k.z * s, k.x * k.z * v + k.y * s,
            k.y * k.x * v + k.z * s, c + k.y * k.y * v,       k.y * k.z * v - k.x * s,
            k.z * k.x * v - k.y * s, k.z * k.y * v + k.x * s, c + k.z * k.z * v};
}

// SO(3) exponential map and its Jacobians (Barfoot, "State Estimation for
// Robotics", 7.1). Left: Exp(a + d) ~ Exp(Jl(a) d) Exp(a); right: ~ Exp(a)
// Exp(Jr(a) d), Jr(a) = Jl(-a).
inline Mat3 so3Exp(const Vec3& phi) { return angleAxisToRotation(phi); }
inline Vec3 so3Log(const Mat3& R) { return rotationToAngleAxis(R); }

inline Mat3 so3RightJacobian(const Vec3& phi) {
    const double th = phi.norm();
    const Mat3 K = crossMatrix(phi);
    const Mat3 K2 = mul(K, K);
    double a, b;
    if (th < 1e-5) {
        a = 0.5;
        b = 1.0 / 6.0;
    } else {
        a = (1.0 - std::cos(th)) / (th * th);
        b = (th - std::sin(th)) / (th * th * th);
    }
    Mat3 J = mat3Identity();
    for (int i = 0; i < 9; i++) J[i] += -a * K[i] + b * K2[i];
    return J;
}
inline Mat3 so3LeftJacobian(const Vec3& phi) { return so3RightJacobian(phi * -1.0); }

// Jl^-1(phi) = I - K/2 + (1/th^2 - (1 + cos th) / (2 th sin th)) K^2.
inline Mat3 so3LeftJacobianInv(const Vec3& phi) {
    const double th = phi.norm();
    const Mat3 K = crossMatrix(phi);
    const Mat3 K2 = mul(K, K);
    const double c = th < 1e-5 ? 1.0 / 12.0
                               : 1.0 / (th * th) - (1.0 + std::cos(th)) / (2.0 * th * std::sin(th));
    Mat3 J = mat3Identity();
    for (int i = 0; i < 9; i++) J[i] += -0.5 * K[i] + c * K2[i];
    return J;
}
inline Mat3 so3RightJacobianInv(const Vec3& phi) { return so3LeftJacobianInv(phi * -1.0); }

// Camera center in world coordinates: c = -R^T t.
inline Vec3 cameraCenter(const Pose& p) {
    Mat3 Rt = transpose(p.R);
    Vec3 c = mul(Rt, p.t);
    return {-c.x, -c.y, -c.z};
}

// Compose: (A then B) applied as x -> B(A(x)); returns world->cam2 given
// world->cam1 (a) and cam1->cam2 (b).
inline Pose composePose(const Pose& b, const Pose& a) {
    return {mul(b.R, a.R), mul(b.R, a.t) + b.t};
}

// ---- similarity transforms ----
//
// X' = scale * R * X + t. This is exactly the gauge freedom of a monocular
// reconstruction, and therefore the transform relating two reconstructions of
// the same scene -- what model merging estimates (sfm/map/Merge.h, D43).
struct Sim3 {
    double scale = 1.0;
    Mat3 R = mat3Identity();
    Vec3 t = {0, 0, 0};
};

inline Vec3 transformPoint(const Sim3& T, const Vec3& X) {
    return mul(T.R, X) * T.scale + T.t;
}

// The same camera, expressed in the transformed world frame. With
// X' = s R X + t and x_cam = R_c X + t_c, substituting X = R^T (X' - t) / s
// gives x_cam' = (R_c R^T) X' + (s t_c - R_c R^T t), where x_cam' = s x_cam:
// camera coordinates scale with the world, which projection ignores. The
// rotation stays orthonormal, so the result is a proper Pose.
inline Pose transformPose(const Sim3& T, const Pose& p) {
    Mat3 R = mul(p.R, transpose(T.R));
    return {R, p.t * T.scale - mul(R, T.t)};
}

// (A then B) applied as x -> B(A(x)), matching composePose's argument order.
inline Sim3 composeSim3(const Sim3& b, const Sim3& a) {
    return {b.scale * a.scale, mul(b.R, a.R), mul(b.R, a.t) * b.scale + b.t};
}

inline Sim3 invertSim3(const Sim3& T) {
    Sim3 inv;
    inv.scale = 1.0 / T.scale;
    inv.R = transpose(T.R);
    inv.t = mul(inv.R, T.t) * (-inv.scale);
    return inv;
}

}  // namespace sfm
