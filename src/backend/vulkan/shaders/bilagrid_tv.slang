// Bilagrid TV loss + channel mean (fwd/bwd), q16 one-shot encode, affine
// identity init, and the small PPISP/bilagrid utility kernels (Vulkan
// backend). Mirrors kernels/bilagrid/BilagridTvLossFwd.cu, BilagridTvLossBwd.cu, the
// encode kernel in BilagridFusedAdam.cu, BilagridInit.cu and PpispInit.cu.
//
// Spec constants (IDs by declaration order — dispatches must pass ALL):
//   0: kValueQuant (tv/channel-mean forward + tv backward grid reads)

#include "bilagrid_common.slang"
#include "optim_quant.slang"

[SpecializationConstant]
const int kValueQuant = 0;

// ---------------------------------------------------------------------------
// tv_loss_forward — flat grid-stride, then a one-workgroup sum.
// ---------------------------------------------------------------------------

groupshared float _bt_red64[64];

// Undo tv_grid()'s fold: N*L/4 (ni, li) blocks (80000 for 40k images at L=8)
// exceed the 65535 dispatch cap, so they fold over x/y at `nl_per_row` per
// row; z packs the spatial blocks. Matches bilagrid_tv_block_ids() in CUDA.
struct BgTvBlocks { int nl, w, h; };

[ForceInline]
BgTvBlocks bg_tv_blocks(uint3 bid, int nl_per_row, int W) {
    BgTvBlocks b;
    b.nl = int(bid.y) * nl_per_row + int(bid.x);
    int wb = (W + 3) / 4;
    b.w = int(bid.z) % wb;
    b.h = int(bid.z) / wb;
    return b;
}

struct TvParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* out_buf;    // v_bilagrid
    int N, C, L, H, W;
    float v_tv_loss;
    int inplace;
    int nl_per_row;    // (ni, li) blocks per grid row -- see bg_tv_blocks
};

struct TvFwdParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* partials;   // one per workgroup
    int N, C, L, H, W;
    uint groups;
};

groupshared float _bt_red256[256];

[ForceInline]
float _bt_reduce256(uint tid, float v) {
    _bt_red256[tid] = v;
    GroupMemoryBarrierWithGroupSync();
    [ForceUnroll]
    for (uint s = 128; s > 0; s >>= 1) {
        if (tid < s) _bt_red256[tid] += _bt_red256[tid + s];
        GroupMemoryBarrierWithGroupSync();
    }
    return _bt_red256[0];
}

