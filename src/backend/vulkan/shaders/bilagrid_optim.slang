// Fused bilagrid TV optimizers, one 256-cell codec block per workgroup.
// Cross-block TV reads can see partially updated neighbours, as on CUDA.
// BgOptimParams exceeds the 128-byte push limit and goes through the params ring.
// Spec constants: 0 = kQuantBits (0/4/8), 1 = kValueQuant (fp32/q16).

#include "bilagrid_common.slang"
#include "optim_quant.slang"

[SpecializationConstant]
const int kQuantBits = 0;
[SpecializationConstant]
const int kValueQuant = 0;

struct BgOptimParams {
    float* grids;          // !kValueQuant
    uint* grids_q16;       //  kValueQuant
    float2* value_bounds;  //  kValueQuant
    float* g1_f;           // adam !quant   (adagrad: accum_f)
    float* g2_f;           // adam !quant   (adagrad: unused)
    uint* packed;          // quant
    float4* quant_bounds4; // adam quant [n_blocks]
    float2* quant_bounds2; // adagrad quant [n_blocks]
    float* image_grad;     // [C_batch, L, H, W, C]
    int* cam_indices;      // [C_batch] (has_cam_indices)
    int N_grids, C_batch, C, L, H, W;
    float lr;
    float tv_weight;
    int adam_step;
    int has_cam_indices;
    uint total_cells;
    uint wgs_per_row;
    float grad_scale;      // on the image gradient only
    uint _pad;
};

// Common front half: flat-index decode, sparse image gradient, inline TV
// gradient. Returns (v, x) — the combined gradient and the current value.
[ForceInline]
void _bo_grad_and_value(BgOptimParams p, uint idx, bool inside, out float v,
                        out float x) {
    int cam = 0, ci = 0, l = 0, h = 0, w = 0;
    if (inside) {
        uint t = idx;
        ci = int(t % uint(p.C)); t /= uint(p.C);
        w = int(t % uint(p.W)); t /= uint(p.W);
        h = int(t % uint(p.H)); t /= uint(p.H);
        l = int(t % uint(p.L)); t /= uint(p.L);
        cam = int(t);
    }

    float image_g = 0.0f;
    if (inside) {
        int per_slot = p.L * p.H * p.W * p.C;
        int cell_off = (((l * p.H + h) * p.W + w) * p.C) + ci;
        if (p.has_cam_indices == 0) {
            if (cam < p.C_batch)
                image_g = p.image_grad[cam * per_slot + cell_off];
        } else {
            for (int b = 0; b < p.C_batch; b++) {
                if (p.cam_indices[b] == cam)
                    image_g += p.image_grad[b * per_slot + cell_off];
            }
        }
    }

    BgReader br = {p.grids, p.grids_q16, p.value_bounds};

    float tv_g = 0.0f;
    if (inside && p.tv_weight > 0.0f) {
        float s = p.tv_weight / (6.0f * float(p.N_grids));
        float sx = s / float(p.L * p.H * (p.W - 1));
        float sy = s / float(p.L * (p.H - 1) * p.W);
        float sz = s / float((p.L - 1) * p.H * p.W);

        int cam_base = cam * p.L * p.H * p.W * p.C;
        int self_off = ((l * p.H + h) * p.W + w) * p.C + ci;
        float val = bg_read(kValueQuant != 0, br, cam_base + self_off);

        int sx_step = p.C;
        int sy_step = p.W * p.C;
        int sz_step = p.H * p.W * p.C;

        if (w > 0)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off - sx_step)) * sx;
        if (w < p.W - 1)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off + sx_step)) * sx;
        if (h > 0)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off - sy_step)) * sy;
        if (h < p.H - 1)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off + sy_step)) * sy;
        if (l > 0)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off - sz_step)) * sz;
        if (l < p.L - 1)
            tv_g += (val - bg_read(kValueQuant != 0, br,
                                   cam_base + self_off + sz_step)) * sz;
    }

    v = image_g * p.grad_scale + tv_g;
    if (!bg_finite(v)) v = 0.0f;
    x = inside ? bg_read(kValueQuant != 0, br, int(idx)) : 0.0f;
}

