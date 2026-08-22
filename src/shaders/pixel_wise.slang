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

[Differentiable]
[CudaDeviceExport]
float3 blend_background(float3 rgb, float transmittance, float3 background) {
    // Explicit splat: slangc 2026.12 (SPIR-V) constant-folds the SCALAR's
    // derivative of `scalar * vector` to zero under bwd_diff, silently
    // zeroing v_transmittance. float3(t, t, t) * vector keeps the gradient
    // on both compilers (identical primal math).
    return clamp(rgb + float3(transmittance, transmittance, transmittance)
                           * background, 0.0, 1.0);
}

[CudaDeviceExport]
// Straight through the forward's clamp: the derivative of rgb + T*background,
// as if it had not clamped. The clamp's own derivative is a ratchet -- zero
// exactly where a pixel overshot, so nothing tells it to come back.
void blend_background_bwd(
    float3 rgb, float transmittance, float3 background,
    float3 v_out_rgb,
    out float3 v_rgb, out float v_transmittance, out float3 v_background
) {
    v_rgb = v_out_rgb;
    v_transmittance = dot(v_out_rgb, background);
    v_background = float3(transmittance, transmittance, transmittance) * v_out_rgb;
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
    const float a = 1.0f / 255.0f;            // sqrt of the black floor
    float s = sqrt(0.5f + a * a);
    float u = kSh0 * dc + 0.5f;
    float w = u >= 0.0f ? 2.0f * sqrt(u + a * a) : 2.0f * a + u / a;
    return (w - 2.0f * s) * s / kSh0;
}

[ForceInline]
[CudaDeviceExport]
float splat_dc_decode(float x) {
    const float kSh0 = 0.28209479177387814f;
    const float a = 1.0f / 255.0f;
    float s = sqrt(0.5f + a * a);
    float w = x * kSh0 / s + 2.0f * s;
    float u = w >= 2.0f * a ? 0.25f * w * w - a * a : (w - 2.0f * a) * a;
    return (u - 0.5f) / kSh0;
}

[Differentiable]
[CudaDeviceExport]
float3 linear_rgb_to_srgb(float3 rgb, no_diff float3x3 color_matrix) {
    rgb = mul(color_matrix, rgb);
    return float3(
        linear_rgb_to_srgb(rgb.x),
        linear_rgb_to_srgb(rgb.y),
        linear_rgb_to_srgb(rgb.z)
    );
}

[CudaDeviceExport]
float3 linear_rgb_to_srgb_bwd(
    float3 rgb, float3x3 color_matrix, float3 v_out_rgb
) {
    var p_rgb = diffPair(rgb);
    bwd_diff(linear_rgb_to_srgb)(
        p_rgb, color_matrix,
        v_out_rgb
    );
    return p_rgb.d;
}

[Differentiable]
[CudaDeviceExport]
float3 rgb_to_srgb(float3 rgb, no_diff float3x3 color_matrix) {
    rgb = float3(
        srgb_to_linear_rgb(rgb.x),
        srgb_to_linear_rgb(rgb.y),
        srgb_to_linear_rgb(rgb.z)
    );
    rgb = mul(color_matrix, rgb);
    return float3(
        linear_rgb_to_srgb(rgb.x),
        linear_rgb_to_srgb(rgb.y),
        linear_rgb_to_srgb(rgb.z)
    );
}

[CudaDeviceExport]
float3 rgb_to_srgb_bwd(
    float3 rgb, float3x3 color_matrix, float3 v_out_rgb
) {
    var p_rgb = diffPair(rgb);
    bwd_diff(rgb_to_srgb)(
        p_rgb, color_matrix,
        v_out_rgb
    );
    return p_rgb.d;
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
