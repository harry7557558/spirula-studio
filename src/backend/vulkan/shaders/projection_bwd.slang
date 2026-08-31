// Fused projection backward (Vulkan backend), mirroring
// kernels/projection/ProjectionBwd_kernel.cuh: one thread per (camera, gaussian)
// intersection; per-intersection VJP machinery in proj_bwd_common.slang,
// gradients scattered with CAS float atomics (multiple cameras hit the same
// splat).
//
// Variants (specialization constants, matching the CUDA template axes;
// 0..4 declared in proj_bwd_common.slang):
//   kCameraModel  = CameraModelType (0..3)
//   kShDegree     = 0..4  (already capped at max_sh_degree by the host)
//   kAntialiased  = 0 (Vanilla3DGS) | 1 (MipSplatting); ignored by 3DGUT
//   kShValueBits  = 0 (fp32 SH source) | 8 | 16 (word-based codec source)
//   kPacked       = 1: camera_ids/gaussian_ids index nnz intersections
//   kViewmatGrad  = 1: accumulate v_R / v_t into v_viewmats (per-thread CAS
//                   adds -- CUDA's labeled-partition warp reduce is a perf
//                   shortcut with identical, order-nondeterministic sums)
//   kDistortion   = CameraDistortionType (0..3), declared last

#define CudaDeviceExport ForceInline
#include "aabb16.slang"
#include "int64_compat.slang"
#include "screen.slang"
#include "shaders/primitive_3dgs.slang"
#include "shaders/harmonics.slang"
#undef CudaDeviceExport
#include "sh_quant.slang"
#include "sh_vjp.slang"
#include "atomic_float.slang"
#include "proj_bwd_common.slang"

[SpecializationConstant]
const int kViewmatGrad = 0;

// ---------------------------------------------------------------------------
// 3DGS / MipSplatting: ScreenBuffer grads {xy, depth, conic, opac, rgb}.
// ---------------------------------------------------------------------------

struct ProjectionBwd3dgsParams {
    // splat world source values (layouts [N,3] [N,4] [N,3] [N,1] [N,3] [N,K3])
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    // cameras
    float* viewmats;     // [C,16] row-major
    float4* intrins;     // [C]
    float* dist_coeffs;  // [C,8] or fallback zeros
    // packed intersection ids (kPacked; fallback otherwise)
    int* camera_ids;    // [nnz]
    int* gaussian_ids;  // [nnz]
    uint2* aabb;        // [C,N] packed cull marker; unused when kPacked
    // screen-space gradients (inputs)
    float* vs_screen;  // [.., SCR2_STRIDE]
    // world-space gradients (outputs, CAS accumulate)
    float* vw_means;
    float* vw_quats;
    float* vw_scales;
    float* vw_opacities;
    float* vw_dc;
    float* vw_sh;
    float* v_viewmats;  // [C,16] (kViewmatGrad; fallback otherwise)
    // SH value-quant source (kShValueBits 8/16)
    uint* sh_packed;
    float2* sh_bounds;
    i64mem_t sh_bounds_stride;
    uint32_t sh_stride_src;  // floats per splat in features_sh
    uint32_t sh_stride_v;    // floats per splat in vw_sh
    uint32_t C;
    uint32_t N;  // num_splats, or nnz when kPacked
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
};

void _pb_run_3dgs<let cam : CameraModelType, D : ICameraDistortion>(
    ProjectionBwd3dgsParams p, uint idx) {
    uint cid, gid;
    if (kPacked != 0) {
        cid = uint(p.camera_ids[idx]);
        gid = uint(p.gaussian_ids[idx]);
    } else {
        if (aabb16_is_empty(p.aabb[idx]))
            return;
        cid = (idx / p.N) % p.C;
        gid = idx % p.N;
    }

    PbCamera<D> c = pb_load_camera<D>(p.viewmats, p.intrins, p.dist_coeffs,
                                      cid);
    PbSplat s = pb_load_splat(p.means, p.quats, p.scales, p.opacities,
                              p.features_dc, gid);

    float* v_row = scr2_row(p.vs_screen, idx);
    float2 v_xy = scr2_xy(v_row);
    float v_depth = scr2_depth(v_row);
    float3 v_conic = scr2_conic(v_row);
    float v_opac = scr2_opac(v_row);
    float3 v_rgb = scr2_rgb(v_row);

    PbGrads g = pb_grads_zero();
    pb_vjp_2d<cam, D>(s, c, p.width, p.height, v_xy, v_depth, v_conic, v_opac,
                      g);
    ShAddrW a = sh_quant_addr_w(p.num_sh_buffer,
                                i64mem_get(p.sh_bounds_stride));
    pb_sh_vjp<D>(s, c, p.features_sh + gid * p.sh_stride_src, p.sh_packed,
                 p.sh_bounds, sh_addr_base(a, gid), a.bounds_stride,
                 a.pair_pitch, v_rgb, g);

    pb_add3(p.vw_means + 3 * gid, g.v_mean);
    pb_add4(p.vw_quats + 4 * gid, g.v_quat);
    pb_add3(p.vw_scales + 3 * gid, g.v_scale);
    pb_add1(p.vw_opacities + gid, g.v_opacity);
    pb_add3(p.vw_dc + 3 * gid, g.v_dc);
    pb_scatter_sh(p.vw_sh + gid * p.sh_stride_v, g.v_sh, kShDegree);

    if (kViewmatGrad != 0)
        pb_scatter_viewmat(p.v_viewmats, cid, g.v_R, g.v_t);
}

