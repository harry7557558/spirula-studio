// Backend parity tool for the PPISP image-transform + regularization launch
// APIs (kernels/pixelwise/PixelWise.cuh PPISP subset), across all three param types
// (original / rqs / no_crf) and cam_indices on/off. The SAME source builds
// under both backends:
//
//   CUDA build:   ./ppisp_parity dump ref.bin
//   Vulkan build: ./ppisp_parity compare ref.bin   (per device)
//
// Ref format: [nf tight floats] [nl loose floats].
//
// Both backends run the same canonical shaders/ppisp.slang math (CUDA via the
// 2026.2.1 emission, Vulkan via SPIR-V), so per-pixel outputs and per-image
// gradients differ only by fast-math rounding -> tight channel. Atomically
// accumulated buffers (v_ppisp_params from the pixel backward, the summed
// raw-loss tail row and everything derived from it) -> loose channel. The
// regularization BACKWARD is fed synthetic host-side raw_losses/v_losses so
// its outputs stay deterministic (tight).

#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using backend::MemcpyKind;

static std::vector<float> g_tight, g_loose;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(
        ((host.size() * sizeof(T) + 3) / 4) * 4);
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
    return std::make_tuple((uint64_t)0, (uint32_t)4,
                           std::vector<int64_t>{0});
}

DeviceTensor3D<float3> dt3(const void* p, int64_t b, int64_t h, int64_t w) {
    return DeviceTensor3D<float3>(ttv(p, {b, h, w, 3}));
}

