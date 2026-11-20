#pragma once
// ^ not working for [CudaDeviceExport]

#ifndef _PROJECTION_UTILS_SLANG_INC

#define _PROJECTION_UTILS_SLANG_INC


static const float ALPHA_THRESHOLD = 1.0f/255.0f;

// Below this squared cosine a ray misses a surfel's plane; ~0.06 degrees.
static const float SURFEL_GRAZING_COS2 = 1e-6f;

// This must match Common.cuh
enum CameraModelType {
    PINHOLE = 0,
    FISHEYE = 1,
    EQUISOLID = 2,
    EQUIRECTANGULAR = 3,
    ORTHOGRAPHIC = 4,
};

// https://www.shadertoy.com/view/4djSRW

#define fract(x) ((x)-floor(x))

[ForceInline]
float hash14(in float4 p4) {
    p4 = fract(p4 * float4(.1031, .1030, .0973, .1099));
    p4 += dot(p4, p4.wzxy + 33.33);
    return fract((p4.x + p4.y) * (p4.z + p4.w));
}

[ForceInline]
float2 hash24(in float4 p4) {
    p4 = fract(p4 * float4(.1031, .1030, .0973, .1099));
    p4 += dot(p4, p4.wzxy + 33.33);
    return fract((p4.xz + p4.yw) * p4.wx);
}

[ForceInline]
float4 hash44(in float4 p4) {
    p4 = fract(p4 * float4(.1031, .1030, .0973, .1099));
    p4 += dot(p4, p4.wzxy + 33.33);
    return fract((p4.xxyz + p4.yzzw) * p4.zywx);
}

#undef fract

[ForceInline]
float2 box_muller(in float2 uv) {
    return sqrt(-2.0f * log(1.0 - uv.x)) * float2(
        cos(6.28318530718 * uv.y),
        sin(6.28318530718 * uv.y)
    );
}

[ForceInline]
float2 randn2(in float4 seed) {
    float2 val = hash24(seed);
    return box_muller(val);
}

[ForceInline]
float3 randn3(in float4 seed) {
    float4 val = hash44(seed);
    return float3(
        box_muller(val.xz),
        box_muller(val.yw).x
    );
}

[CudaDeviceExport]
[ForceInline]
[Differentiable]
float3x3 normalized_quat_to_rotmat(const float4 quat) {
    float w = quat[0], x = quat[1], y = quat[2], z = quat[3];
    float x2 = x * x, y2 = y * y, z2 = z * z;
    float xy = x * y, xz = x * z, yz = y * z;
    float wx = w * x, wy = w * y, wz = w * z;
    return transpose(float3x3(
        (1.f - 2.f * (y2 + z2)),
        (2.f * (xy + wz)),
        (2.f * (xz - wy)), // 1st col
        (2.f * (xy - wz)),
        (1.f - 2.f * (x2 + z2)),
        (2.f * (yz + wx)), // 2nd col
        (2.f * (xz + wy)),
        (2.f * (yz - wx)),
        (1.f - 2.f * (x2 + y2)) // 3rd col
    ));
}

[ForceInline]
[Differentiable]
void posW2C(
    const float3x3 R,
    const float3 t,
    const float3 pW, // Input position in world coordinates
    out float3 pC       // Output position in camera coordinates
) {
    pC = mul(R, pW) + t;
}

[ForceInline]
[Differentiable]
void covarW2C(
    const float3x3 R,
    const float3x3 covarW, // Input covariance matrix in world coordinates
    out float3x3 covarC       // Output covariance matrix in camera coordinates
) {
    covarC = mul(mul(R, covarW), transpose(R));
}

[CudaDeviceExport]
[ForceInline]
[Differentiable]
void quat_scale_to_covar(
    const float4 quat,
    const float3 scale,
    out float3x3 covar
) {
    float3x3 R = normalized_quat_to_rotmat(quat);
    // C = R * S * S * Rt
    float3x3 S = float3x3(scale[0], 0.f, 0.f, 0.f, scale[1], 0.f, 0.f, 0.f, scale[2]);
    float3x3 M = mul(R, S);
    covar = mul(M, transpose(M));
}

[CudaDeviceExport]
[ForceInline]
[Differentiable]
void quat_scale_to_sqrt_covar(
    const float4 quat,
    const float3 scale,
    out float3x3 M
) {
    float3x3 R = normalized_quat_to_rotmat(quat);
    float3x3 S = float3x3(scale[0], 0.f, 0.f, 0.f, scale[1], 0.f, 0.f, 0.f, scale[2]);
    M = mul(R, S);
}

[CudaDeviceExport]
[ForceInline]
[Differentiable]
float3 apply_sqrt_covar_to_vec(
    const float4 quat,
    const float3 scale,
    const float3 vec
) {
    float3x3 R = normalized_quat_to_rotmat(quat);
    return mul(R, scale * vec);
}

[CudaDeviceExport]
[ForceInline]
[Differentiable]
float3 apply_covar_to_vec(
    const float4 quat,
    const float3 scale,
    const float3 vec
) {
    float3x3 M;
    quat_scale_to_covar(quat, scale, M);
    return mul(M, vec);
}

[ForceInline]
[Differentiable]
float2x2 inverse(float2x2 m){
    float invdet = 1.0f / (m[0][0] * m[1][1] - m[0][1] * m[1][0]);
    return {
        m[1][1] * invdet,
        -m[0][1] * invdet,
        -m[1][0] * invdet,
        m[0][0] * invdet
    };
}

[ForceInline]
[Differentiable]
float det(float2 u, float2 v) {
    return u.x*v.y - u.y*v.x;
}

[ForceInline]
[Differentiable]
void sv_coeffs_to_color<let num_sv : int>(
    float3 viewdir,
    const float3 sv_sites[num_sv],
    const float3 sv_colors[num_sv],
    out float3 color
) {
    viewdir *= rsqrt(dot(viewdir, viewdir));

    float maxdot = dot(sv_sites[0], viewdir);
    [ForceUnroll] for (int i = 1; i < num_sv; i++) {
        maxdot = dot(sv_sites[i], viewdir);
    }

    float total_w = 0.0;
    float3 total_c = float3(0.0);
    [ForceUnroll] for (int i = 0; i < num_sv; i++) {
        float w = exp(dot(sv_sites[i], viewdir) - maxdot);
        total_w += w;
        total_c += w * sv_colors[i];
    }

    color = total_c / total_w;
}

// ================
// Lens distortion tiers
// ================
//
// The tier is a compile-time choice on both backends (CUDA template argument,
// Vulkan specialization constant) and is ORTHOGONAL to CameraModelType: it acts
// on whatever normalized coordinates the camera model produced -- (x/z, y/z)
// for PINHOLE, theta-space for FISHEYE / EQUISOLID -- which is the order COLMAP
// applies it in too. EQUIRECTANGULAR carries no lens distortion.
//
// Coefficient order is each tier's own; a slot index does NOT mean the same
// thing across tiers. Must match CameraDistortionType in core/CameraModel.h.
enum CameraDistortionType {
    DIST_NONE = 0,
    DIST_OPENCV = 1,
    DIST_THIN_PRISM = 2,
};

interface ICameraDistortion {
    static const int kNumParams;
    static const bool kIsIdentity;
    [Differentiable]
    static float2 distort(float2 uv, no_diff float coeffs[kNumParams]);
};

