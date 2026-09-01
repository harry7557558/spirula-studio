#pragma once

#include "core/Tensor.h"

#include "kernels/tile/IntersectTile.cuh"
#include "kernels/background/BackgroundSphericalHarmonics.cuh"
#include "kernels/loss/PerSplatLoss.cuh"
#include "kernels/loss/PerPixelLoss.cuh"
#include "kernels/pixelwise/PixelWise.cuh"
#include "kernels/loss/FusedSSIM.cuh"
#include "kernels/tile/SplatTileIntersector.cuh"
#include "kernels/projection/ProjectionFwd.cuh"
#include "kernels/projection/ProjectionBwd.cuh"
#include "kernels/projection/ProjectionPackedFwd.cuh"
#include "kernels/raster/RasterizationFwd.cuh"
#include "kernels/raster/RasterizationBwd.cuh"
#include "kernels/raster/RasterizationEval3DFwd.cuh"
#include "kernels/raster/RasterizationEval3DBwd.cuh"
#include "kernels/optim/Optimizer.cuh"
#include "kernels/optim/FusedProjectionBwdOptim.cuh"
#include "kernels/densify/Densify.cuh"
#include "kernels/bilagrid/BilagridUtils.cuh"
#include "kernels/visualize/Visualizer.cuh"

#include "data/DataManager.h"
#include "engine/EngineConfig.h"

#include <map>
#include <string>
#include <vector>


// ============================================================
// Engine API -- hand-written, unlike backend/api/
// ============================================================

// --- Lifecycle ---
//
// Reset the EngineState singleton to a freshly-constructed state and free all
// device memory owned by the global pool + scratch buffer. Must be called
// between training runs that swap datasets (a benchmark looping over scenes)
// -- without it the new run inherits the previous run's world splats,
// camera table, bilagrid / PPISP / background config, optimizer moments, and
// color-space matrices, which produces broken renders and wrong metrics.
void engine_reset();

// --- Setup ---

void set_data_3dgs(
    int64_t num_splats,
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
);

void set_camera_params(
    int width,
    int height,
    std::string camera_model,
    std::string distortion,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs
);

void set_training_data(
    // RGB: float32 [0,1], uint8 [0,255], or uint16 [0,65535]. uint8/uint16
    // uploaded as raw bytes; converted to float on the GPU.
    TorchTensorView gt_rgb,          // [C, H, W, 3]
    // Depth: float32 (passthrough) or uint16 (cast on GPU, no scaling). Null OK.
    TorchTensorView gt_depth,        // [C, H, W, 1]
    // Normal: float32 (passthrough) or uint8 (x/127.5 - 1 on GPU). Null OK.
    TorchTensorView gt_normal,       // [C, H, W, 3]
    // External mask (per-pixel bool/uint8). Sets Buffers::has_mask. Null OK.
    // Drives the RGB mask + alpha-sup target inside per_pixel_losses.slang.
    TorchTensorView gt_alpha,        // [C, H, W, 1]
    // When false, the uploaded GT depth is treated as linear (z) depth and
    // converted to ray depth in place (the rasterizer renders ray depth).
    // True = the depth map already stores ray depth, no conversion.
    bool input_depth_is_ray_depth = true
);


// Warp-fused GT upload for a fisheye / equirectangular -> pinhole split: the
// byte GT is warped on GPU straight into post-split float buffers, through
// the camera table set_camera_params installed (call it FIRST).
void set_training_data_warped(
    std::string  input_model_name,    // "FISHEYE" / "EQUISOLID" / "EQUIRECTANGULAR"
    std::string  input_distortion,
    int B_in, int in_H, int in_W,
    int K, int out_H, int out_W,
    TorchTensorView gt_rgb,           // [B_in, in_H, in_W, 3] byte
    TorchTensorView gt_alpha,         // [B_in, mask_in_H, mask_in_W, 1] uint8 (nullable)
    int mask_in_H, int mask_in_W,
    TorchTensorView gt_depth,         // [B_in, depth_in_H, depth_in_W, 1] u16/f32 (nullable)
    int depth_in_H, int depth_in_W,
    TorchTensorView gt_normal,        // [B_in, normal_in_H, normal_in_W, 3] u8/f32 (nullable)
    int normal_in_H, int normal_in_W,
    bool input_depth_is_ray_depth,    // interpretation of the wide GT depth
    TorchTensorView input_intrins,    // [B_in, 4] float
    TorchTensorView input_dist_coeffs,// [B_in, 8] float
    TorchTensorView input_source_models,  // [B_in] int32 (nullable)
    TorchTensorView input_source_params,  // [B_in, 16] float
    TorchTensorView face_axes         // [B_in*K, 3, 3] float (host or device)
);