struct TypeInfo {
    const char* name;
    int n_params;
    int n_raw;
};

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    Rng r(260719u);

    const TypeInfo types[3] = {
        {"original", 36, (int)RawPPISPRegLossIndex::length},
        {"rqs", 39, (int)RawPPISPRegLossIndexRQS::length},
        {"no_crf", 24, (int)RawPPISPRegLossIndexNoCRF::length},
    };

    const int64_t B = 3, H = 22, W = 26, N_cam = 5;
    const float aw = 104.0f, ah = 88.0f;

    for (const TypeInfo& t : types) {
        for (int with_cams = 0; with_cams < 2; with_cams++) {
            const int64_t n_tab = with_cams ? N_cam : B;
            // Small parameters keep the CRF / homography paths well-
            // conditioned; the raw parametrization passes through softplus /
            // sigmoid so 0-centered draws are the intended regime.
            float* params =
                upload(r.vec(n_tab * t.n_params, -0.4f, 0.4f));
            std::vector<float> intr_h;
            for (int64_t b = 0; b < B; b++) {
                intr_h.push_back(80.0f);           // fx (unused)
                intr_h.push_back(80.0f);           // fy (unused)
                intr_h.push_back(r.uf(40.0f, 60.0f));  // cx
                intr_h.push_back(r.uf(35.0f, 50.0f));  // cy
            }
            float* intrins = upload(intr_h);
            std::vector<int> cam_h = {4, 1, 4};  // duplicate camera
            int* cams = with_cams ? upload(cam_h) : nullptr;

            int64_t np = B * H * W;
            float* in_image = upload(r.vec(3 * np, 0.0f, 1.0f));
            float* out_image = alloc_zero<float>(3 * np);
            ppisp_forward(dt3(in_image, B, H, W), ttv(params, {n_tab, t.n_params}),
                          ttv(intrins, {B, 4}), aw, ah, t.name,
                          cams ? ttv(cams, {B}) : ttv_null(),
                          dt3(out_image, B, H, W));
            readback_f(g_tight, out_image, 3 * np);

            float* v_out = upload(r.vec(3 * np, -0.3f, 0.3f));
            float* v_in = alloc_zero<float>(3 * np);
            float* v_params = alloc_zero<float>(n_tab * t.n_params);
            ppisp_backward(dt3(in_image, B, H, W),
                           ttv(params, {n_tab, t.n_params}),
                           ttv(intrins, {B, 4}), aw, ah,
                           dt3(v_out, B, H, W), t.name,
                           cams ? ttv(cams, {B}) : ttv_null(),
                           dt3(v_in, B, H, W),
                           ttv(v_params, {n_tab, t.n_params}));
            readback_f(g_tight, v_in, 3 * np);
            readback_f(g_loose, v_params, n_tab * t.n_params);
        }

        // Regularization forward: per-image raw rows are deterministic
        // (tight); the summed tail row and the weighted losses derived from
        // it are atomic-order dependent (loose).
        {
            const int64_t Bp = 6;
            float* params = upload(r.vec(Bp * t.n_params, -0.5f, 0.5f));
            std::array<float, (int)PPISPRegLossIndex::length> weights{};
            for (auto& w : weights) w = r.uf(0.1f, 2.0f);
            float* losses = alloc_zero<float>((int)PPISPRegLossIndex::length);
            float* raw_losses = alloc_zero<float>((Bp + 1) * t.n_raw);
            compute_ppsip_regularization_forward(
                ttv(params, {Bp, t.n_params}), weights, t.name,
                ttv(losses, {(int)PPISPRegLossIndex::length}),
                ttv(raw_losses, {Bp + 1, t.n_raw}));
            readback_f(g_tight, raw_losses, Bp * t.n_raw);
            readback_f(g_loose, raw_losses + Bp * t.n_raw, t.n_raw);
            readback_f(g_loose, losses, (int)PPISPRegLossIndex::length);

            // Backward with synthetic (host-fixed) summed raw losses so the
            // whole chain stays deterministic.
            std::vector<float> raw_h = r.vec((Bp + 1) * t.n_raw, -1.0f, 1.0f);
            float* raw_fixed = upload(raw_h);
            float* v_losses =
                upload(r.vec((int)PPISPRegLossIndex::length, -1.0f, 1.0f));
            float* v_params = alloc_zero<float>(Bp * t.n_params);
            compute_ppsip_regularization_backward(
                ttv(params, {Bp, t.n_params}), weights,
                ttv(raw_fixed, {Bp + 1, t.n_raw}),
                ttv(v_losses, {(int)PPISPRegLossIndex::length}), t.name,
                ttv(v_params, {Bp, t.n_params}));
            readback_f(g_tight, v_params, Bp * t.n_params);
        }
    }

    // Negative-intensity pixels, exposure and vignetting neutralized so the
    // colour stage stands alone: ~half of them sit below the RGI denominator
    // floor, which is the only thing holding intensity through that branch.
    {
        const int64_t Bx = 2, np = Bx * H * W;
        const int n_params = 24;  // no_crf: exposure(1) + vignetting(15) + colour(8)
        std::vector<float> ph(Bx * n_params, 0.0f);
        for (int64_t b = 0; b < Bx; b++)
            for (int i = 16; i < n_params; i++)
                ph[b * n_params + i] = r.uf(-0.4f, 0.4f);
        float* params = upload(ph);
        std::vector<float> intr_h;
        for (int64_t b = 0; b < Bx; b++) {
            intr_h.push_back(80.0f);
            intr_h.push_back(80.0f);
            intr_h.push_back(52.0f);
            intr_h.push_back(44.0f);
        }
        float* intrins = upload(intr_h);
        std::vector<float> in_h = r.vec(3 * np, -1.0f, 1.0f);
        float* in_image = upload(in_h);
        float* out_image = alloc_zero<float>(3 * np);
        ppisp_forward(dt3(in_image, Bx, H, W), ttv(params, {Bx, n_params}),
                      ttv(intrins, {Bx, 4}), aw, ah, "no_crf", ttv_null(),
                      dt3(out_image, Bx, H, W));

        std::vector<float> out_h(3 * np);
        backend::memcpy_sync(out_h.data(), out_image,
                             3 * np * sizeof(float), MemcpyKind::DeviceToHost);
        double worst_di = 0.0, worst_mag = 0.0;
        for (int64_t i = 0; i < np; i++) {
            double si = (double)in_h[3 * i] + in_h[3 * i + 1] + in_h[3 * i + 2];
            double so = (double)out_h[3 * i] + out_h[3 * i + 1] + out_h[3 * i + 2];
            worst_di = std::max(worst_di, std::fabs(so - si));
            for (int c = 0; c < 3; c++)
                worst_mag = std::max(worst_mag, std::fabs((double)out_h[3 * i + c]));
        }
        std::printf("ppisp_parity: negative-intensity block, "
                    "max |sum(out) - sum(in)| %.3g, max |out| %.3g\n",
                    worst_di, worst_mag);
        // Two-sided: unfloored these leave at |out| ~ 3 (H is near identity),
        // floored they cap near 20x that, and without the floor at all the old
        // 1e-5 denominator sent them past 1e5.
        if (!(worst_di < 1e-3) || !(worst_mag > 10.0) || !(worst_mag < 1e3)) {
            std::fprintf(stderr, "ppisp_parity: colour stage lost intensity, or "
                                 "the denominator floor never bound / blew up\n");
            return 1;
        }
        readback_f(g_tight, out_image, 3 * np);

        float* v_out = upload(r.vec(3 * np, -0.3f, 0.3f));
        float* v_in = alloc_zero<float>(3 * np);
        float* v_params = alloc_zero<float>(Bx * n_params);
        ppisp_backward(dt3(in_image, Bx, H, W), ttv(params, {Bx, n_params}),
                       ttv(intrins, {Bx, 4}), aw, ah, dt3(v_out, Bx, H, W),
                       "no_crf", ttv_null(), dt3(v_in, Bx, H, W),
                       ttv(v_params, {Bx, n_params}));
        readback_f(g_tight, v_in, 3 * np);
        readback_f(g_loose, v_params, Bx * n_params);
    }

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
        std::printf("ppisp_parity: dumped %zu + %zu floats to %s\n",
                    g_tight.size(), g_loose.size(), argv[2]);
        return 0;
    }

    if (const char* dump_got = std::getenv("PPISP_DUMP_GOT"))
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

    auto cmp_f = [](const std::vector<float>& got,
                    const std::vector<float>& want, int64_t& viol,
                    double& max_abs) {
        viol = 0;
        max_abs = 0;
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
        }
    };
    int64_t fviol = 0, lviol = 0;
    double fmax = 0, lmax = 0;
    cmp_f(g_tight, ref, fviol, fmax);
    cmp_f(g_loose, lref, lviol, lmax);
    double ffrac = nf ? (double)fviol / (double)nf : 0.0;
    double lfrac = nl ? (double)lviol / (double)nl : 0.0;
    std::printf(
        "ppisp_parity: %lld tight floats (max_abs %.3g, violations %lld = "
        "%.5f%%), %lld loose floats (max_abs %.3g, violations %lld = "
        "%.5f%%)\n",
        (long long)nf, fmax, (long long)fviol, 100.0 * ffrac, (long long)nl,
        lmax, (long long)lviol, 100.0 * lfrac);
    bool pass = ffrac <= 2e-3 && lfrac <= 2e-2;
    std::printf(pass ? "ppisp_parity: PASSED\n" : "ppisp_parity: FAILED\n");
    return pass ? 0 : 1;
}
