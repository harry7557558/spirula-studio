// Absolute pose (PnP): recover a camera pose from 2D-3D correspondences
// (src/sfm/README.md).
//
// MVP: a normalized DLT solver (>= 6 points) inside LO-RANSAC. The Nister-style
// P3P minimal solver is deferred (D9); DLT is robust enough here because
// register-next selects the image with the most 2D-3D matches, i.e. a high
// inlier ratio, and global BA refines the pose afterward. Works in normalized
// image coordinates (K^-1 applied), so it returns [R|t] directly.
#pragma once

#include <cmath>
#include <vector>

#include "sfm/core/Pose.h"
#include "sfm/geometry/Essential.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/P3P.h"
#include "sfm/optim/Ransac.h"

namespace sfm {

// DLT pose from >= 6 correspondences: world points `X` and unit bearings `b`.
// Uses the forward-ray perspective form (b.x/b.z, b.y/b.z), bit-identical to the
// old normalized-coordinate DLT. Returns 0 or 1 candidate.
inline std::vector<Pose> estimatePoseDLT(const std::vector<Vec3>& X, const std::vector<Vec3>& b,
                                         const std::vector<int>& idx) {
    if (idx.size() < 6) return {};
    // Normalize world points (centroid + isotropic scale) for conditioning.
    Vec3 c{};
    for (int i : idx) c = c + X[i];
    c = c * (1.0 / idx.size());
    double meanDist = 0;
    for (int i : idx) meanDist += (X[i] - c).norm();
    meanDist /= idx.size();
    double sw = meanDist > 1e-12 ? std::sqrt(3.0) / meanDist : 1.0;

    size_t n = idx.size();
    std::vector<double> A(2 * n * 12, 0.0);
    for (size_t k = 0; k < n; k++) {
        int i = idx[k];
        Vec3 W = (X[i] - c) * sw;  // normalized world
        double u = b[i].x / b[i].z, v = b[i].y / b[i].z;
        double Xh[4] = {W.x, W.y, W.z, 1.0};
        double* r0 = &A[(2 * k) * 12];
        double* r1 = &A[(2 * k + 1) * 12];
        for (int t = 0; t < 4; t++) {
            r0[t] = Xh[t];
            r0[8 + t] = -u * Xh[t];
            r1[4 + t] = Xh[t];
            r1[8 + t] = -v * Xh[t];
        }
    }
    std::vector<double> p = nullspaceVector(A, (int)(2 * n), 12);
    // P_n maps normalized-world homogeneous -> image. Undo the world scaling:
    // P = P_n * S, S = [[sw I, -sw c],[0,1]].
    Mat3 M = {p[0], p[1], p[2], p[4], p[5], p[6], p[8], p[9], p[10]};
    Vec3 p4 = {p[3], p[7], p[11]};
    // Actual R (unnormalized) = M * sw ; actual t = p4 - M*(sw c) ... fold below.
    Mat3 Rraw = {M[0] * sw, M[1] * sw, M[2] * sw, M[3] * sw, M[4] * sw,
                 M[5] * sw, M[6] * sw, M[7] * sw, M[8] * sw};
    Vec3 traw = {p4.x - (M[0] * c.x + M[1] * c.y + M[2] * c.z) * sw,
                 p4.y - (M[3] * c.x + M[4] * c.y + M[5] * c.z) * sw,
                 p4.z - (M[6] * c.x + M[7] * c.y + M[8] * c.z) * sw};

    // The null vector is defined only up to a nonzero scalar (arbitrary sign
    // and magnitude). Fix the sign so the rotation block is proper (det > 0):
    // the improper sign is the mirror solution that puts points behind the
    // camera, so det > 0 simultaneously fixes handedness and cheirality.
    if (det3(Rraw) < 0) {
        for (int i = 0; i < 9; i++) Rraw[i] = -Rraw[i];
        traw = {-traw.x, -traw.y, -traw.z};
    }
    double lambda = (Vec3{Rraw[0], Rraw[3], Rraw[6]}.norm() + Vec3{Rraw[1], Rraw[4], Rraw[7]}.norm() +
                     Vec3{Rraw[2], Rraw[5], Rraw[8]}.norm()) / 3.0;
    if (lambda < 1e-12) return {};
    Mat3 Rn;
    for (int i = 0; i < 9; i++) Rn[i] = Rraw[i] / lambda;
    Pose pose;
    pose.R = nearestRotation(Rn);
    pose.t = traw * (1.0 / lambda);
    return {pose};
}

// Squared PnP residual for an observed unit bearing `b`. For a forward ray
// (b.z above the threshold -- every rectilinear lens) this is the classic
// normalized-plane error, bit-identical to the pre-bearing code. A wide/fisheye
// bearing uses sin^2 of the angle between the predicted ray and `b`, which has
// the same small-angle scale (so the same max_error/focal threshold applies) and
// is defined for points at or past 90 deg from the axis (p.z <= 0).
inline double pnpResidualSq(const Pose& pose, const Vec3& X, const Vec3& b) {
    Vec3 p = mul(pose.R, X) + pose.t;
    if (b.z > 0.1) {
        if (p.z < 1e-8) return 1e30;  // cheirality (forward hemisphere)
        double du = p.x / p.z - b.x / b.z, dv = p.y / p.z - b.y / b.z;
        return du * du + dv * dv;
    }
    if (p.dot(b) <= 0) return 1e30;   // cheirality along the ray
    Vec3 ph = p.normalized();         // b is already unit
    Vec3 cr = ph.cross(b);
    return cr.dot(cr);                // sin^2(angle)
}

struct PnPResult {
    Pose pose;
    std::vector<char> inlier_mask;
    int num_inliers = 0;
    bool success = false;
};

namespace pose_detail {

// The two residual components of one correspondence under pose `p`, the
// bearing reinterpreted at focal scale `s`. A cheirality failure is a large
// constant with no gradient: it cannot steer a step, only get it rejected.
inline void residualPair(const Pose& p, const Vec3& X, const Vec3& bi, double s, double* r) {
    Vec3 pc = mul(p.R, X) + p.t;
    if (bi.z > 0.1) {
        if (pc.z < 1e-8) { r[0] = r[1] = 1e3; return; }
        r[0] = pc.x / pc.z - bi.x / (bi.z * s);
        r[1] = pc.y / pc.z - bi.y / (bi.z * s);
        return;
    }
    if (pc.dot(bi) <= 0) { r[0] = r[1] = 1e3; return; }
    // tangent-plane components of the direction error (matches sin^2 form)
    Vec3 e1 = (std::fabs(bi.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0}).cross(bi).normalized();
    Vec3 e2 = bi.cross(e1);
    Vec3 ph = pc.normalized();
    r[0] = ph.dot(e1);
    r[1] = ph.dot(e2);
}

// LM over (angle-axis delta, translation[, log focal scale]) with a
// central-difference Jacobian: `resid(pose, s, j, r)` fills the two residual
// components of correspondence j of n, 1e3 marking a cheirality failure.
template <class Resid>
bool lmRefine(int n, const Resid& resid, int NP, Pose& pose, double& s0, int max_iters) {
    auto cost = [&](const Pose& p, double s) {
        double c = 0, r[2];
        for (int i = 0; i < n; i++) { resid(p, s, i, r); c += r[0] * r[0] + r[1] * r[1]; }
        return c;
    };
    // Compose a step onto a base state: R <- exp(w) R0, t <- t0 + dt,
    // s <- s0 * exp(ds) (multiplicative: focal is a positive scale).
    auto stepP = [&](const Pose& p0, const double* d) {
        Pose p;
        p.R = mul(angleAxisToRotation({d[0], d[1], d[2]}), p0.R);
        p.t = {p0.t.x + d[3], p0.t.y + d[4], p0.t.z + d[5]};
        return p;
    };
    auto stepS = [&](double s, const double* d) { return NP == 7 ? s * std::exp(d[6]) : s; };

    double lambda = 1e-4, c0 = cost(pose, s0);
    for (int it = 0; it < max_iters; it++) {
        // J^T J and J^T r accumulated point by point (numeric Jacobian).
        double JtJ[49] = {0}, Jtr[7] = {0};
        const double h = 1e-6;
        for (int i = 0; i < n; i++) {
            double J[2][7], rp[2], rm[2], r0[2];
            resid(pose, s0, i, r0);
            if (r0[0] >= 1e3) continue;
            for (int k = 0; k < NP; k++) {
                double d[7] = {0, 0, 0, 0, 0, 0, 0};
                d[k] = h;
                resid(stepP(pose, d), stepS(s0, d), i, rp);
                d[k] = -h;
                resid(stepP(pose, d), stepS(s0, d), i, rm);
                J[0][k] = (rp[0] - rm[0]) / (2 * h);
                J[1][k] = (rp[1] - rm[1]) / (2 * h);
            }
            for (int a = 0; a < NP; a++) {
                for (int c = 0; c < NP; c++)
                    JtJ[NP * a + c] += J[0][a] * J[0][c] + J[1][a] * J[1][c];
                Jtr[a] += J[0][a] * r0[0] + J[1][a] * r0[1];
            }
        }
        // (JtJ + lambda diag) d = -Jtr, solved by Gaussian elimination.
        double A[49], g[7], d[7];
        bool solved = false;
        for (int tries = 0; tries < 8 && !solved; tries++) {
            for (int a = 0; a < NP * NP; a++) A[a] = JtJ[a];
            for (int a = 0; a < NP; a++) {
                A[(NP + 1) * a] += lambda * std::max(JtJ[(NP + 1) * a], 1e-12);
                g[a] = -Jtr[a];
            }
            solved = true;
            for (int col = 0; col < NP && solved; col++) {
                int piv = col;
                for (int rw = col + 1; rw < NP; rw++)
                    if (std::fabs(A[NP * rw + col]) > std::fabs(A[NP * piv + col])) piv = rw;
                if (std::fabs(A[NP * piv + col]) < 1e-14) { solved = false; break; }
                if (piv != col) {
                    for (int c = 0; c < NP; c++) std::swap(A[NP * piv + c], A[NP * col + c]);
                    std::swap(g[piv], g[col]);
                }
                for (int rw = col + 1; rw < NP; rw++) {
                    double m = A[NP * rw + col] / A[NP * col + col];
                    for (int c = col; c < NP; c++) A[NP * rw + c] -= m * A[NP * col + c];
                    g[rw] -= m * g[col];
                }
            }
            if (!solved) lambda *= 10;
        }
        if (!solved) break;
        for (int a = NP - 1; a >= 0; a--) {
            double sum = g[a];
            for (int c = a + 1; c < NP; c++) sum -= A[NP * a + c] * d[c];
            d[a] = sum / A[NP * a + a];
        }
        Pose trialP = stepP(pose, d);
        double trialS = stepS(s0, d);
        double c1 = cost(trialP, trialS);
        if (c1 < c0) {
            double dn = 0;
            for (int a = 0; a < NP; a++) dn += d[a] * d[a];
            pose = trialP;
            s0 = trialS;
            lambda = std::max(lambda * 0.3, 1e-10);
            bool converged = c0 - c1 < 1e-12 * std::max(1.0, c0) || dn < 1e-20;
            c0 = c1;
            if (converged) break;
        } else {
            lambda *= 10;
            if (lambda > 1e8) break;
        }
    }
    return true;
}

}  // namespace pose_detail

// LM refinement of a pose over the masked correspondences, on the residual
// RANSAC scored (COLMAP refines every PnP pose). `focal_scale` adds a 7th
// parameter, the bearings reinterpreted at f0*s; wide-angle rays stay fixed.
inline bool refinePose(const std::vector<Vec3>& X, const std::vector<Vec3>& b,
                       const std::vector<char>& mask, Pose& pose, double* focal_scale = nullptr,
                       int max_iters = 30) {
    std::vector<int> idx;
    for (size_t i = 0; i < X.size(); i++)
        if (mask.empty() || mask[i]) idx.push_back((int)i);
    if (idx.size() < 4) return false;
    auto resid = [&](const Pose& p, double s, int j, double* r) {
        pose_detail::residualPair(p, X[idx[j]], b[idx[j]], s, r);
    };
    double s0 = focal_scale ? *focal_scale : 1.0;
    const bool ok = pose_detail::lmRefine((int)idx.size(), resid, focal_scale ? 7 : 6, pose, s0,
                                          max_iters);
    if (focal_scale) *focal_scale = s0;
    return ok;
}

// One member of a rig frame: its correspondences, which of them count, and
// where it sits on the rig.
struct FrameMember {
    const std::vector<Vec3>* X;
    const std::vector<Vec3>* b;
    const std::vector<char>* mask;
    Pose cam_from_rig;
};

// The same refinement over a whole frame: one pose (rig_from_world) explains
// every member's inliers through its extrinsic, so lenses that share no
// view still constrain the frame together.
inline bool refineFramePose(const std::vector<FrameMember>& members, Pose& rig_from_world,
                            int max_iters = 30) {
    std::vector<std::pair<int, int>> idx;
    for (size_t m = 0; m < members.size(); m++)
        for (size_t i = 0; i < members[m].X->size(); i++)
            if (members[m].mask->empty() || (*members[m].mask)[i]) idx.emplace_back((int)m, (int)i);
    if (idx.size() < 4) return false;
    auto resid = [&](const Pose& F, double s, int j, double* r) {
        const FrameMember& m = members[idx[j].first];
        const int i = idx[j].second;
        pose_detail::residualPair(composePose(m.cam_from_rig, F), (*m.X)[i], (*m.b)[i], s, r);
    };
    double s0 = 1.0;
    return pose_detail::lmRefine((int)idx.size(), resid, 6, rig_from_world, s0, max_iters);
}

// LO-RANSAC PnP over 2D-3D correspondences given as world points `X` and unit
// bearings `b`. `max_error_px` is converted to normalized units via `focal`.
// `max_trials` caps the RANSAC budget. The default is the mapper's; a caller
// that runs this over every image of a model at once (the D44 audit) pays the
// full budget on every image whose correspondences are noise, which is most of
// them, and does not need the deep search.
inline PnPResult ransacPnP(const std::vector<Vec3>& X, const std::vector<Vec3>& b, double focal,
                           double max_error_px = 4.0, unsigned seed = 0, int max_trials = 3000) {
    PnPResult out;
    int n = (int)X.size();
    if (n < 4) return out;
    // Minimal solver: P3P on the bearings directly (it already consumes unit
    // rays; robust to coplanar/elongated point sets). LO refit: DLT on the
    // inliers (only accepted if it improves).
    auto fit = [&](const std::vector<int>& s) {
        std::array<Vec3, 3> br, Xs;
        for (int k = 0; k < 3; k++) { br[k] = b[s[k]]; Xs[k] = X[s[k]]; }
        return p3p(br, Xs);
    };
    auto refit = [&](const std::vector<int>& s) { return estimatePoseDLT(X, b, s); };
    auto res = [&](const Pose& p, int i) { return pnpResidualSq(p, X[i], b[i]); };
    RansacOptions ro;
    ro.max_error = max_error_px / focal;  // residual is in normalized units
    ro.seed = seed;
    ro.min_num_trials = std::min(100, max_trials);
    ro.max_num_trials = max_trials;
    RansacReport<Pose> rep = loransac<Pose>(n, 3, fit, refit, res, ro);
    out.pose = rep.model;
    out.inlier_mask = rep.inlier_mask;
    out.num_inliers = rep.num_inliers;
    out.success = rep.success;
    return out;
}

}  // namespace sfm
