// LPIPS against torchmetrics (tools/lpips/compare_torchmetrics.py --golden made
// kGolden, on the CPU in fp32), and tiled against untiled.
//
// Needs the four checkpoints. With them not cached and no --fetch it SKIPS:
// a build machine without network access must still run the suite.
//
//   lpips_test                        # the model cache
//   lpips_test --fetch                # download what is missing
//   lpips_test <dir>                  # a folder holding them (torch's hub cache)
//   lpips_test --pair gt.png render.png [<dir>]

#include "lpips/Fetch.h"
#include "lpips/Lpips.h"
#include "nn/Device.h"
#include "nn/core/Error.h"
#include "nn/io/Image.h"

#include <algorithm>
#include <cstdarg>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <utility>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what.c_str());
    g_failures += !ok;
}

struct Golden {
    int w, h;
    const char* net;
    bool normalize;
    double want[2];   // the noisy view, the blended one
};

// torchmetrics 1.9.0, torch 2.0.1 + torchvision 0.15.2.
const Golden kGolden[] = {
    {64, 48, "alex", true, {0.012999624, 0.0185664855}},
    {64, 48, "vgg", false, {0.0480858684, 0.0412227884}},
    {64, 48, "vgg", true, {0.0524003208, 0.0477415808}},
    {333, 257, "alex", true, {0.0285698604, 0.0284520593}},
    {333, 257, "vgg", false, {0.0506419875, 0.0421764925}},
    {333, 257, "vgg", true, {0.0592096411, 0.0584532954}},
    {1000, 700, "alex", true, {0.0306863822, 0.0291114412}},
    {1000, 700, "vgg", false, {0.0514624529, 0.0413261428}},
    {1000, 700, "vgg", true, {0.0581083149, 0.0681748688}},
};

// Must match compare_torchmetrics.py's hash_u.
double uniform(uint32_t x, uint32_t y, uint32_t c, uint32_t seed) {
    uint32_t h = (x * 73856093u) ^ (y * 19349663u) ^ (c * 83492791u) ^ (seed * 2654435761u);
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return (double)(h >> 8) / 16777216.0;
}

struct Views {
    std::vector<float> ref, noisy, blend;
};

// compare_torchmetrics.py's views().
Views make_views(int w, int h) {
    Views v;
    const size_t n = (size_t)w * h * 3;
    v.ref.resize(n);
    v.noisy.resize(n);
    v.blend.resize(n);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c) {
                const double pattern = ((x * 7 + y * 13 + c * 29) % 256) / 255.0;
                v.ref[((size_t)y * w + x) * 3 + c] =
                    (float)(0.6 * pattern + 0.4 * uniform(x, y, c, 1));
            }
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (int c = 0; c < 3; ++c) {
                const size_t i = ((size_t)y * w + x) * 3 + c;
                const double r = v.ref[i];
                v.noisy[i] = (float)std::clamp(r + 0.3 * (uniform(x, y, c, 2) - 0.5), 0.0, 1.0);
                const double s = v.ref[((size_t)y * w + std::min(x + 3, w - 1)) * 3 + c];
                v.blend[i] = (float)(0.7 * r + 0.3 * s);
            }
    return v;
}

bool close(double got, double want, double rel) {
    return std::fabs(got - want) <= rel * std::fabs(want) + 1e-7;
}