// Slang has no zero-length array, so the identity tier carries one unread slot.
struct DistNone : ICameraDistortion {
    static const int kNumParams = 1;
    static const bool kIsIdentity = true;
    [Differentiable]
    static float2 distort(float2 uv, no_diff float coeffs[1]) { return uv; }
};

// k1 k2 p1 p2 -- COLMAP OPENCV, and the radial-only models zero-extended into it.
struct DistOpenCV : ICameraDistortion {
    static const int kNumParams = 4;
    static const bool kIsIdentity = false;
    [Differentiable]
    static float2 distort(float2 uv, no_diff float coeffs[4]) {
        float k1 = coeffs[0], k2 = coeffs[1], p1 = coeffs[2], p2 = coeffs[3];
        float u = uv.x, v = uv.y;
        float r2 = u*u + v*v;
        float radial = 1.0 + r2*(k1 + r2*k2);
        return uv * radial + float2(
            2.0*p1*u*v + p2*(r2 + 2.0*u*u),
            2.0*p2*u*v + p1*(r2 + 2.0*v*v));
    }
};

// k1 k2 k3 k4 p1 p2 sx1 sy1 -- COLMAP THIN_PRISM_FISHEYE, radial-first
// (COLMAP's own file order interleaves p1,p2 between k2 and k3).
struct DistThinPrism : ICameraDistortion {
    static const int kNumParams = 8;
    static const bool kIsIdentity = false;
    [Differentiable]
    static float2 distort(float2 uv, no_diff float coeffs[8]) {
        float k1 = coeffs[0], k2 = coeffs[1], k3 = coeffs[2], k4 = coeffs[3],
              p1 = coeffs[4], p2 = coeffs[5], sx1 = coeffs[6], sy1 = coeffs[7];
        float u = uv.x, v = uv.y;
        float r2 = u*u + v*v;
        float radial = 1.0 + r2*(k1 + r2*(k2 + r2*(k3 + r2*k4)));
        return uv * radial + float2(
            2.0*p1*u*v + p2*(r2 + 2.0*u*u) + sx1*r2,
            2.0*p2*u*v + p1*(r2 + 2.0*v*v) + sy1*r2);
    }
};


[ForceInline]
[Differentiable]
float2x2 camera_distortion_jac<D : ICameraDistortion>(
    float2 uv, no_diff float dist_coeffs[D.kNumParams]
) {
    DifferentialPair<float2> dp_uv_x = diffPair(uv, float2(1, 0));
    float2 j_x = fwd_diff(D.distort)(dp_uv_x, dist_coeffs).d;
    DifferentialPair<float2> dp_uv_y = diffPair(uv, float2(0, 1));
    float2 j_y = fwd_diff(D.distort)(dp_uv_y, dist_coeffs).d;
    return transpose(float2x2(j_x, j_y));
}

[ForceInline]
bool is_valid_distortion<D : ICameraDistortion>(
    no_diff float2 uv, no_diff float dist_coeffs[D.kNumParams]
) {
    if (D.kIsIdentity)
        return true;
    // TODO: doesn't always work for more than 2 radial coeffs
    float2x2 J = camera_distortion_jac<D>(uv, dist_coeffs);
    float Jdet = min(determinant(J), min(J[0][0], J[1][1]));
    // return Jdet > 0.0f;
    return Jdet > 0.25f && Jdet < 4.0f && dot(uv, D.distort(uv, dist_coeffs)) >= 0.0f;
}



// Value-returning core (assume-valid path). The bool wrapper below and the
// fwd_diff Jacobians both build on this: slangc crashes on bwd-of-fwd_diff
// through `out`-parameter differentiable functions (shader-slang/slang
// #11160), and the documented workaround is returning the value instead.
[ForceInline]
[Differentiable]
float2 persp_proj_uv<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2 uv = p_view.xy / p_view.z;
    uv = D.distort(uv, dist_coeffs);
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;
    return float2(
        fx * uv.x + cx,
        fy * uv.y + cy
    );
}

[ForceInline]
[Differentiable]
bool persp_proj<D : ICameraDistortion, let assume_valid : bool>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams],
    out float2 uv
) {
    if (!assume_valid) {
        float2 uv0 = p_view.xy / p_view.z;
        if (p_view.z < 0.0 || !is_valid_distortion<D>(uv0, dist_coeffs)) {
            uv = uv0;
            return false;
        }
    }
    uv = persp_proj_uv<D>(p_view, intrins, dist_coeffs);
    return true;
}

[ForceInline]
[Differentiable]
float2x3 persp_proj_jac<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2x3 J;
    [ForceUnroll]
    for (int i = 0; i < 3; ++i) {
        DifferentialPair<float3> dp_p_view = diffPair(p_view,
            float3(float(i == 0), float(i == 1), float(i == 2)));
        float2 duv = fwd_diff(persp_proj_uv<D>)(dp_p_view, intrins, dist_coeffs).d;
        J[0][i] = duv.x;
        J[1][i] = duv.y;
    }
    return J;
}

[ForceInline]
[Differentiable]
float2 fisheye_proj(float3 p_view) {
    float r = length(p_view.xy);
    float theta = atan2(r, p_view.z);
    float k = theta < 1e-3 ? (1.0 - theta*theta/3.0) / p_view.z : theta / r;
    return p_view.xy * k;
}

[ForceInline]
[Differentiable]
float2 equisolid_proj(float3 p_view) {
    float r = length(p_view.xy);
    float theta = atan2(r, p_view.z);
    float k = r < 1e-6
        ? (1.0 - theta*theta/24.0) / p_view.z
        : (2.0 * sin(0.5 * theta)) / r;
    return p_view.xy * k;
}

// Value-returning core; see persp_proj_uv for why (slang #11160).
[ForceInline]
[Differentiable]
float2 fisheye_proj_uv<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2 uv = fisheye_proj(p_view);
    uv = D.distort(uv, dist_coeffs);
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;
    return float2(
        fx * uv.x + cx,
        fy * uv.y + cy
    );
}

[ForceInline]
[Differentiable]
bool fisheye_proj<D : ICameraDistortion, let assume_valid : bool>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams],
    out float2 uv
) {
    if (!assume_valid) {
        float2 uv0 = fisheye_proj(p_view);
        if (!is_valid_distortion<D>(uv0, dist_coeffs)) {
            uv = uv0;
            return false;
        }
    }
    uv = fisheye_proj_uv<D>(p_view, intrins, dist_coeffs);
    return true;
}

// Value-returning core; see persp_proj_uv for why (slang #11160).
[ForceInline]
[Differentiable]
float2 equisolid_proj_uv<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2 uv = equisolid_proj(p_view);
    uv = D.distort(uv, dist_coeffs);
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;
    return float2(
        fx * uv.x + cx,
        fy * uv.y + cy
    );
}

[ForceInline]
[Differentiable]
bool equisolid_proj<D : ICameraDistortion, let assume_valid : bool>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams],
    out float2 uv
) {
    if (!assume_valid) {
        float2 uv0 = equisolid_proj(p_view);
        if (!is_valid_distortion<D>(uv0, dist_coeffs)) {
            uv = uv0;
            return false;
        }
    }
    uv = equisolid_proj_uv<D>(p_view, intrins, dist_coeffs);
    return true;
}

