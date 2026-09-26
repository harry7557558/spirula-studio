// Two-view geometry and absolute pose with the rotation supplied by a sensor
// (docs/notes/sensor-priors.md). With R known the epipolar constraint
// b2^T [t]x R b1 = 0 is linear in t and two correspondences fix it (Kneip,
// Chli and Siegwart, BMVC 2011), so the RANSAC samples two points instead of
// seven and keeps only what that rotation can explain: a copy of the scene
// elsewhere, or equipment that moves with the camera, is thrown out however
// consistent it is with some other geometry.
#pragma once

#include <cmath>
#include <vector>

#include "sfm/geometry/AbsolutePose.h"
#include "sfm/geometry/Essential.h"
#include "sfm/geometry/Fundamental.h"
#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/Triangulation.h"
#include "sfm/optim/Ransac.h"

namespace sfm {

struct KnownRotationOptions {
    RansacOptions ransac;   // max_error in radians (the Sampson error on the sphere)
    int min_num_inliers = 15;
    // Past this share of the inliers explained by the rotation alone, the
    // pair is a panorama and t is noise (TwoViewOptions::max_H_inlier_ratio).
    double max_rotation_only_ratio = 0.8;
    // How far the given rotation may be off, radians: a calibration good to
    // a degree is far coarser than a pixel, and a rotation taken as exact
    // lost half its pairs (docs/notes/sensor-priors.md, section 4).
    double rot_sigma = 0;
    // Another starting pose for the refinement, the free estimate's when it
    // has one: the two-point fit under a wrong rotation can start it badly.
    const Pose* start = nullptr;
    KnownRotationOptions() {
        ransac.max_error = 0.003;
        ransac.min_inlier_ratio = 0.0;
    }
};

struct KnownRotationGeometry {
    bool ok = false;
    bool panoramic = false;   // R alone explains the pair; `pose.t` is undetermined
    std::vector<char> inlier_mask;
    int num_inliers = 0;
    int rotation_only = 0;    // correspondences with b2 ~ R b1
    int loose_inliers = 0;    // under the gate the prior's sigma widened
    double moved_deg = 0;     // how far the refinement took R from the prior
    Pose pose;                // R as refined, unit t (camera 1 is the world)
};

namespace known_rotation_detail {

// The smallest-eigenvalue direction of sum m m^T: the translation every
// epipolar normal is orthogonal to.
inline bool leastNormal(const std::vector<Vec3>& m, const std::vector<int>& idx, Vec3& t) {
    std::vector<double> N(9, 0.0), ev, V;
    for (int i : idx) {
        const double v[3] = {m[i].x, m[i].y, m[i].z};
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++) N[3 * r + c] += v[r] * v[c];
    }
    jacobiEigenSymmetric(N, 3, ev, V);
    int k = 0;
    for (int i = 1; i < 3; i++)
        if (ev[i] < ev[k]) k = i;
    t = {V[k], V[3 + k], V[6 + k]};
    const double n = t.norm();
    if (!(n > 1e-12) || !(ev[k] < 0.5 * (ev[0] + ev[1] + ev[2]))) return false;
    t = t * (1.0 / n);
    return true;
}


// Signed Sampson error on the sphere (the square root of sampsonSqBearing).
inline double sampsonBearing(const Mat3& E, const Vec3& b1, const Vec3& b2) {
    const Vec3 Eb1 = mul(E, b1);
    const Vec3 Etb2 = mul(transpose(E), b2);
    const double num = b2.dot(Eb1);
    const Vec3 g1 = Etb2 - b1 * b1.dot(Etb2);
    const Vec3 g2 = Eb1 - b2 * b2.dot(Eb1);
    const double den = g1.dot(g1) + g2.dot(g2);
    return den > 1e-30 ? num / std::sqrt(den) : 1e15;
}

// IRLS Levenberg-Marquardt over (d, a, b), R = Exp(d) R0 from `pose`, t moved
// in its tangent plane: the masked Sampson residuals over `radius` under a
// Cauchy loss (so the loose gate's far outliers lose their pull) plus d / sigma.
inline double refineNearRotation(const std::vector<Vec3>& b1, const std::vector<Vec3>& b2,
                                 const std::vector<char>& mask, const Mat3& R0, double sigma,
                                 double radius, Pose& pose, int iters = 20) {
    const double inv_r = 1.0 / std::max(radius, 1e-12);
    std::vector<int> idx;
    for (size_t i = 0; i < mask.size(); i++)
        if (mask[i]) idx.push_back((int)i);
    if (idx.size() < 5) return 1e300;
    const int n = (int)idx.size();
    const size_t m = (size_t)n + 3;
    const Vec3 d0 = so3Log(mul(pose.R, transpose(R0)));
    double x[5] = {d0.x, d0.y, d0.z, 0, 0};
    Vec3 t0 = pose.t.norm() > 1e-12 ? pose.t * (1.0 / pose.t.norm()) : Vec3{0, 0, 1};
    Vec3 tu, tv;
    auto tangentBasis = [&]() {
        const Vec3 ax = std::fabs(t0.x) < 0.9 ? Vec3{1, 0, 0} : Vec3{0, 1, 0};
        tu = ax.cross(t0);
        tu = tu * (1.0 / tu.norm());
        tv = t0.cross(tu);
    };
    tangentBasis();
    auto tAt = [&](const double* q) {
        Vec3 t = t0 + tu * q[3] + tv * q[4];
        return t * (1.0 / t.norm());
    };
    auto evalAt = [&](const double* q, std::vector<double>& e) {
        const Mat3 R = mul(angleAxisToRotation({q[0], q[1], q[2]}), R0);
        const Mat3 E = mul(crossMatrix(tAt(q)), R);
        e.resize(m);
        for (int k = 0; k < n; k++)
            e[(size_t)k] = inv_r * sampsonBearing(E, b1[(size_t)idx[k]], b2[(size_t)idx[k]]);
        for (int i = 0; i < 3; i++) e[(size_t)n + i] = sigma > 0 ? q[i] / sigma : 0.0;
    };
    auto robust = [&](const std::vector<double>& e) {
        double c = 0;
        for (int k = 0; k < n; k++) c += std::log1p(e[(size_t)k] * e[(size_t)k]);
        for (int i = 0; i < 3; i++) c += e[(size_t)n + i] * e[(size_t)n + i];
        return c;
    };
    std::vector<double> e0, ep, em, w(m, 1.0), J(m * 5);
    evalAt(x, e0);
    double c0 = robust(e0), lambda = 1e-3;
    for (int it = 0; it < iters; it++) {
        for (int k = 0; k < n; k++) w[(size_t)k] = 1.0 / (1.0 + e0[(size_t)k] * e0[(size_t)k]);
        for (int p = 0; p < 5; p++) {
            const double h = 1e-6;
            double xp[5], xm[5];
            for (int k = 0; k < 5; k++) xp[k] = xm[k] = x[k];
            xp[p] += h;
            xm[p] -= h;
            evalAt(xp, ep);
            evalAt(xm, em);
            for (size_t k = 0; k < m; k++) J[k * 5 + (size_t)p] = (ep[k] - em[k]) / (2 * h);
        }
        double H[25] = {0}, g[5] = {0};
        for (size_t k = 0; k < m; k++)
            for (int a = 0; a < 5; a++) {
                g[a] += w[k] * J[k * 5 + (size_t)a] * e0[k];
                for (int b = 0; b < 5; b++) H[5 * a + b] += w[k] * J[k * 5 + (size_t)a] * J[k * 5 + (size_t)b];
            }
        bool accepted = false;
        for (int tries = 0; tries < 6 && !accepted; tries++) {
            double A[25], d[5], rhs[5];
            for (int i = 0; i < 25; i++) A[i] = H[i];
            for (int a = 0; a < 5; a++) {
                A[6 * a] += lambda * std::max(H[6 * a], 1e-12);
                rhs[a] = -g[a];
            }
            bool ok = true;
            for (int c = 0; c < 5 && ok; c++) {
                int piv = c;
                for (int rw = c + 1; rw < 5; rw++)
                    if (std::fabs(A[5 * rw + c]) > std::fabs(A[5 * piv + c])) piv = rw;
                if (std::fabs(A[5 * piv + c]) < 1e-18) { ok = false; break; }
                if (piv != c) {
                    for (int k = 0; k < 5; k++) std::swap(A[5 * piv + k], A[5 * c + k]);
                    std::swap(rhs[piv], rhs[c]);
                }
                for (int rw = c + 1; rw < 5; rw++) {
                    const double f = A[5 * rw + c] / A[5 * c + c];
                    for (int k = c; k < 5; k++) A[5 * rw + k] -= f * A[5 * c + k];
                    rhs[rw] -= f * rhs[c];
                }
            }
            if (!ok) { lambda *= 10; continue; }
            for (int a = 4; a >= 0; a--) {
                double sum = rhs[a];
                for (int k = a + 1; k < 5; k++) sum -= A[5 * a + k] * d[k];
                d[a] = sum / A[5 * a + a];
            }
            double xn[5];
            for (int k = 0; k < 5; k++) xn[k] = x[k] + d[k];
            evalAt(xn, ep);
            const double c1 = robust(ep);
            if (c1 < c0) {
                const double drop = (c0 - c1) / std::max(c0, 1e-300);
                for (int k = 0; k < 3; k++) x[k] = xn[k];
                t0 = tAt(xn);
                x[3] = x[4] = 0;
                tangentBasis();
                e0.swap(ep);
                c0 = c1;
                lambda = std::max(lambda * 0.3, 1e-9);
                accepted = true;
                if (drop < 1e-6) it = iters;
            } else {
                lambda *= 10;
            }
        }
        if (!accepted) break;
    }
    pose.R = mul(angleAxisToRotation({x[0], x[1], x[2]}), R0);
    pose.t = t0;
    return c0;
}

}  // namespace known_rotation_detail

