// The host bundle adjustment (sfm/ba/SolverCpu.h) against independent
// references, on synthetic problems: the dense SPD solver against a textbook
// Cholesky, the analytic Jacobians against central differences, and the whole
// Schur assembly plus parameter update against the unreduced normal equations
// written out in full (dense U/V/W blocks, no packing and no task splitting --
// nothing the solver's own code path shares).
//
//   sfm_ba_cpu_test [--quick]
//
// Prints PASS/FAIL per case and returns 0/1. Needs no GPU. See docs/testing.md.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "sfm/ba/CpuCamera.h"
#include "sfm/ba/CpuDense.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/SolverCpu.h"
#include "sfm/core/Pose.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"

namespace {
using namespace synth;

int g_fail = 0;

void report(const char* name, double err, double tol) {
    const bool ok = err < tol && std::isfinite(err);
    printf("%-34s err %.3e (tol %.0e)  %s\n", name, err, tol, ok ? "PASS" : "FAIL");
    if (!ok) g_fail++;
}

// ---------------------------------------------------------------------------
// dense SPD factor + solve
// ---------------------------------------------------------------------------

void testChol(uint32_t n) {
    std::mt19937 rng(1234);
    std::normal_distribution<double> gauss;
    std::vector<double> A((size_t)n * n, 0.0), b(n);
    {
        std::vector<double> M((size_t)n * n);
        for (double& v : M) v = gauss(rng);
        for (uint32_t i = 0; i < n; i++)
            for (uint32_t j = 0; j <= i; j++) {
                double s = 0;
                for (uint32_t k = 0; k < n; k++) s += M[(size_t)i * n + k] * M[(size_t)j * n + k];
                A[(size_t)i * n + j] = A[(size_t)j * n + i] = s + (i == j ? (double)n : 0.0);
            }
        for (double& v : b) v = gauss(rng);
    }

    bacpu::DenseSpd S;
    S.init(n);
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = 0; j <= i; j++) S.row(i)[j] = A[(size_t)i * n + j];
    std::vector<double> x = b;
    S.factorSolve(x.data(), bacpu::Pool::get(), bacpu::Pool::get().size());

    std::vector<double> L = A;  // textbook Cholesky, unblocked
    for (uint32_t j = 0; j < n; j++) {
        for (uint32_t k = 0; k < j; k++)
            for (uint32_t i = j; i < n; i++)
                L[(size_t)i * n + j] -= L[(size_t)i * n + k] * L[(size_t)j * n + k];
        double d = std::sqrt(L[(size_t)j * n + j]);
        for (uint32_t i = j; i < n; i++) L[(size_t)i * n + j] /= d;
    }
    std::vector<double> y = b;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < i; j++) y[i] -= L[(size_t)i * n + j] * y[j];
        y[i] /= L[(size_t)i * n + i];
    }
    for (int i = (int)n - 1; i >= 0; i--) {
        for (uint32_t j = i + 1; j < n; j++) y[i] -= L[(size_t)j * n + i] * y[j];
        y[i] /= L[(size_t)i * n + i];
    }

    double e = 0, s = 0;
    for (uint32_t i = 0; i < n; i++) {
        e = std::max(e, std::fabs(x[i] - y[i]));
        s = std::max(s, std::fabs(y[i]));
    }
    char name[64];
    snprintf(name, sizeof name, "chol n=%u", n);
    report(name, e / s, 1e-10);
}

// ---------------------------------------------------------------------------
// analytic Jacobian vs central differences
// ---------------------------------------------------------------------------

