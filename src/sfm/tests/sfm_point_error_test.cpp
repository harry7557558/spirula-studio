#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "sfm/Pipeline.h"
#include "sfm/tests/TestMain.h"

using namespace sfm;
namespace fs = std::filesystem;

static int pointErrorTest(int, char**) {
    Reconstruction model;
    model.cameras[1] = Camera::defaultFor(1, 800, 600, 100, CamModel::Pinhole);
    model.cameras[2] = Camera::defaultFor(2, 1920, 1920, 300, CamModel::OpenCVFisheye);
    model.cameras[3] = Camera::defaultFor(3, 3600, 1800, 0, CamModel::Equirect);

    auto image = [&](uint32_t id, uint32_t camera, uint64_t point, Vec2 keypoint) {
        Image im;
        im.id = id;
        im.camera_id = camera;
        im.name = "image" + std::to_string(id) + ".png";
        im.registered = true;
        im.pose = {mat3Identity(), {0, 0, 0}};
        im.points2D = {keypoint};
        im.point3D_ids = {point};
        model.images[id] = std::move(im);
    };
    auto point = [&](uint64_t id, Vec3 xyz, std::vector<TrackElement> track) {
        Point3D p;
        p.xyz = xyz;
        p.track = std::move(track);
        model.points3D[id] = std::move(p);
    };

    image(1, 1, 1, {403, 304});
    image(2, 1, 1, {401, 300});
    image(7, 1, 1, {400, 300});
    model.images[7].pose.t = {0, 0, -10};
    point(1, {0, 0, 5}, {{1, 0}, {2, 0}, {7, 0}});
    image(6, 1, 2, {400, 300});
    point(2, {0, 0, -5}, {{6, 0}});
    point(6, {0, 0, 5}, {});
    image(3, 1, 3, {400, 300});
    point(3, {0, 0, 5}, {{3, 0}});
    Vec3 back = {1, 0, -1};
    Vec2 fish = model.cameras[2].project(back);
    image(4, 2, 4, {fish.x + 2, fish.y});
    point(4, back, {{4, 0}});
    Vec2 sphere = model.cameras[3].project(back);
    image(5, 3, 5, {sphere.x + 3, sphere.y});
    point(5, back, {{5, 0}});

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("sfm-point-error-test-" + std::to_string(stamp));
    std::vector<Reconstruction> models = {std::move(model)};
    models.push_back(models[0]);
    models[1].images[1].points2D[0] = {400, 300};
    writeModels(models, dir, false, {});
    Reconstruction output = Reconstruction::readBinary((dir / "0").string());
    Reconstruction second = Reconstruction::readBinary((dir / "1").string());
    fs::remove_all(dir);

    int fails = 0;
    auto check = [&](uint64_t id, double expected, const char* label) {
        const double value = output.points3D.at(id).error;
        if (!std::isfinite(value) || std::fabs(value - expected) > 1e-8) {
            std::printf("FAIL: %s: got %.9f, expected %.9f\n", label, value, expected);
            fails++;
        }
    };
    check(1, 3, "mean over two observations");
    check(2, -1, "unknown when every observation is behind the camera");
    check(6, -1, "unknown without observations");
    check(3, 0, "exact reprojection");
    check(4, 2, "fisheye behind the image plane");
    check(5, 3, "equirectangular behind the image plane");
    if (std::fabs(second.points3D.at(1).error - 0.5) > 1e-8) {
        std::printf("FAIL: second reconstruction's mean error\n");
        fails++;
    }
    if (!fails) std::printf("PASS: point reprojection errors survive export\n");
    return fails ? 1 : 0;
}

int main(int argc, char** argv) { return sfmTestMain(argc, argv, pointErrorTest); }
