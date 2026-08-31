// Fused projection forward (Vulkan backend), mirroring
// kernels/projection/ProjectionFwd_kernel.cuh: one thread per (camera, gaussian), all math
// via the shared slang modules (primitive_3dgs.slang / harmonics.slang).
//
// Variants (specialization constants, matching the CUDA template axes):
//   kCameraModel  = CameraModelType (0..3)
//   kShDegree     = 0..4
//   kAntialiased  = 0 (Vanilla3DGS) | 1 (MipSplatting)
//   kShValueBits  = 0 (fp32) | 8 | 16 (the CUDA VALUE_BITS template axis;
//                   decode via sh_quant.slang's word-based q8/q16 loads)
//   kDistortion   = CameraDistortionType (0..3), declared last (dist_spec.slang)
//
// Params exceed the 128-byte push floor -> delivered via the params ring
// (push constant = one device address).

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "shaders/packed_mask.h"
#include "screen.slang"
#include "shaders/primitive_3dgs.slang"
#include "shaders/harmonics.slang"
#undef CudaDeviceExport
#include "sh_quant.slang"
#include "dist_spec.slang"

[SpecializationConstant]
const int kCameraModel = 0;
[SpecializationConstant]
const int kShDegree = 0;
[SpecializationConstant]
const int kAntialiased = 0;
[SpecializationConstant]
const int kShValueBits = 0;

// SH -> rgb for one splat, gated exactly like _Base3DGS::project: fp32
// features_sh when kShValueBits == 0, else the q8/q16 codec path
// (docs/notes/sh-quant-layout.md).
[ForceInline]
float3 eval_splat_color(float3 mean, float3x3 R, float3 t, float3 dc,
                        float* features_sh, uint sh_stride, uint gid,
                        uint* sh_packed, float2* sh_bounds,
                        uint num_sh_buffer, gindex_t sh_bounds_stride) {
    if (kShValueBits == 8 || kShValueBits == 16) {
        ShAddrW a = sh_quant_addr_w(num_sh_buffer, sh_bounds_stride);
        gindex_t base = sh_addr_base(a, gid);
        if (kShValueBits == 8)
            return sh_to_color_qw<8>(kShDegree, mean, R, t, dc, sh_packed,
                                     sh_bounds, base, a.bounds_stride,
                                     a.pair_pitch);
        return sh_to_color_qw<16>(kShDegree, mean, R, t, dc, sh_packed,
                                  sh_bounds, base, a.bounds_stride,
                                  a.pair_pitch);
    }
    float* sh = features_sh + gid * sh_stride;
    switch (kShDegree) {
    case 0: return sh0_to_color(mean, R, t, dc, sh);
    case 1: return sh1_to_color(mean, R, t, dc, sh);
    case 2: return sh2_to_color(mean, R, t, dc, sh);
    case 3: return sh3_to_color(mean, R, t, dc, sh);
    case 4: return sh4_to_color(mean, R, t, dc, sh);
    }
    return float3(0.0f);
}

struct ProjectionFwdParams {
    // WorldBuffer (TensorArray<6>) flattened; layouts [N,3] [N,4] [N,3]
    // [N,1] [N,3] [N, sh_stride].
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    // cameras
    float* viewmats;      // [C,16] row-major
    float4* intrins;      // [C]
    float* dist_coeffs;   // [C,8] or null
    // outputs
    float4* out_aabb;     // [C,N]
    float* out_depths;    // [C,N]
    uint32_t* out_radii;  // [N] float bits; atomic max (radius >= 0)
    float* s_screen;      // packed ScreenBuffer rows [.., SCR2_STRIDE]
    // SH value-quant (kShValueBits 8/16); null / 0 in the fp32 path.
    uint* sh_packed;
    float2* sh_bounds;
    i64mem_t sh_bounds_stride;  // cells per bound, resolved host-side
    uint32_t sh_stride;   // floats per splat in features_sh
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row; // grid fold (flat id = wg.y * wgs_per_row*128 + x)
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};