template <class M>
void testJacobianModel(const char* name, const double* intr0, std::mt19937& rng) {
    std::normal_distribution<double> gauss;
    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    double worst = 0;
    for (int trial = 0; trial < 20; trial++) {
        double pose[6], X[3], obs[2] = {0.3, -0.7};
        for (int i = 0; i < 3; i++) pose[i] = 0.3 * unit(rng);
        for (int i = 0; i < 3; i++) pose[3 + i] = 0.5 * unit(rng);
        for (int i = 0; i < 3; i++) X[i] = unit(rng);
        X[2] = 2.0 + unit(rng);  // in front of a +z camera
        double intr[M::kNumIntr];
        for (int i = 0; i < M::kNumIntr; i++) intr[i] = intr0[i] * (1.0 + 0.01 * unit(rng));

        double r[2], Jc[2 * (6 + M::kNumIntr)], Jp[6];
        bacpu::jacobian<M>(pose, intr, X, obs, r, Jc, Jp);

        // central differences over pose, intrinsics and the point
        const int DOF = 6 + M::kNumIntr;
        for (int k = 0; k < DOF + 3; k++) {
            double* p = k < 6 ? &pose[k] : k < DOF ? &intr[k - 6] : &X[k - DOF];
            const double h = 1e-6 * std::max(1.0, std::fabs(*p));
            const double keep = *p;
            double rp[2], rm[2];
            *p = keep + h;
            bacpu::residual<M>(pose, intr, X, obs, rp);
            *p = keep - h;
            bacpu::residual<M>(pose, intr, X, obs, rm);
            *p = keep;
            for (int row = 0; row < 2; row++) {
                const double num = (rp[row] - rm[row]) / (2 * h);
                const double ana = k < DOF ? Jc[row * DOF + k] : Jp[row * 3 + (k - DOF)];
                worst = std::max(worst, std::fabs(num - ana) /
                                            std::max(1.0, std::fabs(num) + std::fabs(ana)));
            }
        }
    }
    report(name, worst, 1e-6);
}

// The rig chain rule -- frame, then extrinsic -- against central differences.
template <class M>
void testJacobianRig(const char* name, const double* intr0, std::mt19937& rng) {
    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    double worst = 0;
    for (int trial = 0; trial < 20; trial++) {
        double pose[6], ext[6], X[3], obs[2] = {0.3, -0.7};
        for (int i = 0; i < 3; i++) pose[i] = 0.3 * unit(rng);
        for (int i = 0; i < 3; i++) pose[3 + i] = 0.5 * unit(rng);
        for (int i = 0; i < 3; i++) ext[i] = 0.2 * unit(rng);
        for (int i = 0; i < 3; i++) ext[3 + i] = 0.1 * unit(rng);
        for (int i = 0; i < 3; i++) X[i] = unit(rng);
        X[2] = 2.0 + unit(rng);
        double intr[M::kNumIntr];
        for (int i = 0; i < M::kNumIntr; i++) intr[i] = intr0[i] * (1.0 + 0.01 * unit(rng));

        constexpr int DOF = 12 + M::kNumIntr;
        double r[2], Jc[2 * DOF], Jp[6];
        bacpu::jacobianRig<M>(pose, ext, intr, X, obs, r, Jc, Jp);
        for (int k = 0; k < DOF + 3; k++) {
            double* p = k < 6 ? &pose[k] : k < 12 ? &ext[k - 6]
                      : k < DOF ? &intr[k - 12] : &X[k - DOF];
            const double h = 1e-6 * std::max(1.0, std::fabs(*p));
            const double keep = *p;
            double rp[2], rm[2];
            *p = keep + h;
            bacpu::residualRig<M>(pose, ext, intr, X, obs, rp);
            *p = keep - h;
            bacpu::residualRig<M>(pose, ext, intr, X, obs, rm);
            *p = keep;
            for (int row = 0; row < 2; row++) {
                const double num = (rp[row] - rm[row]) / (2 * h);
                const double ana = k < DOF ? Jc[row * DOF + k] : Jp[row * 3 + (k - DOF)];
                worst = std::max(worst, std::fabs(num - ana) /
                                            std::max(1.0, std::fabs(num) + std::fabs(ana)));
            }
        }
    }
    report(name, worst, 1e-6);
}

// At a zero angle-axis -- the identity every seed pair starts from -- the norm
// has no derivative. That belongs to the axis columns alone.
template <class M>
void testZeroRotation(const char* name, const double* intr) {
    double pose[6] = {0, 0, 0, 0.1, -0.2, 0.0};
    double X[3] = {0.3, -0.4, 3.0}, obs[2] = {5.0, -2.0};
    double r[2], Jc[2 * (6 + M::kNumIntr)], Jp[6];
    bacpu::jacobian<M>(pose, intr, X, obs, r, Jc, Jp);
    const int DOF = 6 + M::kNumIntr;
    bool ok = std::isfinite(r[0]) && std::isfinite(r[1]);
    for (int i = 0; i < 6; i++) ok = ok && std::isfinite(Jp[i]);
    for (int row = 0; row < 2; row++)
        for (int k = 3; k < DOF; k++) ok = ok && std::isfinite(Jc[row * DOF + k]);
    printf("%-34s %s\n", name, ok ? "                          PASS" : "  FAIL");
    if (!ok) g_fail++;
}

