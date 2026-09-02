#pragma once

// EngineState -- all persistent device-resident state shared across the
// engine_* entrypoints. State is a singleton struct accessed via `engine()`.
// Sub-structs group related fields (world, camera, fwd, gt, grad, optim,
// bilagrid_rgb/depth/normal, ppisp). The singleton lives in EngineState.cpp.

#include "core/Tensor.h"

#include "kernels/background/BackgroundSphericalHarmonics.cuh"
#include "kernels/bilagrid/BilagridUtils.cuh"
#include "kernels/densify/Densify.cuh"
#include "kernels/optim/FusedProjectionBwdOptim.cuh"
#include "kernels/loss/FusedSSIM.cuh"
#include "kernels/tile/IntersectTile.cuh"
#include "kernels/optim/Optimizer.cuh"
#include "kernels/loss/PerPixelLoss.cuh"
#include "kernels/loss/PerSplatLoss.cuh"
#include "kernels/pixelwise/PixelWise.cuh"
#include "primitives/Primitive.cuh"

// Explicit primitive -> rendered channel set. The only place the engine encodes
// what each primitive renders; update here when adding a primitive (e.g. a
// normal-rendering one would return RGB_DN).
inline RenderOutputType engine_primitive_pixel_type(const std::string& primitive) {
    if (primitive == "3dgs" || primitive == "mip" || primitive == "3dgut")
        return RenderOutputType::RGB_D;
    throw std::runtime_error("engine_primitive_pixel_type: unknown primitive: " + primitive);
}

// Pick the distortion channel set from the per-channel weights, restricted to
// the channels the primitive actually renders. Depth is always included when
// any distortion is active (it is the base 2DGS term, and the DistortionType
// enum has no rgb-/normal-only forms). A nonzero normal weight on a primitive
// that doesn't render normal is ignored.
inline DistortionType engine_distortion_type(
    RenderOutputType pt, float rgb_w, float depth_w, float normal_w
) {
    const bool want_rgb    = rgb_w != 0.0f;  // rgb is always rendered
    const bool want_depth  = depth_w != 0.0f && RenderOutput::has_depth(pt);
    const bool want_normal = normal_w != 0.0f && RenderOutput::has_normal(pt);
    if (!(want_rgb || want_depth || want_normal))
        return DistortionType::None;
    if (want_rgb && want_normal) return DistortionType::RGB_DN;
    if (want_normal)             return DistortionType::DN;
    if (want_rgb)                return DistortionType::RGB_D;
    return DistortionType::D;
}
#include "kernels/projection/ProjectionBwd.cuh"
#include "kernels/projection/ProjectionFwd.cuh"
#include "kernels/projection/ProjectionPackedFwd.cuh"
#include "kernels/raster/RasterizationBwd.cuh"
#include "kernels/raster/RasterizationEval3DBwd.cuh"
#include "kernels/raster/RasterizationEval3DFwd.cuh"
#include "kernels/raster/RasterizationFwd.cuh"
#include "kernels/tile/SplatTileIntersector.cuh"
#include "kernels/visualize/Visualizer.cuh"

#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>


// Forward declaration so EngineState can hold a unique_ptr<DataManager>
// without dragging the (thread- / queue- / stb_image-heavy) DataManager.h
// into the dozens of TUs that include EngineState.h.
class DataManager;


// World splat parameters (allocated once at init; persistent on device).
//
// SH value quantization:
//   sh_value_bits == 32 -> canonical storage is `features_sh` (fp32 float3).
//   sh_value_bits in {8, 16} -> canonical storage is the packed buffer inside
//   `features_sh_quant`. Every consumer kernel decodes per-cell via the
//   QuantizedTensor<BITS, 256> codec; every writer (optim step, densify copy)
//   encodes + recomputes per-block bounds. `features_sh` is left empty in
//   that case so the device-side allocation tracks the canonical buffer.
struct WorldSplats {
    bool initialized = false;
    DeviceVector<float3>   means;
    DeviceVector<float4>   quats;
    DeviceVector<float3>   scales;
    DeviceVector<float>    opacities;
    DeviceVector<float3>   features_dc;
    DeviceTensor2D<float3> features_sh;   // [max_N, num_sh] when sh_value_bits == 32

