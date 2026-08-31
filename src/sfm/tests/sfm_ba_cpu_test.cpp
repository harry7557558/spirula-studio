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
#include "sfm/tests/TestMain.h"

namespace {

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
    const double d = 2.0 * M_PI * 2.0 / intr[0];
    const double points[2][3] = {{std::sin(M_PI - d), 0, std::cos(M_PI - d)},
                                 {std::sin(-M_PI + d), 0, std::cos(-M_PI + d)}};
    const double obs[2][2] = {{2.0, 240.0}, {638.0, 240.0}};
    double worst = 0;
    for (int i = 0; i < 2; i++) {
        double r[2];
        bacpu::residual<bacpu::EquirectModel>(pose, intr, points[i], obs[i], r);
        worst = std::max(worst, std::fabs(r[0] - (i == 0 ? -4.0 : 4.0)));
        worst = std::max(worst, std::fabs(r[1]));
    }
    report("equirect seam residual", worst, 1e-10);
}

// ---------------------------------------------------------------------------
// synthetic problems
// ---------------------------------------------------------------------------

// Camera-frame +z is forward for every model but Snavely's, which projects
// along -z; the generator flips the look-at frame for those two.
bool forwardIsMinusZ(uint32_t model) { return model == 0 || model == 1; }

const double* defaultIntr(uint32_t model, int& n) {
    static const double snavely[3] = {std::log(600.0), -1e-8, 1e-12};
    static const double snavely_f[3] = {600.0, -1e-8, 1e-12};
    static const double radial[5] = {600.0, -0.02, 0.003, 320.0, 240.0};
    static const double opencv[8] = {600.0, 605.0, -0.02, 0.003, 1e-4, -2e-4, 320.0, 240.0};
    static const double simple[3] = {600.0, 320.0, 240.0};
    static const double pinhole[4] = {600.0, 605.0, 320.0, 240.0};
    static const double fisheye[8] = {300.0, 305.0, 0.01, -0.002, 3e-4, -1e-5, 320.0, 240.0};
    static const double full[12] = {600.0, 605.0, -0.02, 0.003, 1e-4,  -2e-4,
                                    1e-4,  5e-3,  -8e-4, 2e-5,  320.0, 240.0};
    static const double prism[12] = {300.0, 305.0, 0.01,  -0.002, 3e-4,  -1e-5,
                                     2e-4,  -1e-6, 1e-4,  -2e-4,  320.0, 240.0};
    static const double equirect[2] = {640.0, 480.0};
    switch (model) {
        case 0: n = 3; return snavely;
        case 1: n = 3; return snavely_f;
        case 2: n = 5; return radial;
        case 3: n = 8; return opencv;
        case 4: n = 3; return simple;
        case 5: n = 4; return pinhole;
        case 6: n = 8; return fisheye;
        case 7: n = 12; return full;
        case 8: n = 12; return prism;
        default: n = 2; return equirect;
    }
}

void project(uint32_t model, const double* intr, const double p[3], double out[2]) {
    bacpu::withModel(model, [&](auto M) { decltype(M)::template project<double>(intr, p, out); });
}