void run_projection<let cam: CameraModelType, D : ICameraDistortion>(
    ProjectionFwdParams p, uint idx) {
    uint cid = (idx / p.N) % p.C;
    uint gid = idx % p.N;

    // camera
    float* vm = p.viewmats + cid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    float4 intrin = p.intrins[cid];
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, cid, dist);

    // splat
    float3 mean = float3(p.means[3 * gid], p.means[3 * gid + 1],
                         p.means[3 * gid + 2]);
    float4 quat = float4(p.quats[4 * gid], p.quats[4 * gid + 1],
                         p.quats[4 * gid + 2], p.quats[4 * gid + 3]);
    float3 scale = float3(p.scales[3 * gid], p.scales[3 * gid + 1],
                          p.scales[3 * gid + 2]);
    float in_opacity = p.opacities[gid];
    float3 dc = float3(p.features_dc[3 * gid], p.features_dc[3 * gid + 1],
                       p.features_dc[3 * gid + 2]);

    float4 aabb;
    float sorting_depth, radius;
    float2 s_xy;
    float s_depth;
    float3 s_conic;
    float s_opac;
    projection_3dgs<cam, D, false, false>(
        kAntialiased != 0,
        mean, quat, scale, in_opacity, R, t,
        intrin.x, intrin.y, intrin.z, intrin.w, dist,
        p.width, p.height,
        aabb, sorting_depth, radius, s_xy, s_depth, s_conic, s_opac);

    // SH -> rgb, gated exactly like _Base3DGS::project.
    float3 rgb = float3(0.0f);
    if (aabb.z > aabb.x && aabb.w > aabb.y)
        rgb = eval_splat_color(mean, R, t, dc, p.features_sh, p.sh_stride,
                               gid, p.sh_packed, p.sh_bounds,
                               p.num_sh_buffer, i64mem_get(p.sh_bounds_stride));

    // store (mirrors the kernel epilogue: clamp, degenerate cull, atomics)
    aabb.x = clamp(aabb.x, 0.0f, float(p.width) - 1.0f);
    aabb.y = clamp(aabb.y, 0.0f, float(p.height) - 1.0f);
    aabb.z = clamp(aabb.z, 0.0f, float(p.width) - 1.0f);
    aabb.w = clamp(aabb.w, 0.0f, float(p.height) - 1.0f);
    if (aabb.z - aabb.x > 1e-3f && aabb.w - aabb.y > 1e-3f) {
        if (!is_null(p.s_screen))
            scr2_store(scr2_row(p.s_screen, idx), s_xy, s_depth, s_conic,
                       s_opac, rgb);
        p.out_aabb[idx] = aabb;
        p.out_depths[idx] = sorting_depth;
        // float atomicMax on non-negative floats == uint max on bits.
        InterlockedMax(p.out_radii[gid], asuint(radius));
    } else {
        p.out_aabb[idx] = float4(0.0f);
        p.out_depths[idx] = 0.0f;
    }
}

#define _pf_call(M, D) run_projection<M, D>(p, idx)

[shader("compute")]
[numthreads(128, 1, 1)]
void projection_fwd_3dgs(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform ProjectionFwdParams* pp) {
    ProjectionFwdParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.C * p.N) return;
    SS_DISPATCH_CAM_DIST(_pf_call)
}
#undef _pf_call

// ---------------------------------------------------------------------------
// 3DGUT (eval3d): projection_3dgs<cam, D, false, true>, antialiased always
// false; the projected 2D conic lands in the screen row's "scale" slot, which
// is what the eval3d rasterizer culls with. Mirrors Primitive3DGUT::project().
// ---------------------------------------------------------------------------

struct Projection3dgutParams {
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    float4* out_aabb;
    float* out_depths;
    uint32_t* out_radii;
    float* s_screen;  // [C*N, SCRG_STRIDE]
    uint* sh_packed;
    float2* sh_bounds;
    i64mem_t sh_bounds_stride;
    uint32_t sh_stride;
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};