    // Packed value quantization (sh_value_bits in {8, 16}). Two layouts mirror
    // the optim-state quant pattern (see SplatOptim::sh_quant_state*):
    //
    //   features_sh_quant{8,16}      -- per-CELL-block layout (block_size=256
    //     cells per bound). Used by the non-FPBO Adam path (fused_adam_step)
    //     and by forward/backward kernels via Primitive3DGS::project*.
    //
    //   features_sh_quant{8,16}_fpbo -- per-SPLAT-block layout (1 bound per 256
    //     splats covering all 3*K cells per splat). Used by FPBO's inline
    //     SH read+write: one FPBO block = 256 threads = 256 splats, so a single
    //     bound per block keeps the writeback reduction one-shot.
    //
    // Only one of (cell-block, fpbo) is allocated per training run, selected
    // by cfg.use_fused_proj_bwd_optim at _ensure_optim_state(). The unused
    // half stays as a default-constructed (empty) tensor.
    QuantizedTensor<8,  256> features_sh_quant8;
    QuantizedTensor<16, 256> features_sh_quant16;
    QuantizedTensor<8,  256> features_sh_quant8_fpbo;
    QuantizedTensor<16, 256> features_sh_quant16_fpbo;
    int  sh_value_bits = 32;
    bool sh_value_quantize_enabled() const { return sh_value_bits != 32; }
};

// Camera table (re-copied each frame into pool).
struct CameraTable {
    int32_t num    = 0;
    int32_t width  = 0;
    int32_t height = 0;
    CameraModelType model = (CameraModelType)-1;
    CameraDistortionType distortion = CameraDistortionType::None;
    std::string model_str;
    std::string distortion_str = "NONE";
    DeviceTensor2D<float4> viewmats;      // [C, 4]
    DeviceVector<float4>   intrins;       // [C]
    DeviceTensor2D<float>  dist_coeffs;   // [C, 8]
};

// Forward-pass intermediates retained for backward.
struct ForwardCache {
    DeviceVector<int32_t>             camera_ids;
    DeviceVector<int32_t>             gaussian_ids;
    DeviceTensor2D<uint2>             aabb;         // [nnz,1] or [C,N]; core/AabbQuant.cuh
    std::vector<DeviceTensorFloatND>  splats_w;
    std::vector<DeviceTensorFloatND>  splats_s;
    DeviceTensor3D<int32_t>           tile_offsets;
    DeviceVector<int32_t>             flatten_ids;
    // Binning granularity the forward chose (core/Common.cuh). The backward
    // must bin the same way, so it reads this rather than assuming a default.
    int                               macro_log2 = kMacroLog2Default;
    // [C, tile_h, tile_w] of 0/1: tiles no pixel of the loss reads are left
    // out of the intersections, so the raster gets an empty range for them.
    // Empty unless a training step asked for it (engine_set_tile_skip_mask).
    DeviceVector<int32_t>             tile_active;
    DeviceTensor3D<float>             render_Ts;
    DeviceTensor3D<float>             render_median; // [C,H,W] median depth, empty if not requested
    DeviceTensor3D<int32_t>           last_ids;
    RenderOutput::TensorTuple         renders;
    RenderOutput::TensorTuple         distortions;  // [C,H,W,...] D=W*S-C^2, only the dist_type channels allocated
    DistortionType                    dist_type = DistortionType::None;  // which distortion channels the forward emitted
    DeviceVector<float>               accum_weight; // [max_num_splats] per-splat score from raster bwd
    // What produced accum_weight, so densify folds and finalizes it the same
    // way regardless of which train-step path filled it.
    DensifyAccumMode                  accum_mode = DensifyAccumMode::None;
    // [max_num_splats] per-splat ||dL/dmean_world|| * max post-exp world
    // scale, written by the splat optim step (both FPBO and non-FPBO paths)
    // when OptimConfig::write_densify_world_grad_score is set. Consumed by
    // engine_densify_step as the blend partner of accum_weight
    // (DensifyConfig::score_blend_world_grad). Empty when disabled.
    DeviceVector<float>               world_grad_score;
    // Screen-space gradient handed from rasterize_*_bwd. The default path
    // consumes this immediately inside projection_*_backward; the fused
    // projection-bwd+optim path stashes it here for the optim step.
    std::vector<DeviceTensorFloatND>  v_splats_s;
};

