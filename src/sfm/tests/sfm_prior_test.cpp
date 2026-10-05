// Pose priors in the bundle adjustment (sfm/ba/Priors.h): the analytic
// Jacobians against central differences, the device assembly and solve against
// the host, and a gauge the reprojections cannot see recovered from centre,
// up and rotation priors alone.
//
//   sfm_prior_test [--device N] [--real double|df|float] [--no-gpu]
//
// Prints PASS/FAIL per case and returns 0/1. See docs/testing.md.
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "sfm/ba/GradientNorm.h"
#include "sfm/ba/Priors.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Pose.h"
#include "sfm/tests/SyntheticBA.h"
#include "sfm/tests/TestMain.h"

namespace {
using namespace sfm;

int g_fail = 0;

void report(const char* name, double err, double tol) {
    const bool ok = err < tol && std::isfinite(err);
    printf("%-48s err %.3e (tol %.0e)  %s\n", name, err, tol, ok ? "PASS" : "FAIL");
    if (!ok) g_fail++;
}

double relMax(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0, s = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); i++) {
        d = std::max(d, std::fabs(a[i] - b[i]));
        s = std::max(s, std::fabs(b[i]));
    }
    return d / std::max(s, 1e-300);
}

SolverOptions baseOptions(RealCfg real, int device, bool cg) {
    SolverOptions o;
    o.real = real;
    o.device = device;
    o.loss = "huber";
    o.loss_param = 1.5f;
    o.init_damping = 1e-2;
    o.verbose = false;
    o.solver = cg ? SolverSel::CG : SolverSel::Dense;
    o.cg_tol = 1e-10;
    o.cg_model_tol = 0;
    o.cg_max_iters = 3000;
    o.cg_fallback = CgFallback::Off;
    return o;
}

Pose camPose(const BAProblem& P, uint32_t img) {
    const double* q = &P.poses[6 * (size_t)P.image_frame[img]];
    Pose f{angleAxisToRotation({q[0], q[1], q[2]}), {q[3], q[4], q[5]}};
    const uint32_t m = P.image_member[img];
    if (m == kNoMember) return f;
    const double* e = &P.exts[P.members[m].ext_offset];
    return composePose(Pose{angleAxisToRotation({e[0], e[1], e[2]}), {e[3], e[4], e[5]}}, f);
}

// Every camera and point through one Sim(3); the reprojections do not change.
void moveProblem(BAProblem& P, const Sim3& T) {
    for (uint32_t f = 0; f < P.num_frames; f++) {
        double* q = &P.poses[6 * (size_t)f];
        Pose p{angleAxisToRotation({q[0], q[1], q[2]}), {q[3], q[4], q[5]}};
        p = transformPose(T, p);
        const Vec3 aa = rotationToAngleAxis(p.R);
        q[0] = aa.x; q[1] = aa.y; q[2] = aa.z;
        q[3] = p.t.x; q[4] = p.t.y; q[5] = p.t.z;
    }
    for (uint32_t m = 0; m < P.members.size(); m++)
        for (int k = 3; k < 6; k++) P.exts[P.members[m].ext_offset + k] *= T.scale;
    for (uint32_t p = 0; p < P.num_points; p++) {
        Vec3 X{P.points[3 * (size_t)p], P.points[3 * (size_t)p + 1], P.points[3 * (size_t)p + 2]};
        X = transformPoint(T, X);
        P.points[3 * (size_t)p] = X.x;
        P.points[3 * (size_t)p + 1] = X.y;
        P.points[3 * (size_t)p + 2] = X.z;
    }
}