std::string fmt(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

lpips::Net net_of(const char* name) {
    return std::strcmp(name, "vgg") ? lpips::Net::Alex : lpips::Net::Vgg;
}

void golden(const std::string& dir) {
    lpips::Scorer scorer[2];
    for (lpips::Net net : {lpips::Net::Alex, lpips::Net::Vgg})
        scorer[net == lpips::Net::Vgg].load(net, lpips::resolve(net, dir));

    for (const Golden& g : kGolden) {
        const Views v = make_views(g.w, g.h);
        lpips::Scorer& s = scorer[net_of(g.net) == lpips::Net::Vgg];
        const auto t0 = std::chrono::steady_clock::now();
        const std::vector<double> got =
            s.score(v.ref.data(), {v.noisy.data(), v.blend.data()}, g.w, g.h, g.normalize);
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        for (int k = 0; k < 2; ++k)
            check(close(got[k], g.want[k], 2e-5),
                  fmt("%dx%d %s normalize=%d view %d: %.9g, torchmetrics %.9g (%.0f ms)", g.w, g.h,
                      g.net, (int)g.normalize, k, got[k], g.want[k], ms));

        // Small tiles, so a 1000x700 view is many of them with a 96-pixel halo.
        lpips::ScoreOptions tiled;
        tiled.tile = g.w > 500 ? 256 : 64;
        const std::vector<double> part =
            s.score(v.ref.data(), {v.noisy.data(), v.blend.data()}, g.w, g.h, g.normalize, tiled);
        for (int k = 0; k < 2; ++k)
            check(close(part[k], got[k], 2e-6),
                  fmt("%dx%d %s view %d in %d-pixel tiles: %.9g, whole %.9g", g.w, g.h, g.net, k,
                      tiled.tile, part[k], got[k]));
    }

    // The fit decides tiles on its own when the workspace is small.
    const Views v = make_views(1000, 700);
    lpips::ScoreOptions tight;
    tight.workspace_bytes = 300ull << 20;
    const double whole = scorer[1].score(v.ref.data(), {v.noisy.data()}, 1000, 700, false)[0];
    const double fit = scorer[1].score(v.ref.data(), {v.noisy.data()}, 1000, 700, false, tight)[0];
    check(close(fit, whole, 2e-6), fmt("vgg in a 300 MiB workspace: %.9g, whole %.9g", fit, whole));

    bool refused = false;
    try {
        const Views tiny = make_views(24, 24);
        scorer[0].score(tiny.ref.data(), {tiny.noisy.data()}, 24, 24, true);
    } catch (const nn::Error&) {
        refused = true;
    }
    check(refused, "a view smaller than AlexNet's receptive field is refused");
}

std::vector<float> to_float(const nn::Image& img) {
    std::vector<float> out(img.data.size());
    for (size_t i = 0; i < out.size(); ++i) out[i] = img.data[i] / 255.0f;
    return out;
}

int pairs(const std::vector<std::pair<const char*,const char*>>& paths,
          const std::string& dir, bool alex_only) {
    for (lpips::Net net : {lpips::Net::Alex,lpips::Net::Vgg}) {
        if (alex_only && net != lpips::Net::Alex) continue;
        lpips::Scorer scorer;
        scorer.load(net,lpips::resolve(net,dir));
        for (const auto& [gt_path,render_path] : paths) {
            const nn::Image gt = nn::load_image(gt_path), render = nn::load_image(render_path);
            if (gt.empty() || render.empty() || gt.width != render.width || gt.height != render.height) {
                std::printf("FAIL cannot read two images of one size\n");
                return 1;
            }
            const std::vector<float> a = to_float(gt), b = to_float(render);
            double value = scorer.score(a.data(),{b.data()},gt.width,gt.height,net == lpips::Net::Alex)[0];
            if (paths.size() == 1) std::printf("lpips_%s: %.9g\n",lpips::net_name(net),value);
            else std::printf("lpips_%s\t%s\t%s\t%.9g\n",lpips::net_name(net),gt_path,render_path,value);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    bool fetch = false, alex_only = false;
    std::string dir;
    std::vector<std::pair<const char*,const char*>> pair_args;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--fetch")) fetch = true;
        else if (!std::strcmp(argv[i], "--alex-only")) alex_only = true;
        else if (!std::strcmp(argv[i], "--pair") && i + 2 < argc) {
            pair_args.emplace_back(argv[i + 1],argv[i + 2]);
            i += 2;
        } else dir = argv[i];
    }
    int rc = 0;
    try {
        if (dir.empty() && !fetch &&
            !lpips::missing_files({lpips::Net::Alex, lpips::Net::Vgg}).empty()) {
            std::printf("SKIP the LPIPS checkpoints are not cached (--fetch, or a folder)\n");
        } else if (!pair_args.empty()) {
            rc = pairs(pair_args,dir,alex_only);
        } else {
            golden(dir);
        }
    } catch (const std::exception& e) {
        std::printf("FAIL %s\n", e.what());
        ++g_failures;
    }
    nn::shutdown();
    return rc ? rc : (g_failures ? 1 : 0);
}