// Training ground truth (re-copied each batch).
struct GTData {
    DeviceTensor3D<float3> rgb;
    DeviceTensor3D<float>  depth;
    DeviceTensor3D<float3> normal;
    DeviceTensor3D<bool>   alpha;
    bool has_gt   = false;
    bool has_mask = false;
};

// Splat parameter gradients (pool-backed, zeroed each backward).
//
// GRADIENT QUANTIZATION (non-FPBO path, quantization_level != 0):
//   When `quantize_grad` is set, the per-splat world-gradient accumulator is
//   stored block-wise quantized instead of the fp32 buffers above -- 16-bit
//   linear for the non-SH attributes and 8-bit linear for SH. Layout is
//   per-SPLAT-block (one float2 bound per 256 splats, mirroring the FPBO
//   Adam-state `_NonShQ` convention): n_bounds = ceil(N/256); the SH bound
//   covers all 3*K cells of its 256 splats. The projection-backward kernel
//   (ProjectionBwdQuantGrad) does decode -> accumulate -> block-reduce bounds
//   -> encode each sub-batch; the optim step decodes on read. For 3dgut,
//   means/quats/scales receive rasterization-backward atomicAdds and therefore
//   stay fp32 (only opacities/features_dc/features_sh are quantized); for
//   3dgs/mip all six are quantized and the fp32 buffers above are left empty.
struct SplatGrad {
    DeviceVector<float3>   means;
    DeviceVector<float4>   quats;
    DeviceVector<float3>   scales;
    DeviceVector<float>    opacities;
    DeviceVector<float3>   features_dc;
    DeviceTensor2D<float3> features_sh;

    // Block-wise quantized accumulators (populated only when quantize_grad).
    bool quantize_grad = false;
    QuantizedTensor<16, 256> means_q;         // 3 cells/splat
    QuantizedTensor<16, 256> quats_q;         // 4 cells/splat
    QuantizedTensor<16, 256> scales_q;        // 3 cells/splat
    QuantizedTensor<16, 256> opacities_q;     // 1 cell/splat
    QuantizedTensor<16, 256> features_dc_q;   // 3 cells/splat
    QuantizedTensor<8,  256> features_sh_q;   // 3*K cells/splat
};

// Adam moment + densify aux state (pool-backed, persistent across steps).
struct SplatOptim {
    bool initialized = false;
    // SH Adam-state quantization bit depth (32 = off, 4 or 8 = packed). When
    // off, g1_features_sh / g2_features_sh hold full fp32 moments. When 4/8,
    // sh_quant_state (or sh_quant_state_fpbo) holds the joint-AoS encoded
    // bytes + per-block bounds. Storage holders are typed `<8, 256>` so the
    // packed buffer is sized pessimistically (8-bit footprint, 2 bytes/cell)
    // regardless of sh_optim_bits; runtime dispatch picks the right codec.
    int  sh_optim_bits = 32;
    // Non-SH Adam-state quantization bit depth (means, quats, scales,
    // opacities, features_dc). 32 = off; 16 = QuantizedAdamState<16, 256>
    // per-attribute. FPBO-only. When 16, the fp32 g1_/g2_ buffers below are
    // empty and the canonical store is the `*_quant_state_fpbo` packed buffer.
    int  non_sh_optim_bits = 32;
    bool use_per_splat_bias_correction = false;

