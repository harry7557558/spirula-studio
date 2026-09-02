// Engine loss + combined backward pass.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineInternal.h"
#include "engine/EngineState.h"

#include "kernels/bilagrid/BilagridBindings.h"
#include "kernels/projection/ProjectionBwd.cuh"
#include "kernels/projection/ProjectionBwdQuantGrad.cuh"
#include "kernels/raster/RasterizationBwd.cuh"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <vector>


static void _alloc_grad_buffers() {
    int64_t N = engine().max_num_splats;
    int64_t K = engine().num_sh;

    // Fused projection-bwd+optim folds the world-grad accumulation directly
    // into the optimizer step's local registers. For 3dgs / mip, raster_*_bwd
    // writes only screen-space grads, so all world-grad buffers can be
    // skipped (the per-channel atomicStore guard on a null WorldBuffer turns
    // those writes into no-ops). For 3dgut, raster_*_bwd atomicAdds
    // mean / quat / scale into the WORLD buffer (see Primitive3DGUT.cuh
    // FragmentBwd::atomicStore), so those three slots must remain allocated;
    // opacity/dc/sh still go via the screen buffer or the fused optim path.
    if (engine().optim.use_fused_proj_bwd_optim) {
        bool need_world_geom = (engine().primitive == "3dgut");
        if (need_world_geom) {
            engine().grad.means.resize(PoolSlot::EngVMeans, N);
            engine().grad.quats.resize(PoolSlot::EngVQuats, N);
            engine().grad.scales.resize(PoolSlot::EngVScales, N);
            engine().grad.means.zero();
            engine().grad.quats.zero();
            engine().grad.scales.zero();
        } else {
            engine().grad.means  = DeviceVector<float3>();
            engine().grad.quats  = DeviceVector<float4>();
            engine().grad.scales = DeviceVector<float3>();
        }
        engine().grad.opacities    = DeviceVector<float>();
        engine().grad.features_dc  = DeviceVector<float3>();
        engine().grad.features_sh  = DeviceTensor2D<float3>();
        return;
    }

    // Block-wise quantized gradient accumulation (non-FPBO, grad-quant path).
    // Per-splat-block bounds: one float2 per 256 splats (n_bounds = ceil(N/256)).
    // 3dgs/mip quantize all six attributes; 3dgut keeps means/quats/scales fp32
    // (they receive rasterization-backward atomicAdds) and quantizes the rest.
    // Zeroing is skip_grad_zero-gated (first sub-batch only); the projection-bwd
    // kernel then decode-accumulates each sub-batch. Zero bounds -> decode 0.
    if (engine().grad.quantize_grad) {
        const bool is_gut = (engine().primitive == "3dgut");
        const int64_t n_bounds = (N + 255) / 256;
        const bool skip_zero = engine().optim.skip_grad_zero;

        if (is_gut) {
            engine().grad.means.resize(PoolSlot::EngVMeans, N);
            engine().grad.quats.resize(PoolSlot::EngVQuats, N);
            engine().grad.scales.resize(PoolSlot::EngVScales, N);
            engine().grad.means_q  = QuantizedTensor<16, 256>();
            engine().grad.quats_q  = QuantizedTensor<16, 256>();
            engine().grad.scales_q = QuantizedTensor<16, 256>();
            if (!skip_zero) {
                engine().grad.means.zero();
                engine().grad.quats.zero();
                engine().grad.scales.zero();
            }
        } else {
            engine().grad.means  = DeviceVector<float3>();
            engine().grad.quats  = DeviceVector<float4>();
            engine().grad.scales = DeviceVector<float3>();
            engine().grad.means_q .resize(PoolSlot::EngVMeansQ,  (int64_t)N * 3, n_bounds);
            engine().grad.quats_q .resize(PoolSlot::EngVQuatsQ,  (int64_t)N * 4, n_bounds);
            engine().grad.scales_q.resize(PoolSlot::EngVScalesQ, (int64_t)N * 3, n_bounds);
            if (!skip_zero) {
                engine().grad.means_q.zero();
                engine().grad.quats_q.zero();
                engine().grad.scales_q.zero();
            }
        }
        engine().grad.opacities   = DeviceVector<float>();
        engine().grad.features_dc = DeviceVector<float3>();
        engine().grad.features_sh = DeviceTensor2D<float3>();
        engine().grad.opacities_q  .resize(PoolSlot::EngVOpacitiesQ,  (int64_t)N * 1, n_bounds);
        engine().grad.features_dc_q.resize(PoolSlot::EngVFeaturesDcQ, (int64_t)N * 3, n_bounds);
        engine().grad.features_sh_q.resize(PoolSlot::EngVFeaturesShQ, (int64_t)N * K * 3, n_bounds);
        if (!skip_zero) {
            engine().grad.opacities_q.zero();
            engine().grad.features_dc_q.zero();
            engine().grad.features_sh_q.zero();
        }
        return;
    }

    // Non-quantized fp32 grad path. Release any quantized holders (mode switch).
    engine().grad.means_q       = QuantizedTensor<16, 256>();
    engine().grad.quats_q       = QuantizedTensor<16, 256>();
    engine().grad.scales_q      = QuantizedTensor<16, 256>();
    engine().grad.opacities_q   = QuantizedTensor<16, 256>();
    engine().grad.features_dc_q = QuantizedTensor<16, 256>();
    engine().grad.features_sh_q = QuantizedTensor<8,  256>();

    engine().grad.means.resize(PoolSlot::EngVMeans, N);
    engine().grad.quats.resize(PoolSlot::EngVQuats, N);
    engine().grad.scales.resize(PoolSlot::EngVScales, N);
    engine().grad.opacities.resize(PoolSlot::EngVOpacities, N);
    engine().grad.features_dc.resize(PoolSlot::EngVFeaturesDc, N);
    engine().grad.features_sh.resize(PoolSlot::EngVFeaturesSh, N, K);
    // Sub-batched training: only the FIRST sub-batch zeroes the per-splat
    // grad accumulators; subsequent sub-batches atomicAdd into them. The
    // optim step then consumes the accumulated grad (with grad_scale = 1/B)
    // and zeroes the buffer as a fused side effect.
    if (!engine().optim.skip_grad_zero) {
        engine().grad.means.zero();
        engine().grad.quats.zero();
        engine().grad.scales.zero();
        engine().grad.opacities.zero();
        engine().grad.features_dc.zero();
        engine().grad.features_sh.zero();
    }
}


