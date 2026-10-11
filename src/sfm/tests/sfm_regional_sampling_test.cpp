#include "sfm/map/RegionalTransform.h"
#include "sfm/tests/TestMain.h"
#include <chrono>

using namespace sfm;
static void require(bool v, const char* message) { if (!v) throw std::runtime_error(message); }

static Reconstruction fixture(uint32_t model) {
    Reconstruction rec; rec.cameras[0] = Camera::defaultFor(0, 800, 600, 500, CamModel::Pinhole);
    for (uint32_t i = 0; i < 8; ++i) {
        if ((model == 0 && i >= 4) || (model == 2 && i < 4)) continue;
        Image im; im.id = i; im.camera_id = 0; im.registered = true; im.pose.t = {double(i) * -.1, 0, 0}; rec.images[i] = im;
    }
    for (uint32_t seam = 0; seam < 2; ++seam) for (uint32_t k = 0; k < (seam ? 80u : 1000u); ++k) {
        if ((model == 0 && seam) || (model == 2 && !seam)) continue;
        uint64_t id = uint64_t(model + 1) * 10000 + seam * 2000 + k;
        Point3D p; p.xyz = {double(k % 10) * .2 - 1, double((k / 10) % 10) * .2 - 1, 5 + (seam ? .03 * (k % 7) : .1 * (k / 100))};
        for (uint32_t i = seam * 4; i < seam * 4 + 4; ++i) {
            auto& im = rec.images.at(i); uint32_t f = uint32_t(im.points2D.size());
            im.points2D.push_back(rec.cameras.at(0).project(p.xyz + im.pose.t)); im.point3D_ids.push_back(id); p.track.push_back({i, f});
        }
        rec.points3D[id] = p;
    }
    return rec;
}

static int test(int, char**) {
    namespace fs = std::filesystem;
    auto root = fs::temp_directory_path() / ("sfm-sampling-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::vector<Reconstruction> models; for (uint32_t m = 0; m < 3; ++m) models.push_back(fixture(m));
    std::vector<uint64_t> reference;
    for (unsigned repeat = 0; repeat < 2; ++repeat) {
        auto index = regional::buildSharedIndex(3, [&](size_t m) { return models.at(m); }, root / std::to_string(repeat), 1u << 20, 8u << 20, 256);
        size_t weak = 0; std::vector<uint64_t> keys;
        for (const auto& p : index.landmarks) {
            keys.push_back(p.key);
            if (std::any_of(p.members.begin(), p.members.end(), [](const auto& m) { return m.model == 2; })) ++weak;
        }
        std::printf("seam sampling: %zu selected, %zu on weak seam\n", index.landmarks.size(), weak);
        require(index.landmarks.size() <= 256 && weak >= 50, "dense seam starved weak seam of alignment support");
        auto fit = regional::guardedSharedAlignment(index, 50);
        require(fit.pair_count == 2 && !fit.used_uniform, "protected sample did not recover a seam missed by uniform alignment");
        if (repeat) require(keys == reference, "sampling not deterministic"); else reference = keys;
    }
    fs::remove_all(root); std::puts("PASS weak seam coverage, deterministic bounded sampling, connected alignment"); return 0;
}
int main(int argc, char** argv) { return sfmTestMain(argc, argv, test); }