[ForceInline]
[Differentiable]
float2x3 fisheye_proj_jac<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2x3 J;
    [ForceUnroll]
    for (int i = 0; i < 3; ++i) {
        DifferentialPair<float3> dp_p_view = diffPair(p_view,
            float3(float(i == 0), float(i == 1), float(i == 2)));
        float2 duv = fwd_diff(fisheye_proj_uv<D>)(dp_p_view, intrins, dist_coeffs).d;
        J[0][i] = duv.x;
        J[1][i] = duv.y;
    }
    return J;
}

[ForceInline]
[Differentiable]
float2x3 equisolid_proj_jac<D : ICameraDistortion>(
    float3 p_view, no_diff float4 intrins, no_diff float dist_coeffs[D.kNumParams]
) {
    float2x3 J;
    [ForceUnroll]
    for (int i = 0; i < 3; ++i) {
        DifferentialPair<float3> dp_p_view = diffPair(p_view,
            float3(float(i == 0), float(i == 1), float(i == 2)));
        float2 duv = fwd_diff(equisolid_proj_uv<D>)(dp_p_view, intrins, dist_coeffs).d;
        J[0][i] = duv.x;
        J[1][i] = duv.y;
    }
    return J;
}

// Equirectangular (spherical panorama) projection.
// Convention matches warp_image_equirectangular_to_pinhole_kernel in ImageWarp.cu:
//   lon = atan2(x, z), lat = atan2(y, hypot(x, z)),
//   u = fx*lon + cx, v = fy*lat + cy,  with fx = fy = width/(2*pi), cx = w/2, cy = h/2.
// Carries no lens distortion, so unlike the other models it takes no
// coefficients. Every viewing direction is valid, so the whole sphere projects;
// the atan2 seam at lon = +-pi and the poles at lat = +-pi/2 are the expected
// sources of nonlinearity/discontinuity.
[ForceInline]
[Differentiable]
float2 equirect_proj(float3 p_view) {
    float lon = atan2(p_view.x, p_view.z);
    float lat = atan2(p_view.y, length(p_view.xz));
    return float2(lon, lat);
}

// Value-returning core; see persp_proj_uv for why (slang #11160).
[ForceInline]
[Differentiable]
float2 equirect_proj_uv(float3 p_view, no_diff float4 intrins) {
    float2 lonlat = equirect_proj(p_view);
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;
    return float2(fx * lonlat.x + cx, fy * lonlat.y + cy);
}

[ForceInline]
[Differentiable]
bool equirect_proj<let assume_valid : bool>(
    float3 p_view, no_diff float4 intrins, out float2 uv
) {
    uv = equirect_proj_uv(p_view, intrins);
    return true;
}

[CudaDeviceExport]
bool equirect_proj_nav(float3 p_view, no_diff float4 intrins, out float2 uv) {
    return equirect_proj<false>(p_view, intrins, uv);
}

[ForceInline]
[Differentiable]
[CudaDeviceExport]
float2x3 equirect_proj_jac(float3 p_view, no_diff float4 intrins) {
    float2x3 J;
    [ForceUnroll]
    for (int i = 0; i < 3; ++i) {
        DifferentialPair<float3> dp_p_view = diffPair(p_view,
            float3(float(i == 0), float(i == 1), float(i == 2)));
        float2 duv = fwd_diff(equirect_proj_uv)(dp_p_view, intrins).d;
        J[0][i] = duv.x;
        J[1][i] = duv.y;
    }
    return J;
}

// Inverse of equirect_proj: (lon, lat) angles -> unit ray direction.
[ForceInline]
[Differentiable]
float3 equirect_unproj(float2 lonlat) {
    float lon = lonlat.x, lat = lonlat.y;
    float cl = cos(lat);
    return float3(cl * sin(lon), sin(lat), cl * cos(lon));
}

[ForceInline]
[Differentiable]
float3 fisheye_unproj(float2 uv) {
    float theta = length(uv);
    return float3(
        uv * (theta < 1e-3 ? 1.0-theta*theta/6.0 : sin(theta) / theta),
        cos(theta)
    );
}

[ForceInline]
[Differentiable]
float3 equisolid_unproj(float2 uv) {
    float r = length(uv);
    float z = 1.0 - 0.5 * r * r;
    float xy_scale = sqrt(max(0.0, 1.0 - 0.25 * r * r));
    return float3(uv * xy_scale, z);
}

[ForceInline]
[Differentiable]
float2 _solve2x2(float2x2 A, float2 b) {
    float inv_det = 1.0 / (A[0][0]*A[1][1] - A[0][1]*A[1][0]);
    return float2(
        ( b.x*A[1][1] - b.y*A[0][1]) * inv_det,
        (-b.x*A[1][0] + b.y*A[0][0]) * inv_det
    );
}

// find uv' s.t. distort(uv') = uv
bool undistort_point<D : ICameraDistortion>(
    no_diff float2 uv, no_diff float dist_coeffs[D.kNumParams],
    int maxiter, out float2 uv_undist
) {
    if (D.kIsIdentity) {
        uv_undist = uv;
        return true;
    }

    float2 q = uv;
    for (int i = 0; i < maxiter; i++) {
        float2 qd = D.distort(q, dist_coeffs);
        float2 r = qd - uv;
        float2x2 J = camera_distortion_jac<D>(q, dist_coeffs);
        q -= _solve2x2(J, r);
    }

    uv_undist = q;
    // TODO: better way to tell so
    return is_valid_distortion<D>(uv_undist, dist_coeffs) &&
           length(D.distort(uv_undist, dist_coeffs) - uv) < 1e-2;
}



// The model-aware helpers below take the camera model as a RUNTIME int (the
// pixel-wise kernels dispatch per camera, not per launch); only the distortion
// tier is compile-time.

[ForceInline]
[Differentiable]
float2 distort_point<D : ICameraDistortion>(
    no_diff float2 uv,
    no_diff int camera_model,
    no_diff float dist_coeffs[D.kNumParams]
) {
    CameraModelType model = (CameraModelType)camera_model;

    // Equirectangular has no lens distortion model: uv already holds (lon, lat).
    if (model == CameraModelType::EQUIRECTANGULAR)
        return uv;

    if (model == CameraModelType::FISHEYE) {
        float r = length(uv);
        float theta = atan(r);
        float k = r < 1e-3 ? (1.0 - theta * theta / 6.0) : (theta / r);
        uv *= k;
    }
    else if (model == CameraModelType::EQUISOLID) {
        float r = length(uv);
        float theta = atan(r);
        float k = r < 1e-3 ? (1.0 - theta * theta / 24.0) : (2.0 * sin(0.5 * theta) / r);
        uv *= k;
    }

    return D.distort(uv, dist_coeffs);
}

[ForceInline]
[Differentiable]
bool undistort_point_model<D : ICameraDistortion>(
    no_diff float2 uv,
    no_diff int camera_model,
    no_diff float dist_coeffs[D.kNumParams],
    out float2 uv_undist
) {
    CameraModelType model = (CameraModelType)camera_model;
    uv_undist = float2(0.0f);

    // Equirectangular: uv already holds (lon, lat), no distortion to invert.
    // The "undistorted" normalized coord is the perspective projection of the ray.
    if (model == CameraModelType::EQUIRECTANGULAR) {
        float3 rd = equirect_unproj(uv);
        uv_undist = rd.xy / max(rd.z, 1e-12);
        return true;
    }

    float2 uv_u;
    if (!no_diff undistort_point<D>(uv, dist_coeffs, 8, uv_u))
        return false;
    uv = uv_u;

    float3 raydir;
    if (model == CameraModelType::FISHEYE) {
        float r = length(uv);
        float theta = r;
        float s = r < 1e-3 ? (1.0 - theta * theta / 6.0) : (sin(theta) / r);
        raydir = float3(uv * s, cos(theta));
    }
    else if (model == CameraModelType::EQUISOLID) {
        float r = length(uv);
        float z = 1.0 - 0.5 * r * r;
        float s = sqrt(max(0.0, 1.0 - 0.25 * r * r));
        raydir = float3(uv * s, z);
    }
    else {
        raydir = float3(uv, 1.0);
    }

    uv_undist = raydir.xy / max(raydir.z, 1e-12);
    return true;
}

