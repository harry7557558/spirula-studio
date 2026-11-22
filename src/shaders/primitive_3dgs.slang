#pragma once

// suppress CudaDeviceExport to reduce code size
#ifdef CudaDeviceExport
#include "projection_utils.slang"
#else
#define CudaDeviceExport Differentiable
#include "projection_utils.slang"
#undef CudaDeviceExport
#endif

[ForceInline]
[Differentiable]
bool persp_proj_3dgs<D : ICameraDistortion, let assume_valid : bool>(
    // inputs
    const float3 mean3d,
    const float3x3 cov3d,
    const float4 intrins,
    const float dist_coeffs[D.kNumParams],
    const uint width,
    const uint height,
    // outputs
    out float2x2 cov2d,
    out float2 mean2d
) {
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;
    cov2d = float2x2(0.0f);

    bool valid = persp_proj<D, assume_valid>(mean3d, intrins, dist_coeffs, mean2d);
    if (!assume_valid && !valid)
        return false;

    float2x3 J = persp_proj_jac<D>(mean3d, intrins, dist_coeffs);

    float lim_x_pos = width + 0.3f * (0.5f * width);
    float lim_x_neg = 0.3f * (0.5f * width);
    float lim_y_pos = height + 0.3f * (0.5f * height);
    float lim_y_neg = 0.3f * (0.5f * height);

    float rz = 1.f / mean3d.z;
    float max_Jxz = -(-lim_x_neg - cx) * rz;
    float min_Jxz = -(lim_x_pos - cx) * rz;
    float max_Jyz = -(-lim_y_neg - cy) * rz;
    float min_Jyz = -(lim_y_pos - cy) * rz;
    J[0][2] = clamp(J[0][2], min_Jxz, max_Jxz);
    J[1][2] = clamp(J[1][2], min_Jyz, max_Jyz);

    cov2d = mul(mul(J, cov3d), transpose(J));
    return true;
}

[ForceInline]
[Differentiable]
bool fisheye_proj_3dgs<D : ICameraDistortion, let assume_valid : bool>(
    // inputs
    const float3 mean3d,
    const float3x3 cov3d,
    const float4 intrins,
    const float dist_coeffs[D.kNumParams],
    // outputs
    out float2x2 cov2d,
    out float2 mean2d
) {
    cov2d = float2x2(0.0f);
    bool valid = fisheye_proj<D, assume_valid>(mean3d, intrins, dist_coeffs, mean2d);
    if (!assume_valid && !valid)
        return false;

    float2x3 J = fisheye_proj_jac<D>(mean3d, intrins, dist_coeffs);
    cov2d = mul(mul(J, cov3d), transpose(J));
    return true;
}

[ForceInline]
[Differentiable]
bool equisolid_proj_3dgs<D : ICameraDistortion, let assume_valid : bool>(
    // inputs
    const float3 mean3d,
    const float3x3 cov3d,
    const float4 intrins,
    const float dist_coeffs[D.kNumParams],
    // outputs
    out float2x2 cov2d,
    out float2 mean2d
) {
    cov2d = float2x2(0.0f);
    bool valid = equisolid_proj<D, assume_valid>(mean3d, intrins, dist_coeffs, mean2d);
    if (!assume_valid && !valid)
        return false;

    float2x3 J = equisolid_proj_jac<D>(mean3d, intrins, dist_coeffs);
    cov2d = mul(mul(J, cov3d), transpose(J));
    return true;
}

[ForceInline]
[Differentiable]
bool equirect_proj_3dgs<D : ICameraDistortion, let assume_valid : bool>(
    // inputs
    const float3 mean3d,
    const float3x3 cov3d,
    const float4 intrins,
    const float dist_coeffs[D.kNumParams],
    // outputs
    out float2x2 cov2d,
    out float2 mean2d
) {
    // Every direction is valid for equirectangular; equirect_proj always
    // succeeds. The covariance is pushed through the analytic angular Jacobian.
    equirect_proj<assume_valid>(mean3d, intrins, mean2d);

    float2x3 J = equirect_proj_jac(mean3d, intrins);
    cov2d = mul(mul(J, cov3d), transpose(J));
    return true;
}

// Per-tier wrappers: slangc cannot export a generic. The CUDA side selects one
// at compile time through SlangPrimitive3DGS<D> (core/CameraDistortion.cuh).
#define _DEF_proj_3dgs_nav(suffix, D, NC) \
    [CudaDeviceExport] \
    bool persp_proj_3dgs_nav##suffix( \
        const float3 mean3d, const float3x3 cov3d, const float4 intrins, \
        const float dist_coeffs[NC], const uint width, const uint height, \
        out float2x2 cov2d, out float2 mean2d \
    ) { \
        return persp_proj_3dgs<D, false>( \
            mean3d, cov3d, intrins, dist_coeffs, width, height, cov2d, mean2d); \
    } \
    [CudaDeviceExport] \
    bool fisheye_proj_3dgs_nav##suffix( \
        const float3 mean3d, const float3x3 cov3d, const float4 intrins, \
        const float dist_coeffs[NC], out float2x2 cov2d, out float2 mean2d \
    ) { \
        return fisheye_proj_3dgs<D, false>( \
            mean3d, cov3d, intrins, dist_coeffs, cov2d, mean2d); \
    } \
    [CudaDeviceExport] \
    bool equisolid_proj_3dgs_nav##suffix( \
        const float3 mean3d, const float3x3 cov3d, const float4 intrins, \
        const float dist_coeffs[NC], out float2x2 cov2d, out float2 mean2d \
    ) { \
        return equisolid_proj_3dgs<D, false>( \
            mean3d, cov3d, intrins, dist_coeffs, cov2d, mean2d); \
    }

