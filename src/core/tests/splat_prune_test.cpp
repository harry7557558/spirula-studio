#include "checkpoint/SplatPrune.h"
#include "checkpoint/SplatPly.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main() {
    const std::vector<float> score = {1.0f, 5.0f, 5.0f, 2.0f, 0.0f};
    const std::vector<uint8_t> half = spirula::select_splats_by_contribution(score, 0.4);
    assert((half == std::vector<uint8_t>{0, 1, 1, 0, 0}));

    const std::vector<uint8_t> all = spirula::select_splats_by_contribution(score, 1.0);
    assert((all == std::vector<uint8_t>{1, 1, 1, 1, 1}));

    const std::vector<float> opacity = {-4.0f, -2.0f, 0.0f, 2.0f};
    const std::vector<uint8_t> opaque = spirula::select_splats_by_opacity(opacity, 0.5f);
    assert((opaque == std::vector<uint8_t>{0, 0, 1, 1}));

    spirula::SplatCloud cloud;
    cloud.num = 2;
    cloud.sh_degree = 2;
    cloud.features_sh.resize(2 * 8 * 3);
    for (size_t i = 0; i < cloud.features_sh.size(); ++i) cloud.features_sh[i] = (float)i;
    spirula::reduce_splat_sh_degree(cloud, 1);
    assert(cloud.sh_degree == 1);
    assert(cloud.features_sh.size() == 2 * 3 * 3);
    for (size_t i = 0; i < 9; ++i) assert(cloud.features_sh[i] == (float)i);
    for (size_t i = 0; i < 9; ++i) assert(cloud.features_sh[9 + i] == (float)(24 + i));

    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "spirula-splat-ply-test";
    fs::remove_all(root);
    fs::create_directories(root / "run");
    std::ofstream(root / "run" / "config.json") << "{}";
    std::ofstream(root / "run" / "compressed.ply") << "ply\n";
    const auto [ply, run] = spirula::find_splat_ply((root / "run" / "compressed.ply").string());
    assert(fs::path(ply) == root / "run" / "compressed.ply");
    assert(fs::path(run) == root / "run");
    fs::remove_all(root);
    return 0;
}
