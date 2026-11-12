#pragma once
// ^ not working for [CudaDeviceExport]

#ifndef _PROJECTION_UTILS_SLANG_INC

#define _PROJECTION_UTILS_SLANG_INC


static const float ALPHA_THRESHOLD = 1.0f/255.0f;

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
    ) { return generate_ray<D>(uv, camera_model, dist_coeffs, raydir); }

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
