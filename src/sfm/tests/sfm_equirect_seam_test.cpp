// Equirectangular BA residual periodicity on the host and Vulkan paths.
#include <cmath>
#include <cstdio>

#include "sfm/ba/Problem.h"
#include "sfm/ba/Solver.h"
#include "sfm/tests/TestMain.h"

namespace {

BAProblem makeProblem() {
    BAProblem P;
    P.num_images = 1;
    P.num_points = 2;
    P.num_obs = 2;
    P.poses.assign(6, 0.0);
    P.intr = {640.0, 480.0};
    P.groups = {{0, 6, 0, 9}};
    P.image_group = {0};

    const double d = 2.0 * M_PI * 2.0 / P.intr[0];
    P.points = {std::sin(M_PI - d), 0.0, std::cos(M_PI - d),
                std::sin(-M_PI + d), 0.0, std::cos(-M_PI + d)};
    P.obs_image = {0, 0};
    P.obs_point = {0, 1};
    P.obs_xy = {2.0, 240.0, 638.0, 240.0};
    P.obs_ranges = {0, 1, 2};
    P.pose_dim = 6;
    P.total_intr = 2;
    P.n_dim = 6;
    finalizeTables(P);
    return P;
}

double cost(RealCfg real) {
    BAProblem P = makeProblem();
    SolverOptions opt;
    opt.real = real;
    opt.verbose = false;
    BundleSolver solver(P, opt);
    solver.init();
    return solver.computeCost();
}

int run(int, char**) {
    const double host = cost(RealCfg::CPU);
    const double device = cost(RealCfg::F64);
    const double error = std::max(std::fabs(host - 16.0), std::fabs(device - 16.0));
    std::printf("equirect seam cost: host %.9f, device %.9f\n%s\n", host, device,
                error < 1e-4 ? "PASS" : "FAIL");
    return error < 1e-4 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) { return sfmTestMain(argc, argv, run); }
