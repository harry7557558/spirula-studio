// Elementwise kernels: the binary/unary ops that are not already fused into a
// GEMM or a norm epilogue.
//
// Deliberately few and general. Two entry points cover every use in the model;
// adding one kernel per PyTorch operator would triple the module count for no
// gain, since these are all memory bound and identical in cost.

#include "_common.slang"

// Spec ids 0..3.
[SpecializationConstant] const int kOp        = 0;  // 0 add, 1 mul, 2 sub, 3 max
[SpecializationConstant] const int kBroadcast = 0;  // 0 elementwise, 1 per-column, 2 scalar, 3 per-row
[SpecializationConstant] const int kAct       = 0;
[SpecializationConstant] const int kInputF16  = 0;

static const int WG = 256;

struct BinaryParams {
    float* out;
    uint*  a;
    uint*  b;
    uint   n;          // total elements of `out`
    uint   cols;       // for kBroadcast == 1 and 3
    float  alpha;      // scales a
    float  beta;       // scales b
    uint   groups_per_row;
};

[shader("compute")]
[numthreads(WG, 1, 1)]
void binary(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
            uniform BinaryParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.n) return;
    float av = load_elem(p.a, i, kInputF16 != 0) * p.alpha;
    uint bi = (kBroadcast == 1) ? (i % p.cols)
            : (kBroadcast == 2) ? 0u
            : (kBroadcast == 3) ? (i / p.cols) : i;
    float bv = load_elem(p.b, bi, kInputF16 != 0) * p.beta;
    float r;
    switch (kOp) {
        case 1:  r = av * bv; break;
        case 2:  r = av - bv; break;
        case 3:  r = max(av, bv); break;
        default: r = av + bv; break;
    }
    p.out[i] = apply_act(r, kAct);
}

struct UnaryParams {
    float* out;
    uint*  x;
    uint   n;
    float  pre_scale;
    float  pre_bias;
    float  post_scale;
    float  post_bias;
    uint   groups_per_row;
};

// out = act(x * pre_scale + pre_bias) * post_scale + post_bias
//
// The two-sided affine is what the memory encoder's mask preprocessing needs
// (`sigmoid(logits) * 20 - 10`) and what the DETR box refinement's
// inverse-sigmoid clamp reduces to; folding them here saves a pass each.
[shader("compute")]
[numthreads(WG, 1, 1)]
void unary(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
           uniform UnaryParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (i >= p.n) return;
    float v = load_elem(p.x, i, kInputF16 != 0) * p.pre_scale + p.pre_bias;
    p.out[i] = apply_act(v, kAct) * p.post_scale + p.post_bias;
}

struct ConvertParams {
    uint*  out;   // f32 or packed f16, per kOp: 0 -> f32 out, 1 -> f16 out
    uint*  x;
    uint   n;
    uint   groups_per_row;
};

// f32 <-> f16 conversion. Writing packed halves means two threads share a word,
// so the pairs are assembled by the even-indexed thread reading both values --
// a plain byte/short store would need 16-bit storage, which is outside the
// device baseline.
[shader("compute")]
[numthreads(WG, 1, 1)]
void convert(uint3 gid : SV_GroupID, uint3 tid : SV_GroupThreadID,
             uniform ConvertParams p) {
    uint i = fold_index(gid, tid, p.groups_per_row, uint(WG));
    if (kOp == 1) {
        // f32 -> f16: thread i writes word i, packing elements 2i and 2i+1.
        uint n_words = (p.n + 1u) / 2u;
        if (i >= n_words) return;
        uint lo = i * 2u;
        float a = load_elem(p.x, lo, kInputF16 != 0);
        float b = (lo + 1u < p.n) ? load_elem(p.x, lo + 1u, kInputF16 != 0) : 0.0f;
        p.out[i] = pack_half2(float2(a, b));
    } else {
        if (i >= p.n) return;
        p.out[i] = asuint(load_elem(p.x, i, kInputF16 != 0));
    }
}