// --- Forward ---

// Binning tile edge in pixels for subsequent forwards: a power of two in
// [8, 128], or 0 to choose from the splat footprint each forward measures.
// Sticky; throws on a size that is not a supported power of two.
void engine_set_bin_tile_size(int pixels);

// One training step's fwd+bwd wall time, which the automatic binning search
// compares between granularities. Training only -- a viewport render must not
// feed it. No-op when a size is forced.
void engine_bin_tile_observe(double step_seconds);

// Binning granularity the next forward at the current camera size will use.
// Anything sized against the tile grid before the forward -- the live-tile
// map -- has to ask, or it builds a grid the intersect then indexes wrongly.
int engine_bin_macro_log2();

void forward_3dgs(
    std::string primitive,
    int sh_degree,
    bool packed,
    bool output_median = false,
    int dist_type = 0  // DistortionType (0=None,1=D,2=RGB_D,3=DN,4=RGB_DN)
);

// Hand back the per-splat SCREEN buffers the last forward filled (the
// projection's output layout). They are re-acquired, correctly shaped, by the
// next forward_3dgs -- so this is safe at any point between renders, and it
// is what makes switching primitive in a viewer cost the LARGER of the two
// layouts rather than their union: the layouts share pool slot names but not
// shapes, and the pool only grows.
//
// Not for the training loop: there the shape never changes, and handing the
// buffers back every iteration would trade a free reallocation for a real one.
void engine_release_screen_buffers();

// --- Loss + backward (combined) ---

std::map<std::string, float> engine_compute_loss_backward(
    int step,
    // loss weights
    std::array<float, (int)LossWeightIndex::length> loss_weights,
    float w_ssim,
    int num_loss_scales,
    // When positive, overrides num_loss_scales based on the render resolution
    // (see LossConfig::loss_scale_min_pixels). Zero leaves it untouched.
    int loss_scale_min_pixels,
    bool compute_loss_map,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    // Per-side survival factor for the *_nms modes (see LossConfig).
    float nms_falloff,
    // In-place post-processing of the finished loss map (see LossConfig).
    bool loss_map_normalize,
    float loss_map_clip_quantile,
    float loss_map_power,
    // DensifyAccumMode as an int (see LossConfig::loss_map_accum_mode).
    int loss_map_accum_mode,
    // Image-space overexposure regularization weight (see LossConfig).
    // Zero (default) disables the kernel launch entirely.
    float overexposure_reg_weight = 0.0f,
    // Color-shift regularizer for combined bilagrid + PPISP (see LossConfig).
    // Zero (default) disables the kernel launch entirely.
    float color_shift_reg_weight  = 0.0f,
    float color_shift_reg_beta    = 0.0f
);

// The densification error map for the last forward -- the buffer
// engine_compute_loss_backward hands the raster backward -- into a host
// [C, H, W, 1]. Touches no gradient, optimizer or colour-transform state.
bool engine_preview_loss_map(const LossConfig& loss, TorchTensorView out);