#define _pb_call_2d(M, D) _pb_run_3dgs<M, D>(p, idx)

[shader("compute")]
[numthreads(128, 1, 1)]
void projection_bwd_3dgs(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform ProjectionBwd3dgsParams* pp) {
    ProjectionBwd3dgsParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    uint total = (kPacked != 0) ? p.N : p.C * p.N;
    if (idx >= total) return;
    SS_DISPATCH_CAM_DIST(_pb_call_2d)
}
#undef _pb_call_2d

// ---------------------------------------------------------------------------
// 3DGUT: ScreenBuffer grads {scale, opacity, rgb}.
// ---------------------------------------------------------------------------

struct ProjectionBwd3dgutParams {
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    int* camera_ids;
    int* gaussian_ids;
    uint2* aabb;        // packed, aabb16.slang
    // screen grads {scale, opac, rgb}
    float* vs_screen;  // [.., SCRG_STRIDE]
    // world grads (outputs)
    float* vw_means;
    float* vw_quats;
    float* vw_scales;
    float* vw_opacities;
    float* vw_dc;
    float* vw_sh;
    float* v_viewmats;
    uint* sh_packed;
    float2* sh_bounds;
    i64mem_t sh_bounds_stride;
    uint32_t sh_stride_src;
    uint32_t sh_stride_v;
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
};

void _pb_run_3dgut<let cam : CameraModelType, D : ICameraDistortion>(
    ProjectionBwd3dgutParams p, uint idx) {
    uint cid, gid;
    if (kPacked != 0) {
        cid = uint(p.camera_ids[idx]);
        gid = uint(p.gaussian_ids[idx]);
    } else {
        if (aabb16_is_empty(p.aabb[idx]))
            return;
        cid = (idx / p.N) % p.C;
        gid = idx % p.N;
    }

    PbCamera<D> c = pb_load_camera<D>(p.viewmats, p.intrins, p.dist_coeffs,
                                      cid);
    PbSplat s = pb_load_splat(p.means, p.quats, p.scales, p.opacities,
                              p.features_dc, gid);

    float* v_row = scrg_row(p.vs_screen, idx);
    float3 v_sscale = scrg_scale(v_row);
    float v_sopac = scrg_opac(v_row);
    float3 v_rgb = scrg_rgb(v_row);

    PbGrads g = pb_grads_zero();
    pb_vjp_3dgut<cam, D>(s, c, p.width, p.height, v_sscale, v_sopac, g);
    ShAddrW a = sh_quant_addr_w(p.num_sh_buffer,
                                i64mem_get(p.sh_bounds_stride));
    pb_sh_vjp<D>(s, c, p.features_sh + gid * p.sh_stride_src, p.sh_packed,
                 p.sh_bounds, sh_addr_base(a, gid), a.bounds_stride,
                 a.pair_pitch, v_rgb, g);

    pb_add3(p.vw_means + 3 * gid, g.v_mean);
    pb_add4(p.vw_quats + 4 * gid, g.v_quat);
    pb_add3(p.vw_scales + 3 * gid, g.v_scale);
    pb_add1(p.vw_opacities + gid, g.v_opacity);
    pb_add3(p.vw_dc + 3 * gid, g.v_dc);
    pb_scatter_sh(p.vw_sh + gid * p.sh_stride_v, g.v_sh, kShDegree);

    if (kViewmatGrad != 0)
        pb_scatter_viewmat(p.v_viewmats, cid, g.v_R, g.v_t);
}

#define _pb_call_3d(M, D) _pb_run_3dgut<M, D>(p, idx)

[shader("compute")]
[numthreads(128, 1, 1)]
void projection_bwd_3dgut(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform ProjectionBwd3dgutParams* pp) {
    ProjectionBwd3dgutParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    uint total = (kPacked != 0) ? p.N : p.C * p.N;
    if (idx >= total) return;
    SS_DISPATCH_CAM_DIST(_pb_call_3d)
}
#undef _pb_call_3d

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
