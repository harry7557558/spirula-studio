// The per-splat expression evaluator (shaders/splat_expr.h): one thread per
// splat runs the postfix program over pool buffers, decoding each through the
// codecs the optimizer kernels use. Launcher: kernels/SplatExpr.cpp.

#include "optim_quant.slang"
#include "sh_addr.slang"
#include "shaders/splat_expr.h"

struct ExprVar {  // 48 bytes; mirrors SplatExprVar (kernels/visualize/Visualizer.cuh)
    uint* data;
    float* bounds;
    uint codec, bits, layout, stride;
    uint offset, count, bounds_stride, rows;
};

struct SplatExprParams {  // 40 bytes: pushed directly
    float* out;            // [num_splats]
    uint* code;            // [code_len]
    ExprVar* vars;
    uint num_splats, wgs_per_row, code_len, _pad0;
};

static const float kExprNan = asfloat(0x7fc00000u);

[ForceInline]
gindex_t _xp_cell(ExprVar v, uint i, uint c) {
    if (v.layout == SS_EXPR_SH_PAIR) {
        ShAddrW a = sh_quant_addr_w(v.stride / 3u, 0);
        return sh_cell_at_w(sh_addr_base(a, i), int(c), a.pair_pitch);
    }
    return (gindex_t)v.stride * (gindex_t)i + (gindex_t)c;
}

[ForceInline]
float _xp_decode(ExprVar v, gindex_t cell) {
    if (v.codec == SS_EXPR_F32) return asfloat(elem_at(v.data, cell)[0]);
    if (v.codec == SS_EXPR_I32) return float(asint(elem_at(v.data, cell)[0]));
    if (v.codec == SS_EXPR_U32) return float(elem_at(v.data, cell)[0]);
    if (v.codec == SS_EXPR_U16)
        return float((elem_at(v.data, cell >> 1)[0] >> (16u * uint(cell & 1))) & 0xffffu);
    // The codecs index a cell within a few words; rebasing to the cell's group
    // of four keeps that index small, and every width starts such a group on a
    // word. The bound index is exact because both strides are multiples of 256.
    const uint bytes_per_cell = v.codec >= SS_EXPR_ADAM_M ? v.bits / 4u : v.bits / 8u;
    uint* w = elem_at(v.data, (cell >> 2) * (gindex_t)bytes_per_cell);
    const uint li = uint(cell & 3);
    const uint blk = uint(cell >> 8) / (v.bounds_stride >> 8);
    if (v.codec == SS_EXPR_QLIN) {
        float2 mm = ((float2*)v.bounds)[blk];
        return v.bits == 16 ? qtensor_decode_v<16>(w, li, mm) : qtensor_decode_v<8>(w, li, mm);
    }
    if (v.codec == SS_EXPR_GRADQ) {
        float2 mm = ((float2*)v.bounds)[blk];
        return v.bits == 16 ? gradq_decode1<16>(w, li, mm) : gradq_decode1<8>(w, li, mm);
    }
    float4 mm = ((float4*)v.bounds)[blk];
    float2 g = v.bits == 16 ? qadam_decode_g1g2<16>(w, li, mm)
             : v.bits == 8  ? qadam_decode_g1g2<8>(w, li, mm)
                            : qadam_decode_g1g2<4>(w, li, mm);
    return v.codec == SS_EXPR_ADAM_M ? g.x : g.y;
}

[ForceInline]
float _xp_load(ExprVar v, uint i) {
    if (i >= v.rows) return kExprNan;
    if (v.count == 1u) return _xp_decode(v, _xp_cell(v, i, v.offset));
    float s = 0.0f;
    for (uint c = 0; c < v.count; c++) {
        float x = _xp_decode(v, _xp_cell(v, i, v.offset + c));
        s += x * x;
    }
    return sqrt(s);
}

