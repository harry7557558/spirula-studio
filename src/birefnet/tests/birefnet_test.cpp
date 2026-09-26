// BiRefNet gate: a checkpoint loads, a frame runs, the mask is not degenerate.
// The network's numbers are tools/birefnet/compare_ort.py's job -- we do not
// own these weights and cannot embed a golden copy.
//
//   ./build_vulkan/birefnet_test                       # cached checkpoints, or SKIP
//   ./build_vulkan/birefnet_test --model ID|PATH --image IMG [--out mask.png] [--repeat N]
//   ./build_vulkan/birefnet_test --model ID|PATH --npy input.npy --logits out.npy

#include "birefnet/BiRefNet.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "nn/io/StageDump.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    std::string model, image, out, npy_in, npy_out;
    int repeat = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&] { return i + 1 < argc ? std::string(argv[++i]) : std::string(); };
        if (a == "--model") model = next();
        else if (a == "--image") image = next();
        else if (a == "--out") out = next();
        else if (a == "--npy") npy_in = next();
        else if (a == "--logits") npy_out = next();
        else if (a == "--repeat") repeat = std::atoi(next().c_str());
    }
    nn::set_log_level(2);
    try {
        if (model.empty()) {
            for (const char* id : {"birefnet-lite", "birefnet"}) {
                const birefnet::ModelSource* src = birefnet::find_model_source(id);
                if (fs::exists(nn::cached_path(src->file))) { model = id; break; }
            }
            if (model.empty()) {
                std::printf("SKIP: no BiRefNet checkpoint cached\n");
                return 0;
            }
        }
        birefnet::Predictor p;
        p.load(model);
        if (!npy_in.empty()) {
            std::vector<float> chw;
            std::vector<int64_t> shape;
            NN_CHECK(nn::read_npy_f32(npy_in, chw, shape) && shape.size() == 4 &&
                         shape[1] == 3 && shape[2] == shape[3],
                     "%s is not a [1, 3, S, S] float32 array", npy_in.c_str());
            const int S = (int)shape[2];
            std::vector<float> hwc(chw.size());
            for (int c = 0; c < 3; ++c)
                for (int k = 0; k < S * S; ++k) hwc[(size_t)k * 3 + c] = chw[(size_t)c * S * S + k];
            std::vector<float> logits;
            for (int r = 0; r < repeat; ++r) {
                const double t0 = nn::now_ms();
                logits = p.forwardNormalized(hwc, S);
                std::printf("forward %d: %.1f ms\n", r, nn::now_ms() - t0);
            }
            if (!npy_out.empty()) nn::write_npy_f32(npy_out, logits.data(), {1, 1, S, S});
            return 0;
        }
        // Without an image: a bright disc on a dark, textured ground, which any
        // salient-object model should pick out.
        nn::Image img;
        if (!image.empty()) {
            img = nn::load_image(image);
            NN_CHECK(!img.empty(), "cannot read %s", image.c_str());
        } else {
            img.width = 640;
            img.height = 480;
            img.data.resize((size_t)640 * 480 * 3);
            for (int y = 0; y < 480; ++y)
                for (int x = 0; x < 640; ++x) {
                    const bool disc = (x - 320) * (x - 320) + (y - 240) * (y - 240) < 120 * 120;
                    uint8_t* px = &img.data[((size_t)y * 640 + x) * 3];
                    px[0] = disc ? 230 : (uint8_t)(40 + (x * 7 + y * 3) % 23);
                    px[1] = disc ? 60 : (uint8_t)(45 + (x * 5 + y * 11) % 19);
                    px[2] = disc ? 40 : (uint8_t)(50 + (x * 3 + y * 13) % 17);
                }
        }
        std::vector<uint8_t> mask;
        for (int r = 0; r < repeat; ++r) {
            const double t0 = nn::now_ms();
            mask = p.segment(img);
            std::printf("segment %d: %.1f ms\n", r, nn::now_ms() - t0);
        }
        size_t fg = 0;
        for (uint8_t v : mask) fg += v > 127;
        const double frac = (double)fg / mask.size();
        std::printf("foreground %.1f%% of %dx%d\n", 100.0 * frac, img.width, img.height);
        if (!out.empty()) nn::save_gray_png(mask.data(), img.width, img.height, out);
        if (frac <= 0.0 || frac >= 1.0) {
            std::printf("FAIL: the mask is empty or full\n");
            return 1;
        }
        std::printf("ok\n");
    } catch (const std::exception& e) {
        std::printf("FAIL: %s\n", e.what());
        return 1;
    }
    return 0;
}
