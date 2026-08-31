// Grad-quant projection backward (Vulkan backend), mirroring
// kernels/projection/ProjectionBwdQuantGrad_kernel.cuh: SPLAT-parallel (one thread per
// splat, 256/block); each thread loops its cameras in registers (no
// atomics), then folds the register gradient onto the running QUANTIZED
// grad accumulator via decode -> add -> block-reduce bounds -> encode.
// Codecs from optim_quant.slang (signed symmetric gradq; And+Or masked
// sub-word stores since per-splat cell runs need not be word-aligned).
//
// Spec constants 0..4 from proj_bwd_common.slang, plus:
//   kEval3d       = 1: 3DGUT screen-grad layout + VJP
//   kGqMask       = active-attribute bitmask (1 means, 2 quats, 4 scales,
//                   8 opac, 16 dc, 32 sh) -- folded so inactive attributes'
//                   loads never exist (llvmpipe speculated-load rule)
//   kWorldGradAdd = 1: add mean/quat/scale register grads onto the fp32
//                   world-grad buffer (3DGUT; one owner thread -> plain add)
//   kDistortion   = CameraDistortionType (0..3), declared last
//
// One thread per splat also for !inside tail threads: the camera loop is
// fully skipped for them (CUDA reads out-of-bounds aabb entries harmlessly
// from pool slack; BDA reads must stay in bounds), and every thread reaches
// every block reduce.

#define CudaDeviceExport ForceInline
#include "int64_compat.slang"
#include "screen.slang"
#include "shaders/primitive_3dgs.slang"
#include "shaders/harmonics.slang"
#undef CudaDeviceExport
#include "sh_quant.slang"
#include "sh_vjp.slang"
#include "atomic_float.slang"
#include "proj_bwd_common.slang"
#include "optim_quant.slang"

[SpecializationConstant]
const int kEval3d = 0;
[SpecializationConstant]
const int kGqMask = 0;
[SpecializationConstant]
const int kWorldGradAdd = 0;

struct ProjectionQgradParams {
    // splat world source values
    float* means;
    float* quats;
    float* scales;
    float* opacities;
    float* features_dc;
    float* features_sh;
    // cameras
    float* viewmats;
    float4* intrins;
    float* dist_coeffs;
    // packed intersection ranges (kPacked; fallback otherwise)
    int* camera_id_bounds;  // [N+1]
    int* camera_ids;        // [nnz] ORIGINAL order
    float4* aabb;           // [C,N] or [nnz] (original order)
    // packed screen-space gradient rows (SCR2_* for 2D, SCRG_* for 3DGUT)
    float* vs_screen;
    // fp32 world grads (kWorldGradAdd: mean/quat/scale plain add)
    float* vw_means;
    float* vw_quats;
    float* vw_scales;
    // quantized grad accumulators (16-bit non-SH, 8-bit SH)
    uint* gq_means_packed;
    float2* gq_means_bounds;
    uint* gq_quats_packed;
    float2* gq_quats_bounds;
    uint* gq_scales_packed;
    float2* gq_scales_bounds;
    uint* gq_opac_packed;
    float2* gq_opac_bounds;
    uint* gq_dc_packed;
    float2* gq_dc_bounds;
    uint* gq_sh_packed;
    float2* gq_sh_bounds;
    // SH VALUE quant source (kShValueBits 8/16)
    uint* sh_value_packed;
    float2* sh_value_bounds;
    i64mem_t sh_value_bounds_stride;
    uint32_t sh_stride_src;
    uint32_t C;
    uint32_t N;  // num_splats (splat-parallel even when packed)
    uint32_t width;
    uint32_t height;
    uint32_t wgs_per_row;
    uint32_t num_sh_buffer;
    uint32_t _pad0;
};