void testEquirectSeamResidual() {
    const double intr[2] = {640.0, 480.0};
    const double pose[6] = {0, 0, 0, 0, 0, 0};
    struct Case { double projected, observed, expected; };
    const Case cases[] = {
        {638.0, 2.0, -4.0},
        {2.0, 638.0, 4.0},
        {330.0, 20.0, 310.0},
        {20.0, 330.0, -310.0},
        {321.0, 0.0, -319.0},
        {0.0, 321.0, 319.0},
        {320.0, 0.0, 320.0},
        {0.0, 320.0, -320.0},
    };
    double worst = 0;
    for (const Case& c : cases) {
        const double theta = (c.projected / intr[0] - 0.5) * 2.0 * M_PI;
        const double point[3] = {std::sin(theta), 0.0, std::cos(theta)};
        const double obs[2] = {c.observed, 240.0};
        double r[2];
        bacpu::residual<bacpu::EquirectModel>(pose, intr, point, obs, r);
        worst = std::max(worst, std::fabs(r[0] - c.expected));
        worst = std::max(worst, std::fabs(r[1]));
    }
    report("equirect seam residual", worst, 1e-10);
}

void testEquirectSeamJacobian() {
    const double intr0[2] = {640.0, 480.0};
    const double pose0[6] = {0.002, -0.003, 0.001, 0.001, -0.002, 0.003};
    struct Case { double projected, observed; };
    const Case cases[] = {{638.0, 2.0}, {2.0, 638.0}};
    double worst = 0;
    for (const Case& c : cases) {
        const double theta = (c.projected / intr0[0] - 0.5) * 2.0 * M_PI;
        double X[3] = {std::sin(theta), 0.05, std::cos(theta)};
        double pose[6], intr[2];
        std::copy(pose0, pose0 + 6, pose);
        std::copy(intr0, intr0 + 2, intr);
        const double obs[2] = {c.observed, 240.0};
        double r[2], Jc[16], Jp[6];
        bacpu::jacobian<bacpu::EquirectModel>(pose, intr, X, obs, r, Jc, Jp);
        for (int k = 0; k < 11; k++) {
            double* p = k < 6 ? &pose[k] : k < 8 ? &intr[k - 6] : &X[k - 8];
            const double h = 1e-6 * std::max(1.0, std::fabs(*p));
            const double keep = *p;
            double rp[2], rm[2];
            *p = keep + h;
            bacpu::residual<bacpu::EquirectModel>(pose, intr, X, obs, rp);
            *p = keep - h;
            bacpu::residual<bacpu::EquirectModel>(pose, intr, X, obs, rm);
            *p = keep;
            for (int row = 0; row < 2; row++) {
                const double num = (rp[row] - rm[row]) / (2.0 * h);
                const double ana = k < 8 ? Jc[8 * row + k] : Jp[3 * row + k - 8];
                worst = std::max(worst, std::fabs(num - ana) /
                                            std::max(1.0, std::fabs(num) + std::fabs(ana)));
            }
        }
    }
    report("equirect seam jacobian", worst, 1e-6);
}

// ---------------------------------------------------------------------------
// the unreduced normal equations, written out in full
// ---------------------------------------------------------------------------

struct Reference {
    std::vector<double> S, g;   // n x n (full), n
    std::vector<double> dU, dP; // camera step, 3 per point
};