// Priors stated about the problem's current poses, perturbed by `bias`.
PosePriors priorsFrom(const BAProblem& P, std::mt19937& rng, double bias, bool centres) {
    std::normal_distribution<double> gauss;
    PosePriors pr;
    pr.up_w = Vec3{0.1, 0.9, 0.2}.normalized();
    for (uint32_t i = 0; i + 1 < P.num_images; i++) {
        const Pose a = camPose(P, i), b = camPose(P, i + 1);
        PriorRotation r;
        r.i = i;
        r.j = i + 1;
        r.R_ji = mul(angleAxisToRotation({bias * gauss(rng), bias * gauss(rng), bias * gauss(rng)}),
                     mul(b.R, transpose(a.R)));
        r.sigma = 0.01;
        pr.rotations.push_back(r);
    }
    for (uint32_t i = 0; i < P.num_images; i += 2) {
        PriorUp u;
        u.i = i;
        u.u = (mul(camPose(P, i).R, pr.up_w) +
               Vec3{bias * gauss(rng), bias * gauss(rng), bias * gauss(rng)})
                  .normalized();
        u.sigma = 0.02;
        pr.ups.push_back(u);
    }
    if (centres)
        for (uint32_t i = 0; i < P.num_images; i++) {
            PriorCentre c;
            c.n = 1;
            c.img[0] = i;
            c.A[0] = mat3Identity();
            c.b = cameraCenter(camPose(P, i)) +
                  Vec3{bias * gauss(rng), bias * gauss(rng), bias * gauss(rng)};
            c.sigma = {0.01, 0.01, 0.01};
            pr.centres.push_back(c);
            if (i + 2 < P.num_images) {
                // A three-term factor with distinct matrices, as the inertial triple.
                PriorCentre t;
                t.n = 3;
                t.img[0] = i;
                t.img[1] = i + 1;
                t.img[2] = i + 2;
                t.A[0] = mat3Identity();
                for (int k = 0; k < 9; k++) t.A[0][k] *= 0.3;
                t.A[1] = angleAxisToRotation({0.2, -0.1, 0.3});
                for (int k = 0; k < 9; k++) t.A[1][k] *= -0.8;
                t.A[2] = mat3Identity();
                for (int k = 0; k < 9; k++) t.A[2][k] *= 0.5;
                t.b = mul(t.A[0], cameraCenter(camPose(P, i))) +
                      mul(t.A[1], cameraCenter(camPose(P, i + 1))) +
                      mul(t.A[2], cameraCenter(camPose(P, i + 2)));
                t.sigma = {0.02, 0.02, 0.0};
                pr.centres.push_back(t);
            }
        }
    return pr;
}

// ---- Jacobians against central differences ---------------------------------

void testJacobians(uint32_t rig) {
    std::mt19937 rng(11 + rig);
    BAProblem P = synth::makeProblem(3, 7, 40, 1, 0.2, 5 + rig, -1, rig, true);
    PosePriors pr = priorsFrom(P, rng, 0.05, true);
    P.priors = &pr;
    PriorAssembler pa;
    pa.init(P);
    double worst = 0;
    for (size_t k = 0; k < pa.numFactors(); k++) {
        double r[3], J[3][3][6];
        uint32_t frames[3];
        const int nf = pa.debugFactor(P, P.poses.data(), P.exts.data(), k, r, J, frames);
        for (int a = 0; a < nf; a++)
            for (int q = 0; q < 6; q++) {
                std::vector<double> poses = P.poses;
                double& x = poses[6 * (size_t)frames[a] + q];
                const double h = 1e-6;
                double rp[3], rm[3], Jd[3][3][6];
                uint32_t fr[3];
                x += h;
                pa.debugFactor(P, poses.data(), P.exts.data(), k, rp, Jd, fr);
                x -= 2 * h;
                pa.debugFactor(P, poses.data(), P.exts.data(), k, rm, Jd, fr);
                for (int m = 0; m < 3; m++) {
                    const double num = (rp[m] - rm[m]) / (2 * h);
                    worst = std::max(worst, std::fabs(num - J[a][m][q]) /
                                                std::max(1.0, std::fabs(num)));
                }
            }
    }
    char name[64];
    snprintf(name, sizeof name, "jacobians rig=%u (%zu factors)", rig, pa.numFactors());
    report(name, worst, 1e-6);
}

// ---- a Cauchy centre factor ------------------------------------------------

// Ceres 2.2 CauchyLoss(7.815) as COLMAP 4.1.1 builds it, sigma 1 m: 0.5 rho(d^2) and
// rho'(d^2) at d metres, rho(s) = a^2 log(1 + s/a^2), computed in Python, not here.
struct CeresCauchyRef {
    double d, half_rho, weight;
};
constexpr CeresCauchyRef kCeresCauchy[] = {
    {1.0, 0.495950760593629, 0.9838902539661188},
    {3.0, 4.197787518628054, 0.8715647586541271},
    {7.815, 21.166713431816863, 0.5},
    {20.0, 61.729829656845325, 0.1324607225658732},
};

