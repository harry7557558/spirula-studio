// AppearanceChain.cu -- background blend, PPISP and the display encode as one
// kernel each way (AppearanceChainParams in PixelWise.cuh).
//
// Part of the PixelWise family -- see PixelWiseCommon.cuh.

#include "kernels/pixelwise/BackgroundNoise.cuh"
#include "core/Interpolation.cuh"
#include "core/PixelFormat.h"

#include <cooperative_groups.h>
#include <cooperative_groups/reduce.h>
namespace cg = cooperative_groups;

// PL is a PpispParamLayout, or -1 when no PPISP stage runs.
template<int PL>
static constexpr int kAppParams =
    PL < 0 ? 1 : ppisp_layout_num_params((PpispParamLayout)PL);

template<int PL>
__device__ __forceinline__ float3 _app_ppisp(
    float3 c, float2 pix, float2 center, float2 size, bool clamp,
    const FixedArray<float, kAppParams<PL>>& params
) {
    if constexpr (PL == (int)PpispParamLayout::Original)
        return SlangPPISP::apply_ppisp(c, pix, center, size, params);
    else if constexpr (PL == (int)PpispParamLayout::RQS)
        return SlangPPISP::apply_ppisp_rqs(c, pix, center, size, params);
    else if constexpr (PL == (int)PpispParamLayout::NoCRF)
        return SlangPPISP::apply_ppisp_no_crf(c, pix, center, size, params, clamp);
    else if constexpr (PL == (int)PpispParamLayout::NoCRFNoVig)
        return SlangPPISP::apply_ppisp_no_crf_no_vig(c, pix, center, size, params,
                                                     clamp);
    else
        return c;
}

template<int PL>
__device__ __forceinline__ float3 _app_ppisp_vjp(
    float3 c, float2 pix, float2 center, float2 size, bool clamp,
    const FixedArray<float, kAppParams<PL>>& params, float3 v_out,
    FixedArray<float, kAppParams<PL>>& v_params
) {
    float3 v_c = make_float3(0.0f, 0.0f, 0.0f);
    FixedArray<float, kAppParams<PL>> v;
    if constexpr (PL == (int)PpispParamLayout::Original)
        SlangPPISP::apply_ppisp_vjp(c, pix, center, size, params, v_out, &v_c, &v);
    else if constexpr (PL == (int)PpispParamLayout::RQS)
        SlangPPISP::apply_ppisp_rqs_vjp(c, pix, center, size, params, v_out,
                                        &v_c, &v);
    else if constexpr (PL == (int)PpispParamLayout::NoCRF)
        SlangPPISP::apply_ppisp_no_crf_vjp(c, pix, center, size, params, clamp,
                                           v_out, &v_c, &v);
    else if constexpr (PL == (int)PpispParamLayout::NoCRFNoVig)
        SlangPPISP::apply_ppisp_no_crf_no_vig_vjp(c, pix, center, size, params,
                                                  clamp, v_out, &v_c, &v);
    if constexpr (PL >= 0) {
        #pragma unroll
        for (int i = 0; i < kAppParams<PL>; i++) v_params[i] += v[i];
    }
    return v_c;
}

__device__ __forceinline__ float3x3 _app_matrix(const float* m) {
    float3x3 r;
    r[0].x = m[0]; r[0].y = m[1]; r[0].z = m[2];
    r[1].x = m[3]; r[1].y = m[4]; r[1].z = m[5];
    r[2].x = m[6]; r[2].y = m[7]; r[2].z = m[8];
    return r;
}

// `drawn`: the randomized draw replaced the base here, so no v_bg flows back.
__device__ __forceinline__ float3 _app_background(
    const AppearanceChainParams& p, unsigned bid, unsigned x, unsigned y,
    unsigned W, unsigned H, size_t pix, bool& drawn
) {
    drawn = p.bg_noise != 0 &&
        !(p.bg_mask && (nearest_sample_u8(p.bg_mask, (int)bid, (int)x, (int)y, (int)W,
                                          (int)H, p.bg_mask_w, p.bg_mask_h) & 2));
    if (drawn)
        return _bg_color(p.bg_transfer, p.bg_is_linear != 0, p.bg_blocky != 0,
                         p.bg_block_px, p.bg_seed, bid, x, y, W,
                         p.bg_randomize_weight,
                         _bg_exponent(bid, p.bg_exponent_by_cam, p.cam_indices));
    switch (p.bg) {
    case AppearanceBg::Color:
        return make_float3(p.bg_color[0], p.bg_color[1], p.bg_color[2]);
    case AppearanceBg::Image:
        return ((const float3*)p.bg_image)[pix];
    default:
        return make_float3(0.0f, 0.0f, 0.0f);
    }
}

