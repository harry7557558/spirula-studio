// A GPS-like prior on a collective mode the reprojections do not hold: twelve
// cameras in a line, each link's points private to it, so every link's scale is
// a null direction and the tail answers only to an absolute centre factor. At the
// damping a growth BA leaves, a step cuts the total cost by far less than rtol:
// the tie-zone regime that left a canopy capture's woods chain where PnP put it.
//
//   sfm_ba_soft_prior_test [--device N] [--real double|df|float] [--no-gpu]
//
// Prints FAIL lines and returns the count.
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/ba/Priors.h"
#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/core/Pose.h"
#include "sfm/tests/TestMain.h"

namespace {
using namespace sfm;

int fails = 0;
void check(bool ok, const std::string& what) {
    std::printf("  %s: %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) fails++;
}

constexpr uint32_t kCams = 12;
constexpr uint32_t kPtsPerLink = 40;
constexpr double kShrink = 0.8;   // links 1..10 start at 0.8 of their true length
constexpr double kNoisePx = 3.0;
// Iterations the loop without the prior-step rule takes on the gate fixture (CPU):
// a run without absolute centres must not see the rule at all.
constexpr int kGateIterations = 5;

// Cameras at x = 0..11 looking down world -z; camera k and k+1 alone see link k.
BAProblem makeChain() {
    std::mt19937 rng(20260928);
    std::uniform_real_distribution<double> u01(0.0, 1.0);
    std::normal_distribution<double> gauss;
    BAProblem P;
    P.num_images = P.num_frames = kCams;
    P.image_frame.resize(kCams);
    P.image_member.assign(kCams, kNoMember);
    P.poses.resize(6 * kCams);
    const Mat3 R{1, 0, 0, 0, -1, 0, 0, 0, -1};
    const Vec3 aa = rotationToAngleAxis(R);
    std::vector<double> cx(kCams);
    for (uint32_t i = 0; i < kCams; i++) {
        P.image_frame[i] = i;
        cx[i] = i == 0 ? 0.0 : cx[i - 1] + (i == 1 ? 1.0 : kShrink);
        const Vec3 t = mul(R, Vec3{cx[i], 0, 0}) * -1.0;
        double* q = &P.poses[6 * (size_t)i];
        q[0] = aa.x; q[1] = aa.y; q[2] = aa.z;
        q[3] = t.x; q[4] = t.y; q[5] = t.z;
    }
    P.intr = {1000.0, 1000.0, 0.0, 0.0};
    P.groups = {{0, 0, 0, 5}};  // pinhole, held
    P.image_group.assign(kCams, 0);
    const uint32_t nPt = (kCams - 1) * kPtsPerLink;
    P.num_points = nPt;
    P.points.resize(3 * (size_t)nPt);
    P.obs_ranges.assign(nPt + 1, 0);
    for (uint32_t k = 0; k + 1 < kCams; k++)
        for (uint32_t j = 0; j < kPtsPerLink; j++) {
            const uint32_t p = k * kPtsPerLink + j;
            const Vec3 X{k - 1.5 + 4.0 * u01(rng), -2.0 + 4.0 * u01(rng), -(5.0 + 5.0 * u01(rng))};
            for (uint32_t i : {k, k + 1}) {
                const Vec3 pc = mul(R, X - Vec3{(double)i, 0, 0});
                P.obs_image.push_back(i);
                P.obs_point.push_back(p);
                P.obs_xy.push_back(1000.0 * pc.x / pc.z + kNoisePx * gauss(rng));
                P.obs_xy.push_back(1000.0 * pc.y / pc.z + kNoisePx * gauss(rng));
            }
            P.obs_ranges[p + 1] = (uint32_t)P.obs_image.size();
            // The link shrunk about camera k: it reprojects exactly as the truth does.
            const Vec3 Xs = Vec3{cx[k], 0, 0} + (X - Vec3{(double)k, 0, 0}) * (k ? kShrink : 1.0);
            P.points[3 * (size_t)p] = Xs.x;
            P.points[3 * (size_t)p + 1] = Xs.y;
            P.points[3 * (size_t)p + 2] = Xs.z;
        }
    P.num_obs = (uint32_t)P.obs_image.size();
    P.pose_dim = 6 * kCams;
    P.ext_dim = 0;
    P.total_intr = (uint32_t)P.intr.size();
    P.free_intr = 0;
    P.groups[0].intr_col = P.pose_dim;
    P.n_dim = P.pose_dim;
    finalizeTables(P);
    return P;
}

PosePriors chainPriors(bool absolute_centres, bool tail = true);

// The chain with its reprojections converged under the gauge priors alone, as a
// model the previous solve left: what remains for camera 11's fix is the links' scales.
BAProblem convergedChain() {
    static const BAProblem converged = [] {
        BAProblem P = makeChain();
        static const PosePriors gauge = chainPriors(true, false);
        P.priors = &gauge;
        SolverOptions o;
        o.real = RealCfg::CPU;
        o.loss = "trivial";
        o.solver = SolverSel::Dense;
        o.verbose = false;
        o.rtol = 1e-12;
        o.max_iters = 200;
        BundleSolver s(P, o);
        s.init();
        s.solve();
        P.priors = nullptr;
        return P;
    }();
    return converged;
}

PriorCentre absolute(uint32_t img, const Vec3& at, double sigma) {
    PriorCentre c;
    c.n = 1;
    c.img[0] = img;
    c.b = at;
    c.sigma = {sigma, sigma, sigma};
    return c;
}

PriorCentre displacement(uint32_t from, uint32_t to, double dx, double sigma) {
    PriorCentre c;
    c.n = 2;
    c.img[0] = to;
    c.img[1] = from;
    for (int k = 0; k < 9; k++) c.A[1][k] = -c.A[1][k];
    c.b = {dx, 0, 0};
    c.sigma = {sigma, sigma, sigma};
    return c;
}

// Cameras 0 and 1 hold the gauge and link 0; camera 11's fix at x = 11 is ~1.5 sigma away.
PosePriors chainPriors(bool absolute_centres, bool tail) {
    PosePriors pr;
    pr.up_w = {0, 1, 0};
    PriorUp up;  // the roll about the camera line, which no centre sees
    up.i = 0;
    up.u = {0, -1, 0};
    up.sigma = 0.01;
    pr.ups.push_back(up);
    if (absolute_centres) {
        pr.centres.push_back(absolute(0, {0, 0, 0}, 0.01));
        pr.centres.push_back(absolute(1, {1, 0, 0}, 0.01));
        if (tail) pr.centres.push_back(absolute(kCams - 1, {11, 0, 0}, 1.0));
    } else {
        pr.centres.push_back(displacement(0, 1, 1.0, 0.01));
        if (tail) pr.centres.push_back(displacement(0, kCams - 1, 11.0, 1.0));
    }
    return pr;
}

Vec3 centre(const BAProblem& P, uint32_t img) {
    const double* q = &P.poses[6 * (size_t)P.image_frame[img]];
    return cameraCenter(Pose{angleAxisToRotation({q[0], q[1], q[2]}), {q[3], q[4], q[5]}});
}
double centreX(const BAProblem& P, uint32_t img) { return centre(P, img).x; }

SolverOptions chainOptions(RealCfg real, int device) {
    SolverOptions o;
    o.real = real;
    o.device = device;
    o.loss = "trivial";
    o.solver = SolverSel::Dense;
    o.verbose = false;
    o.rtol = 1e-4;
    o.patience = 5;
    o.max_iters = 25;
    o.init_damping = 1e-2;
    return o;
}

struct Run {
    double x11 = 0;
    SolverStats st;
    RealCfg ran = RealCfg::CPU;
    double prior_at_end = 0;  // the priors' cost recomputed at the solved parameters
};

Run solveChain(const PosePriors& pr, const SolverOptions& o) {
    BAProblem P = convergedChain();
    P.priors = &pr;
    BundleSolver s(P, o);
    s.init();
    s.solve();
    s.downloadParams();
    PriorAssembler pa;
    pa.init(P);
    return {centreX(P, kCams - 1), s.stats(), s.real(), pa.cost(P, P.poses.data(), P.exts.data())};
}

void describe(const char* what, const Run& r) {
    std::printf("  [%s] x11 %.4f, %d iterations, %d prior steps, prior %.4f -> %.4f, "
                "damping %.1e, cost %.3f -> %.3f\n",
                what, r.x11, r.st.iterations, r.st.prior_steps, r.st.prior_initial,
                r.st.prior_final, r.st.final_damping, r.st.initial_cost, r.st.final_cost);
}

// T-C1 and T-C2 on one solver path; returns the follow run for cross-path identity.
Run testPath(RealCfg real, int device, const char* path) {
    std::printf("%s path\n", path);
    const PosePriors pr = chainPriors(true);
    const SolverOptions base = chainOptions(real, device);
    const double x0 = centreX(convergedChain(), kCams - 1);

    const Run follow = solveChain(pr, base);
    describe("follow", follow);
    // Exact null modes converge slowly at the damping floor (a step that cuts the
    // prior 1e-4 of its total is still a tie): three quarters of the gap, not all of it.
    const double closed = (follow.x11 - x0) / (11.0 - x0);
    check(closed > 0.75 && follow.st.prior_steps >= 3 && follow.st.iterations <= 25,
          std::string(path) + " soft mode: the prior is followed");

    SolverOptions frozen = base;
    frozen.prior_patience = 0;
    const Run tie = solveChain(pr, frozen);
    describe("prior_patience 0", tie);
    check((tie.x11 - x0) / (11.0 - x0) < 0.1 && closed > 0.75,
          std::string(path) + " soft mode: the fixture has teeth");

    check(follow.st.prior_final < follow.st.prior_initial &&
              std::fabs(follow.st.prior_final - follow.prior_at_end) <= 1e-6 * follow.prior_at_end &&
              follow.st.final_damping <= base.init_damping,
          std::string(path) + " stats: prior cost is monotone");

    const PosePriors rel = chainPriors(false);
    const Run gate = solveChain(rel, base);
    describe("gate", gate);
    check(gate.st.prior_steps == 0 && (kGateIterations < 0 || gate.st.iterations == kGateIterations),
          std::string(path) + " gate: no absolute centres, no extra steps");

    // T-C2. The design's unreachable target: camera 11 also pinned where it starts.
    PosePriors pinned = pr;
    pinned.centres.push_back(absolute(kCams - 1, centre(convergedChain(), kCams - 1), 0.01));
    SolverOptions pz = base;
    pz.prior_patience = 0;
    pz.max_iters = 200;
    SolverOptions pp = pz;
    pp.prior_patience = base.prior_patience;
    const Run pin0 = solveChain(pinned, pz), pin = solveChain(pinned, pp);
    describe("pinned, prior_patience 0", pin0);
    describe("pinned", pin);
    check(pin.st.iterations <= pin0.st.iterations + pp.prior_patience,
          std::string(path) + " prior_patience bounds the extra iterations (pinned)");

    // ... and a bound that must bind: two prior steps allowed on a target 1.5 m off.
    SolverOptions two = base;
    two.prior_patience = 2;
    two.max_iters = 200;
    SolverOptions none = two;
    none.prior_patience = 0;
    const Run r2 = solveChain(pr, two), r0 = solveChain(pr, none);
    describe("prior_patience 2", r2);
    check(r2.st.prior_steps == 2 && r2.st.iterations <= r0.st.iterations + 2 &&
              (r2.x11 - x0) / (11.0 - x0) < 0.75,
          std::string(path) + " prior_patience bounds the extra iterations (binding)");

    SolverOptions deaf = base;
    deaf.prior_rtol = 1.0;
    const Run d = solveChain(pr, deaf);
    describe("prior_rtol 1", d);
    check(d.st.prior_steps == 0 && std::fabs(d.x11 - tie.x11) < 1e-9,
          std::string(path) + " prior_rtol: a step must cut the prior to count");
    return follow;
}

int run(int argc, char** argv) {
    int device = -1;
    RealCfg real = RealCfg::F64;
    bool gpu = true;
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        if (a == "--device" && i + 1 < argc) device = std::stoi(argv[++i]);
        else if (a == "--real" && i + 1 < argc) real = realCfgFromName(argv[++i]);
        else if (a == "--no-gpu") gpu = false;
    }
    {
        const BAProblem P = convergedChain();
        std::printf("fixture: reprojections converged with camera 11 at x = %.6f\n",
                    centreX(P, kCams - 1));
    }
    const Run cpu = testPath(RealCfg::CPU, device, "cpu");
    if (gpu) {
        const Run dev = testPath(real, device, "device");
        if (dev.ran == RealCfg::CPU) {
            std::printf("  (no device runs '%s'; the device path was not exercised)\n",
                        realCfgName(real));
        } else if (dev.ran == RealCfg::F32) {
            std::printf("  (float: identity with the host is not asserted)\n");
        } else {
            check(dev.st.iterations == cpu.st.iterations &&
                      dev.st.prior_steps == cpu.st.prior_steps &&
                      std::fabs(dev.x11 - cpu.x11) < 1e-3,
                  "both solvers: the same steps");
        }
    }
    std::printf("%s\n", fails ? "FAIL" : "PASS");
    return fails;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
