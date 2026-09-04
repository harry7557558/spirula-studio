// Backend parity tool for the multi-scale per-pixel loss stack
// (compute_multi_scale_per_pixel_losses: per-pixel fused losses, fused SSIM
// backward, image pyramid, edge-aware densification maps) plus the batched
// quantile radix-select. The SAME source builds under both backends:
//
//   CUDA build:   ./msloss_parity dump ref.bin
//   Vulkan build: ./msloss_parity compare ref.bin   (per device)
//
// Ref format: [nf tight floats] [nl loose floats]. Which values go in which
// channel, and the one expected CUDA/Vulkan mismatch: docs/testing.md.

#include <kernels/loss/PerPixelLoss.cuh>
#include <kernels/densify/Densify.cuh>
#include <core/Tensor.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <random>
#include <vector>

using backend::MemcpyKind;

template <bool invert_quantile>
int batch_quantile_masked_radix_select(
    const float* d_x, int B, int N, float q, float* d_out, uint32_t* temp,
    backend::Stream stream);

template <bool invert_quantile>
int batch_quantile_positive_radix_select(
    const float* d_x, int B, int N, float q, float* d_out, uint32_t* temp,
    backend::Stream stream);

static std::vector<float> g_tight, g_loose;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
T* alloc_zero(int64_t n) {
    int64_t bytes = ((n * (int64_t)sizeof(T) + 3) / 4) * 4;
    T* d = (T*)backend::device_malloc(bytes);
    backend::memset_sync(d, 0, bytes);
    return d;
}

