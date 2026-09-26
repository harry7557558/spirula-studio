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

// ---- device against host ---------------------------------------------------

void testParity(uint32_t rig, bool cg, RealCfg real, int device, double tol) {
    std::mt19937 rng(21 + rig);
    const BAProblem base = synth::makeProblem(3, 9, 120, 1, 0.2, 40 + rig, -1, rig, true);
    PosePriors pr = priorsFrom(base, rng, 0.02, true);
    char name[96];
    const char* path = cg ? "cg" : "dense";
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
        snprintf(name, sizeof name, "S with priors rig=%u", rig);
        report(name, relMax(sg.debugPackedS(), sc.debugPackedS()), tol);
        // The device's g differs from the host's by ~4e-6 on this problem
        // with the priors off too; the tolerance is for that, not for them.
        snprintf(name, sizeof name, "g with priors rig=%u", rig);
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
    snprintf(name, sizeof name, "%s descent with priors rig=%u", path, rig);
    report(name, c1 < c0 ? 0.0 : 1.0, 0.5);
    snprintf(name, sizeof name, "%s cost with priors rig=%u", path, rig);
    report(name, std::fabs(c1 - sc.stats().final_cost) / std::max(c1, 1e-300), tol);
    snprintf(name, sizeof name, "%s frames with priors rig=%u", path, rig);
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
        testGauge(0, false, real, device);
        testGauge(2, true, real, device);
    }
    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
