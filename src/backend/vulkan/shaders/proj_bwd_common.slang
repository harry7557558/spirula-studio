// Shared machinery for the projection-backward family (plain backward,
// grad-quant backward, fused projection-bwd-optimizer): splat/camera
// loaders, the per-intersection projection VJP wrappers (2D and 3DGUT
// flavors over the autodiff exports in primitive_3dgs.slang), the SH color
// VJP dispatch, and the CAS scatter helpers.
//
// Declares the spec constants shared by every kernel in the family --
// includers must list their extra spec constants AFTER this include so the
// auto-assigned constant IDs stay 0..4 here and 5.. in the includer,
// matching the host-side SpecList order. kDistortion is the one exception:
// every includer declares it LAST, after its own constants.
//
// Include AFTER primitive_3dgs.slang / harmonics.slang / sh_quant.slang /
// sh_vjp.slang / atomic_float.slang.

#include "int64_compat.slang"
#include "dist_spec.slang"

[SpecializationConstant]
const int kCameraModel = 0;
[SpecializationConstant]
const int kShDegree = 0;
[SpecializationConstant]
const int kAntialiased = 0;
[SpecializationConstant]
const int kShValueBits = 0;
[SpecializationConstant]
const int kPacked = 0;

struct PbSplat {
    float3 mean;
    float4 quat;
    float3 scale;     // log
    float in_opacity; // logit
    float3 dc;
};

[ForceInline]
PbSplat pb_load_splat(float* means, float* quats, float* scales,
                      float* opacities, float* features_dc, uint gid) {
    PbSplat s;
    s.mean = float3(means[3 * gid], means[3 * gid + 1], means[3 * gid + 2]);
    s.quat = float4(quats[4 * gid], quats[4 * gid + 1], quats[4 * gid + 2],
                    quats[4 * gid + 3]);
    s.scale = float3(scales[3 * gid], scales[3 * gid + 1],
                     scales[3 * gid + 2]);
    s.in_opacity = opacities[gid];
    s.dc = float3(features_dc[3 * gid], features_dc[3 * gid + 1],
                  features_dc[3 * gid + 2]);
    return s;
}

struct PbCamera<D : ICameraDistortion> {
    float3x3 R;
    float3 t;
    float4 intrin;
    float dist[D.kNumParams];
};

[ForceInline]
PbCamera<D> pb_load_camera<D : ICameraDistortion>(
    float* viewmats, float4* intrins, float* dist_coeffs, uint cid) {
    PbCamera<D> c;
    float* vm = viewmats + cid * 16;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        c.R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        c.t[r] = vm[4 * r + 3];
    }
    c.intrin = intrins[cid];
    load_dist_coeffs<D>(dist_coeffs, cid, c.dist);
    return c;
}

// Register gradient accumulator for one splat (all VJP wrappers add into it).
struct PbGrads {
    float3 v_mean;
    float4 v_quat;
    float3 v_scale;
    float v_opacity;
    float3 v_dc;
    float3 v_sh[24];
    float3x3 v_R;
    float3 v_t;
};

[ForceInline]
PbGrads pb_grads_zero() {
    PbGrads g;
    g.v_mean = float3(0.0f);
    g.v_quat = float4(0.0f);
    g.v_scale = float3(0.0f);
    g.v_opacity = 0.0f;
    g.v_dc = float3(0.0f);
    for (int j = 0; j < 24; j++)
        g.v_sh[j] = float3(0.0f);
    g.v_R = float3x3(0.0f);
    g.v_t = float3(0.0f);
    return g;
}

// primitive_3dgs.slang exports one VJP per (camera model, tier); this generic
// conformance is the same body, reached with both axes still symbolic so the
// callers can stay generic and fold once at the entry point.
struct PbProj<let cam : CameraModelType, D : ICameraDistortion,
              let eval3d : bool> : _DiffProjection3DGS<D.kNumParams> {
    [ForceInline]
    [Differentiable]
    static void projection(_projection_3dgs_ARGS(D.kNumParams)) {
        return projection_3dgs<cam, D, true, eval3d>(
            antialiased, mean, quat, scale, in_opacity, R, t,
            fx, fy, cx, cy, dist_coeffs, image_width, image_height,
            aabb_xyxy, sorting_depth, radius, mean2d, depth, conic, opacity);
    }
};

// One intersection's projection VJP, 2D (3DGS / Mip) screen layout
// {xy, depth, conic, opac}. kAntialiased selects Mip.
[ForceInline]
void pb_vjp_2d<let cam : CameraModelType, D : ICameraDistortion>(
    PbSplat s, PbCamera<D> c, uint width, uint height, float2 v_xy,
    float v_depth, float3 v_conic, float v_opac, inout PbGrads g) {
    _projection_3dgs_vjp<D.kNumParams, PbProj<cam, D, false> >(
        kAntialiased != 0, s.mean, s.quat, s.scale, s.in_opacity, c.R, c.t,
        c.intrin.x, c.intrin.y, c.intrin.z, c.intrin.w, c.dist, width, height,
        v_xy, v_depth, v_conic, v_opac, g.v_mean, g.v_quat, g.v_scale,
        g.v_opacity, g.v_R, g.v_t);
}