void run_projection_3dgut<let cam: CameraModelType, D : ICameraDistortion>(
    Projection3dgutParams p, uint idx) {
    uint cid = (idx / p.N) % p.C;
    uint gid = idx % p.N;

    float* vm = p.viewmats + cid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    float4 intrin = p.intrins[cid];
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, cid, dist);

    float3 mean = float3(p.means[3 * gid], p.means[3 * gid + 1],
                         p.means[3 * gid + 2]);
    float4 quat = float4(p.quats[4 * gid], p.quats[4 * gid + 1],
                         p.quats[4 * gid + 2], p.quats[4 * gid + 3]);
    float3 scale = float3(p.scales[3 * gid], p.scales[3 * gid + 1],
                          p.scales[3 * gid + 2]);
    float in_opacity = p.opacities[gid];
    float3 dc = float3(p.features_dc[3 * gid], p.features_dc[3 * gid + 1],
                       p.features_dc[3 * gid + 2]);

    float4 aabb;
    float sorting_depth, radius;
    float2 xy;
    float depth;
    float3 s_scale;
    float s_opac;
    projection_3dgs<cam, D, false, true>(
        false,  // antialiased: always false for 3DGUT
        mean, quat, scale, in_opacity, R, t,
        intrin.x, intrin.y, intrin.z, intrin.w, dist,
        p.width, p.height,
        aabb, sorting_depth, radius, xy, depth, s_scale, s_opac);

    float3 rgb = float3(0.0f);
    if (aabb.z > aabb.x && aabb.w > aabb.y)
        rgb = eval_splat_color(mean, R, t, dc, p.features_sh, p.sh_stride,
                               gid, p.sh_packed, p.sh_bounds,
                               p.num_sh_buffer, i64mem_get(p.sh_bounds_stride));

    aabb.x = clamp(aabb.x, 0.0f, float(p.width) - 1.0f);
    aabb.y = clamp(aabb.y, 0.0f, float(p.height) - 1.0f);
    aabb.z = clamp(aabb.z, 0.0f, float(p.width) - 1.0f);
    aabb.w = clamp(aabb.w, 0.0f, float(p.height) - 1.0f);
    if (aabb.z - aabb.x > 1e-3f && aabb.w - aabb.y > 1e-3f) {
        if (!is_null(p.s_screen))
            scrg_store(scrg_row(p.s_screen, idx), s_scale, s_opac, rgb);
        p.out_aabb[idx] = aabb;
        p.out_depths[idx] = sorting_depth;
        InterlockedMax(p.out_radii[gid], asuint(radius));
    } else {
        p.out_aabb[idx] = float4(0.0f);
        p.out_depths[idx] = 0.0f;
    }
}

#define _pg_call(M, D) run_projection_3dgut<M, D>(p, idx)

[shader("compute")]
[numthreads(128, 1, 1)]
void projection_fwd_3dgut(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform Projection3dgutParams* pp) {
    Projection3dgutParams p = *pp;
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.C * p.N) return;
    SS_DISPATCH_CAM_DIST(_pg_call)
}
#undef _pg_call

// ---------------------------------------------------------------------------
// Packed projection (two-pass compaction, ProjectionPackedFwd_kernel.cuh).
// Pass 1 writes an int32 0/1 mask (the CUDA bool mask would need 8-bit
// stores) which the host scans with backend::inclusive_sum<int32>; pass 2
// recomputes the projection and scatters into nnz-compacted outputs.
// Spec constant 3 (kEval3d) selects the 3DGUT projection in the mask pass.
// ---------------------------------------------------------------------------

[SpecializationConstant]
const int kEval3d = 0;

struct PackedMaskParams {  // fits in the 128-byte push floor: pushed direct
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    uint* out_bits;      // [wgs * SS_PMASK_WORDS] visibility bitmask
    int* out_counts;     // [wgs] popcount of this workgroup's bits
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
};