// One non-SH attribute: decode running grad + add this sub-batch's
// register contribution, block-reduce fresh bounds, re-encode. The block
// reduce ALWAYS runs (uniform barriers); `active` is spec-folded.
[ForceInline]
void _qg_nonsh<let PRIMS : int>(bool active, uint* packed, float2* bounds,
                                uint block_id, uint gid, uint tid,
                                bool inside, bool blk_ok,
                                float vals_in[PRIMS]) {
    float vals[PRIMS];
    float2 mm_t = float2(1e30f, -1e30f);
    if (active && inside) {
        float2 old = bounds[block_id];
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++) {
            vals[i] = gradq_decode1<16>(packed, uint(PRIMS) * gid + uint(i),
                                        old) +
                      vals_in[i];
            mm_t.x = min(mm_t.x, vals[i]);
            mm_t.y = max(mm_t.y, vals[i]);
        }
    } else {
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++)
            vals[i] = 0.0f;
    }
    float2 mm = oq_block_reduce_mm2(mm_t, tid);
    if (active && tid == 0 && blk_ok)
        bounds[block_id] = mm;
    if (active && inside) {
        // Contiguous PRIMS-cell run: plain word stores except the run edges
        // (see vkq_run_put_u16).
        uint c0 = uint(PRIMS) * gid;
        uint acc = 0u;
        [ForceUnroll]
        for (int i = 0; i < PRIMS; i++)
            vkq_run_put_u16(packed, c0, c0 + uint(i),
                            gradq_encode1<16>(vals[i], mm), acc);
        vkq_run_flush_u16(packed, c0 + uint(PRIMS), acc);
    }
}

// The per-splat camera loop, generic over the two projection spec axes so the
// entry point folds them once (its block reduces stay outside).
[ForceInline]
void _qg_camera_loop<let cam : CameraModelType, D : ICameraDistortion>(
    ProjectionQgradParams p, uint gid, bool inside, int cid_0, int cid_1,
    inout PbGrads g) {
    if (!inside)
        return;
    PbSplat s = pb_load_splat(p.means, p.quats, p.scales, p.opacities,
                              p.features_dc, gid);
    ShAddrW sha = sh_quant_addr_w(p.num_sh_buffer,
                                 i64mem_get(p.sh_value_bounds_stride));
    gindex_t qbase = sh_addr_base(sha, gid);
    for (int cid_t = cid_0; cid_t < cid_1; ++cid_t) {
        // Packed: the forward emits (gaussian, camera) order, so cid_t is
        // the out_idx that indexes aabb / camera_ids / vs_screen.
        int idx = (kPacked != 0) ? cid_t : cid_t * int(p.N) + int(gid);
        int cid = (kPacked != 0) ? p.camera_ids[idx] : cid_t;
        float4 bb = p.aabb[idx];
        if (bb.z <= bb.x || bb.w <= bb.y)
            continue;
        PbCamera<D> c = pb_load_camera<D>(p.viewmats, p.intrins,
                                          p.dist_coeffs, uint(cid));
        if (kEval3d != 0) {
            float* v_row = scrg_row(p.vs_screen, idx);
            float3 v_ss = scrg_scale(v_row);
            float v_so = scrg_opac(v_row);
            float3 v_rgb = scrg_rgb(v_row);
            pb_vjp_3dgut<cam, D>(s, c, p.width, p.height, v_ss, v_so, g);
            pb_sh_vjp<D>(s, c, p.features_sh + gid * p.sh_stride_src,
                         p.sh_value_packed, p.sh_value_bounds, qbase,
                         sha.bounds_stride, sha.pair_pitch, v_rgb, g);
        } else {
            float* v_row = scr2_row(p.vs_screen, idx);
            float2 v_xy = scr2_xy(v_row);
            float v_depth = scr2_depth(v_row);
            float3 v_conic = scr2_conic(v_row);
            float v_opac = scr2_opac(v_row);
            float3 v_rgb = scr2_rgb(v_row);
            pb_vjp_2d<cam, D>(s, c, p.width, p.height, v_xy, v_depth, v_conic,
                              v_opac, g);
            pb_sh_vjp<D>(s, c, p.features_sh + gid * p.sh_stride_src,
                         p.sh_value_packed, p.sh_value_bounds, qbase,
                         sha.bounds_stride, sha.pair_pitch, v_rgb, g);
        }
    }
}