[ForceInline]
bool unproject_point<D : ICameraDistortion>(
    no_diff float2 uv,
    no_diff int camera_model,
    no_diff float dist_coeffs[D.kNumParams],
    out float3 raydir
) {
    CameraModelType model = (CameraModelType)camera_model;
    if (model == CameraModelType::ORTHOGRAPHIC) {
        raydir = float3(0, 0, 1);
        return true;
    }


    raydir = float3(0);

    // Equirectangular: uv holds (lon, lat); map straight to a direction.
    if (model == CameraModelType::EQUIRECTANGULAR) {
        raydir = equirect_unproj(uv);
        return true;
    }

    float2 uv_u;
    if (!undistort_point<D>(uv, dist_coeffs, 8, uv_u))
        return false;
    uv = uv_u;

    if (model == CameraModelType::FISHEYE) {
        float r = length(uv);
        float theta = r;
        float s = r < 1e-3 ? (1.0 - theta * theta / 6.0) : (sin(theta) / r);
        raydir = float3(uv * s, cos(theta));
    }
    else if (model == CameraModelType::EQUISOLID) {
        float r = length(uv);
        float z = 1.0 - 0.5 * r * r;
        float s = sqrt(max(0.0, 1.0 - 0.25 * r * r));
        raydir = float3(uv * s, z);
    }
    else {
        raydir = float3(uv, 1.0);
    }

    return true;
}

[ForceInline]
[Differentiable]
bool generate_ray<D : ICameraDistortion>(
    no_diff float2 uv,
    no_diff int camera_model,
    no_diff float dist_coeffs[D.kNumParams],
    out float3 raydir
) {
    CameraModelType model = (CameraModelType)camera_model;
    if (model == CameraModelType::ORTHOGRAPHIC) {
        raydir = float3(0, 0, 1);
        return true;
    }


    // Equirectangular: uv holds (lon, lat). Reject pixels outside the valid
    // angular range so black borders (partial-vertical-FOV panoramas) don't
    // wrap; otherwise map (lon, lat) straight to a unit ray.
    if (model == CameraModelType::EQUIRECTANGULAR) {
        if (abs(uv.x) > 3.14159265358979 || abs(uv.y) > 1.57079632679490) {
            raydir = float3(0);
            return false;
        }
        raydir = equirect_unproj(uv);  // already unit length
        return true;
    }

    float2 uv_u;
    if (!no_diff undistort_point<D>(uv, dist_coeffs, 8, uv_u)) {
        raydir = float3(0);
        return false;
    }
    uv = uv_u;

    if (model == CameraModelType::FISHEYE) {
        float r = length(uv);
        if (r >= 3.14159265358979) {
            raydir = float3(0);
            return false;
        }
        float theta = r;
        float s = r < 1e-3 ? (1.0 - theta * theta / 6.0) : (sin(theta) / r);
        raydir = float3(uv * s, cos(theta));
    }
    else if (model == CameraModelType::EQUISOLID) {
        float r = length(uv);
        if (r >= 2.0) {
            raydir = float3(0);
            return false;
        }
        float z = 1.0 - 0.5 * r * r;
        float s = sqrt(max(0.0, 1.0 - 0.25 * r * r));
        raydir = float3(uv * s, z);
    }
    else {
        raydir = float3(uv, 1.0);
    }

    raydir = normalize(raydir);
    return true;
}


// Four camera-space rays, in image order, whose quad contains every ray of the
// pixel rectangle [lo, hi]: a lens bends its edges, so one that bows out moves to
// twice the bow of its midpoint. False where the lens model lacks a ray.
bool pixel_rect_quad<D : ICameraDistortion>(
    int camera_model, float4 intrins, float dist_coeffs[D.kNumParams],
    float2 lo, float2 hi, out float3 q0, out float3 q1, out float3 q2, out float3 q3
) {
    q0 = float3(0.0f); q1 = q0; q2 = q0; q3 = q0;
    float2 c[4] = { lo, float2(hi.x, lo.y), hi, float2(lo.x, hi.y) };
    float3 r[4];
    float3 n[4];
    bool ok = true;
    [ForceUnroll] for (int k = 0; k < 4; k++)
        ok = ok && generate_ray<D>((c[k] - intrins.zw) / intrins.xy, camera_model,
                                   dist_coeffs, r[k]);
    // Past 60 degrees across, four rays stop bounding what lies between them.
    ok = ok && dot(r[0], r[2]) > 0.5f && dot(r[1], r[3]) > 0.5f;
    [ForceUnroll] for (int k = 0; k < 4; k++) {
        float3 mid;
        ok = ok && generate_ray<D>((0.5f * (c[k] + c[(k + 1) & 3]) - intrins.zw) / intrins.xy,
                                   camera_model, dist_coeffs, mid);
        // The edge's plane, its normal pointing into the quad.
        float3 along = r[(k + 1) & 3] - r[k];
        n[k] = cross(r[k], along);
        float3 chord = normalize(r[k] + r[(k + 1) & 3]);
        if (dot(n[k], mid) < 0.0f)
            n[k] = cross(mid + (mid - chord), along);
    }
    if (!ok)
        return false;
    // A corner is where two edge planes meet; an edge shrunk to a point (the
    // pole of a panorama) has none, and keeps its own ray.
    float3 q[4];
    [ForceUnroll] for (int k = 0; k < 4; k++) {
        float3 x = cross(n[(k + 3) & 3], n[k]);
        float xx = dot(x, x);
        q[k] = xx > 1e-20f ? x * (rsqrt(xx) * (dot(x, r[k]) < 0.0f ? -1.0f : 1.0f)) : r[k];
    }
    q0 = q[0]; q1 = q[1]; q2 = q[2]; q3 = q[3];
    return true;
}


// ================
// Per-tier CUDA exports
// ================
//
// slangc cannot export a generic, so each tier gets a thin wrapper. The CUDA
// side selects one at compile time through SlangDistortion<D> in
// core/CameraDistortion.cuh; the Vulkan side calls the generics directly and
// picks the tier with the kDistortion specialization constant.

