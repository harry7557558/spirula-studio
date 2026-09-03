// Shared device helpers. Include-only: the leading underscore tells
// cmake/SsNn.cmake not to compile it as a module of its own.
//
// Rules this file exists to enforce:
//
//  * fp16 tensors are read through a `uint*` as packed half pairs, unpacked
//    with GLSL.std.450 UnpackHalf2x16 -- which needs no capability beyond
//    Shader, so fp16 weights cost no device feature. NOTE: Slang's `f16tof32`
//    intrinsic does NOT compile to this. It lowers to
//    `OpBitcast %half` + `OpFConvert`, dragging OpCapability Float16 and Int16
//    into the module and pushing the device requirement past Vulkan 1.2 core.
//    That is why these two helpers drop to inline SPIR-V; do not "simplify"
//    them back to the intrinsic without re-checking the capability list
//    (`spirv-dis <blob> | grep OpCapability`).
//  * Every buffer is addressed through a `uint*`; f32 elements come back via
//    asfloat(). One pointer type for both dtypes means one entry point per op
//    instead of two.
//  * No 8-bit or 64-bit integer types anywhere.

#ifndef NN_COMMON_INCLUDED
#define NN_COMMON_INCLUDED

static const float NN_PI = 3.14159265358979323846f;

// ---------------------------------------------------------------------------
// Typed loads / stores over a untyped uint buffer
// ---------------------------------------------------------------------------

// The two halves packed in one 32-bit word, low half first.
float2 unpack_half2(uint v) {
    return spirv_asm {
        result:$$float2 = OpExtInst glsl450 UnpackHalf2x16 $v
    };
}

uint pack_half2(float2 v) {
    return spirv_asm {
        result:$$uint = OpExtInst glsl450 PackHalf2x16 $v
    };
}

// Element `i` of a buffer holding f32 (is_f16 == false) or packed f16 pairs.
float load_elem(uint* base, uint i, bool is_f16) {
    if (is_f16) {
        float2 h = unpack_half2(base[i >> 1]);
        return ((i & 1u) == 0u) ? h.x : h.y;
    }
    return asfloat(base[i]);
}

// ---------------------------------------------------------------------------
// Addressing past 4 GiB
// ---------------------------------------------------------------------------

// `base[i]` wraps to the base once i * 4 reaches 4 GiB, and a 64-bit index does
// not change it -- only 64-bit pointer arithmetic does. NN_WIDE builds the
// `<stem>_wide` copy that does; README rule 7 has the rest.
#ifdef NN_WIDE

uint* wide_u(uint* base, uint i) {
    uint64_t off = uint64_t(i) * 4;
    return spirv_asm {
        %a : $$uint64_t = OpConvertPtrToU $base;
        %b : $$uint64_t = OpIAdd %a $off;
        result:$$uint* = OpConvertUToPtr %b
    };
}
float* wide_f(float* base, uint i) {
    uint64_t off = uint64_t(i) * 4;
    return spirv_asm {
        %a : $$uint64_t = OpConvertPtrToU $base;
        %b : $$uint64_t = OpIAdd %a $off;
        result:$$float* = OpConvertUToPtr %b
    };
}

float load_at(uint* base, uint i, bool is_f16) {
    if (is_f16) {
        float2 h = unpack_half2(wide_u(base, i >> 1)[0]);
        return ((i & 1u) == 0u) ? h.x : h.y;
    }
    return asfloat(wide_u(base, i)[0]);
}
uint load_u32_at(uint* base, uint i) { return wide_u(base, i)[0]; }
void store_at(float* base, uint i, float v) { wide_f(base, i)[0] = v; }
void store_u32_at(uint* base, uint i, uint v) { wide_u(base, i)[0] = v; }

#else

float load_at(uint* base, uint i, bool is_f16) { return load_elem(base, i, is_f16); }
uint load_u32_at(uint* base, uint i) { return base[i]; }
void store_at(float* base, uint i, float v) { base[i] = v; }
void store_u32_at(uint* base, uint i, uint v) { base[i] = v; }