#define _qg_call(M, D) _qg_camera_loop<M, D>(p, gid, inside, cid_0, cid_1, g)

[shader("compute")]
[numthreads(256, 1, 1)]
void projection_qgrad(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform ProjectionQgradParams* pp) {
    ProjectionQgradParams p = *pp;
    uint block_id = wg.y * p.wgs_per_row + wg.x;
    uint gid = block_id * 256 + tid;
    bool inside = gid < p.N;
    // Grid-fold tail: block_id can pad past the last quant block, so bounds
    // writes are blk_ok-gated (optimizer.slang convention).
    bool blk_ok = block_id < (p.N + 255u) / 256u;

    int cid_0 = 0, cid_1 = int(p.C);
    if (kPacked != 0) {
        cid_0 = inside ? p.camera_id_bounds[gid] : 0;
        cid_1 = inside ? p.camera_id_bounds[gid + 1] : 0;
    }

    PbGrads g = pb_grads_zero();
    SS_DISPATCH_CAM_DIST(_qg_call)

    // fp32 geometry writeback (3DGUT: add proj contribution onto the
    // raster-bwd world grads; one owner thread per gid -> race-free adds).
    if (kWorldGradAdd != 0 && inside) {
        [ForceUnroll]
        for (int k = 0; k < 3; k++)
            p.vw_means[3 * gid + k] += g.v_mean[k];
        [ForceUnroll]
        for (int k = 0; k < 4; k++)
            p.vw_quats[4 * gid + k] += g.v_quat[k];
        [ForceUnroll]
        for (int k = 0; k < 3; k++)
            p.vw_scales[3 * gid + k] += g.v_scale[k];
    }

    // Quantized attribute writebacks (each block-reduces unconditionally).
    {
        float vm[3] = {g.v_mean.x, g.v_mean.y, g.v_mean.z};
        _qg_nonsh<3>((kGqMask & 1) != 0, p.gq_means_packed,
                     p.gq_means_bounds, block_id, gid, tid, inside, blk_ok,
                     vm);
        float vq[4] = {g.v_quat.x, g.v_quat.y, g.v_quat.z, g.v_quat.w};
        _qg_nonsh<4>((kGqMask & 2) != 0, p.gq_quats_packed,
                     p.gq_quats_bounds, block_id, gid, tid, inside, blk_ok,
                     vq);
        float vs[3] = {g.v_scale.x, g.v_scale.y, g.v_scale.z};
        _qg_nonsh<3>((kGqMask & 4) != 0, p.gq_scales_packed,
                     p.gq_scales_bounds, block_id, gid, tid, inside, blk_ok,
                     vs);
        float vo[1] = {g.v_opacity};
        _qg_nonsh<1>((kGqMask & 8) != 0, p.gq_opac_packed, p.gq_opac_bounds,
                     block_id, gid, tid, inside, blk_ok, vo);
        float vd[3] = {g.v_dc.x, g.v_dc.y, g.v_dc.z};
        _qg_nonsh<3>((kGqMask & 16) != 0, p.gq_dc_packed, p.gq_dc_bounds,
                     block_id, gid, tid, inside, blk_ok, vd);
    }

    // SH grad (8-bit): two passes over the full 3*K cells so inactive
    // warmup bands (contribution 0) stay re-encoded against the fresh bound.
    if ((kGqMask & 32) != 0) {
        const int num_sh = kShDegree * (kShDegree + 2);
        const uint sh_base = 3u * p.num_sh_buffer * gid;
        const int total = int(3u * p.num_sh_buffer);
        const float2 sh_old =
            inside ? p.gq_sh_bounds[block_id] : float2(0.0f, 0.0f);
        float2 sh_mm = float2(1e30f, -1e30f);
        if (inside) {
            // Split so the v_sh read is a constant channel index; the inactive
            // warmup bands contribute nothing and only re-encode.
            for (int j = 0; j < num_sh; ++j) {
                [ForceUnroll]
                for (int ch = 0; ch < 3; ++ch) {
                    float nv = gradq_decode1<8>(
                                   p.gq_sh_packed,
                                   sh_base + uint(3 * j + ch), sh_old) +
                               g.v_sh[j][ch];
                    sh_mm.x = min(sh_mm.x, nv);
                    sh_mm.y = max(sh_mm.y, nv);
                }
            }
            for (int c = 3 * num_sh; c < total; ++c) {
                float nv = gradq_decode1<8>(p.gq_sh_packed,
                                            sh_base + uint(c), sh_old);
                sh_mm.x = min(sh_mm.x, nv);
                sh_mm.y = max(sh_mm.y, nv);
            }
        }
        sh_mm = oq_block_reduce_mm2(sh_mm, tid);
        if (tid == 0 && blk_ok)
            p.gq_sh_bounds[block_id] = sh_mm;

        // Whole-word writeback of this thread's contiguous byte-cell run. The
        // per-cell stores it replaces cost a masked atomic pair each; here only
        // the up-to-two edge words are shared with a neighbouring splat.
        if (inside) {
            uint c0 = sh_base, c1 = sh_base + uint(total);
            uint acc = 0u;
            for (uint cell = c0; cell < c1; ++cell) {
                int c = int(cell - c0);
                float old = gradq_decode1<8>(p.gq_sh_packed, cell, sh_old);
                float contrib =
                    (c < 3 * num_sh) ? g.v_sh[c / 3][c % 3] : 0.0f;
                uint code = gradq_encode1<8>(old + contrib, sh_mm) & 0xffu;
                uint b = cell & 3u;
                acc |= code << (8u * b);
                if (b == 3u || cell + 1u == c1) {
                    uint w = cell >> 2;
                    uint fb = (w == (c0 >> 2)) ? (c0 & 3u) : 0u;
                    if (fb == 0u && b == 3u) {
                        p.gq_sh_packed[w] = acc;
                    } else {
                        uint mask = (0xffffffffu >> (8u * (3u - b))) &
                                    (0xffffffffu << (8u * fb));
                        InterlockedAnd(p.gq_sh_packed[w], ~mask);
                        InterlockedOr(p.gq_sh_packed[w], acc & mask);
                    }
                    acc = 0u;
                }
            }
        }
    }
}