Reference referenceSolve(const BAProblem& P, double lambda, double lossParam,
                         const std::string& loss) {
    const uint32_t n = P.n_dim;
    Reference R;
    R.S.assign((size_t)n * n, 0.0);
    R.g.assign(n, 0.0);
    std::vector<double> V(9 * (size_t)P.num_points, 0.0), bp(3 * (size_t)P.num_points, 0.0);
    std::vector<double> Wp((size_t)P.num_points * n * 3, 0.0);

    bacpu::withLoss(loss, [&]([[maybe_unused]] auto L) {  // only decltype(L) is read
        for (uint32_t o = 0; o < P.num_obs; o++) {
            const uint32_t img = P.obs_image[o], pt = P.obs_point[o];
            const BAProblem::Group& gr = P.groups[P.image_group[img]];
            double Jc[2 * kMaxCamDof] = {}, Jp[6], r[2];
            uint32_t cols[kMaxCamDof];
            const uint32_t dof = imageColumns(P, img, cols);
            const uint32_t member = P.image_member[img];
            const uint32_t ne = P.memberFree(img);
            const double* pose = &P.poses[6 * (size_t)P.image_frame[img]];
            bacpu::withModel(gr.model, [&](auto M) {
                using MT = decltype(M);
                const int NE = member == kNoMember ? 0 : 6;
                const int DOF = 6 + NE + MT::kNumIntr;
                double jc[2 * (12 + MT::kNumIntr)];
                if (member == kNoMember)
                    bacpu::jacobian<MT>(pose, &P.intr[gr.intr_offset], &P.points[3 * (size_t)pt],
                                        &P.obs_xy[2 * (size_t)o], r, jc, Jp);
                else
                    bacpu::jacobianRig<MT>(pose, &P.exts[P.members[member].ext_offset],
                                           &P.intr[gr.intr_offset], &P.points[3 * (size_t)pt],
                                           &P.obs_xy[2 * (size_t)o], r, jc, Jp);
                const double sw = std::sqrt(decltype(L)::weight(r[0] * r[0] + r[1] * r[1],
                                                                lossParam));
                for (int row = 0; row < 2; row++) {
                    for (uint32_t a = 0; a < 6 + ne; a++)
                        Jc[row * kMaxCamDof + a] = jc[row * DOF + a] * sw;
                    for (uint32_t i = 0; i < gr.n_intr; i++)
                        Jc[row * kMaxCamDof + 6 + ne + i] = jc[row * DOF + 6 + NE + i] * sw;
                }
                for (int i = 0; i < 6; i++) Jp[i] *= sw;
                r[0] *= sw;
                r[1] *= sw;
            });
            for (uint32_t a = 0; a < dof; a++) {
                for (uint32_t b = 0; b < dof; b++)
                    R.S[(size_t)cols[a] * n + cols[b]] +=
                        Jc[a] * Jc[b] + Jc[kMaxCamDof + a] * Jc[kMaxCamDof + b];
                R.g[cols[a]] += Jc[a] * r[0] + Jc[kMaxCamDof + a] * r[1];
                for (int j = 0; j < 3; j++)
                    Wp[((size_t)pt * n + cols[a]) * 3 + j] +=
                        Jc[a] * Jp[j] + Jc[kMaxCamDof + a] * Jp[3 + j];
            }
            for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 3; j++)
                    V[9 * (size_t)pt + 3 * i + j] += Jp[i] * Jp[j] + Jp[3 + i] * Jp[3 + j];
                bp[3 * (size_t)pt + i] += Jp[i] * r[0] + Jp[3 + i] * r[1];
            }
        }
    });

    for (uint32_t i = 0; i < n; i++) R.S[(size_t)i * n + i] *= 1.0 + lambda;
    R.dP.assign(3 * (size_t)P.num_points, 0.0);
    std::vector<double> Vi(9 * (size_t)P.num_points);
    for (uint32_t p = 0; p < P.num_points; p++) {
        double m[9];
        memcpy(m, &V[9 * (size_t)p], sizeof m);
        m[0] *= 1.0 + lambda;
        m[4] *= 1.0 + lambda;
        m[8] *= 1.0 + lambda;
        const double c00 = m[4] * m[8] - m[5] * m[7], c01 = m[5] * m[6] - m[3] * m[8],
                     c02 = m[3] * m[7] - m[4] * m[6];
        const double inv = 1.0 / (m[0] * c00 + m[1] * c01 + m[2] * c02);
        double* w = &Vi[9 * (size_t)p];
        w[0] = c00 * inv;
        w[1] = (m[2] * m[7] - m[1] * m[8]) * inv;
        w[2] = (m[1] * m[5] - m[2] * m[4]) * inv;
        w[3] = c01 * inv;
        w[4] = (m[0] * m[8] - m[2] * m[6]) * inv;
        w[5] = (m[2] * m[3] - m[0] * m[5]) * inv;
        w[6] = c02 * inv;
        w[7] = (m[1] * m[6] - m[0] * m[7]) * inv;
        w[8] = (m[0] * m[4] - m[1] * m[3]) * inv;

        // S -= W V^-1 W^T, g -= W V^-1 bp
        std::vector<double> T((size_t)n * 3, 0.0);
        for (uint32_t a = 0; a < n; a++)
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    T[(size_t)a * 3 + i] += Wp[((size_t)p * n + a) * 3 + j] * w[3 * j + i];
        for (uint32_t a = 0; a < n; a++) {
            for (uint32_t b = 0; b < n; b++) {
                double s = 0;
                for (int j = 0; j < 3; j++)
                    s += T[(size_t)a * 3 + j] * Wp[((size_t)p * n + b) * 3 + j];
                R.S[(size_t)a * n + b] -= s;
            }
            double s = 0;
            for (int j = 0; j < 3; j++) s += T[(size_t)a * 3 + j] * bp[3 * (size_t)p + j];
            R.g[a] -= s;
        }
    }

    // dense solve of S dU = g
    std::vector<double> L = R.S;
    for (uint32_t j = 0; j < n; j++) {
        for (uint32_t k = 0; k < j; k++)
            for (uint32_t i = j; i < n; i++)
                L[(size_t)i * n + j] -= L[(size_t)i * n + k] * L[(size_t)j * n + k];
        const double d = std::sqrt(L[(size_t)j * n + j]);
        for (uint32_t i = j; i < n; i++) L[(size_t)i * n + j] /= d;
    }
    R.dU = R.g;
    for (uint32_t i = 0; i < n; i++) {
        for (uint32_t j = 0; j < i; j++) R.dU[i] -= L[(size_t)i * n + j] * R.dU[j];
        R.dU[i] /= L[(size_t)i * n + i];
    }
    for (int i = (int)n - 1; i >= 0; i--) {
        for (uint32_t j = i + 1; j < n; j++) R.dU[i] -= L[(size_t)j * n + i] * R.dU[j];
        R.dU[i] /= L[(size_t)i * n + i];
    }
    for (uint32_t p = 0; p < P.num_points; p++) {
        double t[3];
        for (int i = 0; i < 3; i++) {
            double s = bp[3 * (size_t)p + i];
            for (uint32_t a = 0; a < n; a++) s -= Wp[((size_t)p * n + a) * 3 + i] * R.dU[a];
            t[i] = s;
        }
        for (int i = 0; i < 3; i++)
            R.dP[3 * (size_t)p + i] = Vi[9 * (size_t)p + 3 * i] * t[0] +
                                      Vi[9 * (size_t)p + 3 * i + 1] * t[1] +
                                      Vi[9 * (size_t)p + 3 * i + 2] * t[2];
    }
    return R;
}

