// Backend parity tool for the dataset GT warp + byte->float conversion
// launch APIs (kernels/pixelwise/PixelWise.cuh launch_warp_* + EngineInternal.h raw
// converters). The SAME source builds under both backends:
//
//   CUDA build:   ./warp_parity dump ref.bin
//   Vulkan build: ./warp_parity compare ref.bin   (per device)
//
// Ref format: [nf tight floats].
//
// Everything here is deterministic per-pixel math (no atomics), so all
// outputs live in one tight channel. Both backends run the same canonical
// slang projection math (CUDA via the 2026.2.1 emission, Vulkan via
// SPIR-V), so residual differences are fast-math rounding plus isolated
// boundary pixels: a proj_nav valid-flip or a nearest/floor rounding flip
// moves a whole sample, so the pass criterion is a small violation
// fraction rather than a max-error bound. Mask bytes (0/1) are compared
// through the same channel; a flipped boundary texel counts as one
// violation.
//
// Coverage: all three wide camera models across the distortion tiers
// (the NONE row also passes null dist_coeffs, the zeros-fallback path),
// u8/u16 image, u8 mask, u16-ray + f32-linear depth
// (GT at a different resolution than the intrinsics reference, exercising
// the sx/sy rescale), u8/f32 normal (with all-zero "no data" sentinel
// pixels), the equirectangular variants (x-wrap sampling), the re-distort
// family at two modality resolutions, both branches of the skewed source
// model and its fold rejection (shaders/camera_source.slang), and the four
// raw byte->float converters with odd element counts (word-tail handling).

#include <backend/tests/DistortionFixture.h>
#include <kernels/pixelwise/PixelWise.cuh>
#include <engine/EngineInternal.h>
#include <core/Tensor.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

using backend::MemcpyKind;

static std::vector<float> g_tight;

template <typename T>
T* upload(const std::vector<T>& host) {
    T* d = (T*)backend::device_malloc(
        ((host.size() * sizeof(T) + 3) / 4) * 4);
    backend::memcpy_sync(d, host.data(), host.size() * sizeof(T),
                         MemcpyKind::HostToDevice);
    return d;
}

template <typename T>
T* alloc_out(int64_t n) {
    int64_t bytes = ((n * (int64_t)sizeof(T) + 3) / 4) * 4;
    T* d = (T*)backend::device_malloc(bytes);
    backend::memset_sync(d, 0, bytes);
    return d;
}

void readback_f(const float* d, int64_t n) {
    size_t off = g_tight.size();
    g_tight.resize(off + n);
    backend::memcpy_sync(g_tight.data() + off, d, n * sizeof(float),
                         MemcpyKind::DeviceToHost);
}

void readback_b(const uint8_t* d, int64_t n) {
    std::vector<uint8_t> h(n);
    backend::memcpy_sync(h.data(), d, n, MemcpyKind::DeviceToHost);
    for (uint8_t v : h) g_tight.push_back((float)v);
}

struct Rng {
    std::mt19937 rng;
    explicit Rng(uint32_t seed) : rng(seed) {}
    float uf(float lo, float hi) {
        return lo + (hi - lo) * (float)(rng() & 0xffffff) / 16777215.0f;
    }
    std::vector<uint8_t> bytes(int64_t n) {
        std::vector<uint8_t> v(n);
        for (auto& x : v) x = (uint8_t)(rng() & 0xff);
        return v;
    }
    std::vector<uint16_t> words(int64_t n, uint16_t max) {
        std::vector<uint16_t> v(n);
        for (auto& x : v) x = (uint16_t)(rng() % (uint32_t)(max + 1));
        return v;
    }
    std::vector<float> vec(int64_t n, float lo, float hi) {
        std::vector<float> v(n);
        for (auto& x : v) x = uf(lo, hi);
        return v;
    }
};