// Shared rasterization + projection backward tail. Consumes the per-pixel
// output cotangents (v_render_rgb [C,H,W,3], v_render_depth / v_render_Ts
// [C,H,W,1]; device pool buffers) and writes per-splat gradients into
// engine().grad.* (plus the fused-path screen-space stash where applicable).
// Callers must have run _alloc_grad_buffers() first. Used by the loss-driven
// backward and by the direct cotangent-injection entrypoint
// (engine_backward_from_render_grad).
static void _engine_raster_proj_backward(
    TorchTensorView v_render_rgb,
    TorchTensorView v_render_depth,
    TorchTensorView v_render_Ts_tv,
    const DeviceTensor3D<float>& accum_weight_map,
    DensifyAccumMode accum_mode,
    const DeviceTensor3D<float>& v_median = DeviceTensor3D<float>(),
    TorchTensorView v_rgb_dist = _tv_null(),
    TorchTensorView v_depth_dist = _tv_null(),
    TorchTensorView v_normal_dist = _tv_null()
) {
    RenderOutput::TensorTuple v_render_outputs = std::make_tuple(
        DeviceTensor3D<float3>(v_render_rgb),
        DeviceTensor3D<float>(v_render_depth),
        DeviceTensor3D<float3>()  // no normal gradient yet
    );
    DeviceTensor3D<float> v_render_Ts(v_render_Ts_tv);

    // Distortion: route the forward distortion image D (for the backward's
    // S = (D + C^2)/W reconstruction) and its per-channel loss gradient to the
    // raster backward, templated on engine().fwd.dist_type. Per-channel grad
    // tensors are present only for the channels the forward emitted.
    const DistortionType dist_type = engine().fwd.dist_type;
    std::optional<RenderOutput::TensorTuple> distortion_fwd_opt = std::nullopt;
    std::optional<RenderOutput::TensorTuple> v_distortion_opt = std::nullopt;
    if (dist_any(dist_type)) {
        distortion_fwd_opt = engine().fwd.distortions;
        // Build per-channel grad views ONLY for the channels dist_type carries.
        // Absent channels arrive as null views (_tv_null, shape {0}); feeding
        // those to DeviceTensor3D throws ("Expected 4D tensor view"), so leave
        // them default-constructed (null) — e.g. dist_type=D has no rgb/normal.
        v_distortion_opt = std::make_tuple(
            dist_has_rgb(dist_type)    ? DeviceTensor3D<float3>(v_rgb_dist)    : DeviceTensor3D<float3>(),
            dist_has_depth(dist_type)  ? DeviceTensor3D<float>(v_depth_dist)   : DeviceTensor3D<float>(),
            dist_has_normal(dist_type) ? DeviceTensor3D<float3>(v_normal_dist) : DeviceTensor3D<float3>()
        );
    }

    // Build the world-grad vector handed to rasterize_*_bwd. Three cases:
    //   - non-fused: all six slots are real per-channel grad buffers.
    //   - fused + 3dgs / mip: empty vector; raster_*_bwd writes nothing into
    //     the world buffer (its atomicStore is screen-only for those prims).
    //   - fused + 3dgut: mean/quat/scale slots are real (raster_*_bwd atomic-
    //     adds them); opacity/dc/sh slots are null (those flow via screen or
    //     are accumulated inside the FPBO kernel).
    //   - grad-quant (non-fused, quantized grad): same raster handoff as fused
    //     -- 3dgs/mip empty (screen-only), 3dgut mean/quat/scale into the fp32
    //     world buffer; the projection contribution is folded in by the
    //     requantize-accumulate projection-bwd kernel below.
    const bool grad_q = engine().grad.quantize_grad;
    std::vector<DeviceTensorFloatND> v_splats_w;
    if (!engine().optim.use_fused_proj_bwd_optim && !grad_q) {
        DeviceTensorFloatND v_fnd_opac;
        {
            TorchTensorView opac_tv((uint64_t)engine().grad.opacities.data_ptr(), 4,
                {engine().grad.opacities.size(), 1LL, 1LL});
            v_fnd_opac = DeviceTensorFloatND(opac_tv);  // ndim=2, shape=[N, 1]
        }
        v_splats_w = {
            DeviceTensorFloatND(engine().grad.means),         // [N, 3]
            DeviceTensorFloatND(engine().grad.quats),         // [N, 4]
            DeviceTensorFloatND(engine().grad.scales),        // [N, 3]
            v_fnd_opac,                                       // [N, 1]
            DeviceTensorFloatND(engine().grad.features_dc),   // [N, 3]
            DeviceTensorFloatND(engine().grad.features_sh),   // [N, K, 3]
        };
    } else if (engine().primitive == "3dgut") {
        v_splats_w = {
            DeviceTensorFloatND(engine().grad.means),         // [N, 3]
            DeviceTensorFloatND(engine().grad.quats),         // [N, 4]
            DeviceTensorFloatND(engine().grad.scales),        // [N, 3]
            DeviceTensorFloatND(),                            // opacities -- null
            DeviceTensorFloatND(),                            // features_dc -- null
            DeviceTensorFloatND(),                            // features_sh -- null
        };
    }

    std::vector<DeviceTensorFloatND> v_splats_w_out, v_splats_s_out;

    if (engine().primitive == "3dgs" || engine().primitive == "mip") {
        auto [vw, vs, accum_weight] = rasterize_to_pixels_3dgs_bwd(
            engine().cur_num_splats,
            engine().fwd.splats_w,
            engine().fwd.splats_s,
            engine().fwd.gaussian_ids,
            (uint32_t)engine().camera.width,
            (uint32_t)engine().camera.height,
            engine().fwd.tile_offsets,
            engine().fwd.flatten_ids,
            engine().fwd.macro_log2,
            engine().fwd.render_Ts,
            engine().fwd.last_ids,
            engine().fwd.renders,
            distortion_fwd_opt,  // forward distortion D (for S reconstruction)
            dist_type,
            accum_weight_map,
            accum_mode,
            v_render_outputs,
            v_render_Ts,
            v_median,
            v_distortion_opt,  // gradient w.r.t. distortion image
            std::make_optional(v_splats_w),
            std::nullopt
        );
        v_splats_w_out = vw;
        v_splats_s_out = vs;
        if (accum_weight.data_ptr()) {
            engine().fwd.accum_weight = accum_weight;
            engine().fwd.accum_mode = accum_mode;
        }
    } else if (engine().primitive == "3dgut") {
        auto [vw, vs, vviewmats, accum_weight] = rasterize_to_pixels_3dgut_bwd(
            engine().cur_num_splats,
            engine().fwd.splats_w,
            engine().fwd.splats_s,
            engine().fwd.gaussian_ids,
            _dt2d_tv(engine().camera.viewmats),
            _dv_tv(engine().camera.intrins),
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dt2d_tv(engine().camera.dist_coeffs),
            engine().fwd.aabb,
            (uint32_t)engine().camera.width,
            (uint32_t)engine().camera.height,
            engine().fwd.tile_offsets,
            engine().fwd.flatten_ids,
            engine().fwd.macro_log2,
            engine().fwd.render_Ts,
            engine().fwd.last_ids,
            engine().fwd.renders,
            distortion_fwd_opt,  // forward distortion D (for S reconstruction)
            dist_type,
            DeviceTensor3D<float>(),  // loss_map
            accum_weight_map,
            accum_mode,
            v_render_outputs,
            v_render_Ts,
            v_median,
            v_distortion_opt,  // gradient w.r.t. distortion image
            std::make_optional(v_splats_w),
            std::nullopt,
            false
        );
        v_splats_w_out = vw;
        v_splats_s_out = vs;
        if (accum_weight.data_ptr()) {
            engine().fwd.accum_weight = accum_weight;
            engine().fwd.accum_mode = accum_mode;
        }
    } else {
        throw std::runtime_error("engine raster/proj backward: unknown primitive: " + engine().primitive);
    }

    // --- Projection backward ---
    // In fused-proj-bwd-optim mode the projection backward is folded into the
    // optimizer step; we just stash the screen-space gradients for that call.
    if (engine().optim.use_fused_proj_bwd_optim) {
        engine().fwd.v_splats_s = v_splats_s_out;
    } else {
        // SH VALUE-quant: when active in non-FPBO mode, project_vjp reads
        // the source SH via the codec instead of fp32 features_sh (which is
        // unallocated). Pick the (packed, bounds) buffer + bounds-cell
        // stride based on which storage layout was set up at allocation
        // time (mirrors EngineForward.cpp).
        std::optional<TorchTensorView> vp_opt = std::nullopt;
        std::optional<TorchTensorView> vb_opt = std::nullopt;
        int      sh_value_bits   = engine().world.sh_value_bits;
        uint32_t num_sh_buffer   = (uint32_t)engine().num_sh;
        int64_t  sh_bounds_stride = 0;
        if (sh_value_bits == 8) {
            auto pick = [&](auto& vq) {
                if (!vq.initialized()) return;
                vp_opt = TorchTensorView((uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
                vb_opt = TorchTensorView((uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
            };
            if (engine().world.features_sh_quant8_fpbo.initialized()) {
                pick(engine().world.features_sh_quant8_fpbo);
                sh_bounds_stride = 0;  // FPBO layout
            } else {
                pick(engine().world.features_sh_quant8);
                sh_bounds_stride = 256;
            }
        } else if (sh_value_bits == 16) {
            auto pick = [&](auto& vq) {
                if (!vq.initialized()) return;
                vp_opt = TorchTensorView((uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
                vb_opt = TorchTensorView((uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
            };
            if (engine().world.features_sh_quant16_fpbo.initialized()) {
                pick(engine().world.features_sh_quant16_fpbo);
                sh_bounds_stride = 0;  // FPBO layout
            } else {
                pick(engine().world.features_sh_quant16);
                sh_bounds_stride = 256;
            }
        }
        // Grad-quant path: non-atomic, block-wise quantized requantize-accumulate
        // projection backward. Reads/writes engine().grad.*_q (per-splat-block);
        // for 3dgut, means/quats/scales are the fp32 v_splats_w_out (raster's
        // world contribution) that the kernel adds the projection grad onto.
        if (grad_q) {
            GradQuantBuffers gq;
            auto& g = engine().grad;
            if (g.means_q.initialized())       { gq.means_packed  = g.means_q.packed_ptr();       gq.means_bounds  = g.means_q.bounds_ptr(); }
            if (g.quats_q.initialized())       { gq.quats_packed  = g.quats_q.packed_ptr();       gq.quats_bounds  = g.quats_q.bounds_ptr(); }
            if (g.scales_q.initialized())      { gq.scales_packed = g.scales_q.packed_ptr();      gq.scales_bounds = g.scales_q.bounds_ptr(); }
            if (g.opacities_q.initialized())   { gq.opac_packed   = g.opacities_q.packed_ptr();   gq.opac_bounds   = g.opacities_q.bounds_ptr(); }
            if (g.features_dc_q.initialized()) { gq.dc_packed     = g.features_dc_q.packed_ptr(); gq.dc_bounds     = g.features_dc_q.bounds_ptr(); }
            if (g.features_sh_q.initialized()) { gq.sh_packed     = g.features_sh_q.packed_ptr(); gq.sh_bounds     = g.features_sh_q.bounds_ptr(); }
            auto call_qg = [&](auto fn) {
                fn(engine().cur_num_splats, engine().sh_degree, engine().fwd.splats_w,
                   _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                   (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                   engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                   engine().fwd.camera_ids, engine().fwd.gaussian_ids, engine().fwd.aabb,
                   v_splats_s_out, v_splats_w_out, gq,
                   vp_opt, vb_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
            };
            if (engine().primitive == "3dgs")       call_qg(projection_3dgs_backward_quantgrad);
            else if (engine().primitive == "mip")   call_qg(projection_mip_backward_quantgrad);
            else if (engine().primitive == "3dgut") call_qg(projection_3dgut_backward_quantgrad);
            else throw std::runtime_error("grad-quant projection backward: unknown primitive: " + engine().primitive);
        } else if (engine().primitive == "3dgs") {
            projection_3dgs_backward(
                engine().cur_num_splats, engine().sh_degree, engine().fwd.splats_w,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().fwd.camera_ids, engine().fwd.gaussian_ids,
                engine().fwd.aabb, v_splats_s_out, v_splats_w_out, nullptr,
                vp_opt, vb_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
        } else if (engine().primitive == "mip") {
            projection_mip_backward(
                engine().cur_num_splats, engine().sh_degree, engine().fwd.splats_w,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().fwd.camera_ids, engine().fwd.gaussian_ids,
                engine().fwd.aabb, v_splats_s_out, v_splats_w_out, nullptr,
                vp_opt, vb_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
        } else if (engine().primitive == "3dgut") {
            projection_3dgut_backward(
                engine().cur_num_splats, engine().sh_degree, engine().fwd.splats_w,
                _dt2d_tv(engine().camera.viewmats), _dv_tv(engine().camera.intrins),
                (uint32_t)engine().camera.width, (uint32_t)engine().camera.height,
                engine().camera.model_str,
        engine().camera.distortion_str, _dt2d_tv(engine().camera.dist_coeffs),
                engine().fwd.camera_ids, engine().fwd.gaussian_ids,
                engine().fwd.aabb, v_splats_s_out, v_splats_w_out, nullptr,
                vp_opt, vb_opt, num_sh_buffer, sh_value_bits, sh_bounds_stride);
        }
    }
}


// Backward from caller-supplied output cotangents (the vjp seed), bypassing the
// loss computation entirely. Mirrors the old per-kernel
// renderer.backward(v_render_colors, v_render_Ts): copies host (or device)
// v_render_* into pool buffers, then runs the shared raster + projection
// backward. Per-splat gradients land in engine().grad.* (read back with
// engine_copy_grads_to_host). forward_3dgs must have run first.
void engine_backward_from_render_grad(
    TorchTensorView v_render_rgb,    // [C, H, W, 3] float32
    TorchTensorView v_render_depth,  // [C, H, W, 1] float32
    TorchTensorView v_render_Ts      // [C, H, W, 1] float32
) {
    if (std::get<0>(engine().fwd.renders).data_ptr() == nullptr)
        throw std::runtime_error("engine_backward_from_render_grad: forward_3dgs must be called first");

    // Cotangent-injection / grad-readback path: keep grad fp32 so host readback
    // (engine_copy_grads_to_host) sees dense buffers, not the packed codec.
    engine().grad.quantize_grad = false;
    _alloc_grad_buffers();

    int64_t C = engine().camera.num;
    int64_t H = engine().camera.height;
    int64_t W = engine().camera.width;

    // Stage the cotangents into pool buffers. backend::MemcpyKind::Auto auto-detects
    // host vs device pointers (UVA), so the same entrypoint serves the
    // correctness path (CPU seeds) and the profiling path (CUDA seeds).
    TorchTensorView v_rgb_buf   = _pool_tv(PoolSlot::EngVRgb,   C, H, W, 3);
    TorchTensorView v_depth_buf = _pool_tv(PoolSlot::EngVDepth, C, H, W, 1);
    TorchTensorView v_Ts_buf    = _pool_tv(PoolSlot::EngVTs,    C, H, W, 1);
    backend::memcpy_sync((void*)std::get<0>(v_rgb_buf),   (void*)std::get<0>(v_render_rgb),
               (size_t)C * H * W * 3 * sizeof(float), backend::MemcpyKind::Auto);
    backend::memcpy_sync((void*)std::get<0>(v_depth_buf), (void*)std::get<0>(v_render_depth),
               (size_t)C * H * W * 1 * sizeof(float), backend::MemcpyKind::Auto);
    backend::memcpy_sync((void*)std::get<0>(v_Ts_buf),    (void*)std::get<0>(v_render_Ts),
               (size_t)C * H * W * 1 * sizeof(float), backend::MemcpyKind::Auto);

    DeviceTensor3D<float> accum_weight_map;  // empty: no loss-map weighting
    _engine_raster_proj_backward(v_rgb_buf, v_depth_buf, v_Ts_buf, accum_weight_map,
                                 DensifyAccumMode::None);
}


// Shared by engine_compute_loss_backward and engine_preview_loss_map: a valid
// `map_out` stops after the per-pixel loss and copies the loss map there,
// before anything that mutates a gradient or an optimizer moment.
static std::map<std::string, float> _engine_loss(
    std::array<float, (int)LossWeightIndex::length> loss_weights,
    float w_ssim,
    int num_loss_scales,
    int loss_scale_min_pixels,
    bool compute_loss_map,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    float nms_falloff,
    bool loss_map_normalize,
    float loss_map_clip_quantile,
    float loss_map_power,
    int loss_map_accum_mode,
    float overexposure_reg_weight,
    float color_shift_reg_weight,
    float color_shift_reg_beta,
    TorchTensorView map_out
) {
    const bool map_only = _tv_valid(map_out);
    // Validate that forward was run
    if (std::get<0>(engine().fwd.renders) .data_ptr() == nullptr)
        throw std::runtime_error("engine_compute_loss_backward: forward_3dgs must be called first");
    if (!engine().gt.has_gt)
        throw std::runtime_error("engine_compute_loss_backward: set_training_data must be called first");

    // Allocate and zero gradient buffers from pool
    if (!map_only) _alloc_grad_buffers();

    int64_t C = engine().camera.num;
    int64_t H = engine().camera.height;
    int64_t W = engine().camera.width;

    // Resolution-adaptive multi-scale loss: when loss_scale_min_pixels > 0 it
    // overrides num_loss_scales based on this step's render resolution, so that
    // the smallest image dimension is halved down toward (but not below) the
    // requested pixel count. e.g. loss_scale_min_pixels=2000 -> min dim 1999
    // gives 1 scale, 2000 gives 2, 4000 gives 3, 8000 gives 4. Adapts per step,
    // so mixed-resolution datasets pick the right count per image automatically.
    if (loss_scale_min_pixels > 0) {
        int64_t min_dim = std::min(H, W);
        int auto_scales = 1;
        if (min_dim >= (int64_t)loss_scale_min_pixels)
            auto_scales = (int)std::floor(
                std::log2((double)min_dim / (double)loss_scale_min_pixels)) + 2;
        // Clamp to the kernel's MAX_SCALES (see PerPixelLoss.cu) so extreme
        // resolutions saturate the scale count rather than throwing.
        num_loss_scales = std::min(auto_scales, 4);
    }

    // Pool-allocate intermediates for loss computation
    TorchTensorView loss_map_buf = compute_loss_map ?
        _pool_tv_zero(PoolSlot::EngLossMap, C, H, W, 1) : _tv_null();

    // v_losses: constant vector [1, 0, 1, 1, ...] (1 for all, 0 for psnr slot).
    // Uploaded once per engine lifetime; the flag lives in EngineState so
    // engine_reset() clears it along with the buffer it describes.
    TorchTensorView v_losses_buf = _pool_tv_1d(PoolSlot::EngVLosses, (int)LossIndex::length);
    if (!engine().v_losses_uploaded) {
        float h_v[(int)LossIndex::length];
        for (int i = 0; i < (int)LossIndex::length; i++) h_v[i] = 1.0f;
        h_v[(int)LossIndex::RgbPSNR] = 0.0f;
        backend::memcpy_sync((void*)std::get<0>(v_losses_buf), h_v, sizeof(h_v), backend::MemcpyKind::HostToDevice);
        engine().v_losses_uploaded = true;
    }

    // Render outputs from forward pass (pool-backed, already populated)
    TorchTensorView render_rgb = TorchTensorView(
        (uint64_t)std::get<0>(engine().fwd.renders).data_ptr(), 4, {C, H, W, 3});
    TorchTensorView render_depth = TorchTensorView(
        (uint64_t)std::get<1>(engine().fwd.renders).data_ptr(), 4, {C, H, W, 1});
    TorchTensorView render_Ts = TorchTensorView(
        (uint64_t)engine().fwd.render_Ts.data_ptr(), 4, {C, H, W, 1});

    // Render normal: not yet available from forward (needs a normal-rendering
    // primitive); render_normal/depth_normal stay null / derived below.
    TorchTensorView render_normal = _tv_null();
    TorchTensorView depth_normal = _tv_null();

    // Distortion image D = W*S - C^2 from the forward, only the channels the
    // forward's dist_type emitted (depth always when active; rgb/normal
    // optional). Each present channel feeds the distortion regularizer in the
    // per-pixel loss, and its gradient flows to the raster backward (which
    // reconstructs S from D).
    const bool has_rgb_dist =
        std::get<0>(engine().fwd.distortions).data_ptr() != nullptr;
    const bool has_depth_dist =
        std::get<1>(engine().fwd.distortions).data_ptr() != nullptr;
    const bool has_normal_dist =
        std::get<2>(engine().fwd.distortions).data_ptr() != nullptr;
    TorchTensorView rgb_dist = _tv_null();
    TorchTensorView depth_dist = _tv_null();
    TorchTensorView normal_dist = _tv_null();
    if (has_rgb_dist)
        rgb_dist = TorchTensorView(
            (uint64_t)std::get<0>(engine().fwd.distortions).data_ptr(), 4, {C, H, W, 3});
    if (has_depth_dist)
        depth_dist = TorchTensorView(
            (uint64_t)std::get<1>(engine().fwd.distortions).data_ptr(), 4, {C, H, W, 1});
    if (has_normal_dist)
        normal_dist = TorchTensorView(
            (uint64_t)std::get<2>(engine().fwd.distortions).data_ptr(), 4, {C, H, W, 3});

    // Depth -> normal: derive depth_normal from rendered depth when gt_normal
    // is provided. Also needed (independently of gt_normal) by the
    // median-vs-depth-normal regularizer, which compares the median normal
    // against this depth normal.
    bool compute_depth_normal = (engine().gt.normal.data_ptr() != nullptr) ||
        loss_weights[(int)LossWeightIndex::MedianDepthNormalReg] > 0.0f;
    // bool is_ray_depth = (engine().primitive != "3dgs" && engine().primitive != "mip");
    bool is_ray_depth = true;
    if (compute_depth_normal) {
        depth_normal = _pool_tv(PoolSlot::EngDepthNormal, C, H, W, 3);
        depth_to_normal_forward(
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dv_tv(engine().camera.intrins),
            _dt2d_tv(engine().camera.dist_coeffs),
            is_ray_depth,
            DeviceTensor3D<float>(render_depth),
            DeviceTensor3D<float3>(depth_normal)
        );
    }

    // Median depth (+ its normal) for the median-depth loss terms. Present
    // only when forward emitted it (output_median). The median normal is built
    // from the median depth the same way depth_normal is, and only when one of
    // the median-normal losses is active.
    TorchTensorView median_depth = _tv_null();
    TorchTensorView median_normal = _tv_null();
    bool has_median = (engine().fwd.render_median.data_ptr() != nullptr);
    bool median_normal_active =
        loss_weights[(int)LossWeightIndex::MedianDepthNormalReg] > 0.0f ||
        loss_weights[(int)LossWeightIndex::MedianNormalSup] > 0.0f ||
        loss_weights[(int)LossWeightIndex::MedianRenderNormalReg] > 0.0f;
    if (has_median) {
        median_depth = TorchTensorView(
            (uint64_t)engine().fwd.render_median.data_ptr(), 4, {C, H, W, 1});
        if (median_normal_active) {
            median_normal = _pool_tv(PoolSlot::EngMedianNormal, C, H, W, 3);
            depth_to_normal_forward(
                engine().camera.model_str,
        engine().camera.distortion_str,
                _dv_tv(engine().camera.intrins),
                _dt2d_tv(engine().camera.dist_coeffs),
                is_ray_depth,
                DeviceTensor3D<float>(median_depth),
                DeviceTensor3D<float3>(median_normal)
            );
        }
    }

    std::vector<bool> needs_input_grad = {
        true,                                  // pred_rgb
        false,                                 // gt_rgb
        true,                                  // pred_depth
        engine().bilagrid_depth.enabled,       // gt_depth (true when bilagrid depth)
        true,                                  // pred_normal
        true,                                  // pred_depth_normal
        engine().bilagrid_normal.enabled,      // gt_normal (true when bilagrid normal)
        true,                                  // pred_transmittance
        has_rgb_dist, has_depth_dist, has_normal_dist, // distortion (rgb, depth, normal)
        has_median,                            // pred_median_depth
        has_median && median_normal_active,    // pred_median_normal
    };

    PerPixelGrads pixel_grads = {};

    // Allocate per-pixel gradient outputs
    pixel_grads.v_render_rgb  = _pool_tv(PoolSlot::EngVRgb,   C, H, W, 3);
    pixel_grads.v_render_depth = _pool_tv(PoolSlot::EngVDepth, C, H, W, 1);
    pixel_grads.v_render_Ts   = _pool_tv(PoolSlot::EngVTs,    C, H, W, 1);
    if (compute_depth_normal) {
        pixel_grads.v_depth_normal = _pool_tv(PoolSlot::EngVDepthNormal, C, H, W, 3);
    }
    if (has_median) {
        pixel_grads.v_median_depth = _pool_tv(PoolSlot::EngVMedianDepth, C, H, W, 1);
        if (median_normal_active)
            pixel_grads.v_median_normal = _pool_tv(PoolSlot::EngVMedianNormal, C, H, W, 3);
    }
    // GT-modality grads live at the GT's own resolution (which may differ
    // from render H, W). The per-pixel loss kernel bilinearly scatters into
    // these buffers via atomicAdd, and bilagrid backward then consumes them
    // at the GT's grid resolution. When the GT happens to match render shape
    // these allocations match the previous behavior exactly.
    if (engine().bilagrid_depth.enabled) {
        long Hd = engine().gt.depth.template size<1>();
        long Wd = engine().gt.depth.template size<2>();
        pixel_grads.v_ref_depth = _pool_tv(PoolSlot::EngVRefDepth, C, Hd, Wd, 1);
    }
    if (engine().bilagrid_normal.enabled) {
        long Hn = engine().gt.normal.template size<1>();
        long Wn = engine().gt.normal.template size<2>();
        pixel_grads.v_ref_normal = _pool_tv(PoolSlot::EngVRefNormal, C, Hn, Wn, 3);
    }
    // Distortion gradient buffers (d loss / d D), consumed by the raster bwd.
    // RGB_D primitives: rgb + depth only; normal distortion grad stays null.
    if (has_rgb_dist)
        pixel_grads.v_rgb_dist    = _pool_tv(PoolSlot::EngVRgbDist,    C, H, W, 3);
    if (has_depth_dist)
        pixel_grads.v_depth_dist  = _pool_tv(PoolSlot::EngVDepthDist,  C, H, W, 1);
    if (has_normal_dist)
        pixel_grads.v_normal_dist = _pool_tv(PoolSlot::EngVNormalDist, C, H, W, 3);

    // --- Compute per-pixel losses + SSIM, get gradients ---
    LossValues lv = compute_multi_scale_per_pixel_losses(
        num_loss_scales,
        render_rgb,
        _dt3d_tv(engine().gt.rgb),
        render_depth,
        _dt3d_tv(engine().gt.depth),
        render_normal,
        depth_normal,
        _dt3d_tv(engine().gt.normal),
        render_Ts,
        rgb_dist,
        depth_dist,
        normal_dist,
        median_depth,
        median_normal,
        _dt3d_tv(engine().gt.alpha),
        engine().gt.has_mask,
        loss_weights,
        w_ssim,
        v_losses_buf,
        needs_input_grad,
        -1,  // num_train_images: -1 means use batch size
        _tv_null(),  // camera_indices: null means identity
        loss_map_buf,
        loss_map_mode,
        robust_edge_aware_quantile,
        nms_falloff,
        pixel_grads
    );

    // The one place the error map is conditioned, so the preview copy below
    // and the raster backward aggregation read the same picture.
    if (compute_loss_map && _tv_valid(loss_map_buf))
        normalize_clip_map_inplace_tensor(loss_map_buf, loss_map_normalize,
                                          loss_map_clip_quantile,
                                          loss_map_power);

    if (map_only) {
        backend::memcpy_sync((void*)std::get<0>(map_out),
                             (void*)std::get<0>(loss_map_buf),
                             (size_t)(C * H * W) * sizeof(float),
                             backend::MemcpyKind::DeviceToHost);
        return {};
    }

    // --- Color-shift regularizer (combined bilagrid + PPISP) ---
    // Inject the design-(1) gradient on v_render_rgb BEFORE either bilagrid /
    // PPISP bwd runs, so each transform's vjp routes the contribution to its
    // own parameter gradient. `pre` is the input to whichever of the two
    // ran first in fwd; `post` is the final post-both-transforms image, which
    // is still pointed to by engine().fwd.renders.rgb at this point (color
    // space bwd runs strictly after the PPISP/bilagrid hooks).
    {
        const bool bg_rgb_on = engine().bilagrid_rgb.enabled;
        const bool ppisp_on  = engine().ppisp.enabled;
        if (color_shift_reg_weight > 0.0f && (bg_rgb_on || ppisp_on)) {
            // Identify the "pre" buffer = input to the FIRST forward transform.
            // Forward order (set in EngineTrainStep.cpp):
            //   run_before_bilagrid=false -> bilagrid -> PPISP : pre = bilagrid_rgb.fwd_pre
            //   run_before_bilagrid=true  -> PPISP    -> bilagrid : pre = ppisp.fwd_pre
            // When only one of the two is on, that one's fwd_pre is the splat
            // output regardless of the flag.
            const float* pre_ptr = nullptr;
            if (bg_rgb_on && ppisp_on) {
                pre_ptr = engine().ppisp.cur_run_before_bilagrid
                    ? (const float*)engine().ppisp.fwd_pre.data_ptr()
                    : (const float*)engine().bilagrid_rgb.fwd_pre.data_ptr();
            } else if (bg_rgb_on) {
                pre_ptr = (const float*)engine().bilagrid_rgb.fwd_pre.data_ptr();
            } else {
                pre_ptr = (const float*)engine().ppisp.fwd_pre.data_ptr();
            }
            const float* post_ptr =
                (const float*)std::get<0>(engine().fwd.renders).data_ptr();
            float* v_rgb_ptr = (float*)std::get<0>(pixel_grads.v_render_rgb);
            if (pre_ptr != nullptr && post_ptr != nullptr && v_rgb_ptr != nullptr) {
                auto& cs = engine().color_shift_reg;
                if (!cs.initialized) {
                    cs.ema.resize(PoolSlot::EngColorShiftRegEma, 3);
                    cs.ema.zero();
                    cs.batch_sum.resize(PoolSlot::EngColorShiftRegBatchSum, 3);
                    cs.batch_sum.zero();
                    cs.steps = 0;
                    cs.initialized = true;
                }
                cs.cur_weight = color_shift_reg_weight;
                cs.cur_beta   = color_shift_reg_beta;
                int N_pixels = (int)(C * H * W);
                color_shift_reg_step(
                    v_rgb_ptr, post_ptr, pre_ptr,
                    cs.ema.data_ptr(),
                    cs.batch_sum.data_ptr(),
                    N_pixels,
                    cs.cur_weight,
                    cs.cur_beta,
                    cs.steps,
                    /*stream=*/(backend::Stream)0);
                cs.steps += 1;
            }
        }
    }

    // --- PPISP / Bilagrid backward hooks ---
    // Backward order is the inverse of forward (set in EngineTrainStep.cpp
    // and stashed on engine().ppisp.cur_run_before_bilagrid):
    //   forward bilagrid->PPISP  =>  backward PPISP first, then bilagrid.
    //   forward PPISP->bilagrid  =>  backward bilagrid first, then PPISP.
    // Each hook rewrites v_render_rgb (post-<self> -> pre-<self>) and
    // accumulates parameter grads into its own buffer; the next hook then
    // consumes the rewritten v_render_rgb. Depth/normal grids are GT-side
    // and live entirely inside the bilagrid hook regardless of order.
    auto _ppisp_bwd = [&]() {
        if (engine().ppisp.enabled) {
            _ensure_ppisp_optim_state();
            _engine_ppisp_backward_hook(pixel_grads.v_render_rgb);
        }
    };
    auto _bilagrid_bwd = [&]() {
        if (engine().bilagrid_rgb.enabled || engine().bilagrid_depth.enabled ||
            engine().bilagrid_normal.enabled) {
            _ensure_bilagrid_optim_state();
            _engine_bilagrid_backward_hook(
                pixel_grads.v_render_rgb,
                pixel_grads.v_ref_depth,
                pixel_grads.v_ref_normal);
        }
    };
    if (engine().ppisp.cur_run_before_bilagrid) {
        _bilagrid_bwd();
        _ppisp_bwd();
    } else {
        _ppisp_bwd();
        _bilagrid_bwd();
    }

    // --- Color space backward hook ---
    // Forward is render -> bg -> display encode -> {bilagrid, PPISP} -> loss,
    // so this runs after BOTH of those backwards, in either of their orders.
    if (engine().color_space.splat_enabled) {
        _engine_color_space_backward_hook(pixel_grads.v_render_rgb);
    }

    // --- Image-space overexposure regularization ---
    // Skipped under a blend: the blend backward applies it instead, on the
    // composite rather than on this pre-blend buffer.
    if (overexposure_reg_weight != 0.0f && !engine().background.enabled) {
        // Both buffers are in the splat working color space here: cs.fwd_pre
        // is the pre-conversion render when color space is on (its bwd hook
        // does not re-point fwd.renders.rgb), render_rgb itself when off.
        DeviceTensor3D<float3> rgb_t = engine().color_space.splat_enabled
            ? engine().color_space.fwd_pre
            : DeviceTensor3D<float3>(render_rgb);
        DeviceTensor3D<float3> v_rgb_t(pixel_grads.v_render_rgb);
        overexposure_grad_add(rgb_t, overexposure_reg_weight, v_rgb_t);
    }

    // --- Background blend backward hook ---
    // After the color-space hook, per the forward order above. It ADDS the
    // blend's transmittance gradient into v_render_Ts, not overwrite.
    if (engine().background.enabled) {
        _engine_background_backward_hook(
            pixel_grads.v_render_rgb,
            pixel_grads.v_render_Ts,
            overexposure_reg_weight);
    }

    // Depth -> normal backward: propagate v_depth_normal grads into v_render_depth (in-place add)
    if (compute_depth_normal) {
        depth_to_normal_backward(
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dv_tv(engine().camera.intrins),
            _dt2d_tv(engine().camera.dist_coeffs),
            is_ray_depth,
            DeviceTensor3D<float>(render_depth),
            DeviceTensor3D<float3>(pixel_grads.v_depth_normal),
            DeviceTensor3D<float>(pixel_grads.v_render_depth)
        );
    }

    // Median depth -> normal backward: fold v_median_normal into v_median_depth
    // (in-place add), the same way as depth_normal above.
    if (has_median && median_normal_active) {
        depth_to_normal_backward(
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dv_tv(engine().camera.intrins),
            _dt2d_tv(engine().camera.dist_coeffs),
            is_ray_depth,
            DeviceTensor3D<float>(median_depth),
            DeviceTensor3D<float3>(pixel_grads.v_median_normal),
            DeviceTensor3D<float>(pixel_grads.v_median_depth)
        );
    }

    // --- Rasterization + projection backward ---
    // Build accum_weight_map from loss_map (pixel-space -> per-splat mapping in raster bwd)
    DeviceTensor3D<float> accum_weight_map;
    if (compute_loss_map && _tv_valid(loss_map_buf)) {
        accum_weight_map = DeviceTensor3D<float>(loss_map_buf);
    }
    _engine_raster_proj_backward(
        pixel_grads.v_render_rgb,
        pixel_grads.v_render_depth,
        pixel_grads.v_render_Ts,
        accum_weight_map,
        (DensifyAccumMode)loss_map_accum_mode,
        has_median ? DeviceTensor3D<float>(pixel_grads.v_median_depth)
                   : DeviceTensor3D<float>(),
        pixel_grads.v_rgb_dist,
        pixel_grads.v_depth_dist,
        pixel_grads.v_normal_dist);

    // --- Build loss dict for display ---
    auto sdiv = [](float x, float y) -> float { return y != 0.0f ? x / y : 0.0f; };

    std::map<std::string, float> loss_dict;
    loss_dict["rgb_loss"] = lv.rgb_loss + w_ssim * (1.0f - lv.ssim);
    loss_dict["psnr"] = lv.rgb_psnr;
    loss_dict["ssim"] = lv.ssim;
    loss_dict["depth_loss"] = sdiv(lv.depth_sup,
        loss_weights[(int)LossWeightIndex::DepthSup]);
    loss_dict["normal_loss"] = sdiv(lv.normal_sup,
        loss_weights[(int)LossWeightIndex::NormalSup]);
    loss_dict["alpha_loss"] = sdiv(lv.alpha_sup,
        loss_weights[(int)LossWeightIndex::AlphaSup] + loss_weights[(int)LossWeightIndex::AlphaSupUnder]);
    loss_dict["normal_reg"] = sdiv(lv.normal_reg,
        loss_weights[(int)LossWeightIndex::NormalReg]);
    loss_dict["alpha_reg"] = sdiv(lv.alpha_reg,
        loss_weights[(int)LossWeightIndex::AlphaReg]);
    loss_dict["rgb_dist_reg"] = sdiv(lv.rgb_dist_reg,
        loss_weights[(int)LossWeightIndex::RgbDistReg]);
    loss_dict["depth_dist_reg"] = sdiv(lv.depth_dist_reg,
        loss_weights[(int)LossWeightIndex::DepthDistReg]);
    loss_dict["normal_dist_reg"] = sdiv(lv.normal_dist_reg,
        loss_weights[(int)LossWeightIndex::NormalDistReg]);
    loss_dict["mean_median_depth_loss"] = sdiv(lv.mean_median_depth_sup,
        loss_weights[(int)LossWeightIndex::MeanMedianDepthSup]);
    loss_dict["median_depth_normal_reg"] = sdiv(lv.median_depth_normal_reg,
        loss_weights[(int)LossWeightIndex::MedianDepthNormalReg]);
    loss_dict["median_normal_loss"] = sdiv(lv.median_normal_sup,
        loss_weights[(int)LossWeightIndex::MedianNormalSup]);
    loss_dict["median_render_normal_reg"] = sdiv(lv.median_render_normal_reg,
        loss_weights[(int)LossWeightIndex::MedianRenderNormalReg]);

    return loss_dict;
}


std::map<std::string, float> engine_compute_loss_backward(
    int step,
    std::array<float, (int)LossWeightIndex::length> loss_weights,
    float w_ssim,
    int num_loss_scales,
    int loss_scale_min_pixels,
    bool compute_loss_map,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    float nms_falloff,
    bool loss_map_normalize,
    float loss_map_clip_quantile,
    float loss_map_power,
    int loss_map_accum_mode,
    float overexposure_reg_weight,
    float color_shift_reg_weight,
    float color_shift_reg_beta
) {
    return _engine_loss(loss_weights, w_ssim, num_loss_scales,
                        loss_scale_min_pixels, compute_loss_map, loss_map_mode,
                        robust_edge_aware_quantile, nms_falloff,
                        loss_map_normalize,
                        loss_map_clip_quantile, loss_map_power,
                        loss_map_accum_mode,
                        overexposure_reg_weight,
                        color_shift_reg_weight, color_shift_reg_beta,
                        _tv_null());
}


bool engine_preview_loss_map(const LossConfig& loss, TorchTensorView out) {
    if (!loss.compute_loss_map || loss.loss_map_mode == 0) return false;
    if (!engine().gt.has_gt) return false;
    if (!_tv_valid(out))
        throw std::runtime_error("engine_preview_loss_map: null destination");
    // The regularizer weights are dropped: each of them only ever adds to a
    // gradient this call does not compute.
    _engine_loss(loss.weights, loss.w_ssim, loss.num_loss_scales,
                 loss.loss_scale_min_pixels, true, loss.loss_map_mode,
                 loss.robust_edge_aware_quantile, loss.nms_falloff,
                 loss.loss_map_normalize,
                 loss.loss_map_clip_quantile, loss.loss_map_power,
                 loss.loss_map_accum_mode,
                 0.0f, 0.0f, 0.0f, out);
    return true;
}
