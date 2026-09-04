// Engine densification step (MCMC + revised relocate/add splits).

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineState.h"

#include <algorithm>
#include <cmath>


// Helper: build DeviceVector<T> from TorchTensorView (non-owning view).
template<typename T>
static inline DeviceVector<T> _dv(const TorchTensorView& tv) {
    return DeviceVector<T>(tv);
}
// For null-able TorchTensorViews, return default (null) DeviceVector.
template<typename T>
static inline DeviceVector<T> _dv_or_null(const TorchTensorView& tv) {
    if (std::get<0>(tv) == 0) return DeviceVector<T>();
    return DeviceVector<T>(tv);
}
// For [N, K, C] tensors like features_sh: flatten to [N*K, C] then build DeviceVector.
template<typename T>
static inline DeviceVector<T> _dv_flat(const TorchTensorView& tv) {
    if (std::get<0>(tv) == 0) return DeviceVector<T>();
    const auto& shape = std::get<2>(tv);
    int64_t flat_count = 1;
    for (size_t i = 0; i + 1 < shape.size(); i++) flat_count *= shape[i];
    TorchTensorView flat_tv(std::get<0>(tv), std::get<1>(tv),
        {flat_count, (int64_t)(sizeof(T) / std::get<1>(tv))});
    return DeviceVector<T>(flat_tv);
}