// --- Backward from supplied output cotangents (no loss) ---
//
// Seeds the rasterization backward with caller-supplied per-pixel cotangents
// instead of a loss gradient, then runs raster + projection backward.
// Per-splat grads land in engine().grad.* (read with
// engine_copy_grads_to_host). The seeds may be host or device pointers
// (backend::MemcpyKind::Auto auto-detects). forward_3dgs must have run
// first. The 3dgs / mip path does not accumulate a viewmats gradient
// (projection bwd is called with v_viewmats = null).
void engine_backward_from_render_grad(
    TorchTensorView v_render_rgb,    // [C, H, W, 3] float32
    TorchTensorView v_render_depth,  // [C, H, W, 1] float32
    TorchTensorView v_render_Ts      // [C, H, W, 1] float32
);

// --- Optimizer step ---

void engine_optim_step(int step, const OptimConfig& cfg);

// --- Bilagrid (RGB / depth / normal) ---
// Allocates and identity-initializes a bilagrid per camera. Must be called
// after set_camera_params. RGB applies to the rendered prediction; depth and
// normal apply to GT.

// optim_bits: bit depth for the optimizer-state quantization. 32 = no quant
// (full fp32 g1/g2 for Adam; fp32 accum for AdaGrad). 4 or 8 = packed
// QuantizedAdamState<BITS, 256> for Adam OR QuantizedTensorLog<BITS, 256>
// for AdaGrad. Configurable per-type so RGB and geometry can be quantized
// independently.
//
// use_adagrad: if true, the bilagrid uses AdaGrad (lr_decay=0,
// weight_decay=0, initial_accumulator_value=0, eps=1e-15) instead of Adam.
void engine_init_bilagrid_rgb(int n_grids, std::string type, int L, int H, int W,
                              int optim_bits, int value_bits, bool use_adagrad);
void engine_init_bilagrid_depth(int n_grids, int L, int H, int W,
                                int optim_bits, int value_bits, bool use_adagrad);
void engine_init_bilagrid_normal(int n_grids, int L, int H, int W,
                                 int optim_bits, int value_bits, bool use_adagrad);

// Apply forward bilagrid for the current batch. cam_indices is a [C_batch]
// int32 tensor of per-image camera-table indices; pass null/empty for identity
// (image i uses grid slot i). Must be called between forward_3dgs and
// engine_compute_loss_backward.
void engine_bilagrid_forward(TorchTensorView cam_indices);

// Adam step + optional TV-loss regularization for each enabled bilagrid type.
void engine_bilagrid_optim_step(int step, const BilagridStepConfig& cfg);

// --- PPISP (RGB only, applied AFTER bilagrid). ---
// Table seeded with the param_type's defaults; exposure_init optionally seeds
// params[:, 0] with [n_grids] log2 gains. use_adagrad: AdaGrad over Adam.
void engine_init_ppisp(int n_grids, std::string param_type, bool use_adagrad,
                       const std::vector<float>& exposure_init = {});

// Apply PPISP forward in place on the current rendered RGB; saves a pre-PPISP
// copy used by backward. cam_indices: [C_batch] int32, or null/empty for
// identity. Must be called after forward_3dgs (and after engine_bilagrid_forward
// when bilagrid is also enabled).
void engine_ppisp_forward(TorchTensorView cam_indices);

// Optimizer step over the PPISP parameter table (Adam or AdaGrad depending on
// use_adagrad at init). Also folds in the 6 PPISP regularization-loss gradients
// (using the provided per-loss weights) before the update. Pass loss weights
// in PPISPRegLossIndex order.
void engine_ppisp_optim_step(int step, const PpispStepConfig& cfg);

// --- Background blending (applied BEFORE bilagrid/PPISP) ---
//
// Two modes -- exactly one of these init calls activates background blending:
//   Noise: random per-pixel color, warmup-weighted via cfg.background.randomize_weight.
//   SH:    skybox = SH(world ray dir), DC (slot 0) seeds the "background_color"
//          plus higher-order bands; both updated by Adam when train_color=true.
//
// dc_color is the linear-space DC color used at SH init time (set slot 0).
void engine_init_background_noise(bool splat_color_is_linear);
void engine_init_background_sh(
    int sh_degree, bool splat_color_is_linear);