#endif

// ---------------------------------------------------------------------------
// Activations. `kind` is always a specialization constant at the call site, so
// the switch folds away at pipeline creation.
// ---------------------------------------------------------------------------

static const int ACT_NONE      = 0;
static const int ACT_RELU      = 1;
static const int ACT_GELU_ERF  = 2;  // nn.GELU()          -- the PyTorch default
static const int ACT_GELU_TANH = 3;  // nn.GELU('tanh')
static const int ACT_SIGMOID   = 4;
static const int ACT_SELU      = 5;  // nn.SELU()          -- ALIKED's gate
static const int ACT_LOGSIGMOID = 6; // F.logsigmoid       -- LightGlue
static const int ACT_TANH      = 7;  // torch.tanh         -- Metric3D's ConvGRU
static const int ACT_ELU       = 8;  // F.elu(alpha=1)     -- its normal kappa
static const int ACT_SILU      = 9;  // F.silu             -- SwiGLU (ViT-giant2)
static const int ACT_EXP       = 10; // torch.exp          -- MoGe's z remap

// nn.SELU's constants, which are fixed by the paper (Klambauer et al. 2017)
// and are what torch hard-codes -- not tunable, and not the same as ELU's.
static const float SELU_ALPHA = 1.6732632423543772848170429916717f;
static const float SELU_SCALE = 1.0507009873554804934193349852946f;

// erf-exact GELU: 0.5x(1 + erf(x/sqrt2)).
//
// Abramowitz & Stegun 7.1.26 gives |eps| < 1.5e-7 on erf, i.e. below fp32
// rounding of the product -- matching torch's exact GELU closely enough that
// the divergence never shows up in a mask. The tanh approximation is NOT
// interchangeable here: SAM 3's ViT MLP and the neck use exact GELU, and the
// difference is visible in the logits.
float erf_approx(float x) {
    float s = (x < 0.0f) ? -1.0f : 1.0f;
    float a = abs(x);
    float t = 1.0f / (1.0f + 0.3275911f * a);
    float y = 1.0f - (((((1.061405429f * t - 1.453152027f) * t) + 1.421413741f) * t
                       - 0.284496736f) * t + 0.254829592f) * t * exp(-a * a);
    return s * y;
}

float gelu_erf(float x) { return 0.5f * x * (1.0f + erf_approx(x * 0.70710678118f)); }

float gelu_tanh(float x) {
    float inner = 0.7978845608028654f * (x + 0.044715f * x * x * x);
    return 0.5f * x * (1.0f + tanh(inner));
}

float apply_act(float x, int kind) {
    switch (kind) {
        case ACT_RELU:      return max(x, 0.0f);
        case ACT_GELU_ERF:  return gelu_erf(x);
        case ACT_GELU_TANH: return gelu_tanh(x);
        case ACT_SIGMOID:   return 1.0f / (1.0f + exp(-x));
        case ACT_SELU:      return SELU_SCALE * ((x > 0.0f) ? x : SELU_ALPHA * (exp(x) - 1.0f));
        // log(sigmoid(x)), in the form that does not overflow either tail:
        // min(x,0) - log(1 + exp(-|x|)).
        case ACT_LOGSIGMOID: return min(x, 0.0f) - log(1.0f + exp(-abs(x)));
        case ACT_TANH:      return tanh(x);
        case ACT_ELU:       return (x > 0.0f) ? x : (exp(x) - 1.0f);
        case ACT_SILU:      return x / (1.0f + exp(-x));
        case ACT_EXP:       return exp(x);
        default:            return x;
    }
}

// ---------------------------------------------------------------------------
// Grid folding
// ---------------------------------------------------------------------------

