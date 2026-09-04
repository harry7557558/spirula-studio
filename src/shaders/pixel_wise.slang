// suppress CudaDeviceExport to reduce code size
#ifdef CudaDeviceExport
#include "projection_utils.slang"
#else
#define CudaDeviceExport Differentiable
#include "projection_utils.slang"
#undef CudaDeviceExport
#endif

[Differentiable]
[CudaDeviceExport]
float rendered_depth_to_expected_depth(float depth, float transmittance) {
    return depth / max(1.0f - transmittance, 1e-10f);
}

[CudaDeviceExport]
void rendered_depth_to_expected_depth_bwd(
    float depth, float transmittance,
    float v_out_depth,
    out float v_depth, out float v_transmittance
) {
    var p_depth = diffPair(depth);
    var p_transmittance = diffPair(transmittance);
    bwd_diff(rendered_depth_to_expected_depth)(
        p_depth, p_transmittance,
        v_out_depth
    );
    v_depth = p_depth.d;
    v_transmittance = p_transmittance.d;
}

// Hand-written: the Vulkan half's slangc autodiffs `transmittance` to zero
// through the splat in the multiply below, so the SH skybox and the alpha
// gradient would silently vanish. Exact -- the blend is affine.
void blend_background_bwd_impl(
    inout DifferentialPair<float3> dp_rgb,
    inout DifferentialPair<float> dp_transmittance,
    inout DifferentialPair<float3> dp_background,
    float3 v_out
) {
    float t = dp_transmittance.p;
    float3 bg = dp_background.p;
    dp_rgb = diffPair(dp_rgb.p, v_out);
    dp_transmittance = diffPair(t, dot(bg, v_out));
    dp_background = diffPair(bg, t * v_out);
}

// The composite is deliberately unclamped: clamping to [0,1] here costs the
// photometric loss every gradient in a region that saturates in both render
// and GT, which lets the pre-blend colour drift with nothing to pull it back.
[BackwardDerivative(blend_background_bwd_impl)]
[Differentiable]
[CudaDeviceExport]
float3 blend_background(float3 rgb, float transmittance, float3 background) {
    return rgb + transmittance * background;
}

// dL/dc of L = weight * mean(max(-c, c-1, 0)^2); scale is 2 * weight / N.
[CudaDeviceExport]
float3 overexposure_grad(float3 c, float scale) {
    return scale * (min(c, float3(0.0f)) + max(c - float3(1.0f), float3(0.0f)));
}

[CudaDeviceExport]
void blend_background_bwd(
    float3 rgb, float transmittance, float3 background,
    float3 v_out_rgb, float overexposure_scale,
    out float3 v_rgb, out float v_transmittance, out float3 v_background
) {
    // `rgb` MUST be the pre-blend render, not the blend output.
    // Branch, not a multiply by zero: an infinite composite would make 0*inf.
    if (overexposure_scale != 0.0f)
        v_out_rgb += overexposure_grad(rgb + transmittance * background,
                                       overexposure_scale);
    var p_rgb = diffPair(rgb);
    var p_transmittance = diffPair(transmittance);
    var p_background = diffPair(background);
    bwd_diff(blend_background)(
        p_rgb, p_transmittance, p_background,
        v_out_rgb
    );
    v_rgb = p_rgb.d;
    v_transmittance = p_transmittance.d;
    v_background = p_background.d;
}

[ForceInline]
[Differentiable]
[CudaDeviceExport]
float linear_rgb_to_srgb(float x) {
    const float cutoff = 0.0031308f;
    return (x < cutoff ? x * 12.92f : 1.055f * pow(x, 1.0f / 2.4f) - 0.055f);
}

[CudaDeviceExport]
float linear_rgb_to_srgb_grad(float x) {
    var dp_x = diffPair(x, 1.0);
    var dp_y = fwd_diff(linear_rgb_to_srgb)(dp_x);
    return dp_y.d;
}