// `R` takes camera-1 bearings into camera 2: b2 ~ R b1 + t-parallax.
inline KnownRotationGeometry estimateTwoViewKnownRotation(const std::vector<Vec3>& b1,
                                                          const std::vector<Vec3>& b2,
                                                          const Mat3& R,
                                                          const KnownRotationOptions& opt) {
    using namespace known_rotation_detail;
    KnownRotationGeometry g;
    const int n = (int)b1.size();
    if (n < 2) return g;
    const double strict = opt.ransac.max_error;
    const double loose = strict + 2.5 * opt.rot_sigma;
    // m_k = (R b1) x b2: t must be orthogonal to every one of them, and a
    // vanishing m_k is a ray the rotation alone explains.
    std::vector<Vec3> m(n);
    std::vector<char> rot_only(n, 0);
    int rot_only_loose = 0;
    const double sin_loose = std::sin(loose);
    for (int k = 0; k < n; k++) {
        m[k] = mul(R, b1[k]).cross(b2[k]);
        rot_only_loose += m[k].norm() < sin_loose ? 1 : 0;
    }
    auto fit = [&](const std::vector<int>& s) {
        std::vector<Vec3> out;
        const Vec3 t = m[s[0]].cross(m[s[1]]);
        const double tn = t.norm();
        if (tn > 1e-9) out.push_back(t * (1.0 / tn));
        return out;
    };
    auto refit = [&](const std::vector<int>& s) {
        std::vector<Vec3> out;
        Vec3 t;
        if (leastNormal(m, s, t)) out.push_back(t);
        return out;
    };
    auto res = [&](const Vec3& t, int k) {
        return sampsonSqBearing(mul(crossMatrix(t), R), b1[k], b2[k]);
    };
    RansacOptions ro = opt.ransac;
    ro.max_error = loose;
    RansacReport<Vec3> rep = loransac<Vec3>(n, 2, fit, refit, res, ro);
    if (!rep.success || rep.num_inliers < opt.min_num_inliers) {
        // Nothing but the rotation: a panorama, or no geometry at all.
        if (rot_only_loose >= opt.min_num_inliers) {
            g.ok = g.panoramic = true;
            for (int k = 0; k < n; k++) rot_only[k] = m[k].norm() < sin_loose ? 1 : 0;
            g.inlier_mask = rot_only;
            g.num_inliers = g.rotation_only = rot_only_loose;
            g.pose.R = R;
        }
        return g;
    }
    // The sign of t: whichever puts more loose inliers in front of both cameras.
    const Mat34 P1 = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
    Pose pose{R, rep.model};
    int best = -1;
    for (double sgn : {1.0, -1.0}) {
        const Pose cand{R, rep.model * sgn};
        const Mat34 P2 = poseToP(cand);
        int count = 0;
        for (int k = 0; k < n; k++) {
            if (!rep.inlier_mask[k] || m[k].norm() < sin_loose) continue;
            const Vec3 X = triangulateDLT(P1, P2, b1[k], b2[k]);
            const Vec3 Xc2 = mul(cand.R, X) + cand.t;
            if (X.dot(b1[k]) > 0 && Xc2.dot(b2[k]) > 0) count++;
        }
        if (count > best) {
            best = count;
            pose = cand;
        }
    }
    // Then the rotation moves as far as the prior allows, from the loose fit
    // and from the caller's start; the strict radius decides between them.
    g.loose_inliers = rep.num_inliers;
    const double thr2 = strict * strict;
    auto countStrict = [&](const Pose& p) {
        const Mat3 E = mul(crossMatrix(p.t), p.R);
        int c = 0;
        for (int k = 0; k < n; k++) c += sampsonSqBearing(E, b1[k], b2[k]) < thr2 ? 1 : 0;
        return c;
    };
    if (opt.rot_sigma > 0) {
        refineNearRotation(b1, b2, rep.inlier_mask, R, opt.rot_sigma, strict, pose);
        if (opt.start) {
            Pose alt = *opt.start;
            refineNearRotation(b1, b2, rep.inlier_mask, R, opt.rot_sigma, strict, alt);
            if (countStrict(alt) > countStrict(pose)) pose = alt;
        }
    }
    {
        const Mat3 D = mul(pose.R, transpose(R));
        const double tr = std::max(-1.0, std::min(1.0, (D[0] + D[4] + D[8] - 1.0) * 0.5));
        g.moved_deg = std::acos(tr) * 180.0 / M_PI;
    }
    const Mat3 E = mul(crossMatrix(pose.t), pose.R);
    const double sin_strict = std::sin(strict);
    g.inlier_mask.assign(n, 0);
    for (int k = 0; k < n; k++) {
        rot_only[k] = mul(pose.R, b1[k]).cross(b2[k]).norm() < sin_strict ? 1 : 0;
        g.rotation_only += rot_only[k];
        if (sampsonSqBearing(E, b1[k], b2[k]) < thr2) {
            g.inlier_mask[k] = 1;
            g.num_inliers++;
        }
    }
    if (g.num_inliers < opt.min_num_inliers) {
        g = KnownRotationGeometry{};
        return g;
    }
    g.ok = true;
    g.panoramic = g.rotation_only >= opt.max_rotation_only_ratio * g.num_inliers;
    g.pose = pose;
    return g;
}