    DeviceVector<float3>   g1_means,         g2_means;
    DeviceVector<float4>   g1_quats,         g2_quats;
    DeviceVector<float3>   g1_scales,        g2_scales;
    DeviceVector<float>    g1_opacities,     g2_opacities;
    DeviceVector<float3>   g1_features_dc,   g2_features_dc;
    DeviceTensor2D<float3> g1_features_sh,   g2_features_sh;       // when sh_optim_bits == 32
    QuantizedAdamState<8, 256> sh_quant_state;                     // when sh_optim_bits in {4, 8}
    // FPBO uses a different per-bound granularity (one float4 per 256 splats,
    // covering all 3*K cells per splat), so the kernel cannot reuse
    // `sh_quant_state`. Allocated only when use_fused_proj_bwd_optim + sh_optim_bits!=32.
    QuantizedAdamState<8, 256> sh_quant_state_fpbo;

    // Non-SH Adam-state quantization (FPBO-only). One quant state per
    // attribute; each holds (cells = N * primitives_per_splat) packed bytes
    // and (bounds = ceil(N / kFpboBlock)) float4 bounds. Allocated only when
    // use_fused_proj_bwd_optim + non_sh_optim_bits != 32.
    QuantizedAdamState<16, 256> means_quant_state_fpbo;            // 3 prim/splat
    QuantizedAdamState<16, 256> quats_quant_state_fpbo;            // 4 prim/splat
    QuantizedAdamState<16, 256> scales_quant_state_fpbo;           // 3 prim/splat
    QuantizedAdamState<16, 256> opacities_quant_state_fpbo;        // 1 prim/splat
    QuantizedAdamState<16, 256> features_dc_quant_state_fpbo;      // 3 prim/splat

    // Convenience flag mirroring `sh_optim_bits != 32`.
    bool sh_quantize_enabled() const { return sh_optim_bits != 32; }
    bool non_sh_quantize_enabled() const { return non_sh_optim_bits != 32; }

    DeviceVector<float>    radii;                  // [max_N]
    DeviceVector<float2>   accum_buffer;           // [max_N]
    DeviceVector<int32_t>  bias_correction_steps;  // [max_N], or empty
    // accum_buffer with DensifyConfig::final_score_power applied to lane 0.
    // Empty when that power is 1, which is when accum_buffer IS the score.
    DeviceVector<float2>   densify_sample_score;   // [cur_N], or empty
    // Summed log2(radii / max_screen_size) over the steps since the last
    // refine, zeroed with accum_buffer. Empty when the oversize split
    // channel is off.
    DeviceVector<float>    densify_oversize;       // [max_N], or empty

    // Set per-step from cfg.optim.use_fused_proj_bwd_optim before forward/loss
    // so engine_compute_loss_backward knows to skip projection_*_backward and
    // stash v_splats_s for the subsequent fused projection-bwd-and-optim call.
    bool   use_fused_proj_bwd_optim = false;
    // Mirrors use_fused_proj_bwd_optim at the time _ensure_optim_state last
    // ran. Drives reallocation when the fused flag flips between steps (the
    // FPBO SH-quant layout differs from the regular FusedAppearanceOptim.cu layout).
    bool   fused_state_active       = false;

    // ---- sub-batched training state ----
    // When the engine is mid-train-step in sub-batched mode, the per-splat
    // grad buffers must accumulate atomicAdds across sub-batches; only the
    // FIRST sub-batch zeroes them. After all sub-batches finish, the optim
    // step consumes the accumulated grad with grad_scale = 1/B (and zeroes
    // it back as a fused side effect so the next train step starts fresh).
    //
    // Set by engine_train_step's sub-batch dispatcher around the
    // forward+loss_bwd calls; engine_compute_loss_backward reads
    // skip_grad_zero, engine_optim_step reads grad_scale + zero_grad.
    bool   skip_grad_zero = false;     // _alloc_grad_buffers skips zeroing
    float  grad_scale     = 1.0f;      // multiplied into v_* inside optim
    bool   zero_grad_in_optim = false; // optim zeroes v_* after consuming
};

