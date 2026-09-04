// The per-splat regularizers must not turn a small splat into a NaN, and must
// not walk it down without end.
//
// scale_reg / erank_reg (shaders/per_splat_losses.slang) take ratios of
// exp(scales); the backward divides by a denominator that flushes to zero
// under exp(-22.4), and 0/0 lands in the scales the optimizer writes. That is
// reachable in any run: mcmc_scale_reg penalizes the LOG scale, so an unused
// splat drops ~lr a step until the kMinLogScale floor stops it. -inf, which
// mcmc_relocation used to manufacture, is covered too.
//
// Self-checking. Drives the shared backward through fused_optim_3dgs_geometry.

#include <kernels/optim/Optimizer.cuh>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using backend::MemcpyKind;

template <typename T>
static T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(host.size() * sizeof(T));
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
static DeviceVector<T> dv(const void* p, int64_t n) {
    return DeviceVector<T>(std::make_tuple((uint64_t)p, (uint32_t)sizeof(T),
                                           std::vector<int64_t>{n, 1}));
}

// nan_only spares the -inf case its own input: feeding -inf in is the point,
// manufacturing a NaN out of it is the bug.
static int64_t count_bad(const float* d, int64_t n, bool nan_only) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(float),
                         MemcpyKind::DeviceToHost);
    int64_t bad = 0;
    for (float v : h) bad += (nan_only ? std::isnan(v) : !std::isfinite(v)) ? 1 : 0;
    return bad;
}

static std::vector<float> download(const float* d, int64_t n) {
    std::vector<float> h((size_t)n);
    backend::memcpy_sync(h.data(), d, (size_t)n * sizeof(float),
                         MemcpyKind::DeviceToHost);
    return h;
}

static constexpr int64_t N = 1024;  // multiple of 256 (block reduces)

// kMinLogScale / kDeadLogScale, shaders/per_splat_losses.slang + densify.slang.
static constexpr float MIN_LOG_SCALE = -40.f;

struct Case {
    const char* label;
    float erank, erank_s3, scale_reg_w, anisotropy;
    bool neg_inf;      // seed every axis at -inf instead of sweeping
    bool check_floor;  // mcmc_scale_reg must not push a splat below the floor
};