_DEF_proj_3dgs_nav(_none, DistNone, 1)
_DEF_proj_3dgs_nav(_opencv, DistOpenCV, 4)
_DEF_proj_3dgs_nav(_prism, DistThinPrism, 8)

#undef _DEF_proj_3dgs_nav

[CudaDeviceExport]
bool equirect_proj_3dgs_nav(
    const float3 mean3d,
    const float3x3 cov3d,
    const float4 intrins,
    out float2x2 cov2d,
    out float2 mean2d
) {
    const float dist_coeffs[1] = { 0.0f };
    return equirect_proj_3dgs<DistNone, false>(
        mean3d, cov3d, intrins, dist_coeffs, cov2d, mean2d);
}

// ================
// Exact eval3d bounds
// ================

static const int kFootprintRim = 8;
// 1 / cos(pi/8): the octagon through rim points pushed out this far contains the rim.
static const float kFootprintRimInflate = 1.0823922f;

[ForceInline]
float3 footprint_rim(FootprintDisc d, int k, float radius) {
    static const float cs[8] = { 1.0f, 0.70710678f, 0.0f, -0.70710678f,
                                 -1.0f, -0.70710678f, 0.0f, 0.70710678f };
    return d.c0 + radius * (cs[k] * d.a + cs[(k + 6) & 7] * d.b);
}

// A point's lens coordinates before distortion (x/z, or the fisheye's own), and
// whether the lens can see it at all.
[ForceInline]
bool footprint_lens_coords<let camera_model: CameraModelType>(float3 p, out float2 uv0) {
    if (camera_model == CameraModelType::PINHOLE) {
        uv0 = p.xy / p.z;
        return p.z > 0.0f;
    }
    if (camera_model == CameraModelType::FISHEYE)
        uv0 = fisheye_proj(p);
    else if (camera_model == CameraModelType::EQUISOLID)
        uv0 = equisolid_proj(p);
    else
        uv0 = equirect_proj(p);
    return true;
}

// The squared radius, toward uv0 and no further than it or `cap`, up to which the
// distorted radius keeps growing. Past its first turning point a distortion
// polynomial folds points back into the image, where its Jacobian can look sound.
[ForceInline]
float footprint_lens_trust<D : ICameraDistortion>(
    float2 uv0, float cap, float dist_coeffs[D.kNumParams]
) {
    if (D.kIsIdentity)
        return 1e30f;
    float len = length(uv0);
    float2 step = uv0 * (0.125f * min(1.0f, cap / max(len, 1e-30f)));
    float last = 0.0f, trusted = 8.0f;
    [ForceUnroll] for (int i = 1; i <= 8; i++) {
        float2 q = D.distort(step * i, dist_coeffs);
        float r = dot(q, q);
        if (!(r > last) && trusted == 8.0f)
            trusted = i - 1;
        last = r;
    }
    return trusted * trusted * dot(step, step);
}

// The lens radius toward `dir` (unit) past which nothing is drawn: where the
// distorted radius leaves the circle around the image (reach2), or turns back.
float footprint_lens_reach<D : ICameraDistortion>(
    float2 dir, float cap, float reach2, float dist_coeffs[D.kNumParams]
) {
    if (D.kIsIdentity)
        return sqrt(reach2);
    float last = 0.0f;
    for (int i = 1; i <= 16; i++) {
        float2 q = D.distort(dir * (cap * i / 16.0f), dist_coeffs);
        float r = dot(q, q);
        if (!(r > last) || r > reach2)
            return cap * i / 16.0f;
        last = r;
    }
    return cap;
}

// The slab (slab_misses_box) of a footprint that spans [lo, hi] along unit n.
[ForceInline]
float3 footprint_slab(float2 n, float lo, float hi) {
    // A hundredth of a pixel of slack: the slab only spares the exact test.
    float inv = 2.0f / (1.001f * (hi - lo) + 0.02f);
    return float3(n * inv, 0.5f * (lo + hi) * inv);
}

// Unit eigenvector of the smaller eigenvalue of [[sxx, sxy], [sxy, syy]].
[ForceInline]
float2 minor_axis(float sxx, float sxy, float syy) {
    float lambda = 0.5f * (sxx + syy) - sqrt(0.25f * (sxx - syy) * (sxx - syy) + sxy * sxy);
    float2 u = float2(sxy, lambda - sxx), v = float2(lambda - syy, sxy);
    float uu = dot(u, u), vv = dot(v, v);
    if (!(max(uu, vv) > 1e-12f * (sxx * sxx + syy * syy)))
        return float2(1.0f, 0.0f);
    return uu > vv ? u * rsqrt(uu) : v * rsqrt(vv);
}

// The box and the slab of an elliptic footprint, from its covariance in pixels.
[ForceInline]
void footprint_ellipse_bounds(float2 centre, float3 cov, out float4 aabb, out float3 slab) {
    float2 half = sqrt(cov.xz);
    aabb = float4(centre - half, centre + half);
    float2 n = minor_axis(cov.x, cov.y, cov.z);
    float w = sqrt(max(n.x * n.x * cov.x + 2.0f * n.x * n.y * cov.y + n.y * n.y * cov.z, 0.0f));
    float c = dot(n, centre);
    slab = footprint_slab(n, c - w, c + w);
}

// How far the circle around the image (radius in pixels) bows out of the chord
// between two points pulled back onto it.
[ForceInline]
float footprint_arc_bow(float2 a, float2 b, float radius) {
    float h2 = 0.25f * dot(a - b, a - b);
    return radius - sqrt(max(radius * radius - h2, 0.0f));
}