// One factor d metres off: its cost, and its weight read back as g.(J^T r)/|J^T r|^2.
void testCauchyScale() {
    for (const CeresCauchyRef& ref : kCeresCauchy) {
        BAProblem P = synth::makeProblem(3, 7, 40, 1, 0.2, 9, -1, 0, true);
        PosePriors pr;
        PriorCentre c;
        c.n = 1;
        c.b = cameraCenter(camPose(P, 0)) + Vec3{ref.d, 0.0, 0.0};
        c.cauchy = 7.815;
        pr.centres.push_back(c);
        P.priors = &pr;
        PriorAssembler pa;
        pa.init(P);
        char name[80];
        snprintf(name, sizeof name, "cauchy centre: Ceres cost at %.3f m", ref.d);
        report(name, std::fabs(pa.cost(P, P.poses.data(), P.exts.data()) - ref.half_rho) /
                         ref.half_rho, 1e-9);
        pa.assemble(P, P.poses.data(), P.exts.data(), 0.0);
        double r[3], J[3][3][6];
        uint32_t frames[3];
        pa.debugFactor(P, P.poses.data(), P.exts.data(), 0, r, J, frames);
        double gv = 0, vv = 0;
        for (int p = 0; p < 6; p++) {
            double v = 0;
            for (int m = 0; m < 3; m++) v += J[0][m][p] * r[m];
            gv += pa.gradient()[6 * (size_t)frames[0] + p] * v;
            vv += v * v;
        }
        snprintf(name, sizeof name, "cauchy centre: Ceres weight at %.3f m", ref.d);
        report(name, std::fabs(gv / vv - ref.weight) / ref.weight, 1e-9);
    }
}

// A gradient that is the cost's derivative, over factors at several distances.
void testCauchyCentre() {
    std::mt19937 rng(3);
    BAProblem P = synth::makeProblem(3, 7, 40, 1, 0.2, 9, -1, 0, true);
    PosePriors pr;
    for (uint32_t i = 0; i < P.num_images; i++) {
        PriorCentre c;
        c.n = 1;
        c.img[0] = i;
        c.b = cameraCenter(camPose(P, i)) + Vec3{2.0 + i, -1.0, 0.5 * i};
        c.sigma = {1.0, 1.0, 1.0};
        c.cauchy = 7.815;
        pr.centres.push_back(c);
    }
    P.priors = &pr;
    PriorAssembler pa;
    pa.init(P);
    pa.assemble(P, P.poses.data(), P.exts.data(), 0.0);
    std::vector<double> g = pa.gradient(), num(g.size());
    for (size_t k = 0; k < P.poses.size(); k++) {
        std::vector<double> x = P.poses;
        const double h = 1e-6;
        x[k] += h;
        const double cp = pa.cost(P, x.data(), P.exts.data());
        x[k] -= 2 * h;
        num[k] = (cp - pa.cost(P, x.data(), P.exts.data())) / (2 * h);
    }
    report("cauchy centre: gradient is the cost's derivative", relMax(g, num), 1e-6);
}

// Every camera between a Cauchy(7.815) centre 10 m along x and a quadratic one
// where it stands: the free translation x solves (x-10)/(1+(x-10)^2/a^2) + x = 0.
constexpr double kCauchyEquilibrium = 3.80446213147765;  // a^2 = 61.07, Python
constexpr double kCauchyUnsquared = 0.7759693441454945;  // a^2 = 7.815: the wrong scale

// `seed`: the model re-expressed in camera 0's frame, which then sits at exactly
// the zero angle-axis and origin a seed pair's first image starts from.
void testCauchyEquilibrium(RealCfg real, int device, double tol, bool seed = false,
                           bool cg = false) {
    BAProblem P0 = synth::makeProblem(3, 12, 150, 1, 0.1, 77, -1, 0, true);
    {
        SolverOptions o = baseOptions(RealCfg::CPU, device, false);
        o.max_iters = 30;
        BundleSolver s(P0, o);
        s.init();
        s.solve();
    }
    if (seed) {
        const Pose c0 = camPose(P0, 0);
        Sim3 T;
        T.R = c0.R;
        T.t = c0.t;
        moveProblem(P0, T);
        for (int k = 0; k < 6; k++) P0.poses[6 * (size_t)P0.image_frame[0] + k] = 0.0;
    }
    PosePriors pr;
    pr.huber = 1e9;
    for (uint32_t i = 0; i < P0.num_images; i++) {
        PriorCentre c;
        c.n = 1;
        c.img[0] = i;
        c.b = cameraCenter(camPose(P0, i)) + Vec3{10.0, 0.0, 0.0};
        c.cauchy = 7.815;
        pr.centres.push_back(c);
        c.b = cameraCenter(camPose(P0, i));
        c.cauchy = 0;
        pr.centres.push_back(c);
    }
    BAProblem P = P0;
    P.priors = &pr;
    SolverOptions o = baseOptions(real, device, cg);
    o.max_iters = 400;
    o.rtol = 1e-12;
    BundleSolver s(P, o);
    s.init();
    s.solve();
    s.downloadParams();
    double worst = 0;
    for (uint32_t i = 0; i < P.num_images; i++) {
        const Vec3 m = cameraCenter(camPose(P, i)) - cameraCenter(camPose(P0, i));
        worst = std::max(worst, (m - Vec3{kCauchyEquilibrium, 0.0, 0.0}).norm());
    }
    char name[96];
    if (seed)
        snprintf(name, sizeof name, "identity seed: settles at the balance (%s %s)",
                 cg ? "cg" : "dense", realCfgName(real));
    else
        snprintf(name, sizeof name, "cauchy centre: settles at the Ceres balance (%s)",
                 realCfgName(real));
    printf("  %u cameras in %d iterations, worst %.3e m off x = %.5f (the unsquared scale: %.5f)\n",
           P.num_images, s.stats().iterations, worst, kCauchyEquilibrium, kCauchyUnsquared);
    report(name, worst, tol);
}

