// All per-Pixel losses in one kernel

[ForceInline]
[Differentiable]
float mean3(float3 a) {
    return (a.x + a.y + a.z) * (1.0 / 3.0);
}

[ForceInline]
[Differentiable]
float l1_loss(float3 a, float3 b) {
    return mean3(abs(b - a));
}

[ForceInline]
[Differentiable]
float l2_loss(float3 a, float3 b) {
    return dot(b - a, b - a) * (1.0 / 3.0);
}

[ForceInline]
[Differentiable]
float3 normalize_normal(float3 normal, inout bool mask) {
    float norm2 = dot(normal, normal);
    if (norm2 == 0.0) {
        mask = false;
        return float3(0.0);
    }
    return normal * rsqrt(norm2);
}

[ForceInline]
[Differentiable]
float normal_loss(float3 normal, float3 normal_ref) {
    // return 1.0 - dot(normal, normal_ref);
    // return l1_loss(normal, normal_ref);
    float cos_sim_loss = 0.5 - 0.5 * dot(normal, normal_ref);
    return cos_sim_loss + sqrt(max(cos_sim_loss, 1e-12));
}

[ForceInline]
[Differentiable]
float bce_loss(float val, float ref) {
    const float eps = 1e-6;
    return -lerp(log(max(1.0 - val, eps)), log(max(val, eps)), ref);
}

[ForceInline]
[Differentiable]
float alpha_loss(float x, float y) {
    return bce_loss(max(x, y), y);
}

enum RawLossIndex {
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
    // median-depth losses
    MeanMedianDepthSup,      // L1(mean depth, median depth)
    MedianDepthNormalReg,    // normal_loss(median normal, depth normal)
    MedianNormalSup,         // normal_loss(median normal, ref normal)
    MedianRenderNormalReg,   // normal_loss(median normal, render normal)
    PixelsTotal,
    MaskTotal,
    DepthMaskTotal,
    RenderNormalMaskTotal,
    DepthNormalMaskTotal,
    NormalRegMaskTotal,
    // Alpha supervision scores only half the image each: "should be empty"
    // pixels for AlphaSup, "should be solid" ones for AlphaSupUnder. Each is a
    // mean over its own half, so the weights do not shift with subject size.
    AlphaOverTotal,
    AlphaUnderTotal,
    // median-depth mask totals
    MeanMedianDepthMaskTotal,
    MedianDepthNormalMaskTotal,
    MedianNormalMaskTotal,
    MedianRenderNormalMaskTotal,
    length
};