// The box of a footprint too wide, or too far out of sight, for eight samples:
// kFootprintWideRim of them, each pulled back onto the circle around the image
// and into the lens model's trusted radius. False: nothing of it can be drawn.
static const int kFootprintWideRim = 32;

bool footprint_wide_box<let camera_model: CameraModelType, D : ICameraDistortion>(
    FootprintDisc d, float4 intrins, float dist_coeffs[D.kNumParams],
    uint width, uint height, out float4 aabb
) {
    aabb = float4(0.0f, 0.0f, float(width), float(height));
    float2 centre = intrins.zw, focal = intrins.xy;
    float2 corner = max(centre, float2(float(width), float(height)) - centre) / focal;
    float reach2 = 1.1f * dot(corner, corner);
    float2 uv[kFootprintWideRim];
    bool seen[kFootprintWideRim];
    float2 outer = float2(0.0f);
    // Behind a pinhole's camera plane the rim has no image: such a point moves
    // toward the disc's centre onto a near plane, and from there out to the
    // circle, so the samples still trace the edge of what is in front.
    float zn = 1e-3f * (abs(d.c0.z) + length(d.a) + length(d.b));
    if (camera_model == CameraModelType::PINHOLE && !(d.c0.z > zn))
        return true;
    const float step = 6.28318530717958648f / kFootprintWideRim;
    for (int k = 0; k < kFootprintWideRim; k++) {
        float3 p = d.c0 + cos(step * k) * d.a + sin(step * k) * d.b;
        if (camera_model == CameraModelType::PINHOLE && p.z < zn)
            p += (d.c0 - p) * ((zn - p.z) / (d.c0.z - p.z));
        footprint_lens_coords<camera_model>(p, uv[k]);
        seen[k] = true;
        if (dot(uv[k], uv[k]) > dot(outer, outer))
            outer = uv[k];
    }
    float trusted = footprint_lens_trust<D>(outer, 3.0f * length(corner), dist_coeffs);
    int n_seen = 0;
    for (int k = 0; k < kFootprintWideRim; k++) {
        float r2 = dot(uv[k], uv[k]);
        if (!D.kIsIdentity && r2 > trusted) {
            seen[k] = false;
            uv[k] *= sqrt(trusted / r2);
        }
        float2 q = D.distort(uv[k], dist_coeffs);
        float q2 = dot(q, q);
        if (q2 > reach2) {
            seen[k] = false;
            q *= sqrt(reach2 / q2);
        }
        uv[k] = focal * q + centre;
        n_seen += seen[k] ? 1 : 0;
    }
    float r = sqrt(max(dot(d.a, d.a), dot(d.b, d.b))), dist = length(d.c0);
    if (n_seen == 0) {
        // Out of sight at every sample, a long splat can still cross the image
        // between two: kept unless all of it is outside the cone the image fills.
        if (footprint_disc_contains(d, float3(0.0f, 0.0f, 1.0f)) || r >= dist)
            return true;
        float2 dir = d.c0.xy * rsqrt(max(dot(d.c0.xy, d.c0.xy), 1e-30f));
        float lens = footprint_lens_reach<D>(dir, 3.0f * length(corner), reach2, dist_coeffs);
        float fov = camera_model == CameraModelType::PINHOLE ? atan(lens)
                  : camera_model == CameraModelType::FISHEYE ? lens
                  : 2.0f * asin(min(0.5f * lens, 1.0f));
        if (acos(clamp(d.c0.z / dist, -1.0f, 1.0f)) - asin(r / dist) > fov)
            return false;
    }
    // In sight nearer the back pole than four times its own width, its image
    // wraps around the lens circle.
    if (camera_model != CameraModelType::PINHOLE) {
        float pole = 4.0f * r / max(dist - r, 1e-20f);
        for (int k = 0; k < kFootprintWideRim; k++) {
            float3 p = d.c0 + cos(step * k) * d.a + sin(step * k) * d.b;
            if (seen[k] && p.z < 0.0f && length(p.xy) < -p.z * pole)
                return true;
        }
    }
    // The samples' box, half their second difference for the bow between two,
    // and the circle's own bow between two pulled back onto it.
    float2 lo = float2(1e30f), hi = float2(-1e30f), bow = float2(0.0f);
    float arc = 0.0f, radius = sqrt(reach2) * max(focal.x, focal.y);
    for (int k = 0; k < kFootprintWideRim; k++) {
        int next = (k + 1) % kFootprintWideRim;
        lo = min(lo, uv[k]);
        hi = max(hi, uv[k]);
        bow = max(bow, abs(uv[k] - 0.5f * (uv[(k + kFootprintWideRim - 1) % kFootprintWideRim] +
                                           uv[next])));
        if (!seen[k] && !seen[next])
            arc = max(arc, footprint_arc_bow(uv[k], uv[next], radius));
    }
    aabb = float4(lo - 0.5f * bow - arc, hi + 0.5f * bow + arc);
    return true;
}