// ---- device against host ---------------------------------------------------

// `seed`: image 0 at exactly the zero rotation, where the device's rotation
// derivative has to come from its first-order branch (rig 0 only).
void testParity(uint32_t rig, bool cg, RealCfg real, int device, double tol, bool seed = false) {
    std::mt19937 rng(21 + rig);
    BAProblem base = synth::makeProblem(3, 9, 120, 1, 0.2, 40 + rig, -1, rig, true);
    if (seed) {
        const Pose c0 = camPose(base, 0);
        Sim3 T;
        T.R = c0.R;
        T.t = c0.t;
        moveProblem(base, T);
        for (int k = 0; k < 6; k++) base.poses[6 * (size_t)base.image_frame[0] + k] = 0.0;
    }
    PosePriors pr = priorsFrom(base, rng, 0.02, true);
    char name[96];
    const char* path = cg ? "cg" : "dense";
    const char* at = seed ? " at the identity" : "";
    if (!cg) {
        BAProblem Pg = base, Pc = base;
        Pg.priors = Pc.priors = &pr;
        SolverOptions og = baseOptions(real, device, false), oc = og;
        oc.real = RealCfg::CPU;
        BundleSolver sg(Pg, og);
        sg.init();
        sg.debugAssemble(1e-2f);
        BundleSolver sc(Pc, oc);
        sc.init();
        sc.debugAssemble(1e-2f);
        snprintf(name, sizeof name, "S with priors rig=%u%s", rig, at);
        report(name, relMax(sg.debugPackedS(), sc.debugPackedS()), tol);
        // The device's g differs from the host's by ~4e-6 on this problem
        // with the priors off too; the tolerance is for that, not for them.
        snprintf(name, sizeof name, "g with priors rig=%u%s", rig, at);
        report(name, relMax(sg.debugG(), sc.debugG()), 10 * tol);
    }
    BAProblem Pg = base, Pc = base;
    Pg.priors = Pc.priors = &pr;
    SolverOptions og = baseOptions(real, device, cg), oc = og;
    oc.real = RealCfg::CPU;
    og.max_iters = oc.max_iters = 12;
    BundleSolver sg(Pg, og);
    sg.init();
    sg.solve();
    sg.downloadParams();
    BundleSolver sc(Pc, oc);
    sc.init();
    sc.solve();
    const double c0 = sg.stats().initial_cost, c1 = sg.stats().final_cost;
    snprintf(name, sizeof name, "%s descent with priors rig=%u%s", path, rig, at);
    report(name, c1 < c0 ? 0.0 : 1.0, 0.5);
    snprintf(name, sizeof name, "%s cost with priors rig=%u%s", path, rig, at);
    report(name, std::fabs(c1 - sc.stats().final_cost) / std::max(c1, 1e-300), tol);
    snprintf(name, sizeof name, "%s frames with priors rig=%u%s", path, rig, at);
    report(name, relMax(Pg.poses, Pc.poses), tol);
}

// ---- the gauge, recovered from the priors alone ------------------------------

// A reconstruction moved by a similarity reprojects identically; only the
// priors know where it stood. With them the solve puts it back.
void testGauge(uint32_t rig, bool cg, RealCfg real, int device) {
    BAProblem P0 = synth::makeProblem(3, 12, 150, 1, 0.1, 77 + rig, -1, rig, true);
    {
        SolverOptions o = baseOptions(RealCfg::CPU, device, false);
        o.max_iters = 30;
        BundleSolver s(P0, o);
        s.init();
        s.solve();
    }
    std::mt19937 rng(5);
    PosePriors pr = priorsFrom(P0, rng, 0.0, true);
    // Move the whole model: every camera centre and point through one Sim(3).
    Sim3 T;
    T.scale = 1.7;
    T.R = angleAxisToRotation({0.3, -0.2, 0.4});
    T.t = {0.5, -0.3, 0.8};
    BAProblem P = P0;
    moveProblem(P, T);
    P.priors = &pr;
    SolverOptions o = baseOptions(real, device, cg);
    o.max_iters = 60;
    o.rtol = 1e-9;
    BundleSolver s(P, o);
    s.init();
    s.solve();
    s.downloadParams();
    double worst = 0;
    for (uint32_t i = 0; i < P.num_images; i++)
        worst = std::max(worst, (cameraCenter(camPose(P, i)) - cameraCenter(camPose(P0, i))).norm());
    char name[96];
    snprintf(name, sizeof name, "%s gauge recovered rig=%u (%s)", cg ? "cg" : "dense", rig,
             realCfgName(real));
    printf("  cost %.4e -> %.4e in %d iterations (%d accepted)\n", s.stats().initial_cost,
           s.stats().final_cost, s.stats().iterations, s.stats().accepted);
    report(name, worst, 2e-3);
}