// Vulkan caps EVERY dispatch dimension at 65535, so a flat range longer than
// 65535 * block folds into a second grid row. The host computes `per_row`
// (Stream::fold1D) and the shader reconstructs the flat index here.
//
// Tail rule: the last folded row is padding -- whole workgroups past the end of
// the range. Kernels that keep out-of-range threads alive for a groupshared
// reduction must gate their block-indexed writes; kernels that early-out on the
// flat bound before any barrier need nothing.
uint fold_index(uint3 gid, uint3 tid, uint per_row, uint block) {
    return (gid.y * per_row + gid.x) * block + tid.x;
}

// ---------------------------------------------------------------------------
// Bilinear sampling of a channel-last [H, W, C] map
// ---------------------------------------------------------------------------

// Clamped bilinear fetch. `x`/`y` are continuous coordinates in source pixel
// space (pixel centers at integer + 0.5 handled by the caller).
float sample_bilinear(uint* src, bool src_f16, int H, int W, int C, int c, float y,
                      float x) {
    int x0 = int(floor(x));
    int y0 = int(floor(y));
    float fx = x - float(x0);
    float fy = y - float(y0);
    int x1 = min(x0 + 1, W - 1);
    int y1 = min(y0 + 1, H - 1);
    x0 = clamp(x0, 0, W - 1);
    y0 = clamp(y0, 0, H - 1);
    uint stride_y = uint(W) * uint(C);
    float v00 = load_at(src, uint(y0) * stride_y + uint(x0) * uint(C) + uint(c), src_f16);
    float v01 = load_at(src, uint(y0) * stride_y + uint(x1) * uint(C) + uint(c), src_f16);
    float v10 = load_at(src, uint(y1) * stride_y + uint(x0) * uint(C) + uint(c), src_f16);
    float v11 = load_at(src, uint(y1) * stride_y + uint(x1) * uint(C) + uint(c), src_f16);
    return (1.0f - fy) * ((1.0f - fx) * v00 + fx * v01) +
           fy * ((1.0f - fx) * v10 + fx * v11);
}

// Bilinear fetch that reads ZERO outside the map instead of clamping to the
// edge -- torchvision's rule for deformable convolution and torch's
// `grid_sample(padding_mode='zeros')`.
//
// The corner-by-corner guards matter and are not the same as an early
// bounds test on (y, x): at y = H - 0.5 the upper row is out of range but
// contributes weight 0.5, and torchvision takes that contribution as zero
// rather than repeating row H-1. Getting this wrong is invisible in the
// interior and shifts every response near the border.
float sample_bilinear_zero(uint* src, bool src_f16, int H, int W, int C, int c, float y,
                           float x) {
    if (y <= -1.0f || float(H) <= y || x <= -1.0f || float(W) <= x) return 0.0f;
    int y0 = int(floor(y));
    int x0 = int(floor(x));
    int y1 = y0 + 1;
    int x1 = x0 + 1;
    float ly = y - float(y0);
    float lx = x - float(x0);
    float hy = 1.0f - ly;
    float hx = 1.0f - lx;
    uint stride_y = uint(W) * uint(C);
    float v00 = 0.0f, v01 = 0.0f, v10 = 0.0f, v11 = 0.0f;
    if (y0 >= 0 && x0 >= 0)     v00 = load_at(src, uint(y0) * stride_y + uint(x0) * uint(C) + uint(c), src_f16);
    if (y0 >= 0 && x1 <= W - 1) v01 = load_at(src, uint(y0) * stride_y + uint(x1) * uint(C) + uint(c), src_f16);
    if (y1 <= H - 1 && x0 >= 0) v10 = load_at(src, uint(y1) * stride_y + uint(x0) * uint(C) + uint(c), src_f16);
    if (y1 <= H - 1 && x1 <= W - 1) v11 = load_at(src, uint(y1) * stride_y + uint(x1) * uint(C) + uint(c), src_f16);
    return hy * (hx * v00 + lx * v01) + ly * (hx * v10 + lx * v11);
}

#endif  // NN_COMMON_INCLUDED