// Pixel bounds of an eval3d splat's footprint: its box, and where it is known the
// slab across its narrow side. False when it cannot reach the image. Loose bounds
// only cost tests: the tiles test the footprint itself, and each pixel its alpha.
bool footprint_bounds<let camera_model: CameraModelType, D : ICameraDistortion>(
    FootprintDisc d, float4 intrins, float dist_coeffs[D.kNumParams],
    uint width, uint height, out float4 aabb, out float3 slab
) {
    const float4 full = float4(0.0f, 0.0f, float(width), float(height));
    aabb = full;
    slab = float3(0.0f);
    if (d.inside)
        return true;
    float fx = intrins.x, fy = intrins.y, cx = intrins.z, cy = intrins.w;

    if (camera_model == CameraModelType::PINHOLE && D.kIsIdentity) {
        float2 centre;
        float3 cov;
        if (footprint_pinhole_ellipse(d, intrins, centre, cov)) {
            footprint_ellipse_bounds(centre, cov, aabb, slab);
            return true;
        }
        // Crossing the camera plane, the footprint is unbounded: the bounds of
        // the circumscribed octagon, cut at a near plane.
        float zn = 1e-3f * (abs(d.c0.z) + length(d.a) + length(d.b));
        float2 lo = float2(1e30f), hi = float2(-1e30f);
        float3 prev = footprint_rim(d, kFootprintRim - 1, kFootprintRimInflate);
        [ForceUnroll] for (int k = 0; k < kFootprintRim; k++) {
            float3 cur = footprint_rim(d, k, kFootprintRimInflate);
            if (cur.z >= zn) {
                lo = min(lo, cur.xy / cur.z);
                hi = max(hi, cur.xy / cur.z);
            }
            if ((cur.z >= zn) != (prev.z >= zn)) {
                float3 cut = prev + (cur - prev) * ((zn - prev.z) / (cur.z - prev.z));
                lo = min(lo, cut.xy / zn);
                hi = max(hi, cut.xy / zn);
            }
            prev = cur;
        }
        aabb = float4(fx * lo.x + cx, fy * lo.y + cy, fx * hi.x + cx, fy * hi.y + cy);
        return lo.x <= hi.x;
    }

    // Any other camera: project points of the rim. Only what lands in the circle
    // around the image, where the lens model is trusted, can be drawn: a point
    // past either is pulled back onto it, which bounds the footprint cut there.
    bool equirect = camera_model == CameraModelType::EQUIRECTANGULAR;
    float2 centre = float2(cx, cy), focal = float2(fx, fy);
    float2 corner = max(centre, float2(float(width), float(height)) - centre) / focal;
    float reach2 = 1.1f * dot(corner, corner);
    float2 uv[kFootprintRim];
    bool seen[kFootprintRim];
    float2 outer = float2(0.0f);
    bool behind = false;
    [ForceUnroll] for (int k = 0; k < kFootprintRim; k++) {
        seen[k] = footprint_lens_coords<camera_model>(footprint_rim(d, k, 1.0f), uv[k]);
        behind = behind || !seen[k];
        if (dot(uv[k], uv[k]) > dot(outer, outer))
            outer = uv[k];
    }
    // Crossing a pinhole's camera plane the rim leaves the image.
    if (behind)
        return footprint_wide_box<camera_model, D>(d, intrins, dist_coeffs, width, height, aabb);
    // The lens model is trusted out to where its radius first turns back.
    float trusted = footprint_lens_trust<D>(outer, 3.0f * length(corner), dist_coeffs);
    int n_seen = 0;
    [ForceUnroll] for (int k = 0; k < kFootprintRim; k++) {
        float r2 = dot(uv[k], uv[k]);
        if (!D.kIsIdentity && r2 > trusted) {
            seen[k] = false;
            uv[k] *= sqrt(trusted / r2);
        }
        float2 q = D.distort(uv[k], dist_coeffs);
        float q2 = dot(q, q);
        if (!equirect && q2 > reach2) {
            seen[k] = false;
            q *= sqrt(reach2 / q2);
        }
        uv[k] = focal * q + centre;
        n_seen += seen[k] ? 1 : 0;
    }
    // Around the back pole a footprint is the outside of its rim.
    if (!equirect && camera_model != CameraModelType::PINHOLE &&
        footprint_disc_contains(d, float3(0.0f, 0.0f, -1.0f)))
        return true;
    bool big = max(dot(d.a, d.a), dot(d.b, d.b)) > 0.0625f * dot(d.c0, d.c0);
    if (n_seen == 0) {
        // Only a footprint over the lens axis, or one wide enough to cross the
        // image between two samples, can still reach it.
        if (footprint_disc_contains(d, float3(0.0f, 0.0f, 1.0f)))
            return true;
        if (max(dot(d.a, d.a), dot(d.b, d.b)) <= 0.04f * dot(d.c0, d.c0))
            return false;
        return footprint_wide_box<camera_model, D>(d, intrins, dist_coeffs, width, height, aabb);
    }
    if (n_seen < kFootprintRim) {
        if (big && !equirect)
            return footprint_wide_box<camera_model, D>(
                d, intrins, dist_coeffs, width, height, aabb);
        // Cut by the circle it is no ellipse: the samples' box, with the bows
        // footprint_wide_box allows for.
        float2 lo = float2(1e30f), hi = float2(-1e30f), bow = float2(0.0f);
        float arc = 0.0f, radius = sqrt(reach2) * max(fx, fy);
        [ForceUnroll] for (int k = 0; k < kFootprintRim; k++) {
            lo = min(lo, uv[k]);
            hi = max(hi, uv[k]);
            bow = max(bow, abs(uv[k] - 0.5f * (uv[(k + 7) & 7] + uv[(k + 1) & 7])));
            if (!equirect && !seen[k] && !seen[(k + 1) & 7])
                arc = max(arc, footprint_arc_bow(uv[k], uv[(k + 1) & 7], radius));
        }
        aabb = float4(lo - 0.5f * bow - arc, hi + 0.5f * bow + arc);
        return true;
    }

    if (equirect) {
        // u is periodic: unwrap about the centre.
        float period = fx * 6.28318530717958648f;
        float2 uc;
        equirect_proj<false>(d.c0, intrins, uc);
        [ForceUnroll] for (int k = 0; k < kFootprintRim; k++)
            uv[k].x -= period * round((uv[k].x - uc.x) / period);
    }
    // Under an affine map the rim is an ellipse, which the samples' first harmonic
    // gives exactly; what the lens adds is what they leave over, allowed for twice.
    static const float cs[8] = { 1.0f, 0.70710678f, 0.0f, -0.70710678f,
                                 -1.0f, -0.70710678f, 0.0f, 0.70710678f };
    float2 mid = float2(0.0f), ha = float2(0.0f), hb = float2(0.0f);
    [ForceUnroll] for (int k = 0; k < kFootprintRim; k++) {
        mid += 0.125f * uv[k];
        ha += 0.25f * cs[k] * uv[k];
        hb += 0.25f * cs[(k + 6) & 7] * uv[k];
    }
    float2 pad = float2(0.0f);
    [ForceUnroll] for (int k = 0; k < kFootprintRim; k++)
        pad = max(pad, abs(uv[k] - mid - cs[k] * ha - cs[(k + 6) & 7] * hb));
    pad = 2.0f * pad + 0.05f;
    float3 cov = float3(ha.x * ha.x + hb.x * hb.x, ha.x * ha.y + hb.x * hb.y,
                        ha.y * ha.y + hb.y * hb.y);
    float2 half = sqrt(cov.xz) + pad;
    float2 lo = mid - half, hi = mid + half;
    float2 span = 2.0f * half / focal;

    if (equirect) {
        // Half a radian tall, eight samples no longer bound it.
        if (span.y > 0.5f)
            return true;
        bool north = footprint_disc_contains(d, float3(0.0f, -1.0f, 0.0f));
        bool south = footprint_disc_contains(d, float3(0.0f, 1.0f, 0.0f));
        // Nearer a pole than its own size, or across the seam: the full width.
        float lat = max(abs(lo.y - cy), abs(hi.y - cy)) / fy;
        float size = max(hi.y - lo.y, (hi.x - lo.x) * cos(min(lat, 1.5707963f))) / fy;
        float period = fx * 6.28318530717958648f;
        if (north || south || 1.5707963f - lat < size ||
            lo.x < cx - 0.5f * period || hi.x > cx + 0.5f * period) {
            lo.x = 0.0f;
            hi.x = float(width);
        }
        aabb = float4(lo.x, north ? 0.0f : lo.y, hi.x, south ? float(height) : hi.y);
        return true;
    }

    // Over half a radian eight samples stop bounding it.
    if (camera_model != CameraModelType::PINHOLE && (max(span.x, span.y) > 0.5f || big))
        return footprint_wide_box<camera_model, D>(d, intrins, dist_coeffs, width, height, aabb);

    aabb = float4(lo, hi);
    float2 n = minor_axis(cov.x, cov.y, cov.z);
    float w = sqrt(max(n.x * n.x * cov.x + 2.0f * n.x * n.y * cov.y + n.y * n.y * cov.z, 0.0f)) +
              dot(abs(n), pad);
    float c = dot(n, mid);
    slab = footprint_slab(n, c - w, c + w);
    return true;
}