// ---- the gradient stop (sfm/ba/GradientNorm.h) -----------------------------

// (w, x, y, z) of a rotation, Shepperd's method, w >= 0.
void quatOf(const Mat3& R, double q[4]) {
    const double t = R[0] + R[4] + R[8];
    if (t > 0) {
        const double s = 2 * std::sqrt(1 + t);
        q[0] = s / 4, q[1] = (R[7] - R[5]) / s, q[2] = (R[2] - R[6]) / s, q[3] = (R[3] - R[1]) / s;
    } else if (R[0] > R[4] && R[0] > R[8]) {
        const double s = 2 * std::sqrt(1 + R[0] - R[4] - R[8]);
        q[0] = (R[7] - R[5]) / s, q[1] = s / 4, q[2] = (R[1] + R[3]) / s, q[3] = (R[2] + R[6]) / s;
    } else if (R[4] > R[8]) {
        const double s = 2 * std::sqrt(1 + R[4] - R[0] - R[8]);
        q[0] = (R[2] - R[6]) / s, q[1] = (R[1] + R[3]) / s, q[2] = s / 4, q[3] = (R[5] + R[7]) / s;
    } else {
        const double s = 2 * std::sqrt(1 + R[8] - R[0] - R[4]);
        q[0] = (R[3] - R[1]) / s, q[1] = (R[2] + R[6]) / s, q[2] = (R[5] + R[7]) / s, q[3] = s / 4;
    }
    if (q[0] < 0)
        for (int i = 0; i < 4; i++) q[i] = -q[i];
}

// The total cost's derivatives by central differences: per column, and the
// largest over point coordinates.
struct FdGradient {
    std::vector<double> col;
    double points = 0;
};

FdGradient fdGradient(BAProblem& P, BundleSolver& cost) {
    FdGradient r;
    r.col.assign(P.n_dim, 0.0);
    const double h = 1e-6;
    auto d = [&](double& x) {
        const double keep = x;
        x = keep + h;
        const double cp = cost.computeCost();
        x = keep - h;
        const double cm = cost.computeCost();
        x = keep;
        return (cp - cm) / (2 * h);
    };
    for (uint32_t f = 0; f < P.num_frames; f++)
        for (int k = 0; k < 6; k++) r.col[6 * (size_t)f + k] = d(P.poses[6 * (size_t)f + k]);
    for (const BAProblem::Member& m : P.members) {
        if (!m.n_free) continue;
        for (uint32_t i = 0, j = 0; i < 6; i++)
            if ((m.mask >> i) & 1u) r.col[m.ext_col + j++] = d(P.exts[m.ext_offset + i]);
    }
    for (const BAProblem::Group& g : P.groups)
        for (uint32_t j = 0; j < g.n_intr; j++) r.col[g.intr_col + j] = d(P.intr[g.intr_offset + j]);
    for (double& x : P.points) r.points = std::max(r.points, std::fabs(d(x)));
    return r;
}

// Ceres' |q - Plus(q, -g)|_inf from the left gradient alone: the quaternion
// tangent is half a rotation vector, so the step turns R by Exp(-4 g_left),
// and the literal product q_d q has dot cos(2 |g_left|) with q.
double quatStepRef(const double* aa, const double* gl) {
    const Mat3 R = angleAxisToRotation({aa[0], aa[1], aa[2]});
    const Vec3 g{gl[0], gl[1], gl[2]};
    double q[4], p[4];
    quatOf(R, q);
    quatOf(mul(angleAxisToRotation(g * -4.0), R), p);
    double dot = 0, m = 0;
    for (int i = 0; i < 4; i++) dot += p[i] * q[i];
    const double sgn = (dot < 0) == (std::cos(2 * g.norm()) < 0) ? 1.0 : -1.0;
    for (int i = 0; i < 4; i++) m = std::max(m, std::fabs(sgn * p[i] - q[i]));
    return m;
}

