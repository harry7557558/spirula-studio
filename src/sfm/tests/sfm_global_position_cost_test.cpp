#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>

#include "sfm/ba/CpuCamera.h"
#include "sfm/map/GlobalPositionCost.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static double component(const Vec3& v, int i) { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }
static void addComponent(Vec3& v, int i, double x) {
    if (i == 0) v.x += x;
    else if (i == 1) v.y += x;
    else v.z += x;
}

static int testGlobalPositionCost(int, char**) {
    std::mt19937 rng(8);
    std::uniform_real_distribution<double> dist(-3, 3), positive(0.1, 3);
    double worst_jacobian = 0, worst_gauge = 0, worst_huber = 0;
    constexpr double h = 1e-6;
    for (int sample = 0; sample < 300; sample++) {
        const Vec3 ray = Vec3{dist(rng), dist(rng), dist(rng)}.normalized();
        const Vec3 point{dist(rng), dist(rng), dist(rng)};
        const Vec3 camera{dist(rng), dist(rng), dist(rng)};
        const double scale = positive(rng);
        const auto lin = globalPositionLinearize(ray, point, camera, scale);
        for (int variable = 0; variable < 7; variable++) {
            Vec3 p_plus = point, p_minus = point, c_plus = camera, c_minus = camera;
            double s_plus = scale, s_minus = scale;
            if (variable < 3) {
                addComponent(p_plus, variable, h); addComponent(p_minus, variable, -h);
            } else if (variable < 6) {
                addComponent(c_plus, variable - 3, h); addComponent(c_minus, variable - 3, -h);
            } else {
                s_plus += h; s_minus -= h;
            }
            const Vec3 plus = globalPositionLinearize(ray, p_plus, c_plus, s_plus).residual;
            const Vec3 minus = globalPositionLinearize(ray, p_minus, c_minus, s_minus).residual;
            for (int axis = 0; axis < 3; axis++) {
                const double numeric = (component(plus, axis) - component(minus, axis)) / (2 * h);
                const double analytic = variable < 3 ? (variable == axis ? lin.point_diagonal : 0) :
                    variable < 6 ? (variable - 3 == axis ? lin.camera_diagonal : 0) :
                                   component(lin.scale_column, axis);
                worst_jacobian = std::max(worst_jacobian, std::fabs(numeric - analytic));
            }
        }
        const Vec3 shift{dist(rng), dist(rng), dist(rng)};
        const double factor = positive(rng);
        const Vec3 changed = globalPositionLinearize(ray, (point + shift) * factor,
                                                      (camera + shift) * factor,
                                                      scale / factor).residual;
        worst_gauge = std::max(worst_gauge, (changed - lin.residual).norm());
        const double squared = lin.residual.dot(lin.residual);
        const double delta = 0.5;
        const double dh = 1e-6;
        if (squared > dh && std::fabs(squared - delta * delta) > dh) {
            const double numerical = (bacpu::HuberLoss::cost(squared + dh, delta) -
                                      bacpu::HuberLoss::cost(squared - dh, delta)) / (2 * dh);
            worst_huber = std::max(worst_huber,
                                   std::fabs(numerical - bacpu::HuberLoss::weight(squared, delta)));
        }
    }
    std::printf("jacobian: %.3g, gauge: %.3g, Huber derivative: %.3g\n",
                worst_jacobian, worst_gauge, worst_huber);
    return worst_jacobian < 1e-8 && worst_gauge < 1e-12 && worst_huber < 1e-8 ? 0 : 1;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testGlobalPositionCost); }