// One bilagrid channel (RGB / depth / normal).
//
// Two mutually exclusive optimizer paths:
//   use_adagrad=false  -> Adam: g1/g2 (float) OR quant_state (uint8 AoS)
//   use_adagrad=true   -> AdaGrad: accum_f (float) OR adagrad_quant (uint8 log)
struct BilagridRGB {
    DeviceTensor5D<float>      grids;
    DeviceTensor5D<float>      image_grad;
    // Adam state
    DeviceTensor5D<float>      g1, g2;
    QuantizedAdamState<8, 256> quant_state;
    // AdaGrad state
    DeviceTensor5D<float>      accum_f;
    QuantizedTensorLog<8, 256> adagrad_quant;
    // Bit depth for optimizer-state quantization: 32 = off, 4 or 8 = packed.
    // Storage holders above are typed `<8, 256>` so allocation is pessimistic;
    // runtime dispatch in the kernel launcher picks the codec template.
    int         optim_bits         = 32;
    // Value (grid-parameter) quantization. 32 = fp32 grids (canonical), 16 =
    // 16-bit linear-quant packed buffer + per-256-cell float2 bounds (the
    // canonical store; `grids` is left empty). Samplers read through
    // BilagridReader which dispatches on the populated buffer.
    int         value_bits         = 32;
    QuantizedTensor<16, 256>   grids_quant;
    bool        use_adagrad        = false;
    bool quantize_optim() const { return optim_bits != 32; }
    bool quantize_value() const { return value_bits != 32; }
    bool        enabled            = false;
    bool        optim_initialized  = false;
    DeviceTensor3D<float3>     fwd_pre;            // pre-bilagrid render
    std::string type;
    int         C = 0;
};

// Color-shift regularizer (design 1): EMA of mean_per_pixel(sign(post-pre))
// per channel, all on device. `steps` counts EMA updates so far (host int);
// the device-side EMA is not bias-corrected itself -- ColorShiftReg.cu applies
// (1 - beta^t) in the inject kernel's coefficient. `cur_weight` / `cur_beta`
// are stashed per step from LossConfig and read by the loss bwd just before
// the bilagrid / PPISP bwd hooks consume v_render_rgb. The state is shared
// across both transforms because the regularizer penalizes their composed
// effect, not either one in isolation.
struct ColorShiftRegState {
    DeviceVector<float> ema;        // [3]
    DeviceVector<float> batch_sum;  // [3] scratch (atomic-accumulated)
    int64_t steps        = 0;
    bool    initialized  = false;
    float   cur_weight   = 0.0f;
    float   cur_beta     = 0.0f;
};
struct BilagridDepth {
    DeviceTensor5D<float>      grids;
    DeviceTensor5D<float>      image_grad;
    DeviceTensor5D<float>      g1, g2;
    QuantizedAdamState<8, 256> quant_state;
    DeviceTensor5D<float>      accum_f;
    QuantizedTensorLog<8, 256> adagrad_quant;
    int         optim_bits         = 32;
    int         value_bits         = 32;
    QuantizedTensor<16, 256>   grids_quant;
    bool        use_adagrad        = false;
    bool        enabled            = false;
    bool        optim_initialized  = false;
    DeviceTensor3D<float>      fwd_pre;
    DeviceVector<float>        scalars;            // per-camera median quantile
    bool quantize_optim() const { return optim_bits != 32; }
    bool quantize_value() const { return value_bits != 32; }
};
struct BilagridNormal {
    DeviceTensor5D<float>      grids;
    DeviceTensor5D<float>      image_grad;
    DeviceTensor5D<float>      g1, g2;
    QuantizedAdamState<8, 256> quant_state;
    DeviceTensor5D<float>      accum_f;
    QuantizedTensorLog<8, 256> adagrad_quant;
    int         optim_bits         = 32;
    int         value_bits         = 32;
    QuantizedTensor<16, 256>   grids_quant;
    bool        use_adagrad        = false;
    bool        enabled            = false;
    bool        optim_initialized  = false;
    DeviceTensor3D<float3>     fwd_pre;
    bool quantize_optim() const { return optim_bits != 32; }
    bool quantize_value() const { return value_bits != 32; }
};