void readback_f(std::vector<float>& acc, const float* d, int64_t n) {
    size_t off = acc.size();
    acc.resize(off + n);
    backend::memcpy_sync(acc.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

struct Rng {
    std::mt19937 rng;
    explicit Rng(uint32_t seed) : rng(seed) {}
    float uf(float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    }
    std::vector<float> vec(int64_t n, float lo, float hi) {
        std::vector<float> v(n);
        for (auto& x : v) x = uf(lo, hi);
        return v;
    }
};

TorchTensorView ttv(const void* p, std::vector<int64_t> shape) {
    return std::make_tuple((uint64_t)p, (uint32_t)4, std::move(shape));
}
TorchTensorView ttv_null() {
    return std::make_tuple((uint64_t)0, (uint32_t)4, std::vector<int64_t>{0});
}

// ---------------------------------------------------------------------------
// One multi-scale loss configuration.
// ---------------------------------------------------------------------------

struct MsCfg {
    const char* name;
    int64_t B, H, W;
    int num_scales;
    // GT resolutions (0 = same as render). Depth/normal bilinear, alpha
    // nearest.
    int64_t Hd, Wd, Hn, Wn, Ha, Wa;
    bool with_alpha;      // ref_alpha buffer + has_mask
    bool with_cams;       // camera_indices ({3,1}) with num_train_images=4
    bool minimal;         // rgb+Ts only (all other modalities null)
    int loss_map_mode;    // DensifyLossMapMode
    float quantile;
    float nms_falloff;    // *_nms modes only; 0 = the hard canny suppression
};

void run_ms_cfg(Rng& r, const MsCfg& c) {
    const int64_t np = c.B * c.H * c.W;
    const int64_t Hd = c.Hd ? c.Hd : c.H, Wd = c.Wd ? c.Wd : c.W;
    const int64_t Hn = c.Hn ? c.Hn : c.H, Wn = c.Wn ? c.Wn : c.W;
    const int64_t Ha = c.Ha ? c.Ha : c.H, Wa = c.Wa ? c.Wa : c.W;
    const int64_t npd = c.B * Hd * Wd, npn = c.B * Hn * Wn, npa = c.B * Ha * Wa;

    float* render_rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
    float* ref_rgb = upload(r.vec(3 * np, 0.0f, 1.0f));
    float* render_Ts = upload(r.vec(np, 0.0f, 1.0f));

    float* render_depth = nullptr;
    float* ref_depth = nullptr;
    float* render_normal = nullptr;
    float* depth_normal = nullptr;
    float* ref_normal = nullptr;
    float* rgb_dist = nullptr;
    float* depth_dist = nullptr;
    float* normal_dist = nullptr;
    float* median_depth = nullptr;
    float* median_normal = nullptr;
    if (!c.minimal) {
        render_depth = upload(r.vec(np, 0.3f, 5.0f));
        {
            // ~15% zero sentinel ("no GT depth").
            auto v = r.vec(npd, 0.3f, 5.0f);
            for (auto& x : v)
                if (r.uf(0, 1) < 0.15f) x = 0.0f;
            ref_depth = upload(v);
        }
        render_normal = upload(r.vec(3 * np, -1.0f, 1.0f));
        depth_normal = upload(r.vec(3 * np, -1.0f, 1.0f));
        ref_normal = upload(r.vec(3 * npn, -1.0f, 1.0f));
        rgb_dist = upload(r.vec(3 * np, 0.0f, 0.1f));
        depth_dist = upload(r.vec(np, 0.0f, 0.1f));
        normal_dist = upload(r.vec(3 * np, 0.0f, 0.1f));
        {
            auto v = r.vec(np, 0.3f, 5.0f);
            for (auto& x : v)
                if (r.uf(0, 1) < 0.1f) x = 0.0f;
            median_depth = upload(v);
        }
        median_normal = upload(r.vec(3 * np, -1.0f, 1.0f));
    }

    uint8_t* ref_alpha = nullptr;
    if (c.with_alpha) {
        std::vector<uint8_t> m(npa);
        for (auto& x : m) x = r.uf(0, 1) < 0.8f ? 1 : 0;
        ref_alpha = upload(m);
    }

    int64_t* cams = nullptr;
    long num_train = c.B;
    if (c.with_cams) {
        std::vector<int64_t> ch = {3, 1};
        ch.resize((size_t)c.B, 3);
        cams = upload(ch);
        num_train = 4;
    }

    std::array<float, (int)LossWeightIndex::length> weights{};
    for (auto& w : weights) w = r.uf(0.1f, 1.0f);
    const float w_ssim = r.uf(0.1f, 0.5f);

    float* v_losses = upload(r.vec((int)LossIndex::length, -1.0f, 1.0f));
    float* loss_map_out = alloc_zero<float>(np);

    const int n_grads = c.minimal ? 1 : 13;
    std::vector<bool> needs(13, !c.minimal);
    needs[0] = true;

    PerPixelGrads grads = {};
    grads.v_render_rgb = ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
    if (!c.minimal) {
        grads.v_ref_rgb = ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
        grads.v_render_depth = ttv(alloc_zero<float>(np), {c.B, c.H, c.W, 1});
        grads.v_ref_depth = ttv(alloc_zero<float>(npd), {c.B, Hd, Wd, 1});
        grads.v_render_normal =
            ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
        grads.v_depth_normal =
            ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
        grads.v_ref_normal = ttv(alloc_zero<float>(3 * npn), {c.B, Hn, Wn, 3});
        grads.v_render_Ts = ttv(alloc_zero<float>(np), {c.B, c.H, c.W, 1});
        grads.v_rgb_dist = ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
        grads.v_depth_dist = ttv(alloc_zero<float>(np), {c.B, c.H, c.W, 1});
        grads.v_normal_dist =
            ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
        grads.v_median_depth = ttv(alloc_zero<float>(np), {c.B, c.H, c.W, 1});
        grads.v_median_normal =
            ttv(alloc_zero<float>(3 * np), {c.B, c.H, c.W, 3});
    }

    auto run_once = [&]() -> LossValues {
        return compute_multi_scale_per_pixel_losses(
            c.num_scales,
            ttv(render_rgb, {c.B, c.H, c.W, 3}),
            ttv(ref_rgb, {c.B, c.H, c.W, 3}),
            render_depth ? ttv(render_depth, {c.B, c.H, c.W, 1}) : ttv_null(),
            ref_depth ? ttv(ref_depth, {c.B, Hd, Wd, 1}) : ttv_null(),
            render_normal ? ttv(render_normal, {c.B, c.H, c.W, 3})
                          : ttv_null(),
            depth_normal ? ttv(depth_normal, {c.B, c.H, c.W, 3}) : ttv_null(),
            ref_normal ? ttv(ref_normal, {c.B, Hn, Wn, 3}) : ttv_null(),
            ttv(render_Ts, {c.B, c.H, c.W, 1}),
            rgb_dist ? ttv(rgb_dist, {c.B, c.H, c.W, 3}) : ttv_null(),
            depth_dist ? ttv(depth_dist, {c.B, c.H, c.W, 1}) : ttv_null(),
            normal_dist ? ttv(normal_dist, {c.B, c.H, c.W, 3}) : ttv_null(),
            median_depth ? ttv(median_depth, {c.B, c.H, c.W, 1}) : ttv_null(),
            median_normal ? ttv(median_normal, {c.B, c.H, c.W, 3})
                          : ttv_null(),
            ref_alpha
                ? std::make_tuple((uint64_t)ref_alpha, (uint32_t)1,
                                  std::vector<int64_t>{c.B, Ha, Wa, 1})
                : ttv_null(),
            /*has_mask=*/c.with_alpha, weights, w_ssim,
            ttv(v_losses, {(int)LossIndex::length}), needs, num_train,
            cams ? ttv(cams, {c.B}) : ttv_null(),
            ttv(loss_map_out, {c.B, c.H, c.W, 1}), c.loss_map_mode,
            c.quantile, c.nms_falloff, grads);
    };

    run_once();  // primes the one-behind readouts
    LossValues lv = run_once();
    backend::device_synchronize();

    const bool gt_scaled = c.Hd != 0 || c.Hn != 0;

    readback_f(g_tight, (const float*)std::get<0>(grads.v_render_rgb), 3 * np);
    if (!c.minimal) {
        readback_f(g_tight, (const float*)std::get<0>(grads.v_ref_rgb), 3 * np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_render_depth), np);
        readback_f(gt_scaled ? g_loose : g_tight,
                   (const float*)std::get<0>(grads.v_ref_depth), npd);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_render_normal), 3 * np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_depth_normal), 3 * np);
        readback_f(gt_scaled ? g_loose : g_tight,
                   (const float*)std::get<0>(grads.v_ref_normal), 3 * npn);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_render_Ts), np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_rgb_dist), 3 * np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_depth_dist), np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_normal_dist), 3 * np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_median_depth), np);
        readback_f(g_tight, (const float*)std::get<0>(grads.v_median_normal), 3 * np);
    }
    if (c.loss_map_mode != (int)DensifyLossMapMode::None)
        readback_f(g_tight, loss_map_out, np);
    g_loose.push_back(lv.rgb_loss);
    g_loose.push_back(lv.rgb_psnr);
    g_loose.push_back(lv.depth_sup);
    g_loose.push_back(lv.normal_sup);
    g_loose.push_back(lv.alpha_sup);
    g_loose.push_back(lv.normal_reg);
    g_loose.push_back(lv.alpha_reg);
    g_loose.push_back(lv.rgb_dist_reg);
    g_loose.push_back(lv.depth_dist_reg);
    g_loose.push_back(lv.normal_dist_reg);
    g_loose.push_back(lv.mean_median_depth_sup);
    g_loose.push_back(lv.median_depth_normal_reg);
    g_loose.push_back(lv.median_normal_sup);
    g_loose.push_back(lv.median_render_normal_reg);
    g_loose.push_back(lv.ssim);

    (void)n_grads;
    std::printf("msloss_parity: cfg %-12s done\n", c.name);
}