double relMax(const double* a, const double* b, size_t n) {
    double d = 0, s = 0;
    for (size_t i = 0; i < n; i++) {
        d = std::max(d, std::fabs(a[i] - b[i]));
        s = std::max(s, std::fabs(a[i]));
    }
    return d / std::max(s, 1e-300);
}

// One LM iteration of the solver against the reference: the assembled S and g,
// and the parameters the step leaves behind.
void testAgainstReference(uint32_t model, uint32_t groups, const char* loss, bool cg,
                          uint32_t nImg = 9, int nfree = -1, uint32_t rig = 0,
                          bool rig_free = true) {
    const double lambda = 1e-2;
    BAProblem P = makeProblem(model, nImg, 90, groups, 0.15, 7 * model + groups, nfree, rig,
                              rig_free);
    if (P.num_obs < 100) {
        printf("model %u: too few observations, skipped\n", model);
        return;
    }
    BAProblem P2 = P;

    SolverOptions opt;
    opt.real = RealCfg::CPU;
    opt.loss = loss;
    opt.loss_param = 1.5f;
    opt.init_damping = lambda;
    opt.max_iters = 1;
    opt.verbose = false;
    opt.solver = cg ? SolverSel::CG : SolverSel::Dense;
    opt.cg_tol = 1e-12;
    opt.cg_max_iters = 4000;
    opt.cg_fallback = CgFallback::Off;

    Reference R = referenceSolve(P, lambda, opt.loss_param, loss);

    char name[96];
    bacpu::Solver solver(P, opt);
    solver.init();
    if (!cg) {
        solver.assembleOnly(lambda);
        std::vector<double> S = solver.packedS(), g = solver.gradient();
        double dS = 0, sS = 0;
        for (uint32_t i = 0; i < P.n_dim; i++)
            for (uint32_t j = 0; j <= i; j++) {
                const double ref = R.S[(size_t)i * P.n_dim + j];
                dS = std::max(dS, std::fabs(S[(size_t)i * (i + 1) / 2 + j] - ref));
                sS = std::max(sS, std::fabs(ref));
            }
        snprintf(name, sizeof name, "S  model=%u groups=%u n=%u f=%d rig=%u%s", model, groups,
                 nImg, nfree, rig, rig && !rig_free ? " held" : "");
        report(name, dS / sS, 1e-10);
        snprintf(name, sizeof name, "g  model=%u groups=%u n=%u f=%d rig=%u%s", model, groups,
                 nImg, nfree, rig, rig && !rig_free ? " held" : "");
        report(name, relMax(g.data(), R.g.data(), P.n_dim), 1e-10);
    }

    solver.solve();
    double dmax = 0, smax = 0;
    for (uint32_t i = 0; i < P.pose_dim; i++) {
        dmax = std::max(dmax, std::fabs(P.poses[i] - (P2.poses[i] - R.dU[i])));
        smax = std::max(smax, std::fabs(R.dU[i]));
    }
    for (const BAProblem::Member& m : P.members)
        for (uint32_t j = 0; j < m.n_free; j++) {
            const double want = P2.exts[m.ext_offset + j] - R.dU[m.ext_col + j];
            dmax = std::max(dmax, std::fabs(P.exts[m.ext_offset + j] - want));
            smax = std::max(smax, std::fabs(R.dU[m.ext_col + j]));
        }
    for (const BAProblem::Group& gr : P.groups)
        for (uint32_t j = 0; j < gr.n_intr; j++) {
            const double want = P2.intr[gr.intr_offset + j] - R.dU[gr.intr_col + j];
            dmax = std::max(dmax, std::fabs(P.intr[gr.intr_offset + j] - want) /
                                      std::max(1.0, std::fabs(want)));
            smax = std::max(smax, std::fabs(R.dU[gr.intr_col + j]));
        }
    snprintf(name, sizeof name, "%s step model=%u groups=%u %s rig=%u", cg ? "cg " : "dU ", model,
             groups, loss, rig);
    report(name, dmax / std::max(smax, 1e-300), cg ? 1e-6 : 1e-9);

    double pmax = 0, psc = 0;
    for (uint32_t p = 0; p < 3 * P.num_points; p++) {
        pmax = std::max(pmax, std::fabs(P.points[p] - (P2.points[p] - R.dP[p])));
        psc = std::max(psc, std::fabs(R.dP[p]));
    }
    snprintf(name, sizeof name, "%s dP   model=%u groups=%u", cg ? "cg " : "dU ", model, groups);
    report(name, pmax / std::max(psc, 1e-300), cg ? 1e-6 : 1e-9);
}