// Grid-stride over the flat grid so that neighbouring lanes read neighbouring
// floats; a partial per workgroup replaces the float atomic, which serializes
// on RDNA 4 when thousands target one address.
[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_tv_loss_fwd(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                          uniform TvFwdParams p) {
    BgReader r = {p.fp32, p.q16, p.vbounds};
    uint C = uint(p.C), W = uint(p.W), H = uint(p.H), L = uint(p.L);
    uint sh = W * C, sl = H * sh, sn = L * sl;
    uint total = uint(p.N) * sn;
    float kw = float(p.L * p.H * (p.W - 1));
    float kh = float(p.L * (p.H - 1) * p.W);
    float kl = float((p.L - 1) * p.H * p.W);

    // e's offsets within its row, layer and image advance by a fixed step;
    // updating them avoids five runtime divisions per element.
    uint e = wg.x * 256u + tid, step = p.groups * 256u;
    uint oh = e % sh, ol = e % sl, on = e % sn;
    uint dh = step % sh, dl = step % sl, dn = step % sn;
    float acc = 0.0f;
    for (; e < total; e += step) {
        float val = bg_read(kValueQuant != 0, r, int(e));
        if (oh >= C) {
            float d = val - bg_read(kValueQuant != 0, r, int(e - C));
            acc += d * d / kw;
        }
        if (ol >= sh) {
            float d = val - bg_read(kValueQuant != 0, r, int(e - sh));
            acc += d * d / kh;
        }
        if (on >= sl) {
            float d = val - bg_read(kValueQuant != 0, r, int(e - sl));
            acc += d * d / kl;
        }
        oh += dh; if (oh >= sh) oh -= sh;
        ol += dl; if (ol >= sl) ol -= sl;
        on += dn; if (on >= sn) on -= sn;
    }
    float sum = _bt_reduce256(tid, acc / float(p.C * p.N));
    if (tid == 0) p.partials[wg.x] = sum;
}

struct TvSumParams {
    float* partials;
    float* out;
    uint n;
};

// Fixed summation order, so the loss is the same on every run.
[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_tv_sum(uint tid: SV_GroupThreadID, uniform TvSumParams p) {
    float acc = 0.0f;
    for (uint i = tid; i < p.n; i += 256u) acc += p.partials[i];
    float sum = _bt_reduce256(tid, acc);
    if (tid == 0) p.out[0] += sum;
}

// ---------------------------------------------------------------------------
// tv_loss_backward — 4x4x4 blocks; (ni, li) folded over x/y, z packs (wi, hi).
// ---------------------------------------------------------------------------

[shader("compute")]
// Flat 64-thread workgroup (X must be a multiple of the pinned subgroup
// size); logical 4x4x4 decode below matches the CUDA block geometry.
[numthreads(64, 1, 1)]
void bilagrid_tv_loss_bwd(uint3 bid: SV_GroupID, uint ftid: SV_GroupThreadID,
                          uniform TvParams p) {
    uint3 t = uint3(ftid % 4u, (ftid / 4u) % 4u, ftid / 16u);
    BgTvBlocks b = bg_tv_blocks(bid, p.nl_per_row, p.W);
    int wi = b.w * 4 + int(t.x);
    int hi = b.h * 4 + int(t.y);
    int idx = b.nl * 4 + int(t.z);
    if (wi >= p.W || hi >= p.H || idx >= p.N * p.L) return;
    int li = idx % p.L; idx /= p.L;
    int ni = idx;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    float s = p.v_tv_loss / float(6 * p.N);
    float sx = s / float(p.L * p.H * (p.W - 1));
    float sy = s / float(p.L * (p.H - 1) * p.W);
    float sz = s / float((p.L - 1) * p.H * p.W);

    int C = p.C;
    int sw = C;
    int sh = p.W * C;
    int sl = p.H * p.W * C;
    int sn = p.L * p.H * p.W * C;
    int cell_base = ni * sn + li * sl + hi * sh + wi * sw;

    for (int ci = 0; ci < C; ci++) {
        int cell_idx = cell_base + ci;
        float half_grad = 0.0f;
        float val = bg_read(kValueQuant != 0, r, cell_idx);
        if (wi > 0)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx - sw)) * sx;
        if (wi < p.W - 1)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx + sw)) * sx;
        if (hi > 0)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx - sh)) * sy;
        if (hi < p.H - 1)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx + sh)) * sy;
        if (li > 0)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx - sl)) * sz;
        if (li < p.L - 1)
            half_grad += (val - bg_read(kValueQuant != 0, r, cell_idx + sl)) * sz;

        if (p.inplace != 0)
            p.out_buf[cell_idx] += half_grad;
        else
            p.out_buf[cell_idx] = half_grad;
    }
}

// ---------------------------------------------------------------------------
// channel_mean forward / backward — same 4x4x4 geometry as CUDA.
// ---------------------------------------------------------------------------

struct ChannelMeanParams {
    float* fp32;
    uint* q16;
    float2* vbounds;
    float* channel_mean;   // fwd: [C] out; bwd: v_channel_mean in
    float* v_bilagrid;     // bwd only
    int N, C, L, H, W;
    int inplace;
    int nl_per_row;    // (ni, li) blocks per grid row -- see bg_tv_blocks
    int _pad0;
};

[shader("compute")]
// Flat 64-thread workgroup (X must be a multiple of the pinned subgroup
// size); logical 4x4x4 decode below matches the CUDA block geometry.
[numthreads(64, 1, 1)]
void bilagrid_channel_mean_fwd(uint3 bid: SV_GroupID, uint ftid: SV_GroupThreadID,
                               uniform ChannelMeanParams p) {
    uint3 t = uint3(ftid % 4u, (ftid / 4u) % 4u, ftid / 16u);
    BgTvBlocks b = bg_tv_blocks(bid, p.nl_per_row, p.W);
    int wi = b.w * 4 + int(t.x);
    int hi = b.h * 4 + int(t.y);
    int idx = b.nl * 4 + int(t.z);
    bool inside = (wi < p.W && hi < p.H && idx < p.L * p.N);
    int li = idx % p.L; idx /= p.L;
    int ni = idx;
    BgReader r = {p.fp32, p.q16, p.vbounds};

    uint tid = (t.z * 4 + t.y) * 4 + t.x;
    int cell_base = ((((ni * p.L) + li) * p.H + hi) * p.W + wi) * p.C;

    for (int ci = 0; ci < p.C; ci++) {
        float val = 0.0f;
        if (inside) val = bg_read(kValueQuant != 0, r, cell_base + ci);
        GroupMemoryBarrierWithGroupSync();
        _bt_red64[tid] = val;
        GroupMemoryBarrierWithGroupSync();
        [ForceUnroll]
        for (uint s = 32; s > 0; s >>= 1) {
            if (tid < s) _bt_red64[tid] += _bt_red64[tid + s];
            GroupMemoryBarrierWithGroupSync();
        }
        if (tid == 0)
            atomic_add_f32(&p.channel_mean[ci],
                           _bt_red64[0] / float(p.N * p.L * p.H * p.W));
    }
}