// ---------------------------------------------------------------------------
// Direct quantile radix-select checks (bit-exact integer selection).
// ---------------------------------------------------------------------------

void run_quantile(Rng& r) {
    const int B = 5, N = 10000;
    std::vector<float> h = r.vec((int64_t)B * N, -2.0f, 2.0f);
    // Sprinkle non-finite / zero values the selector must ignore.
    for (size_t i = 0; i < h.size(); i += 97) h[i] = 0.0f;
    for (size_t i = 13; i < h.size(); i += 331)
        h[i] = std::numeric_limits<float>::infinity();
    for (size_t i = 51; i < h.size(); i += 613)
        h[i] = std::numeric_limits<float>::quiet_NaN();
    float* d_x = upload(h);
    uint32_t* temp = (uint32_t*)alloc_zero<uint32_t>((256 + 5) * B);

    for (float q : {0.5f, 0.9f}) {
        float* out_a = alloc_zero<float>(B);
        float* out_b = alloc_zero<float>(B);
        float* out_c = alloc_zero<float>(B);
        batch_quantile_masked_radix_select<false>(d_x, B, N, q, out_a, temp,
                                                  backend::kDefaultStream);
        batch_quantile_masked_radix_select<true>(d_x, B, N, q, out_b, temp,
                                                 backend::kDefaultStream);
        // Same rows, negatives dropped instead of folded in as |x|.
        batch_quantile_positive_radix_select<false>(d_x, B, N, q, out_c, temp,
                                                    backend::kDefaultStream);
        backend::device_synchronize();
        readback_f(g_tight, out_a, B);
        readback_f(g_tight, out_b, B);
        readback_f(g_tight, out_c, B);
    }
    std::printf("msloss_parity: cfg %-12s done\n", "quantile");
}