#define _DEF_DIST_ENTRIES(suffix, D, NC) \
    [CudaDeviceExport] \
    bool is_valid_distortion##suffix(float2 uv, float dist_coeffs[NC]) { \
        return is_valid_distortion<D>(uv, dist_coeffs); \
    } \
    [CudaDeviceExport] \
    bool persp_proj_nav##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC], out float2 uv \
    ) { return persp_proj<D, false>(p_view, intrins, dist_coeffs, uv); } \
    [CudaDeviceExport] \
    bool fisheye_proj_nav##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC], out float2 uv \
    ) { return fisheye_proj<D, false>(p_view, intrins, dist_coeffs, uv); } \
    [CudaDeviceExport] \
    bool equisolid_proj_nav##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC], out float2 uv \
    ) { return equisolid_proj<D, false>(p_view, intrins, dist_coeffs, uv); } \
    [CudaDeviceExport] \
    float2x3 persp_proj_jac##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC] \
    ) { return persp_proj_jac<D>(p_view, intrins, dist_coeffs); } \
    [CudaDeviceExport] \
    float2x3 fisheye_proj_jac##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC] \
    ) { return fisheye_proj_jac<D>(p_view, intrins, dist_coeffs); } \
    [CudaDeviceExport] \
    float2x3 equisolid_proj_jac##suffix( \
        float3 p_view, float4 intrins, float dist_coeffs[NC] \
    ) { return equisolid_proj_jac<D>(p_view, intrins, dist_coeffs); } \
    [CudaDeviceExport] \
    float2 distort_point##suffix( \
        float2 uv, int camera_model, float dist_coeffs[NC] \
    ) { return distort_point<D>(uv, camera_model, dist_coeffs); } \
    [CudaDeviceExport] \
    bool undistort_point##suffix( \
        float2 uv, int camera_model, float dist_coeffs[NC], out float2 uv_undist \
    ) { return undistort_point_model<D>(uv, camera_model, dist_coeffs, uv_undist); } \
    [CudaDeviceExport] \
    bool unproject_point##suffix( \
        float2 uv, int camera_model, float dist_coeffs[NC], out float3 raydir \
    ) { return unproject_point<D>(uv, camera_model, dist_coeffs, raydir); } \
    [CudaDeviceExport] \
    bool generate_ray##suffix( \
        float2 uv, int camera_model, float dist_coeffs[NC], out float3 raydir \
    ) { return generate_ray<D>(uv, camera_model, dist_coeffs, raydir); } \
    [CudaDeviceExport] \
    bool pixel_rect_quad##suffix( \
        int camera_model, float4 intrins, float dist_coeffs[NC], float2 lo, float2 hi, \
        out float3 q0, out float3 q1, out float3 q2, out float3 q3 \
    ) { return pixel_rect_quad<D>(camera_model, intrins, dist_coeffs, lo, hi, q0, q1, q2, q3); }

_DEF_DIST_ENTRIES(_none, DistNone, 1)
_DEF_DIST_ENTRIES(_opencv, DistOpenCV, 4)
_DEF_DIST_ENTRIES(_prism, DistThinPrism, 8)

#undef _DEF_DIST_ENTRIES


[CudaDeviceExport]
[Differentiable]
float3 transform_ray_o(float3x3 R, float3 t) {
    return -mul(t, R);
}

[CudaDeviceExport]
[Differentiable]
float3 transform_ray_d(float3x3 R, float3 raydir) {
    return mul(raydir, R);
}

[CudaDeviceExport]
float3 undo_transform_ray_d(float3x3 R, float3 raydir) {
    return mul(raydir, transpose(R));
}

[CudaDeviceExport]
void transform_ray_o_vjp(
    float3x3 R, float3 t, float3 v_ray_o,
    out float3x3 v_R, out float3 v_t
) {
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(transform_ray_o)(dp_R, dp_t, v_ray_o);
    v_R = dp_R.d;
    v_t = dp_t.d;
}

[CudaDeviceExport]
void transform_ray_d_vjp(
    float3x3 R, float3 raydir, float3 v_ray_d,
    out float3x3 v_R, out float3 v_raydir
) {
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_raydir = diffPair(raydir);
    bwd_diff(transform_ray_d)(
        dp_R, dp_raydir,
        v_ray_d
    );
    v_R = dp_R.d;
    v_raydir = dp_raydir.d;
}


[CudaDeviceExport]
[Differentiable]
float3x3 compute_3dgut_iscl_rot(float4 quat, float3 scale) {
    // float3x3 R = normalized_quat_to_rotmat(normalize(quat));
    float3x3 R = normalized_quat_to_rotmat(quat);
    scale = exp(-scale);
    float3x3 S = float3x3(
        scale.x, 0.f, 0.f,
        0.f, scale.y, 0.f,
        0.f, 0.f, scale.z
    );
    return mul(S, transpose(R));
}

[CudaDeviceExport]
void compute_3dgut_iscl_rot_vjp(
    float4 quat, float3 scale,
    float3x3 v_iscl_rot,
    out float4 v_quat, out float3 v_scale
) {
    DifferentialPair<float4> dp_quat = diffPair(quat);
    DifferentialPair<float3> dp_scale = diffPair(scale);
    bwd_diff(compute_3dgut_iscl_rot)(
        dp_quat, dp_scale, v_iscl_rot
    );
    v_quat = dp_quat.d;
    v_scale = dp_scale.d;
}

[CudaDeviceExport]
[Differentiable]
float evaluate_alpha_3dgs(
    float3 mean, float3x3 iscl_rot, float opacity,
    float3 ray_o, float3 ray_d
) {
    float3 gro = mul(iscl_rot, (ray_o - mean));
    float3 grd = mul(iscl_rot, ray_d);
    float3 gcrod = cross(grd, gro);
    float val = exp(-0.5f * dot(gcrod, gcrod) / dot(grd, grd));
    return opacity * val;
}

[CudaDeviceExport]
void evaluate_alpha_3dgs_vjp(
    float3 mean, float3x3 iscl_rot, float opacity,
    float3 ray_o, float3 ray_d, float v_alpha,
    out float3 v_mean, out float3x3 v_iscl_rot, out float v_opacity,
    out float3 v_ray_o, out float3 v_ray_d
) {
    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_iscl_rot = diffPair(iscl_rot);
    DifferentialPair<float> dp_opacity = diffPair(opacity);
    DifferentialPair<float3> dp_ray_o = diffPair(ray_o);
    DifferentialPair<float3> dp_ray_d = diffPair(ray_d);
    bwd_diff(evaluate_alpha_3dgs)(
        dp_mean, dp_iscl_rot, dp_opacity,
        dp_ray_o, dp_ray_d, v_alpha
    );
    v_mean = dp_mean.d;
    v_iscl_rot = dp_iscl_rot.d;
    v_opacity = dp_opacity.d;
    v_ray_o = dp_ray_o.d;
    v_ray_d = dp_ray_d.d;
}

[CudaDeviceExport]
[Differentiable]
void evaluate_color_3dgs(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d,
    out float3 out_rgb, out float depth
) {
    out_rgb = rgb;
    // depth = length(mean - ray_o);
    float3 gro = mul(iscl_rot, (ray_o - mean));
    float3 grd = mul(iscl_rot, ray_d);
    depth = -dot(gro, grd) / dot(grd, grd);
}

[CudaDeviceExport]
void evaluate_color_3dgs_vjp(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d, float3 v_out_rgb, float v_depth,
    out float3 v_mean, out float3x3 v_iscl_rot, out float v_opacity, out float3 v_rgb,
    out float3 v_ray_o, out float3 v_ray_d
) {
    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_iscl_rot = diffPair(iscl_rot);
    DifferentialPair<float> dp_opacity = diffPair(opacity);
    DifferentialPair<float3> dp_rgb = diffPair(rgb);
    DifferentialPair<float3> dp_ray_o = diffPair(ray_o);
    DifferentialPair<float3> dp_ray_d = diffPair(ray_d);
    bwd_diff(evaluate_color_3dgs)(
        dp_mean, dp_iscl_rot, dp_opacity, dp_rgb,
        dp_ray_o, dp_ray_d, v_out_rgb, v_depth
    );
    v_mean = dp_mean.d;
    v_iscl_rot = dp_iscl_rot.d;
    v_opacity = dp_opacity.d;
    v_rgb = dp_rgb.d;
    v_ray_o = dp_ray_o.d;
    v_ray_d = dp_ray_d.d;
}

