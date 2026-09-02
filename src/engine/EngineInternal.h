#pragma once

// EngineInternal -- declarations for engine-private helpers that cross file
// boundaries within the Engine.cpp split, plus extern declarations for raw
// device functions defined in other translation units (ImageConvert.cu,
// BilagridFusedAdam.cu, PpispInit.cu, BilagridInit.cu).
//
// Anything declared here is intentionally not part of the public Engine.h API.

#include "core/Tensor.h"
#include "kernels/loss/PerPixelLoss.cuh"   // LossIndex / LossValues / PerPixelGrads
#include "kernels/pixelwise/PixelWise.cuh"      // PPISPRegLossIndex

#include <array>
#include <cstdint>
#include <cstddef>


// --- Raw-pointer image conversion helpers (ImageConvert.cu). ---
void uint8_image_to_float_raw (const uint8_t*  d_in, float* d_out,
                               int B, int H, int W, int C);
void uint16_image_to_float_raw(const uint16_t* d_in, float* d_out,
                               int B, int H, int W, int C);
void uint8_normal_to_float_raw(const uint8_t*  d_in, float* d_out,
                               int B, int H, int W, int C);
void uint16_depth_to_float_raw(const uint16_t* d_in, float* d_out,
                               int B, int H, int W, int C);

// --- Fused bilagrid (image-grad + TV + Adam) kernel (BilagridFusedAdam.cu). ---
// quant_bits: 4 or 8 -- selects QuantizedAdamState<BITS, 256> codec when
// quantize=true. Ignored when quantize=false.
void fused_bilagrid_tv_adam(
    float*    grids,                    // null when value_quantize
    uint16_t* grids_q16,                // when value_quantize
    float2*   value_bounds,             // when value_quantize
    float*    g1_f,    float*  g2_f,
    uint8_t*  packed,                   // AoS packed (u, sqrt_g2) when quantize
    float4*   quant_bounds,
    const float* image_grad,
    const int*   cam_indices,
    int N_grids, int C_batch, int C,
    int L, int H, int W,
    float lr,
    float tv_weight,
    int32_t adam_step,
    bool quantize,
    int  quant_bits,
    bool value_quantize,
    backend::Stream stream
);

// --- One-shot encode of an fp32 bilagrid grid into 16-bit packed + bounds
// (BilagridFusedAdam.cu). Called once after init when value-quant is on;
// after this the fp32 buffer can be freed.
void bilagrid_encode_q16_launch(
    const float* grids,
    uint16_t* grids_q16,
    float2* value_bounds,
    int64_t total_cells,
    backend::Stream stream
);

// --- Color-shift regularizer for bilagrid + PPISP (ColorShiftReg.cu). ---
// Injects design (1) gradient on the POST-transforms v_render_rgb buffer and
// updates an on-device per-channel EMA of mean(sign(post - pre)). `pre` is
// the splat-side rendered RGB (input to whichever of bilagrid / PPISP runs
// first); `post` is the final post-both-transforms image. All on `stream`;
// no D->H readbacks.
void color_shift_reg_step(
    float* v_render_rgb,
    const float* post_rgb,
    const float* pre_rgb,
    float* shift_reg_ema,           // [3] device
    float* shift_reg_batch_sum,     // [3] device scratch
    int N_pixels,                   // B * H * W
    float weight,
    float beta,
    int64_t step,                   // # of EMA updates done BEFORE this call
    backend::Stream stream
);