[ForceInline]
[Differentiable]
[CudaDeviceExport]
float srgb_to_linear_rgb(float x) {
    // 0.04045 is the branch point (= 12.92 * 0.0031308); 0.055 is the offset.
    return x < 0.04045f ?
        x * (1.0f / 12.92f) :
        pow((1.0f / 1.055f) * (x + 0.055f), 2.4f);
}

[CudaDeviceExport]
float srgb_to_linear_rgb_grad(float x) {
    var dp_x = diffPair(x, 1.0);
    var dp_y = fwd_diff(srgb_to_linear_rgb)(dp_x);
    return dp_y.d;
}

// Splat DC coefficient <-> an affine map of sqrt(colour), scaled to
// dx/d(dc) == 1 at colour 0.5, continued linearly below 0 for wide-gamut
// negatives. An sRGB encode here cost 1.5 dB: its toe frees near-black splats.
[ForceInline]
[CudaDeviceExport]
float splat_dc_encode(float dc) {
    const float kSh0 = 0.28209479177387814f;
#if 1
    const float a = 1.0f / 255.0f;            // sqrt of the black floor
    float s = sqrt(0.5f + a * a);
    float u = kSh0 * dc + 0.5f;
    float w = u >= 0.0f ? 2.0f * sqrt(u + a * a) : 2.0f * a + u / a;
    return (w - 2.0f * s) * s / kSh0;
#else
    return linear_rgb_to_srgb(0.5f + kSh0 * dc) * (1.0f / kSh0) - 0.5f;
#endif
}

[ForceInline]
[CudaDeviceExport]
float splat_dc_decode(float x) {
    const float kSh0 = 0.28209479177387814f;
#if 1
    const float a = 1.0f / 255.0f;
    float s = sqrt(0.5f + a * a);
    float w = x * kSh0 / s + 2.0f * s;
    float u = w >= 2.0f * a ? 0.25f * w * w - a * a : (w - 2.0f * a) * a;
    return (u - 0.5f) / kSh0;
#else
    return max(srgb_to_linear_rgb(0.5f + kSh0 * x), 0.0f) * (1.0f / kSh0) - 0.5f;
#endif
}

// ================
// Output transfer: linear light -> display code values
// ================
// Orthogonal to whether the buffer is stored linear (the `is_linear` argument
// below). Mirrored on the host in core/ColorSpace.h; the two must agree.
static const int kXferSrgb        = 0;
static const int kXferSrgbClamped = 1;
static const int kXferAces        = 2;
static const int kXferFilmic      = 3;
static const int kXferUncharted2  = 4;

// Scene value that decodes from display white. ACES reaches 1.0 at 7.24 on its
// own; Hejl only reaches it at infinity, so it borrows Hable's 11.2.
static const float kXferWhite = 11.2f;

// Straight-through: the forward value is clipped, the gradient is not. A true
// clip strands any splat that overshoots white where the target is below it.
void xfer_pass_grad(inout DifferentialPair<float> dp, float v_out) {
    dp = diffPair(dp.p, v_out);
}

// [BackwardDerivative(xfer_pass_grad)]
[Differentiable]
float xfer_clamp01(float x) { return clamp(x, 0.0f, 1.0f); }

// [BackwardDerivative(xfer_pass_grad)]
[Differentiable]
float xfer_max0(float x) { return max(x, 0.0f); }

// Narkowicz ACES fit. Denominator has no real roots, so it is safe below zero.
[Differentiable]
float xfer_aces(float x) {
    return (x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f);
}

float xfer_aces_inv(float y) {
    float a = 2.51f - 2.43f * y, b = 0.03f - 0.59f * y, c = -0.14f * y;
    return (-b + sqrt(max(b * b - 4.0f * a * c, 0.0f))) / (2.0f * a);
}

// Hable's Uncharted 2 curve, normalized so kXferWhite maps to 1.0.
static const float kU2A = 0.15f, kU2B = 0.50f, kU2C = 0.10f;
static const float kU2D = 0.20f, kU2E = 0.02f, kU2F = 0.30f;

[Differentiable]
float xfer_hable(float x) {
    return (x * (kU2A * x + kU2C * kU2B) + kU2D * kU2E)
         / (x * (kU2A * x + kU2B) + kU2D * kU2F) - kU2E / kU2F;
}