template<int PL>
__global__ void appearance_chain_forward_kernel(
    const AppearanceChainParams p,
    const unsigned B, const unsigned H, const unsigned W,
    const PixelPtr in_rgb,
    const TensorView<float, 4> in_transmittance,
    const PixelOut out_rgb,
    const PixelOut raw16
) {
    const unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned bid = blockIdx.y;
    if (bid >= B || gid >= H * W) return;
    const unsigned y = gid / W, x = gid % W;
    const size_t pix = (size_t)bid * H * W + gid;

    float3 c = make_float3(in_rgb[3 * pix], in_rgb[3 * pix + 1], in_rgb[3 * pix + 2]);
    // From the copy on, the chain sees what its backward will replay from.
    if (raw16.p) {
        c = make_float3(__half2float(__float2half(c.x)), __half2float(__float2half(c.y)),
                        __half2float(__float2half(c.z)));
        raw16[3 * pix] = c.x;
        raw16[3 * pix + 1] = c.y;
        raw16[3 * pix + 2] = c.z;
    }
    if (appearance_has_bg(p)) {
        bool drawn;
        const float3 bg = _app_background(p, bid, x, y, W, H, pix, drawn);
        c = SlangPixelWise::blend_background(c, in_transmittance.load1(bid, y, x), bg);
    }
    if constexpr (PL >= 0) {
        const int slot = p.cam_indices ? p.cam_indices[bid] : (int)bid;
        FixedArray<float, kAppParams<PL>> params;
        #pragma unroll
        for (int i = 0; i < kAppParams<PL>; i++)
            params[i] = p.ppisp_params[slot * kAppParams<PL> + i];
        const float4 intr = ((const float4*)p.intrins)[bid];
        const float2 pix = make_float2((float)x, (float)y);
        const float2 center = make_float2(intr.z, intr.w);
        const float2 size = make_float2((float)W, (float)H);
        if (p.ppisp == AppearancePpisp::BeforeEncode)
            c = _app_ppisp<PL>(c, pix, center, size, p.ppisp_clamp != 0, params);
        if (p.cs_enabled)
            c = SlangPixelWise::working_to_display(c, _app_matrix(p.cs_matrix),
                                                   p.cs_transfer, p.cs_is_linear != 0);
        if (p.ppisp == AppearancePpisp::AfterEncode)
            c = _app_ppisp<PL>(c, pix, center, size, p.ppisp_clamp != 0, params);
    } else if (p.cs_enabled) {
        c = SlangPixelWise::working_to_display(c, _app_matrix(p.cs_matrix),
                                               p.cs_transfer, p.cs_is_linear != 0);
    }
    out_rgb[3 * pix] = c.x;
    out_rgb[3 * pix + 1] = c.y;
    out_rgb[3 * pix + 2] = c.z;
}

// One block covers one image, so the PPISP parameter gradient is reduced over
// the block before its one atomic per parameter.
constexpr unsigned kAppBwdBlock = 256;