// GAUSSIAN-major, so the nnz list the prefix scan compacts comes out sorted
// by gaussian id and the backward passes need no sort of their own.
bool run_packed_mask<let cam: CameraModelType, D : ICameraDistortion,
                     let eval3d: bool>(PackedMaskParams p, uint idx) {
    uint gid = idx / p.C;
    uint cid = idx % p.C;

    float* vm = p.viewmats + cid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    float4 intrin = p.intrins[cid];
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, cid, dist);

    float3 mean = float3(p.means[3 * gid], p.means[3 * gid + 1],
                         p.means[3 * gid + 2]);
    float4 quat = float4(p.quats[4 * gid], p.quats[4 * gid + 1],
                         p.quats[4 * gid + 2], p.quats[4 * gid + 3]);
    float3 scale = float3(p.scales[3 * gid], p.scales[3 * gid + 1],
                          p.scales[3 * gid + 2]);
    float in_opacity = p.opacities[gid];

    float4 aabb;
    float sorting_depth, radius;
    float2 xy;
    float depth;
    float3 conic;
    float opac;
    // The mask only consumes the AABB; SH evaluation is skipped (the CUDA
    // mask kernel computes and discards it).
    projection_3dgs<cam, D, false, eval3d>(
        !eval3d && kAntialiased != 0,
        mean, quat, scale, in_opacity, R, t,
        intrin.x, intrin.y, intrin.z, intrin.w, dist,
        p.width, p.height,
        aabb, sorting_depth, radius, xy, depth, conic, opac);

    aabb.x = clamp(aabb.x, 0.0f, float(p.width) - 1.0f);
    aabb.y = clamp(aabb.y, 0.0f, float(p.height) - 1.0f);
    aabb.z = clamp(aabb.z, 0.0f, float(p.width) - 1.0f);
    aabb.w = clamp(aabb.w, 0.0f, float(p.height) - 1.0f);
    return aabb.z - aabb.x > 1e-3f && aabb.w - aabb.y > 1e-3f;
}

#define _pm_call_3d(M, D) keep = run_packed_mask<M, D, true>(p, idx)
#define _pm_call_2d(M, D) keep = run_packed_mask<M, D, false>(p, idx)

groupshared uint g_mask_bits[SS_PMASK_WORDS];

[shader("compute")]
[numthreads(SS_PMASK_BLOCK, 1, 1)]
void projection_packed_mask(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform PackedMaskParams p) {
    if (tid < SS_PMASK_WORDS) g_mask_bits[tid] = 0u;
    GroupMemoryBarrierWithGroupSync();

    uint blk = wg.y * p.wgs_per_row + wg.x;
    uint idx = blk * SS_PMASK_BLOCK + tid;
    if (idx < p.C * p.N) {
        bool keep = false;
        if (kEval3d != 0) {
            SS_DISPATCH_CAM_DIST(_pm_call_3d)
        } else {
            SS_DISPATCH_CAM_DIST(_pm_call_2d)
        }
        if (keep) InterlockedOr(g_mask_bits[tid >> 5], 1u << (tid & 31));
    }
    GroupMemoryBarrierWithGroupSync();

    if (tid < SS_PMASK_WORDS)
        p.out_bits[blk * SS_PMASK_WORDS + tid] = g_mask_bits[tid];
    if (tid == 0) {
        int n = 0;
        for (uint k = 0; k < SS_PMASK_WORDS; ++k)
            n += int(countbits(g_mask_bits[k]));
        p.out_counts[blk] = n;
    }
}
#undef _pm_call_3d
#undef _pm_call_2d

struct PackedFwdParams {
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    uint* mask_bits;        // [wgs * SS_PMASK_WORDS] visibility bitmask
    int* block_scan;        // [wgs] inclusive scan of the workgroup counts
    int* out_camera_ids;    // [nnz]
    int* out_gaussian_ids;  // [nnz]
    float4* out_aabb;       // [nnz]
    float* out_depths;      // [nnz]
    uint32_t* out_radii;    // [N] float bits, atomic max
    float* s_screen;        // packed rows: SCR2_* for 2D, SCRG_* for 3DGUT
    uint* sh_packed;
    float2* sh_bounds;
    i64mem_t sh_bounds_stride;
    uint32_t sh_stride;
    uint32_t C;
    uint32_t N;
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};