struct V3 {
    float x, y, z;
};
static V3 norm3(V3 a) {
    float l = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
    return {a.x / l, a.y / l, a.z / l};
}
static V3 cross3(V3 a, V3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

// [B*K, 3, 3] face frames (unit rows) and [B*K, 4] face intrinsics, the
// per-post-camera tables the warps take; face k of every image has half-fov
// tangent `s` and an off-centre principal point that wanders with k.
static std::vector<float> make_axes(int B, int K) {
    const V3 zs[4] = {{0, 0, 1},
                      {1, 0, 0},
                      {0.577f, 0.577f, 0.577f},
                      {0, -1, 0}};
    std::vector<float> axes;
    for (int p = 0; p < B * K; p++) {
        V3 z = norm3(zs[p % 4]);
        V3 up = std::fabs(z.y) > 0.9f ? V3{0, 0, 1} : V3{0, 1, 0};
        V3 x = norm3(cross3(up, z));
        V3 y = cross3(z, x);
        float row[9] = {x.x, x.y, x.z, y.x, y.y, y.z, z.x, z.y, z.z};
        axes.insert(axes.end(), row, row + 9);
    }
    return axes;
}
static std::vector<float> make_face_intrins(int B, int K, int W, int H, float s) {
    std::vector<float> intr;
    for (int p = 0; p < B * K; p++) {
        const float f = 0.5f * (float)W / s;
        intr.push_back(f);
        intr.push_back(f * (1.0f + 0.05f * (float)(p % 3)));
        intr.push_back(0.5f * (float)W + 0.15f * (float)W * (float)(p % K));
        intr.push_back(0.5f * (float)H - 0.1f * (float)H * (float)(p % 2));
    }
    return intr;
}

int main(int argc, char** argv) {
    if (argc != 3 ||
        (std::strcmp(argv[1], "dump") && std::strcmp(argv[1], "compare"))) {
        std::fprintf(stderr, "usage: %s dump|compare <ref.bin>\n", argv[0]);
        return 2;
    }
    const bool dumping = std::strcmp(argv[1], "dump") == 0;

    Rng r(260722u);

    // ---- raw byte -> float converters (odd totals exercise word tails) ----
    {
        auto u8img = r.bytes(2 * 13 * 17 * 3);
        float* out = alloc_out<float>(u8img.size());
        uint8_image_to_float_raw(upload(u8img), out, 2, 13, 17, 3);
        readback_f(out, u8img.size());

        auto u16img = r.words(1 * 9 * 21, 65535);
        float* out2 = alloc_out<float>(u16img.size());
        uint16_image_to_float_raw(upload(u16img), out2, 1, 9, 21, 1);
        readback_f(out2, u16img.size());

        auto u8nrm = r.bytes(2 * 11 * 15 * 3);
        for (size_t i = 0; i < u8nrm.size(); i += 97) u8nrm[i] = 0;
        float* out3 = alloc_out<float>(u8nrm.size());
        uint8_normal_to_float_raw(upload(u8nrm), out3, 2, 11, 15, 3);
        readback_f(out3, u8nrm.size());

        auto u16dep = r.words(1 * 14 * 10, 5000);
        float* out4 = alloc_out<float>(u16dep.size());
        uint16_depth_to_float_raw(upload(u16dep), out4, 1, 14, 10, 1);
        readback_f(out4, u16dep.size());
    }

    // ---- wide warps (FISHEYE / EQUISOLID / PINHOLE) -----------------------
    {
        const int B = 2, Hin = 48, Win = 64, K = 3, Hout = 24, Wout = 32;
        const int Hd = 36, Wd = 48;  // GT depth/normal at their own res
        float* d_axes = upload(make_axes(B, K));
        float* d_face = upload(make_face_intrins(B, K, Wout, Hout, 0.7f));

        struct Cam {
            const char* model;
            float fx, fy;
            int dist;  // tier; NONE also drives the null dist_coeffs path
            int source;  // COLMAP model id for the fused re-distort, 0 = none
        };
        const Cam cams[6] = {
            {"FISHEYE", 20.0f, 20.5f, 2, 0},
            {"EQUISOLID", 24.0f, 24.0f, 1, 0},
            {"PINHOLE", 30.0f, 31.0f, 0, 0},
            {"PINHOLE", 30.0f, 31.0f, 3, 0},
            // EUCM source: the warp projects through it instead of the fitted
            // camera, which is the fused re-distort + warp-to-pinhole path.
            {"FISHEYE", 20.0f, 20.5f, 2, 16},
            // The skewed source, its other branch (Metashape b2).
            {"FISHEYE", 20.0f, 20.5f, 2, 1000},
        };
        std::vector<float> dist_rows = dist_fixture::distortion_rows(B);
        const float* d_dist_all = upload(dist_rows);
        for (const Cam& cam : cams) {
            std::vector<float> intr;
            for (int b = 0; b < B; b++) {
                intr.push_back(cam.fx + 0.5f * b);
                intr.push_back(cam.fy + 0.3f * b);
                intr.push_back(0.5f * Win + 0.4f * b);
                intr.push_back(0.5f * Hin - 0.2f * b);
            }
            float* d_intr = upload(intr);
            const float* d_dist =
                cam.dist ? d_dist_all + dist_fixture::row_offset(cam.dist, B)
                         : nullptr;
            const char* d_tier = dist_fixture::kTierNames[cam.dist];

            const int* d_src_models = nullptr;
            const float* d_src_params = nullptr;
            std::vector<int32_t> src_models;
            std::vector<float> src_params;
            if (cam.source) {
                src_models.assign(B, cam.source);
                src_params.assign((size_t)B * 16, 0.0f);
                for (int b = 0; b < B; b++) {
                    float* p = &src_params[(size_t)b * 16];
                    p[0] = intr[b*4 + 0]; p[1] = intr[b*4 + 1];
                    p[2] = intr[b*4 + 2]; p[3] = intr[b*4 + 3];
                    if (cam.source == 1000) {
                        p[4] = 0.35f;                 // skew, pixels
                        p[5] = 0.02f;  p[6] = -0.003f;  // k1 k2
                        // k4 folds this lens at 110 deg, inside the 135 deg
                        // the widest face ray reaches, so source_unfolded runs
                        // both ways here.
                        p[7] = 0.001f; p[8] = -8e-4f;   // k3 k4
                        p[9] = 7e-4f;  p[10] = -4e-4f;  // p1 p2
                        p[11] = 3e-4f; p[12] = -2e-4f;  // sx1 sy1
                        p[13] = 1.0f;                 // equidistant fisheye base
                        p[14] = 0.0f;                 // polynomial radial
                    } else {
                        p[4] = 0.6f;   // EUCM alpha
                        p[5] = 1.1f;   // EUCM beta
                    }
                }
                d_src_models = (const int*)upload(src_models);
                d_src_params = upload(src_params);
            }
            const int64_t n_out = (int64_t)B * K * Hout * Wout;

            // u8 image (+ u16 for the first model)
            {
                auto img = r.bytes((int64_t)B * Hin * Win * 3);
                float* out = alloc_out<float>(n_out * 3);
                launch_warp_byte_to_float_wide(
                    cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params, upload(img), false,
                    B, Hin, Win, 3, out, K, Hout, Wout, d_face, d_axes);
                readback_f(out, n_out * 3);
            }
            if (cam.model[0] == 'F') {
                auto img = r.words((int64_t)B * Hin * Win * 3, 65535);
                float* out = alloc_out<float>(n_out * 3);
                launch_warp_byte_to_float_wide(
                    cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params, upload(img), true,
                    B, Hin, Win, 3, out, K, Hout, Wout, d_face, d_axes);
                readback_f(out, n_out * 3);
            }

            // mask (mostly-set with holes)
            {
                auto m = r.bytes((int64_t)B * Hin * Win);
                for (auto& v : m) v = v < 200 ? 1 : 0;
                uint8_t* out = alloc_out<uint8_t>(n_out);
                launch_warp_mask_wide(cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params,
                                      upload(m), B, Hin, Win, out, K, Hout,
                                      Wout, d_face, d_axes);
                readback_b(out, n_out);
            }

            // u16 ray depth + f32 linear depth at (Hd, Wd)
            {
                auto dep = r.words((int64_t)B * Hd * Wd, 5000);
                for (size_t i = 0; i < dep.size(); i += 53) dep[i] = 0;
                float* out = alloc_out<float>(n_out);
                launch_warp_depth_wide(cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params,
                                       upload(dep), 2, B, Hd, Wd, Hin, Win,
                                       out, K, Hout, Wout, d_face, d_axes, true);
                readback_f(out, n_out);
            }
            {
                auto dep = r.vec((int64_t)B * Hd * Wd, 0.5f, 8.0f);
                for (size_t i = 0; i < dep.size(); i += 41) dep[i] = -0.5f;
                float* out = alloc_out<float>(n_out);
                launch_warp_depth_wide(cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params,
                                       upload(dep), 4, B, Hd, Wd, Hin, Win,
                                       out, K, Hout, Wout, d_face, d_axes, false);
                readback_f(out, n_out);
            }

            // u8 normal at (Hd, Wd) with "no data" holes (+ f32 for the
            // first model)
            {
                auto nrm = r.bytes((int64_t)B * Hd * Wd * 3);
                for (size_t i = 0; i + 2 < nrm.size(); i += 87)
                    nrm[i] = nrm[i + 1] = nrm[i + 2] = 0;
                float* out = alloc_out<float>(n_out * 3);
                launch_warp_normal_wide(cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params,
                                        upload(nrm), 1, B, Hd, Wd, Hin, Win,
                                        out, K, Hout, Wout, d_face, d_axes);
                readback_f(out, n_out * 3);
            }
            if (cam.model[0] == 'F') {
                auto nrm = r.vec((int64_t)B * Hd * Wd * 3, -1.0f, 1.0f);
                float* out = alloc_out<float>(n_out * 3);
                launch_warp_normal_wide(cam.model, d_tier, d_intr, d_dist, d_src_models, d_src_params,
                                        upload(nrm), 4, B, Hd, Wd, Hin, Win,
                                        out, K, Hout, Wout, d_face, d_axes);
                readback_f(out, n_out * 3);
            }
        }
    }

    // ---- re-distort (K == 1): a camera whose lens model no tier represents,
    // resampled from the true source projection onto the fitted tier. The
    // modality grids deliberately DIFFER from the intrinsics reference: an
    // equal-resolution case cannot tell scale_in from scale_out, so it would
    // pass with the source scale applied to the destination step.
    {
        const int B = 2, refW = 42, refH = 58;
        struct Grid { int in_w, in_h, out_w, out_h; };
        const Grid grids[2] = {
            {refW, refH, refW, refH},   // same resolution
            {21, 29, 31, 43},           // half-res source, odd-size destination
        };
        std::vector<float> intr;
        for (int b = 0; b < B; b++) {
            intr.push_back(30.0f + 0.5f * b);
            intr.push_back(31.0f + 0.3f * b);
            intr.push_back(0.5f * refW + 0.4f * b);
            intr.push_back(0.5f * refH - 0.2f * b);
        }
        float* d_intr = upload(intr);
        std::vector<float> dist_rows = dist_fixture::distortion_rows(B);
        const float* d_dist =
            upload(dist_rows) + dist_fixture::row_offset(2, B);   // ThinPrism

        // EUCM (COLMAP model 16), the wide-angle case the fitter targets.
        std::vector<int32_t> src_models(B, 16);
        std::vector<float> src_params((size_t)B * 16, 0.0f);
        for (int b = 0; b < B; b++) {
            float* p = &src_params[(size_t)b * 16];
            p[0] = intr[b*4 + 0]; p[1] = intr[b*4 + 1];
            p[2] = intr[b*4 + 2]; p[3] = intr[b*4 + 3];
            p[4] = 0.6f; p[5] = 1.1f;
        }
        const int* d_src_m = (const int*)upload(src_models);
        const float* d_src_p = upload(src_params);

        for (const Grid& g : grids) {
            const int64_t n_in  = (int64_t)B * g.in_h * g.in_w;
            const int64_t n_out = (int64_t)B * g.out_h * g.out_w;

            auto img = r.bytes(n_in * 3);
            float* o_img = alloc_out<float>(n_out * 3);
            launch_redistort_byte_to_float(
                "FISHEYE", "THIN_PRISM", d_intr, d_dist, d_src_m, d_src_p,
                upload(img), false, B, g.in_h, g.in_w, 3,
                o_img, g.out_h, g.out_w, refH, refW, 0.5f);
            readback_f(o_img, n_out * 3);

            auto msk = r.bytes(n_in);
            for (size_t i = 0; i < msk.size(); i++) msk[i] = (msk[i] > 40) ? 1 : 0;
            uint8_t* o_msk = alloc_out<uint8_t>(n_out);
            launch_redistort_mask(
                "FISHEYE", "THIN_PRISM", d_intr, d_dist, d_src_m, d_src_p,
                upload(msk), B, g.in_h, g.in_w,
                o_msk, g.out_h, g.out_w, refH, refW);
            readback_b(o_msk, n_out);

            // A smooth ramp, not noise: neighbouring taps thousands of counts
            // apart amplify a last-ULP difference in uv_src into a visible
            // depth error that is data range, not geometry.
            std::vector<uint16_t> dep((size_t)n_in);
            for (int64_t i = 0; i < n_in; i++)
                dep[i] = (uint16_t)(1000 + (i % 997));
            for (int64_t i = 0; i < n_in; i += 53) dep[i] = 0;  // "no data"
            float* o_dep = alloc_out<float>(n_out);
            launch_redistort_depth(
                "FISHEYE", "THIN_PRISM", d_intr, d_dist, d_src_m, d_src_p,
                upload(dep), 2, B, g.in_h, g.in_w,
                o_dep, g.out_h, g.out_w, refH, refW);
            readback_f(o_dep, n_out);

            auto nrm = r.bytes(n_in * 3);
            for (size_t i = 0; i + 2 < nrm.size(); i += 87)
                nrm[i] = nrm[i + 1] = nrm[i + 2] = 0;   // "no data" sentinel
            float* o_nrm = alloc_out<float>(n_out * 3);
            launch_redistort_normal(
                "FISHEYE", "THIN_PRISM", d_intr, d_dist, d_src_m, d_src_p,
                upload(nrm), false, B, g.in_h, g.in_w,
                o_nrm, g.out_h, g.out_w, refH, refW);
            readback_f(o_nrm, n_out * 3);
        }

        // The skewed source's rational branch on a perspective base -- the one
        // combination the wide config above does not reach.
        {
            std::vector<int32_t> sm(B, 1000);
            std::vector<float> sp((size_t)B * 16, 0.0f);
            for (int b = 0; b < B; b++) {
                float* p = &sp[(size_t)b * 16];
                p[0] = intr[b*4 + 0]; p[1] = intr[b*4 + 1];
                p[2] = intr[b*4 + 2]; p[3] = intr[b*4 + 3];
                p[4] = 0.35f;                                    // skew
                p[5] = -0.28f; p[6] = 0.11f;  p[7] = -0.02f;     // k1 k2 k3
                p[8] = 0.9f;   p[9] = 0.05f;  p[10] = -0.004f;   // k4 k5 k6
                p[11] = 7e-4f; p[12] = -4e-4f;                   // p1 p2
                p[13] = 0.0f;   // perspective base
                p[14] = 1.0f;   // rational radial
            }
            const int64_t n_in  = (int64_t)B * refH * refW;
            const int64_t n_out = n_in;
            auto img = r.bytes(n_in * 3);
            float* out = alloc_out<float>(n_out * 3);
            launch_redistort_byte_to_float(
                "PINHOLE", "RATIONAL", d_intr,
                upload(dist_rows) + dist_fixture::row_offset(3, B),
                (const int*)upload(sm), upload(sp),
                upload(img), false, B, refH, refW, 3,
                out, refH, refW, refH, refW, 0.5f);
            readback_f(out, n_out * 3);
        }
    }

    // ---- equirectangular warps -------------------------------------------
    {
        const int B = 1, Hin = 32, Win = 64, K = 4, Hout = 16, Wout = 16;
        float* d_axes = upload(make_axes(B, K));
        float* d_face = upload(make_face_intrins(B, K, Wout, Hout, 0.8f));
        const int64_t n_out = (int64_t)B * K * Hout * Wout;

        {
            auto img = r.bytes((int64_t)B * Hin * Win * 3);
            float* out = alloc_out<float>(n_out * 3);
            launch_warp_byte_to_float_equi(upload(img), false, B, Hin, Win,
                                           3, out, K, Hout, Wout, d_face, d_axes);
            readback_f(out, n_out * 3);
        }
        {
            auto img = r.words((int64_t)B * Hin * Win * 3, 65535);
            float* out = alloc_out<float>(n_out * 3);
            launch_warp_byte_to_float_equi(upload(img), true, B, Hin, Win,
                                           3, out, K, Hout, Wout, d_face, d_axes);
            readback_f(out, n_out * 3);
        }
        {
            auto m = r.bytes((int64_t)B * Hin * Win);
            for (auto& v : m) v = v < 180 ? 1 : 0;
            uint8_t* out = alloc_out<uint8_t>(n_out);
            launch_warp_mask_equi(upload(m), B, Hin, Win, out, K, Hout,
                                  Wout, d_face, d_axes);
            readback_b(out, n_out);
        }
        {
            auto dep = r.words((int64_t)B * Hin * Win, 5000);
            for (size_t i = 0; i < dep.size(); i += 53) dep[i] = 0;
            float* out = alloc_out<float>(n_out);
            launch_warp_depth_equi(upload(dep), 2, B, Hin, Win, out, K,
                                   Hout, Wout, d_face, d_axes, true);
            readback_f(out, n_out);
        }
        {
            auto dep = r.vec((int64_t)B * Hin * Win, 0.5f, 8.0f);
            for (size_t i = 0; i < dep.size(); i += 41) dep[i] = -0.5f;
            float* out = alloc_out<float>(n_out);
            launch_warp_depth_equi(upload(dep), 4, B, Hin, Win, out, K,
                                   Hout, Wout, d_face, d_axes, false);
            readback_f(out, n_out);
        }
        {
            auto nrm = r.bytes((int64_t)B * Hin * Win * 3);
            for (size_t i = 0; i + 2 < nrm.size(); i += 87)
                nrm[i] = nrm[i + 1] = nrm[i + 2] = 0;
            float* out = alloc_out<float>(n_out * 3);
            launch_warp_normal_equi(upload(nrm), 1, B, Hin, Win, out, K,
                                    Hout, Wout, d_face, d_axes);
            readback_f(out, n_out * 3);
        }
        {
            auto nrm = r.vec((int64_t)B * Hin * Win * 3, -1.0f, 1.0f);
            float* out = alloc_out<float>(n_out * 3);
            launch_warp_normal_equi(upload(nrm), 4, B, Hin, Win, out, K,
                                    Hout, Wout, d_face, d_axes);
            readback_f(out, n_out * 3);
        }
    }

    backend::device_synchronize();
    if (const char* err = backend::last_error()) {
        std::fprintf(stderr, "backend error: %s\n", err);
        return 1;
    }

    const int64_t nf = (int64_t)g_tight.size();
    if (dumping) {
        std::ofstream f(argv[2], std::ios::binary);
        f.write((const char*)&nf, 8);
        f.write((const char*)g_tight.data(), nf * 4);
        std::printf("warp_parity: dumped %lld floats to %s\n",
                    (long long)nf, argv[2]);
        return 0;
    }

    std::ifstream f(argv[2], std::ios::binary);
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", argv[2]);
        return 2;
    }
    int64_t rn = 0;
    f.read((char*)&rn, 8);
    if (rn != nf) {
        std::fprintf(stderr, "float count mismatch: ref %lld vs got %lld\n",
                     (long long)rn, (long long)nf);
        return 1;
    }
    std::vector<float> ref(nf);
    f.read((char*)ref.data(), nf * 4);

    int64_t viol = 0;
    double max_abs = 0;
    for (int64_t i = 0; i < nf; i++) {
        double d = std::fabs((double)g_tight[i] - (double)ref[i]);
        double tol = 5e-3 + 1e-3 * std::fabs((double)ref[i]);
        max_abs = std::max(max_abs, d);
        if (d > tol) viol++;
    }
    double frac = nf ? (double)viol / (double)nf : 0.0;
    std::printf("warp_parity: %lld floats (max_abs %.3g, violations %lld = "
                "%.5f%%)\n",
                (long long)nf, max_abs, (long long)viol, 100.0 * frac);
    bool pass = frac <= 2e-3;
    std::printf(pass ? "warp_parity: PASSED\n" : "warp_parity: FAILED\n");
    return pass ? 0 : 1;
}