// Per-iter (seed, randomize_weight) for the next forward_3dgs background blend.
// Training calls this each step; the viewer/eval path can ignore it and reuse
// the last-stashed values (the engine defaults of 0/0 produce a uniform
// half-gray noise blend, avoiding per-frame flicker in noise-mode viewer renders).
void engine_set_background_step_params(uint32_t seed, float randomize_weight);

// Adam step over the SH coefficient table. No-op for Noise mode or when SH
// training was not enabled at init.
void engine_background_optim_step(int step, const BackgroundStepConfig& cfg);

// Copy the engine's background image for the current camera setup to a host
// (..., H, W, 3) float buffer. SH mode: returns the skybox rendered by the
// most recent forward_3dgs. Noise mode: returns a uniform mean-color image.
// Returns 1 on success, 0 if no engine background is active.
int engine_copy_background_to_host(TorchTensorView out_image);

// --- Linear / wide-gamut color space ---
//
// Configure linear / wide-gamut handling for both the splat (predicted) RGB
// and the GT image color space. When enabled, the splat-side conversion
// (linear or wide-gamut -> sRGB) runs at the end of every forward, and is
// inverted through the vjp on the loss-side gradient before raster bwd. The
// image-side conversion (same direction) runs once per upload in
// set_training_data. Pass row-major 3x3 source->Rec.709 matrices, or empty
// matrices when the corresponding side is not enabled.
void engine_init_color_space(
    bool splat_enabled,
    bool splat_is_linear,
    std::vector<float> splat_color_matrix,    // [9], row-major
    bool image_enabled,
    bool image_is_linear,
    std::vector<float> image_color_matrix     // [9], row-major
);

// --- Densification step ---

// Returns: number of splats added (0 if no densification this step)
int engine_densify_step(int step, int max_steps, const DensifyConfig& cfg);

// --- Fused training step (set_camera + set_training + forward + loss/bwd + optim + densify) ---
//
// Bilagrid + PPISP entries in `cfg` are no-ops when the corresponding
// engine_init_* was never called. `bilagrid_cam_indices` is a [C_batch] int32
// tensor for per-image grid-slot lookup (null/empty -> identity).
std::map<std::string, float> engine_train_step(
    int step, int max_steps,
    std::string primitive,
    int sh_degree,
    bool packed,
    int width, int height, std::string camera_model, std::string distortion,
    TorchTensorView viewmats,
    TorchTensorView intrins,
    TorchTensorView dist_coeffs,
    TorchTensorView gt_rgb,
    TorchTensorView gt_depth,
    TorchTensorView gt_normal,
    TorchTensorView gt_alpha,
    TorchTensorView bilagrid_cam_indices,
    const EngineStepConfig& cfg
);


// Warp-fused fused train step. Used by engine_train_step_managed when a batch
// needs fisheye/equirectangular -> pinhole splitting. Camera table is set up
// at POST-split (B*K) PINHOLE; GT is byte-warped on GPU into a float
// post-split buffer with no intermediate full-res float image.
std::map<std::string, float> engine_train_step_warped(
    int step, int max_steps,
    std::string primitive, int sh_degree, bool packed,
    int out_W, int out_H,
    TorchTensorView post_viewmats,
    TorchTensorView post_intrins,
    TorchTensorView post_dist_coeffs,
    std::string input_camera_model,
    std::string input_distortion,
    int B_in, int in_H, int in_W, int K,
    TorchTensorView input_intrins,
    TorchTensorView input_dist_coeffs,
    TorchTensorView input_source_models,
    TorchTensorView input_source_params,
    TorchTensorView gt_rgb_byte,
    TorchTensorView gt_alpha_byte,
    int mask_in_H, int mask_in_W,
    TorchTensorView gt_depth_byte,
    int depth_in_H, int depth_in_W,
    TorchTensorView gt_normal_byte,
    int normal_in_H, int normal_in_W,
    TorchTensorView face_axes,
    // Runs of equal-size faces (DecodedBatch::face_passes). Empty or a single
    // entry renders every face in one pass at out_W x out_H; more than one
    // needs the accumulating path, so the fused optimizer must be off.
    const std::vector<WarpFacePass>& face_passes,
    TorchTensorView bilagrid_cam_indices,
    const EngineStepConfig& cfg
);