[Differentiable]
float xfer_uncharted2(float x) {
    return xfer_hable(xfer_max0(x)) / xfer_hable(kXferWhite);
}

float xfer_uncharted2_inv(float y) {
    float r = y * xfer_hable(kXferWhite) + kU2E / kU2F;
    float a = kU2A * (1.0f - r);
    float b = kU2B * (kU2C - r);
    float c = kU2D * (kU2E - r * kU2F);
    return (-b + sqrt(max(b * b - 4.0f * a * c, 0.0f))) / (2.0f * a);
}

// Hejl / Burgess-Dawson: the display encode is baked in, so no OETF follows.
[Differentiable]
float xfer_filmic(float x) {
    float t = xfer_max0(x - 0.004f);
    return (t * (6.2f * t + 0.5f)) / (t * (6.2f * t + 1.7f) + 0.06f);
}

float xfer_filmic_inv(float y) {
    y = min(y, xfer_filmic(kXferWhite));
    float a = 6.2f * (1.0f - y), b = 0.5f - 1.7f * y, c = -0.06f * y;
    return (-b + sqrt(max(b * b - 4.0f * a * c, 0.0f))) / (2.0f * a) + 0.004f;
}

[Differentiable]
float tone_encode(float x, no_diff int transfer) {
    if (transfer == kXferFilmic)
        return xfer_filmic(x);
    if (transfer == kXferAces)
        return linear_rgb_to_srgb(xfer_clamp01(xfer_aces(xfer_max0(x))));
    if (transfer == kXferUncharted2)
        return linear_rgb_to_srgb(xfer_clamp01(xfer_uncharted2(x)));
    if (transfer == kXferSrgbClamped)
        return linear_rgb_to_srgb(xfer_clamp01(x));
    return linear_rgb_to_srgb(x);
}

float tone_decode(float d, int transfer) {
    if (transfer == kXferFilmic)      return xfer_filmic_inv(d);
    if (transfer == kXferAces)        return xfer_aces_inv(srgb_to_linear_rgb(d));
    if (transfer == kXferUncharted2)  return xfer_uncharted2_inv(srgb_to_linear_rgb(d));
    return srgb_to_linear_rgb(d);
}

// A working-space colour -> the display code values the loss is measured on.
// `is_linear` says whether the working space stores linear light; `transfer`
// is the curve out of it. The two are independent.
[Differentiable]
[CudaDeviceExport]
float3 working_to_display(float3 rgb, no_diff float3x3 color_matrix,
                          no_diff int transfer, no_diff bool is_linear) {
    if (!is_linear)
        rgb = float3(srgb_to_linear_rgb(rgb.x), srgb_to_linear_rgb(rgb.y),
                     srgb_to_linear_rgb(rgb.z));
    rgb = mul(color_matrix, rgb);
    return float3(tone_encode(rgb.x, transfer), tone_encode(rgb.y, transfer),
                  tone_encode(rgb.z, transfer));
}

[CudaDeviceExport]
float3 working_to_display_bwd(
    float3 rgb, float3x3 color_matrix, int transfer, bool is_linear,
    float3 v_out_rgb
) {
    var p_rgb = diffPair(rgb);
    bwd_diff(working_to_display)(p_rgb, color_matrix, transfer, is_linear,
                                 v_out_rgb);
    return p_rgb.d;
}

// The inverse, per channel, with no gamut matrix: what a splat must hold to
// display as `rgb`. Seeds and the noise background are both stated that way.
[CudaDeviceExport]
float3 display_to_working3(float3 rgb, int transfer, bool is_linear) {
    float3 lin = float3(tone_decode(rgb.x, transfer),
                        tone_decode(rgb.y, transfer),
                        tone_decode(rgb.z, transfer));
    if (is_linear) return lin;
    return float3(linear_rgb_to_srgb(lin.x), linear_rgb_to_srgb(lin.y),
                  linear_rgb_to_srgb(lin.z));
}

