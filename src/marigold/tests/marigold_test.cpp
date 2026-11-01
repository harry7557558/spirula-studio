// Marigold v2: the installer's host arithmetic, the installed files against the
// reference's constants, and a prediction with its stages dumped for
// tools/marigold/reference.py. Runs what it can and SKIPs the rest.
//
//   marigold_test                           host checks; a small prediction if installed
//   marigold_test --install depth,normals   fetch and convert (~43 GB streamed)
//   marigold_test --constants <ref dir>     installed constants vs a reference run's dumps
//   marigold_test --image IMG|--rgb X.npy [--task depth|normals|both] [--tokens N]
//                 [--repeat N] [--batch N] [--out DIR]
//                                           a prediction; SS_MARIGOLD_DUMP=<dir> for stages

#include "marigold/Common.h"
#include "marigold/Marigold.h"
#include "marigold/model/Fetch.h"
#include "marigold/model/HostMath.h"
#include "marigold/model/Install.h"
#include "marigold/model/Layout.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"
#include "nn/Device.h"
#include "nn/io/Image.h"
#include "nn/io/Safetensors.h"
#include "nn/io/StageDump.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <string>
#include <vector>

using namespace marigold;

namespace {

int g_failures = 0;

void check(bool ok, const char* what) {
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failures;
}

void test_host_math() {
    std::printf("Host arithmetic\n");
    check(f32_to_bf16(1.0f) == 0x3f80 && bf16r(1.00390625f) == 1.0f &&
              bf16r(1.01171875f) == 1.015625f,
          "bf16 rounds to nearest even");
    // The one timestep: bf16(499) is 500, and 500 / 1000 is exact.
    check(bf16r(bf16r(499.0f) / 1000.0f) == 0.5f, "the timestep is 0.5");

    // NF4: each level decodes to itself times the block's absmax, and an
    // all-zero block stays zero.
    std::vector<uint16_t> w(128);
    std::mt19937 rng(7);
    std::normal_distribution<float> nd(0.0f, 0.02f);
    for (int i = 0; i < 64; ++i) w[(size_t)i] = f32_to_bf16(nd(rng));
    std::vector<uint8_t> codes(64);
    std::vector<uint16_t> absmax(2);
    nf4_quantize(w.data(), 128, codes.data(), absmax.data());
    std::vector<float> back(128);
    nf4_dequantize(codes.data(), absmax.data(), 128, back.data());
    float am = 0.0f, worst = 0.0f;
    for (int i = 0; i < 64; ++i) am = std::fmax(am, std::fabs(bf16_to_f32(w[(size_t)i])));
    for (int i = 0; i < 64; ++i)
        worst = std::fmax(worst, std::fabs(back[(size_t)i] - bf16_to_f32(w[(size_t)i])));
    check(bf16_to_f32(absmax[0]) == am, "nf4 absmax is the block's max");
    // Half the widest gap between levels (0.161 at the ends) is the worst case.
    check(worst <= 0.0811f * am + 1e-6f, "nf4 error within half a level");
    bool zero = true;
    for (int i = 64; i < 128; ++i) zero = zero && back[(size_t)i] == 0.0f;
    check(zero, "nf4 zero block");
}

// The reference's dump `name` against the installed tensor `ours`. What went
// through bitsandbytes' one-row kernel reproduces bit for bit; what went
// through cuBLAS differs by its reduced-precision reductions, a step or two.
void compare(const nn::SafetensorsFile& f, const std::string& ours, const std::string& ref_dir,
             const std::string& name, double within) {
    std::vector<float> want;
    std::vector<int64_t> shape;
    if (!nn::read_npy_f32(ref_dir + "/" + name + ".npy", want, shape)) {
        std::printf("  SKIP %s (no %s.npy)\n", ours.c_str(), name.c_str());
        return;
    }
    const nn::OnnxTensor got = f.read(ours);
    const size_t n = want.size();
    if (got.data.size() < n || want.size() < n) {
        check(false, (ours + ": size differs from the reference").c_str());
        return;
    }
    // bf16-rounded constants: count elements more than one bf16 step apart.
    size_t off = 0;
    double sq = 0, err = 0;
    for (size_t i = 0; i < n; ++i) {
        const float a = got.data[i], b = want[i];
        sq += (double)b * b;
        err += (double)(a - b) * (a - b);
        if (std::fabs(a - b) > std::fabs(b) * (1.0f / 128.0f) + 1e-6f) ++off;
    }
    char line[256];
    std::snprintf(line, sizeof line, "%-22s rel L2 %.2e, %zu of %zu past one bf16 step",
                  ours.c_str(), std::sqrt(err / std::fmax(sq, 1e-30)), off, n);
    check((double)off <= (1.0 - within) * (double)n, line);
}

void test_constants(const std::string& ref, Task t) {
    std::printf("Installed constants vs %s\n", ref.c_str());
    if (!base_installed() || !task_installed(t)) {
        std::printf("  SKIP not installed\n");
        return;
    }
    const nn::SafetensorsFile base(base_path()), task(task_path(t));
    compare(base, "temb", ref, "temb", 0.999);
    std::vector<float> mods;
    std::vector<int64_t> shape;
    for (const char* s : {"img", "txt"})
        if (nn::read_npy_f32(ref + "/mod_" + s + ".npy", mods, shape)) {
            size_t off = 0;
            for (int i = 0; i < kLayers; ++i) {
                const nn::OnnxTensor got =
                    base.read(block_tensor(i, s[0] == 'i' ? "mod_img" : "mod_txt"));
                for (size_t k = 0; k < got.data.size(); ++k) {
                    const float b = mods[(size_t)i * got.data.size() + k];
                    if (std::fabs(got.data[k] - b) > std::fabs(b) / 128.0f + 1e-6f) ++off;
                }
            }
            char line[128];
            std::snprintf(line, sizeof line, "mod_%s: %zu of %zu past one bf16 step", s, off,
                          mods.size());
            check(off <= mods.size() / 100, line);
        }
    compare(task, "txt", ref, "txt0", 0.95);
    compare(task, "norm_out", ref, "norm_out", 0.95);
}

void save_outputs(const Prediction& p, const std::string& dir) {
    if (!p.depth_log.empty()) {
        float lo = 1e9f, hi = -1e9f;
        for (float v : p.depth_log) lo = std::fmin(lo, v), hi = std::fmax(hi, v);
        nn::Image img;
        img.width = p.width;
        img.height = p.height;
        img.data.resize((size_t)p.width * p.height * 3);
        for (size_t i = 0; i < p.depth_log.size(); ++i)
            img.data[i * 3] = img.data[i * 3 + 1] = img.data[i * 3 + 2] =
                (uint8_t)std::lround(255.0f * (p.depth_log[i] - lo) / std::fmax(hi - lo, 1e-6f));
        nn::save_image(img, dir + "/depth.png", -1);
    }
    if (!p.normal.empty()) {
        nn::Image img;
        img.width = p.width;
        img.height = p.height;
        img.data.resize(p.normal.size());
        // The usual picture of OpenCV normals: x right red, y up green, z out blue.
        for (size_t i = 0; i * 3 < p.normal.size(); ++i) {
            const float v[3] = {p.normal[i * 3], -p.normal[i * 3 + 1], -p.normal[i * 3 + 2]};
            for (int c = 0; c < 3; ++c)
                img.data[i * 3 + c] = (uint8_t)std::lround(127.5f + 127.5f * v[c]);
        }
        nn::save_image(img, dir + "/normal.png", -1);
    }
}

int run_prediction(const std::string& image, const std::string& npy, const std::string& tasks,
                   int tokens, int repeat, int batch, const std::string& out,
                   bool synthetic = false) {
    std::printf("Prediction\n");
    const bool d = tasks.find("depth") != std::string::npos || tasks == "both";
    const bool n = tasks.find("normal") != std::string::npos || tasks == "both";
    if (!base_installed() || (d && !task_installed(Task::Depth)) ||
        (n && !task_installed(Task::Normals))) {
        std::printf("  SKIP not installed\n");
        return 0;
    }
    int w = 0, h = 0;
    std::vector<float> rgb;
    if (synthetic) {
        // A lit box in a corridor: enough structure for a finite, oriented answer.
        w = 256;
        h = 192;
        rgb.resize((size_t)w * h * 3);
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const bool box = x > 96 && x < 160 && y > 80 && y < 150;
                const float v = box ? 0.8f : 0.2f + 0.6f * (float)y / (float)h;
                for (int c = 0; c < 3; ++c)
                    rgb[((size_t)y * w + x) * 3 + c] = v * (0.8f + 0.1f * (float)c);
            }
    } else if (!npy.empty()) {
        std::vector<int64_t> shape;
        NN_CHECK(nn::read_npy_f32(npy, rgb, shape) && shape.size() == 3 && shape[2] == 3,
                 "%s: not an [H, W, 3] float32 array", npy.c_str());
        h = (int)shape[0];
        w = (int)shape[1];
    } else {
        const nn::Image img = nn::load_image(image);
        NN_CHECK(!img.empty(), "cannot read %s", image.c_str());
        w = img.width;
        h = img.height;
        rgb.resize(img.data.size());
        for (size_t i = 0; i < rgb.size(); ++i) rgb[i] = img.data[i] / 255.0f;
    }
    Predictor pred;
    double t0 = now_ms();
    pred.load(d, n);
    std::printf("  loaded in %.1f s\n", (now_ms() - t0) / 1000.0);
    PredictOptions o;
    o.want_depth = d;
    o.want_normal = n;
    o.num_tokens = tokens;
    int pw, ph;
    Predictor::processingSize(w, h, tokens, &pw, &ph);
    std::printf("  %dx%d runs at %dx%d (%d tokens), batch capacity %d\n", w, h, pw, ph,
                (pw / kPatch) * (ph / kPatch), pred.batchCapacity(w, h, tokens));
    std::vector<Prediction> p;
    // A batch of mirror images: the same work as distinct ones, and image 0
    // must come back exactly as it does alone.
    std::vector<float> flipped(rgb.size());
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c)
                flipped[((size_t)y * w + x) * 3 + c] = rgb[((size_t)y * w + (w - 1 - x)) * 3 + c];
    std::vector<Predictor::Image> images{{rgb.data(), w, h}};
    for (int b = 1; b < batch; ++b) images.push_back({b % 2 ? flipped.data() : rgb.data(), w, h});
    for (int r = 0; r < repeat; ++r) {
        t0 = now_ms();
        p = pred.predict(images, o);
        std::printf("  predict %.2f s (%.2f s an image)\n", (now_ms() - t0) / 1000.0,
                    (now_ms() - t0) / 1000.0 / batch);
    }
    if (batch > 1) {
        const std::vector<Prediction> one = pred.predict({{rgb.data(), w, h}}, o);
        check(one[0].depth_log == p[0].depth_log && one[0].normal == p[0].normal &&
                  p[2 % batch].depth_log == p[0].depth_log,
              "a batch gives each image what it gets alone");
    }
    std::printf("  device %.2f GB, peak allocation %.2f GB\n", pred.deviceBytes() / 1e9,
                vk::Allocator::get().peakBytes() / 1e9);
    vk::Stream::get().report();
    if (!out.empty()) save_outputs(p[0], out);
    bool finite = true;
    for (float v : p[0].depth_log) finite = finite && std::isfinite(v);
    for (float v : p[0].normal) finite = finite && std::isfinite(v);
    check(finite, "prediction is finite");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    nn::set_log_level(2);
    std::string to_install, constants, image, npy, tasks = "both", out;
    int tokens = 3600, repeat = 1, batch = 1;
    Task task = Task::Depth;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--install" && i + 1 < argc) to_install = argv[++i];
        else if (a == "--constants" && i + 1 < argc) constants = argv[++i];
        else if (a == "--task" && i + 1 < argc) {
            tasks = argv[++i];
            task = tasks == "normals" ? Task::Normals : Task::Depth;
        }
        else if (a == "--image" && i + 1 < argc) image = argv[++i];
        else if (a == "--rgb" && i + 1 < argc) npy = argv[++i];
        else if (a == "--tokens" && i + 1 < argc) tokens = std::atoi(argv[++i]);
        else if (a == "--repeat" && i + 1 < argc) repeat = std::atoi(argv[++i]);
        else if (a == "--batch" && i + 1 < argc) batch = std::max(1, std::atoi(argv[++i]));
        else if (a == "--out" && i + 1 < argc) out = argv[++i];
    }
    try {
        if (!to_install.empty()) {
            const bool d = to_install.find("depth") != std::string::npos;
            const bool n = to_install.find("normal") != std::string::npos;
            const InstallNeeds need = install_needs(d, n);
            std::printf("to stream %.2f GB, to write %.2f GB, %.1f GB free\n",
                        need.download / 1e9, need.disk / 1e9, need.free / 1e9);
            const double t0 = now_ms();
            install(d, n, [&](uint64_t done, uint64_t total) {
                static double last = 0;
                const double now = now_ms();
                if (now - last < 2000 && done < total) return;
                last = now;
                std::printf("\r  %.2f / %.2f GB  %.1f MB/s   ", done / 1e9, total / 1e9,
                            done / 1e3 / std::fmax(now - t0, 1.0));
                std::fflush(stdout);
            }, nullptr);
            std::printf("\ninstalled in %.0f s\n", (now_ms() - t0) / 1000.0);
            return 0;
        }
        test_host_math();
        if (!constants.empty()) test_constants(constants, task);
        if (!image.empty() || !npy.empty())
            run_prediction(image, npy, tasks, tokens, repeat, batch, out);
        else if (constants.empty())
            run_prediction("", "", "both", 192, 1, 2, "", true);
    } catch (const std::exception& e) {
        std::printf("EXCEPTION: %s\n", e.what());
        nn::shutdown();
        return 1;
    }
    nn::shutdown();
    std::printf("%s\n", g_failures ? "FAILURES" : "all passed");
    return g_failures ? 1 : 0;
}