[shader("compute")]
// Flat 64-thread workgroup (X must be a multiple of the pinned subgroup
// size); logical 4x4x4 decode below matches the CUDA block geometry.
[numthreads(64, 1, 1)]
void bilagrid_channel_mean_bwd(uint3 bid: SV_GroupID, uint ftid: SV_GroupThreadID,
                               uniform ChannelMeanParams p) {
    uint3 t = uint3(ftid % 4u, (ftid / 4u) % 4u, ftid / 16u);
    BgTvBlocks b = bg_tv_blocks(bid, p.nl_per_row, p.W);
    int wi = b.w * 4 + int(t.x);
    int hi = b.h * 4 + int(t.y);
    int idx = b.nl * 4 + int(t.z);
    if (wi >= p.W || hi >= p.H || idx >= p.N * p.L) return;
    int li = idx % p.L; idx /= p.L;
    int ni = idx;

    int cell_base = ((((ni * p.L) + li) * p.H + hi) * p.W + wi) * p.C;
    for (int ci = 0; ci < p.C; ci++) {
        float grad = p.channel_mean[ci] / float(p.N * p.L * p.H * p.W);
        int cell_idx = cell_base + ci;
        if (p.inplace != 0)
            p.v_bilagrid[cell_idx] += grad;
        else
            p.v_bilagrid[cell_idx] = grad;
    }
}

// ---------------------------------------------------------------------------
// bilagrid_encode_q16 — one 256-cell codec block per workgroup.
// ---------------------------------------------------------------------------

struct EncodeQ16Params {
    float* grids;
    uint* grids_q16;
    float2* value_bounds;
    uint total_cells;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_encode_q16(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform EncodeQ16Params p) {
    uint block_id = wg.y * p.wgs_per_row + wg.x;
    uint idx = block_id * 256 + tid;
    bool inside = idx < p.total_cells;
    float v = inside ? p.grids[idx] : 0.0f;
    float2 mm = inside ? float2(v, v) : float2(1e30f, -1e30f);
    mm = oq_block_reduce_mm2(mm, tid);
    uint q = inside ? qtensor_quantize_v<16>(v, mm) : 0u;
    qtensor_store<16>(p.grids_q16, idx, tid, inside, q);
    if (tid == 0 && inside) p.value_bounds[block_id] = mm;
}

// ---------------------------------------------------------------------------
// bilagrid_affine_identity_init — flat over N*L*H*W cells, 12 floats each.
// ---------------------------------------------------------------------------

struct AffineInitParams {
    float* grids;
    uint total;  // N*L*H*W cells
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_affine_identity_init(uint3 wg: SV_GroupID,
                                   uint tid: SV_GroupThreadID,
                                   uniform AffineInitParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    // Row-major 3x4 identity per cell.
    [ForceUnroll]
    for (uint ci = 0; ci < 12; ci++) {
        float v = (ci == 0 || ci == 5 || ci == 10) ? 1.0f : 0.0f;
        p.grids[idx * 12 + ci] = v;
    }
}

// ---------------------------------------------------------------------------
// PPISP "original" default init ([N, 36]) + elementwise grad add + scatter.
// ---------------------------------------------------------------------------

struct PpispInitParams {
    float* params;
    uint total;  // N*36
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ppisp_original_default_init(uint3 wg: SV_GroupID,
                                 uint tid: SV_GroupThreadID,
                                 uniform PpispInitParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    uint pid = idx % 36u;
    const float a = 0.013658988289535046f;
    const float b = 0.37816452980041504f;
    float v = 0.0f;
    if (pid >= 24u) {
        uint j = (pid - 24u) % 4u;
        v = (j == 0u || j == 1u) ? a : (j == 2u ? b : 0.0f);
    }
    p.params[idx] = v;
}

struct AddIntoGradParams {
    float* src;
    float* dst;
    uint total;
    uint wgs_per_row;
    float dst_scale;
    uint has_src;
};

[shader("compute")]
[numthreads(256, 1, 1)]
void ppisp_add_into_grad(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform AddIntoGradParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (idx >= p.total) return;
    p.dst[idx] = p.dst[idx] * p.dst_scale + (p.has_src != 0 ? p.src[idx] : 0.0f);
}

struct ScatterFloatsParams {
    float* src;
    int* indices;
    float* dst;
    uint total;
    uint wgs_per_row;
};

[shader("compute")]
[numthreads(128, 1, 1)]
void bilagrid_scatter_floats(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                             uniform ScatterFloatsParams p) {
    uint idx = (wg.y * p.wgs_per_row + wg.x) * 128 + tid;
    if (idx >= p.total) return;
    p.dst[p.indices[idx]] = p.src[idx];
}
