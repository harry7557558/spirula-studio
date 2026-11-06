#include "engine/Engine.h"
#include "external/stb_image_write.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace {
TorchTensorView tensor(const std::vector<float>& v, std::vector<int64_t> shape) {
    return {(uint64_t)v.data(), 4, std::move(shape)};
}

void run(CacheMode mode, const std::filesystem::path& root) {
    engine_reset();
    std::vector<float> mean{0,0,3}, quat{1,0,0,0}, scale{-1,-1,-1}, opacity{0}, color{.5f,.5f,.5f}, sh(3,0);
    set_data_3dgs(1, tensor(mean,{1,3}), tensor(quat,{1,4}), tensor(scale,{1,3}),
        tensor(opacity,{1,1}), tensor(color,{1,3}), tensor(sh,{1,1,3}), 0);
    constexpr int N = 7;
    std::vector<std::string> images;
    std::vector<int32_t> widths, heights, indices(N);
    std::vector<float> viewmats(16*N,0), intrinsics(4*N), distortion(8*N,0);
    std::iota(indices.begin(),indices.end(),0);
    for (int i = 0; i < N; ++i) {
        int w = 24 + 4*(i%2), h = 20;
        widths.push_back(w); heights.push_back(h);
        std::vector<uint8_t> rgb(w*h*3, uint8_t(20+31*i));
        auto path = root / (std::to_string(i)+".png");
        if (!stbi_write_png(path.string().c_str(), w, h, 3, rgb.data(), 3*w))
            throw std::runtime_error("could not write eval fixture");
        images.push_back(path.string());
        for (int k = 0; k < 4; ++k) viewmats[16*i+5*k] = 1;
        intrinsics[4*i] = intrinsics[4*i+1] = 20;
        intrinsics[4*i+2] = .5f*w; intrinsics[4*i+3] = .5f*h;
    }
    DataManagerConfig cfg;
    cfg.cache_mode = mode;
    cfg.load_masks = cfg.load_depths = cfg.load_normals = false;
    cfg.seed = 123;
    engine_setup_data_manager(cfg, std::vector<int32_t>(N,0), std::vector<int32_t>(N,0),
        images, {}, {}, {}, widths, heights, {}, {}, viewmats, intrinsics, distortion,
        {}, {}, {}, {}, {}, {}, {}, indices, {});
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < N; ++i) {
            if (engine_eval_forward(i,"3dgs",0,false) != 1)
                throw std::runtime_error("eval must render exactly the requested input image");
            auto shape = engine_get_gt_rgb_shape();
            if (std::get<1>(shape) != heights[i] || std::get<2>(shape) != widths[i])
                throw std::runtime_error("eval used another input image's dimensions");
            std::vector<float> gt(widths[i]*heights[i]*3);
            engine_copy_gt_rgb_to_host({(uint64_t)gt.data(),4,{1,heights[i],widths[i],3}});
            for (float value : gt)
                if (std::abs(value - float(20+31*i)/255.0f) > 1e-6f)
                    throw std::runtime_error("eval repeated or skipped an input image");
        }
    }
    engine_reset();
}
}

int main() {
    auto root = std::filesystem::temp_directory_path() /
        ("ss-eval-views-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        run(CacheMode::CPU, root);
        run(CacheMode::DISK, root);
    } catch (const std::exception& e) {
        std::fprintf(stderr,"%s\n",e.what());
        std::filesystem::remove_all(root);
        return 1;
    }
    std::filesystem::remove_all(root);
    std::puts("eval_views_once passed");
}
