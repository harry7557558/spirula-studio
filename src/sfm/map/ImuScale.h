// Metric scale from the accelerometer: the velocity-free constraint over two
// consecutive pre-integrated intervals (Mur-Artal and Tardos 2017, sec. IV),
// s L = Q + g Gs, linear in the scale, the biases and gravity. Shared by the
// gauge fit that runs on a finished model (SensorGauge.h) and the priors a
// bundle adjustment takes while the model is built (SensorPriors.h).
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "sfm/core/Preintegration.h"
#include "sfm/map/ImuExtrinsic.h"

namespace sfm {

// Consecutive frames j < k of one group, with the IMU integrated between them.
struct ImuPair {
    int j = 0, k = 0;    // frame indices
    Preintegration P;
    Mat3 B;              // attitude-only captures: R_i(j) <- i(k)
    bool has_preint = false;
};

struct ImuTriple {
    int j = 0, k = 0, l = 0;
    int p1 = 0, p2 = 0;   // pair indices
};

// s * L = Q + Gs * g_w in the model frame; `Ja`, `Jg` are d Q / d bias.
struct ImuTripleTerms {
    Vec3 L;
    double Gs = 0;
    Vec3 Q;
    Mat3 Ja, Jg;
};

// Two pre-integrated pairs sharing a frame make one triple.
inline std::vector<ImuTriple> imuTriples(const std::vector<ImuPair>& pairs) {
    std::vector<ImuTriple> tr;
    for (size_t p = 1; p < pairs.size(); p++) {
        if (pairs[p - 1].k != pairs[p].j) continue;
        if (!pairs[p - 1].has_preint || !pairs[p].has_preint) continue;
        tr.push_back({pairs[p - 1].j, pairs[p].j, pairs[p].k, (int)p - 1, (int)p});
    }
    return tr;
}

// `X` is camera <- IMU; `lever` where the IMU sits from the lens (camera
// frame, metres).
inline ImuTripleTerms imuTripleTerms(const std::vector<SensorFrame>& frames,
                                     const std::vector<ImuPair>& pairs, const ImuTriple& t,
                                     const Mat3& X, const Vec3& bg, const Vec3& ba,
                                     const Vec3& lever) {
    const Preintegration& P1 = pairs[(size_t)t.p1].P;
    const Preintegration& P2 = pairs[(size_t)t.p2].P;
    const SensorFrame& fj = frames[(size_t)t.j];
    const SensorFrame& fk = frames[(size_t)t.k];
    const SensorFrame& fl = frames[(size_t)t.l];
    const double d1 = P1.dt, d2 = P2.dt;
    ImuTripleTerms T;
    T.L = (fl.c - fk.c) * d1 - (fk.c - fj.c) * d2;
    T.Gs = 0.5 * d1 * d2 * (d1 + d2);
    const Mat3 Rj = mul(transpose(fj.R), X), Rk = mul(transpose(fk.R), X);
    T.Q = mul(Rj, P1.velocity(bg, ba) * (d1 * d2) - P1.position(bg, ba) * d2) +
          mul(Rk, P2.position(bg, ba) * d1);
    T.Ja = mat3Add(mul(Rj, mat3Add(mat3Scale(P1.dv_dba, d1 * d2), mat3Scale(P1.dp_dba, -d2))),
                   mul(Rk, mat3Scale(P2.dp_dba, d1)));
    T.Jg = mat3Add(mul(Rj, mat3Add(mat3Scale(P1.dv_dbg, d1 * d2), mat3Scale(P1.dp_dbg, -d2))),
                   mul(Rk, mat3Scale(P2.dp_dbg, d1)));
    const Vec3 arm = (mul(transpose(fl.R), lever) - mul(transpose(fk.R), lever)) * d1 -
                     (mul(transpose(fk.R), lever) - mul(transpose(fj.R), lever)) * d2;
    T.Q = T.Q - arm;
    return T;
}

// Variance of one component of a triple's pre-integrated position for white
// accelerometer noise of density q: (q^2/3) d1^2 d2^2 (d1 + d2).
inline double imuTripleNoise(double q, const std::vector<ImuPair>& pairs, const ImuTriple& t) {
    const double d1 = pairs[(size_t)t.p1].P.dt, d2 = pairs[(size_t)t.p2].P.dt;
    return q * q * d1 * d1 * d2 * d2 * (d1 + d2) / 3.0;
}

// x = pseudo-inverse solve of the normal equations N x = b (n <= 16).
inline std::vector<double> solveNormalPinv(std::vector<double> N, const std::vector<double>& b,
                                           int n) {
    std::vector<double> ev, V, x((size_t)n, 0.0);
    jacobiEigenSymmetric(N, n, ev, V);
    double emax = 0;
    for (double e : ev) emax = std::max(emax, e);
    for (int i = 0; i < n; i++) {
        if (!(ev[(size_t)i] > 1e-12 * emax)) continue;
        double d = 0;
        for (int r = 0; r < n; r++) d += V[(size_t)r * n + i] * b[(size_t)r];
        for (int r = 0; r < n; r++) x[(size_t)r] += V[(size_t)r * n + i] * d / ev[(size_t)i];
    }
    return x;
}

// One lens's share of a scale fit: its frames, pairs and triples, its
// extrinsic and its accelerometer's noise density.
struct ImuGroupView {
    const std::vector<SensorFrame>* frames = nullptr;
    const std::vector<ImuPair>* pairs = nullptr;
    const std::vector<ImuTriple>* triples = nullptr;
    Mat3 X = mat3Identity();
    double noise = 0.02;
};

struct ImuScaleFit {
    double s = 0, sigma = 0;   // metres per model unit; sigma absolute
    int triples = 0, inliers = 0;
    Vec3 ba, bg;
    double g_norm = 0, g_angle_deg = 0;   // gravity refitted free, as the check
    double res_scale = 0;                 // robust residual scale, model units
    std::vector<double> residual;         // per triple over the groups, model units
};

// Solved for the INVERSE scale with the centres as the response: noise in a
// regressor attenuates a slope (a 360 rig's views scatter 5 cm about their
// frame and came out 10-50% low), in the response it only widens it.
inline ImuScaleFit solveImuScale(const std::vector<ImuGroupView>& groups, const Vec3& up_w) {
    ImuScaleFit fit;
    std::vector<std::pair<size_t, size_t>> ids;   // (group, triple)
    for (size_t g = 0; g < groups.size(); g++)
        for (size_t i = 0; i < groups[g].triples->size(); i++) ids.push_back({g, i});
    fit.triples = (int)ids.size();
    if (ids.size() < 5) return fit;

    // Gyro bias from the rotation pairs alone.
    Vec3 bg{0, 0, 0};
    {
        std::vector<double> N(9, 0.0), rhs(3, 0.0);
        for (int c = 0; c < 3; c++) N[(size_t)c * 4] += 1.0 / (0.02 * 0.02);
        for (const ImuGroupView& gv : groups)
            for (const ImuPair& pm : *gv.pairs) {
                if (!pm.has_preint) continue;
                const SensorFrame& fj = (*gv.frames)[(size_t)pm.j];
                const SensorFrame& fk = (*gv.frames)[(size_t)pm.k];
                const Mat3 A = mul(fj.R, transpose(fk.R));
                const Vec3 e = so3Log(mul(transpose(pm.P.dR), mul(transpose(gv.X), mul(A, gv.X))));
                const double sr = 0.5 * M_PI / 180.0;
                for (int r = 0; r < 3; r++) {
                    const double* row = &pm.P.dR_dbg[(size_t)r * 3];
                    for (int a = 0; a < 3; a++) {
                        rhs[(size_t)a] += row[a] * (&e.x)[r] / (sr * sr);
                        for (int c = 0; c < 3; c++) N[(size_t)a * 3 + c] += row[a] * row[c] / (sr * sr);
                    }
                }
            }
        const std::vector<double> x = solveNormalPinv(N, rhs, 3);
        bg = {x[0], x[1], x[2]};
    }

    auto terms = [&](size_t i) {
        const ImuGroupView& gv = groups[ids[i].first];
        return imuTripleTerms(*gv.frames, *gv.pairs, (*gv.triples)[ids[i].second], gv.X, bg,
                              {0, 0, 0}, {0, 0, 0});
    };
    std::vector<double> w(ids.size(), 1.0);
    double k = 1.0, var_k = 0;
    Vec3 bak{0, 0, 0};   // accel bias times k
    auto solve = [&](bool with_g, Vec3& g_out) {
        const int n = with_g ? 7 : 4;
        std::vector<double> N((size_t)n * n, 0.0), rhs((size_t)n, 0.0);
        auto add_row = [&](const double* row, double b, double wi) {
            for (int a = 0; a < n; a++) {
                rhs[(size_t)a] += wi * row[a] * b;
                for (int c = 0; c < n; c++) N[(size_t)a * n + c] += wi * row[a] * row[c];
            }
        };
        double noise_n00 = 0;
        for (size_t i = 0; i < ids.size(); i++) {
            const ImuTripleTerms T = terms(i);
            const Vec3 pred = T.Q + up_w * (T.Gs * -9.81);
            for (int r = 0; r < 3; r++) {
                double row[7] = {0, 0, 0, 0, 0, 0, 0};
                row[0] = with_g ? (&T.Q.x)[r] : (&pred.x)[r];
                for (int c = 0; c < 3; c++) row[1 + c] = T.Ja[3 * r + c];
                if (with_g) row[4 + r] = T.Gs;
                add_row(row, (&T.L.x)[r], w[i]);
            }
            const ImuGroupView& gv = groups[ids[i].first];
            noise_n00 += w[i] * 3.0 * imuTripleNoise(gv.noise, *gv.pairs, (*gv.triples)[ids[i].second]);
        }
        // The pre-integrated regressor carries the accelerometer's own noise,
        // which attenuates k -- 10% on the DJI's 30 Hz stream over 1 s pairs.
        // Corrected least squares takes that variance back out.
        N[0] = std::max(N[0] - noise_n00, 0.5 * N[0]);
        const double prior = 0.2 * std::max(k, 0.05);   // 0.2 m/s^2 on the bias itself
        for (int c = 0; c < 3; c++) {
            double row[7] = {0, 0, 0, 0, 0, 0, 0};
            row[1 + c] = 1;
            add_row(row, 0.0, 1.0 / (prior * prior));
        }
        const std::vector<double> x = solveNormalPinv(N, rhs, n);
        k = x[0];
        bak = {x[1], x[2], x[3]};
        if (with_g) g_out = {x[4], x[5], x[6]};
        std::vector<double> Ncopy = N, ev, V;
        jacobiEigenSymmetric(Ncopy, n, ev, V);
        double var = 0;
        for (int i = 0; i < n; i++)
            if (ev[(size_t)i] > 1e-12) var += V[(size_t)i] * V[(size_t)i] / ev[(size_t)i];
        return var;
    };
    Vec3 g_unused;
    double var_r = 1;
    fit.residual.assign(ids.size(), 0.0);
    for (int round = 0; round < 6; round++) {
        var_k = solve(false, g_unused);
        for (size_t i = 0; i < ids.size(); i++) {
            const ImuTripleTerms T = terms(i);
            const Vec3 pred = (T.Q + up_w * (T.Gs * -9.81)) * k + mul(T.Ja, bak);
            fit.residual[i] = (T.L - pred).norm();
        }
        fit.res_scale = std::max(extrinsic_detail::medianOf(fit.residual), 1e-9);
        double ss = 0, sw = 0;
        fit.inliers = 0;
        for (size_t i = 0; i < ids.size(); i++) {
            const double x = fit.residual[i] / fit.res_scale;
            w[i] = x <= 1.345 ? 1.0 : 1.345 / x;
            if (fit.residual[i] < 3 * fit.res_scale) fit.inliers++;
            ss += w[i] * fit.residual[i] * fit.residual[i];
            sw += w[i];
        }
        var_r = sw > 3 ? ss / (3 * sw - 4) : 1.0;
    }
    if (!(k > 0) && !(k < 0)) return fit;
    fit.s = 1.0 / k;
    fit.sigma = std::sqrt(var_k * var_r) / (k * k);
    fit.ba = bak * (1.0 / k);
    fit.bg = bg;
    // Gravity check: the same solve with the gravity vector free.
    Vec3 gk;
    const double k_fixed = k;
    const Vec3 bak_fixed = bak;
    solve(true, gk);
    const Vec3 gv = gk * (1.0 / k);
    fit.g_norm = gv.norm();
    fit.g_angle_deg = gv.norm() > 0 ? extrinsic_detail::angleDeg(gv, up_w * -1.0) : 0;
    k = k_fixed;
    bak = bak_fixed;
    return fit;
}

}  // namespace sfm