// PnP with the rotation given: b x (R X + t) = 0 is linear in t, so two
// correspondences solve it and the RANSAC needs no P3P. The result is judged
// on the same residual `ransacPnP` uses, so the two are comparable.
inline PnPResult ransacPnPKnownRotation(const std::vector<Vec3>& X, const std::vector<Vec3>& b,
                                        const Mat3& R, double focal, double max_error_px = 4.0,
                                        unsigned seed = 0, int max_trials = 1000) {
    PnPResult out;
    const int n = (int)X.size();
    if (n < 2) return out;
    std::vector<Vec3> Y(n);
    for (int k = 0; k < n; k++) Y[k] = mul(R, X[k]);
    // Normal equations of [b]x t = -[b]x Y over the sample.
    auto solve = [&](const std::vector<int>& s) {
        std::vector<Pose> out2;
        Mat3 N{};
        Vec3 rhs{0, 0, 0};
        for (int k : s) {
            const Mat3 K = crossMatrix(b[k]);
            const Mat3 KtK = mul(transpose(K), K);
            for (int i = 0; i < 9; i++) N[i] += KtK[i];
            rhs = rhs - mul(KtK, Y[k]);
        }
        if (std::fabs(det3(N)) < 1e-18) return out2;
        const Pose p{R, mul(inverse3(N), rhs)};
        for (int k : s)
            if ((Y[k] + p.t).dot(b[k]) <= 0) return out2;   // behind the camera
        out2.push_back(p);
        return out2;
    };
    auto res = [&](const Pose& p, int k) { return pnpResidualSqAt(Y[k] + p.t, b[k]); };
    RansacOptions ro;
    ro.max_error = max_error_px / focal;
    ro.seed = seed;
    ro.min_num_trials = std::min(100, max_trials);
    ro.max_num_trials = max_trials;
    RansacReport<Pose> rep = loransac<Pose>(n, 2, solve, solve, res, ro);
    out.pose = rep.model;
    out.inlier_mask = rep.inlier_mask;
    out.num_inliers = rep.num_inliers;
    out.success = rep.success;
    return out;
}