// Unproject an undistorted, normalized image point to a ray direction.
// Handles all camera models (PINHOLE / FISHEYE / EQUISOLID); EQUIRECTANGULAR
// is currently unsupported and falls back to PINHOLE.
// is_ray_depth: ray is unit length (depth measured along the ray);
// otherwise the ray has z == 1 (depth measured along the optical axis).
[Differentiable]
float3 unproject_raydir(
    float2 uv,
    no_diff int camera_model,
    no_diff bool is_ray_depth
) {
    CameraModelType model = (CameraModelType)camera_model;

    float3 raydir;
    bool is_unit = false;  // base ray already has unit length
    if (model == CameraModelType::FISHEYE) {
        float theta = length(uv);
        raydir = float3(uv / max(theta, 1e-7f) * sin(theta), cos(theta));
        is_unit = true;
    }
    else if (model == CameraModelType::EQUISOLID) {
        float r = length(uv);
        float z = 1.0f - 0.5f * r * r;
        float s = sqrt(max(0.0f, 1.0f - 0.25f * r * r));
        raydir = float3(uv * s, z);
        is_unit = true;
    }
    else {
        // PINHOLE (EQUIRECTANGULAR unsupported -> pinhole fallback)
        raydir = float3(uv.x, uv.y, 1.0f);
    }

    if (is_ray_depth)
        raydir = is_unit ? raydir : normalize(raydir);
    else
        raydir /= raydir.z;
    return raydir;
}

[ForceInline]
float3 generate_ray_d2n<D : ICameraDistortion>(
    const no_diff float2 pix_pos,
    const no_diff float4 intrins,
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth
) {
    float2 uv = (pix_pos - intrins.zw) / intrins.xy;
    float2 uv_u;
    if (!undistort_point<D>(uv, dist_coeffs, 12, uv_u)) {
        return float3(0);
    }

    return unproject_raydir(uv_u, camera_model, is_ray_depth);
}

[ForceInline]
[Differentiable]
float3 depth_to_point<D : ICameraDistortion>(
    const no_diff float2 pix_pos,
    const no_diff float4 intrins,  // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float depth
) {
    float2 uv = (pix_pos - intrins.zw) / intrins.xy;
    float2 uv_u;
    if (!no_diff undistort_point<D>(uv, dist_coeffs, 12, uv_u)) {
        return float3(0.0f);
    }

    return depth * unproject_raydir(uv_u, camera_model, is_ray_depth);
}

[ForceInline]
float depth_to_point_vjp<D : ICameraDistortion>(
    const no_diff float2 pix_pos,
    const no_diff float4 intrins,  // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float depth,
    const float3 v_point
) {
    DifferentialPair<float> dp_depth = diffPair(depth);
    bwd_diff(depth_to_point<D>)(
        pix_pos,
        intrins, dist_coeffs,
        camera_model, is_ray_depth,
        dp_depth, v_point
    );
    return dp_depth.d;
}

[CudaDeviceExport]
[Differentiable]
float3 points_to_normal(
    const float3 points[4] // x-, x+, y-, y+
) {
    if (dot(points[0], points[0]) == 0.0 ||
        dot(points[1], points[1]) == 0.0 ||
        dot(points[2], points[2]) == 0.0 ||
        dot(points[3], points[3]) == 0.0
    ) return float3(0.0);

    float3 dx = points[1] - points[0];
    float3 dy = points[3] - points[2];
    float3 normal = cross(dx, -dy);
    if (dot(normal, normal) != 0.0)
        normal /= length(normal);
    // normal *= -sign(dot(normal, points[0]+points[1]+points[2]+points[3]));
    return normal;
}

[CudaDeviceExport]
void points_to_normal_vjp(
    const float3 points[4], // x-, x+, y-, y+
    const float3 v_normal,
    out float3 v_points[4]
) {
    DifferentialPair<float3[4]> dp_points = diffPair(points);
    bwd_diff(points_to_normal)(
        dp_points, v_normal
    );
    v_points = dp_points.d;
}