// Background blending. Applied BEFORE bilagrid/PPISP. Two modes:
//   - Noise: random per-pixel color (warmup-weighted). No persistent state.
//   - Sh:    skybox = SH(world ray dir) + DC color. DC + L1+ coeffs trained.
struct EngineBackground {
    enum class Mode { None = 0, Noise = 1, Sh = 2 };
    Mode mode    = Mode::None;
    bool enabled = false;

    // Common config (set at init time)
    int  splat_transfer = 0;             // noise mode: display -> working space
    bool splat_is_linear = false;

    // SH mode config
    int  sh_degree       = 0;            // 0..4

    // SH parameters: index 0 is DC (the "background_color"), 1..(deg+1)^2-1 are
    // higher-order SH bands. Stored once on device, persistent across steps.
    DeviceVector<float3> sh_coeffs;

    // SH Adam moments (allocated lazily on first optim_step when train_sh_color).
    DeviceVector<float3> sh_g1, sh_g2;
    bool sh_optim_initialized = false;

    // Per-iter, resized each forward. fwd_pre_blend_rgb is kept in BOTH modes
    // (the backward reads it); fwd_background is the skybox image (Sh mode).
    DeviceTensor3D<float3> fwd_pre_blend_rgb;
    DeviceTensor3D<float3> fwd_background;

    // Stashed at forward time so backward can reconstruct the random skybox
    // (Noise mode) and so the optim step can run after backward (both modes).
    uint32_t cur_seed             = 0;
    float    cur_randomize_weight = 0.0f;
};

// Linear / wide-gamut color space conversion.
// Splat side: pred RGB (rendered in working color space) is converted to sRGB
// before loss; the loss-side gradient is converted back through the vjp.
// Image side: GT RGB is converted to sRGB once at upload time and kept that way.
struct ColorSpaceState {
    // Splat (per-frame fwd + bwd)
    bool                   splat_enabled    = false;
    int                    splat_transfer   = 0;   // colorspace::Transfer
    bool                   splat_is_linear  = false;
    DeviceTensor2D<float3> splat_color_matrix;   // [3, 3], stored as 3 float3 rows

    // Image (one-shot at upload)
    bool                   image_enabled    = false;
    int                    image_transfer   = 0;
    bool                   image_is_linear  = false;
    DeviceTensor2D<float3> image_color_matrix;   // [3, 3], stored as 3 float3 rows

    // Per-iter scratch: pre-conversion render kept for the backward vjp
    // (working_to_display_backward consumes the working-space input).
    DeviceTensor3D<float3> fwd_pre;
};

// PPISP (RGB-only photometric correction; applied AFTER bilagrid).
//
// Optimizer selection (set at engine_init_ppisp):
//   use_adagrad=false -> Adam: g1 (1st moment) + g2 (2nd moment), both fp32.
//   use_adagrad=true  -> unscheduled AdaGrad: accum_f (squared-grad sum), fp32.
// No quantization path for PPISP (parameter table is small: N_cam x ~36 fp).
struct PpispState {
    DeviceTensor2D<float>  params;
    DeviceTensor2D<float>  grads;
    DeviceTensor2D<float>  g1, g2;        // Adam (use_adagrad=false)
    DeviceTensor2D<float>  accum_f;       // AdaGrad (use_adagrad=true)
    DeviceTensor3D<float3> fwd_pre;
    std::string param_type;
    int  num_params         = 0;
    bool enabled            = false;
    bool optim_initialized  = false;
    bool use_adagrad        = false;
    // Per-iteration: mirrors PpispStepConfig::run_before_bilagrid for the
    // current step. The forward path stashes this before launching bilagrid /
    // PPISP forwards; the backward hooks in EngineLoss.cpp read it back to
    // invert the order. Reset each step.
    bool cur_run_before_bilagrid = false;
};


