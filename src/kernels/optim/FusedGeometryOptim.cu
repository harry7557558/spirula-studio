// FusedGeometryOptim.cu -- fused 3DGS geometry optimizer (means, scales, quats, opacities in one pass).
//
// Part of the Optimizer family -- see OptimizerCommon.cuh.

#include "kernels/optim/OptimizerCommon.cuh"
#include "kernels/optim/ScreenSizeHinge.cuh"

// ================
// Fused geometry optimizer
// ================

// Per-attribute non-SH Adam-state quantization helper. Identical math to the
// FPBO kernel's _NonShQ helper (kept inline here to avoid pulling FPBO
// templates into Optimizer.cu's TU). PRIMS is the per-splat cell count
// (means/scales/dc=3, quats=4, opacities=1).
template<int PRIMS>
struct _OptimNonShQ {
    using NQB = QuantizedAdamState<16, 256>;
    __device__ static inline void decode(
        const uint8_t* __restrict__ packed, const float4* __restrict__ bounds,
        int64_t gid, float* __restrict__ g1_out, float* __restrict__ g2_out
    ) {
        float4 mm_old = bounds[blockIdx.x];
        int64_t base = (int64_t)PRIMS * gid;
        #pragma unroll
        for (int i = 0; i < PRIMS; ++i) {
            float2 ab = NQB::decode_g1g2(packed, base + i, mm_old);
            g1_out[i] = ab.x;
            g2_out[i] = ab.y;
        }
    }
    __device__ static inline void accumulate(
        const float* __restrict__ g1, const float* __restrict__ g2, float4& mm
    ) {
        #pragma unroll
        for (int i = 0; i < PRIMS; ++i) {
            float2 us = NQB::g1g2_to_us(g1[i], g2[i]);
            mm.x = fminf(mm.x, us.x);
            mm.y = fmaxf(mm.y, us.x);
            mm.z = fminf(mm.z, us.y);
            mm.w = fmaxf(mm.w, us.y);
        }
    }
    __device__ static inline void encode(
        uint8_t* __restrict__ packed, int64_t gid,
        const float* __restrict__ g1, const float* __restrict__ g2, float4 mm
    ) {
        int64_t base = (int64_t)PRIMS * gid;
        #pragma unroll
        for (int i = 0; i < PRIMS; ++i) {
            NQB::encode_g1g2(packed, base + i, g1[i], g2[i], mm);
        }
    }
};