[ForceInline]
[Differentiable]
float add_blur(const float eps2d, inout float2x2 covar, out float compensation) {
    float det_orig = covar[0][0] * covar[1][1] - covar[0][1] * covar[1][0];
    covar[0][0] += eps2d;
    covar[1][1] += eps2d;
    float det_blur = covar[0][0] * covar[1][1] - covar[0][1] * covar[1][0];
    compensation = sqrt(max(0.f, det_orig / det_blur));
    return det_blur;
}


#define _projection_3dgs_ARGS(NC) \
    const bool antialiased, \
    const float3 mean, const float4 quat, const float3 scale, const float in_opacity, \
    const float3x3 R, const float3 t, \
    no_diff const float fx, no_diff const float fy,  no_diff const float cx, no_diff const float cy, \
    no_diff const float dist_coeffs[NC], const uint image_width, const uint image_height, \
    no_diff out float4 aabb_xyxy, no_diff out float sorting_depth, no_diff out float radius, \
    out float2 mean2d, out float depth, out float3 conic, out float opacity

[ForceInline]
[Differentiable]
void projection_3dgs<
    let camera_model: CameraModelType, D : ICameraDistortion,
    let assume_valid: bool, let eval3d: bool, let surfel: bool
>(
    _projection_3dgs_ARGS(D.kNumParams)
) {
    // Culled early returns leave every output at this "invalid" state
    // (aabb_xyxy == 0 is the marker consumers test).
    aabb_xyxy = float4(0.0f);
    sorting_depth = 0.0f;
    radius = 0.0f;
    mean2d = float2(0.0f);
    conic = float3(0.0f);
    opacity = 0.0f;

    // transform Gaussian center to camera space
    float3 mean_c;
    posW2C(R, t, mean, mean_c);
    float cull_z = (camera_model == CameraModelType::PINHOLE ||
                    camera_model == CameraModelType::ORTHOGRAPHIC) ?
        mean_c.z : length(mean_c);
    depth = camera_model == CameraModelType::ORTHOGRAPHIC ? mean_c.z : length(mean_c);
    // Center only, as gsplat and PlayCanvas cull. A size-aware cull hides near splats from training that every
    // other renderer still draws: -0.76 dB in gsplat on Mip-NeRF 360 room.
    if (!assume_valid && cull_z <= 0.0f) {
        aabb_xyxy = float4(0.0f);
        return;
    }

    float4 intrins = { fx, fy, cx, cy };
    float2x2 covar2d = float2x2(0.0f);
    opacity = 1.0 / (1.0 + exp(-in_opacity));

    // The eval3d raster draws the splat itself, so only its bounds are projected:
    // the conic slot carries their slab, and the opacity is the one output with a
    // gradient.
    if (eval3d) {
        if (!assume_valid) {
            float opac = detach<float>(opacity);
            if (opac < ALPHA_THRESHOLD)
                return;
            float4 box;
            float3 slab;
            if (camera_model == CameraModelType::ORTHOGRAPHIC) {
                float2 centre;
                float3 cov;
                no_diff footprint_ortho_ellipse(
                    detach<float3>(mean), detach<float4>(quat), detach<float3>(scale), opac,
                    detach<float3x3>(R), detach<float3>(t), intrins, surfel, centre, cov);
                footprint_ellipse_bounds(centre, cov, box, slab);
            } else {
                FootprintDisc disc = no_diff footprint_disc_of(
                    detach<float3>(mean), detach<float4>(quat), detach<float3>(scale), opac,
                    detach<float3x3>(R), detach<float3>(t), surfel);
                if (!no_diff footprint_bounds<camera_model, D>(
                        disc, intrins, dist_coeffs, image_width, image_height, box, slab))
                    return;
            }
            if (box.z <= 0 || box.x >= image_width || box.w <= 0 || box.y >= image_height)
                return;
            // A box between pixel centres draws nothing: the rays are point samples.
            const float kSlack = 1e-3f;
            if (ceil(box.x - 0.5f - kSlack) > floor(box.z - 0.5f + kSlack) ||
                ceil(box.y - 0.5f - kSlack) > floor(box.w - 0.5f + kSlack))
                return;
            aabb_xyxy = detach<float4>(box);
            conic = detach<float3>(slab);
            sorting_depth = no_diff get_sorting_depth(camera_model, detach<float3>(mean_c));
            float3 campos = -mul(transpose(detach<float3x3>(R)), detach<float3>(t));
            radius = view_radius_3dgs(detach<float3>(mean), detach<float3>(scale),
                                      detach<float>(in_opacity), campos);
        }
        return;
    }

    bool is_valid = true;
    if (camera_model == CameraModelType::ORTHOGRAPHIC) {
        float3x3 covar, covar_c;
        quat_scale_to_covar(normalize(quat), exp(scale), covar);
        covarW2C(R, covar, covar_c);
        mean2d = float2(fx * mean_c.x + cx, fy * mean_c.y + cy);
        covar2d = float2x2(fx * fx * covar_c[0][0], fx * fy * covar_c[0][1],
                          fx * fy * covar_c[1][0], fy * fy * covar_c[1][1]);
    }
    else {
        // transform Gaussian covariance to camera space
        float3x3 covar;
        {
            // compute from quaternions and scales
            quat_scale_to_covar(normalize(quat), exp(scale), covar);
            // quat_scale_to_covar(quat, exp(scale), covar);  // quat is already normalized but can affect gradient
        }
        float3x3 covar_c;
        covarW2C(R, covar, covar_c);

        if (camera_model == CameraModelType::PINHOLE)
            is_valid &= persp_proj_3dgs<D, assume_valid>(
                mean_c, covar_c,
                intrins, dist_coeffs, image_width, image_height,
                covar2d, mean2d
            );
        else if (camera_model == CameraModelType::FISHEYE)
            is_valid &= fisheye_proj_3dgs<D, assume_valid>(
                mean_c, covar_c,
                intrins, dist_coeffs,
                covar2d, mean2d
            );
        else if (camera_model == CameraModelType::EQUISOLID)
            is_valid &= equisolid_proj_3dgs<D, assume_valid>(
                mean_c, covar_c,
                intrins, dist_coeffs,
                covar2d, mean2d
            );
        else if (camera_model == CameraModelType::EQUIRECTANGULAR)
            is_valid &= equirect_proj_3dgs<D, assume_valid>(
                mean_c, covar_c,
                intrins, dist_coeffs,
                covar2d, mean2d
            );

        // do this since sorting for >180deg fisheye is weird
        if (!assume_valid && (camera_model == CameraModelType::FISHEYE || camera_model == CameraModelType::EQUISOLID)) {
            float2 mean2d_c = mean2d - float2(cx, cy);
            float opac = opacity * exp(-0.5f * dot(mul(inverse(covar2d), mean2d_c), mean2d_c));
            if (mean_c.z < 0.0 && opac > 1.0 / 255.0)
                is_valid = false;
        }
    }
    if (!assume_valid && !is_valid) {
        aabb_xyxy = float4(0.0f);
        return;
    }

    float compensation;
    const float eps2d = antialiased ? 0.1 : 0.3;
    float det = add_blur(eps2d, covar2d, compensation);
    if (!assume_valid && det <= 0.f) {
        aabb_xyxy = float4(0.0f);
        return;
    }

    // compute the inverse of the 2d covariance
    float2x2 covar2d_inv = inverse(covar2d);

    float extend = 3.33f;
    {
        if (antialiased)
            opacity *= compensation;
        if (!assume_valid && opacity < ALPHA_THRESHOLD) {
            aabb_xyxy = float4(0.0f);
            return;
        }
        // Compute opacity-aware bounding box.
        // https://arxiv.org/pdf/2402.00525 Section B.2
        extend = min(extend, sqrt(2.0f * log(opacity / ALPHA_THRESHOLD)));
    }

    // compute tight rectangular bounding box (non differentiable)
    // https://arxiv.org/pdf/2402.00525
    float radius_x = extend * sqrt(covar2d[0].x);
    float radius_y = extend * sqrt(covar2d[1].y);
    float xmin = detach<float>(mean2d.x - radius_x);
    float xmax = detach<float>(mean2d.x + radius_x);
    float ymin = detach<float>(mean2d.y - radius_y);
    float ymax = detach<float>(mean2d.y + radius_y);

    // mask out gaussians outside the image region
    if (!assume_valid && (
        xmax <= 0 || xmin >= image_width ||
        ymax <= 0 || ymin >= image_height
    )) {
        aabb_xyxy = float4(0.0f);
        return;
    }

    aabb_xyxy = float4(xmin, ymin, xmax, ymax);
    sorting_depth = no_diff get_sorting_depth(camera_model, mean_c);
    // conic = eval3d ? exp(-scale) :
    //     float3(covar2d_inv[0][0], covar2d_inv[0][1], covar2d_inv[1][1]);
    conic = float3(covar2d_inv[0][0], covar2d_inv[0][1], covar2d_inv[1][1]);

    float3 campos = -mul(transpose(detach<float3x3>(R)), detach<float3>(t));
    radius = view_radius_3dgs(mean, scale, in_opacity, campos);
}

