// The stochastic renderer must converge to the sorted one: for every primitive,
// several lenses and both estimators, average many frames of a random scene and
// hold rgb, alpha, depth and normal to the regular forward, with the error
// falling as the frames grow (docs/notes/stochastic-splats.md).
//
//   ./stochastic_render        (either backend, no reference file)
//   ./stochastic_render model.ply cameras.json [index] [sorted]
//       times sorted and stochastic frames of a real model at 1280x720 and
//       prints the PSNR of the running average to the sorted render; cameras
//       as hardware_splat_test --compare reads them. `sorted`: sorted only.

#include <checkpoint/SplatPly.h>
#include <data/Json.h>
#include <engine/Engine.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#define SS_SETENV(k, v) _putenv_s(k, v)
#else
#define SS_SETENV(k, v) setenv(k, v, 1)
#endif

static constexpr int64_t N = 3000;
static constexpr int W = 160, H = 120;

static TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
static TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, 4u, std::vector<int64_t>{0});
}

static int g_failures = 0;
static void check(bool ok, const std::string& what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok) g_failures++;
}

struct Frame {
    std::vector<float> rgb, depth, T, normal;
};

static Frame read_back(bool normal) {
    Frame f;
    f.rgb.resize((size_t)H * W * 3);
    f.depth.resize((size_t)H * W);
    f.T.resize((size_t)H * W);
    engine_copy_render_to_host(ttv(f.rgb.data(), {1, H, W, 3}), ttv(f.depth.data(), {1, H, W, 1}),
                               ttv(f.T.data(), {1, H, W, 1}), ttv_null(), ttv_null());
    if (normal) {
        f.normal.assign((size_t)H * W * 3, 0.0f);
        engine_copy_render_normal_to_host(ttv(f.normal.data(), {1, H, W, 3}));
    }
    return f;
}

// Mean |a - b| over rgb, alpha, depth (where both are mostly covered) and normal.
struct Error {
    double rgb = 0, alpha = 0, depth = 0, normal = 0;
};
static Error compare(const Frame& a, const Frame& b) {
    Error e;
    int64_t covered = 0;
    for (int64_t i = 0; i < (int64_t)H * W; i++) {
        for (int c = 0; c < 3; c++) e.rgb += std::abs(a.rgb[3 * i + c] - b.rgb[3 * i + c]);
        e.alpha += std::abs(a.T[i] - b.T[i]);
        if (a.T[i] < 0.5f && b.T[i] < 0.5f) {
            e.depth += std::abs(a.depth[i] - b.depth[i]) / std::max(a.depth[i], 1e-3f);
            covered++;
        }
        if (!a.normal.empty())
            for (int c = 0; c < 3; c++)
                e.normal += std::abs(a.normal[3 * i + c] - b.normal[3 * i + c]);
    }
    e.rgb /= 3.0 * H * W;
    e.alpha /= (double)H * W;
    e.depth /= (double)std::max<int64_t>(covered, 1);
    e.normal /= 3.0 * H * W;
    return e;
}

static double ms_since(std::chrono::steady_clock::time_point t0) {
    return 1e3 * std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
}