// C = u . R(aa) v, a smooth cost of one rotation: its angle-axis and left
// gradients by central differences, and Ceres' step from each, over gradient
// sizes that put the literal product's cos |2 g_left| on both signs.
void testQuaternionStep() {
    std::mt19937 rng(23);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);
    double worst = 0, jl = 0;
    int negative = 0;
    const double h = 1e-6;
    for (int k = 0; k < 64; k++) {
        double aa[3];
        for (double& x : aa) x = 1.5 * unit(rng);
        const double sc = 0.05 * std::pow(10.0, (k % 4) / 1.5);
        const Vec3 u = Vec3{unit(rng), unit(rng), unit(rng)} * sc;
        const Vec3 v{unit(rng), unit(rng), unit(rng)};
        auto C = [&](const Mat3& R) {
            const Vec3 w = mul(R, v);
            return u.x * w.x + u.y * w.y + u.z * w.z;
        };
        const Mat3 R = angleAxisToRotation({aa[0], aa[1], aa[2]});
        double gaa[3], gl[3];
        for (int i = 0; i < 3; i++) {
            double ap[3] = {aa[0], aa[1], aa[2]}, am[3] = {aa[0], aa[1], aa[2]};
            ap[i] += h;
            am[i] -= h;
            gaa[i] = (C(angleAxisToRotation({ap[0], ap[1], ap[2]})) -
                      C(angleAxisToRotation({am[0], am[1], am[2]}))) / (2 * h);
            const Vec3 e{i == 0 ? h : 0.0, i == 1 ? h : 0.0, i == 2 ? h : 0.0};
            gl[i] = (C(mul(angleAxisToRotation(e), R)) - C(mul(angleAxisToRotation(e * -1.0), R))) /
                    (2 * h);
        }
        const Vec3 left{gl[0], gl[1], gl[2]};
        const Vec3 m =
            mul(transpose(so3LeftJacobianInv({aa[0], aa[1], aa[2]})), Vec3{gaa[0], gaa[1], gaa[2]});
        jl = std::max(jl, (m - left).norm() / left.norm());
        negative += std::cos(2 * left.norm()) < 0;
        worst = std::max(worst, std::fabs(quaternionStepMax(aa, gaa) - quatStepRef(aa, gl)));
    }
    printf("  64 rotations, %d with cos |2 g_left| < 0\n", negative);
    report("fixture: the step's sign flips in some cases",
           negative >= 8 && negative <= 56 ? 0.0 : 1.0, 0.5);
    report("gradient norm: Jl^-T takes the angle-axis gradient to the left one", jl, 1e-6);
    report("gradient norm: rotation is Ceres' quaternion step", worst, 1e-6);
}

struct GradCase {
    BAProblem P;
    PosePriors pr;
};

// Every prior kind, lengths in units of 0.4 m, and with `rig` a free extrinsic.
constexpr double kMetresPerUnit = 2.5;
GradCase gradCase(uint32_t rig) {
    GradCase c;
    c.P = synth::makeProblem(3, 7, 40, 1, 0.2, 9, -1, rig, true);
    std::mt19937 rng(17);
    c.pr = priorsFrom(c.P, rng, 0.05, true);
    return c;
}
BAProblem problemOf(const GradCase& c, bool priors) {
    BAProblem P = c.P;
    P.priors = priors ? &c.pr : nullptr;
    return P;
}