[ForceInline]
[Differentiable]
float3 depth_to_normal<D : ICameraDistortion>(
    const no_diff float2 pix_center,
    const no_diff float4 intrins,  // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float4 depths  // x-, x+, y-, y+
) {
    if (depths.x == 0.0 || depths.y == 0.0 || depths.z == 0.0 || depths.w == 0.0)
        return float3(0.0);

    float3 points[4];

    [ForceUnroll]
    for (int i = 0; i < 4; i++) {
        float2 pix_pos = pix_center + float2(
            (2*i-1)*float(i < 2), (2*(i-2)-1)*float(i>=2)
        );

        float2 uv = (pix_pos - intrins.zw) / intrins.xy;
        float2 uv_u;
        if (!no_diff undistort_point<D>(uv, dist_coeffs, 12, uv_u)) {
            return float3(0.0f);
        }

        points[i] = depths[i] * unproject_raydir(uv_u, camera_model, is_ray_depth);
    }

    float3 dx = points[1] - points[0];
    float3 dy = points[3] - points[2];
    float3 normal = cross(dx, -dy);
    if (dot(normal, normal) != 0.0)
        normal /= length(normal);
    // normal *= -sign(dot(normal, points[0]+points[1]+points[2]+points[3]));
    return normal;
}

[ForceInline]
void depth_to_normal_vjp<D : ICameraDistortion>(
    const no_diff float2 pix_center,
    const no_diff float4 intrins,  // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float4 depths,  // x-, x+, y-, y+
    const float3 v_normal,
    out float4 v_depths
) {
    DifferentialPair<float4> dp_depths = diffPair(depths);
    bwd_diff(depth_to_normal<D>)(
        pix_center,
        intrins, dist_coeffs,
        camera_model, is_ray_depth,
        dp_depths, v_normal
    );
    v_depths = dp_depths.d;
}


[ForceInline]
[Differentiable]
float ray_depth_to_linear_depth_factor<D : ICameraDistortion>(
    const no_diff float2 pix_center,
    const no_diff float4 intrins,  // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model
) {

    float2 uv = (pix_center - intrins.zw) / intrins.xy;

    float2 uv_u;
    if (!no_diff undistort_point<D>(uv, dist_coeffs, 12, uv_u))
        return 0.0f;

    // ray scaled to z == 1; factor = linear_depth / ray_depth
    float3 raydir = unproject_raydir(uv_u, camera_model, false);

    return sign(raydir.z) / length(raydir);
}

[ForceInline]
[Differentiable]
float depth_normal_loss<D : ICameraDistortion>(
    const no_diff float2 pix_center,
    const no_diff float4 intrins, // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float4 depths, // x-, x+, y-, y+
    float3 gt_normal
) {
    float3 points[5];
    float3 raydir;

    [ForceUnroll]
    for (int i = 0; i <= 4; i++) {
        float2 pix_pos = pix_center + float(i != 4) * float2(
            (2 * i - 1) * float(i < 2), (2 * (i - 2) - 1) * float(i >= 2)
        );

        float2 uv = (pix_pos - intrins.zw) / intrins.xy;
        float2 uv_u;
        if (!no_diff undistort_point<D>(uv, dist_coeffs, 12, uv_u)) {
            return 0.0f;
        }

        raydir = unproject_raydir(uv_u, camera_model, is_ray_depth);

        if (i != 4)
            points[i] = depths[i] * raydir;
    }

    float3 dx = points[1] - points[0];
    float3 dy = points[3] - points[2];
    float3 normal = cross(dx, -dy);
    if (dot(normal, normal) != 0.0)
        normal = normalize(normal);
    if (dot(gt_normal, gt_normal) != 0.0)
        gt_normal = normalize(gt_normal);

    float num = 1.0 - dot(normal, gt_normal);
    float den = dot(normal, -normalize(raydir));
    constexpr float eps = 1e-3;
    return (num + eps) / (max(den, 0.0f) + eps);
}