// --- Fused bilagrid (image-grad + TV + AdaGrad) kernel (BilagridFusedAdam.cu). ---
// Same gradient pipeline as fused_bilagrid_tv_adam, but applies an AdaGrad
// update with lr_decay=0, weight_decay=0, initial_accumulator_value=0,
// eps=1e-15. The accumulator is float (one per cell) or, when quantize=true,
// log-encoded uint8 via QuantizedTensorLog<BITS, 256> with BITS selected by
// `quant_bits` (4 or 8). 4-bit serializes the byte-shared write between
// odd/even thread partners with a __syncthreads inside the kernel.
void fused_bilagrid_tv_adagrad(
    float*    grids,                    // null when value_quantize
    uint16_t* grids_q16,                // when value_quantize
    float2*   value_bounds,             // when value_quantize
    float*    accum_f,                  // when !quantize
    uint8_t*  packed,                   // when quantize (log-encoded |accum|)
    float2*   quant_bounds,             // when quantize, [n_blocks]
    const float* image_grad,
    const int*   cam_indices,
    int N_grids, int C_batch, int C,
    int L, int H, int W,
    float lr,
    float tv_weight,
    bool quantize,
    int  quant_bits,
    bool value_quantize,
    backend::Stream stream
);

// --- Bilagrid affine identity init (BilagridInit.cu). ---
void bilagrid_affine_identity_init(
    float* grids, int N, int L, int H, int W, backend::Stream stream);

// --- PPISP "original" default initializer (PpispInit.cu). ---
void ppisp_original_default_init(
    float* params, int N, backend::Stream stream);

// --- PPISP elementwise grad accumulation (PpispInit.cu). ---
void ppisp_add_into_grad(
    const float* src, float* dst, size_t n, backend::Stream stream);

// --- Scatter per-image scalars into a per-camera table (BilagridInit.cu). ---
void bilagrid_scatter_floats(
    const float* src, const int* indices, int n,
    float* dst, backend::Stream stream);


// --- Cross-section helpers within the Engine split. ---

// Stash cam_indices for the current step (used by both bilagrid and PPISP).
void _set_cur_cam_indices(TorchTensorView tv);

// Viewer thumbnail capture (no-op until engine_viewer_init() is called or
// once all post-cameras have been seen). Called from the fused train step
// right after _set_cur_cam_indices and after set_training_data{,_warped}
// has populated engine().gt.rgb; downsamples each post-camera slot's GT
// into the device-side thumbnail cache and marks the host seen-mask bit.
void engine_viewer_capture_thumbnails(TorchTensorView cam_indices_tv);

// Bilagrid: backward hook + state setup + TV readout. The hook overwrites
// v_render_rgb (post->pre bilagrid) when RGB bilagrid is enabled, and
// accumulates per-type sparse image grads.
void _engine_bilagrid_backward_hook(
    TorchTensorView v_render_rgb,
    TorchTensorView v_ref_depth,
    TorchTensorView v_ref_normal);
void _ensure_bilagrid_optim_state();
void _engine_bilagrid_tv_into(float* tv_buf3_device);

// Background blend: forward runs inside forward_3dgs, out of place, so viewer
// renders blend too. The backward hook adds v_T, rewrites v_render_rgb, and
// folds in overexposure_reg -- which needs the blend composite.
void _engine_background_forward();
void _engine_background_backward_hook(
    TorchTensorView v_render_rgb,
    TorchTensorView v_render_Ts,
    float overexposure_reg_weight);

// Color space, from forward_3dgs after the background blend: the forward
// encodes the render for display, the backward turns v_render_rgb back into
// working-space gradient before raster bwd. No-op when none is configured.
void _engine_color_space_forward();
void _engine_color_space_backward_hook(TorchTensorView v_render_rgb);
// Apply image-side conversion in-place on engine().gt.rgb. Called from
// set_training_data after the H->D upload.
void _engine_color_space_apply_to_gt();

// PPISP: backward hook + state setup + regularization-loss compute.
void _engine_ppisp_backward_hook(TorchTensorView v_render_rgb);
void _ensure_ppisp_optim_state();

// Force-allocate splat optimizer state to a given quant layout (checkpoint
// loader; no training step). Defined in EngineOptim.cpp.
void engine_ensure_optim_state(int sh_optim_bits, int sh_value_bits,
                               int non_sh_optim_bits,
                               bool use_per_splat_bias_correction);
// Returns a pool-backed [PPISPRegLossIndex::length] device buffer.
float* _engine_ppisp_reg_loss_into(
    const std::array<float, (int)PPISPRegLossIndex::length>& loss_weights,
    bool compute_grad);