static int bench(const char* ply, const char* cameras, int index, bool sorted_only) {
    const int BW = 1280, BH = 720;
    const spirula::SplatCloud c = spirula::read_splat_ply(ply);
    engine_reset();
    set_data_3dgs(c.num, ttv(c.means.data(), {c.num, 3}), ttv(c.quats.data(), {c.num, 4}),
                  ttv(c.scales.data(), {c.num, 3}), ttv(c.opacities.data(), {c.num, 1}),
                  ttv(c.features_dc.data(), {c.num, 3}),
                  ttv(c.features_sh.data(), {c.num, c.coeffs(), 3}), c.sb_lobes);
    const auto json = json_parse_file(cameras);
    const auto& item = json.arr.at(index);
    const auto& camera = *item.find("camera");
    const auto& params = camera.find("params")->arr;
    std::vector<float> view(16);
    for (int i = 0; i < 16; i++) view[i] = (float)item.find("opencv_w2c_row_major")->arr.at(i).num;
    const std::vector<float> intr = {
        (float)(params[0].num * BW / camera.get_double("width", BW)),
        (float)(params[1].num * BH / camera.get_double("height", BH)), BW * 0.5f, BH * 0.5f};
    const std::vector<float> dist(kCameraDistortionParams, 0.0f);
    set_camera_params(BW, BH, camera.get_double("model", 0) == 5 ? "FISHEYE" : "PINHOLE", "NONE",
                      ttv(view.data(), {1, 4, 4}), ttv(intr.data(), {1, 4}),
                      ttv(dist.data(), {1, kCameraDistortionParams}));
    std::printf("%lld splats, SH %d, %d lobes\n", (long long)c.num, c.sh_degree, c.sb_lobes);
    auto rgb = [&]() {
        std::vector<float> v((size_t)BW * BH * 3);
        engine_copy_render_to_host(ttv(v.data(), {1, BH, BW, 3}), ttv_null(), ttv_null(),
                                   ttv_null(), ttv_null());
        return v;
    };
    auto median = [](std::vector<double> v) {
        std::sort(v.begin(), v.end());
        return v[v.size() / 2];
    };
    int channel = 100;
    for (const char* prim : {"3dgs", "mip", "3dgut", "2dgs"}) {
        std::vector<double> sorted_ms;
        for (int k = 0; k < 7; k++) {
            const auto t0 = std::chrono::steady_clock::now();
            forward_3dgs(prim, c.sh_degree, false, false, 0);
            backend::device_synchronize();
            sorted_ms.push_back(ms_since(t0));
        }
        const std::vector<float> ref = rgb();
        int64_t pairs = 0, peak = 0, isects = 0;
        engine_get_isect_peaks(pairs, peak, isects);
        std::printf("%s sorted %.2f ms, %lld splat-tile pairs\n", prim, median(sorted_ms),
                    (long long)isects);
        if (sorted_only) continue;
        for (const char* sorted : {"1", "0"})
        for (const char* spp : {"1", "4", "8"}) for (int winner = 0; winner < 2; winner++) {
            SS_SETENV("SS_VIEWER_STOCHASTIC_SORTED", sorted);
            SS_SETENV("SS_VIEWER_STOCHASTIC_SPP", spp);
            SS_SETENV("SS_VIEWER_STOCHASTIC_WINNER", winner ? "1" : "0");
            std::vector<double> ms;
            std::string psnr;
            ++channel;
            for (int frame = 1; frame <= 64; frame++) {
                const auto t0 = std::chrono::steady_clock::now();
                engine_arm_stochastic(channel, 1);
                forward_3dgs(prim, c.sh_degree, false, false, 0);
                backend::device_synchronize();
                ms.push_back(ms_since(t0));
                if (frame == 1 || frame == 4 || frame == 16 || frame == 64) {
                    const std::vector<float> est = rgb();
                    double mse = 0;
                    for (size_t i = 0; i < est.size(); i++) {
                        const double d = std::clamp(est[i], 0.f, 1.f) - std::clamp(ref[i], 0.f, 1.f);
                        mse += d * d;
                    }
                    char b[48];
                    std::snprintf(b, sizeof b, " %d:%.2f", frame, -10 * std::log10(mse / est.size() + 1e-20));
                    psnr += b;
                }
            }
            std::printf("%s stochastic %s lists, spp %s %s %.2f ms, PSNR frames%s\n", prim,
                        sorted[0] == '1' ? "sorted" : "unsorted", spp,
                        winner ? "winner" : "vis", median(ms), psnr.c_str());
        }
    }
    engine_reset();
    return 0;
}

