#pragma once

#include <core/Tensor.h>

enum class RawLossIndex {
    RgbLoss,
    RgbL2,
    DepthSupX,
    DepthSupY,
    DepthSupXX,
    DepthSupYY,
    DepthSupXY,
    RenderNormalSup,
    DepthNormalSup,
    AlphaSup,
    AlphaSupUnder,
    NormalReg,
    AlphaReg,
    RgbDistReg,
    DepthDistReg,
    NormalDistReg,
    // median-depth losses (mirror per_pixel_losses.slang::RawLossIndex)
    MeanMedianDepthSup,
    MedianDepthNormalReg,
    MedianNormalSup,
    MedianRenderNormalReg,
    PixelsTotal,
    MaskTotal,
    DepthMaskTotal,
    RenderNormalMaskTotal,
    DepthNormalMaskTotal,
    NormalRegMaskTotal,
    AlphaOverTotal,
    AlphaUnderTotal,
    // median-depth mask totals
    MeanMedianDepthMaskTotal,
    MedianDepthNormalMaskTotal,
    MedianNormalMaskTotal,
    MedianRenderNormalMaskTotal,
    length
};

enum class LossWeightIndex {
    RgbSupL1,
    RgbSupL2,
    // YUV (BT.601) per-pixel supervision; folded into RgbLoss in the slang
    // kernel so it shares the MaskTotal divisor. Keep the order in sync with
    // per_pixel_losses.slang::LossWeightIndex.
    YSupL1,
    YSupL2,
    USupL2,
    VSupL2,
    DepthSup,
    NormalSup,
    AlphaSup,
    AlphaSupUnder,
    NormalReg,
    AlphaReg,
    RgbDistReg,
    DepthDistReg,
    NormalDistReg,
    // median-depth loss weights
    MeanMedianDepthSup,
    MedianDepthNormalReg,
    MedianNormalSup,
    MedianRenderNormalReg,
    length
};

enum class LossIndex {
    RgbLoss,
    RgbPSNR,
    DepthSup,
    NormalSup,
    AlphaSup,
    NormalReg,
    AlphaReg,
    RgbDistReg,
    DepthDistReg,
    NormalDistReg,
    MeanMedianDepthSup,
    MedianDepthNormalReg,
    MedianNormalSup,
    MedianRenderNormalReg,
    length
};

struct PerPixelGrads {
    TorchTensorView v_render_rgb, v_ref_rgb, v_render_depth, v_ref_depth;
    TorchTensorView v_render_normal, v_depth_normal, v_ref_normal;
    TorchTensorView v_render_Ts, v_rgb_dist, v_depth_dist, v_normal_dist;
    TorchTensorView v_median_depth, v_median_normal;
};

// Numeric values are shared with the `densify_loss_map_mode` config flag, and
// the per-mode meanings with its help text in i18n/catalog/TrainFields.h.
// EdgeAware is Plenoxels' edge guidance (https://arxiv.org/abs/2603.08661).
enum class DensifyLossMapMode : int {
    None = 0,
    LossFull = 1,
    SsimFull = 2,
    SsimContrastStruct = 3,
    SsimStructure = 4,
    EdgeAware = 5,
    RobustEdgeAware = 6,
    LossFullNms = 7,
    SsimFullNms = 8,
    SsimContrastStructNms = 9,
    SsimStructureNms = 10,
};

// A *Nms mode is its base plus canny's non-maximum-suppression step (see
// loss_map_nms in PerPixelLoss.cu), so everything upstream switches on base.
constexpr bool densify_loss_map_has_nms(DensifyLossMapMode m) {
    return m >= DensifyLossMapMode::LossFullNms;
}
constexpr DensifyLossMapMode densify_loss_map_base(DensifyLossMapMode m) {
    return densify_loss_map_has_nms(m)
        ? (DensifyLossMapMode)((int)m - (int)DensifyLossMapMode::LossFullNms + 1)
        : m;
}

// Mode dispatched to the SSIM kernel (the SSIM kernel doesn't need to know
// about per-pixel L1/L2 or edge-aware; those are handled outside it).
//   SsimNone  -> skip SSIM loss_map write.
//   SsimFull  -> full SSIM(LCS).
//   SsimCs    -> SSIM contrast*structure.
//   SsimStr   -> SSIM structure only.
enum class SsimLossMapMode : int {
    SsimNone = 0,
    SsimFull = 1,
    SsimCs   = 2,
    SsimStr  = 3,
};

// Per-step loss values returned by compute_multi_scale_per_pixel_losses.
// Fields mirror LossIndex (raw weighted sums) plus ssim.
struct LossValues {
    float rgb_loss;
    float rgb_psnr;
    float depth_sup;
    float normal_sup;
    float alpha_sup;
    float normal_reg;
    float alpha_reg;
    float rgb_dist_reg;
    float depth_dist_reg;
    float normal_dist_reg;
    float mean_median_depth_sup;
    float median_depth_normal_reg;
    float median_normal_sup;
    float median_render_normal_reg;
    float ssim;
};


/* == AUTO HEADER GENERATOR - DO NOT EDIT THIS LINE OR ANYTHING BELOW THIS LINE == */



LossValues compute_multi_scale_per_pixel_losses(
    int num_loss_scales,
    TorchTensorView render_rgb,
    TorchTensorView ref_rgb,
    TorchTensorView render_depth,
    TorchTensorView ref_depth,
    TorchTensorView render_normal,
    TorchTensorView depth_normal,
    TorchTensorView ref_normal,
    TorchTensorView render_Ts,
    TorchTensorView rgb_dist,
    TorchTensorView depth_dist,
    TorchTensorView normal_dist,
    TorchTensorView median_depth,
    TorchTensorView median_normal,
    TorchTensorView ref_alpha,
    bool has_mask,
    const std::array<float, (int)LossWeightIndex::length> loss_weights_0,
    const float w_ssim,
    const float ssim_grad_scale,
    // Positive: a pixel whose render and reference are both above it in every
    // channel leaves the loss entirely -- value, map and gradient. Both are
    // clipped there, so no colour error at that pixel is recoverable.
    const float saturation_threshold,
    TorchTensorView v_losses,
    std::vector<bool> needs_input_grad,
    long num_train_images,
    TorchTensorView camera_indices,
    TorchTensorView loss_map_out,
    int loss_map_mode,
    float robust_edge_aware_quantile,
    float nms_falloff,
    PerPixelGrads& grads_out
);