template<int PL>
__global__ void appearance_chain_backward_kernel(
    const AppearanceChainParams p,
    const float overexposure_scale,
    const unsigned B, const unsigned H, const unsigned W,
    const PixelPtr in_rgb,
    const TensorView<float, 4> in_transmittance,
    TensorView<float, 4> v_rgb,
    TensorView<float, 4> v_transmittance
) {
    const unsigned gid = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned bid = blockIdx.y;
    const bool inside = bid < B && gid < H * W;
    const unsigned y = inside ? gid / W : 0u, x = inside ? gid % W : 0u;
    constexpr int P = kAppParams<PL>;

    // maybe_unused: only `if constexpr (PL >= 0)` reads these.
    [[maybe_unused]] FixedArray<float, P> params;
    FixedArray<float, P> v_params;
    #pragma unroll
    for (int i = 0; i < P; i++) v_params[i] = 0.0f;
    [[maybe_unused]] int slot = 0;
    if constexpr (PL >= 0) {
        __shared__ float params_shared[P];
        slot = p.cam_indices ? p.cam_indices[bid] : (int)bid;
        for (int i = threadIdx.x; i < P; i += blockDim.x)
            params_shared[i] = p.ppisp_params[slot * P + i];
        __syncthreads();
        #pragma unroll
        for (int i = 0; i < P; i++) params[i] = params_shared[i];
    }

    if (inside) {
        const size_t pix = (size_t)bid * H * W + gid;
        const float3 raw = make_float3(in_rgb[3 * pix], in_rgb[3 * pix + 1],
                                       in_rgb[3 * pix + 2]);
        const bool has_bg = appearance_has_bg(p);
        const float T = has_bg ? in_transmittance.load1(bid, y, x) : 0.0f;
        bool drawn = false;
        const float3 bg = has_bg ? _app_background(p, bid, x, y, W, H, pix, drawn)
                                 : make_float3(0.0f, 0.0f, 0.0f);
        const float3 c_bg = has_bg ? SlangPixelWise::blend_background(raw, T, bg) : raw;
        float3 v = v_rgb.load3(bid, y, x);

        // Every stage is linear in the incoming gradient, so a zero one -- a
        // masked pixel, mostly -- skips the recompute.
        if (v.x != 0.0f || v.y != 0.0f || v.z != 0.0f) {
            const float4 intr = PL >= 0 ? ((const float4*)p.intrins)[bid]
                                        : make_float4(0.0f, 0.0f, 0.0f, 0.0f);
            const float2 px = make_float2((float)x, (float)y);
            const float2 center = make_float2(intr.z, intr.w);
            const float2 size = make_float2((float)W, (float)H);
            [[maybe_unused]] const bool clamp = p.ppisp_clamp != 0;
            float3 c_pre = c_bg;
            if constexpr (PL >= 0)
                if (p.ppisp == AppearancePpisp::BeforeEncode)
                    c_pre = _app_ppisp<PL>(c_bg, px, center, size, clamp, params);
            if constexpr (PL >= 0) {
                if (p.ppisp == AppearancePpisp::AfterEncode) {
                    const float3 c_enc = p.cs_enabled
                        ? SlangPixelWise::working_to_display(
                              c_pre, _app_matrix(p.cs_matrix), p.cs_transfer,
                              p.cs_is_linear != 0)
                        : c_pre;
                    v = _app_ppisp_vjp<PL>(c_enc, px, center, size, clamp, params,
                                           v, v_params);
                }
            }
            if (p.cs_enabled)
                v = SlangPixelWise::working_to_display_bwd(
                    c_pre, _app_matrix(p.cs_matrix), p.cs_transfer,
                    p.cs_is_linear != 0, v);
            if constexpr (PL >= 0)
                if (p.ppisp == AppearancePpisp::BeforeEncode)
                    v = _app_ppisp_vjp<PL>(c_bg, px, center, size, clamp, params,
                                           v, v_params);
        }

        if (has_bg) {
            float3 v_raw, v_bg; float v_T;
            SlangPixelWise::blend_background_bwd(raw, T, bg, v, overexposure_scale,
                                                 &v_raw, &v_T, &v_bg);
            v = v_raw;
            v_transmittance.store1(bid, y, x,
                                   v_transmittance.load1(bid, y, x) + v_T);
            if (p.bg == AppearanceBg::Image)
                ((float3*)p.v_bg_image)[pix] =
                    drawn ? make_float3(0.0f, 0.0f, 0.0f) : v_bg;
        } else if (overexposure_scale != 0.0f) {
            v = v + SlangPixelWise::overexposure_grad(raw, overexposure_scale);
        }
        v_rgb.store3(bid, y, x, v);
    }

    if constexpr (PL >= 0) {
        __shared__ float partial[kAppBwdBlock / WARP_SIZE];
        auto block = cg::this_thread_block();
        cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
        const unsigned lane = threadIdx.x % WARP_SIZE, wid = threadIdx.x / WARP_SIZE;
        #pragma unroll
        for (int i = 0; i < P; i++) {
            float g = isfinite(v_params[i]) ? v_params[i] : 0.0f;
            g = cg::reduce(warp, g, cg::plus<float>());
            if (lane == 0) partial[wid] = g;
            __syncthreads();
            if (wid == 0) {
                g = lane < kAppBwdBlock / WARP_SIZE ? partial[lane] : 0.0f;
                g = cg::reduce(warp, g, cg::plus<float>());
                if (lane == 0 && g != 0.0f)
                    atomicAdd(&p.v_ppisp_params[slot * P + i], g);
            }
            __syncthreads();
        }
    }
}