// std::pow: SPIR-V's Pow is undefined for a negative base, and x^2 is the
// first thing anyone writes.
[ForceInline]
float _xp_pow(float a, float b) {
    if (b == 0.0f) return 1.0f;
    float r = pow(abs(a), b);
    if (a < 0.0f) {
        if (floor(b) != b) return kExprNan;
        if (fmod(b, 2.0f) != 0.0f) r = -r;
    }
    return r;
}

[ForceInline]
float _xp_unary(uint op, float x) {
    switch (op) {
        case SS_EXPR_NEG:     return -x;
        case SS_EXPR_ABS:     return abs(x);
        case SS_EXPR_SQRT:    return sqrt(x);
        case SS_EXPR_LN:      return log(x);
        case SS_EXPR_LOG2:    return log2(x);
        case SS_EXPR_LOG10:   return log10(x);
        case SS_EXPR_EXP:     return exp(x);
        case SS_EXPR_SIN:     return sin(x);
        case SS_EXPR_COS:     return cos(x);
        case SS_EXPR_TANH:    return tanh(x);
        case SS_EXPR_SIGMOID: return 1.0f / (1.0f + exp(-x));
        case SS_EXPR_FLOOR:   return floor(x);
        case SS_EXPR_SIGN:    return x > 0.0f ? 1.0f : (x < 0.0f ? -1.0f : 0.0f);
        default:              return kExprNan;
    }
}

[ForceInline]
float _xp_binary(uint op, float a, float b) {
    switch (op) {
        case SS_EXPR_ADD: return a + b;
        case SS_EXPR_SUB: return a - b;
        case SS_EXPR_MUL: return a * b;
        case SS_EXPR_DIV: return a / b;
        case SS_EXPR_POW: return _xp_pow(a, b);
        case SS_EXPR_MIN: return min(a, b);
        case SS_EXPR_MAX: return max(a, b);
        case SS_EXPR_LT:  return a < b ? 1.0f : 0.0f;
        case SS_EXPR_LE:  return a <= b ? 1.0f : 0.0f;
        case SS_EXPR_GT:  return a > b ? 1.0f : 0.0f;
        case SS_EXPR_GE:  return a >= b ? 1.0f : 0.0f;
        case SS_EXPR_EQ:  return a == b ? 1.0f : 0.0f;
        case SS_EXPR_NE:  return a != b ? 1.0f : 0.0f;
        default:          return kExprNan;
    }
}

[shader("compute")]
[numthreads(256, 1, 1)]
void splat_expr_eval(uint3 wg: SV_GroupID, uint tid: SV_GroupThreadID,
                     uniform SplatExprParams p) {
    uint i = (wg.y * p.wgs_per_row + wg.x) * 256 + tid;
    if (i >= p.num_splats) return;
    float st[SS_EXPR_MAX_STACK] = {};
    int sp = 0;
    for (uint pc = 0; pc < p.code_len; pc++) {
        uint w = p.code[pc];
        uint op = w & ((1u << SS_EXPR_OP_BITS) - 1u);
        if (op == SS_EXPR_VAR) {
            st[sp++] = _xp_load(p.vars[w >> SS_EXPR_OP_BITS], i);
        } else if (op == SS_EXPR_CONST) {
            st[sp++] = asfloat(p.code[++pc]);
        } else if (op == SS_EXPR_WHERE) {
            sp -= 2;
            st[sp - 1] = st[sp - 1] != 0.0f ? st[sp] : st[sp + 1];
        } else if (op == SS_EXPR_CLAMP) {
            sp -= 2;
            st[sp - 1] = min(max(st[sp - 1], st[sp]), st[sp + 1]);
        } else if (op >= SS_EXPR_ADD) {
            sp--;
            st[sp - 1] = _xp_binary(op, st[sp - 1], st[sp]);
        } else {
            st[sp - 1] = _xp_unary(op, st[sp - 1]);
        }
    }
    p.out[i] = st[0];
}
