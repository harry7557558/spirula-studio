// Generalized P3P: a camera rig's pose from three 2D-3D correspondences its
// members contribute between them, each ray free to start at its own lens
// (docs/notes/sfm-rig-constraints.md). Rays are given in the rig's frame, the
// world point of ray i sitting at o_i + lambda_i * d_i, and the poses returned
// are rig_from_world. Three rays through one point is P3P, which this defers to
// -- a rig whose members share an optical centre has no baseline to use.
#pragma once

#include <array>
#include <cmath>
#include <vector>

#include "sfm/geometry/Essential.h"  // Pose, nearestRotation
#include "sfm/geometry/LinAlg.h"
#include "sfm/geometry/P3P.h"
#include "sfm/geometry/PolynomialRoots.h"

namespace sfm {

// One ray of a generalized camera, in the rig's frame.
struct RigRay {
    Vec3 o;  // the member's centre
    Vec3 d;  // unit direction
};

namespace gp3p_detail {

// lambda_i^2 + lambda_j^2 - 2c li lj + 2p li - 2q lj + e = 0, one per point
// pair: the pair's distance in the world, seen along two rays.
struct PairEq {
    double c = 0, p = 0, q = 0, e = 0;
};

// Dense coefficients of a polynomial in the hidden depth, low order first.
// Degree 8 is the whole elimination, so the array never grows.
struct Poly {
    double c[9] = {0};
    int n = 0;
};

inline Poly polyC(double a0) {
    Poly p;
    p.c[0] = a0;
    return p;
}
inline Poly polyL(double a0, double a1) {
    Poly p;
    p.c[0] = a0;
    p.c[1] = a1;
    p.n = 1;
    return p;
}
inline Poly polyQ(double a0, double a1, double a2) {
    Poly p;
    p.c[0] = a0;
    p.c[1] = a1;
    p.c[2] = a2;
    p.n = 2;
    return p;
}

inline Poly mulP(const Poly& a, const Poly& b) {
    Poly r;
    r.n = a.n + b.n;
    for (int i = 0; i <= a.n; i++)
        for (int j = 0; j <= b.n; j++) r.c[i + j] += a.c[i] * b.c[j];
    return r;
}
inline Poly addP(const Poly& a, const Poly& b) {
    Poly r;
    r.n = a.n > b.n ? a.n : b.n;
    for (int i = 0; i <= r.n; i++) r.c[i] = (i <= a.n ? a.c[i] : 0) + (i <= b.n ? b.c[i] : 0);
    return r;
}
inline Poly subP(const Poly& a, const Poly& b) {
    Poly r;
    r.n = a.n > b.n ? a.n : b.n;
    for (int i = 0; i <= r.n; i++) r.c[i] = (i <= a.n ? a.c[i] : 0) - (i <= b.n ? b.c[i] : 0);
    return r;
}

inline int realRoots(const Poly& p, double* out, double imag_tol = 1e-4) {
    return realPolynomialRoots(p.c, p.n, out, imag_tol);
}

inline double eqResidual(const PairEq& q, double li, double lj) {
    return li * li + lj * lj - 2 * q.c * li * lj + 2 * q.p * li - 2 * q.q * lj + q.e;
}

// Newton on the three distance equations, returning the residual it reached
// relative to the depths' size: the elimination's root is only as exact as an
// octic with near-coincident roots allows, and this is what makes it a pose.
inline double polishLambdas(const PairEq eq[3], double l[3]) {
    static const int I[3] = {0, 0, 1}, J[3] = {1, 2, 2};
    double r[3] = {0, 0, 0};
    for (int it = 0; it < 8; it++) {
        double sum = 0;
        for (int k = 0; k < 3; k++) {
            r[k] = eqResidual(eq[k], l[I[k]], l[J[k]]);
            sum += std::fabs(r[k]);
        }
        const double mag = 1.0 + l[0] * l[0] + l[1] * l[1] + l[2] * l[2];
        if (sum < 1e-13 * mag) return sum / mag;
        Mat3 Jm{};
        for (int k = 0; k < 3; k++) {
            Jm[3 * k + I[k]] = 2 * l[I[k]] - 2 * eq[k].c * l[J[k]] + 2 * eq[k].p;
            Jm[3 * k + J[k]] = 2 * l[J[k]] - 2 * eq[k].c * l[I[k]] - 2 * eq[k].q;
        }
        bool ok = false;
        const Mat3 Ji = inverse3(Jm, &ok);
        if (!ok) break;
        const Vec3 d = mul(Ji, Vec3{r[0], r[1], r[2]});
        l[0] -= d.x;
        l[1] -= d.y;
        l[2] -= d.z;
    }
    double sum = 0;
    for (int k = 0; k < 3; k++) sum += std::fabs(eqResidual(eq[k], l[I[k]], l[J[k]]));
    return sum / (1.0 + l[0] * l[0] + l[1] * l[1] + l[2] * l[2]);
}

}  // namespace gp3p_detail

inline std::vector<Pose> gp3p(const std::array<RigRay, 3>& rays, const std::array<Vec3, 3>& X) {
    using namespace gp3p_detail;
    std::vector<Pose> out;

    const Vec3 W01 = X[0] - X[1], W02 = X[0] - X[2], W12 = X[1] - X[2];
    const double scale = (W01.norm() + W02.norm() + W12.norm()) / 3.0;
    if (!(scale > 0) || !std::isfinite(scale)) return out;
    const double inv = 1.0 / scale;

    Vec3 o[3], d[3];
    double baseline = 0;
    for (int i = 0; i < 3; i++) {
        o[i] = rays[i].o * inv;
        d[i] = rays[i].d.normalized();
    }
    for (int i = 0; i < 3; i++)
        for (int j = i + 1; j < 3; j++) baseline = std::max(baseline, (o[i] - o[j]).norm());

    // Concurrent rays carry no baseline: the generalized problem is P3P about
    // their shared centre, and the octic below is degenerate there.
    if (baseline < 1e-5) {
        const Vec3 centre = (rays[0].o + rays[1].o + rays[2].o) * (1.0 / 3.0);
        out = p3p({d[0], d[1], d[2]}, X);
        for (Pose& p : out) p.t = p.t + centre;
        return out;
    }

    static const int I[3] = {0, 0, 1}, J[3] = {1, 2, 2};
    const double dist[3] = {W01.norm() * inv, W02.norm() * inv, W12.norm() * inv};
    PairEq eq[3];
    for (int k = 0; k < 3; k++) {
        const Vec3 u = o[I[k]] - o[J[k]];
        eq[k].c = d[I[k]].dot(d[J[k]]);
        eq[k].p = u.dot(d[I[k]]);
        eq[k].q = u.dot(d[J[k]]);
        eq[k].e = u.dot(u) - dist[k] * dist[k];
    }

    // Hiding lambda_0: eq[0] and eq[1] are monic quadratics in lambda_1 and
    // lambda_2, and reducing eq[2] by them leaves a bilinear relation, so
    // lambda_2 is a ratio in lambda_1 and one resultant ends the elimination.
    const Poly b1 = polyL(-2 * eq[0].q, -2 * eq[0].c);
    const Poly c1 = polyQ(eq[0].e, 2 * eq[0].p, 1);
    const Poly b2 = polyL(-2 * eq[1].q, -2 * eq[1].c);
    const Poly c2 = polyQ(eq[1].e, 2 * eq[1].p, 1);
    const Poly A = polyC(-2 * eq[2].c);
    const Poly B = polyL(2 * eq[2].p + 2 * eq[0].q, 2 * eq[0].c);
    const Poly C = polyL(-2 * eq[2].q + 2 * eq[1].q, 2 * eq[1].c);
    const Poly D = polyQ(eq[2].e - eq[0].e - eq[1].e, -2 * (eq[0].p + eq[1].p), -2);

    // (B l1 + D)^2 - b2 (B l1 + D)(A l1 + C) + c2 (A l1 + C)^2, a quadratic in
    // lambda_1 that eq[0] must share a root with.
    const Poly alpha = addP(subP(mulP(B, B), mulP(mulP(b2, A), B)), mulP(c2, mulP(A, A)));
    const Poly beta = addP(subP(mulP(polyC(2), mulP(B, D)),
                                mulP(b2, addP(mulP(B, C), mulP(D, A)))),
                           mulP(polyC(2), mulP(c2, mulP(A, C))));
    const Poly gamma = addP(subP(mulP(D, D), mulP(mulP(b2, D), C)), mulP(c2, mulP(C, C)));
    const Poly r0 = subP(mulP(alpha, c1), gamma);
    const Poly r1 = subP(mulP(alpha, b1), beta);
    const Poly r2 = subP(mulP(beta, c1), mulP(gamma, b1));
    const Poly res = subP(mulP(r0, r0), mulP(r1, r2));

    const Vec3 w1 = W01 * inv, w2 = W02 * inv, w3 = w1.cross(w2);
    bool invertible = false;
    const Mat3 Wi = inverse3({w1.x, w2.x, w3.x, w1.y, w2.y, w3.y, w1.z, w2.z, w3.z}, &invertible);
    if (!invertible) return out;

    double roots[9];
    const int nr = realRoots(res, roots);
    for (int k = 0; k < nr; k++) {
        const double l0 = roots[k];
        if (!(l0 > 1e-9) || !std::isfinite(l0)) continue;
        const double bb1 = b1.c[0] + b1.c[1] * l0, cc1 = c1.c[0] + l0 * (c1.c[1] + l0);
        const double bb2 = b2.c[0] + b2.c[1] * l0, cc2 = c2.c[0] + l0 * (c2.c[1] + l0);
        // A double root sits where the discriminant does: rounding puts it just
        // below zero as often as just above, and Newton recovers either way.
        const double disc1 = bb1 * bb1 - 4 * cc1, disc2 = bb2 * bb2 - 4 * cc2;
        if (disc1 < -1e-8 || disc2 < -1e-8) continue;
        const double s1 = std::sqrt(std::max(disc1, 0.0)), s2 = std::sqrt(std::max(disc2, 0.0));
        for (int u = 0; u < 2; u++) {
            const double l1 = 0.5 * (-bb1 + (u ? -s1 : s1));
            if (!(l1 > 1e-9)) continue;
            for (int v = 0; v < 2; v++) {
                double lam[3] = {l0, l1, 0.5 * (-bb2 + (v ? -s2 : s2))};
                if (!(lam[2] > 1e-9)) continue;
                if (std::fabs(eqResidual(eq[2], lam[1], lam[2])) >
                    0.05 * (1 + lam[1] * lam[1] + lam[2] * lam[2]))
                    continue;
                if (polishLambdas(eq, lam) > 1e-10) continue;
                if (!(lam[0] > 0 && lam[1] > 0 && lam[2] > 0)) continue;

                const Vec3 P0 = o[0] + d[0] * lam[0];
                const Vec3 v1 = P0 - (o[1] + d[1] * lam[1]);
                const Vec3 v2 = P0 - (o[2] + d[2] * lam[2]);
                const Vec3 v3 = v1.cross(v2);
                const Mat3 R = nearestRotation(
                    mul(Mat3{v1.x, v2.x, v3.x, v1.y, v2.y, v3.y, v1.z, v2.z, v3.z}, Wi));
                const Pose pose{R, P0 * scale - mul(R, X[0])};
                bool dup = false;
                for (const Pose& q : out) {
                    double dr = 0;
                    for (int i = 0; i < 9; i++) dr += std::fabs(q.R[i] - R[i]);
                    dup = dup || (dr < 1e-8 && (q.t - pose.t).norm() < 1e-8 * scale);
                }
                if (!dup) out.push_back(pose);
            }
        }
    }
    return out;
}

}  // namespace sfm