#undef _qg_call

// ---------------------------------------------------------------------------
// Packed-mode preprocessing helpers (mirror the FPBO/qgrad iota +
// camera_id_bounds kernels; the sort itself runs via backend::sort_pairs).
// ---------------------------------------------------------------------------

struct QgIotaParams {  // 16 bytes: pushed directly
    int* buf;
    uint32_t n;
    uint32_t wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void qgrad_iota(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                uniform QgIotaParams p) {
    uint i = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (i < p.n)
        p.buf[i] = int(i);
}

struct QgCamBoundsParams {  // 24 bytes: pushed directly
    int* gaussian_ids_sorted;  // [nnz], sorted by gid
    int* camera_id_bounds;     // [N+1]
    uint32_t nnz;
    uint32_t N;
    uint32_t wgs_per_row;
    uint32_t _pad0;
};

// lower_bound over the sorted list, one thread per OUTPUT slot. Filling the
// gaps from the input side makes one thread walk every id a sub-batch never
// saw: 478 ms/call at N=20M, nnz=0.8M against ~2 ms here.
[shader("compute")]
[numthreads(256, 1, 1)]
void qgrad_camera_id_bounds(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                            uniform QgCamBoundsParams p) {
    uint k = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (k > p.N)
        return;
    int lo = 0, hi = int(p.nnz);
    while (lo < hi) {
        int mid = lo + ((hi - lo) >> 1);
        if (p.gaussian_ids_sorted[mid] < int(k)) lo = mid + 1;
        else hi = mid;
    }
    p.camera_id_bounds[k] = lo;
}

// Declared last: spec IDs follow declaration order (see dist_spec.slang).
[SpecializationConstant]
const int kDistortion = 0;
