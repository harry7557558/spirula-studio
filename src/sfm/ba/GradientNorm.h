// Ceres' gradient tolerance test, |x - Plus(x, -g)|_inf, stated over this
// solver's parameters as COLMAP 4.1.1 parameterises its global BA: every pose
// and extrinsic rotation a unit quaternion on EigenQuaternionManifold, the rest
// Euclidean. Lengths are measured in metres, as COLMAP's are once its model is
// aligned to the pose priors. Ceres 2.2 trust_region_minimizer.cc
// EvaluateGradientNorms; the stop itself is SolverOptions::gradient_tol.
#pragma once

#include <algorithm>
#include <cmath>

#include "sfm/ba/Problem.h"
#include "sfm/core/Pose.h"

namespace sfm {

// Plus(q, -g) on EigenQuaternionManifold for the rotation R(aa): its tangent
// is half a left rotation vector, so -g there turns R by Exp(-4 g_phi), where
// g_phi = Jl(aa)^-T g_aa is the gradient in a left rotation of R.
inline double quaternionStepMax(const double aa[3], const double g_aa[3]) {
    const Vec3 a{aa[0], aa[1], aa[2]};
    const double th = a.norm();
    const double s = th > 0 ? std::sin(0.5 * th) / th : 0.5;
    const double q[4] = {std::cos(0.5 * th), s * a.x, s * a.y, s * a.z};
    const Vec3 gphi = mul(transpose(so3LeftJacobianInv(a)), Vec3{g_aa[0], g_aa[1], g_aa[2]});
    const Vec3 d = gphi * -2.0;
    const double n = d.norm();
    if (!std::isfinite(n)) return INFINITY;
    if (n == 0) return 0;
    const double sn = std::sin(n) / n;
    const double w = std::cos(n), x = sn * d.x, y = sn * d.y, z = sn * d.z;
    const double p[4] = {w * q[0] - x * q[1] - y * q[2] - z * q[3],
                         w * q[1] + x * q[0] + y * q[3] - z * q[2],
                         w * q[2] - x * q[3] + y * q[0] + z * q[1],
                         w * q[3] + x * q[2] - y * q[1] + z * q[0]};
    double m = 0;
    for (int i = 0; i < 4; i++) m = std::max(m, std::fabs(p[i] - q[i]));
    return std::isfinite(m) ? m : INFINITY;
}

// The norm's parts, so a test can check each against its own reference.
struct GradientNormParts {
    double rotation = 0, translation = 0, intrinsics = 0, points = 0;
    double max() const { return std::max({rotation, translation, intrinsics, points}); }
};

// `g` holds d cost / d column over P's n_dim columns, priors included;
// `points_max` is max |d cost / d point coordinate|. Both per model unit.
inline GradientNormParts gradientNormParts(const BAProblem& P, const double* poses,
                                           const double* exts, const double* g,
                                           double points_max, double metres_per_unit) {
    GradientNormParts r;
    // Non-finite reads as infinite: std::max would pass a NaN over.
    auto grow = [](double& m, double v) { m = std::max(m, std::isfinite(v) ? v : INFINITY); };
    auto rigid = [&](const double* aa, const double* g6) {
        grow(r.rotation, quaternionStepMax(aa, g6));
        for (int i = 3; i < 6; i++) grow(r.translation, std::fabs(g6[i]) / metres_per_unit);
    };
    for (uint32_t f = 0; f < P.num_frames; f++) rigid(poses + 6 * (size_t)f, g + 6 * (size_t)f);
    for (const BAProblem::Member& m : P.members) {
        if (!m.n_free) continue;
        double g6[6] = {0, 0, 0, 0, 0, 0};
        for (uint32_t i = 0, j = 0; i < 6; i++)
            if ((m.mask >> i) & 1u) g6[i] = g[m.ext_col + j++];
        rigid(exts + m.ext_offset, g6);
    }
    for (const BAProblem::Group& gr : P.groups)
        for (uint32_t j = 0; j < gr.n_intr; j++)
            grow(r.intrinsics, std::fabs(g[gr.intr_col + j]));
    grow(r.points, points_max / metres_per_unit);
    return r;
}

}  // namespace sfm