// nImg cameras on a sphere looking at the origin, nPt points inside it; every
// observation that projects sanely is kept. `groups` is 1 (one shared camera)
// or nImg (one per image, the exclusive case).
BAProblem makeProblem(uint32_t model, uint32_t nImg, uint32_t nPt, uint32_t groups,
                      double noise, uint32_t seed, int nfree = -1) {
    std::mt19937 rng(seed);
    std::normal_distribution<double> gauss;
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    int ni = 0;
    const double* base = defaultIntr(model, ni);
    const bool minusZ = forwardIsMinusZ(model);

    BAProblem P;
    P.num_images = nImg;
    P.poses.resize(6 * (size_t)nImg);
    std::vector<double> centers(3 * (size_t)nImg), rot(9 * (size_t)nImg);
    for (uint32_t i = 0; i < nImg; i++) {
        const double th = 2.0 * M_PI * i / nImg + 0.03 * unit(rng);
        const double ph = 0.4 * unit(rng);
        const sfm::Vec3 c{4.0 * std::cos(th) * std::cos(ph), 4.0 * std::sin(ph),
                          4.0 * std::sin(th) * std::cos(ph)};
        const sfm::Vec3 f = (sfm::Vec3{0, 0, 0} - c).normalized();
        const sfm::Vec3 rt = sfm::Vec3{0, 1, 0}.cross(f).normalized();
        const sfm::Vec3 u2 = f.cross(rt);
        // -z forward for Snavely; flipping two rows keeps det = +1
        const double s = minusZ ? -1.0 : 1.0;
        const sfm::Mat3 R{rt.x,     rt.y,     rt.z,     s * u2.x, s * u2.y,
                          s * u2.z, s * f.x,  s * f.y,  s * f.z};
        const sfm::Vec3 aa = sfm::rotationToAngleAxis(R);
        P.poses[6 * (size_t)i + 0] = aa.x;
        P.poses[6 * (size_t)i + 1] = aa.y;
        P.poses[6 * (size_t)i + 2] = aa.z;
        for (int r = 0; r < 3; r++) {
            P.poses[6 * (size_t)i + 3 + r] =
                -(R[3 * r] * c.x + R[3 * r + 1] * c.y + R[3 * r + 2] * c.z);
            for (int k = 0; k < 3; k++) rot[9 * (size_t)i + 3 * r + k] = R[3 * r + k];
        }
        centers[3 * (size_t)i + 0] = c.x;
        centers[3 * (size_t)i + 1] = c.y;
        centers[3 * (size_t)i + 2] = c.z;
    }

    P.groups.resize(groups);
    P.image_group.resize(nImg);
    P.intr.resize((size_t)ni * groups);
    for (uint32_t g = 0; g < groups; g++) {
        for (int j = 0; j < ni; j++) P.intr[(size_t)ni * g + j] = base[j] * (1.0 + 0.005 * unit(rng));
        // EQUIRECTANGULAR's two parameters are the image size and never refine
        const uint32_t nf = model == 9 ? 0u : (uint32_t)(nfree >= 0 ? std::min(nfree, ni) : ni);
        P.groups[g] = {(uint32_t)(ni * g), 0, nf, model};
    }
    for (uint32_t i = 0; i < nImg; i++) P.image_group[i] = groups == 1 ? 0 : i;

    P.num_points = nPt;
    P.points.resize(3 * (size_t)nPt);
    for (uint32_t p = 0; p < nPt; p++)
        for (int k = 0; k < 3; k++) P.points[3 * (size_t)p + k] = 1.2 * unit(rng);

    P.obs_ranges.assign(nPt + 1, 0);
    for (uint32_t p = 0; p < nPt; p++) {
        for (uint32_t i = 0; i < nImg; i++) {
            double d[3] = {P.points[3 * (size_t)p] - centers[3 * (size_t)i],
                           P.points[3 * (size_t)p + 1] - centers[3 * (size_t)i + 1],
                           P.points[3 * (size_t)p + 2] - centers[3 * (size_t)i + 2]};
            double pc[3];
            for (int r = 0; r < 3; r++)
                pc[r] = rot[9 * (size_t)i + 3 * r] * d[0] + rot[9 * (size_t)i + 3 * r + 1] * d[1] +
                        rot[9 * (size_t)i + 3 * r + 2] * d[2];
            if (minusZ) pc[2] = -std::fabs(pc[2]);
            double px[2];
            project(model, &P.intr[(size_t)ni * (groups == 1 ? 0 : i)], pc, px);
            if (!std::isfinite(px[0]) || !std::isfinite(px[1])) continue;
            if (std::fabs(px[0]) > 4000 || std::fabs(px[1]) > 4000) continue;
            P.obs_image.push_back(i);
            P.obs_point.push_back(p);
            P.obs_xy.push_back(px[0] + noise * gauss(rng));
            P.obs_xy.push_back(px[1] + noise * gauss(rng));
        }
        P.obs_ranges[p + 1] = (uint32_t)P.obs_image.size();
    }
    P.num_obs = (uint32_t)P.obs_image.size();

    P.pose_dim = 6 * nImg;
    P.total_intr = (uint32_t)P.intr.size();
    P.free_intr = 0;
    for (BAProblem::Group& g : P.groups) {
        g.intr_col = P.pose_dim + P.free_intr;
        P.free_intr += g.n_intr;
    }
    P.n_dim = P.pose_dim + P.free_intr;
    finalizeTables(P);

    // perturb, so the solver has something to do
    for (uint32_t i = 0; i < 6 * nImg; i++) P.poses[i] += 0.004 * gauss(rng);
    for (uint32_t p = 0; p < 3 * nPt; p++) P.points[p] += 0.01 * gauss(rng);
    return P;
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
            const uint32_t dof = 6 + gr.n_intr;
            double Jc[2 * kMaxCamDof] = {}, Jp[6], r[2];
            uint32_t cols[kMaxCamDof];
            for (uint32_t i = 0; i < 6; i++) cols[i] = 6 * img + i;
            for (uint32_t i = 0; i < gr.n_intr; i++) cols[6 + i] = gr.intr_col + i;
            bacpu::withModel(gr.model, [&](auto M) {
                using MT = decltype(M);
                constexpr int DOF = 6 + MT::kNumIntr;
                double jc[2 * DOF];
                bacpu::jacobian<MT>(&P.poses[6 * (size_t)img], &P.intr[gr.intr_offset],
                                    &P.points[3 * (size_t)pt], &P.obs_xy[2 * (size_t)o], r, jc, Jp);
                const double sw = std::sqrt(decltype(L)::weight(r[0] * r[0] + r[1] * r[1],
                                                                lossParam));
                for (int row = 0; row < 2; row++)
                    for (uint32_t a = 0; a < dof; a++) Jc[row * kMaxCamDof + a] = jc[row * DOF + a] * sw;
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
                          uint32_t nImg = 9, int nfree = -1) {
    const double lambda = 1e-2;
    BAProblem P = makeProblem(model, nImg, 90, groups, 0.15, 7 * model + groups, nfree);
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
        snprintf(name, sizeof name, "S  model=%u groups=%u n=%u f=%d", model, groups, nImg, nfree);
        report(name, dS / sS, 1e-10);
        snprintf(name, sizeof name, "g  model=%u groups=%u n=%u f=%d", model, groups, nImg, nfree);
        report(name, relMax(g.data(), R.g.data(), P.n_dim), 1e-10);
    }

    solver.solve();
    double dmax = 0, smax = 0;
    for (uint32_t i = 0; i < P.pose_dim; i++) {
        dmax = std::max(dmax, std::fabs(P.poses[i] - (P2.poses[i] - R.dU[i])));
        smax = std::max(smax, std::fabs(R.dU[i]));
    }
    for (const BAProblem::Group& gr : P.groups)
        for (uint32_t j = 0; j < gr.n_intr; j++) {
            const double want = P2.intr[gr.intr_offset + j] - R.dU[gr.intr_col + j];
            dmax = std::max(dmax, std::fabs(P.intr[gr.intr_offset + j] - want) /
                                      std::max(1.0, std::fabs(want)));
            smax = std::max(smax, std::fabs(R.dU[gr.intr_col + j]));
        }
    snprintf(name, sizeof name, "%s step model=%u groups=%u %s", cg ? "cg " : "dU ", model, groups,
             loss);
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
void testFullSolve(uint32_t model, uint32_t groups) {
    BAProblem base = makeProblem(model, 12, 200, groups, 0.3, 31 + model);
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
            printf("full  model=%u groups=%u %s: cost did not decrease  FAIL\n", model, groups,
                   k ? "cg" : "dense");
            g_fail++;
        }
    }
    char name[96];
    snprintf(name, sizeof name, "dense/cg cost model=%u groups=%u", model, groups);
    report(name, std::fabs(cost[0] - cost[1]) / std::max(cost[0], 1e-300), 1e-8);
    snprintf(name, sizeof name, "dense/cg poses model=%u groups=%u", model, groups);
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
        testZeroRotation<bacpu::OpenCVModel>("zero rotation opencv", defaultIntr(3, n));
        testZeroRotation<bacpu::SnavelyModel>("zero rotation snavely", defaultIntr(0, n));
        testZeroRotation<bacpu::ThinPrismFisheyeModel>("zero rotation thin_prism",
                                                       defaultIntr(8, n));
        testZeroRotation<bacpu::EquirectModel>("zero rotation equirect", defaultIntr(9, n));
        testEquirectSeamResidual();
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

    if (!quick)
        for (uint32_t model : {3u, 6u, 7u, 8u})
            for (uint32_t groups : {1u, 12u}) testFullSolve(model, groups);

    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
