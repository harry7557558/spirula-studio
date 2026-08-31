// Engine Adam + per-splat regularization optimizer step.

#include "engine/Engine.h"
#include "engine/EngineCommon.h"
#include "engine/EngineState.h"

#include <stdexcept>
#include <variant>
#include <vector>


// FPBO block granularity for SH-quant bounds: one float4 per kFpboBlock splats,
// covering all 3*K cells per splat. Mirrors the BLOCK_SIZE used inside the
// fused projection-bwd+optim kernel launcher.
static constexpr int64_t kFpboBlock = 256;


static void _ensure_optim_state(int sh_optim_bits, int sh_value_bits,
                                int non_sh_optim_bits,
                                bool use_per_splat_bias_correction = false) {
    if (sh_optim_bits != 32 && sh_optim_bits != 8 && sh_optim_bits != 4) {
        throw std::runtime_error(
            "_ensure_optim_state: sh_optim_bits must be 32 (off), 4, or 8; got " +
            std::to_string(sh_optim_bits));
    }
    if (sh_value_bits != 32 && sh_value_bits != 8 && sh_value_bits != 16) {
        throw std::runtime_error(
            "_ensure_optim_state: sh_value_bits must be 32 (off), 8, or 16; got " +
            std::to_string(sh_value_bits));
    }
    if (non_sh_optim_bits != 32 && non_sh_optim_bits != 16) {
        throw std::runtime_error(
            "_ensure_optim_state: non_sh_optim_bits must be 32 (off) or 16; got " +
            std::to_string(non_sh_optim_bits));
    }
    bool quantize_sh = (sh_optim_bits != 32);
    bool quantize_sh_value = (sh_value_bits != 32);
    bool quantize_non_sh = (non_sh_optim_bits != 32);
    bool fused = engine().optim.use_fused_proj_bwd_optim;
    // Non-FPBO + value-quant: requires BOTH optim-state quant AND value quant
    // to be active (the kernel is a doubly-quantized Adam step that reads /
    // writes through both codecs in lockstep, indexed per cell-block). Plain
    // fp32 optim-state with quantized values isn't a supported combination.
    if (quantize_sh_value && !fused && !quantize_sh) {
        throw std::runtime_error(
            "_ensure_optim_state: non-FPBO sh_value_bits = " +
            std::to_string(sh_value_bits) +
            " requires sh_optim_bits != 32 as well (the doubly-quantized "
            "Adam kernel pairs the two codecs).");
    }
    // Non-SH Adam-state quantization is now supported on the non-FPBO path
    // via the doubly-quantized branch in fused_optim_3dgs_geometry_kernel
    // (per-splat-block bound layout matches the kernel's 256-thread launch
    // geometry). Both FPBO and non-FPBO use the same packed buffer layout
    // (engine().optim.*_quant_state_fpbo), so no separate allocation path
    // is needed; the kernel reads/writes through _OptimNonShQ when non_sh
    // is enabled, just like FPBO's _NonShQ.
    if (engine().optim.initialized && engine().optim.sh_optim_bits == sh_optim_bits
        && engine().world.sh_value_bits == sh_value_bits
        && engine().optim.non_sh_optim_bits == non_sh_optim_bits
        && engine().optim.use_per_splat_bias_correction == use_per_splat_bias_correction
        && engine().optim.fused_state_active == fused)
        return;

    int64_t N = engine().max_num_splats;
    int64_t K = engine().num_sh;
    engine().optim.sh_optim_bits = sh_optim_bits;
    engine().optim.non_sh_optim_bits = non_sh_optim_bits;
    engine().optim.fused_state_active = fused;
    engine().world.sh_value_bits = sh_value_bits;

    // SH VALUE-quant: canonical storage is the packed buffer below. Drop the
    // fp32 features_sh allocation (sized N*K*float3 = 12*N*K bytes -- the
    // largest world buffer on big scenes) and keep only its [N, K] shape
    // descriptor so downstream consumers that derive sh_degree from the
    // TensorArray stride still work. Re-alloc on flip back to fp32.
    if (quantize_sh_value) {
        DevicePool::global().free(PoolSlot::WorldFeaturesSh);
        engine().world.features_sh.set_shape_no_alloc(N, K);
    } else if (engine().world.features_sh.data_ptr() == nullptr && N > 0 && K > 0) {
        engine().world.features_sh.resize(PoolSlot::WorldFeaturesSh, N, K);
    }

    // g1 (exp_avg) / g2 (exp_avg_sq): FP32 buffers per attribute. When
    // non_sh_optim_bits != 32 the canonical store is the per-attribute
    // QuantizedAdamState packed buffer below; free the fp32 buffers then.
    if (!quantize_non_sh) {
        engine().optim.g1_means.resize       (PoolSlot::EngG1Means,       N);
        engine().optim.g1_quats.resize       (PoolSlot::EngG1Quats,       N);
        engine().optim.g1_scales.resize      (PoolSlot::EngG1Scales,      N);
        engine().optim.g1_opacities.resize   (PoolSlot::EngG1Opacities,   N);
        engine().optim.g1_features_dc.resize (PoolSlot::EngG1FeaturesDc, N);
        engine().optim.g2_means.resize       (PoolSlot::EngG2Means,       N);
        engine().optim.g2_quats.resize       (PoolSlot::EngG2Quats,       N);
        engine().optim.g2_scales.resize      (PoolSlot::EngG2Scales,      N);
        engine().optim.g2_opacities.resize   (PoolSlot::EngG2Opacities,   N);
        engine().optim.g2_features_dc.resize (PoolSlot::EngG2FeaturesDc, N);
    } else {
        DevicePool::global().free(PoolSlot::EngG1Means);
        DevicePool::global().free(PoolSlot::EngG1Quats);
        DevicePool::global().free(PoolSlot::EngG1Scales);
        DevicePool::global().free(PoolSlot::EngG1Opacities);
        DevicePool::global().free(PoolSlot::EngG1FeaturesDc);
        DevicePool::global().free(PoolSlot::EngG2Means);
        DevicePool::global().free(PoolSlot::EngG2Quats);
        DevicePool::global().free(PoolSlot::EngG2Scales);
        DevicePool::global().free(PoolSlot::EngG2Opacities);
        DevicePool::global().free(PoolSlot::EngG2FeaturesDc);
        engine().optim.g1_means       = DeviceVector<float3>();
        engine().optim.g1_quats       = DeviceVector<float4>();
        engine().optim.g1_scales      = DeviceVector<float3>();
        engine().optim.g1_opacities   = DeviceVector<float>();
        engine().optim.g1_features_dc = DeviceVector<float3>();
        engine().optim.g2_means       = DeviceVector<float3>();
        engine().optim.g2_quats       = DeviceVector<float4>();
        engine().optim.g2_scales      = DeviceVector<float3>();
        engine().optim.g2_opacities   = DeviceVector<float>();
        engine().optim.g2_features_dc = DeviceVector<float3>();
    }
    if (!quantize_sh)
        engine().optim.g1_features_sh.resize(PoolSlot::EngG1FeaturesSh, N, K);
    else
        engine().optim.g1_features_sh = DeviceTensor2D<float3>();
    if (!quantize_sh)
        engine().optim.g2_features_sh.resize(PoolSlot::EngG2FeaturesSh, N, K);
    else
        engine().optim.g2_features_sh = DeviceTensor2D<float3>();

    // Non-SH Adam-state quant: per-attribute QuantizedAdamState<16, 256>.
    // Per-splat-block layout (one float4 bound per 256 splats, covering all
    // primitives of that attribute). The non-FPBO geometry kernel launches
    // the same 256-thread blocks, so it reads/writes the SAME layout via
    // _OptimNonShQ.
    if (quantize_non_sh) {
        int64_t n_bounds = (N + kFpboBlock - 1) / kFpboBlock;
        engine().optim.means_quant_state_fpbo       .resize(PoolSlot::EngMeansQfpbo,       (int64_t)N * 3, n_bounds);
        engine().optim.quats_quant_state_fpbo       .resize(PoolSlot::EngQuatsQfpbo,       (int64_t)N * 4, n_bounds);
        engine().optim.scales_quant_state_fpbo      .resize(PoolSlot::EngScalesQfpbo,      (int64_t)N * 3, n_bounds);
        engine().optim.opacities_quant_state_fpbo   .resize(PoolSlot::EngOpacitiesQfpbo,   (int64_t)N * 1, n_bounds);
        engine().optim.features_dc_quant_state_fpbo .resize(PoolSlot::EngFeaturesDcQfpbo, (int64_t)N * 3, n_bounds);
    } else {
        engine().optim.means_quant_state_fpbo       = QuantizedAdamState<16, 256>();
        engine().optim.quats_quant_state_fpbo       = QuantizedAdamState<16, 256>();
        engine().optim.scales_quant_state_fpbo      = QuantizedAdamState<16, 256>();
        engine().optim.opacities_quant_state_fpbo   = QuantizedAdamState<16, 256>();
        engine().optim.features_dc_quant_state_fpbo = QuantizedAdamState<16, 256>();
    }

    // radii [max_N]
    engine().optim.radii.resize(PoolSlot::EngRadii, N);

    // accum_buffer [max_N]
    engine().optim.accum_buffer.resize(PoolSlot::EngAccumBuffer, N);

    // Joint (u, sqrt(g2)) Adam state for SH features.
    //   Non-fused path: FusedAppearanceOptim.cu's fused_adam_with_steps_8bit_kernel
    //   launches one thread per cell (= per (splat, coef, channel)), grouped
    //   256 threads per CUDA block -- one float4 bounds slot per 256 cells.
    //   Fused path: FPBO launches one thread per SPLAT, grouped 256 splats
    //   per block, and the kernel writes one bounds slot per block. So same
    //   packed-cell count, but 3*K x fewer bounds slots.
    int64_t sh_cells = (int64_t)N * K * 3;
    // The FPBO layout addresses whole 256-splat blocks of whole u32 words, so
    // it needs the rounded-up cell count (docs/notes/sh-quant-layout.md).
    int64_t sh_cells_fpbo = sh_fpbo_cells(N, (uint32_t)K);
    if (quantize_sh && !fused) {
        constexpr int64_t BLOCK_SIZE = QuantizedAdamState<8, 256>::kBlockSize;
        int64_t sh_bounds = (sh_cells + BLOCK_SIZE - 1) / BLOCK_SIZE;
        engine().optim.sh_quant_state.resize(PoolSlot::EngShQuant, sh_cells, sh_bounds);
        engine().optim.sh_quant_state_fpbo = QuantizedAdamState<8, 256>();
    } else if (quantize_sh && fused) {
        int64_t sh_bounds = (N + kFpboBlock - 1) / kFpboBlock;
        engine().optim.sh_quant_state_fpbo.resize(PoolSlot::EngShQuantFpbo, sh_cells_fpbo, sh_bounds);
        engine().optim.sh_quant_state = QuantizedAdamState<8, 256>();
    } else {
        engine().optim.sh_quant_state      = QuantizedAdamState<8, 256>();
        engine().optim.sh_quant_state_fpbo = QuantizedAdamState<8, 256>();
    }

    // SH VALUE-quant storage. Mirror of the optim-state allocation above:
    //   non-FPBO uses the per-CELL-block layout (`features_sh_quant{8,16}`),
    //   FPBO uses the per-SPLAT-block layout (`features_sh_quant{8,16}_fpbo`).
    // Bounds count per layout:
    //   cell-block: ceil(sh_cells / 256)
    //   fpbo:       ceil(N / 256)
    // Only one of (cell-block, fpbo) is sized; the unused one is reset to an
    // empty (default-constructed) view. The pool keeps the unused slot's prior
    // allocation at zero cap.
    {
        constexpr int64_t BLOCK_SIZE = QuantizedTensor<8, 256>::kBlockSize;
        int64_t v_bounds_cell = (sh_cells + BLOCK_SIZE - 1) / BLOCK_SIZE;
        int64_t v_bounds_fpbo = (N + kFpboBlock - 1) / kFpboBlock;
        if (quantize_sh_value && sh_value_bits == 8 && !fused) {
            engine().world.features_sh_quant8.resize(PoolSlot::EngWorldShVq8, sh_cells, v_bounds_cell);
            engine().world.features_sh_quant16 = QuantizedTensor<16, 256>();
            engine().world.features_sh_quant8_fpbo  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16_fpbo = QuantizedTensor<16, 256>();
        } else if (quantize_sh_value && sh_value_bits == 8 && fused) {
            engine().world.features_sh_quant8_fpbo.resize(PoolSlot::EngWorldShVq8Fpbo, sh_cells_fpbo, v_bounds_fpbo);
            engine().world.features_sh_quant16 = QuantizedTensor<16, 256>();
            engine().world.features_sh_quant8  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16_fpbo = QuantizedTensor<16, 256>();
        } else if (quantize_sh_value && sh_value_bits == 16 && !fused) {
            engine().world.features_sh_quant16.resize(PoolSlot::EngWorldShVq16, sh_cells, v_bounds_cell);
            engine().world.features_sh_quant8  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant8_fpbo  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16_fpbo = QuantizedTensor<16, 256>();
        } else if (quantize_sh_value && sh_value_bits == 16 && fused) {
            engine().world.features_sh_quant16_fpbo.resize(PoolSlot::EngWorldShVq16Fpbo, sh_cells_fpbo, v_bounds_fpbo);
            engine().world.features_sh_quant8  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16 = QuantizedTensor<16, 256>();
            engine().world.features_sh_quant8_fpbo  = QuantizedTensor<8,  256>();
        } else {
            engine().world.features_sh_quant8       = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16      = QuantizedTensor<16, 256>();
            engine().world.features_sh_quant8_fpbo  = QuantizedTensor<8,  256>();
            engine().world.features_sh_quant16_fpbo = QuantizedTensor<16, 256>();
        }
    }

    // bias_correction_steps
    engine().optim.use_per_splat_bias_correction = use_per_splat_bias_correction;
    if (use_per_splat_bias_correction) {
        engine().optim.bias_correction_steps.resize(PoolSlot::EngBiasCorrectionSteps, N);
    } else {
        engine().optim.bias_correction_steps = DeviceVector<int32_t>();
    }

    // Zero everything on first init. For the quantized non-SH path the codec's
    // (u=0, log_s=0) -> (g1=0, g2=0) fixed point makes zeroing the packed
    // bytes a valid init (no encode pass needed); bounds get re-reduced on
    // the first FPBO step.
    if (!quantize_non_sh) {
        engine().optim.g1_means.zero();       engine().optim.g2_means.zero();
        engine().optim.g1_quats.zero();       engine().optim.g2_quats.zero();
        engine().optim.g1_scales.zero();      engine().optim.g2_scales.zero();
        engine().optim.g1_opacities.zero();   engine().optim.g2_opacities.zero();
        engine().optim.g1_features_dc.zero(); engine().optim.g2_features_dc.zero();
    } else {
        engine().optim.means_quant_state_fpbo       .zero();
        engine().optim.quats_quant_state_fpbo       .zero();
        engine().optim.scales_quant_state_fpbo      .zero();
        engine().optim.opacities_quant_state_fpbo   .zero();
        engine().optim.features_dc_quant_state_fpbo .zero();
    }
    if (quantize_sh && !fused) {
        engine().optim.sh_quant_state.zero();
    } else if (quantize_sh && fused) {
        engine().optim.sh_quant_state_fpbo.zero();
    } else {
        engine().optim.g1_features_sh.zero();
        engine().optim.g2_features_sh.zero();
    }
    // SH VALUE-quant zero-init. Initial features_sh is zero (only DC has
    // non-zero defaults), so zeroing both packed bytes and bounds is a valid
    // initial state -- the codec decodes byte=0 against bound=(0,0) to 0.0f.
    // If non-zero initial values ever get baked in upstream, this needs an
    // encode pass instead.
    if (engine().world.features_sh_quant8.initialized())       engine().world.features_sh_quant8.zero();
    if (engine().world.features_sh_quant16.initialized())      engine().world.features_sh_quant16.zero();
    if (engine().world.features_sh_quant8_fpbo.initialized())  engine().world.features_sh_quant8_fpbo.zero();
    if (engine().world.features_sh_quant16_fpbo.initialized()) engine().world.features_sh_quant16_fpbo.zero();
    engine().optim.accum_buffer.zero();
    engine().optim.bias_correction_steps.zero();

    engine().optim.initialized = true;
}