// A full solve has to descend, and the two linear solvers have to agree on
// where it lands.
void testFullSolve(uint32_t model, uint32_t groups, uint32_t rig = 0) {
    BAProblem base = makeProblem(model, 12, 200, groups, 0.3, 31 + model, -1, rig);
    double cost[2];
    std::vector<double> poses[2];
    for (int k = 0; k < 2; k++) {
        BAProblem P = base;
        SolverOptions opt;
        opt.real = RealCfg::CPU;
        opt.loss = "huber";
        opt.loss_param = 2.0f;
        opt.max_iters = 12;
        opt.verbose = false;
        opt.solver = k ? SolverSel::CG : SolverSel::Dense;
        opt.cg_tol = 1e-10;
        opt.cg_max_iters = 2000;
        opt.cg_fallback = CgFallback::Off;
        bacpu::Solver s(P, opt);
        s.init();
        s.solve();
        cost[k] = s.stats().final_cost;
        poses[k] = P.poses;
        if (!(s.stats().final_cost < s.stats().initial_cost)) {
            printf("full  model=%u groups=%u rig=%u %s: cost did not decrease  FAIL\n", model,
                   groups, rig, k ? "cg" : "dense");
            g_fail++;
        }
    }
    char name[96];
    snprintf(name, sizeof name, "dense/cg cost model=%u groups=%u rig=%u", model, groups, rig);
    report(name, std::fabs(cost[0] - cost[1]) / std::max(cost[0], 1e-300), 1e-8);
    snprintf(name, sizeof name, "dense/cg poses model=%u groups=%u rig=%u", model, groups, rig);
    report(name, relMax(poses[0].data(), poses[1].data(), poses[0].size()), 1e-6);
}

}  // namespace