// One homogeneous, non-warp sub-batch of a heterogeneous training step: all its
// cameras share (width, height, camera_model, distortion). The views point at
// host buffers that must outlive the whole engine_train_step_hetero call.
struct HeteroSubBatch {
    int width  = 0;
    int height = 0;
    int num    = 0;                       // number of cameras in this sub-batch
    std::string     camera_model;
    std::string     distortion;
    TorchTensorView viewmats{0, 0, {}};
    TorchTensorView intrins{0, 0, {}};
    TorchTensorView dist_coeffs{0, 0, {}};
    TorchTensorView gt_rgb{0, 0, {}};
    TorchTensorView gt_depth{0, 0, {}};
    TorchTensorView gt_normal{0, 0, {}};
    TorchTensorView gt_alpha{0, 0, {}};
    TorchTensorView bilagrid_cam_indices{0, 0, {}};
};


// Heterogeneous (mixed-resolution) training step: one optimizer step whose
// gradient is accumulated over `subs` sub-batches of differing (W, H, model),
// then applied in a single optim + densify pass with grad_scale = 1 / (total
// cameras). Requires the non-fused (split_batch) path -- FPBO folds grads into
// the optim kernel and cannot span sub-batches. Used by
// engine_train_step_managed for cross-group-packed steps. `subs` must contain
// at least one entry (callers use the plain engine_train_step for a single
// sub-batch).
std::map<std::string, float> engine_train_step_hetero(
    int step, int max_steps,
    std::string primitive,
    int sh_degree,
    bool packed,
    const std::vector<HeteroSubBatch>& subs,
    const EngineStepConfig& cfg
);


// --- DataManager-driven training step --------------------------------------
//
// Two-call setup + fused per-step pull. `engine_setup_data_manager` installs
// (or replaces) the engine's DataManager from a parsed dataset; subsequent
// `engine_train_step_managed(...)` calls pull the next training batch
// internally and run the same fused forward + loss/bwd + optim + densify
// pipeline as `engine_train_step`.
//
// This is the "engine handles data loading" entrypoint: the caller does not
// construct per-step GT/camera tensors.

void engine_setup_data_manager(
    DataManagerConfig         cfg,
    // Per-camera model enum value (int; cast to CameraModelType internally).
    // Mixed pinhole + fisheye datasets just produce extra index groups.
    std::vector<int32_t>      camera_models,
    std::vector<int32_t>      camera_distortions,
    std::vector<std::string>  image_filenames,
    std::vector<std::string>  mask_filenames,
    std::vector<std::string>  depth_filenames,
    std::vector<std::string>  normal_filenames,
    std::vector<int32_t>      widths,
    std::vector<int32_t>      heights,
    // Per-input split factor + exclusive-prefix-sum offset into the
    // POST-split camera arrays below. Pass empty vectors to skip the warp
    // path entirely (K defaults to 1 per camera).
    std::vector<int32_t>      K_per_camera,
    std::vector<int32_t>      post_offsets,
    // POST-split camera params (length sum(K_per_camera) along the first dim).
    std::vector<float>        viewmats,
    std::vector<float>        intrins,
    std::vector<float>        dist_coeffs,
    // POST-split face size and frame (PostSplitCameras::post_widths /
    // post_heights / face_axes); empty when the warp path is off.
    std::vector<int32_t>      post_widths,
    std::vector<int32_t>      post_heights,
    std::vector<float>        face_axes,
    // Per-INPUT intrins / dist_coeffs (length N). Required for the wide
    // warp kernel (fisheye / equisolid); pass empty when no fisheye-warp
    // camera is in the dataset.
    std::vector<float>        input_intrins,
    std::vector<float>        input_dist_coeffs,
    std::vector<int32_t>      redistort_models,
    std::vector<float>        redistort_params,
    std::vector<int32_t>      train_indices,
    std::vector<int32_t>      val_indices);