// The shortest axis, facing the ray: iscl_rot's longest row is axis / scale.
[Differentiable]
[ForceInline]
float3 shortest_axis_normal(float3x3 iscl_rot, float3 ray_d) {
    float3 r0 = float3(iscl_rot[0][0], iscl_rot[0][1], iscl_rot[0][2]);
    float3 r1 = float3(iscl_rot[1][0], iscl_rot[1][1], iscl_rot[1][2]);
    float3 r2 = float3(iscl_rot[2][0], iscl_rot[2][1], iscl_rot[2][2]);
    float l0 = dot(r0, r0), l1 = dot(r1, r1), l2 = dot(r2, r2);
    float3 n;
#if 0
    if (l0 >= l1 && l0 >= l2) n = r0;
    else if (l1 >= l2) n = r1;
    else n = r2;
#else
    if (l0 >= l1 && l0 >= l2) n = cross(r1, r2);
    else if (l1 >= l2) n = cross(r2, r0);
    else n = cross(r0, r1);
#endif
    n = n * rsqrt(dot(n, n));
    return dot(n, ray_d) > 0.f ? -n : n;
}

[CudaDeviceExport]
[Differentiable]
void evaluate_color_3dgs_normal(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d,
    out float3 out_rgb, out float depth, out float3 normal
) {
    evaluate_color_3dgs(mean, iscl_rot, opacity, rgb, ray_o, ray_d, out_rgb, depth);
    normal = shortest_axis_normal(iscl_rot, ray_d);
}

[CudaDeviceExport]
void evaluate_color_3dgs_normal_vjp(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d, float3 v_out_rgb, float v_depth, float3 v_normal,
    out float3 v_mean, out float3x3 v_iscl_rot, out float v_opacity, out float3 v_rgb,
    out float3 v_ray_o, out float3 v_ray_d
) {
    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_iscl_rot = diffPair(iscl_rot);
    DifferentialPair<float> dp_opacity = diffPair(opacity);
    DifferentialPair<float3> dp_rgb = diffPair(rgb);
    DifferentialPair<float3> dp_ray_o = diffPair(ray_o);
    DifferentialPair<float3> dp_ray_d = diffPair(ray_d);
    bwd_diff(evaluate_color_3dgs_normal)(
        dp_mean, dp_iscl_rot, dp_opacity, dp_rgb,
        dp_ray_o, dp_ray_d, v_out_rgb, v_depth, v_normal
    );
    v_mean = dp_mean.d;
    v_iscl_rot = dp_iscl_rot.d;
    v_opacity = dp_opacity.d;
    v_rgb = dp_rgb.d;
    v_ray_o = dp_ray_o.d;
    v_ray_d = dp_ray_d.d;
}


// ================
// 2DGS surfel
// ================

// Rows: tangent u / su, tangent v / sv, unit normal. scale.z is ignored.
[CudaDeviceExport]
[Differentiable]
float3x3 compute_2dgs_iscl_rot(float4 quat, float3 scale) {
    float3x3 R = normalized_quat_to_rotmat(quat);
    float3x3 S = float3x3(
        exp(-scale.x), 0.f, 0.f,
        0.f, exp(-scale.y), 0.f,
        0.f, 0.f, 1.f
    );
    return mul(S, transpose(R));
}

[CudaDeviceExport]
void compute_2dgs_iscl_rot_vjp(
    float4 quat, float3 scale,
    float3x3 v_iscl_rot,
    out float4 v_quat, out float3 v_scale
) {
    DifferentialPair<float4> dp_quat = diffPair(quat);
    DifferentialPair<float3> dp_scale = diffPair(scale);
    bwd_diff(compute_2dgs_iscl_rot)(dp_quat, dp_scale, v_iscl_rot);
    v_quat = dp_quat.d;
    v_scale = dp_scale.d;
}

struct SurfelHit : IDifferentiable {
    float g;       // exp(-r^2/2) at the ray-plane hit; 0 past grazing
    float t_hit;   // ray parameter of the plane hit
};

// The bare disc, with no screen-space low-pass: it renders as 3DGUT renders a
// flat gaussian, so the mesher's moments and a 3dgut view see what trained.
[Differentiable]
[ForceInline]
SurfelHit surfel_ray_hit(float3 mean, float3x3 iscl_rot, float3 ray_o, float3 ray_d) {
    SurfelHit h;
    float3 gro = mul(iscl_rot, ray_o - mean);
    float3 grd = mul(iscl_rot, ray_d);
    // The normal row is unit length, so grd.z / |ray_d| is the cosine.
    if (grd.z * grd.z > SURFEL_GRAZING_COS2 * dot(ray_d, ray_d)) {
        h.t_hit = -gro.z / grd.z;
        float2 uv = gro.xy + h.t_hit * grd.xy;
        h.g = exp(-0.5f * dot(uv, uv));
    } else {
        h.t_hit = 0.f;
        h.g = 0.f;
    }
    return h;
}

[CudaDeviceExport]
[Differentiable]
float evaluate_alpha_2dgs(
    float3 mean, float3x3 iscl_rot, float opacity, float3 ray_o, float3 ray_d
) {
    return opacity * surfel_ray_hit(mean, iscl_rot, ray_o, ray_d).g;
}

// The normal faces the ray.
[CudaDeviceExport]
[Differentiable]
float evaluate_2dgs(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d,
    out float3 out_rgb, out float depth, out float3 normal
) {
    out_rgb = rgb;
    SurfelHit h = surfel_ray_hit(mean, iscl_rot, ray_o, ray_d);
    depth = h.t_hit;
    float3 n = float3(iscl_rot[2][0], iscl_rot[2][1], iscl_rot[2][2]);
    normal = dot(n, ray_d) > 0.f ? -n : n;
    return opacity * h.g;
}

[CudaDeviceExport]
void evaluate_color_2dgs(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d,
    out float3 out_rgb, out float depth, out float3 normal
) {
    evaluate_2dgs(mean, iscl_rot, opacity, rgb, ray_o, ray_d, out_rgb, depth, normal);
}

// Alpha and colour in one VJP: the ray-disc hit is computed once.
[CudaDeviceExport]
void evaluate_2dgs_vjp(
    float3 mean, float3x3 iscl_rot, float opacity, float3 rgb,
    float3 ray_o, float3 ray_d,
    float v_alpha, float3 v_out_rgb, float v_depth, float3 v_normal,
    out float3 v_mean, out float3x3 v_iscl_rot, out float v_opacity, out float3 v_rgb,
    out float3 v_ray_o, out float3 v_ray_d
) {
    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float3x3> dp_iscl_rot = diffPair(iscl_rot);
    DifferentialPair<float> dp_opacity = diffPair(opacity);
    DifferentialPair<float3> dp_rgb = diffPair(rgb);
    DifferentialPair<float3> dp_ray_o = diffPair(ray_o);
    DifferentialPair<float3> dp_ray_d = diffPair(ray_d);
    bwd_diff(evaluate_2dgs)(
        dp_mean, dp_iscl_rot, dp_opacity, dp_rgb,
        dp_ray_o, dp_ray_d, v_out_rgb, v_depth, v_normal, v_alpha
    );
    v_mean = dp_mean.d;
    v_iscl_rot = dp_iscl_rot.d;
    v_opacity = dp_opacity.d;
    v_rgb = dp_rgb.d;
    v_ray_o = dp_ray_o.d;
    v_ray_d = dp_ray_d.d;
}