// Each Euclidean part of the norm from the finite-difference gradient, in metres,
// and the rotation part over the frames' and the extrinsic's own blocks.
void testGradientNorm() {
    const GradCase gc = gradCase(2);
    BAProblem base = problemOf(gc, true);
    SolverOptions o = baseOptions(RealCfg::CPU, -1, false);
    BundleSolver cost(base, o);
    cost.init();
    const FdGradient fd = fdGradient(base, cost);
    const GradientNormParts got = gradientNormParts(base, base.poses.data(), base.exts.data(),
                                                    fd.col.data(), fd.points, kMetresPerUnit);
    GradientNormParts want;
    auto rigid = [&](const double* aa, const double* g6) {
        want.rotation = std::max(want.rotation, quaternionStepMax(aa, g6));
        for (int i = 3; i < 6; i++)
            want.translation = std::max(want.translation, std::fabs(g6[i]) / kMetresPerUnit);
    };
    for (uint32_t f = 0; f < base.num_frames; f++)
        rigid(&base.poses[6 * (size_t)f], &fd.col[6 * (size_t)f]);
    int free = 0;
    for (const BAProblem::Member& m : base.members)
        if (m.n_free == 6) {
            rigid(&base.exts[m.ext_offset], &fd.col[m.ext_col]);
            free++;
        }
    for (const BAProblem::Group& g : base.groups)
        for (uint32_t j = 0; j < g.n_intr; j++)
            want.intrinsics = std::max(want.intrinsics, std::fabs(fd.col[g.intr_col + j]));
    want.points = fd.points / kMetresPerUnit;
    printf("  rotation %.4e translation %.4e intrinsics %.4e points %.4e (%d free extrinsics)\n",
           want.rotation, want.translation, want.intrinsics, want.points, free);
    report("fixture: a free extrinsic and no zero part",
           free >= 1 && want.rotation > 0 && want.translation > 0 && want.intrinsics > 0 &&
                   want.points > 0 ? 0.0 : 1.0, 0.5);
    report("gradient norm: rotation over every frame and extrinsic",
           std::fabs(got.rotation - want.rotation), 1e-12);
    report("gradient norm: translation in metres", std::fabs(got.translation - want.translation) /
                                                       want.translation, 1e-12);
    report("gradient norm: intrinsics", std::fabs(got.intrinsics - want.intrinsics) /
                                            want.intrinsics, 1e-12);
    report("gradient norm: points in metres", std::fabs(got.points - want.points) / want.points,
           1e-12);
    // Only an extrinsic's columns nonzero, since above a frame may hold every maximum.
    const BAProblem::Member* m0 = nullptr;
    for (const BAProblem::Member& m : base.members)
        if (!m0 && m.n_free == 6) m0 = &m;
    if (!m0) return;
    std::vector<double> only(fd.col.size(), 0.0);
    only[m0->ext_col + 1] = 0.3;
    only[m0->ext_col + 4] = 5.0;
    const double g6[6] = {0, 0.3, 0, 0, 5.0, 0};
    const GradientNormParts e = gradientNormParts(base, base.poses.data(), base.exts.data(),
                                                  only.data(), 0.0, kMetresPerUnit);
    const double rot = quaternionStepMax(&base.exts[m0->ext_offset], g6);
    report("gradient norm: an extrinsic's own block counts",
           rot > 0 ? std::fabs(e.rotation - rot) + std::fabs(e.translation - 5.0 / kMetresPerUnit)
                   : 1.0, 1e-12);
}

// A tolerance above the first point: the solve stops there, before any step,
// and the gradient it measured is the cost's. The priors' gradient reaches the
// frame columns only, so a rig carries none (its extrinsic columns would disagree).
void testGradientAtStart(RealCfg real, int device, bool cg, double tol, uint32_t rig) {
    const GradCase gc = gradCase(rig);
    BAProblem base = problemOf(gc, rig == 0);
    FdGradient fd;
    {
        BAProblem Q = base;
        SolverOptions o = baseOptions(RealCfg::CPU, device, false);
        BundleSolver cost(Q, o);
        cost.init();
        fd = fdGradient(Q, cost);
    }
    SolverOptions o = baseOptions(real, device, cg);
    o.max_iters = 5;
    o.gradient_tol = 1e300;
    o.metres_per_unit = kMetresPerUnit;
    BAProblem P = base;
    BundleSolver s(P, o);
    s.init();
    s.solve();
    s.downloadParams();
    const SolverStats& st = s.stats();
    char name[112];
    const char* tag = realCfgName(real);
    const char* what = rig ? "rig" : "priors";
    snprintf(name, sizeof name, "gradient stop at the start: 0 iterations (%s %s %s)",
             cg ? "cg" : "dense", what, tag);
    report(name, st.gradient_stop && st.iterations == 0 && st.gradient_norms.size() == 1 ? 0.0 : 1.0,
           0.5);
    snprintf(name, sizeof name, "gradient stop at the start: nothing moves (%s %s %s)",
             cg ? "cg" : "dense", what, tag);
    report(name, std::max({relMax(P.poses, base.poses), relMax(P.exts, base.exts),
                           relMax(P.intr, base.intr), relMax(P.points, base.points)}),
           real == RealCfg::F32 ? 1e-6 : 1e-12);
    if (cg) return;
    snprintf(name, sizeof name, "gradient stop: d cost / d column is the cost's (%s %s)", what, tag);
    report(name, relMax(s.lastGradient(), fd.col), tol);
    snprintf(name, sizeof name, "gradient stop: max d cost / d point is the cost's (%s %s)", what,
             tag);
    report(name, std::fabs(s.lastPointGradientMax() - fd.points) / fd.points, tol);
    const double want = gradientNormParts(base, base.poses.data(), base.exts.data(), fd.col.data(),
                                          fd.points, kMetresPerUnit).max();
    snprintf(name, sizeof name, "gradient stop: the norm it measured (%s %s)", what, tag);
    report(name, st.gradient_norms.empty() ? 1.0 : std::fabs(st.gradient_norms[0] - want) / want,
           tol);
}