std::map<std::string, float> engine_train_step_managed(
    int step, int max_steps,
    std::string primitive,
    int sh_degree,
    bool packed,
    const EngineStepConfig& cfg);

// Answer the DataDecodeError a managed step threw: true re-runs the decode
// the worker is parked on, false abandons the pipeline. See DataManager.h.
void engine_resolve_data_error(bool retry);

// Pull the next batch from the DataManager, install it as GT + camera params,
// and run the forward pass only -- no loss, no backward, no optimizer, no
// densification. What an eval pass needs: it reuses the same decode, mask and
// fisheye/equirect warp path training does, so eval GT is the warped GT and
// not a host-side reconstruction of it.
//
// Afterwards `engine_copy_gt_rgb_to_host` and `engine_copy_render_to_host`
// read the pair back. Returns the POST-split view count in the batch (K per
// input image), or 0 when the stream is exhausted.
//
// Call `engine_setup_data_manager` with the eval split first; that replaces
// the training DataManager, so this belongs after the training loop.
int engine_eval_forward(std::string primitive, int sh_degree, bool packed);

// Same, for ONE image by dataset index (the GUI's compare view), rendering one
// pass of its faces: returns that pass's views, `out_passes` the image's count.
// Overwrites the GT and camera state a step owns -- BETWEEN steps, under its mutex.
int engine_preview_forward(int index, std::string primitive, int sh_degree,
                           bool packed, bool apply_color_correction,
                           const LossConfig& loss, int pass = 0,
                           int* out_passes = nullptr);

// --- Debug rendering ---

void engine_debug_forward(
    TorchTensorView override_features_dc,  // [max_N, 3] custom DC color, or null to use original
    int override_sh_degree,                 // -1 to use original
    TorchTensorView out_rgb                 // [C, H, W, 3] output
);

// --- Query internal state ---

void engine_copy_accum_buffer(TorchTensorView dst);
int64_t engine_get_cur_num_splats();
int64_t engine_get_max_num_splats();

// `out_rgb_raw` is the pre-color-space-conversion render (linear / wide-gamut)
// stashed by the color-space forward hook. Null OK; when the engine has no
// color space configured, `out_rgb` already holds the same values so the
// caller can pass null. When the engine has a color space, `out_rgb` is the
// sRGB post-conversion render and `out_rgb_raw` is the working-space version.
// Debug introspection: shape + D->H copy for engine().gt.rgb / gt.alpha and
// fwd.renders.rgb -- what the engine actually sees per step, which is worth
// looking at on the warp path.
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_rgb_shape();
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_alpha_shape();
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_render_rgb_shape();
void engine_copy_gt_rgb_to_host(TorchTensorView out);   // float [B, H, W, 3]
void engine_copy_gt_alpha_to_host(TorchTensorView out); // bool  [B, H, W, 1]

// The supervision modalities, at whatever resolution the files are -- the
// loss samples them rather than resizing them, so these need not be the
// render's shape. {0,0,0,0} when the DataManager was not asked for them.
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_depth_shape();
std::tuple<int64_t, int64_t, int64_t, int64_t> engine_get_gt_normal_shape();
void engine_copy_gt_depth_to_host(TorchTensorView out);  // float [B, H, W, 1]
void engine_copy_gt_normal_to_host(TorchTensorView out); // float [B, H, W, 3]

// The normal the loss derives from the rendered depth, with the same kernel
// and the camera the last forward installed. No primitive renders normals
// (engine_primitive_pixel_type), so this IS the render's normal.
void engine_copy_render_depth_normal_to_host(TorchTensorView out); // [B,H,W,3]


void engine_copy_render_to_host(
    TorchTensorView out_rgb,      // [C, H, W, 3] float32, CPU
    TorchTensorView out_depth,    // [C, H, W, 1] float32, CPU
    TorchTensorView out_Ts,       // [C, H, W, 1] float32, CPU
    TorchTensorView out_rgb_raw,  // [C, H, W, 3] float32, CPU, optional
    TorchTensorView out_median    // [C, H, W, 1] float32, CPU, optional
);

