// Quantized weight formats, expanded to the packed f16 a GEMM reads. A model
// streams its weights in the small format and expands one matrix at a time
// into scratch, so the quantization costs PCIe and VRAM nothing at run time
// and the GEMMs stay the fp16 kernels every device already has.

#include "_common.slang"

// Spec ids 0..1.
[SpecializationConstant] const int kHasResidual = 0;
[SpecializationConstant] const int kHasScale    = 0;

static const int WG = 256;

// bitsandbytes' NF4 code book (functional.py, get_4bit_type("nf4")).
static const float NF4_LEVELS[16] = {
    -1.0f, -0.6961928009986877f, -0.5250730514526367f, -0.39491748809814453f,
    -0.28444138169288635f, -0.18477343022823334f, -0.09105003625154495f, 0.0f,
    0.07958029955625534f, 0.16093020141124725f, 0.24611230194568634f,
    0.33791524171829224f, 0.44070982933044434f, 0.5626170039176941f,
    0.7229568362236023f, 1.0f};

groupshared float sLevels[16];

struct DequantNf4Params {
    uint*  out;        // [N, K] packed f16
    uint*  codes;      // [N, K / 2] bytes, the even element in the high nibble
    uint*  absmax;     // [N * K / 64] bf16
    float* residual;   // [N, K] f32 (never null)
    float* row_scale;  // [N] f32 (never null)
    uint   words;      // N * K / 8: one thread per word of codes
    uint   K;
    uint   groups_per_row;
};

// Round-to-nearest-even to bf16 and back: what dequantize_4bit returns for a
// bf16 weight, which is the precision the model was fine-tuned against.
float bf16_round(float v) {
    uint b = asuint(v);
    b += 0x7fffu + ((b >> 16) & 1u);
    return asfloat(b & 0xffff0000u);
}

[shader("compute")]
[numthreads(WG, 1, 1)]
void dequant_nf4(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
                 uniform DequantNf4Params p) {
    if (tid.x < 16u) sLevels[tid.x] = NF4_LEVELS[tid.x];
    GroupMemoryBarrierWithGroupSync();

    const uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.words) return;
    const uint e = i * 8u;
    const uint blk = e >> 6;
    const uint aw = p.absmax[blk >> 1];
    const float am = asfloat((blk & 1u) != 0u ? (aw & 0xffff0000u) : (aw << 16));
    const float s = kHasScale != 0 ? p.row_scale[e / p.K] : 1.0f;
    const uint c = p.codes[i];

    [ForceUnroll] for (uint j = 0; j < 4u; ++j) {
        const uint byte = (c >> (8u * j)) & 0xffu;
        float a = bf16_round(sLevels[byte >> 4] * am);
        float b = bf16_round(sLevels[byte & 15u] * am);
        if (kHasResidual != 0) {
            a += p.residual[e + 2u * j];
            b += p.residual[e + 2u * j + 1u];
        }
        p.out[i * 4u + j] = pack_half2(float2(a * s, b * s));
    }
}