int engine_densify_step(int step, int max_steps, const DensifyConfig& cfg) {
    int64_t cur_num_splats = engine().cur_num_splats;
    int64_t max_num_splats = engine().max_num_splats;
    bool quantize_sh = engine().optim.sh_quantize_enabled();
    const int sh_optim_bits = engine().optim.sh_optim_bits;
    // The 4-bit (joint-nibble) path is wired through both _zero_quant_sh_for_splat
    // and the relocate/MCMC kernels via uchar3 stride; 32 / 8 / 4 are all
    // valid here. Anything else (e.g. 16) would silently mis-stride.
    if (quantize_sh && sh_optim_bits != 8 && sh_optim_bits != 4) {
        throw std::runtime_error(
            "engine_densify_step: SH-optim quantization at "
            + std::to_string(sh_optim_bits)
            + " bits is not supported alongside densification "
              "(only 4-bit and 8-bit are wired through the dst zero-encoding).");
    }
    // SH VALUE-quant: pick the right buffer (cell-block or FPBO layout) for
    // the codec-aware src->dst copy. We use clipping (no atomic bounds
    // expansion) -- inheritance precision is lost in the first densify step
    // but the FPBO writeback's block-wide (min, max) reduction expands the
    // bound on the next training step, so the child splat adapts within one
    // iteration. See `_copy_quant_sh_value_for_splat` for the rationale.
    int  sh_value_bits = engine().world.sh_value_bits;
    DeviceVector<uint8_t> dv_sh_value_packed;
    DeviceVector<float2>  dv_sh_value_bounds;
    bool sh_value_bounds_per_splat = false;
    if (sh_value_bits == 8) {
        if (engine().world.features_sh_quant8_fpbo.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant8_fpbo.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant8_fpbo.bounds;
            sh_value_bounds_per_splat = true;
        } else if (engine().world.features_sh_quant8.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant8.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant8.bounds;
            sh_value_bounds_per_splat = false;
        }
    } else if (sh_value_bits == 16) {
        if (engine().world.features_sh_quant16_fpbo.initialized()) {
            // QuantizedTensor<16>::packed is uint8_t storage with kBytesPerCell=2,
            // so the DeviceVector<uint8_t> view passed to the densify kernel
            // covers the raw byte stream; the kernel template parameter
            // BITS=16 inside the codec re-interprets each pair of bytes as a
            // uint16_t cell.
            dv_sh_value_packed = engine().world.features_sh_quant16_fpbo.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant16_fpbo.bounds;
            sh_value_bounds_per_splat = true;
        } else if (engine().world.features_sh_quant16.initialized()) {
            dv_sh_value_packed = engine().world.features_sh_quant16.packed;
            dv_sh_value_bounds = engine().world.features_sh_quant16.bounds;
            sh_value_bounds_per_splat = false;
        }
    }
    int num_sh = engine().num_sh;
    int num_sh_buffer = num_sh;  // buffer stride = engine().num_sh (model max)

    bool use_revised = (cfg.use_revised_densification);
    bool densify_ongoing =
        (step < std::max(cfg.refine_stop_iter,
                         max_steps - cfg.refine_stop_num_iter));
    bool do_densify = densify_ongoing && (step > cfg.refine_start_iter && step % cfg.refine_every == 0);
    float progress = ((float)step + 0.5f) / (float)max_steps;

    // Use pool-backed DeviceVector/DeviceTensor from Buffers directly
    auto& dv_means = engine().world.means;
    auto& dv_quats = engine().world.quats;
    auto& dv_scales = engine().world.scales;
    auto& dv_opacs = engine().world.opacities;
    auto& dv_features_dc = engine().world.features_dc;
    // features_sh: DeviceTensor2D<float3> [N, K] -> flatten to DeviceVector<float3> [N*K]
    DeviceVector<float3> dv_features_sh;
    {
        auto& t = engine().world.features_sh;
        TorchTensorView tv((uint64_t)t.data_ptr(), 4,
            {t.size<0>() * t.size<1>(), 3LL});
        dv_features_sh = DeviceVector<float3>(tv);
    }
    auto& dv_g1_means = engine().optim.g1_means;
    auto& dv_g1_quats = engine().optim.g1_quats;
    auto& dv_g1_scales = engine().optim.g1_scales;
    auto& dv_g1_opacs = engine().optim.g1_opacities;
    auto& dv_g1_features_dc = engine().optim.g1_features_dc;
    auto& dv_g2_means = engine().optim.g2_means;
    auto& dv_g2_quats = engine().optim.g2_quats;
    auto& dv_g2_scales = engine().optim.g2_scales;
    auto& dv_g2_opacs = engine().optim.g2_opacities;
    auto& dv_g2_features_dc = engine().optim.g2_features_dc;
    // g1/g2 features_sh as DeviceVector<float3> with numel=N*K. The densify kernel
    // reinterprets the data as uchar3 when quantize_sh, so the element_size carrier
    // type is just for the constructor's shape check.
    DeviceVector<float3> dv_g1_features_sh, dv_g2_features_sh;
    int64_t sh_flat = (int64_t)engine().max_num_splats * (int64_t)engine().num_sh;
    if (quantize_sh) {
        // Joint (u, sqrt(g2)) AoS layout: bytes for both halves of every cell
        // sit in a single packed buffer. Densify only zeros bytes on dst
        // splats, so we pass the same packed pointer for both the g1 and g2
        // wrappers; the kernel template is instantiated with `short3` (6 bytes
        // per SH coef = 3 channels x 2 bytes), so each write zeros the joint
        // (u, sqrt_g2) record for one (splat, coef). The second write
        // (g2_features_sh[i] = 0) targets the same memory and is a no-op.
        //
        // FPBO mode uses a separate `sh_quant_state_fpbo` buffer because its
        // per-block bounds granularity differs (per-splat-block vs per-cell-
        // block). The PACKED bytes have identical layout in both modes
        // (N*K*3*2 bytes, indexed by (splat, coef, channel)), so densify only
        // needs to pick the right buffer; bounds get rewritten by the next
        // optim-kernel pass and don't need touching here.
        uint8_t* packed = engine().optim.use_fused_proj_bwd_optim
            ? engine().optim.sh_quant_state_fpbo.packed_ptr()
            : engine().optim.sh_quant_state.packed_ptr();
        TorchTensorView tv((uint64_t)packed, 1, {sh_flat, 12LL});
        dv_g1_features_sh = DeviceVector<float3>(tv);
        dv_g2_features_sh = DeviceVector<float3>(tv);
    } else {
        // Non-quantized: storage is float3 N*K elements (12 bytes each), shape [N*K, 3], element_size=4
        TorchTensorView tv1((uint64_t)engine().optim.g1_features_sh.data_ptr(), 4, {sh_flat, 3LL});
        TorchTensorView tv2((uint64_t)engine().optim.g2_features_sh.data_ptr(), 4, {sh_flat, 3LL});
        dv_g1_features_sh = DeviceVector<float3>(tv1);
        dv_g2_features_sh = DeviceVector<float3>(tv2);
    }
    auto& dv_radii = engine().optim.radii;
    auto& dv_accum_buf = engine().optim.accum_buffer;
    auto& dv_bias_steps = engine().optim.bias_correction_steps;

    // SH quant bounds (one float4 per block) -- needed by the densify kernels
    // to encode (g1=0, g2=0) into the dst splats' packed bytes against the
    // current bounds slot. FPBO mode uses a per-splat-block layout, regular
    // FusedAppearanceOptim.cu uses a per-cell-block layout. Empty vector when SH quant
    // is disabled; the kernels treat data_ptr() == nullptr as a sentinel and
    // fall back to plain-zero writes (which don't apply to non-quant g1/g2
    // anyway).
    DeviceVector<float4> dv_sh_quant_bounds;
    bool sh_bounds_per_splat = false;
    if (quantize_sh) {
        if (engine().optim.use_fused_proj_bwd_optim) {
            dv_sh_quant_bounds = engine().optim.sh_quant_state_fpbo.bounds;
            sh_bounds_per_splat = true;
        } else {
            dv_sh_quant_bounds = engine().optim.sh_quant_state.bounds;
            sh_bounds_per_splat = false;
        }
    }

    // Non-SH Adam-state quant bundle. Same struct as FPBO consumes; densify
    // uses it only to encode (g1=0, g2=0) into each relocated dst splat's
    // packed bytes against the current per-splat-block bound (FPBO layout).
    NonShQuantState non_sh;
    if (engine().optim.non_sh_optim_bits == 16
        && engine().optim.means_quant_state_fpbo.initialized()) {
        non_sh.enabled            = true;
        non_sh.means_packed       = engine().optim.means_quant_state_fpbo.packed_ptr();
        non_sh.quats_packed       = engine().optim.quats_quant_state_fpbo.packed_ptr();
        non_sh.scales_packed      = engine().optim.scales_quant_state_fpbo.packed_ptr();
        non_sh.opacities_packed   = engine().optim.opacities_quant_state_fpbo.packed_ptr();
        non_sh.features_dc_packed = engine().optim.features_dc_quant_state_fpbo.packed_ptr();
        non_sh.means_bounds       = engine().optim.means_quant_state_fpbo.bounds_ptr();
        non_sh.quats_bounds       = engine().optim.quats_quant_state_fpbo.bounds_ptr();
        non_sh.scales_bounds      = engine().optim.scales_quant_state_fpbo.bounds_ptr();
        non_sh.opacities_bounds   = engine().optim.opacities_quant_state_fpbo.bounds_ptr();
        non_sh.features_dc_bounds = engine().optim.features_dc_quant_state_fpbo.bounds_ptr();
    }

    // The oversize split channel only feeds draws, so the accumulator is
    // allocated for exactly as long as densification runs.
    const bool use_oversize_channel =
        densify_ongoing && use_revised && cfg.oversize_split_fraction > 0.0f
        && std::isfinite(cfg.max_screen_size);
    auto& dv_oversize = engine().optim.densify_oversize;
    if (use_oversize_channel) {
        if (dv_oversize.data_ptr() == nullptr) {
            dv_oversize.resize(PoolSlot::EngDensifyOversize, max_num_splats);
            dv_oversize.zero();
        }
    } else {
        dv_oversize = DeviceVector<float>();
    }

    // Clip large splats. With the soft penalty on, the hard clip is a
    // backstop the split draw below can pay for in the same step; running it
    // every step instead leaves the penalty no room to balance against.
    const bool clip_now = !cfg.clip_screen_size_at_refine
        || cfg.refine_every <= 0 || (step % cfg.refine_every) == 0;
    if (clip_now &&
        (std::isfinite(cfg.max_screen_size) || std::isfinite(cfg.max_world_size))) {
        densify_clip_scale_tensor(
            cur_num_splats,
            dv_radii, dv_scales,
            nullptr,  // don't clip opacities
            cfg.max_screen_size,
            cfg.max_screen_size_clip_hardness,
            cfg.max_world_size
        );
    }

    // weight = accum_weight^(1-w) * world_grad_score^w, where w == 0 leaves
    // the world-grad buffer unallocated and w == 1 passes that score alone
    // (TrainerCore then also skips producing the loss map).
    if (densify_ongoing && use_revised && dv_accum_buf.data_ptr() != nullptr) {
        const float blend_w = cfg.score_blend_world_grad;
        DeviceVector<float> wg_score = engine().fwd.world_grad_score;

        // Avg's raster-bwd buffer is [sum(w*a*T), sum(a*T)] planar; divide it
        // down to a plain [N] score now that every camera has landed.
        if (engine().fwd.accum_mode == DensifyAccumMode::Avg &&
            engine().fwd.accum_weight.data_ptr() != nullptr) {
            densify_accum_finalize_tensor(cur_num_splats,
                                          engine().fwd.accum_weight);
        }

        DeviceVector<float> score;
        DeviceVector<float> score2;  // stays null unless 0 < w < 1
        // Only the opacity-gradient fallback wants the opacity weighting:
        // accum_weight (max of loss_map * alpha * T) and the world-grad score
        // already carry it, and re-applying would square the factor.
        bool weight_by_opacity = false;
        if (blend_w >= 1.0f && wg_score.data_ptr() != nullptr) {
            score = wg_score;
        } else {
            if (engine().fwd.accum_weight.data_ptr() != nullptr) {
                score = engine().fwd.accum_weight;
            } else {
                score = engine().grad.opacities.data_ptr() ? engine().grad.opacities : dv_opacs;
                weight_by_opacity = true;
            }
            if (blend_w > 0.0f && wg_score.data_ptr() != nullptr)
                score2 = wg_score;
        }

        densify_update_weight(
            cur_num_splats,
            dv_radii,
            nullptr,
            weight_by_opacity ? (float*)dv_opacs.data_ptr() : nullptr,
            score,
            score2,
            blend_w,
            cfg.score_power,
            dv_accum_buf,
            cfg.score_mode,
            cfg.max_screen_size,
            dv_oversize
        );

        // Clipping above the q-quantile leaves that quantile where it was, so
        // running it in place every step does not ratchet. A power would, so
        // it lands in the side buffer the draw and the preview then read.
        auto& dv_sample_score = engine().optim.densify_sample_score;
        // Sized for the whole pool, not cur_num_splats: the oversize draw
        // adds splats that the second draw then has to rank.
        if (cfg.final_score_power != 1.0f)
            dv_sample_score.resize(PoolSlot::EngDensifySampleScore,
                                   max_num_splats);
        else
            dv_sample_score = DeviceVector<float2>();
        densify_clip_score_tensor(cur_num_splats, dv_accum_buf,
                                  cfg.score_clip_quantile,
                                  cfg.final_score_power, dv_sample_score);
    }

    int num_added = 0;

    // Long-axis-split opacity split factor `k`, linearly scheduled over the
    // first `las_split_opacity_k_warmup` steps (clamped to [init, final]).
    float split_opacity_k = cfg.las_split_opacity_k_final;
    if (cfg.las_split_opacity_k_warmup > 0) {
        float t = (float)step / (float)cfg.las_split_opacity_k_warmup;
        t = std::max(0.0f, std::min(1.0f, t));
        split_opacity_k = cfg.las_split_opacity_k_init
            + t * (cfg.las_split_opacity_k_final - cfg.las_split_opacity_k_init);
    }

    if (do_densify && use_revised) {
        // Revised relocation (long axis split)
        relocate_splats_with_long_axis_split_tensor(
            cur_num_splats, cfg.min_opacity, split_opacity_k,
            dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
            dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
            dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
            dv_accum_buf, engine().optim.densify_sample_score, dv_bias_steps,
            sh_optim_bits, num_sh,
            dv_sh_quant_bounds, sh_bounds_per_splat,
            dv_sh_value_packed, dv_sh_value_bounds,
            sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
            non_sh,
            2 * step + 0
        );

        // Add more splats
        int64_t n_target = std::min(max_num_splats, (int64_t)(cfg.growth_factor * cur_num_splats));
        num_added = (int)std::max((int64_t)0, n_target - cur_num_splats);
        if (num_added > 0) {
            // The oversize channel draws first, so its candidate set is the
            // splats the accumulator actually covers.
            int n_oversize = 0;
            if (use_oversize_channel) {
                n_oversize = (int)std::lround(
                    cfg.oversize_split_fraction * (float)num_added);
                n_oversize = std::max(0, std::min(n_oversize, num_added));
            }
            auto add_split = [&](int64_t n_cur, int n_new,
                                 DeviceVector<float2> weights, uint32_t seed) {
                add_splats_with_long_axis_split_tensor(
                    n_cur, n_new, split_opacity_k,
                    dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
                    dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
                    dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
                    dv_accum_buf, weights,
                    dv_bias_steps,
                    sh_optim_bits, num_sh,
                    dv_sh_quant_bounds, sh_bounds_per_splat,
                    dv_sh_value_packed, dv_sh_value_bounds,
                    sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
                    non_sh,
                    seed
                );
            };
            auto& dv_score = engine().optim.densify_sample_score;
            if (n_oversize > 0) {
                DeviceVector<float2> dv_over_w;
                dv_over_w.resize(PoolSlot::EngDensifyOversizeWeight,
                                 cur_num_splats);
                densify_oversize_weight_tensor(
                    cur_num_splats, cfg.oversize_score_blend, dv_oversize,
                    dv_score.data_ptr() ? dv_score : dv_accum_buf, dv_over_w);
                add_split(cur_num_splats, n_oversize, dv_over_w,
                          2 * step + 1 + 0x5bf03635u);
                // The split kernel carries accum_buf src -> dst but not the
                // transformed score, so the second draw would rank the new
                // splats on whatever those slots held in a past life.
                if (dv_score.data_ptr())
                    backend::memset_sync(dv_score.data_ptr() + cur_num_splats,
                                         0, (size_t)n_oversize * sizeof(float2));
            }
            if (num_added - n_oversize > 0)
                add_split(cur_num_splats + n_oversize,
                          num_added - n_oversize, dv_score, 2 * step + 1);
        }
    } else if (do_densify) {
        // MCMC relocation
        relocate_splats_mcmc_tensor(
            cur_num_splats, cfg.min_opacity,
            dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
            dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
            dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
            dv_bias_steps,
            sh_optim_bits, num_sh,
            dv_sh_quant_bounds, sh_bounds_per_splat,
            dv_sh_value_packed, dv_sh_value_bounds,
            sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
            non_sh,
            2 * step + 0
        );

        // MCMC sample add
        int64_t n_target = std::min(max_num_splats, (int64_t)(cfg.growth_factor * cur_num_splats));
        num_added = (int)std::max((int64_t)0, n_target - cur_num_splats);
        if (num_added > 0) {
            add_splats_mcmc_tensor(
                cur_num_splats, num_added, cfg.min_opacity,
                dv_means, dv_quats, dv_scales, dv_opacs, dv_features_dc, dv_features_sh,
                dv_g1_means, dv_g1_quats, dv_g1_scales, dv_g1_opacs, dv_g1_features_dc, dv_g1_features_sh,
                dv_g2_means, dv_g2_quats, dv_g2_scales, dv_g2_opacs, dv_g2_features_dc, dv_g2_features_sh,
                dv_bias_steps,
                sh_optim_bits, num_sh,
                dv_sh_quant_bounds, sh_bounds_per_splat,
                dv_sh_value_packed, dv_sh_value_bounds,
                sh_value_bits, sh_value_bounds_per_splat, num_sh_buffer,
                non_sh,
                2 * step + 1
            );
        }
    }

    // Reset accum buffer after densification step
    if (do_densify && dv_accum_buf.data_ptr() != nullptr) {
        backend::memset_sync(dv_accum_buf.data_ptr(), 0, dv_accum_buf.size() * sizeof(float2));
    }
    if (do_densify && dv_oversize.data_ptr() != nullptr) {
        backend::memset_sync(dv_oversize.data_ptr(), 0,
                             dv_oversize.size() * sizeof(float));
    }

    // Add MCMC noise
    if (cfg.noise_lr > 0.0f && cfg.noise_lr_final > 0.0f) {
        float noise_scalar = cfg.noise_lr * powf(cfg.noise_lr_final / cfg.noise_lr, progress);
        if (use_revised) {
            revised_add_noise_tensor(
                cur_num_splats + num_added, noise_scalar,
                dv_radii, dv_means, dv_scales, dv_quats, dv_opacs);
        } else {
            mcmc_add_noise_tensor(
                cur_num_splats + num_added, noise_scalar,
                dv_means, dv_scales, dv_quats, dv_opacs);
        }
    }

    engine().cur_num_splats = cur_num_splats + num_added;
    return num_added;
}