// ================
// Exact eval3d footprint
// ================

// docs/notes/footprint.md. A ray is carried in a splat's frame: g = M (origin -
// mean), e = M direction, M the fragment's iscl_rot. alpha > ALPHA_THRESHOLD
// where q(g, e) <= 0 with the hit in front, and q is quadratic along a quad edge.

struct FootprintRay {
    float3 c;  // moment of the ray about the splat; a surfel's is its plane offset
    float3 w;
    float f;   // > 0: the hit is in front of the ray origin
};

[ForceInline]
FootprintRay footprint_ray(float3 g, float3 e, bool surfel) {
    // Unit e keeps q of a 1e-9-thin splat inside fp32.
    e *= rsqrt(max(dot(e, e), 1e-37f));
    FootprintRay k;
    if (surfel) {
        k.c = float3(g.x * e.z - g.z * e.x, g.y * e.z - g.z * e.y, 0.0f);
        k.w = float3(e.z, 0.0f, 0.0f);
        k.f = -g.z * e.z;
    } else {
        k.c = cross(g, e);
        k.w = e;
        k.f = -dot(g, e);
    }
    return k;
}

[ForceInline]
float footprint_form(FootprintRay a, FootprintRay b, float r2) {
    return dot(a.c, b.c) - r2 * dot(a.w, b.w);
}

// Two rays that both miss: whether the footprint crosses the edge between them.
// q is taken at its minimum from the interpolated ray: across a thin splat the
// corner values are ~1e12 against a minimum of ~10, and their difference is noise.
[ForceInline]
bool footprint_edge_hit(FootprintRay a, FootprintRay b, float r2) {
    float3 dc = b.c - a.c, dw = b.w - a.w;
    float lin = dot(a.c, dc) - r2 * dot(a.w, dw);
    float quad = dot(dc, dc) - r2 * dot(dw, dw);
    if (!(lin < 0.0f && -lin < quad))
        return false;
    float s = -lin / quad;
    float3 c = a.c + s * dc, w = a.w + s * dw;
    return dot(c, c) - r2 * dot(w, w) <= 0.0f && a.f + s * (b.f - a.f) > 0.0f;
}

// Every ray that reaches a splat passes through one planar ellipse, c0 + a cos +
// b sin: the surfel itself, or the gaussian's tangent circle seen from the camera.
struct FootprintDisc {
    float3 c0, a, b;
    bool inside;  // the camera is inside the gaussian: every forward ray hits
};

// axes: the splat's axes in camera space, as rows. scale is linear.
[ForceInline]
FootprintDisc footprint_disc(
    float3 mean_c, float3x3 axes, float3 scale, float r2, bool surfel
) {
    FootprintDisc d;
    d.inside = false;
    if (surfel) {
        float r = sqrt(r2);
        d.c0 = mean_c;
        d.a = r * scale.x * axes[0];
        d.b = r * scale.y * axes[1];
        return d;
    }
    // Bounds only: a thicker gaussian contains the real one and keeps h in fp32.
    scale = max(scale, 1e-3f * max(scale.x, max(scale.y, scale.z)));
    float3 h = mul(axes, mean_c) / scale;
    float hh = dot(h, h);
    if (!(hh > 1.001f * r2)) {
        d.inside = true;
        d.c0 = mean_c;
        d.a = float3(0.0f);
        d.b = float3(0.0f);
        return d;
    }
    float k = r2 / hh;
    float rho = sqrt(r2 * (1.0f - k));
    h *= rsqrt(hh);
    // Duff et al. 2017, "Building an Orthonormal Basis, Revisited"
    float sg = h.z >= 0.0f ? 1.0f : -1.0f;
    float p = -1.0f / (sg + h.z);
    float o = h.x * h.y * p;
    float3 u1 = rho * scale * float3(1.0f + sg * h.x * h.x * p, sg * o, -sg * h.x);
    float3 u2 = rho * scale * float3(o, sg + h.y * h.y * p, -h.y);
    d.c0 = mean_c * (1.0f - k);
    d.a = mul(u1, axes);
    d.b = mul(u2, axes);
    return d;
}

// Whether the ray from the camera along dir passes through the disc.
[ForceInline]
bool footprint_disc_contains(FootprintDisc d, float3 dir) {
    if (d.inside)
        return true;
    float3 n = cross(d.a, d.b);
    float den = dot(dir, n);
    float num = dot(d.c0, n);
    if (!(num * den > 0.0f))
        return false;
    float3 p = dir * (num / den) - d.c0;
    float nn = dot(n, n);
    float u = dot(cross(p, d.b), n);
    float v = dot(cross(d.a, p), n);
    return u * u + v * v <= nn * nn;
}

// The footprint of an undistorted pinhole view is the conic with dual c0 c0^T -
// a a^T - b b^T: an ellipse while the disc stays in front of the camera plane.
// Its centre and covariance (xx, xy, yy) in pixels; false when it is not one.
[ForceInline]
bool footprint_pinhole_ellipse(FootprintDisc d, float4 intrins, out float2 centre, out float3 cov) {
    centre = float2(0.0f);
    cov = float3(0.0f);
    float den = d.c0.z * d.c0.z - d.a.z * d.a.z - d.b.z * d.b.z;
    if (d.inside || !(d.c0.z > 0.0f && den > 1e-6f * d.c0.z * d.c0.z))
        return false;
    float inv = 1.0f / den;
    float2 ctr = float2(d.c0.x * d.c0.z - d.a.x * d.a.z - d.b.x * d.b.z,
                        d.c0.y * d.c0.z - d.a.y * d.a.z - d.b.y * d.b.z) * inv;
    // Half extents as cross products: the plain difference cancels for a
    // splat a pixel wide.
    float ab_x = d.a.x * d.b.z - d.a.z * d.b.x, ab_y = d.a.y * d.b.z - d.a.z * d.b.y;
    float ca_x = d.c0.x * d.a.z - d.c0.z * d.a.x, cb_x = d.c0.x * d.b.z - d.c0.z * d.b.x;
    float ca_y = d.c0.y * d.a.z - d.c0.z * d.a.y, cb_y = d.c0.y * d.b.z - d.c0.z * d.b.y;
    float fx = intrins.x, fy = intrins.y;
    centre = float2(fx * ctr.x + intrins.z, fy * ctr.y + intrins.w);
    cov = float3(
        fx * fx * max(ca_x * ca_x + cb_x * cb_x - ab_x * ab_x, 0.0f) * inv * inv,
        fx * fy * (ca_x * ca_y + cb_x * cb_y - ab_x * ab_y) * inv * inv,
        fy * fy * max(ca_y * ca_y + cb_y * cb_y - ab_y * ab_y, 0.0f) * inv * inv);
    return true;
}

// The splat a camera sees, as footprint_disc takes it.
[ForceInline]
FootprintDisc footprint_disc_of(
    float3 mean, float4 quat, float3 log_scale, float opacity, float3x3 R, float3 t, bool surfel
) {
    float3x3 axes = transpose(mul(R, normalized_quat_to_rotmat(normalize(quat))));
    return footprint_disc(mul(R, mean) + t, axes, exp(log_scale),
                          2.0f * log(opacity / ALPHA_THRESHOLD), surfel);
}