int main(int argc, char** argv) {
    if (argc >= 3)
        return bench(argv[1], argv[2], argc >= 4 ? std::atoi(argv[3]) : 0,
                     argc >= 5 && std::string(argv[4]) == "sorted");
    std::mt19937 rng(20261121u);
    auto uf = [&](float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    };
    spirula::SplatCloud c;
    c.num = N;
    c.sh_degree = 2;
    const int64_t K = c.coeffs();
    c.means.resize(N * 3); c.quats.resize(N * 4); c.scales.resize(N * 3);
    c.opacities.resize(N); c.features_dc.resize(N * 3); c.features_sh.resize(N * K * 3);
    for (int64_t i = 0; i < N; i++) {
        c.means[3 * i + 0] = uf(-2.f, 2.f);
        c.means[3 * i + 1] = uf(-1.5f, 1.5f);
        c.means[3 * i + 2] = uf(-1.f, 1.f);
        float n = 0;
        for (int k = 0; k < 4; k++) { c.quats[4 * i + k] = uf(-1.f, 1.f); n += c.quats[4 * i + k] * c.quats[4 * i + k]; }
        for (int k = 0; k < 4; k++) c.quats[4 * i + k] /= std::sqrt(n);
        c.scales[3 * i + 0] = uf(-3.5f, -1.5f);
        c.scales[3 * i + 1] = uf(-3.5f, -1.5f);
        c.scales[3 * i + 2] = uf(-5.0f, -2.0f);
        // Opaque and faint both, so samples are accepted and rejected.
        c.opacities[i] = uf(-2.f, 5.f);
        for (int k = 0; k < 3; k++) c.features_dc[3 * i + k] = uf(-1.f, 2.f);
    }
    for (float& v : c.features_sh) v = uf(-0.3f, 0.3f);
    engine_reset();
    set_data_3dgs(c.num, ttv(c.means.data(), {N, 3}), ttv(c.quats.data(), {N, 4}),
                  ttv(c.scales.data(), {N, 3}), ttv(c.opacities.data(), {N, 1}),
                  ttv(c.features_dc.data(), {N, 3}), ttv(c.features_sh.data(), {N, K, 3}), 0);

    const std::vector<float> V = {1, 0, 0, 0.1f, 0, 1, 0, -0.05f, 0, 0, 1, 4.0f, 0, 0, 0, 1};
    const std::vector<float> dist(kCameraDistortionParams, 0.0f);
    const struct { const char* prim; const char* cam; float f; } cfgs[] = {
        {"3dgs", "PINHOLE", 120}, {"mip", "PINHOLE", 120}, {"3dgut", "PINHOLE", 120},
        {"2dgs", "PINHOLE", 120}, {"3dgut", "FISHEYE", 60}, {"2dgs", "EQUIRECTANGULAR", 25},
        {"3dgs", "ORTHOGRAPHIC", 30}, {"3dgut", "ORTHOGRAPHIC", 30}};
    int channel = 0;
    for (const auto& cfg : cfgs) {
        const std::vector<float> intr = {cfg.f, cfg.f, W * 0.5f, H * 0.5f};
        set_camera_params(W, H, cfg.cam, "NONE", ttv(V.data(), {1, 4, 4}), ttv(intr.data(), {1, 4}),
                          ttv(dist.data(), {1, kCameraDistortionParams}));
        const bool normal = engine_primitive_renders_normal(cfg.prim);
        forward_3dgs(cfg.prim, c.sh_degree, false, false, 0, normal);
        const Frame ref = read_back(normal);
        for (int mode = 0; mode < 3; mode++) {
            // Depth-ordered tile lists with either estimator, and the sort-free lists.
            const int winner = mode == 1;
            SS_SETENV("SS_VIEWER_STOCHASTIC_WINNER", winner ? "1" : "0");
            SS_SETENV("SS_VIEWER_STOCHASTIC_SORTED", mode == 2 ? "0" : "1");
            SS_SETENV("SS_VIEWER_STOCHASTIC_SPP", "4");
            Error at[2];
            const int frames[2] = {64, 256};
            int done = 0;
            ++channel;
            for (int k = 0; k < 2; k++) {
                for (; done < frames[k]; done++) {
                    engine_arm_stochastic(channel, 1234);
                    forward_3dgs(cfg.prim, c.sh_degree, false, false, 0, normal);
                }
                backend::device_synchronize();
                check(engine_stochastic_frames() == (uint32_t)frames[k],
                      std::string(cfg.prim) + " frames counted");
                at[k] = compare(ref, read_back(normal));
            }
            if (const char* err = backend::last_error()) {
                std::fprintf(stderr, "backend error: %s\n", err);
                return 1;
            }
            char line[256];
            std::snprintf(line, sizeof line,
                          "%s %s %s: rgb %.5f -> %.5f, alpha %.5f -> %.5f, depth %.5f -> %.5f, normal %.5f -> %.5f",
                          cfg.prim, cfg.cam, winner ? "winner" : mode == 2 ? "vis, unsorted" : "vis",
                          at[0].rgb, at[1].rgb,
                          at[0].alpha, at[1].alpha, at[0].depth, at[1].depth, at[0].normal, at[1].normal);
            // 4x the frames halves Monte Carlo noise; a bias would not move.
            const double tol = winner ? 0.02 : 0.006;
            check(at[1].rgb < tol && at[1].alpha < tol && at[1].rgb < 0.7 * at[0].rgb &&
                      at[1].alpha < 0.75 * at[0].alpha + 1e-4 && at[1].depth < 0.02 &&
                      at[1].normal < 2 * tol,
                  line);
        }
    }
    engine_reset();
    std::printf("%s (%d failure%s)\n", g_failures ? "FAILED" : "PASSED", g_failures,
                g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