enum LossWeightIndex {
    RgbSupL1,
    RgbSupL2,
    // Per-pixel YUV (BT.601) supervision on top of plain RGB. Folded into
    // RawLossIndex::RgbLoss so the same MaskTotal divisor applies.
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

// BT.601 RGB (linear-ish) -> YUV. Used by the YSup* / USup* / VSup* losses.
[ForceInline]
[Differentiable]
float3 rgb_to_yuv(float3 rgb) {
    float Y =  0.299f * rgb.x + 0.587f * rgb.y + 0.114f * rgb.z;
    float U = -0.14713f * rgb.x - 0.28886f * rgb.y + 0.436f * rgb.z;
    float V =  0.615f * rgb.x - 0.51499f * rgb.y - 0.10001f * rgb.z;
    return float3(Y, U, V);
}

[Differentiable]
[CudaDeviceExport]
float[RawLossIndex::length] per_pixel_losses(
    float3 render_rgb,
    float3 ref_rgb,
    float render_depth,
    float ref_depth,
    float3 render_normal,
    float3 depth_normal,
    float3 ref_normal,
    float render_Ts,
    float3 rgb_dist,
    float depth_dist,
    float3 normal_dist,
    float median_depth,
    float3 median_normal,
    bool ref_alpha,
    no_diff bool has_mask,
    no_diff float[LossWeightIndex::length] weights
) {
    float[RawLossIndex::length] losses;

    // Mean (expected) depth captured before the Pearson-depth log remap below,
    // so the mean-vs-median L1 term sees the raw linear depth.
    float mean_depth_raw = render_depth;

    // Per-pixel validity. 0 depth is the "no gt" sentinel; normal_mask is
    // gt_normal_valid, spelled out here because core/Interpolation.cuh is
    // CUDA-only. No mask means ref_alpha is false everywhere, not "empty".
    bool mask = has_mask ? ref_alpha : true;
    bool depth_mask = (ref_depth != 0.0f);
    bool normal_mask = (ref_normal.x + ref_normal.y + ref_normal.z > -2.366f &&
                        dot(ref_normal, ref_normal) > 0.25f);
    bool alpha_mask = has_mask;

    // RGB - L1 and L2, plus YUV (BT.601) per-channel L1/L2, for training.
    // RgbL2 stays plain-RGB (PSNR calculation).
    float3 render_yuv = rgb_to_yuv(render_rgb);
    float3 ref_yuv    = rgb_to_yuv(ref_rgb);
    float dY = render_yuv.x - ref_yuv.x;
    float dU = render_yuv.y - ref_yuv.y;
    float dV = render_yuv.z - ref_yuv.z;
    losses[RawLossIndex::RgbLoss] = float(mask) * (
        weights[LossWeightIndex::RgbSupL1] * l1_loss(render_rgb, ref_rgb) +
        weights[LossWeightIndex::RgbSupL2] * l2_loss(render_rgb, ref_rgb) +
        weights[LossWeightIndex::YSupL1]  * abs(dY) +
        weights[LossWeightIndex::YSupL2]  * dY * dY +
        weights[LossWeightIndex::USupL2]  * dU * dU +
        weights[LossWeightIndex::VSupL2]  * dV * dV
    );
    losses[RawLossIndex::RgbL2] = float(mask) *
        clamp(l2_loss(render_rgb, ref_rgb), 0.0f, 1.0f);

    // Depth - Pearson correlation in log space
    depth_mask &= mask;
    render_depth = float(depth_mask) * max(render_depth, 1e-4);
    ref_depth = float(depth_mask) * max(ref_depth, 1e-4);
    losses[RawLossIndex::DepthSupX] = render_depth;
    losses[RawLossIndex::DepthSupY] = ref_depth;
    losses[RawLossIndex::DepthSupXX] = render_depth * render_depth;
    losses[RawLossIndex::DepthSupYY] = ref_depth * ref_depth;
    losses[RawLossIndex::DepthSupXY] = render_depth * ref_depth;

    // Normal - Supervision and regularization - Pairwise loss between different normals
    normal_mask &= mask;
    bool render_normal_mask = true,
         depth_normal_mask = true;
    render_normal = normalize_normal(render_normal, render_normal_mask);
    depth_normal = normalize_normal(depth_normal, depth_normal_mask);
    ref_normal = normalize_normal(ref_normal, normal_mask);
    losses[RawLossIndex::RenderNormalSup] = weights[LossWeightIndex::NormalSup] *
        float(render_normal_mask & normal_mask) * normal_loss(render_normal, ref_normal);
    losses[RawLossIndex::DepthNormalSup] = weights[LossWeightIndex::NormalSup] *
        float(depth_normal_mask & normal_mask) * normal_loss(depth_normal, ref_normal);
    losses[RawLossIndex::NormalReg] = weights[LossWeightIndex::NormalReg] *
        float(render_normal_mask & depth_normal_mask & mask) * normal_loss(render_normal, depth_normal);

    // Median depth - L1 to mean depth + normal consistency against the depth
    // normal, ref normal, and render normal. median_normal is the normal
    // extracted from the median depth map (computed outside, like depth_normal).
    bool median_normal_mask = true;
    median_normal = normalize_normal(median_normal, median_normal_mask);
    // 1) mean vs median depth L1, only where both depths are nonzero
    bool mean_median_mask = mask && (mean_depth_raw > 1e-10f) && (median_depth > 1e-10f);
    // Clamped INSIDE the log: where the mask is 0 a depth is 0 too, and
    // 0 * log(0) is a NaN -- every pixel of the loss_full densify error map
    // on a run with no median depth.
    losses[RawLossIndex::MeanMedianDepthSup] = weights[LossWeightIndex::MeanMedianDepthSup] *
        float(mean_median_mask) * abs(log(max(mean_depth_raw, 1e-10f)) -
                                      log(max(median_depth, 1e-10f)));
    // 2) median normal vs depth (mean) normal
    losses[RawLossIndex::MedianDepthNormalReg] = weights[LossWeightIndex::MedianDepthNormalReg] *
        float(median_normal_mask & depth_normal_mask & mask) * normal_loss(median_normal, depth_normal);
    // 3) median normal vs reference normal
    losses[RawLossIndex::MedianNormalSup] = weights[LossWeightIndex::MedianNormalSup] *
        float(median_normal_mask & normal_mask) * normal_loss(median_normal, ref_normal);
    // 4) median normal vs render normal (render_normal is a placeholder for now)
    losses[RawLossIndex::MedianRenderNormalReg] = weights[LossWeightIndex::MedianRenderNormalReg] *
        float(median_normal_mask & render_normal_mask & mask) * normal_loss(median_normal, render_normal);

    // Alpha - loss with mask + regularization to go to 0 or 1
    float render_alpha = clamp(1.0f-render_Ts, 0.0f, 1.0f);
    losses[RawLossIndex::AlphaSup] = weights[LossWeightIndex::AlphaSup] *
        float(alpha_mask) * alpha_loss(render_alpha, float(ref_alpha));
    losses[RawLossIndex::AlphaSupUnder] = weights[LossWeightIndex::AlphaSupUnder] *
        float(alpha_mask) * alpha_loss(1.0 - render_alpha, 1.0 - float(ref_alpha));
    losses[RawLossIndex::AlphaReg] = weights[LossWeightIndex::AlphaReg] *
        float(mask) * 4.0f * render_alpha * (1.0f - render_alpha);  // TODO: push-to-1 mode

    // Distortion regularization
    losses[RawLossIndex::RgbDistReg] = weights[LossWeightIndex::RgbDistReg] *
        float(mask) * mean3(rgb_dist);// / max(render_alpha, 1e-12);
    losses[RawLossIndex::DepthDistReg] = weights[LossWeightIndex::DepthDistReg] *
        float(mask) * depth_dist;// / max(render_alpha, 1e-12);
    losses[RawLossIndex::NormalDistReg] = weights[LossWeightIndex::NormalDistReg] *
        float(mask) * mean3(normal_dist);// / max(render_alpha, 1e-12);

    // Mask total
    losses[RawLossIndex::PixelsTotal] = 1.0f;  // required for patched batching
    losses[RawLossIndex::MaskTotal] = float(mask);
    losses[RawLossIndex::DepthMaskTotal] = float(depth_mask);
    losses[RawLossIndex::RenderNormalMaskTotal] = float(render_normal_mask & normal_mask);
    losses[RawLossIndex::DepthNormalMaskTotal] = float(depth_normal_mask & normal_mask);
    losses[RawLossIndex::NormalRegMaskTotal] = float(render_normal_mask & depth_normal_mask & mask);
    losses[RawLossIndex::AlphaOverTotal] = float(alpha_mask && !ref_alpha);
    losses[RawLossIndex::AlphaUnderTotal] = float(alpha_mask && ref_alpha);
    losses[RawLossIndex::MeanMedianDepthMaskTotal] = float(mean_median_mask);
    losses[RawLossIndex::MedianDepthNormalMaskTotal] = float(median_normal_mask & depth_normal_mask & mask);
    losses[RawLossIndex::MedianNormalMaskTotal] = float(median_normal_mask & normal_mask);
    losses[RawLossIndex::MedianRenderNormalMaskTotal] = float(median_normal_mask & render_normal_mask & mask);

    return losses;
}

[CudaDeviceExport]
void per_pixel_losses_bwd(
    float3 render_rgb,
    float3 ref_rgb,
    float render_depth,
    float ref_depth,
    float3 render_normal,
    float3 depth_normal,
    float3 ref_normal,
    float render_Ts,
    float3 rgb_dist,
    float depth_dist,
    float3 normal_dist,
    float median_depth,
    float3 median_normal,
    bool ref_alpha,
    bool has_mask,
    no_diff float[LossWeightIndex::length] weights,
    float[RawLossIndex::length] v_losses,
    out float3 v_render_rgb,
    out float3 v_ref_rgb,
    out float v_render_depth,
    out float v_ref_depth,
    out float3 v_render_normal,
    out float3 v_depth_normal,
    out float3 v_ref_normal,
    out float v_render_Ts,
    out float3 v_rgb_dist,
    out float v_depth_dist,
    out float3 v_normal_dist,
    out float v_median_depth,
    out float3 v_median_normal
) {
    DifferentialPair<float3> dp_render_rgb = diffPair(render_rgb);
    DifferentialPair<float3> dp_ref_rgb = diffPair(ref_rgb);
    DifferentialPair<float> dp_render_depth = diffPair(render_depth);
    DifferentialPair<float> dp_ref_depth = diffPair(ref_depth);
    DifferentialPair<float3> dp_render_normal = diffPair(render_normal);
    DifferentialPair<float3> dp_depth_normal = diffPair(depth_normal);
    DifferentialPair<float3> dp_ref_normal = diffPair(ref_normal);
    DifferentialPair<float> dp_render_Ts = diffPair(render_Ts);
    DifferentialPair<float3> dp_rgb_dist = diffPair(rgb_dist);
    DifferentialPair<float> dp_depth_dist = diffPair(depth_dist);
    DifferentialPair<float3> dp_normal_dist = diffPair(normal_dist);
    DifferentialPair<float> dp_median_depth = diffPair(median_depth);
    DifferentialPair<float3> dp_median_normal = diffPair(median_normal);
    bwd_diff(per_pixel_losses)(
        dp_render_rgb,
        dp_ref_rgb,
        dp_render_depth,
        dp_ref_depth,
        dp_render_normal,
        dp_depth_normal,
        dp_ref_normal,
        dp_render_Ts,
        dp_rgb_dist,
        dp_depth_dist,
        dp_normal_dist,
        dp_median_depth,
        dp_median_normal,
        ref_alpha,
        has_mask,
        weights,
        v_losses
    );
    v_render_rgb = dp_render_rgb.d;
    v_ref_rgb = dp_ref_rgb.d;
    v_render_depth = dp_render_depth.d;
    v_ref_depth = dp_ref_depth.d;
    v_render_normal = dp_render_normal.d;
    v_depth_normal = dp_depth_normal.d;
    v_ref_normal = dp_ref_normal.d;
    v_render_Ts = dp_render_Ts.d;
    v_rgb_dist = dp_rgb_dist.d;
    v_depth_dist = dp_depth_dist.d;
    v_normal_dist = dp_normal_dist.d;
    v_median_depth = dp_median_depth.d;
    v_median_normal = dp_median_normal.d;
}

enum LossIndex {
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

[Differentiable]
[CudaDeviceExport]
float[LossIndex::length] per_pixel_losses_reduce(
    float[RawLossIndex::length] raw_losses,
    no_diff float[LossWeightIndex::length] weights
) {
    float[LossIndex::length] losses;

    // image loss
    losses[LossIndex::RgbLoss] = 
        raw_losses[RawLossIndex::RgbLoss] / max(raw_losses[RawLossIndex::MaskTotal], 1);
    losses[LossIndex::RgbPSNR] = -10.0 * log10(
        raw_losses[RawLossIndex::RgbL2] / max(raw_losses[RawLossIndex::MaskTotal], 1));

    // depth supervision loss - Pearson correlation
    losses[LossIndex::DepthSup] = (
        raw_losses[RawLossIndex::DepthMaskTotal] > 0.0 &&
        raw_losses[RawLossIndex::DepthSupY] != 0.0
    ) ?
        weights[LossWeightIndex::DepthSup] * clamp(1.0 -
        (
            raw_losses[RawLossIndex::DepthSupXY] -
                raw_losses[RawLossIndex::DepthSupX] * raw_losses[RawLossIndex::DepthSupY]
                    / raw_losses[RawLossIndex::DepthMaskTotal]
        ) / sqrt(max(1e-12,
            (raw_losses[RawLossIndex::DepthSupXX] -
                raw_losses[RawLossIndex::DepthSupX] * raw_losses[RawLossIndex::DepthSupX]
                    / raw_losses[RawLossIndex::DepthMaskTotal]
            ) * (raw_losses[RawLossIndex::DepthSupYY] -
                raw_losses[RawLossIndex::DepthSupY] * raw_losses[RawLossIndex::DepthSupY]
                    / raw_losses[RawLossIndex::DepthMaskTotal]
            )
         + 1.0))  // numerical issue
    , 0.0, 2.0) : 0.0;

    // normal supervision loss
    losses[LossIndex::NormalSup] = (
        raw_losses[RawLossIndex::RenderNormalSup] / max(raw_losses[RawLossIndex::RenderNormalMaskTotal], 1) +
        raw_losses[RawLossIndex::DepthNormalSup] / max(raw_losses[RawLossIndex::DepthNormalMaskTotal], 1)
    ) / max(int(raw_losses[RawLossIndex::RenderNormalMaskTotal] > 0.5) +
            int(raw_losses[RawLossIndex::DepthNormalMaskTotal] > 0.5), 1);

    // alpha supervision loss
    losses[LossIndex::AlphaSup] =
        raw_losses[RawLossIndex::AlphaSup] /
            max(raw_losses[RawLossIndex::AlphaOverTotal], 1) +
        raw_losses[RawLossIndex::AlphaSupUnder] /
            max(raw_losses[RawLossIndex::AlphaUnderTotal], 1);

    // normal regularization
    losses[LossIndex::NormalReg] =
        raw_losses[RawLossIndex::NormalReg] / max(raw_losses[RawLossIndex::NormalRegMaskTotal], 1);

    // alpha regularization
    // TODO: push-to-1 mode for random background
    losses[LossIndex::AlphaReg] =
        raw_losses[RawLossIndex::AlphaReg] / max(raw_losses[RawLossIndex::MaskTotal], 1);

    // distortion regularizations
    losses[LossIndex::RgbDistReg] =
        raw_losses[RawLossIndex::RgbDistReg] / max(raw_losses[RawLossIndex::MaskTotal], 1);
    losses[LossIndex::DepthDistReg] =
        raw_losses[RawLossIndex::DepthDistReg] / max(raw_losses[RawLossIndex::MaskTotal], 1);
    losses[LossIndex::NormalDistReg] =
        raw_losses[RawLossIndex::NormalDistReg] / max(raw_losses[RawLossIndex::MaskTotal], 1);

    // median-depth losses, each normalized by its own mask total
    losses[LossIndex::MeanMedianDepthSup] =
        raw_losses[RawLossIndex::MeanMedianDepthSup] / max(raw_losses[RawLossIndex::MeanMedianDepthMaskTotal], 1);
    losses[LossIndex::MedianDepthNormalReg] =
        raw_losses[RawLossIndex::MedianDepthNormalReg] / max(raw_losses[RawLossIndex::MedianDepthNormalMaskTotal], 1);
    losses[LossIndex::MedianNormalSup] =
        raw_losses[RawLossIndex::MedianNormalSup] / max(raw_losses[RawLossIndex::MedianNormalMaskTotal], 1);
    losses[LossIndex::MedianRenderNormalReg] =
        raw_losses[RawLossIndex::MedianRenderNormalReg] / max(raw_losses[RawLossIndex::MedianRenderNormalMaskTotal], 1);

    return losses;
}

[CudaDeviceExport]
float[RawLossIndex::length] per_pixel_losses_reduce_bwd(
    const float[RawLossIndex::length] raw_losses,
    const no_diff float[LossWeightIndex::length] weights,
    float[LossIndex::length] v_losses
) {
    DifferentialPair<float[RawLossIndex::length]> dp_raw_losses = diffPair(raw_losses);
    bwd_diff(per_pixel_losses_reduce)(
        dp_raw_losses, weights,
        v_losses
    );
    return dp_raw_losses.d;
}