void engine_copy_distortion_to_host(
    TorchTensorView out_rgb_dist,   // [C, H, W, 3] float32, CPU, optional
    TorchTensorView out_depth_dist  // [C, H, W, 1] float32, CPU, optional
);

void engine_copy_splats_to_host(
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
);

// Per-splat gradients from the most recent backward. Same layout as
// engine_copy_splats_to_host; buffers sized to max_num_splats.
void engine_copy_grads_to_host(
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
);

// --- Viewer ---
//
// engine_viewer_init: lazy one-shot upload of the dataset-wide POST-split
// camera arrays (intrins / dist_coeffs + tier / c2w / camera_models / W / H)
// and allocation of the device-side per-camera thumbnail cache + done-mask.
// After this, every set_training_data / set_training_data_warped also runs
// the thumbnail-fill kernel for any new post-camera ids in the batch (cheap
// kernel; gated on host counter once all cams have a thumbnail). Idempotent;
// safe to call multiple times (re-uploads + resets thumbnail state).
//
// camera_models / intrins / dist_coeffs / distortions / camera_to_worlds /
// widths / heights are at POST-split layout (length N_post == sum over input
// cameras of K), matching the DataManager's post-split arrays.
//
// camera_size: the visualization-frustum scale (knn-style), in world units.
void engine_viewer_init(
    TorchTensorView camera_models,   // [N_post]   int32
    TorchTensorView intrins,         // [N_post,4] float32
    TorchTensorView dist_coeffs,     // [N_post,8] float32
    TorchTensorView distortions,     // [N_post]   int32 CameraDistortionType
    TorchTensorView camera_to_worlds,// [N_post,3,4] float32 (y/z-flipped form)
    TorchTensorView widths,          // [N_post]   int32
    TorchTensorView heights,         // [N_post]   int32
    float camera_size);

// Update just the visualization-frustum scale without re-running
// engine_viewer_init (which would reset the thumbnail cache). No-op until
// engine_viewer_init has run. Host-only; safe from any thread. The cached
// visualization BVH is rebuilt on the next blit when the size changed.
void engine_viewer_set_camera_size(float camera_size);

// Build + upload the axes/grid overlay (capsule line segments appended to
// the camera-frustum BVH, drawn when engine_blit_view's show_overlay is
// set). Axis-aligned in the ENGINE's own frame -- the frame splats are
// trained and saved in -- so lines mark round coordinates of the exported
// model. radius = scene radius in that frame (grid extent); view_distance =
// initial nav distance for the power-of-10 cell size (<= 0 uses radius).
// Per-render zoom / recenter updates arrive via engine_blit_view's
// grid_dist / grid_target. No-op until engine_viewer_init has run.
void engine_viewer_set_grid(float radius, float view_distance);

// engine_blit_view: fused viewer-render annotation. Caches the BVH on the
// first call that shows cameras or the overlay (rebuilt when camera_size
// or the overlay changes); reads thumbnails out of the
// engine's device-side cache (no thumbnails arg). buffer_key selects which
// channel of the rendered output drives the colormap path:
//   "rgb"          render_rgbs is [H,W,3] float -- passed through.
//   "depth"        render_rgbs is [H,W,1] float -- turbo-colormapped.
//   "alpha"        render_rgbs is [H,W,1] float -- viridis-colormapped.
//   "normal"       [H,W,3] float in [0,1] -- passed through.
//   "depth_normal" [H,W,3] float in [0,1] -- passed through.
// depth + alpha are always [H,W,1] float and provide the occlusion test.
// view_* fields describe the visualization camera; out_rgb is [H,W,3] uint8
// (pre-allocated CUDA buffer).
void engine_blit_view(
    std::string     buffer_key,
    TorchTensorView render_buffer,   // [H,W,1|3] float (channel matches buffer_key)
    TorchTensorView render_depth,    // [H,W,1] float, may be null when buffer_key=="alpha"
    TorchTensorView render_alpha,    // [H,W,1] float
    int             view_camera_model,
    std::string     distortion,
    TorchTensorView view_intrins,    // [1,4]   float
    TorchTensorView view_viewmat,    // [4,4]   float
    TorchTensorView view_dist_coeffs,
    bool            show_training_cameras,
    bool            show_overlay,    // axes/grid from engine_viewer_set_grid
    float           grid_dist,       // nav distance (engine frame) for the
                                     // zoom-adaptive grid decade; <= 0 keeps
                                     // the current spacing
    float           grid_target_x,   // nav orbit target (engine frame); the
    float           grid_target_y,   // grid line patch recenters on it so it
    float           grid_target_z,   // stays under the viewer at any zoom
    TorchTensorView out_rgb          // [H,W,3] uint8, pre-allocated CUDA
);

