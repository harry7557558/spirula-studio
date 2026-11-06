#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "sfm/map/Bundle.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;

static double reprojection(const Reconstruction& model) {
    double cost = 0;
    for (const auto& kv : model.points3D) {
        for (const TrackElement& obs : kv.second.track) {
            const Image& im = model.images.at(obs.image_id);
            const Vec2 pixel = model.cameras.at(im.camera_id).project(mul(im.pose.R, kv.second.xyz) + im.pose.t);
            const Vec2 observed = im.points2D[obs.point2D_idx];
            const double dx = pixel.x - observed.x, dy = pixel.y - observed.y;
            cost += dx * dx + dy * dy;
        }
    }
    return cost;
}

static int testFixedRotationBA(int argc, char** argv) {
    const bool gpu = argc > 1 && std::string(argv[1]) == "--vulkan";
    const Camera camera = Camera::defaultFor(1, 1280, 960, 1000);
    const std::vector<Vec3> centres = {{0, 0, 0}, {1, 0, 0}, {0.1, 1, 0.1}, {1, 1, 0.2}};
    Reconstruction initial;
    initial.cameras[1] = camera;
    std::vector<Pose> truth(centres.size());
    for (uint32_t i = 0; i < centres.size(); i++) {
        truth[i].R = angleAxisToRotation({0.02 * i, 0.03 * i, -0.01 * i});
        truth[i].t = mul(truth[i].R, centres[i]) * -1.0;
        Image im;
        im.id = i; im.camera_id = 1; im.registered = true;
        im.pose.R = angleAxisToRotation({0.02 * i + 0.03, 0.03 * i - 0.02, -0.01 * i});
        im.pose.t = truth[i].t + Vec3{0.08 * i, -0.04 * i, 0.03};
        initial.images[i] = im;
    }
    std::mt19937 rng(73);
    std::uniform_real_distribution<double> xy(-1.5, 1.5), depth(4, 8), perturb(-0.08, 0.08);
    for (uint32_t p = 0; p < 100; p++) {
        const Vec3 x{xy(rng), xy(rng), depth(rng)};
        std::vector<TrackElement> track;
        for (uint32_t i = 0; i < centres.size(); i++) {
            Image& im = initial.images.at(i);
            im.points2D.push_back(camera.project(mul(truth[i].R, x) + truth[i].t));
            im.point3D_ids.push_back(kInvalidPoint3D);
            track.push_back({i, p});
        }
        initial.addPoint3D(x + Vec3{perturb(rng), perturb(rng), perturb(rng)}, track);
    }
    PosePriors priors;
    priors.up_w = {0, 1, 0};
    for (uint32_t i = 0; i < centres.size(); i++) {
        PriorUp up;
        up.i = i; up.u = mul(truth[i].R, priors.up_w); up.sigma = 0.05;
        priors.ups.push_back(up);
        PriorCentre centre;
        centre.n = 1; centre.img[0] = i; centre.b = centres[i]; centre.sigma = {0.1, 0.1, 0.1};
        priors.centres.push_back(centre);
    }
    int failures = 0;
    for (const char* method : {"dense", "cg"}) {
        Reconstruction result = initial;
        BundleOptions opt;
        opt.real = gpu ? RealCfg::F32 : RealCfg::CPU;
        opt.solver = method; opt.loss = "trivial"; opt.max_iters = 30;
        opt.fix_rotations = true; opt.priors = &priors; opt.threads = 2;
        const double before = reprojection(result);
        runGlobalBA(result, opt);
        const double after = reprojection(result);
        double rotation_error = 0, translation_change = 0;
        for (const auto& kv : result.images) {
            const Image& old = initial.images.at(kv.first);
            for (int k = 0; k < 9; k++)
                rotation_error = std::max(rotation_error, std::abs(kv.second.pose.R[k] - old.pose.R[k]));
            translation_change = std::max(translation_change, (kv.second.pose.t - old.pose.t).norm());
        }
        const bool fixed = rotation_error < (gpu ? 1e-6 : 1e-12) &&
                           translation_change > 1e-3 && after < before * 0.2;
        opt.fix_rotations = false;
        runGlobalBA(result, opt);
        double released = 0;
        for (const auto& kv : result.images)
            for (int k = 0; k < 9; k++)
                released = std::max(released, std::abs(kv.second.pose.R[k] - initial.images.at(kv.first).pose.R[k]));
        const bool ok = fixed && released > 1e-3;
        std::printf("%s %s fixed %.2g, cost %.3g -> %.3g, released %.3g: %s\n",
                    gpu ? "Vulkan" : "CPU", method, rotation_error, before, after, released,
                    ok ? "PASS" : "FAIL");
        failures += !ok;
    }
    return failures ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, testFixedRotationBA); }