// Public wrapper so the checkpoint loader can force-allocate optimizer state to
// a saved layout without running a training step. Reads engine().optim.
// use_fused_proj_bwd_optim (set by the loader from state.json) for the layout.
void engine_ensure_optim_state(int sh_optim_bits, int sh_value_bits,
                               int non_sh_optim_bits,
                               bool use_per_splat_bias_correction) {
    _ensure_optim_state(sh_optim_bits, sh_value_bits, non_sh_optim_bits,
                        use_per_splat_bias_correction);
}


// Build a [N, P]-shaped DeviceTensorFloatND view over a raw float buffer used
// for the per-attribute Adam state (g1/g2). Mirrors the layout produced by
// `DeviceTensorFloatND(DeviceVector<floatK>)`.
static inline DeviceTensorFloatND _fnd_view(float* ptr, int64_t N, int64_t P) {
    TorchTensorView tv((uint64_t)ptr, 4, {N, P, 1LL});
    return DeviceTensorFloatND(tv);
}


// Fused projection-bwd + Adam-and-regularization optimizer step. Replaces
// the projection_*_backward + engine_optim_step pair when
// `cfg.use_fused_proj_bwd_optim` is set. Requires
// engine_compute_loss_backward to have stashed v_splats_s into
// engine().fwd.v_splats_s. World-space gradient buffers are not allocated
// in this path (raster_*_bwd's world atomicStore is null-pointer-guarded
// per channel, and the fused kernel's atomicLoad uses the same guard).
void engine_fused_proj_bwd_optim_step(int step, const OptimConfig& cfg) {
    _ensure_optim_state(cfg.sh_optim_bits, cfg.sh_value_bits,
                        cfg.non_sh_optim_bits,
                        cfg.use_per_splat_bias_correction);

    int64_t N = engine().cur_num_splats;
    if (engine().fwd.v_splats_s.empty())
        throw std::runtime_error(
            "engine_fused_proj_bwd_optim_step: v_splats_s not stashed; "
            "engine_compute_loss_backward must run first");

    // World-grad vector handed to the FPBO kernel. For 3dgs / mip the
    // per-channel slots stay null (raster_*_bwd never writes the world
    // buffer for those prims, so there is nothing to pick up). For 3dgut,
    // raster_*_bwd has already atomicAdded mean / quat / scale into the
    // engine grad buffers (_alloc_grad_buffers allocates them in that case);
    // pass those three slots so the kernel's atomicLoad picks them up and
    // sums them with the projection-bwd contribution. Other slots remain
    // null -- they were never written and reading 0 is correct.
    std::vector<DeviceTensorFloatND> v_splats_w;
    if (engine().grad.means.data_ptr() != nullptr) {
        v_splats_w = {
            DeviceTensorFloatND(engine().grad.means),         // [N, 3]
            DeviceTensorFloatND(engine().grad.quats),         // [N, 4]
            DeviceTensorFloatND(engine().grad.scales),        // [N, 3]
            DeviceTensorFloatND(),                            // opacities -- null
            DeviceTensorFloatND(),                            // features_dc -- null
            DeviceTensorFloatND(),                            // features_sh -- null
        };
    }

    // Adam state. For SH, fp32 g1/g2 OR quantized packed-bytes -- never both.
    int64_t M  = engine().max_num_splats;
    int64_t K3 = (int64_t)engine().num_sh * 3;

    // SH g1/g2 view: zero-shaped empty view when quantized SH is active so
    // the kernel takes its quant path (driven by sh_packed != nullptr).
    const bool quantize_sh = (cfg.sh_optim_bits != 32);
    DeviceTensorFloatND g1_sh = quantize_sh
        ? DeviceTensorFloatND()
        : _fnd_view((float*)engine().optim.g1_features_sh.data_ptr(), M, K3);
    DeviceTensorFloatND g2_sh = quantize_sh
        ? DeviceTensorFloatND()
        : _fnd_view((float*)engine().optim.g2_features_sh.data_ptr(), M, K3);

    std::vector<DeviceTensorFloatND> g1 = {
        _fnd_view((float*)engine().optim.g1_means.data_ptr(),       M, 3),
        _fnd_view((float*)engine().optim.g1_quats.data_ptr(),       M, 4),
        _fnd_view((float*)engine().optim.g1_scales.data_ptr(),      M, 3),
        _fnd_view((float*)engine().optim.g1_opacities.data_ptr(),   M, 1),
        _fnd_view((float*)engine().optim.g1_features_dc.data_ptr(), M, 3),
        g1_sh,
    };
    std::vector<DeviceTensorFloatND> g2 = {
        _fnd_view((float*)engine().optim.g2_means.data_ptr(),       M, 3),
        _fnd_view((float*)engine().optim.g2_quats.data_ptr(),       M, 4),
        _fnd_view((float*)engine().optim.g2_scales.data_ptr(),      M, 3),
        _fnd_view((float*)engine().optim.g2_opacities.data_ptr(),   M, 1),
        _fnd_view((float*)engine().optim.g2_features_dc.data_ptr(), M, 3),
        g2_sh,
    };

    // SH quantization: pass packed-byte + per-block bounds when active.
    std::optional<TorchTensorView> sh_packed_opt   = std::nullopt;
    std::optional<TorchTensorView> sh_bounds_opt   = std::nullopt;
    if (quantize_sh && engine().optim.sh_quant_state_fpbo.initialized()) {
        auto& qs = engine().optim.sh_quant_state_fpbo;
        sh_packed_opt = TorchTensorView(
            (uint64_t)qs.packed_ptr(), 1, {qs.packed_bytes()});
        sh_bounds_opt = TorchTensorView(
            (uint64_t)qs.bounds_ptr(), 4, {qs.n_bounds, 4LL});
    }

    // Per-splat bias correction: increment all steps by 1 before use.
    std::variant<int32_t, TorchTensorView> step_arg;
    if (cfg.use_per_splat_bias_correction) {
        increment_int32_inplace(engine().optim.bias_correction_steps,
                                engine().max_num_splats);
        step_arg = _dv_tv(engine().optim.bias_correction_steps);
    } else {
        step_arg = (int32_t)(step + 1);
    }

    // World-splat tensors come straight from fwd cache; they alias the
    // engine's mutable world params, which the fused kernel updates in place.
    auto& splats_w = engine().fwd.splats_w;

    auto& aabb_q = engine().fwd.aabb;

    // Dispatch on primitive. Pass engine().sh_degree (= sh_degree_to_use) so
    // the kernel-template SH degree matches the forward pass during warmup --
    // otherwise SH backward computes spurious gradients on unused bands which
    // contaminate v_mean through the viewdir chain.
    int max_sh_degree = engine().sh_degree;
    // SH VALUE-quant: pass packed + per-splat-block bounds when active. The
    // FPBO kernel expects the per-splat-block layout (1 float2 per 256 splats)
    // -- the allocation in _ensure_optim_state above set that up in
    // engine().world.features_sh_quant{8,16}_fpbo.
    std::optional<TorchTensorView> sh_value_packed_opt = std::nullopt;
    std::optional<TorchTensorView> sh_value_bounds_opt = std::nullopt;
    if (cfg.sh_value_bits == 8 && engine().world.features_sh_quant8_fpbo.initialized()) {
        auto& vq = engine().world.features_sh_quant8_fpbo;
        sh_value_packed_opt = TorchTensorView(
            (uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
        sh_value_bounds_opt = TorchTensorView(
            (uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
    } else if (cfg.sh_value_bits == 16 && engine().world.features_sh_quant16_fpbo.initialized()) {
        auto& vq = engine().world.features_sh_quant16_fpbo;
        sh_value_packed_opt = TorchTensorView(
            (uint64_t)vq.packed_ptr(), 1, {vq.packed_bytes()});
        sh_value_bounds_opt = TorchTensorView(
            (uint64_t)vq.bounds_ptr(), 4, {vq.n_bounds, 2LL});
    }

    // Densification world-grad score output. Allocated (pool-backed) only
    // when the score blend is enabled; a null buffer makes the kernel skip
    // the per-splat store. Consumed by engine_densify_step, which runs after
    // this optim step within the same training step.
    DeviceVector<float> densify_score;
    if (cfg.write_densify_world_grad_score) {
        engine().fwd.world_grad_score.resize(
            PoolSlot::EngDensifyWorldGradScore, engine().max_num_splats);
        densify_score = engine().fwd.world_grad_score;
    } else {
        engine().fwd.world_grad_score = DeviceVector<float>();
    }

    // Non-SH Adam-state quant bundle. Pointers all populated when 16-bit
    // quant is on (FPBO-only, enforced in _ensure_optim_state above).
    NonShQuantState non_sh;
    if (cfg.non_sh_optim_bits == 16
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

    auto call_dispatch = [&](auto fn) {
        fn(
            N, max_sh_degree, splats_w,
            _dt2d_tv(engine().camera.viewmats),
            _dv_tv(engine().camera.intrins),
            (uint32_t)engine().camera.width,
            (uint32_t)engine().camera.height,
            engine().camera.model_str,
        engine().camera.distortion_str,
            _dt2d_tv(engine().camera.dist_coeffs),
            engine().fwd.camera_ids,
            engine().fwd.gaussian_ids,
            aabb_q,
            v_splats_w,
            engine().fwd.v_splats_s,
            g1, g2,
            sh_packed_opt, sh_bounds_opt,
            sh_value_packed_opt, sh_value_bounds_opt,
            non_sh,
            engine().optim.radii,
            densify_score,
            cfg.lr_means, cfg.lr_quats, cfg.lr_scales,
            cfg.lr_opacities, cfg.lr_features_dc, cfg.lr_features_sh,
            cfg.max_gauss_ratio, cfg.scale_regularization_weight,
            cfg.mcmc_opacity_reg_weight, cfg.mcmc_scale_reg_weight,
            cfg.erank_reg_weight, cfg.erank_reg_weight_s3,
            cfg.quat_norm_reg_weight, cfg.dc_reg_weight, cfg.sh_reg_weight,
            cfg.max_screen_size, cfg.max_screen_size_penalty,
            cfg.use_scale_agnostic_mean,
            // The two flags are tied to the same Python source
            // (splat_color_is_linear); collapse to one to halve the FPBO
            // color-space instantiation axis.
            cfg.use_color_trust_region || cfg.color_is_linear,
            cfg.eps_tr,
            step_arg,
            cfg.quantization_level
        );
    };

    if (engine().primitive == "3dgs") {
        call_dispatch(&fused_projection_bwd_optimizer_3dgs);
    } else if (engine().primitive == "mip") {
        call_dispatch(&fused_projection_bwd_optimizer_mip);
    } else if (engine().primitive == "3dgut") {
        call_dispatch(&fused_projection_bwd_optimizer_3dgut);
    } else {
        throw std::runtime_error(
            "engine_fused_proj_bwd_optim_step: unsupported primitive: " +
            engine().primitive);
    }
}


void engine_optim_step(int step, const OptimConfig& cfg) {
    if (engine().optim.use_fused_proj_bwd_optim) {
        engine_fused_proj_bwd_optim_step(step, cfg);
        return;
    }

    _ensure_optim_state(cfg.sh_optim_bits, cfg.sh_value_bits,
                        cfg.non_sh_optim_bits, cfg.use_per_splat_bias_correction);

    int64_t N = engine().cur_num_splats;

    // Per-splat bias correction: increment all steps by 1 before use
    DeviceVector<int32_t> per_splat_steps;
    if (cfg.use_per_splat_bias_correction) {
        increment_int32_inplace(engine().optim.bias_correction_steps, engine().max_num_splats);
        per_splat_steps = engine().optim.bias_correction_steps;
    }

    // Sub-batched training: grad_scale = 1/B and zero_grad_in_optim = true
    // are stashed by engine_train_step's sub-batch dispatcher. When the
    // dispatcher isn't active (single-batch training) these stay at the
    // defaults (1.0f, false) and the kernels behave exactly as before.
    const float grad_scale = engine().optim.grad_scale;
    const bool  zero_grad  = engine().optim.zero_grad_in_optim;

    // Non-SH Adam-state quant bundle. Same struct as FPBO consumes; when
    // enabled, the geometry kernel reads/writes the 5 non-SH attrs (means,
    // quats, scales, opacities, features_dc) through the codec instead of
    // the fp32 g1_*/g2_* DeviceVectors (which are freed in this mode).
    NonShQuantState non_sh_optim;
    if (cfg.non_sh_optim_bits == 16
        && engine().optim.means_quant_state_fpbo.initialized()) {
        non_sh_optim.enabled            = true;
        non_sh_optim.means_packed       = engine().optim.means_quant_state_fpbo.packed_ptr();
        non_sh_optim.quats_packed       = engine().optim.quats_quant_state_fpbo.packed_ptr();
        non_sh_optim.scales_packed      = engine().optim.scales_quant_state_fpbo.packed_ptr();
        non_sh_optim.opacities_packed   = engine().optim.opacities_quant_state_fpbo.packed_ptr();
        non_sh_optim.features_dc_packed = engine().optim.features_dc_quant_state_fpbo.packed_ptr();
        non_sh_optim.means_bounds       = engine().optim.means_quant_state_fpbo.bounds_ptr();
        non_sh_optim.quats_bounds       = engine().optim.quats_quant_state_fpbo.bounds_ptr();
        non_sh_optim.scales_bounds      = engine().optim.scales_quant_state_fpbo.bounds_ptr();
        non_sh_optim.opacities_bounds   = engine().optim.opacities_quant_state_fpbo.bounds_ptr();
        non_sh_optim.features_dc_bounds = engine().optim.features_dc_quant_state_fpbo.bounds_ptr();
    }

    // Densification world-grad score output. Allocated (pool-backed) only
    // when the score blend is enabled; a null buffer makes the kernel skip
    // the per-splat store. Must be written here (not read from grad.means at
    // densify time): the geometry kernel consumes the batch-accumulated mean
    // gradient this step and zeroes it as a fused side effect in split-batch
    // mode.
    DeviceVector<float> densify_score;
    if (cfg.write_densify_world_grad_score) {
        engine().fwd.world_grad_score.resize(
            PoolSlot::EngDensifyWorldGradScore, engine().max_num_splats);
        densify_score = engine().fwd.world_grad_score;
    } else {
        engine().fwd.world_grad_score = DeviceVector<float>();
    }

    // Block-wise quantized gradient input (non-FPBO grad-quant path). Populated
    // per attribute from engine().grad.*_q; a null *_packed makes the geometry
    // kernel read the fp32 v_* buffer for that attribute (3dgut means/quats/
    // scales, or grad-quant off).
    GradQuantBuffers grad_q;
    {
        auto& g = engine().grad;
        if (g.means_q.initialized())       { grad_q.means_packed  = g.means_q.packed_ptr();       grad_q.means_bounds  = g.means_q.bounds_ptr(); }
        if (g.quats_q.initialized())       { grad_q.quats_packed  = g.quats_q.packed_ptr();       grad_q.quats_bounds  = g.quats_q.bounds_ptr(); }
        if (g.scales_q.initialized())      { grad_q.scales_packed = g.scales_q.packed_ptr();      grad_q.scales_bounds = g.scales_q.bounds_ptr(); }
        if (g.opacities_q.initialized())   { grad_q.opac_packed   = g.opacities_q.packed_ptr();   grad_q.opac_bounds   = g.opacities_q.bounds_ptr(); }
        if (g.features_dc_q.initialized()) { grad_q.dc_packed     = g.features_dc_q.packed_ptr(); grad_q.dc_bounds     = g.features_dc_q.bounds_ptr(); }
    }

    fused_optim_3dgs_geometry(
        N,
        engine().world.means,     engine().grad.means,     engine().optim.g1_means,     engine().optim.g2_means,
        engine().world.quats,     engine().grad.quats,     engine().optim.g1_quats,     engine().optim.g2_quats,
        engine().world.scales,    engine().grad.scales,    engine().optim.g1_scales,    engine().optim.g2_scales,
        engine().world.opacities, engine().grad.opacities, engine().optim.g1_opacities, engine().optim.g2_opacities,
        engine().world.features_dc, engine().grad.features_dc,
        engine().optim.radii,
        densify_score,
        cfg.lr_means, cfg.lr_quats, cfg.lr_scales, cfg.lr_opacities,
        cfg.lr_features_dc,
        cfg.max_gauss_ratio, cfg.scale_regularization_weight,
        cfg.mcmc_opacity_reg_weight, cfg.mcmc_scale_reg_weight,
        cfg.erank_reg_weight, cfg.erank_reg_weight_s3, cfg.quat_norm_reg_weight,
        cfg.dc_reg_weight, cfg.sh_reg_weight,
        cfg.max_screen_size, cfg.max_screen_size_penalty,
        cfg.use_scale_agnostic_mean,
        non_sh_optim,
        grad_q,
        step + 1, per_splat_steps,
        grad_scale, zero_grad
    );

    // DC color update path. When non-SH quant is on the geometry kernel
    // already updated features_dc through the codec; skip the separate
    // launch. Otherwise: trust-region Adam (linear/sRGB color space) or
    // plain fused Adam, mirroring the previous behavior.
    if (!non_sh_optim.enabled) {
        if (cfg.use_color_trust_region) {
            const int s1 = step + 1;
            const float beta1 = 0.9f, beta2 = 0.999f, eps = 1e-15f;
            (cfg.color_is_linear ? fused_adamtr_linear_rgb_optim : fused_adamtr_rgb_optim)(
                _dv_tv(engine().world.features_dc),
                _dv_tv(engine().grad.features_dc),
                _dv_tv(engine().optim.g1_features_dc),
                _dv_tv(engine().optim.g2_features_dc),
                _dv_tv(engine().world.opacities),
                cfg.lr_features_dc,
                beta1, beta2, eps, cfg.eps_tr, s1,
                grad_scale, zero_grad
            );
        } else {
            fused_adam_step(N,
                DeviceTensorFloatND(engine().world.features_dc),
                DeviceTensorFloatND(engine().grad.features_dc),
                DeviceTensorFloatND(engine().optim.g1_features_dc),
                DeviceTensorFloatND(engine().optim.g2_features_dc),
                cfg.lr_features_dc, step + 1, per_splat_steps,
                cfg.dc_reg_weight, 0.5f / 0.28209479177387814f,
                grad_scale, zero_grad);
        }
    }

    // SH update: pick the right kernel based on (quant value, quant optim,
    // trust region). FPBO is handled by engine_fused_proj_bwd_optim_step
    // above; everything below is the non-FPBO path.
    //
    //   (value-quant on  + optim-quant on)  -> doubly-quantized kernel
    //   (value-quant off + optim-quant on)  -> singly-quantized (optim) kernel
    //   (value-quant off + optim-quant off + TR)  -> TR Adam (fp32)
    //   (value-quant off + optim-quant off)  -> plain fused Adam (fp32)
    //
    // Value-quant + optim-fp32 is rejected at _ensure_optim_state.
    // SH-quant + TR is not supported (TR kernels need fp32 g1/g2), so the
    // doubly-quant and singly-quant paths ignore use_color_trust_region.
    // Block-wise quantized SH grad (non-FPBO grad-quant path). Null keeps the
    // fp32 grad read; when set, the doubly-quantized kernel decodes SH grad
    // from the 8-bit codec (per-splat-block bound) instead of dense features_sh.
    const uint8_t* sh_grad_q_packed = nullptr;
    const float2*  sh_grad_q_bounds = nullptr;
    if (engine().grad.quantize_grad && engine().grad.features_sh_q.initialized()) {
        sh_grad_q_packed = engine().grad.features_sh_q.packed_ptr();
        sh_grad_q_bounds = engine().grad.features_sh_q.bounds_ptr();
    }
    if (cfg.sh_value_bits != 32
        && engine().world.features_sh_quant8.initialized()) {
        // 8-bit value + (4 or 8)-bit optim; canonical SH value lives in the
        // packed buffer (engine().world.features_sh is unallocated here).
        fused_adam_step_quantized_value(
            N,
            (int64_t)N * (int64_t)engine().num_sh * 3LL,
            DeviceTensorFloatND(engine().grad.features_sh),
            sh_grad_q_packed, sh_grad_q_bounds,
            engine().optim.sh_quant_state.packed_ptr(),
            engine().optim.sh_quant_state.bounds_ptr(),
            engine().world.features_sh_quant8.packed_ptr(),
            engine().world.features_sh_quant8.bounds_ptr(),
            cfg.lr_features_sh, step + 1, per_splat_steps,
            cfg.sh_reg_weight, 0.0f,
            cfg.sh_optim_bits, /*value_bits=*/8,
            grad_scale, zero_grad);
    } else if (cfg.sh_value_bits != 32
               && engine().world.features_sh_quant16.initialized()) {
        // 16-bit value + (4 or 8)-bit optim.
        fused_adam_step_quantized_value(
            N,
            (int64_t)N * (int64_t)engine().num_sh * 3LL,
            DeviceTensorFloatND(engine().grad.features_sh),
            sh_grad_q_packed, sh_grad_q_bounds,
            engine().optim.sh_quant_state.packed_ptr(),
            engine().optim.sh_quant_state.bounds_ptr(),
            engine().world.features_sh_quant16.packed_ptr(),
            engine().world.features_sh_quant16.bounds_ptr(),
            cfg.lr_features_sh, step + 1, per_splat_steps,
            cfg.sh_reg_weight, 0.0f,
            cfg.sh_optim_bits, /*value_bits=*/16,
            grad_scale, zero_grad);
    } else if (cfg.sh_optim_bits != 32 && engine().optim.sh_quant_state.initialized()) {
        // SH-quant (optim) only -- fp32 SH values. Plain quantized Adam kernel.
        // SH-quant + TR is not supported by fused_adamtr_*_sh_optim (those
        // kernels need fp32 g1/g2). Fall back to the plain quantized kernel.
        fused_adam_step_quantized(N,
            DeviceTensorFloatND(engine().world.features_sh),
            DeviceTensorFloatND(engine().grad.features_sh),
            engine().optim.sh_quant_state.packed_ptr(),
            engine().optim.sh_quant_state.bounds_ptr(),
            cfg.lr_features_sh, step + 1, per_splat_steps,
            cfg.sh_reg_weight, 0.0f,
            cfg.sh_optim_bits,
            grad_scale, zero_grad);
    } else if (cfg.use_color_trust_region) {
        const int s1 = step + 1;
        const float beta1 = 0.9f, beta2 = 0.999f, eps = 1e-15f;
        (cfg.color_is_linear ? fused_adamtr_linear_rgb_sh_optim : fused_adamtr_rgb_sh_optim)(
            _dt2d_tv(engine().world.features_sh),
            _dt2d_tv(engine().grad.features_sh),
            _dt2d_tv(engine().optim.g1_features_sh),
            _dt2d_tv(engine().optim.g2_features_sh),
            _dv_tv(engine().world.features_dc),
            _dv_tv(engine().world.opacities),
            cfg.lr_features_sh,
            beta1, beta2, eps, cfg.eps_tr, s1,
            grad_scale, zero_grad
        );
    } else {
        fused_adam_step(N,
            DeviceTensorFloatND(engine().world.features_sh),
            DeviceTensorFloatND(engine().grad.features_sh),
            DeviceTensorFloatND(engine().optim.g1_features_sh),
            DeviceTensorFloatND(engine().optim.g2_features_sh),
            cfg.lr_features_sh, step + 1, per_splat_steps,
            cfg.sh_reg_weight, 0.0f,
            grad_scale, zero_grad);
    }
}