// --- Viewer scenes (docs/notes/compare-view.md) ---
//
// Several splat sets resident at once, one bound at a time; activating one is
// a pointer swap. Viewer-only: a slot has no optimizer or densify state.
constexpr int kMaxEngineScenes = 8;

void engine_scene_set_data_3dgs(
    int slot,
    int64_t num_splats,
    TorchTensorView means,
    TorchTensorView quats,
    TorchTensorView scales,
    TorchTensorView opacities,
    TorchTensorView features_dc,
    TorchTensorView features_sh
);

// The model's own colour space, applied by engine_scene_activate. Two models
// trained in different spaces are converted each its own way, which is the
// whole point of carrying it here rather than in the engine's one slot.
void engine_scene_set_color_space(int slot, bool enabled, bool is_linear,
                                  std::vector<float> color_matrix);

// Bind `slot` to the world buffers. Throws when the slot holds nothing.
void engine_scene_activate(int slot);
bool engine_scene_loaded(int slot);
void engine_scene_free(int slot);
// Drop every slot without freeing (engine_reset has already freed the pool).
void engine_scenes_forget();

// Pool VRAM breakdown: [(key, used_bytes, cap_bytes), ...]
std::vector<std::tuple<std::string, size_t, size_t>> engine_get_pool_breakdown();
// Same, with an authoritative category tag: [(key, category, used, cap), ...]
std::vector<std::tuple<std::string, std::string, size_t, size_t>>
engine_get_pool_breakdown_categorized();
size_t engine_get_scratch_bytes();

// Formatted per-category pool report (SS_PROFILE). Reads the pool's
// high-water capacities, so it is a peak, not an instantaneous figure.
std::string engine_vram_report();
// Hand that report to the profiler, which prints it at process exit -- by
// which time the pool this reads is already destroyed. No-op unless
// SS_PROFILE is set.
void engine_profile_capture_vram();

// --- Checkpoint save ---
//
// Writes into `output_dir`:
//   splat.ply  : cur_num_splats only, NaN+low-opacity-filtered (inference).
//   state.tar  : POSIX ustar (zero-dep) containing
//                  state.json          -- runtime + validation manifest
//                  <slot_name>.npy     -- one flat typed array per saved pool
//                                         slot (e.g. world.means.npy,
//                                         eng.sh_quant.q.npy). Quantized codecs
//                                         emit .q (packed) and .qb (bounds).
// Which slots are saved is driven by SaveClass metadata (PoolSlots.h), so the
// set can never drift from the live buffers:
//   full_dump=false -> Always slots  (appearance / inference params)
//   full_dump=true  -> Always+Resume (world raw params + all optimizer state,
//                                      i.e. sufficient to resume training)
// The device->host copy is chunked, using no extra device memory.
void engine_save_checkpoint(
    std::string output_dir,
    bool full_dump,
    int step
);

// Restore engine state from `input_dir`/state.tar (resume training). The engine
// skeleton must already be configured (world seeded at max_num_splats; any
// bilagrid/PPISP channels in the checkpoint initialized). Sets runtime scalars +
// optimizer layout from state.json, force-allocates optimizer/appearance-optim
// buffers, and restores every saved buffer by slot name. Returns the saved step.
int engine_load_checkpoint(std::string input_dir);
