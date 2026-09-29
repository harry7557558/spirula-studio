#pragma once
// The op surface every model calls. Knows nothing about SAM: this is a
// general-purpose inference layer, and a feature detector / matcher / depth
// model added later builds on it unchanged.
//
// There is no computation graph. An op runs when you call it, writing into a
// caller-provided output tensor -- usually one taken from an Arena inside an
// ArenaScope. That keeps a module's forward pass readable next to its PyTorch
// reference, and keeps peak VRAM equal to the deepest stage's live set.
//
// Every op is a thin launcher over one Slang entry point. Where two logical
// operations always appear together (bias + activation + residual after a
// matmul; the residual add before a post-norm LayerNorm) they are fused,
// because at these shapes the extra pass costs more than the arithmetic.

#include "nn/Tensor.h"
#include "nn/vk/Memory.h"

#include <cstdint>

namespace nn {

enum class Act : uint32_t {
    None = 0,
    Relu = 1,
    GeluErf = 2,   // nn.GELU() -- SAM 3's ViT/neck/decoder use this exact form
    GeluTanh = 3,
    Sigmoid = 4,
    Selu = 5,      // nn.SELU() -- ALIKED's gate, in every block and both heads
    LogSigmoid = 6,// F.logsigmoid -- LightGlue's matchability term
    Tanh = 7,      // torch.tanh -- Metric3D's ConvGRU candidate and hidden init
    Elu = 8,       // F.elu(alpha=1) -- its normal head's concentration channel
    Silu = 9,      // F.silu -- the SwiGLU FFN in DINOv2 giant2
    Exp = 10,      // torch.exp -- MoGe's `remap_output='exp'` on the point map
};

enum class AttnBias : uint32_t {
    None = 0,
    PerKey = 1,   // bias[nk], broadcast over queries and heads (token padding)
    Full = 2,     // bias[n_heads][nq][nk]                      (DETR box RPB)
    Causal = 3,   // key j visible to query i iff j <= i        (text encoder)
    Window = 4,   // bias[n_heads][nq][nk] + -100 where AttnOpts::labels differ
                  // between query and key (Swin's shifted-window mask)
};

// ================
// Matmul / Linear
// ================

struct LinearOpts {
    Tensor  bias;       // [N]; empty -> none
    Tensor  residual;   // [M, N]; empty -> none. Added AFTER bias, before act.
    Act     act = Act::None;
    float   alpha = 1.0f;       // scales the matmul before bias/residual
    int64_t x_row_stride = 0;   // 0 -> x.cols(); set when x is a wide-buffer slice
};

// out[M, N] = act(alpha * x[M, K] @ w[N, K]^T + bias + residual)
//
// `w` may be F16 (the normal case for checkpoint weights) or F32; `x` and `out`
// are F32. The weight layout is PyTorch's [out_features, in_features] verbatim.
void linear(const Tensor& out, const Tensor& x, const Tensor& w,
            const LinearOpts& opts = {});

// out[M, N] = alpha * a[M, K] @ b[N, K]^T. Same kernel, spelled for the places
// that multiply two activations (mask embedding x pixel embedding, the
// hypernetwork mask heads).
void matmul_nt(const Tensor& out, const Tensor& a, const Tensor& b, float alpha = 1.0f,
               Act act = Act::None);

// Tensor cores (VK_KHR_cooperative_matrix), used by the fp16-weight GEMM and
// by attention's Q @ K^T. On wherever the device supports them;
// $SS_NN_COOPMAT=0 turns them off for the process. This pair does the same
// at runtime, so a test can measure both paths without starting a second
// process -- the operands become fp16 there, so the two do not agree to fp32
// precision and a numerical difference is worth being able to bisect.
bool coop_matrix_enabled();
void set_coop_matrix_enabled(bool on);

// Which tiled fp32 kernel the big shapes use; OpGemm.cpp measures it per
// device. This forces it, so one process can check both -- they accumulate over
// K in the same order, so a disagreement is a tiling bug, not precision.
enum class GemmTile { Measured, Wide, Narrow };
void set_gemm_tile(GemmTile t);

// ================
// Normalization
// ================

// LayerNorm over the last dimension. `residual`, when present, is added to x
// first -- the post-norm pattern `x = LN(x + sublayer(x))`.
void layer_norm(const Tensor& out, const Tensor& x, const Tensor& w, const Tensor& b,
                float eps = 1e-5f, const Tensor& residual = {});

// GroupNorm over the channel (last) dimension of a channel-last map.
// Needs an arena for the [groups, 2] statistics.
void group_norm(vk::Arena& arena, const Tensor& out, const Tensor& x, const Tensor& w,
                const Tensor& b, int groups, float eps = 1e-5f, Act act = Act::None);

// ================
// Attention
// ================

struct AttnOpts {
    int   n_heads = 1;
    int   head_dim = 0;
    float scale = 0.0f;          // 0 -> 1/sqrt(head_dim)
    int   batch = 1;             // independent problems (ViT windows)
    AttnBias bias_mode = AttnBias::None;
    Tensor   bias;
    // Row strides in elements; 0 -> n_heads * head_dim. Non-default values let
    // q/k/v point straight into a fused [N, 3*E] projection with no copy.
    int64_t q_stride = 0, k_stride = 0, v_stride = 0, out_stride = 0;
    // For AttnBias::Full: element strides of the [n_heads][nq][nk] bias.
    int64_t bias_stride_h = 0, bias_stride_q = 0;
    // Scratch for flash-decoding. When a problem has too few (queries x heads)
    // to fill the device -- one head over a few thousand queries, which is what
    // memory attention is -- the key range is split across extra workgroups and
    // the partial softmaxes are merged in a second pass. Leaving this null just
    // runs the single-pass kernel.
    vk::Arena* arena = nullptr;
    // AttnBias::Window: [batch, nq] region ids (i32), nq == nk. Swin's shifted
    // windows mask a pair whose tokens came from different regions of the
    // un-rolled map; nothing else is masked.
    Tensor labels;
};

// out[batch, nq, n_heads*head_dim] = softmax(scale * q k^T + bias) v
//
// Fused: the score matrix is never written to memory. See attention.slang for
// the tiling and why it has to be this way.
void attention(const Tensor& out, const Tensor& q, const Tensor& k, const Tensor& v,
               int64_t nq, int64_t nk, const AttnOpts& opts);

// In-place 2-D axial RoPE on a [batch, n, n_heads*head_dim] tensor.
// `freqs` is [n, head_dim/2, 2] holding (cos, sin) per complex pair.
void rope(const Tensor& x, const Tensor& freqs, int n_heads, int head_dim,
          int64_t n, int batch = 1, int64_t row_stride = 0);

// ================
// Elementwise
// ================

void fill(const Tensor& t, float value);
void copy(const Tensor& dst, const Tensor& src);  // dtype conversion allowed

// out = act(alpha*a + beta*b), b broadcast per-column ([C]) or scalar as needed.
void add(const Tensor& out, const Tensor& a, const Tensor& b, float alpha = 1.0f,
         float beta = 1.0f, Act act = Act::None);
void mul(const Tensor& out, const Tensor& a, const Tensor& b, Act act = Act::None);
// out[r, c] = act(a[r, c] * b[r]): one scalar per row, e.g. a [H*W, 1] gate
// over a channel-last map.
void mul_rows(const Tensor& out, const Tensor& a, const Tensor& b, Act act = Act::None);

// out = act(x * pre_scale + pre_bias) * post_scale + post_bias
void unary(const Tensor& out, const Tensor& x, Act act = Act::None, float pre_scale = 1.0f,
           float pre_bias = 0.0f, float post_scale = 1.0f, float post_bias = 0.0f);

// ================
// Spatial
// ================

struct ConvOpts {
    int stride_y = 1, stride_x = 1;
    int pad_y = 0, pad_x = 0;
    Act act = Act::None;
    Tensor bias;
    // nn.Conv2d(padding_mode='replicate'), which every 3x3 in MoGe's head uses.
    // Only conv2d honours it; the depthwise and deformable kernels zero-pad.
    bool pad_replicate = false;
};

// out[Ho, Wo, Co] = conv2d(in[Hi, Wi, Ci], w[Co, Ci, kh, kw]).
// Runs as chunked im2col + GEMM; `arena` supplies the column workspace.
void conv2d(vk::Arena& arena, const Tensor& out, const Tensor& in, const Tensor& w,
            int kh, int kw, const ConvOpts& opts = {});

// Depthwise (groups == channels); w is [C, kh, kw].
void conv2d_depthwise(const Tensor& out, const Tensor& in, const Tensor& w, int kh,
                      int kw, const ConvOpts& opts = {});

// torchvision's deform_conv2d with mask = None, groups = 1.
//
// `offset` is [Ho, Wo, 2*kh*kw] -- channel-last, (dy, dx) per tap in kernel
// order, which is what a plain conv2d producing 2*kh*kw channels lands in with
// no permute. `max_offset > 0` clamps each component to +-max_offset before
// sampling (ALIKED clamps to max(H, W) / 4); pass 0 for no clamp.
//
// Runs as chunked deform-im2col + the same GEMM and the same weight layout as
// conv2d, so a deformable conv costs one address computation more per tap than
// a normal one.
void deform_conv2d(vk::Arena& arena, const Tensor& out, const Tensor& in,
                   const Tensor& offset, const Tensor& w, int kh, int kw,
                   float max_offset = 0.0f, const ConvOpts& opts = {});

// The same with torchvision's `mask` (DCNv2): `mask` is [Ho, Wo, kh*kw] and
// scales each tap's sample before the GEMM.
void modulated_deform_conv2d(vk::Arena& arena, const Tensor& out, const Tensor& in,
                             const Tensor& offset, const Tensor& mask, const Tensor& w,
                             int kh, int kw, const ConvOpts& opts = {});

// out[N, C*k*k] = the k x k patch of `in` centred on each of N integer
// (x, y) centres, in the column order conv2d's weight expects. Out-of-range
// taps read zero. `centers` is an I32 [N, 2] tensor.
void patch_gather(const Tensor& out, const Tensor& in, const Tensor& centers, int k);

// ConvTranspose2d(kernel=2, stride=2). `w_packed` is the checkpoint weight
// repacked to [Cout*4, Cin] at load time (see model/Weights.cpp); the four
// kernel taps become four output-channel groups, so the tuned GEMM does the
// work and a scatter reorders it.
void conv_transpose2x2(vk::Arena& arena, const Tensor& out, const Tensor& in,
                       const Tensor& w_packed, const Tensor& bias, Act act = Act::None);

// Non-overlapping patch extraction for a ViT stem:
// out[(H/p)*(W/p), p*p*C] with column order c*p*p + ky*p + kx.
void patchify(const Tensor& out, const Tensor& in, int patch);

// `align_corners` picks between torch's two mappings and is NOT cosmetic:
// false (the default here, and what mask upsampling depends on) maps
// src = (dst + 0.5) * scale - 0.5; true maps src = dst * (Hi-1)/(Ho-1).
void resize_bilinear(const Tensor& out, const Tensor& in, bool align_corners = false);

// F.interpolate(mode='bicubic', align_corners=False), torch's a = -0.75. Only
// that mapping: the cubic kernel is where DaD's score pyramid is summed, and
// align_corners=True has no caller here.
void resize_bicubic(const Tensor& out, const Tensor& in);

void upsample_nearest2x(const Tensor& out, const Tensor& in);
void maxpool2x2(const Tensor& out, const Tensor& in);

// F.interpolate(scale_factor=(sy, sx), mode='nearest'). Pass the SCALE the
// reference asked for, not out/in: torch sizes the output floor(in * s) and
// still maps with the original s, and at 7/2 the two disagree.
void resize_nearest(const Tensor& out, const Tensor& in, float scale_y, float scale_x);

// nn.AvgPool2d(kernel, stride, padding), ceil_mode = False, and torch's
// count_include_pad = True (the divisor is always kernel^2). `out` must be
// sized [(Hi + 2*pad - kernel)/stride + 1, ...]; stride 0 means "same as
// kernel".
void avgpool(const Tensor& out, const Tensor& in, int kernel, int stride = 0, int pad = 0);

// out[N, C] = bilinear sample of in[H, W, C] at `pos`, an f32 [N, 2] tensor of
// normalized (x, y) in [-1, 1]. Reads zero outside, i.e. torch's
// padding_mode='zeros'. See the shader for why align_corners matters.
void grid_sample_points(const Tensor& out, const Tensor& in, const Tensor& pos,
                        bool align_corners = true);

// F.normalize(x, p=2, dim=-1). `out` may alias `x`.
void l2_normalize_rows(const Tensor& out, const Tensor& x, float eps = 1e-12f);

// softmax over the last dimension. On a channel-last [H, W, C] map that is
// torch's softmax(dim=1) over channels. `out` may alias `x`.
void softmax_rows(const Tensor& out, const Tensor& x);

// Resize a single-channel logit map to [Ho, Wo], threshold, and write packed
// 0/255 bytes. `out` is a U8 tensor; the buffer must be 4-byte rounded (every
// allocation is).
void resize_binarize(const Tensor& out_u8, const Tensor& logits, int64_t Ho, int64_t Wo,
                     float threshold = 0.0f);

// ================
// Gather / shuffle
// ================

void gather_rows(const Tensor& out, const Tensor& table, const Tensor& ids);

// Copy `rows` x `cols` between buffers with independent row strides. The
// building block for concat, slice and token-sequence assembly.
void strided_copy(const Tensor& out, const Tensor& in, int64_t rows, int64_t cols,
                  int64_t in_stride, int64_t out_stride);

// `shift` is Swin's cyclic shift: the map is zero-padded to whole windows and
// rolled by -shift before partitioning, and by +shift on the way back.
// `accumulate` adds into `out` instead of overwriting it (the residual).
void window_partition(const Tensor& out, const Tensor& in, int H, int W, int C, int ws,
                      int shift = 0);
void window_unpartition(const Tensor& out, const Tensor& in, int H, int W, int C, int ws,
                        int shift = 0, bool accumulate = false);

// Swin's PatchMerging gather: [H, W, C] -> [ceil(H/2) * ceil(W/2), 4*C] in the
// order x[0::2,0::2], x[1::2,0::2], x[0::2,1::2], x[1::2,1::2]; odd edges pad 0.
void patch_merge(const Tensor& out, const Tensor& in, int H, int W, int C);

// [H, W, C] cut into a gh x gw grid of blocks, stacked as channels:
// out[h][w][k] = in[gy*H/gh + h][gx*W/gw + w][c], where k is
enum class BlockOrder {
    ChannelMajor,      // (c*gh + gy)*gw + gx: einops '(c gh gw)'
    ColumnsThenRows,   // (gx*gh + gy)*C + c: torch.split over columns, then rows
};
void blocks_to_channels(const Tensor& out, const Tensor& in, int H, int W, int C, int gh,
                        int gw, BlockOrder order = BlockOrder::ChannelMajor);

// The level table of a multi-scale deformable attention: `value` stacks the
// levels' [h, w] maps row-major, level 0 first.
struct MsDeformLevels {
    int n = 0;
    int h[4] = {}, w[4] = {};
};

// Deformable DETR's MultiScaleDeformableAttention, softmax included. offsets
// are [nq, heads, levels, points, 2], logits [nq, heads, levels * points], and
// `refs` normalized [nq, 2] points or [nq, 4] cx,cy,w,h boxes.
void ms_deform_attn(const Tensor& out, const Tensor& value, const Tensor& offsets,
                    const Tensor& attn, const Tensor& refs, const MsDeformLevels& levels,
                    int n_heads, int n_points);

// out[y][x][c] = in[y][x][c] + tile[y % th][x % tw][c]
void add_tiled(const Tensor& out, const Tensor& in, const Tensor& tile, int H, int W,
               int C, int th, int tw);

// torchvision roi_align(sampling_ratio=0). `boxes` is [n, 4] xyxy in
// feature-grid coordinates; `out` is [n, S, S, C].
void roi_align(const Tensor& out, const Tensor& feat, const Tensor& boxes, int H, int W,
               int C, int S);

}  // namespace nn