// ---------------------------------------------------------------------------
// Loss-map conditioning (median normalize + quantile clip), in place.
// ---------------------------------------------------------------------------

void run_normalize_clip(Rng& r) {
    const int64_t B = 3, H = 37, W = 53, N = H * W;
    std::vector<float> h = r.vec(B * N, 0.0f, 1.0f);
    // The population the quantiles are taken over excludes these. The tiny
    // negatives are the real map's flat-region background: folded in as |x|
    // they would put the median in the noise floor.
    for (size_t i = 0; i < h.size(); i += 3) h[i] = -h[i] * 1e-8f;
    for (size_t i = 0; i < h.size(); i += 11) h[i] = 0.0f;
    for (size_t i = 5; i < h.size(); i += 401)
        h[i] = std::numeric_limits<float>::infinity();
    for (size_t i = 7; i < h.size(); i += 509)
        h[i] = std::numeric_limits<float>::quiet_NaN();
    // Spikes: what the clip exists to flatten.
    for (size_t i = 3; i < h.size(); i += 257) h[i] = 1e4f;

    const struct { bool norm; float q; float p; } cases[] = {
        {true, 1.0f, 1.0f}, {false, 0.98f, 1.0f}, {true, 0.98f, 1.0f},
        {false, 1.0f, 1.0f}, {false, 1.0f, 0.5f}, {true, 0.98f, 2.0f},
    };
    for (const auto& c : cases) {
        float* d = upload(h);
        normalize_clip_map_inplace_tensor(ttv(d, {B, H, W, 1}), c.norm, c.q,
                                          c.p);
        backend::device_synchronize();
        readback_f(g_tight, d, B * N);
    }
    std::printf("msloss_parity: cfg %-12s done\n", "norm_clip");
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    Rng r(260720u);

    const MsCfg cfgs[] = {
        // Full modality set, equal-shape GT, mask, 3 scales, LossFull map.
        {"full", 2, 64, 80, 3, 0, 0, 0, 0, 0, 0, true, false, false,
         (int)DensifyLossMapMode::LossFull, 0.9f, 0.5f},
        // Scaled GT modalities (bilinear paths), camera indices, no mask,
        // 2 scales, full-SSIM map; checks the SSIM scalar.
        {"scaled_gt", 2, 40, 48, 2, 80, 96, 30, 36, 0, 0, false, true, false,
         (int)DensifyLossMapMode::SsimFull, 0.9f, 0.5f},
        // Minimal modalities, robust edge-aware map (residual + quantile +
        // tukey + canny), equal-res mask (canny indexes the mask at render
        // resolution), single scale.
        {"robust_edge", 1, 48, 64, 1, 0, 0, 0, 0, 0, 0, true, false, true,
         (int)DensifyLossMapMode::RobustEdgeAware, 0.85f, 0.5f},
        // Plain edge-aware map (canny of GT rgb), 2 scales.
        {"edge_aware", 1, 32, 32, 2, 0, 0, 0, 0, 0, 0, true, false, true,
         (int)DensifyLossMapMode::EdgeAware, 0.9f, 0.5f},
        // SSIM contrast*structure and structure-only map variants; the
        // latter with a smaller-resolution mask (SSIM's clamped mask path).
        {"ssim_cs", 1, 32, 32, 1, 0, 0, 0, 0, 0, 0, false, false, true,
         (int)DensifyLossMapMode::SsimContrastStruct, 0.9f, 0.5f},
        {"ssim_str", 1, 32, 32, 1, 0, 0, 0, 0, 16, 16, true, false, true,
         (int)DensifyLossMapMode::SsimStructure, 0.9f, 0.5f},
        // The NMS variants: same maps, thinned to their ridges. Multi-scale
        // on the loss_full one, since NMS runs per scale.
        {"loss_nms", 1, 40, 48, 2, 0, 0, 0, 0, 0, 0, true, false, false,
         (int)DensifyLossMapMode::LossFullNms, 0.9f, 0.5f},
        {"ssim_f_nms", 1, 32, 32, 1, 0, 0, 0, 0, 0, 0, false, false, true,
         (int)DensifyLossMapMode::SsimFullNms, 0.9f, 0.0f},
        {"ssim_cs_nms", 1, 32, 32, 1, 0, 0, 0, 0, 0, 0, false, false, true,
         (int)DensifyLossMapMode::SsimContrastStructNms, 0.9f, 0.25f},
        {"ssim_str_nms", 1, 32, 32, 1, 0, 0, 0, 0, 16, 16, true, false, true,
         (int)DensifyLossMapMode::SsimStructureNms, 0.9f, 1.0f},
    };
    for (const MsCfg& c : cfgs) run_ms_cfg(r, c);
    run_quantile(r);
    run_normalize_clip(r);

    auto write_all = [&](const char* path) {
        std::ofstream f(path, std::ios::binary);
        int64_t nf = (int64_t)g_tight.size(), nl = (int64_t)g_loose.size();
        f.write((const char*)&nf, 8);
        f.write((const char*)g_tight.data(), nf * 4);
        f.write((const char*)&nl, 8);
        f.write((const char*)g_loose.data(), nl * 4);
    };

    if (dumping) {
        write_all(argv[2]);
        std::printf("msloss_parity: dumped %zu + %zu floats to %s\n",
                    g_tight.size(), g_loose.size(), argv[2]);
        return 0;
    }

    if (const char* dump_got = std::getenv("MSLOSS_DUMP_GOT"))
        write_all(dump_got);

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t nf = 0, nl = 0;
    f.read((char*)&nf, 8);
    if (nf != (int64_t)g_tight.size()) {
        std::fprintf(stderr, "tight count mismatch: ref %lld vs got %zu\n",
                     (long long)nf, g_tight.size());
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);
    f.read((char*)&nl, 8);
    if (nl != (int64_t)g_loose.size()) {
        std::fprintf(stderr, "loose count mismatch: ref %lld vs got %zu\n",
                     (long long)nl, g_loose.size());
        return 1;
    }
    std::vector<float> lref(nl);
    f.read((char*)lref.data(), nl * 4);

    // Paired with a relative-RMS gate: the NMS / quantile / clip modes make
    // discrete per-pixel choices that flip where an architecture's rounding
    // differs, so a few large outliers are expected (docs/testing.md).
    auto cmp_f = [](const std::vector<float>& got,
                    const std::vector<float>& want, int64_t& viol,
                    double& max_abs, double& rel_rms) {
        viol = 0;
        max_abs = 0;
        double se = 0, sr = 0;
        for (size_t i = 0; i < got.size(); i++) {
            bool gfin = std::isfinite(got[i]), wfin = std::isfinite(want[i]);
            if (!gfin || !wfin) {
                if (gfin != wfin) viol++;
                continue;
            }
            double d = std::fabs((double)got[i] - (double)want[i]);
            double tol = 5e-3 + 1e-3 * std::fabs((double)want[i]);
            max_abs = std::max(max_abs, d);
            if (d > tol) viol++;
            se += d * d;
            sr += (double)want[i] * (double)want[i];
        }
        rel_rms = sr > 0 ? std::sqrt(se / sr) : 0.0;
    };
    int64_t fviol = 0, lviol = 0;
    double fmax = 0, lmax = 0, frms = 0, lrms = 0;
    cmp_f(g_tight, ref, fviol, fmax, frms);
    cmp_f(g_loose, lref, lviol, lmax, lrms);
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double lfrac = nl ? (double)lviol / (double)nl : 0.0;
    std::printf(
        "msloss_parity: %lld tight floats (max_abs %.3g, violations %lld = "
        "%.5f%%, rel_rms %.3g), %lld loose floats (max_abs %.3g, violations "
        "%lld = %.5f%%, rel_rms %.3g)\n",
        (long long)nf, fmax, (long long)fviol, 100.0 * ffrac, frms,
        (long long)nl, lmax, (long long)lviol, 100.0 * lfrac, lrms);
    bool pass = ffrac <= 5e-3 && frms <= 5e-3 &&
                lfrac <= 2e-2 && lrms <= 5e-2;
    std::printf(pass ? "msloss_parity: PASSED\n" : "msloss_parity: FAILED\n");
    return pass ? 0 : 1;
}