/* Forward Projection */

// One export per (camera model, distortion tier) the CUDA instantiation table
// compiles -- slangc cannot export a generic. Keep this list and kCameraVariants
// in tools/codegen/generate_kernel_instantiation.py in step.
#define _DEF_projection_3dgs(funname, MODEL, D, NC, AV, E3D, SURF) \
    [CudaDeviceExport] \
    [ForceInline] \
    [Differentiable] \
    void funname(_projection_3dgs_ARGS(NC)) { \
        return projection_3dgs<MODEL, D, AV, E3D, SURF>( \
            antialiased, \
            mean, quat, scale, in_opacity, R, t, \
            fx, fy, cx, cy, dist_coeffs, \
            image_width, image_height, \
            aabb_xyxy, sorting_depth, radius, mean2d, depth, conic, opacity \
        ); \
    }

#define _DEF_projection_3dgs_tiers(base, MODEL, E3D, SURF) \
    _DEF_projection_3dgs(base##_none, MODEL, DistNone, 1, false, E3D, SURF) \
    _DEF_projection_3dgs(base##_opencv, MODEL, DistOpenCV, 4, false, E3D, SURF) \
    _DEF_projection_3dgs(base##_prism, MODEL, DistThinPrism, 8, false, E3D, SURF)

_DEF_projection_3dgs_tiers(projection_3dgs_persp, CameraModelType::PINHOLE, false, false)
_DEF_projection_3dgs_tiers(projection_3dgs_fisheye, CameraModelType::FISHEYE, false, false)
_DEF_projection_3dgs_tiers(projection_3dgs_equisolid, CameraModelType::EQUISOLID, false, false)
_DEF_projection_3dgs(projection_3dgs_equirect_none, CameraModelType::EQUIRECTANGULAR, DistNone, 1, false, false, false)

_DEF_projection_3dgs_tiers(projection_3dgut_persp, CameraModelType::PINHOLE, true, false)
_DEF_projection_3dgs_tiers(projection_3dgut_fisheye, CameraModelType::FISHEYE, true, false)
_DEF_projection_3dgs_tiers(projection_3dgut_equisolid, CameraModelType::EQUISOLID, true, false)
_DEF_projection_3dgs(projection_3dgut_equirect_none, CameraModelType::EQUIRECTANGULAR, DistNone, 1, false, true, false)

// 2DGS: forward only. Its screen gradients are 3DGUT's (opacity and colour),
// so the backward reuses the 3DGUT exports.
_DEF_projection_3dgs_tiers(projection_2dgs_persp, CameraModelType::PINHOLE, true, true)
_DEF_projection_3dgs_tiers(projection_2dgs_fisheye, CameraModelType::FISHEYE, true, true)
_DEF_projection_3dgs_tiers(projection_2dgs_equisolid, CameraModelType::EQUISOLID, true, true)
_DEF_projection_3dgs(projection_2dgs_equirect_none, CameraModelType::EQUIRECTANGULAR, DistNone, 1, false, true, true)


/* Backward Projection */

interface _DiffProjection3DGS<let NC : int>
{
    [ForceInline]
    [Differentiable]
    static void projection(_projection_3dgs_ARGS(NC));
};

#define _DEF_projection_3dgs_interface(_InterfaceName, MODEL, D, NC, AV, E3D, SURF) \
    struct _InterfaceName : _DiffProjection3DGS<NC> { \
        [ForceInline] \
        [Differentiable] \
        static void projection(_projection_3dgs_ARGS(NC)) { \
            return projection_3dgs<MODEL, D, AV, E3D, SURF>( \
                antialiased, \
                mean, quat, scale, in_opacity, R, t, \
                fx, fy, cx, cy, dist_coeffs, \
                image_width, image_height, \
                aabb_xyxy, sorting_depth, radius, mean2d, depth, conic, opacity \
            ); \
        } \
    };

#define _DEF_projection_3dgs_interface_tiers(base, MODEL, E3D) \
    _DEF_projection_3dgs_interface(base##_none, MODEL, DistNone, 1, true, E3D, false) \
    _DEF_projection_3dgs_interface(base##_opencv, MODEL, DistOpenCV, 4, true, E3D, false) \
    _DEF_projection_3dgs_interface(base##_prism, MODEL, DistThinPrism, 8, true, E3D, false)

_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgs_persp, CameraModelType::PINHOLE, false)
_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgs_fisheye, CameraModelType::FISHEYE, false)
_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgs_equisolid, CameraModelType::EQUISOLID, false)
_DEF_projection_3dgs_interface(_DiffProjection3DGS_3dgs_equirect_none, CameraModelType::EQUIRECTANGULAR, DistNone, 1, true, false, false)

_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgut_persp, CameraModelType::PINHOLE, true)
_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgut_fisheye, CameraModelType::FISHEYE, true)
_DEF_projection_3dgs_interface_tiers(_DiffProjection3DGS_3dgut_equisolid, CameraModelType::EQUISOLID, true)
_DEF_projection_3dgs_interface(_DiffProjection3DGS_3dgut_equirect_none, CameraModelType::EQUIRECTANGULAR, DistNone, 1, true, true, false)


#define _projection_3dgs_vjp_ARGS(NC) \
    const bool antialiased, \
    const float3 mean, const float4 quat, const float3 scale, const float in_opacity, \
    const float3x3 R, const float3 t, const float fx, const float fy, const float cx, const float cy, \
    const float dist_coeffs[NC], const uint image_width, const uint image_height, \
    const float2 v_mean2d, const float v_depth, const float3 v_conic, const float v_opacity, \
    inout float3 v_mean, inout float4 v_quat, inout float3 v_scale, inout float v_in_opacity, \
    inout float3x3 v_R, inout float3 v_t

// Generic over the projection interface (not an existential parameter):
// slangc 2026.12 crashes IR-gen on `bwd_diff` of a static method reached
// through an interface-typed VALUE parameter; the generic-constraint form
// compiles on both 2026.2.1 (CUDA emission) and 2026.12 (SPIR-V).
[ForceInline]
void _projection_3dgs_vjp<let NC : int, ProjT : _DiffProjection3DGS<NC> >(
    _projection_3dgs_vjp_ARGS(NC)
) {
    DifferentialPair<float3> dp_mean = diffPair(mean);
    DifferentialPair<float4> dp_quat = diffPair(quat);
    DifferentialPair<float3> dp_scale = diffPair(scale);
    DifferentialPair<float> dp_in_opacity = diffPair(in_opacity);
    DifferentialPair<float3x3> dp_R = diffPair(R);
    DifferentialPair<float3> dp_t = diffPair(t);
    bwd_diff(ProjT::projection)(
        antialiased,
        dp_mean, dp_quat, dp_scale, dp_in_opacity, dp_R, dp_t,
        fx, fy, cx, cy, dist_coeffs,
        image_width, image_height,
        v_mean2d, v_depth, v_conic, v_opacity
    );
    v_mean += dp_mean.d;
    v_quat += dp_quat.d;
    v_scale += dp_scale.d;
    v_in_opacity += dp_in_opacity.d;
    v_R += dp_R.d;
    v_t += dp_t.d;
}

#define _DEF_projection_3dgs_vjp(funname, _InterfaceName, NC) \
    [CudaDeviceExport] \
    void funname( \
        _projection_3dgs_vjp_ARGS(NC) \
    ) { \
        return _projection_3dgs_vjp<NC, _InterfaceName>( \
            antialiased, \
            mean, quat, scale, in_opacity, R, t, \
            fx, fy, cx, cy, dist_coeffs, \
            image_width, image_height, \
            v_mean2d, v_depth, v_conic, v_opacity, \
            v_mean, v_quat, v_scale, v_in_opacity, v_R, v_t \
        ); \
    }

#define _DEF_projection_3dgs_vjp_tiers(base, iface) \
    _DEF_projection_3dgs_vjp(base##_none_vjp, iface##_none, 1) \
    _DEF_projection_3dgs_vjp(base##_opencv_vjp, iface##_opencv, 4) \
    _DEF_projection_3dgs_vjp(base##_prism_vjp, iface##_prism, 8)

_DEF_projection_3dgs_vjp_tiers(projection_3dgs_persp, _DiffProjection3DGS_3dgs_persp)
_DEF_projection_3dgs_vjp_tiers(projection_3dgs_fisheye, _DiffProjection3DGS_3dgs_fisheye)
_DEF_projection_3dgs_vjp_tiers(projection_3dgs_equisolid, _DiffProjection3DGS_3dgs_equisolid)
_DEF_projection_3dgs_vjp(projection_3dgs_equirect_none_vjp, _DiffProjection3DGS_3dgs_equirect_none, 1)

_DEF_projection_3dgs_vjp_tiers(projection_3dgut_persp, _DiffProjection3DGS_3dgut_persp)
_DEF_projection_3dgs_vjp_tiers(projection_3dgut_fisheye, _DiffProjection3DGS_3dgut_fisheye)
_DEF_projection_3dgs_vjp_tiers(projection_3dgut_equisolid, _DiffProjection3DGS_3dgut_equisolid)
_DEF_projection_3dgs_vjp(projection_3dgut_equirect_none_vjp, _DiffProjection3DGS_3dgut_equirect_none, 1)


_DEF_projection_3dgs(projection_3dgs_ortho_none, CameraModelType::ORTHOGRAPHIC, DistNone, 1, false, false, false)
_DEF_projection_3dgs(projection_3dgut_ortho_none, CameraModelType::ORTHOGRAPHIC, DistNone, 1, false, true, false)
_DEF_projection_3dgs(projection_2dgs_ortho_none, CameraModelType::ORTHOGRAPHIC, DistNone, 1, false, true, true)
