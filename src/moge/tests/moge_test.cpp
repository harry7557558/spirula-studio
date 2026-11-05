// MoGe checkpoint and forward-pass gate: shapes, a finite prediction, and
// the shift solve against an analytic plane. It cannot check the network's
// numbers -- we do not own these weights and cannot embed a golden copy. That
// is tools/moge/compare_ort.py's job (and check_refiner.py's for MoGe-3).
//
//   ./build_vulkan/moge_test                        # cached checkpoints, or SKIP
//   ./build_vulkan/moge_test --fetch                # download them first
//   ./build_vulkan/moge_test --model M --image IMG.jpg [--num-tokens N] [--repeat N]
//       [--refine-steps N]
//   SS_MOGE_DUMP=/tmp/ours ./build_vulkan/moge_test --model M --image IMG.jpg

#include "moge/Moge.h"
#include "moge/model/Fetch.h"
#include "moge/model/Recover.h"

#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "nn/io/Image.h"
#include "nn/vk/Stream.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

// Nearest-neighbour on purpose: this is a test harness, and a resampling
// difference against the reference would be indistinguishable from a model
// difference.
std::vector<float> load_scaled(const std::string& path, int max_size, int& w, int& h) {
    const nn::Image img = nn::load_image(path);
    NN_CHECK(!img.empty(), "cannot read '%s'", path.c_str());
    double s = 1.0;
    if (max_size > 0 && std::max(img.width, img.height) > max_size)
        s = (double)max_size / std::max(img.width, img.height);
    w = std::max(14, (int)(img.width * s));
    h = std::max(14, (int)(img.height * s));

    std::vector<float> out((size_t)w * h * 3);
    for (int y = 0; y < h; ++y) {
        const int sy = std::min(img.height - 1, (int)((y + 0.5) / h * img.height));
        for (int x = 0; x < w; ++x) {
            const int sx = std::min(img.width - 1, (int)((x + 0.5) / w * img.width));
            const uint8_t* p = &img.data[((size_t)sy * img.width + sx) * 3];
            for (int c = 0; c < 3; ++c)
                out[((size_t)y * w + x) * 3 + c] = p[c] * (1.0f / 255.0f);
        }
    }
    return out;
}

// An analytic plane through a pinhole, handed to the solver with its z shifted
// away. The one part of the pipeline whose answer is knowable without the
// network, and getting it wrong is a depth map that looks plausible and is bent.
void test_recover() {
    const int W = 640, H = 480;
    const double diag = std::sqrt((double)W * W + (double)H * H);
    const double fx = 520.0;
    const double focal = 2.0 * fx / diag;
    const double plane[3] = {0.31, -0.22, 0.925};   // not axis-aligned
    const double dist = 3.5;

    for (double shift : {0.0, 1.7, -0.6}) {
        std::vector<float> p, uv;
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x) {
                const double u = ((x + 0.5) / 64 * W - W * 0.5) / fx;
                const double v = ((y + 0.5) / 64 * H - H * 0.5) / fx;
                const double z = dist / (plane[0] * u + plane[1] * v + plane[2]);
                p.push_back((float)(u * z));
                p.push_back((float)(v * z));
                p.push_back((float)(z - shift));
                uv.push_back((float)(2.0 * ((x + 0.5) / 64 * W - W * 0.5) / diag));
                uv.push_back((float)(2.0 * ((y + 0.5) / 64 * H - H * 0.5) / diag));
            }
        const int n = (int)(p.size() / 3);

        const moge::Recovered known = moge::recover_shift(p.data(), uv.data(), n,
                                                          (float)focal);
        const moge::Recovered free_ = moge::recover_shift(p.data(), uv.data(), n, 0.0f);
        char msg[128];
        std::snprintf(msg, sizeof msg,
                      "shift %.2f recovered %.4f (known focal) / %.4f at focal %.4f",
                      shift, (double)known.shift, (double)free_.shift,
                      (double)free_.focal);
        check(std::fabs(known.shift - shift) < 1e-3 * dist &&
                  std::fabs(free_.shift - shift) < 1e-2 * dist &&
                  std::fabs(free_.focal - focal) < 1e-3 * focal,
              msg);
    }
}