static bool run_case(const Case& c) {
    std::vector<float> means(3 * N), quats(4 * N), scales(3 * N), opacs(N),
        dc(3 * N), radii(N, 4.f), zero(4 * N, 0.f);
    // Log scales from ordinary down through every underflow threshold, the
    // third axis pulled further down so the eigenvalue shares degenerate too.
    for (int64_t i = 0; i < N; i++) {
        float s = -1.f - 59.f * (float)i / (float)(N - 1);
        if (c.neg_inf) s = -std::numeric_limits<float>::infinity();
        scales[3 * i + 0] = s;
        scales[3 * i + 1] = s;
        scales[3 * i + 2] = c.neg_inf ? s : s - c.anisotropy;
        for (int k = 0; k < 3; k++) means[3 * i + k] = 0.1f * (float)k;
        quats[4 * i + 0] = 1.f;
        opacs[i] = -2.f;
    }
    std::vector<float> start = scales;

    float* d_means = upload(means);
    float* d_quats = upload(quats);
    float* d_scales = upload(scales);
    float* d_opacs = upload(opacs);
    float* d_dc = upload(dc);
    float* d_radii = upload(radii);
    // No data gradient: an invisible splat is exactly the case that walks
    // itself down, and it isolates the regularizers.
    float* d_v[5];
    for (int a = 0; a < 5; a++) d_v[a] = upload(zero);
    float* d_g1[4], *d_g2[4];
    for (int a = 0; a < 4; a++) {
        d_g1[a] = upload(zero);
        d_g2[a] = upload(zero);
    }

    NonShQuantState nq{};
    GradQuantBuffers gq{};
    for (int step = 1; step <= 4; step++) {
        fused_optim_3dgs_geometry(
            N,
            dv<float3>(d_means, N), dv<float3>(d_v[0], N),
            dv<float3>(d_g1[0], N), dv<float3>(d_g2[0], N),
            dv<float4>(d_quats, N), dv<float4>(d_v[1], N),
            dv<float4>(d_g1[1], N), dv<float4>(d_g2[1], N),
            dv<float3>(d_scales, N), dv<float3>(d_v[2], N),
            dv<float3>(d_g1[2], N), dv<float3>(d_g2[2], N),
            dv<float>(d_opacs, N), dv<float>(d_v[3], N),
            dv<float>(d_g1[3], N), dv<float>(d_g2[3], N),
            dv<float3>(d_dc, N), dv<float3>(d_v[4], N),
            dv<float>(d_radii, N), DeviceVector<float>(),
            1.6e-4f, 1e-3f, 5e-3f, 5e-2f, 2.5e-3f,
            /*max_gauss_ratio=*/10.f, c.scale_reg_w,
            /*mcmc_op=*/0.01f, /*mcmc_scale=*/0.01f,
            c.erank, c.erank_s3, /*quat_norm=*/0.01f,
            /*dc_reg=*/0.001f, /*sh_reg=*/0.001f,
            /*max_screen_size=*/0.3f, /*max_screen_size_penalty=*/0.f,
            /*scale_agnostic_mean=*/false, nq, gq,
            step, DeviceVector<int32_t>(), /*grad_scale=*/1.f,
            /*zero_grad=*/false);
        backend::device_synchronize();
    }

    struct Out { const char* name; const float* p; int64_t n; };
    const Out outs[] = {
        {"means", d_means, 3 * N},   {"quats", d_quats, 4 * N},
        {"scales", d_scales, 3 * N}, {"opacities", d_opacs, N},
        {"g1_scales", d_g1[2], 3 * N}, {"g2_scales", d_g2[2], 3 * N},
    };
    int64_t total = 0;
    for (const Out& o : outs) {
        int64_t bad = count_bad(o.p, o.n, c.neg_inf);
        if (bad) std::printf("  %s: %lld / %lld %s\n", o.name,
                             (long long)bad, (long long)o.n,
                             c.neg_inf ? "NaN" : "non-finite");
        total += bad;
    }
    if (c.check_floor) {
        std::vector<float> now = download(d_scales, 3 * N);
        int64_t sank = 0;
        for (int64_t i = 0; i < 3 * N; i++)
            if (start[i] <= MIN_LOG_SCALE && now[i] < start[i] - 1e-5f) sank++;
        if (sank) std::printf("  scales: %lld pushed below the floor\n",
                              (long long)sank);
        total += sank;
    }
    std::printf("%s %s\n", total ? "FAIL" : "ok  ", c.label);

    backend::device_free(d_means);  backend::device_free(d_quats);
    backend::device_free(d_scales); backend::device_free(d_opacs);
    backend::device_free(d_dc);     backend::device_free(d_radii);
    for (int a = 0; a < 5; a++) backend::device_free(d_v[a]);
    for (int a = 0; a < 4; a++) {
        backend::device_free(d_g1[a]);
        backend::device_free(d_g2[a]);
    }
    return total == 0;
}

int main() {
    const Case cases[] = {
        {"shipping defaults (both reg weights zero)", 0.f, 0.f, 0.f, 0.f, false, true},
        {"erank_reg on",                              0.1f, 0.05f, 0.f, 0.f, false, false},
        {"scale_regularization on",                   0.f, 0.f, 0.1f, 0.f, false, false},
        {"both on, anisotropic",                      0.1f, 0.05f, 0.1f, 25.f, false, false},
        {"defaults, extreme anisotropy",              0.f, 0.f, 0.f, 90.f, false, false},
        {"-inf log scale (mcmc_relocation underflow)", 0.1f, 0.05f, 0.1f, 0.f, true, false},
    };
    bool ok = true;
    for (const Case& c : cases) ok &= run_case(c);
    std::printf("%s\n", ok ? "PASS" : "FAILED");
    return ok ? 0 : 1;
}