// Compile-time thumbnail resolution. Square RGBA uint8; stored in
// device-side cache populated during training data ingestion (set_training_data
// / set_training_data_warped) and consumed by the viewer's blit kernel.
#ifndef VIEWER_THUMBNAIL_SIZE
#define VIEWER_THUMBNAIL_SIZE 64
#endif


// Viewer state: device-side per-post-camera thumbnail cache, BVH cache for
// the training-camera frustum visualization, and a copy of the static
// dataset-wide camera arrays (intrins / dist_coeffs + tier / c2w / model /
// W / H).
// All allocated lazily on engine_viewer_init(); kernels gated on
// `initialized` so the training loop pays zero cost until the viewer asks.
struct EngineViewerState {
    bool   initialized = false;
    int    N_post      = 0;   // total post-split training cameras

    // Per-post-camera dataset arrays (uploaded once at init).
    DeviceVector<float>   d_intrins;          // [N_post, 4]
    DeviceVector<int32_t> d_widths;           // [N_post]
    DeviceVector<int32_t> d_heights;          // [N_post]
    DeviceVector<int32_t> d_camera_models;    // [N_post]
    DeviceVector<float>   d_dist_coeffs;      // [N_post, 8]
    DeviceVector<int32_t> d_distortions;      // [N_post] CameraDistortionType
    DeviceVector<float>   d_camera_to_worlds; // [N_post, 3, 4] (y/z-flipped form)
    float  camera_size = 0.0f;                // frustum render scale, from knn-dist

    // Thumbnail cache: [N_post, S, S, 4] uint8, S = VIEWER_THUMBNAIL_SIZE.
    // done_mask[i] = 1 once cam i's thumbnail has been written. Host fast-path
    // counter `pending_thumb` is decremented as new ids first appear in
    // training batches; once 0, the hook short-circuits.
    DeviceVector<uint8_t> thumbnails;         // [N_post * S * S * 4]
    DeviceVector<uint8_t> thumbnail_done_mask;// [N_post]
    std::vector<uint8_t>  host_seen_mask;     // [N_post], 1 if id was ever passed
    int    pending_thumb = 0;                 // N_post - sum(host_seen_mask)

    // Overlay line segments (axes / grid): world-frame capsules appended
    // after the camera frusta in the LSS buffer at BVH build, with
    // per-segment colors (frusta keep their adaptive black/white).
    // Uploaded via engine_viewer_set_overlay; gated per request by
    // engine_blit_view's show_overlay.
    DeviceVector<float> d_overlay_segs;    // [M, 2, 4] endpoint xyzr pairs
    DeviceVector<float> d_overlay_colors;  // [M, 3]
    int    num_overlay = 0;
    float  grid_radius = 0.0f;             // scene radius (engine frame)
    float  grid_spacing = 0.0f;            // minor cell size the overlay was built at
    int    grid_half = 0;                  // half-extent (in minor cells) ditto
    float  grid_center[2] = {0, 0};        // patch center (lattice-snapped) ditto

    // BVH cache for training-camera + overlay visualization. Built on the
    // first engine_blit_view call that needs it; rebuilt when camera_size
    // or the overlay changes (training cameras themselves are static).
    bool   bvh_built = false;
    float  bvh_camera_size = -1.0f;        // camera_size the BVH was built at
    int    bvh_num_cam_lss = 0;            // frustum capsules before overlay
    DeviceVector<float>   bvh_lss_buffer;     // [num_lss * 2 * 4]
    DeviceVector<float>   bvh_tri_buffer;     // [num_tri * 4 * 4]
    DeviceVector<int32_t> bvh_lss_nodes;      // [num_lss * 2]  (int2)
    DeviceVector<float>   bvh_lss_aabb;       // [num_lss * 2 * 3] (float3)
    DeviceVector<int32_t> bvh_tri_nodes;      // [num_tri * 2]
    DeviceVector<float>   bvh_tri_aabb;       // [num_tri * 2 * 3]
    int    bvh_num_lss = 0;
    int    bvh_num_tri = 0;