void run_one(const std::string& model, const std::string& image, int max_size,
             int num_tokens, int repeat, int refine_steps) {
    std::printf("%s\n", model.c_str());
    const double t0 = nn::now_ms();
    moge::Predictor pred;
    pred.load(model);
    check(pred.loaded(), "checkpoint loads");
    std::printf("  weights %.1f MiB, loaded in %.2f s\n",
                (double)pred.deviceBytes() / 1048576.0, (nn::now_ms() - t0) / 1000.0);
    if (image.empty()) return;

    int w = 0, h = 0;
    const std::vector<float> rgb = load_scaled(image, max_size, w, h);
    std::printf("  %s at %dx%d, %d tokens\n", image.c_str(), w, h, num_tokens);

    moge::PredictOptions po;
    po.num_tokens = num_tokens;
    po.want_mask = true;
    po.refine_steps = refine_steps;
    moge::Prediction out = pred.predict(rgb.data(), w, h, po);
    // The first call builds pipelines and measures the GEMM tiling; the rate a
    // dataset actually runs at is the second one onwards.
    for (int i = 1; i < repeat; ++i) {
        const double t = nn::now_ms();
        out = pred.predict(rgb.data(), w, h, po);
        std::printf("  predict %.0f ms\n", nn::now_ms() - t);
    }
    nn::vk::Stream::get().report();   // per-kernel times under SS_PROFILE=1

    check(out.width == w && out.height == h, "prediction comes back at the input size");
    check((size_t)out.depth.size() == (size_t)w * h, "depth is full size");
    check((size_t)out.normal.size() == (size_t)w * h * 3, "normal is full size");
    check((size_t)out.mask.size() == (size_t)w * h, "mask is full size");

    double dmin = 1e30, dmax = -1e30, nmin = 1e30, nmax = -1e30;
    int64_t masked = 0;
    bool finite = true;
    for (float v : out.depth) {
        finite &= std::isfinite(v);
        if (v <= 0.0f) { ++masked; continue; }
        dmin = std::min(dmin, (double)v);
        dmax = std::max(dmax, (double)v);
    }
    for (size_t i = 0; i + 2 < out.normal.size(); i += 3) {
        const double n = std::sqrt((double)out.normal[i] * out.normal[i] +
                                   (double)out.normal[i + 1] * out.normal[i + 1] +
                                   (double)out.normal[i + 2] * out.normal[i + 2]);
        finite &= std::isfinite(n);
        // Zero is the "no ground truth here" sentinel, not a broken normal.
        if (n < 0.5) continue;
        nmin = std::min(nmin, n);
        nmax = std::max(nmax, n);
    }
    check(finite, "every output is finite");
    check(masked < (int64_t)w * h, "the mask kept something");
    check(dmin > 0.0 && dmax < 1e4, "depth is a plausible number of metres");
    check(nmin > 0.99 && nmax < 1.01, "kept normals are unit length");
    check(out.metric_scale > 1e-4f && out.metric_scale < 1e4f, "the metric scale is sane");
    std::printf("  depth %.3f..%.3f m, scale %.4f, shift %.4f, focal %.4f, %.1f%% masked\n",
                dmin, dmax, (double)out.metric_scale, (double)out.shift, (double)out.focal,
                100.0 * (double)masked / ((double)w * h));
}

}  // namespace

int main(int argc, char** argv) {
    std::string model, image;
    int max_size = 0, num_tokens = 1800, repeat = 1, refine_steps = 3;
    bool fetch = false;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--model" && i + 1 < argc) model = argv[++i];
        else if (a == "--image" && i + 1 < argc) image = argv[++i];
        else if (a == "--max-size" && i + 1 < argc) max_size = std::atoi(argv[++i]);
        else if (a == "--num-tokens" && i + 1 < argc) num_tokens = std::atoi(argv[++i]);
        else if (a == "--repeat" && i + 1 < argc) repeat = std::atoi(argv[++i]);
        else if (a == "--refine-steps" && i + 1 < argc) refine_steps = std::atoi(argv[++i]);
        else if (a == "--fetch") fetch = true;
        else {
            std::fprintf(stderr, "unknown argument '%s'\n", a.c_str());
            return 2;
        }
    }

    try {
        std::printf("shift recovery\n");
        test_recover();

        if (!model.empty()) {
            run_one(model, image, max_size, num_tokens, repeat, refine_steps);
        } else {
            int ran = 0;
            for (const char* id : {"moge2-vits", "moge2-vitb", "moge2-vitl", "moge3-vitl"}) {
                const moge::ModelSource* src = moge::find_model_source(id);
                if (!src) continue;
                std::error_code ec;
                if (!fetch && !fs::exists(nn::cached_path(src->checkpoint), ec)) continue;
                run_one(id, image, max_size, num_tokens, repeat, refine_steps);
                ++ran;
            }
            if (ran == 0)
                std::printf("SKIP: no cached checkpoint (--fetch downloads them, or "
                            "--model <path.onnx|.pt>)\n");
        }
    } catch (const std::exception& e) {
        std::fprintf(stderr, "\nerror: %s\n", e.what());
        return 1;
    }

    std::printf("\n%s\n", g_failures ? "FAILURES" : "all checks passed");
    return g_failures ? 1 : 0;
}