// The same for a rig frame: every lens's correspondences at once, the frame
// rotation given, the frame translation from two of them
// (RigPnPMember / ransacRigPnP in AbsolutePose.h are the free-rotation form).
inline RigPnPResult ransacRigPnPKnownRotation(const std::vector<RigPnPMember>& members,
                                              const Mat3& R_frame, unsigned seed = 0,
                                              int max_trials = 1000) {
    RigPnPResult out;
    struct Entry {
        int m, i;
        Vec3 Y;   // R_m R_f X + t_m: the camera-frame point less R_m t_f
    };
    std::vector<Entry> pool;
    std::vector<double> inv2(members.size(), 0.0);
    for (size_t m = 0; m < members.size(); m++) {
        const RigPnPMember& mem = members[m];
        if (!(mem.max_error > 0)) continue;
        inv2[m] = 1.0 / (mem.max_error * mem.max_error);
        const Mat3 RmRf = mul(mem.cam_from_rig.R, R_frame);
        for (size_t i = 0; i < mem.X->size(); i++)
            pool.push_back({(int)m, (int)i, mul(RmRf, (*mem.X)[i]) + mem.cam_from_rig.t});
    }
    if (pool.size() < 2) return out;
    auto solve = [&](const std::vector<int>& s) {
        std::vector<Pose> got;
        Mat3 N{};
        Vec3 rhs{0, 0, 0};
        for (int k : s) {
            const Entry& e = pool[(size_t)k];
            const Mat3 KR = mul(crossMatrix((*members[(size_t)e.m].b)[(size_t)e.i]),
                                members[(size_t)e.m].cam_from_rig.R);
            const Mat3 NtN = mul(transpose(KR), KR);
            for (int i = 0; i < 9; i++) N[i] += NtN[i] * inv2[(size_t)e.m];
            rhs = rhs - mul(mul(transpose(KR), crossMatrix((*members[(size_t)e.m].b)[(size_t)e.i])), e.Y) *
                            inv2[(size_t)e.m];
        }
        if (std::fabs(det3(N)) < 1e-18) return got;
        const Vec3 tf = mul(inverse3(N), rhs);
        for (int k : s) {
            const Entry& e = pool[(size_t)k];
            const Vec3 pc = e.Y + mul(members[(size_t)e.m].cam_from_rig.R, tf);
            if (pc.dot((*members[(size_t)e.m].b)[(size_t)e.i]) <= 0) return got;
        }
        got.push_back(Pose{R_frame, tf});
        return got;
    };
    auto res = [&](const Pose& F, int k) {
        const Entry& e = pool[(size_t)k];
        const RigPnPMember& mem = members[(size_t)e.m];
        return pnpResidualSqAt(e.Y + mul(mem.cam_from_rig.R, F.t), (*mem.b)[(size_t)e.i]) *
               inv2[(size_t)e.m];
    };
    RansacOptions ro;
    ro.max_error = 1.0;
    ro.seed = seed;
    ro.min_num_trials = std::min(100, max_trials);
    ro.max_num_trials = max_trials;
    RansacReport<Pose> rep = loransac<Pose>((int)pool.size(), 2, solve, solve, res, ro);
    if (!rep.success) return out;
    out.rig_from_world = rep.model;
    out.num_inliers = rep.num_inliers;
    out.success = true;
    return out;
}

}  // namespace sfm
