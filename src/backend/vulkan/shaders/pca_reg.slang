// Vulkan entries of the local-PCA regularizer (kernels/loss/PcaReg.cu); the
// device math is the shared shaders/pca_reg.slang. No 64-bit integer: the
// sort keys are written as (low, high) words.

#include "atomic_float.slang"

#define PCA_REG_ATOMIC_ADD(ptr, v) atomic_add_f32(ptr, v)
#define CudaDeviceExport ForceInline
#include "shaders/pca_reg.slang"
#undef CudaDeviceExport

static const uint kBoxGroup = 256;
static const uint kOwnerGroup = 128;

struct PcaBoxParams {  // pushed directly
    float* means;
    uint* box;         // [6] order keys: min xyz, max xyz
    uint n, stride;
};

groupshared uint g_box[6][kBoxGroup];

[shader("compute")]
[numthreads(256, 1, 1)]
void pca_box(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID, uniform PcaBoxParams p) {
    uint lo[3] = { 0xffffffffu, 0xffffffffu, 0xffffffffu };
    uint hi[3] = { 0u, 0u, 0u };
    for (uint i = wg.x * kBoxGroup + tid; i < p.n; i += p.stride) {
        for (uint c = 0; c < 3; c++) {
            float v = p.means[3 * i + c];
            if (isnan(v) || isinf(v)) continue;
            uint key = pca_order_key(v);
            lo[c] = min(lo[c], key);
            hi[c] = max(hi[c], key);
        }
    }
    for (uint c = 0; c < 3; c++) {
        g_box[c][tid] = lo[c];
        g_box[3 + c][tid] = hi[c];
    }
    GroupMemoryBarrierWithGroupSync();
    for (uint s = kBoxGroup / 2; s > 0; s >>= 1) {
        if (tid < s) {
            for (uint c = 0; c < 3; c++) {
                g_box[c][tid] = min(g_box[c][tid], g_box[c][tid + s]);
                g_box[3 + c][tid] = max(g_box[3 + c][tid], g_box[3 + c][tid + s]);
            }
        }
        GroupMemoryBarrierWithGroupSync();
    }
    if (tid < 3) InterlockedMin(p.box[tid], g_box[tid][0]);
    else if (tid < 6) InterlockedMax(p.box[tid], g_box[tid][0]);
}

struct PcaKeysParams {  // pushed directly
    float* means;
    uint* box;
    uint* keys;        // [2N]: the int64 key's low then high word
    int* vals;         // [N]
    uint n, wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void pca_keys(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID, uniform PcaKeysParams p) {
    uint i = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (i >= p.n) return;
    uint2 code = pca_reg_key(pca_load3(p.means, i), p.box);
    p.keys[2 * i] = code.x;
    p.keys[2 * i + 1] = code.y;
    p.vals[i] = int(i);
}

struct PcaGatherParams {  // pushed directly
    int* perm;
    float* means;
    float4* sorted;
    int* perm_out;
    uint n, write_perm, wgs_per_row, _pad0;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void pca_gather(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID, uniform PcaGatherParams p) {
    uint r = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (r >= p.n) return;
    pca_reg_gather(r, p.perm, p.means, p.sorted, p.perm_out, p.write_perm);
}

struct PcaBuildParams {  // pushed directly
    float4* sorted;
    uint* box;
    float4* nodes;
    uint* node_bits;   // nodes again, for the atomic reads
    int4* meta;
    int* ranges;
    uint n, wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void pca_build(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID, uniform PcaBuildParams p) {
    uint i = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (i >= p.n) return;
    pca_reg_build(i, p.n, p.sorted, p.box, p.nodes, p.node_bits, p.meta, p.ranges);
}

struct PcaOwnerParams {  // 168 bytes: through the params ring
    float* means;
    float* quats;
    float* scales;
    int* perm;
    float4* sorted;
    float4* nodes;
    int4* meta;
    int* ranges;
    int* nbrs;
    float* radii;
    float* v_means;
    float* v_quats;
    float* v_scales;
    float* loss_sums;  // [3]
    uint n, mode, k, window;
    uint store_list, wgs_per_row;
    float w_plane, w_tangent, w_normal, w_surfel;
    uint gate_means, _pad0;
};

groupshared float3 g_loss[kOwnerGroup];

// One entry per capacity (pca_reg_capacity in kernels/loss/PcaReg.cuh) and
// scale mode, as the CUDA exports.
void pca_owner<let K : int, let SA : bool>(uint3 wg, uint tid, PcaOwnerParams p) {
    uint r = (wg.y * p.wgs_per_row + wg.x) * kOwnerGroup + tid;
    // Every lane runs the search, whose votes take the whole subgroup; a lane
    // past n repeats the last rank and writes nothing.
    bool valid = r < p.n;
    float on = valid ? 1.0 : 0.0;
    float3 l = pca_reg_owner<K, SA>(valid ? r : p.n - 1, p.mode, p.n, p.k, p.window, p.means,
                                p.quats, p.scales, p.perm, p.sorted, p.nodes, p.meta, p.ranges,
                                p.nbrs, valid ? p.store_list : 0u, p.radii, p.gate_means,
                                p.v_means, p.v_quats,
                                p.v_scales, on * p.w_plane, on * p.w_tangent, on * p.w_normal,
                                on * p.w_surfel);
    if (!valid) l = float3(0.0);
    g_loss[tid] = l;
    GroupMemoryBarrierWithGroupSync();
    for (uint s = kOwnerGroup / 2; s > 0; s >>= 1) {
        if (tid < s) g_loss[tid] += g_loss[tid + s];
        GroupMemoryBarrierWithGroupSync();
    }
    if (tid == 0) {
        float3 t = g_loss[0];
        atomic_add_f32(p.loss_sums, t.x);
        atomic_add_f32(p.loss_sums + 1, t.y);
        atomic_add_f32(p.loss_sums + 2, t.z);
    }
}

#define DEF_PCA_OWNER(NAME, K, SA)                                                 \
    [shader("compute")]                                                             \
    [numthreads(128, 1, 1)]                                                         \
    void NAME(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,                     \
              uniform PcaOwnerParams* pp) {                                         \
        pca_owner<K, SA>(wg, tid, *pp);                                             \
    }
DEF_PCA_OWNER(pca_owner_k8, 8, false)
DEF_PCA_OWNER(pca_owner_k16, 16, false)
DEF_PCA_OWNER(pca_owner_k32, 32, false)
DEF_PCA_OWNER(pca_owner_k8_sa, 8, true)
DEF_PCA_OWNER(pca_owner_k16_sa, 16, true)
DEF_PCA_OWNER(pca_owner_k32_sa, 32, true)