[ForceInline]
void depth_normal_loss_vjp<D : ICameraDistortion>(
    const no_diff float2 pix_center,
    const no_diff float4 intrins, // fx, fy, cx, cy
    const no_diff float dist_coeffs[D.kNumParams],
    const int camera_model,
    const bool is_ray_depth,
    const float4 depths, // x-, x+, y-, y+
    const float3 gt_normal,
    const float v_loss,
    out float4 v_depths,
    out float3 v_gt_normal
) {
    DifferentialPair<float4> dp_depths = diffPair(depths);
    DifferentialPair<float3> dp_gt_normal = diffPair(gt_normal);
    bwd_diff(depth_normal_loss<D>)(
        pix_center,
        intrins, dist_coeffs,
        camera_model, is_ray_depth,
        dp_depths, dp_gt_normal, v_loss
    );
    v_depths = dp_depths.d;
    v_gt_normal = dp_gt_normal.d;
}


// ================
// Per-tier CUDA exports
// ================
//
// slangc cannot export a generic. These kernels take the camera model at
// runtime (one launch covers a whole batch) but the distortion tier at compile
// time, so there is one wrapper per tier and none per model.

#define _DEF_PIXELWISE_DIST_ENTRIES(suffix, D, NC) \
    [CudaDeviceExport] \
    float3 generate_ray_d2n##suffix( \
        const float2 pix_pos, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth \
    ) { \
        return generate_ray_d2n<D>(pix_pos, intrins, dist_coeffs, camera_model, is_ray_depth); \
    } \
    [CudaDeviceExport] \
    float3 depth_to_point##suffix( \
        const float2 pix_pos, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float depth \
    ) { \
        return depth_to_point<D>(pix_pos, intrins, dist_coeffs, camera_model, is_ray_depth, depth); \
    } \
    [CudaDeviceExport] \
    float depth_to_point_vjp##suffix( \
        const float2 pix_pos, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float depth, \
        const float3 v_point \
    ) { \
        return depth_to_point_vjp<D>(pix_pos, intrins, dist_coeffs, camera_model, \
                                     is_ray_depth, depth, v_point); \
    } \
    [CudaDeviceExport] \
    float3 depth_to_normal##suffix( \
        const float2 pix_center, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float4 depths \
    ) { \
        return depth_to_normal<D>(pix_center, intrins, dist_coeffs, camera_model, \
                                  is_ray_depth, depths); \
    } \
    [CudaDeviceExport] \
    void depth_to_normal_vjp##suffix( \
        const float2 pix_center, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float4 depths, \
        const float3 v_normal, out float4 v_depths \
    ) { \
        depth_to_normal_vjp<D>(pix_center, intrins, dist_coeffs, camera_model, \
                               is_ray_depth, depths, v_normal, v_depths); \
    } \
    [CudaDeviceExport] \
    float ray_depth_to_linear_depth_factor##suffix( \
        const float2 pix_center, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model \
    ) { \
        return ray_depth_to_linear_depth_factor<D>(pix_center, intrins, dist_coeffs, camera_model); \
    } \
    [CudaDeviceExport] \
    float depth_normal_loss##suffix( \
        const float2 pix_center, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float4 depths, \
        const float3 gt_normal \
    ) { \
        return depth_normal_loss<D>(pix_center, intrins, dist_coeffs, camera_model, \
                                    is_ray_depth, depths, gt_normal); \
    } \
    [CudaDeviceExport] \
    void depth_normal_loss_vjp##suffix( \
        const float2 pix_center, const float4 intrins, const float dist_coeffs[NC], \
        const int camera_model, const bool is_ray_depth, const float4 depths, \
        const float3 gt_normal, const float v_loss, \
        out float4 v_depths, out float3 v_gt_normal \
    ) { \
        depth_normal_loss_vjp<D>(pix_center, intrins, dist_coeffs, camera_model, \
                                 is_ray_depth, depths, gt_normal, v_loss, \
                                 v_depths, v_gt_normal); \
    }

_DEF_PIXELWISE_DIST_ENTRIES(_none, DistNone, 1)
_DEF_PIXELWISE_DIST_ENTRIES(_opencv, DistOpenCV, 4)
_DEF_PIXELWISE_DIST_ENTRIES(_prism, DistThinPrism, 8)
_DEF_PIXELWISE_DIST_ENTRIES(_rational, DistRational, 8)

#undef _DEF_PIXELWISE_DIST_ENTRIES