// A parallel projection of a gaussian is its marginal, and of a disc an ellipse:
// the footprint's centre and covariance (xx, xy, yy) in pixels.
[ForceInline]
void footprint_ortho_ellipse(
    float3 mean, float4 quat, float3 log_scale, float opacity, float3x3 R, float3 t,
    float4 intrins, bool surfel, out float2 centre, out float3 cov
) {
    float3 scale = exp(log_scale);
    if (surfel)
        scale.z = 0.0f;
    float3x3 covar, covar_c;
    quat_scale_to_covar(normalize(quat), scale, covar);
    covarW2C(R, covar, covar_c);
    float3 mean_c = mul(R, mean) + t;
    float r2 = 2.0f * log(opacity / ALPHA_THRESHOLD);
    float fx = intrins.x, fy = intrins.y;
    centre = float2(fx * mean_c.x + intrins.z, fy * mean_c.y + intrins.w);
    cov = r2 * float3(fx * fx * covar_c[0][0], fx * fy * covar_c[0][1], fy * fy * covar_c[1][1]);
}

// The footprint of an undistorted pinhole (planar 1) or orthographic (2) view as
// an ellipse, a hundredth of a pixel^2 wider so an edge-on disc keeps a width the
// tile sweep can divide by. False where a pinhole's is not one.
[CudaDeviceExport]
bool footprint_planar_cov(
    float3 mean, float4 quat, float3 log_scale, float opacity, float3x3 R, float3 t,
    float4 intrins, bool surfel, int planar, out float2 centre, out float3 cov
) {
    bool ok = true;
    if (planar == 2)
        footprint_ortho_ellipse(mean, quat, log_scale, opacity, R, t, intrins, surfel,
                                centre, cov);
    else
        ok = footprint_pinhole_ellipse(
            footprint_disc_of(mean, quat, log_scale, opacity, R, t, surfel), intrins,
            centre, cov);
    cov.x += 0.01f;
    cov.z += 0.01f;
    return ok;
}

// How a splat's footprint meets the quad of four rays from one origin, given in
// image order (x0,y0) (x1,y0) (x1,y1) (x0,y1): 0 misses it, 2 covers it (both are
// convex, so four corners inside is all of it), 1 neither.
[ForceInline]
int footprint_quad_coverage(
    float3 g, float3 e0, float3 e1, float3 e2, float3 e3, float r2, bool surfel
) {
    FootprintRay k0 = footprint_ray(g, e0, surfel);
    FootprintRay k1 = footprint_ray(g, e1, surfel);
    FootprintRay k2 = footprint_ray(g, e2, surfel);
    FootprintRay k3 = footprint_ray(g, e3, surfel);
    float q0 = footprint_form(k0, k0, r2);
    float q1 = footprint_form(k1, k1, r2);
    float q2 = footprint_form(k2, k2, r2);
    float q3 = footprint_form(k3, k3, r2);
    int inside = int(q0 <= 0.0f && k0.f > 0.0f) + int(q1 <= 0.0f && k1.f > 0.0f) +
                 int(q2 <= 0.0f && k2.f > 0.0f) + int(q3 <= 0.0f && k3.f > 0.0f);
    if (inside != 0)
        return inside == 4 ? 2 : 1;
    if (footprint_edge_hit(k0, k1, r2) || footprint_edge_hit(k1, k2, r2) ||
        footprint_edge_hit(k2, k3, r2) || footprint_edge_hit(k3, k0, r2))
        return 1;
    // No edge is crossed: the footprint is inside the quad when its centre, -g, is.
    return int(dot(cross(e0, e1), g) <= 0.0f && dot(cross(e1, e2), g) <= 0.0f &&
               dot(cross(e2, e3), g) <= 0.0f && dot(cross(e3, e0), g) <= 0.0f);
}

// footprint_quad_coverage for rays from origin o with directions d, in the frame
// of mean and M. A zero direction is a corner the lens model has no ray for: the
// quad is kept, and never counts as covered.
[CudaDeviceExport]
int footprint_rays_coverage(
    float3 mean, float3x3 M, float opacity, bool surfel, float3 o,
    float3 d0, float3 d1, float3 d2, float3 d3
) {
    if (dot(d0, d0) == 0.0f || dot(d1, d1) == 0.0f || dot(d2, d2) == 0.0f ||
        dot(d3, d3) == 0.0f)
        return 1;
    return footprint_quad_coverage(
        mul(M, o - mean), mul(M, d0), mul(M, d1), mul(M, d2), mul(M, d3),
        2.0f * log(opacity / ALPHA_THRESHOLD), surfel);
}

// Whether the box reaches a pixel centre of the micro tile at (bx0, by0).
[CudaDeviceExport]
bool aabb_reaches_micro_tile(float4 aabb, float bx0, float by0, float size) {
    return aabb.x <= bx0 + size - 0.5f && aabb.z >= bx0 + 0.5f &&
           aabb.y <= by0 + size - 0.5f && aabb.w >= by0 + 0.5f;
}

// A footprint's extent across its narrow side, |slab.xy . p - slab.z| <= 1 in
// pixels: whether a box (centre, half extent) lies outside it. Zero: no slab.
[CudaDeviceExport]
bool slab_misses_box(float3 slab, float2 centre, float2 half_extent) {
    return abs(dot(slab.xy, centre) - slab.z) >
           1.0f + dot(abs(slab.xy), half_extent);
}


[ForceInline]
float3x3 outerProduct(float3 u) {
    return float3x3(u.x * u, u.y * u, u.z * u);
}

// linear depth in front for compatibility with conventional viewers
// continuous near boundary to support >180deg fisheye
[ForceInline]
float get_sorting_depth(CameraModelType camera_model, float3 p_view) {
    if (camera_model == CameraModelType::PINHOLE || camera_model == CameraModelType::ORTHOGRAPHIC)
        return p_view.z;
    // Equirectangular sees the full sphere, so sort strictly by distance
    // (squared, monotonic) rather than the forward-biased fisheye heuristic.
    if (camera_model == CameraModelType::EQUIRECTANGULAR)
        return dot(p_view, p_view);
    // float z = p_view.z;
    // float z = max(p_view.z, sqrt(0.5f * dot(p_view, p_view)));
    // float z = 0.5f * (p_view.z + length(p_view));
    // float z = max(p_view.z, (1.0f / (1.0f + sqrt(2.0f))) * (p_view.z + length(p_view)));
    // return log(z);
    // float z = max(abs(p_view.z), 0.125*length(p_view));
    float x = p_view.x, y = p_view.y, z = p_view.z;
    // z = sqrt(sqrt(z*z*z*z + (1.0f/512.0f) * (x*x+y*y)*(x*x+y*y)));
    z = z*z*z*z + (1.0f/512.0f) * (x*x+y*y)*(x*x+y*y);  // doesn't matter
    return z;
}


[CudaDeviceExport]
float view_radius_3dgs(
    float3 mean,
    float3 log_scale,
    float logit_opacity,
    float3 campos
) {
    float extend = sqrt(2.0*log(max(255.0/(1.0+exp(-logit_opacity)), 1.0)));
    float radius = exp(max(log_scale.x, max(log_scale.y, log_scale.z))) * extend;
    float dist = length(mean - campos);
    return radius / (max(dist, radius) + sqrt(max(dist*dist-radius*radius, 0.0)));
}


#endif  // _PROJECTION_UTILS_SLANG_INC