// One intersection's projection VJP, 3DGUT screen layout {scale, opac}: the
// screen-scale grad feeds the conic slot with v_mean2d = 0, v_depth = 0
// (mirrors Primitive3DGUT::project_vjp), antialiased always false.
[ForceInline]
void pb_vjp_3dgut<let cam : CameraModelType, D : ICameraDistortion>(
    PbSplat s, PbCamera<D> c, uint width, uint height, float3 v_sscale,
    float v_sopac, inout PbGrads g) {
    _projection_3dgs_vjp<D.kNumParams, PbProj<cam, D, true> >(
        false, s.mean, s.quat, s.scale, s.in_opacity, c.R, c.t, c.intrin.x,
        c.intrin.y, c.intrin.z, c.intrin.w, c.dist, width, height,
        float2(0.0f), 0.0f, v_sscale, v_sopac, g.v_mean, g.v_quat, g.v_scale,
        g.v_opacity, g.v_R, g.v_t);
}

// SH color VJP with the value-quant bit depth folded from kShValueBits
// (generic args must be parse-time constants; the branch folds away).
// `coeffs` is the splat's fp32 SH pointer (unused when kShValueBits != 0).
[ForceInline]
void pb_sh_vjp<D : ICameraDistortion>(
    PbSplat s, PbCamera<D> c, float* coeffs, uint* qpacked, float2* qbounds,
    gindex_t qbase, gindex_t qstride, gindex_t qpitch, float3 v_rgb,
    inout PbGrads g) {
    if (kShValueBits == 8)
        sh_color_vjp<8>(kShDegree, s.mean, c.R, c.t, s.dc, coeffs, qpacked,
                        qbounds, qbase, qstride, qpitch, v_rgb, g.v_dc, g.v_sh,
                        g.v_mean, g.v_R, g.v_t);
    else if (kShValueBits == 16)
        sh_color_vjp<16>(kShDegree, s.mean, c.R, c.t, s.dc, coeffs, qpacked,
                         qbounds, qbase, qstride, qpitch, v_rgb, g.v_dc, g.v_sh,
                         g.v_mean, g.v_R, g.v_t);
    else
        sh_color_vjp<0>(kShDegree, s.mean, c.R, c.t, s.dc, coeffs, qpacked,
                        qbounds, qbase, qstride, qpitch, v_rgb, g.v_dc, g.v_sh,
                        g.v_mean, g.v_R, g.v_t);
}

// ---- CAS scatter helpers (mirror atomicAddFVec / the atomic SH writes) ----

[ForceInline]
void pb_add1(float* dst, float v) {
    atomic_add_f32(dst, isfinite(v) ? v : 0.0f);
}
[ForceInline]
void pb_add3(float* dst, float3 v) {
    pb_add1(dst, v.x);
    pb_add1(dst + 1, v.y);
    pb_add1(dst + 2, v.z);
}
[ForceInline]
void pb_add4(float* dst, float4 v) {
    pb_add1(dst, v.x);
    pb_add1(dst + 1, v.y);
    pb_add1(dst + 2, v.z);
    pb_add1(dst + 3, v.w);
}

// Scatter the SH gradient array: CUDA's per-coefficient predicate
// (isfinite(dot) && dot != 0) from _sh_coeffs_to_color_vjp<2>, then one CAS
// add per channel.
[ForceInline]
void pb_scatter_sh(float* v_sh, float3 v_coeffs[24], int degree) {
    const int K = degree * (degree + 2);
    for (int j = 0; j < K; j++) {
        float3 v = v_coeffs[j];
        float d = dot(v, v);
        if (isfinite(d) && d != 0.0f) {
            atomic_add_f32(v_sh + 3 * j, v.x);
            atomic_add_f32(v_sh + 3 * j + 1, v.y);
            atomic_add_f32(v_sh + 3 * j + 2, v.z);
        }
    }
}

// v_R / v_t -> v_viewmats[cid] (row-major [4,4]; only the 3x4 block).
[ForceInline]
void pb_scatter_viewmat(float* v_viewmats, uint cid, float3x3 v_R,
                        float3 v_t) {
    float* dst = v_viewmats + cid * 16;
    [ForceUnroll]
    for (int i = 0; i < 3; i++) {
        atomic_add_f32(dst + i * 4 + 0, v_R[i].x);
        atomic_add_f32(dst + i * 4 + 1, v_R[i].y);
        atomic_add_f32(dst + i * 4 + 2, v_R[i].z);
    }
    atomic_add_f32(dst + 0 * 4 + 3, v_t.x);
    atomic_add_f32(dst + 1 * 4 + 3, v_t.y);
    atomic_add_f32(dst + 2 * 4 + 3, v_t.z);
}