// Block reduction over a float4 (paired min/max for u and sqrt_g2).
template<int BLOCK_SIZE>
__device__ inline float4 _optim_block_reduce_minmax_f4(float4 mm) {
    // cg::reduce's comparators keep or drop a NaN depending on which side it
    // lands; one in a block bound decodes all 256 cells to NaN. Vulkan's
    // _oq_min/_oq_max (optim_quant.slang) drops it the same way.
    if (!isfinite(mm.x)) mm.x =  1e30f;
    if (!isfinite(mm.y)) mm.y = -1e30f;
    if (!isfinite(mm.z)) mm.z =  1e30f;
    if (!isfinite(mm.w)) mm.w = -1e30f;
    cg::thread_block       block = cg::this_thread_block();
    cg::thread_block_tile<WARP_SIZE> warp = cg::tiled_partition<WARP_SIZE>(block);
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    mm.z = cg::reduce(warp, mm.z, cg::less<float>());
    mm.w = cg::reduce(warp, mm.w, cg::greater<float>());
    __shared__ float4 shared_reduce[BLOCK_SIZE / WARP_SIZE];
    if (threadIdx.x % WARP_SIZE == 0)
        shared_reduce[threadIdx.x / WARP_SIZE] = mm;
    __syncthreads();
    mm = (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        ? shared_reduce[threadIdx.x]
        : float4{1e30f, -1e30f, 1e30f, -1e30f};
    mm.x = cg::reduce(warp, mm.x, cg::less<float>());
    mm.y = cg::reduce(warp, mm.y, cg::greater<float>());
    mm.z = cg::reduce(warp, mm.z, cg::less<float>());
    mm.w = cg::reduce(warp, mm.w, cg::greater<float>());
    __syncthreads();
    if (threadIdx.x < BLOCK_SIZE / WARP_SIZE)
        shared_reduce[threadIdx.x] = mm;
    __syncthreads();
    return shared_reduce[threadIdx.x / WARP_SIZE];
}


template<bool use_scale_agnostic_mean, bool zero_grad, bool non_sh_quant>
__global__ void fused_optim_3dgs_geometry_kernel(
    float3* __restrict__ means,
    float3* __restrict__ v_means,
    float3* __restrict__ g1_means,
    float3* __restrict__ g2_means,
    float4* __restrict__ quats,
    float4* __restrict__ v_quats,
    float4* __restrict__ g1_quats,
    float4* __restrict__ g2_quats,
    float3* __restrict__ scales,
    float3* __restrict__ v_scales,
    float3* __restrict__ g1_scales,
    float3* __restrict__ g2_scales,
    float* __restrict__ opacities,
    float* __restrict__ v_opacities,
    float* __restrict__ g1_opacities,
    float* __restrict__ g2_opacities,
    // features_dc -- only consumed when non_sh_quant is on; the non-quant path
    // continues to drive features_dc via the separate fused_adam_step call.
    float3* __restrict__ features_dc,
    float3* __restrict__ v_features_dc,
    const float* __restrict__ radii,
    // [N] optional densification score output: ||dL/dmean_world|| * max
    // post-exp world scale, written per splat when non-null (see
    // DensifyConfig::score_blend_world_grad).
    float* __restrict__ densify_score,
    float lr_means,
    float lr_quats,
    float lr_scales,
    float lr_opacs,
    float lr_features_dc,
    const float max_gauss_ratio,
    const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight,
    const float mcmc_scale_reg_weight,
    const float erank_reg_weight,
    const float erank_reg_weight_s3,
    const float quat_norm_reg_weight,
    const float dc_reg_weight,
    const float sh_reg_weight,
    const float max_screen_size,
    const float max_screen_size_penalty,
    const float grad_scale,
    // Non-SH Adam-state quantization bundle. Only read when non_sh_quant is on.
    const NonShQuantState non_sh,
    // Block-wise QUANTIZED gradient input (non-FPBO grad-quant path). For each
    // attribute whose *_packed is non-null the per-splat grad is decoded from
    // the codec instead of the fp32 v_* buffer (which is unallocated then);
    // 3dgut leaves means/quats/scales null so they stay fp32. Bound index is
    // blockIdx.x (256 splats/block, matching the encode kernel).
    const GradQuantBuffers gq,
    const int32_t scalar_step,
    const int32_t* __restrict__ steps,
    const int64_t numel
) {
    static constexpr float eps = 1e-15f;
    static constexpr float beta1 = 0.9f;
    static constexpr float beta2 = 0.999f;
    [[maybe_unused]] static constexpr int   BLOCK_SIZE = 256;
    [[maybe_unused]] static constexpr float kSh0 = 0.28209479177387814f;  // 1 / (2*sqrt(pi))

    const int64_t idx = (int64_t)blockIdx.x * (int64_t)blockDim.x + (int64_t)threadIdx.x;
    bool inside = (idx < numel);

    // Per-thread non-SH-quant state. Hoisted to function scope so the
    // post-update block reduction can see all attrs together; only used when
    // non_sh_quant is on.
    [[maybe_unused]] float3 nq_g1_scale = make_float3(0.f), nq_g2_scale = make_float3(0.f);
    [[maybe_unused]] float4 nq_g1_quat  = make_float4(0.f), nq_g2_quat  = make_float4(0.f);
    [[maybe_unused]] float  nq_g1_opac = 0.f, nq_g2_opac = 0.f;
    [[maybe_unused]] float3 nq_g1_mean = make_float3(0.f), nq_g2_mean = make_float3(0.f);
    [[maybe_unused]] float3 nq_g1_dc   = make_float3(0.f), nq_g2_dc   = make_float3(0.f);
    [[maybe_unused]] float4 nq_mm_scale = make_float4(1e30f, -1e30f, 1e30f, -1e30f);
    [[maybe_unused]] float4 nq_mm_quat  = make_float4(1e30f, -1e30f, 1e30f, -1e30f);
    [[maybe_unused]] float4 nq_mm_opac  = make_float4(1e30f, -1e30f, 1e30f, -1e30f);
    [[maybe_unused]] float4 nq_mm_mean  = make_float4(1e30f, -1e30f, 1e30f, -1e30f);
    [[maybe_unused]] float4 nq_mm_dc    = make_float4(1e30f, -1e30f, 1e30f, -1e30f);

    if (inside) {
        float step = (float)(steps != nullptr ? steps[idx] : scalar_step);
        float inv_bias_correction1 = 1.0f / (1.0f - powf(beta1, step));
        float inv_bias_correction2 = 1.0f / (1.0f - powf(beta2, step));
        lr_means *= inv_bias_correction1;
        lr_quats *= inv_bias_correction1;
        lr_scales *= inv_bias_correction1;
        lr_opacs *= inv_bias_correction1;
        lr_features_dc *= inv_bias_correction1;

        // load params
        float3 scale = scales[idx];
        float4 quat = normalize(quats[idx]);
        float opac = opacities[idx];

        // load gradient, with regularization
        static constexpr int kNumPerSplatLosses = 5;
        FixedArray<float, kNumPerSplatLosses> v_losses = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        float3 v_scale = make_float3(0.0f);
        float4 v_quat = make_float4(0.0f);
        float v_opac = 0.0f;
        SlangPerSplatLosses::per_splat_losses_bwd(
            scale, opac, quat, v_losses,
            &v_scale, &v_opac, &v_quat,
            mcmc_opacity_reg_weight,
            mcmc_scale_reg_weight,
            max_gauss_ratio,
            scale_regularization_weight,
            erank_reg_weight,
            erank_reg_weight_s3,
            quat_norm_reg_weight
        );
        // Data gradient: decode from the block-wise quantized grad buffer when
        // present (3dgs/mip), else read the fp32 v_* buffer (3dgut geom / off).
        float3 gd_scale; float4 gd_quat; float gd_opac;
        if (gq.scales_packed) { float _v[3]; gradq::Codec<16>::decode(gq.scales_packed, (int64_t)3*idx, 3, gq.scales_bounds[blockIdx.x], _v); gd_scale = make_float3(_v[0], _v[1], _v[2]); }
        else                  { gd_scale = v_scales[idx]; }
        if (gq.quats_packed)  { float _v[4]; gradq::Codec<16>::decode(gq.quats_packed,  (int64_t)4*idx, 4, gq.quats_bounds[blockIdx.x],  _v); gd_quat  = make_float4(_v[0], _v[1], _v[2], _v[3]); }
        else                  { gd_quat = v_quats[idx]; }
        if (gq.opac_packed)   { float _v[1]; gradq::Codec<16>::decode(gq.opac_packed,   (int64_t)1*idx, 1, gq.opac_bounds[blockIdx.x],   _v); gd_opac  = _v[0]; }
        else                  { gd_opac = v_opacities[idx]; }
        v_scale += grad_scale * gd_scale;
        v_quat  += grad_scale * gd_quat;
        v_opac  += grad_scale * gd_opac;
        if constexpr (zero_grad) {
            if (!gq.scales_packed) v_scales[idx] = make_float3(0.0f);
            if (!gq.quats_packed)  v_quats[idx] = make_float4(0.0f);
            if (!gq.opac_packed)   v_opacities[idx] = 0.0f;
        }

        // update scales
        float3 g1_scale, g2_scale;
        if constexpr (non_sh_quant) {
            _OptimNonShQ<3>::decode(non_sh.scales_packed, non_sh.scales_bounds, idx,
                                    (float*)&g1_scale, (float*)&g2_scale);
        } else {
            g1_scale = g1_scales[idx];
            g2_scale = g2_scales[idx];
        }
        if (radii != nullptr)
            v_scale += screen_size_hinge_grad(
                radii[idx], max_screen_size, max_screen_size_penalty, scale,
                sqrtf(g2_scale * inv_bias_correction2) + eps);
        g1_scale = beta1 * g1_scale + (1.f - beta1) * v_scale;
        g2_scale = beta2 * g2_scale + (1.f - beta2) * v_scale*v_scale;
        float3 updated_scale = scale - lr_scales * g1_scale / (sqrtf(g2_scale * inv_bias_correction2) + eps);
        scales[idx] = updated_scale;
        if constexpr (non_sh_quant) {
            nq_g1_scale = g1_scale;
            nq_g2_scale = g2_scale;
            _OptimNonShQ<3>::accumulate((float*)&g1_scale, (float*)&g2_scale, nq_mm_scale);
        } else {
            g1_scales[idx] = g1_scale;
            g2_scales[idx] = g2_scale;
        }

        // update quats (Riemannian)
        v_quat -= dot(quat, v_quat) * quat;
        float4 g1_quat, g2_quat;
        if constexpr (non_sh_quant) {
            _OptimNonShQ<4>::decode(non_sh.quats_packed, non_sh.quats_bounds, idx,
                                    (float*)&g1_quat, (float*)&g2_quat);
        } else {
            g1_quat = g1_quats[idx];
            g2_quat = g2_quats[idx];
        }
        g1_quat = beta1 * g1_quat + (1.f - beta1) * v_quat;
        g2_quat = beta2 * g2_quat + (1.f - beta2) * v_quat*v_quat;
        quats[idx] = normalize(quat - lr_quats * g1_quat / (sqrtf(g2_quat * inv_bias_correction2) + eps));
        if constexpr (non_sh_quant) {
            nq_g1_quat = g1_quat;
            nq_g2_quat = g2_quat;
            _OptimNonShQ<4>::accumulate((float*)&g1_quat, (float*)&g2_quat, nq_mm_quat);
        } else {
            g1_quats[idx] = g1_quat;
            g2_quats[idx] = g2_quat;
        }

        // update opacs
        float g1_opac, g2_opac;
        if constexpr (non_sh_quant) {
            _OptimNonShQ<1>::decode(non_sh.opacities_packed, non_sh.opacities_bounds, idx,
                                    &g1_opac, &g2_opac);
        } else {
            g1_opac = g1_opacities[idx];
            g2_opac = g2_opacities[idx];
        }
        g1_opac = beta1 * g1_opac + (1.f - beta1) * v_opac;
        g2_opac = beta2 * g2_opac + (1.f - beta2) * v_opac*v_opac;
        float updated_opac = opac - lr_opacs * g1_opac / (sqrtf(g2_opac * inv_bias_correction2) + eps);
        opacities[idx] = updated_opac;
        if constexpr (non_sh_quant) {
            nq_g1_opac = g1_opac;
            nq_g2_opac = g2_opac;
            _OptimNonShQ<1>::accumulate(&g1_opac, &g2_opac, nq_mm_opac);
        } else {
            g1_opacities[idx] = g1_opac;
            g2_opacities[idx] = g2_opac;
        }

        // update means (scale agnostic)
        float3 mean = means[idx];
        float3 gd_mean;
        if (gq.means_packed) { float _v[3]; gradq::Codec<16>::decode(gq.means_packed, (int64_t)3*idx, 3, gq.means_bounds[blockIdx.x], _v); gd_mean = make_float3(_v[0], _v[1], _v[2]); }
        else                 { gd_mean = v_means[idx]; }
        float3 v_mean = grad_scale * gd_mean;
        if constexpr (zero_grad)
            if (!gq.means_packed) v_means[idx] = make_float3(0.0f);
        // densification score: ||dL/dmean_world|| * max post-exp world scale.
        // v_mean is the (1/B-scaled) batch-accumulated data gradient; `scale`
        // holds the pre-update log scales loaded above. The grad_scale factor
        // is a global constant and doesn't affect relative ranking.
        if (densify_score != nullptr) {
            float s_max = fmaxf(fmaxf(scale.x, scale.y), scale.z);
            densify_score[idx] = expf(s_max) * sqrtf(dot(v_mean, v_mean));
        }
        float3 g1_mean, g2_mean;
        if constexpr (non_sh_quant) {
            _OptimNonShQ<3>::decode(non_sh.means_packed, non_sh.means_bounds, idx,
                                    (float*)&g1_mean, (float*)&g2_mean);
        } else {
            g1_mean = g1_means[idx];
            g2_mean = g2_means[idx];
        }

        float opac_post_sigmoid = sigmoid(opac);

        if constexpr (use_scale_agnostic_mean) {
            float3 v_mean_scaled_num = SlangProjectionUtils::apply_covar_to_vec(
                quat,
                {expf(0.5f*scale.x), expf(0.5f*scale.y), expf(0.5f*scale.z)},
                v_mean
            ) * sqrtf(2.0f * __logf(fmaxf(255.0f * opac_post_sigmoid, 1.00001f)));
            float v_mean_scaled_den = radii[idx] * 0.6f;
            g1_mean = beta1 * g1_mean + (1.f - beta1) * v_mean_scaled_num;
            g2_mean = beta2 * g2_mean + (1.f - beta2) * v_mean*v_mean * v_mean_scaled_den*v_mean_scaled_den;
        } else {
            g1_mean = beta1 * g1_mean + (1.f - beta1) * v_mean;
            g2_mean = beta2 * g2_mean + (1.f - beta2) * v_mean*v_mean;
        }

        means[idx] = mean - lr_means * g1_mean / (sqrtf(g2_mean * inv_bias_correction2) + eps);
        if constexpr (non_sh_quant) {
            nq_g1_mean = g1_mean;
            nq_g2_mean = g2_mean;
            _OptimNonShQ<3>::accumulate((float*)&g1_mean, (float*)&g2_mean, nq_mm_mean);
        } else {
            g1_means[idx] = g1_mean;
            g2_means[idx] = g2_mean;
        }

        // update features_dc (only when non_sh_quant is on; the fp32 path
        // keeps the separate fused_adam_step launch for features_dc).
        if constexpr (non_sh_quant) {
            float3 fdc  = features_dc[idx];
            float3 gd_dc;
            if (gq.dc_packed) { float _v[3]; gradq::Codec<16>::decode(gq.dc_packed, (int64_t)3*idx, 3, gq.dc_bounds[blockIdx.x], _v); gd_dc = make_float3(_v[0], _v[1], _v[2]); }
            else              { gd_dc = v_features_dc[idx]; }
            float3 v_dc = grad_scale * gd_dc;
            if constexpr (zero_grad)
                if (!gq.dc_packed) v_features_dc[idx] = make_float3(0.0f);
            // Mirrors the fused_adam_step(features_dc) launch's hinge
            // (l2_reg + l2_reg_offset = 0.5/kSh0) so non_sh_quant does not
            // silently drop the DC color reg.
            const float dc_off = 0.5f / kSh0;
            const float under_reg_weight = dc_reg_weight + sh_reg_weight;
            v_dc.x += dc_reg_weight * fmaxf(fdc.x - dc_off, 0.f) + under_reg_weight * fminf(fdc.x + dc_off, 0.f);
            v_dc.y += dc_reg_weight * fmaxf(fdc.y - dc_off, 0.f) + under_reg_weight * fminf(fdc.y + dc_off, 0.f);
            v_dc.z += dc_reg_weight * fmaxf(fdc.z - dc_off, 0.f) + under_reg_weight * fminf(fdc.z + dc_off, 0.f);

            float3 g1_dc, g2_dc;
            _OptimNonShQ<3>::decode(non_sh.features_dc_packed, non_sh.features_dc_bounds, idx,
                                    (float*)&g1_dc, (float*)&g2_dc);
            g1_dc = beta1 * g1_dc + (1.f - beta1) * v_dc;
            g2_dc = beta2 * g2_dc + (1.f - beta2) * v_dc*v_dc;
            features_dc[idx] = fdc - lr_features_dc * g1_dc / (sqrtf(g2_dc * inv_bias_correction2) + eps);
            nq_g1_dc = g1_dc;
            nq_g2_dc = g2_dc;
            _OptimNonShQ<3>::accumulate((float*)&g1_dc, (float*)&g2_dc, nq_mm_dc);
        }
    }  // inside

    // Block-reduce per-attribute bounds (in u, sqrt_g2 basis) + encode.
    // All threads must participate in the reduces; outside-threads pass
    // sentinel min/max that don't perturb the result.
    if constexpr (non_sh_quant) {
        nq_mm_scale = _optim_block_reduce_minmax_f4<BLOCK_SIZE>(nq_mm_scale);
        if (threadIdx.x == 0) non_sh.scales_bounds[blockIdx.x] = nq_mm_scale;
        nq_mm_quat = _optim_block_reduce_minmax_f4<BLOCK_SIZE>(nq_mm_quat);
        if (threadIdx.x == 0) non_sh.quats_bounds[blockIdx.x] = nq_mm_quat;
        nq_mm_opac = _optim_block_reduce_minmax_f4<BLOCK_SIZE>(nq_mm_opac);
        if (threadIdx.x == 0) non_sh.opacities_bounds[blockIdx.x] = nq_mm_opac;
        nq_mm_mean = _optim_block_reduce_minmax_f4<BLOCK_SIZE>(nq_mm_mean);
        if (threadIdx.x == 0) non_sh.means_bounds[blockIdx.x] = nq_mm_mean;
        nq_mm_dc = _optim_block_reduce_minmax_f4<BLOCK_SIZE>(nq_mm_dc);
        if (threadIdx.x == 0) non_sh.features_dc_bounds[blockIdx.x] = nq_mm_dc;
        if (inside) {
            _OptimNonShQ<3>::encode(non_sh.scales_packed,      idx, (float*)&nq_g1_scale, (float*)&nq_g2_scale, nq_mm_scale);
            _OptimNonShQ<4>::encode(non_sh.quats_packed,       idx, (float*)&nq_g1_quat,  (float*)&nq_g2_quat,  nq_mm_quat);
            _OptimNonShQ<1>::encode(non_sh.opacities_packed,   idx, &nq_g1_opac,          &nq_g2_opac,          nq_mm_opac);
            _OptimNonShQ<3>::encode(non_sh.means_packed,       idx, (float*)&nq_g1_mean,  (float*)&nq_g2_mean,  nq_mm_mean);
            _OptimNonShQ<3>::encode(non_sh.features_dc_packed, idx, (float*)&nq_g1_dc,    (float*)&nq_g2_dc,    nq_mm_dc);
        }
    }
}


/*[AutoHeaderGeneratorExport]*/
void fused_optim_3dgs_geometry(
    int64_t num_splats,
    DeviceVector<float3> means, DeviceVector<float3> v_means, DeviceVector<float3> g1_means, DeviceVector<float3> g2_means,
    DeviceVector<float4> quats, DeviceVector<float4> v_quats, DeviceVector<float4> g1_quats, DeviceVector<float4> g2_quats,
    DeviceVector<float3> scales, DeviceVector<float3> v_scales, DeviceVector<float3> g1_scales, DeviceVector<float3> g2_scales,
    DeviceVector<float> opacities, DeviceVector<float> v_opacities, DeviceVector<float> g1_opacities, DeviceVector<float> g2_opacities,
    // features_dc + grad: consumed only when non_sh has its quant buffers
    // populated (via non_sh.enabled flagged in `non_sh`). The fp32 path
    // ignores them and runs the separate fused_adam_step features_dc call.
    DeviceVector<float3> features_dc, DeviceVector<float3> v_features_dc,
    DeviceVector<float> radii,
    // optional [N] densification world-grad score output; empty disables.
    DeviceVector<float> densify_score,
    const float lr_means, const float lr_quats, const float lr_scales, const float lr_opacs,
    const float lr_features_dc,
    const float max_gauss_ratio, const float scale_regularization_weight,
    const float mcmc_opacity_reg_weight, const float mcmc_scale_reg_weight,
    const float erank_reg_weight, const float erank_reg_weight_s3, const float quat_norm_reg_weight,
    const float dc_reg_weight, const float sh_reg_weight,
    const float max_screen_size, const float max_screen_size_penalty,
    bool use_scale_agnostic_mean,
    NonShQuantState non_sh,
    GradQuantBuffers gq,
    int32_t step, DeviceVector<int32_t> per_splat_steps,
    float grad_scale, bool zero_grad
) {
    if (num_splats == 0)
        return;

    // Dispatch over (use_scale_agnostic_mean, zero_grad, non_sh_quant) -> 8 instantiations.
    using KFn = void(*)(
        float3*, float3*, float3*, float3*,
        float4*, float4*, float4*, float4*,
        float3*, float3*, float3*, float3*,
        float*,  float*,  float*,  float*,
        float3*, float3*,
        const float*,
        float*,
        float, float, float, float, float,
        const float, const float, const float, const float,
        const float, const float, const float, const float,
        const float, const float, const float, const float,
        const NonShQuantState,
        const GradQuantBuffers,
        const int32_t, const int32_t*, const int64_t);
    KFn kfn = nullptr;
    const bool nq = non_sh.enabled;
    if (use_scale_agnostic_mean) {
        if (zero_grad) {
            kfn = nq ? fused_optim_3dgs_geometry_kernel<true,  true,  true>
                     : fused_optim_3dgs_geometry_kernel<true,  true,  false>;
        } else {
            kfn = nq ? fused_optim_3dgs_geometry_kernel<true,  false, true>
                     : fused_optim_3dgs_geometry_kernel<true,  false, false>;
        }
    } else {
        if (zero_grad) {
            kfn = nq ? fused_optim_3dgs_geometry_kernel<false, true,  true>
                     : fused_optim_3dgs_geometry_kernel<false, true,  false>;
        } else {
            kfn = nq ? fused_optim_3dgs_geometry_kernel<false, false, true>
                     : fused_optim_3dgs_geometry_kernel<false, false, false>;
        }
    }
    kfn<<<_LAUNCH_ARGS_1D(num_splats, 256)>>>(
        means.data_ptr(), v_means.data_ptr(), g1_means.data_ptr(), g2_means.data_ptr(),
        quats.data_ptr(), v_quats.data_ptr(), g1_quats.data_ptr(), g2_quats.data_ptr(),
        scales.data_ptr(), v_scales.data_ptr(), g1_scales.data_ptr(), g2_scales.data_ptr(),
        opacities.data_ptr(), v_opacities.data_ptr(), g1_opacities.data_ptr(), g2_opacities.data_ptr(),
        features_dc.data_ptr(), v_features_dc.data_ptr(),
        (const float*)radii.data_ptr(),
        densify_score.data_ptr(),
        lr_means, lr_quats, lr_scales, lr_opacs, lr_features_dc,
        max_gauss_ratio,
        scale_regularization_weight / (float)num_splats,
        mcmc_opacity_reg_weight / (float)num_splats,
        mcmc_scale_reg_weight / (float)num_splats,
        erank_reg_weight / (float)num_splats,
        erank_reg_weight_s3 / (float)num_splats,
        quat_norm_reg_weight / (float)num_splats,
        dc_reg_weight, sh_reg_weight,
        max_screen_size, max_screen_size_penalty,
        grad_scale,
        non_sh,
        gq,
        step, per_splat_steps.data_ptr(), num_splats
    );
    CHECK_DEVICE_ERROR(cudaGetLastError());
}