int run(int argc, char** argv) {
    bool quick = false;
    for (int i = 1; i < argc; i++)
        if (std::string(argv[i]) == "--quick") quick = true;

    testChol(37);
    testChol(200);
    if (!quick) testChol(400);

    std::mt19937 rng(99);
    {
        int n;
        testJacobianModel<bacpu::SnavelyModel>("jac snavely", defaultIntr(0, n), rng);
        testJacobianModel<bacpu::SnavelyFModel>("jac snavely_f", defaultIntr(1, n), rng);
        testJacobianModel<bacpu::PinholeRadialModel>("jac pinhole_radial", defaultIntr(2, n), rng);
        testJacobianModel<bacpu::OpenCVModel>("jac opencv", defaultIntr(3, n), rng);
        testJacobianModel<bacpu::SimplePinholeModel>("jac simple_pinhole", defaultIntr(4, n), rng);
        testJacobianModel<bacpu::PinholeModel>("jac pinhole", defaultIntr(5, n), rng);
        testJacobianModel<bacpu::FisheyeModel>("jac opencv_fisheye", defaultIntr(6, n), rng);
        testJacobianModel<bacpu::FullOpenCVModel>("jac full_opencv", defaultIntr(7, n), rng);
        testJacobianModel<bacpu::ThinPrismFisheyeModel>("jac thin_prism", defaultIntr(8, n), rng);
        testJacobianModel<bacpu::EquirectModel>("jac equirect", defaultIntr(9, n), rng);
        testJacobianRig<bacpu::OpenCVModel>("jac rig opencv", defaultIntr(3, n), rng);
        testJacobianRig<bacpu::FisheyeModel>("jac rig opencv_fisheye", defaultIntr(6, n), rng);
        testJacobianRig<bacpu::EquirectModel>("jac rig equirect", defaultIntr(9, n), rng);
        testZeroRotation<bacpu::OpenCVModel>("zero rotation opencv", defaultIntr(3, n));
        testZeroRotation<bacpu::SnavelyModel>("zero rotation snavely", defaultIntr(0, n));
        testZeroRotation<bacpu::ThinPrismFisheyeModel>("zero rotation thin_prism",
                                                       defaultIntr(8, n));
        testZeroRotation<bacpu::EquirectModel>("zero rotation equirect", defaultIntr(9, n));
        testEquirectSeamResidual();
        testEquirectSeamJacobian();
    }

    for (uint32_t model = 0; model < (uint32_t)kNumModels; model++)
        for (uint32_t groups : {1u, 9u}) testAgainstReference(model, groups, "huber", false);
    testAgainstReference(3, 1, "trivial", false);
    testAgainstReference(3, 9, "cauchy", false);
    testAgainstReference(3, 1, "huber", true);
    testAgainstReference(3, 9, "huber", true);
    // partial free prefixes (a held principal point) and the smallest problem
    // the mapper ever hands the solver
    for (int nf : {0, 2, 6}) {
        testAgainstReference(3, 1, "huber", false, 9, nf);
        testAgainstReference(3, 9, "huber", false, 9, nf);
    }
    for (uint32_t nImg : {2u, 3u}) {
        testAgainstReference(3, 1, "huber", false, nImg, 6);
        testAgainstReference(8, 1, "huber", false, nImg, 10);
    }
    // Rigs: two and three members per frame, extrinsics refined and held, on
    // both linear solvers, with the widest camera model (dof 24).
    for (uint32_t rig : {2u, 3u}) {
        testAgainstReference(3, 1, "huber", false, 7, 6, rig, true);
        testAgainstReference(3, 1, "huber", false, 7, 6, rig, false);
        testAgainstReference(3, 1, "huber", true, 7, 6, rig, true);
        testAgainstReference(7, 1, "huber", false, 7, -1, rig, true);
        testAgainstReference(6, 1, "cauchy", true, 7, -1, rig, true);
    }

    if (!quick)
        for (uint32_t model : {3u, 6u, 7u, 8u})
            for (uint32_t groups : {1u, 12u}) testFullSolve(model, groups);
    testFullSolve(3, 1, 2);
    testFullSolve(6, 1, 3);

    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