template<class F>
static void _with_app_layout(const AppearanceChainParams& p, F&& f) {
    if (p.ppisp == AppearancePpisp::Off) { f(std::integral_constant<int, -1>{}); return; }
    switch (p.ppisp_layout) {
    case PpispParamLayout::Original:
        f(std::integral_constant<int, (int)PpispParamLayout::Original>{}); break;
    case PpispParamLayout::RQS:
        f(std::integral_constant<int, (int)PpispParamLayout::RQS>{}); break;
    case PpispParamLayout::NoCRF:
        f(std::integral_constant<int, (int)PpispParamLayout::NoCRF>{}); break;
    case PpispParamLayout::NoCRFNoVig:
        f(std::integral_constant<int, (int)PpispParamLayout::NoCRFNoVig>{}); break;
    }
}

static PixelPtr _app_in(const TorchTensorView& tv) {
    return PixelPtr((const void*)std::get<0>(tv), pixel_format(tv));
}
static PixelOut _app_out(const TorchTensorView& tv) {
    return PixelOut((void*)std::get<0>(tv), pixel_format(tv));
}

/*[AutoHeaderGeneratorExport]*/
void appearance_chain_forward(
    const AppearanceChainParams& p,
    TorchTensorView rgb,                   // [B, H, W, 3] raw render, float32 or float16
    DeviceTensor3D<float> transmittance,   // [B, H, W, 1]
    TorchTensorView out_rgb,               // [B, H, W, 3] float32 or float16
    TorchTensorView raw16                  // a float16 copy of `rgb`, or null
) {
    const auto& s = std::get<2>(rgb);
    long b = s[0], h = s[1], w = s[2];
    if (b * h * w == 0) return;
    _with_app_layout(p, [&](auto L) {
        appearance_chain_forward_kernel<L.value><<<_LAUNCH_ARGS_2D(h*w, b, 256, 1)>>>(
            p, (unsigned)b, (unsigned)h, (unsigned)w, _app_in(rgb),
            _dt3d_to_tv4<float>(transmittance), _app_out(out_rgb), _app_out(raw16));
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
}

/*[AutoHeaderGeneratorExport]*/
void appearance_chain_backward(
    const AppearanceChainParams& p,
    TorchTensorView rgb,                     // [B, H, W, 3] as the forward read it
    DeviceTensor3D<float>  transmittance,    // [B, H, W, 1]
    DeviceTensor3D<float3> v_rgb,            // in: d/d output, out: d/d raw render
    DeviceTensor3D<float>  v_transmittance   // [B, H, W, 1], added into
) {
    const auto& s = std::get<2>(rgb);
    long b = s[0], h = s[1], w = s[2];
    if (b * h * w == 0) return;
    // 2 * weight / N for L = weight * mean(max(-x, x-1, 0)^2) over N = B*H*W*3.
    const float oe_scale = p.overexposure_weight == 0.0f ? 0.0f
        : (float)(2.0 * (double)p.overexposure_weight / ((double)b * h * w * 3.0));
    _with_app_layout(p, [&](auto L) {
        appearance_chain_backward_kernel<L.value>
        <<<_LAUNCH_ARGS_2D(h*w, b, kAppBwdBlock, 1)>>>(
            p, oe_scale, (unsigned)b, (unsigned)h, (unsigned)w, _app_in(rgb),
            _dt3d_to_tv4<float>(transmittance), _dt3d_to_tv4<float>(v_rgb),
            _dt3d_to_tv4<float>(v_transmittance));
    });
    CHECK_DEVICE_ERROR(cudaGetLastError());
}