    // Raw pointers to the BVH nodes/aabb that build_bvh() actually allocated.
    // build_bvh() stores them in the string-keyed *dynamic* pool; engine_blit_view()
    // must read that SAME allocation. Re-acquiring by a static PoolSlot enum returns
    // a different, uninitialized buffer -- the string->enum pool-key refactor
    // (7632e51) made the two diverge and silently broke "show training cameras".
    const void* bvh_lss_nodes_ptr = nullptr;  // int2*
    const void* bvh_lss_aabb_ptr  = nullptr;  // float3*
    const void* bvh_tri_nodes_ptr = nullptr;  // int2*
    const void* bvh_tri_aabb_ptr  = nullptr;  // float3*
};


struct EngineState {
    // Top-level scalars + flags shared across sections.
    std::string primitive;
    int64_t cur_num_splats = 0;
    int64_t max_num_splats = 0;
    int     num_sh         = 0;
    int     sh_degree      = 0;
    bool    packed         = false;
    // Binning tile edge in pixels the caller asked for, or 0 to let the
    // forward choose from the measured splat footprint (engine_set_bin_tile_size).
    int     bin_tile_request = 0;
    // Keyed by (width << 32 | height): a viewport render at another
    // resolution measures another footprint and must not retune training's.
    // Search: docs/notes/binning-tile-size.md.
    struct BinTileChoice {
        int macro_log2 = kMacroLog2Start;
        int floor      = kMacroLog2Min;  // coarsest the intersect fell back to
        int from       = kMacroLog2Start;  // setting a running probe came from
        int probing    = 0;                // 0 base, 1 trial, 2 base again
        int dir        = -1;               // -1 finer, +1 coarser
        // Steps discarded before the first baseline: image decode and
        // first-touch allocation make the opening steps unrepresentative,
        // and they would otherwise inflate the cost of the starting size.
        int warmup     = 8;
        int have       = 0;
        int wait       = 1;                // windows before the next probe
        double sum     = 0.0;
        double base    = 0.0;              // bracketing cost at `from`
        double trial   = 0.0;              // cost measured at `from + dir`
    };
    std::map<uint64_t, BinTileChoice> bin_tile_auto;
    // PoolSlot::EngVLosses (the per-pixel loss cotangent seed) is uploaded
    // once. Not a function-local static: engine_reset() frees the pool, and a
    // flag outliving it leaves the next scene reading the reallocated buffer.
    bool    v_losses_uploaded = false;

    WorldSplats    world;
    CameraTable    camera;
    ForwardCache   fwd;
    GTData         gt;
    SplatGrad      grad;
    SplatOptim     optim;

    BilagridRGB    bilagrid_rgb;
    BilagridDepth  bilagrid_depth;
    BilagridNormal bilagrid_normal;
    // Per-image-in-batch camera indices for the current step (shared by
    // background, bilagrid, and PPISP). Empty -> kernels fall back to identity.
    DeviceVector<int32_t> bilagrid_cur_cam_indices;

    EngineBackground background;
    PpispState       ppisp;
    ColorSpaceState  color_space;
    ColorShiftRegState color_shift_reg;

    // Viewer (BVH + thumbnail cache + dataset camera arrays).
    EngineViewerState viewer;

    // Host-side dataset orchestrator (RGB / mask / depth / normal decode +
    // batching). Set by engine_setup_data_manager(); when present, the new
    // engine_train_step_managed() entrypoint pulls per-step inputs from it.
    std::unique_ptr<DataManager> dm;

    // Out-of-line ctor/dtor (defined in EngineState.cpp) so the
    // std::unique_ptr<DataManager> deleter only needs the complete type at
    // one site. Move ops are defaulted so engine_reset() can assign a fresh
    // EngineState{} (rvalue); copy ops are deleted by the unique_ptr member.
    EngineState();
    ~EngineState();
    EngineState(EngineState&&) noexcept;
    EngineState& operator=(EngineState&&) noexcept;
};


// Function-local-static singleton. Future evolution: a second instance for
// eval / viewer / multi-model can be added by swapping the accessor's body.
EngineState& engine();