// The stop takes the first accepted point at or under the tolerance, and keeps it.
void testGradientStop() {
    const GradCase gc = gradCase(0);
    BAProblem base = problemOf(gc, true);
    auto solve = [&](double tol, int iters, BAProblem& P) {
        SolverOptions o = baseOptions(RealCfg::CPU, -1, false);
        o.max_iters = iters;
        o.gradient_tol = tol;
        o.metres_per_unit = kMetresPerUnit;
        BundleSolver s(P, o);
        s.init();
        s.solve();
        return s.stats();
    };
    BAProblem A = base;
    const SolverStats a = solve(1e-300, 25, A);
    const std::vector<double>& t = a.gradient_norms;
    size_t j = 2;
    while (j < t.size() && !(t[j] < *std::min_element(t.begin(), t.begin() + j))) j++;
    printf("  trace of %zu norms, first new minimum after two points at %zu\n", t.size(), j);
    report("fixture: a new minimum after two accepted points", j < t.size() ? 0.0 : 1.0, 0.5);
    if (j >= t.size()) return;
    BAProblem B = base;
    const SolverStats b = solve(t[j], 25, B);
    const bool prefix = b.gradient_norms.size() == j + 1 &&
                        std::equal(b.gradient_norms.begin(), b.gradient_norms.end(), t.begin());
    report("gradient stop: stops at the first point under the tolerance",
           b.gradient_stop && prefix ? 0.0 : 1.0, 0.5);
    BAProblem C = base;
    solve(1e-300, b.iterations, C);
    report("gradient stop: keeps the point it measured",
           std::max({relMax(B.poses, C.poses), relMax(B.points, C.points), relMax(B.intr, C.intr)}),
           1e-15);
}

int run(int argc, char** argv) {
    int device = -1;
    RealCfg real = RealCfg::F64;
    bool gpu = true;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--device" && i + 1 < argc) device = std::stoi(argv[++i]);
        else if (std::string(argv[i]) == "--real" && i + 1 < argc) real = realCfgFromName(argv[++i]);
        else if (std::string(argv[i]) == "--no-gpu") gpu = false;
    }
    testJacobians(0);
    testJacobians(2);
    testCauchyScale();
    testCauchyCentre();
    testCauchyEquilibrium(RealCfg::CPU, device, 1e-3);
    testCauchyEquilibrium(RealCfg::CPU, device, 1e-3, true, false);
    testCauchyEquilibrium(RealCfg::CPU, device, 1e-3, true, true);
    testQuaternionStep();
    testGradientNorm();
    for (uint32_t rig : {0u, 2u}) {
        testGradientAtStart(RealCfg::CPU, device, false, 1e-6, rig);
        testGradientAtStart(RealCfg::CPU, device, true, 1e-6, rig);
    }
    testGradientStop();
    testGauge(0, false, RealCfg::CPU, device);
    testGauge(2, false, RealCfg::CPU, device);
    testGauge(0, true, RealCfg::CPU, device);
    testGauge(2, true, RealCfg::CPU, device);
    if (gpu) {
        const double tol = real == RealCfg::F32 ? 5e-3 : 1e-6;
        testParity(0, false, real, device, tol);
        testParity(2, false, real, device, tol);
        testParity(0, true, real, device, tol);
        testParity(2, true, real, device, tol);
        // The moved problem parities at 4.6e-6 in double whether image 0 sits at
        // zero or at 1e-2 rad (1070 Ti): the device's trig, not the branch.
        testParity(0, false, real, device, real == RealCfg::F32 ? tol : 1e-5, true);
        testGauge(0, false, real, device);
        testGauge(2, true, real, device);
        testCauchyEquilibrium(real, device, real == RealCfg::F32 ? 2e-2 : 1e-3);
        testCauchyEquilibrium(real, device, real == RealCfg::F32 ? 2e-2 : 1e-3, true, false);
        testCauchyEquilibrium(real, device, real == RealCfg::F32 ? 2e-2 : 1e-3, true, true);
        // As the parity tests' g: the device gradient is good to ~5e-6 in double.
        for (uint32_t rig : {0u, 2u}) {
            testGradientAtStart(real, device, false, real == RealCfg::F32 ? 1e-3 : 1e-5, rig);
            testGradientAtStart(real, device, true, real == RealCfg::F32 ? 1e-3 : 1e-5, rig);
        }
    }
    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