// Value writeback: fp32 direct store, or block-reduced 16-bit re-encode.
[ForceInline]
void _bo_value_writeback(BgOptimParams p, uint idx, uint tid, bool inside,
                         uint block_id, float x) {
    if (kValueQuant != 0) {
        float2 vmm = inside ? float2(x, x) : float2(1e30f, -1e30f);
        vmm = oq_block_reduce_mm2(vmm, tid);
        uint q = inside ? qtensor_quantize_v<16>(x, vmm) : 0u;
        qtensor_store<16>(p.grids_q16, idx, tid, inside, q);
        if (tid == 0 && inside) p.value_bounds[block_id] = vmm;
    } else {
        if (inside) p.grids[idx] = x;
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_tv_adam(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                      uniform BgOptimParams* pp) {
    BgOptimParams p = *pp;
    const float eps = 1e-15f;
    const float beta1 = 0.9f;
    const float beta2 = 0.999f;

    uint block_id = wg.y * p.wgs_per_row + wg.x;
    // Fold padding must keep every lane at barriers; clamp reads and gate writes.
    uint n_blocks = (p.total_cells + 255u) / 256u;
    bool blk_ok = block_id < n_blocks;
    uint blk_rd = min(block_id, n_blocks - 1u);
    uint idx = block_id * 256 + tid;
    bool inside = idx < p.total_cells;

    float v, x;
    _bo_grad_and_value(p, idx, inside, v, x);

    float step_f = float(p.adam_step);
    float inv_bc1 = 1.0f / (1.0f - pow(beta1, step_f));
    float inv_bc2 = 1.0f / (1.0f - pow(beta2, step_f));

    float g1, g2;
    if (kQuantBits != 0) {
        float4 mm = p.quant_bounds4[blk_rd];
        if (inside) {
            float2 g1g2 = kQuantBits == 4
                              ? qadam_decode_g1g2<4>(p.packed, idx, mm)
                              : qadam_decode_g1g2<8>(p.packed, idx, mm);
            g1 = g1g2.x;
            g2 = g1g2.y;
        } else {
            g1 = 0.0f;
            g2 = 0.0f;
        }
    } else {
        g1 = inside ? p.g1_f[idx] : 0.0f;
        g2 = inside ? p.g2_f[idx] : 0.0f;
    }

    g1 = beta1 * g1 + (1.0f - beta1) * v;
    g2 = beta2 * g2 + (1.0f - beta2) * v * v;

    x -= p.lr * inv_bc1 * g1 / (sqrt(g2 * inv_bc2) + eps);
    _bo_value_writeback(p, idx, tid, inside, block_id, x);

    if (kQuantBits != 0) {
        float2 us = qadam_g1g2_to_us(g1, g2);
        float4 mm = inside ? float4(us.x, us.x, us.y, us.y)
                           : float4(1e30f, -1e30f, 1e30f, -1e30f);
        mm = oq_block_reduce_mm4(mm, tid);
        if (kQuantBits == 4) {
            uint2 q = qadam_quantize_us<4>(us.x, us.y, mm);
            qadam_store<4>(p.packed, idx, tid, inside, q);
        } else {
            uint2 q = qadam_quantize_us<8>(us.x, us.y, mm);
            qadam_store<8>(p.packed, idx, tid, inside, q);
        }
        if (tid == 0 && blk_ok) p.quant_bounds4[block_id] = mm;
    } else {
        if (inside) {
            p.g1_f[idx] = g1;
            p.g2_f[idx] = g2;
        }
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void bilagrid_tv_adagrad(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                         uniform BgOptimParams* pp) {
    BgOptimParams p = *pp;
    const float eps = 1e-15f;

    uint block_id = wg.y * p.wgs_per_row + wg.x;
    // Grid-fold tail handling: see bilagrid_tv_adam.
    uint n_blocks = (p.total_cells + 255u) / 256u;
    bool blk_ok = block_id < n_blocks;
    uint blk_rd = min(block_id, n_blocks - 1u);
    uint idx = block_id * 256 + tid;
    bool inside = idx < p.total_cells;

    float v, x;
    _bo_grad_and_value(p, idx, inside, v, x);

    float accum;
    if (kQuantBits != 0) {
        float2 mm = p.quant_bounds2[blk_rd];
        if (inside) {
            accum = kQuantBits == 4 ? qtlog_decode_v<4>(p.packed, idx, mm)
                                    : qtlog_decode_v<8>(p.packed, idx, mm);
        } else {
            accum = 0.0f;
        }
    } else {
        accum = inside ? p.g1_f[idx] : 0.0f;
    }

    accum += v * v;
    x -= p.lr * v / (sqrt(accum) + eps);
    _bo_value_writeback(p, idx, tid, inside, block_id, x);

    if (kQuantBits != 0) {
        float log_v = inside ? qtlog_forward_v(accum) : 0.0f;
        float2 lm = inside ? float2(log_v, log_v) : float2(1e30f, -1e30f);
        lm = oq_block_reduce_mm2(lm, tid);
        if (kQuantBits == 4) {
            uint q = inside ? qtlog_quantize_log<4>(log_v, lm) : 0u;
            qtlog_store<4>(p.packed, idx, tid, inside, q);
        } else {
            uint q = inside ? qtlog_quantize_log<8>(log_v, lm) : 0u;
            qtlog_store<8>(p.packed, idx, tid, inside, q);
        }
        if (tid == 0 && blk_ok) p.quant_bounds2[block_id] = lm;
    } else {
        if (inside) p.g1_f[idx] = accum;
    }
}