void run_packed_fwd<let cam: CameraModelType, D : ICameraDistortion,
                    let eval3d: bool>(PackedFwdParams p, uint idx, int out_idx) {
    uint gid = idx / p.C;  // see run_packed_mask
    uint cid = idx % p.C;

    float* vm = p.viewmats + cid * 16;
    float3x3 R;
    float3 t;
    [ForceUnroll]
    for (int r = 0; r < 3; r++) {
        R[r] = float3(vm[4 * r + 0], vm[4 * r + 1], vm[4 * r + 2]);
        t[r] = vm[4 * r + 3];
    }
    float4 intrin = p.intrins[cid];
    float dist[D.kNumParams];
    load_dist_coeffs<D>(p.dist_coeffs, cid, dist);

    float3 mean = float3(p.means[3 * gid], p.means[3 * gid + 1],
                         p.means[3 * gid + 2]);
    float4 quat = float4(p.quats[4 * gid], p.quats[4 * gid + 1],
                         p.quats[4 * gid + 2], p.quats[4 * gid + 3]);
    float3 scale = float3(p.scales[3 * gid], p.scales[3 * gid + 1],
                          p.scales[3 * gid + 2]);
    float in_opacity = p.opacities[gid];
    float3 dc = float3(p.features_dc[3 * gid], p.features_dc[3 * gid + 1],
                       p.features_dc[3 * gid + 2]);

    float4 aabb;
    float sorting_depth, radius;
    float2 xy;
    float depth;
    float3 conic;  // eval3d: carries the culling scale (see 3DGUT notes)
    float opac;
    projection_3dgs<cam, D, false, eval3d>(
        !eval3d && kAntialiased != 0,
        mean, quat, scale, in_opacity, R, t,
        intrin.x, intrin.y, intrin.z, intrin.w, dist,
        p.width, p.height,
        aabb, sorting_depth, radius, xy, depth, conic, opac);

    float3 rgb = float3(0.0f);
    if (aabb.z > aabb.x && aabb.w > aabb.y)
        rgb = eval_splat_color(mean, R, t, dc, p.features_sh, p.sh_stride,
                               gid, p.sh_packed, p.sh_bounds,
                               p.num_sh_buffer, i64mem_get(p.sh_bounds_stride));

    p.out_camera_ids[out_idx] = int(cid);
    p.out_gaussian_ids[out_idx] = int(gid);
    aabb.x = clamp(aabb.x, 0.0f, float(p.width) - 1.0f);
    aabb.y = clamp(aabb.y, 0.0f, float(p.height) - 1.0f);
    aabb.z = clamp(aabb.z, 0.0f, float(p.width) - 1.0f);
    aabb.w = clamp(aabb.w, 0.0f, float(p.height) - 1.0f);
    p.out_aabb[out_idx] = aabb;
    p.out_depths[out_idx] = sorting_depth;
    if (!is_null(p.s_screen)) {
        if (eval3d)  // Screen = {scale, opacity, rgb}
            scrg_store(scrg_row(p.s_screen, out_idx), conic, opac, rgb);
        else         // Screen = {xy, conic, opac, depth, rgb}
            scr2_store(scr2_row(p.s_screen, out_idx), xy, depth, conic, opac,
                       rgb);
    }
    InterlockedMax(p.out_radii[gid], asuint(radius));
}

#define _pk_call_3d(M, D) run_packed_fwd<M, D, true>(p, idx, out_idx)
#define _pk_call_2d(M, D) run_packed_fwd<M, D, false>(p, idx, out_idx)

groupshared uint g_fwd_bits[SS_PMASK_WORDS];

[shader("compute")]
[numthreads(SS_PMASK_BLOCK, 1, 1)]
void projection_packed_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                           uniform PackedFwdParams* pp) {
    PackedFwdParams p = *pp;
    uint blk = wg.y * p.wgs_per_row + wg.x;
    if (tid < SS_PMASK_WORDS)
        g_fwd_bits[tid] = p.mask_bits[blk * SS_PMASK_WORDS + tid];
    GroupMemoryBarrierWithGroupSync();

    uint word = g_fwd_bits[tid >> 5];
    if (((word >> (tid & 31)) & 1u) == 0u) return;

    // Output slot = scanned count of the workgroups before this one + the bits
    // set below this thread inside it. Out-of-range lanes never set a bit, so
    // the range test is implicit.
    int out_idx = blk == 0 ? 0 : p.block_scan[blk - 1];
    for (uint k = 0; k < (tid >> 5); ++k)
        out_idx += int(countbits(g_fwd_bits[k]));
    out_idx += int(countbits(word & ((1u << (tid & 31)) - 1u)));

    uint idx = blk * SS_PMASK_BLOCK + tid;
    if (kEval3d != 0) {
        SS_DISPATCH_CAM_DIST(_pk_call_3d)
    } else {
        SS_DISPATCH_CAM_DIST(_pk_call_2d)
    }
}
#undef _pk_call_3d
#undef _pk_call_2d

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
