# GPU inference (`src/nn/`)

A general-purpose inference layer on Vulkan + Slang: its own Vulkan runtime
(`nn/vk/`), a device tensor, an op set, and host image I/O. **It knows nothing
about any model** — `src/sam/` is the first thing built on it, and if a feature
detector, a matcher or a depth model is added later it builds on this
unchanged. Do not put model-specific constants here.

Vulkan-only, like `src/sfm/`: built by default for `SS_BACKEND=vulkan`
(`SS_BUILD_SAM`) and absent from a CUDA build. It carries its own device;
converging the repository's three Vulkan contexts onto one is
`docs/notes/sfm-port-plan.md` phase 6.

Runtime knobs: `SS_NN_LOG=0..3`, `SS_VK_DEVICE`, `SS_PROFILE=1`,
`SS_VK_VALIDATION=1`, `SS_NN_DEBUG_SYNC=1`, `SS_NN_COOPMAT=0`.

## The op layer

An op runs when you call it, writing into a caller-provided output — usually an
`Arena` tensor inside an `ArenaScope`. There is no computation graph, and that
is deliberate: a module's forward pass reads as straight-line C++ next to its
PyTorch reference, and the arena's scoping is the whole memory strategy.

### Layout conventions

Shapes are **PyTorch order** (row-major, slowest dim first). ggml's reversed
`ne[]` is undone once, in the weight loader.

| | |
|---|---|
| feature maps | `[H, W, C]` — channel-last. LayerNorm2d becomes LayerNorm; a 1×1 conv becomes a Linear. |
| token sequences | `[N, n_heads * head_dim]` — a fused qkv projection lands exactly where the attention kernel indexes, with no permute. |
| weights | `[out_features, in_features]`, the checkpoint's bytes unchanged. A `[Cout, Cin, kh, kw]` conv kernel is the same memory as the `[Cout, Cin*kh*kw]` matrix, and `Tensor::asMatrix()` is how ops see it. |

## What is fused, and why

Fusion here is not micro-optimization: at these shapes the extra pass costs
more than the arithmetic.

- **GEMM epilogue** — bias, activation and a residual add. SAM 3's forward pass
  is ~200 Linears and most are followed by all three. The residual may alias
  the output (one thread per element), so `x = W·h + b + x` is in place.
- **LayerNorm + residual** — the post-norm blocks are always
  `x = LN(x + sublayer(x))`.
- **Attention** — flash-style, online softmax. Not optional: the ViT's global
  blocks are 5184 × 5184 × 16 heads, 1.7 GiB of scores.
- **Resize + threshold + byte pack** — mask export, so the host never sees a
  float mask.

## Kernel selection notes

- **GEMM** has four kernels, picked in `OpGemm.cpp`:

  | when | kernel | tile |
  |---|---|---|
  | cooperative matrix, fp16 weight, `M,N ≥ 128`, `N,K` ≡ 0 mod 16 | `gemm_nt_coop` | 128×128, tensor cores, fp16 operands / fp32 accumulate |
  | `M ≤ 4` | `gemm_nt_thin` | one workgroup per output element, reduce over K in registers |
  | `M ≥ 128`, `N ≥ 128`, fp16 weight | `gemm_nt_big` | 128×128, `BK`=16, 8×8 accumulators per thread |
  | otherwise | `gemm_nt` | 64×64, `BK`=16, 4×4 accumulators |

  Small `M` is everywhere in this model — MLP heads run a single token, the
  hypernetwork mask heads one each — and the big tile carries everything that
  matters for frame time. It keeps its weight tile in shared in the fp16 the
  checkpoint already holds, which is why it needs an fp16 weight and why an
  fp32 second operand (a matmul against another activation) falls back.

- **Attention** has a cooperative-matrix variant too (`flash_attn_coop`),
  selected whenever the device offers tensor cores and `head_dim` is a multiple
  of 16 — Hiera's 96 qualifies, its 72 does not. Only `Q @ K^T` moves: the
  second matmul rescales its accumulator by a per-query factor on every key
  tile, and the KHR extension does not say which element of a fragment a lane
  holds, so that rescale is not expressible. The score matrix has no such
  problem because the softmax already reads it by (row, column) out of shared
  memory. Everything below applies to both.

  The scalar kernel is one 64-query × 32-key tile whatever the head dim: Q and K are
  staged in 16-dim chunks, so shared memory no longer scales with `head_dim`
  and the per-thread work is a 4×2 register tile of scores plus
  `4 × ceil(head_dim/16)` output accumulators. Any head dim up to 256 works; one
  that does not divide 16 (Hiera runs 96 and 72) just leaves lanes idle in the
  last chunk, and the bounds tests fold away for the aligned case because
  `kHeadDim` is a specialization constant.

  When `AttnOpts::arena` is set and the problem produces too few workgroups to
  cover the device, the launcher splits the key range across extra workgroups
  and merges the partial softmaxes in a second pass — see below for why that is
  the lever this kernel responds to.

## What the kernels are actually bound by

Measured with `nn_ops_test --bench`, which runs both kernels at the shapes SAM 3
uses. The fp32 analysis below is from an RTX 3070 Laptop; the cooperative-matrix
numbers are from an RTX 5070 Laptop, where the fp32 kernels land at 7.2–8.4
TFLOP/s. Worth reading before tuning either of them, because the two are bound
by different things and the obvious lever only works on one.

An Ampere SM retires 128 fp32 FMAs and reads 128 bytes of shared per clock, so
a kernel gets **one shared byte per FMA** and no more. `gemm_nt_big` is exactly
where that predicts: 4×4 accumulators spend 2 bytes/FMA and ran at 3.5 TFLOP/s;
8×8 with an fp32 weight tile spends 1.0 and ran at 4.7; 8×8 with the weight
tile kept fp16 spends 0.75 and runs at 5.3–6.4. Each step tracked the ratio.

`flash_attn` does not behave that way. Cutting its shared traffic per FMA by
1.7× (a 64-key tile with 4×4 score registers and fp16 probabilities) measured
*identical* — 2.99 vs 2.93 TFLOP/s on the ViT window shape. It is bound by
latency and occupancy, and the lever that moves it is more workgroups: the same
kernel over 5184 queries runs at 3.2 TFLOP/s with 16 heads to spread across and
2.0 with one head (81 workgroups on 46 SMs).

That prediction held, and **flash-decoding** is now implemented on the strength
of it. One head over a few thousand queries is the memory attention's shape and
nobody else's, so the launcher splits the key range only when the workgroup
count falls below `kTargetBlocks`:

| shape | single-pass | split-K |
|---|---|---|
| 5184 q × 5184 k, 1 head, d256 | 1.64 TFLOP/s | 2.36 |
| 4096 q × 28 672 k, 1 head, d256 (SAM 2 memory cross) | 2.02 | 2.60 |

which is what a tail-efficiency model predicts: 64 workgroups over 46 SMs is
1.4 waves at ~72% occupancy, and splitting eight ways takes it to 11 waves at
96%. Raising the target past 512 measured flat, so the win is the tail and
nothing else — 2.6 TFLOP/s is this kernel's steady-state rate. Going further
means fp16 operands and cooperative matrix, which is outside the Vulkan 1.2 core
baseline.

The combine pass costs one extra kernel (~0.4% of frame time) and a partial
buffer of `n_splits × nq × dim` floats, taken from the caller's arena, which is
why `AttnOpts` carries an `arena` at all.

Register prefetching and an L2-friendlier grid order were both tried on the
GEMM and both measured flat; they are still in `gemm_nt_big` because they cost
nothing, but do not expect them to buy anything either.

### Cooperative matrix

`VK_KHR_cooperative_matrix` was what both of the above ran out of road against,
and it is now implemented in `gemm_coop.slang` and `attention_coop.slang`. What
the measurements said, in the order they were taken, because two of the four
steps went the wrong way:

| | fp32 | coop |
|---|---|---|
| the multiplies alone, operands already in shared | — | **45 TFLOP/s** |
| first working GEMM: both operands staged through shared | 8.3 | 7.7 |
| ... with the next K block prefetched into registers | 8.3 | 8.3 |
| ... reading W straight out of the weight matrix | 8.3 | **17.0** |
| a 128×256 tile (16 accumulator fragments per subgroup) | 8.3 | 9.5 |
| the same tile over 512 threads instead | 8.3 | 13.4 |

The lesson is in rows two and four. Tensor cores are ~5× the fp32 pipe here, so
a staging path that was free next to slow arithmetic is the entire cost next to
fast arithmetic — the first version did the multiplies 5× quicker and finished
no sooner. What removed it was noticing that the weight is *already* the operand
the hardware wants: `W[N, K]` read column-major with stride K is exactly the
`K × N` fragment, in exactly the fp16 the checkpoint holds, so W never touches
shared memory at all. Only `x` needs a round trip, and only because it is fp32.

Rows five and six are the usual answer to a memory-bound tile (make it bigger)
failing: 16 accumulator fragments is 128 registers a thread before anything else
is live, and it spills.

The attention kernel takes the same treatment for `Q @ K^T` only, and gains
less — 2.6 → 4.4 TFLOP/s on the ViT window shape, 3.1 → 3.3 on SAM 2's memory
cross-attention with split-K. The other matmul cannot move; the module comment
in `attention_coop.slang` says why.

End to end on that laptop: SAM 3 image segmentation 168 → 123 ms, SAM 2.1 Large
tracking 647 → 470 ms/frame, SAM 2.1 Tiny 299 → 246. On an RTX 3070 Laptop
(Ampere) the same kernels gain less — the GEMM 5.5–7.2 → 6.2–9.8 TFLOP/s and
attention 1.7–3.3 → 2.0–4.2 — and Tiny tracking goes 418 → 358 ms/frame. Two
generations, 1.17x and 1.22x on the same workload, so treat the Blackwell GEMM
row as the ceiling rather than the expectation.

Masks are effectively unchanged — 3 pixels in 13 M for a twelve-frame SAM 2.1
Tiny track, 38 in 17 M for a SAM 3 image, all on boundaries. Both kernels take
fp16 operands with an fp32 accumulator, which is the arithmetic PyTorch runs
these models in; `nn_ops_test` checks each path against a reference rounded the
same way, so the tolerances stay tight rather than being widened to absorb it.
- **Convolution** is chunked im2col + GEMM, not a bespoke kernel. A full im2col
  of the seg head's 288×288×256 3×3 conv would be 764 MiB, so the output
  positions are chunked to keep the column buffer near 32 MiB. Depthwise is
  direct; transposed 2×2/stride-2 is a GEMM plus a scatter (the four kernel taps
  are four output-channel groups). One well-tuned matmul therefore carries ~95%
  of the model's FLOPs.

## Portability rules for new kernels

These are load-bearing; see `src/nn/vk/README.md` for the device baseline.

1. Workgroup **X** must be a multiple of every plausible subgroup size. 2-D
   kernels use flat 64/128/256-wide X and decode `(x, y)` in-shader.
2. Shared memory budget is **16 KiB**, the Vulkan floor. This applies to the
   cooperative-matrix kernels too — they are 14.0 and 15.3 KiB — even though no
   device that advertises tensor cores has less than 64.
3. Never leave a params pointer field null if a shader may load through it —
   CPU Vulkan implementations speculate loads across branches. Use
   `vk::or_fallback()`.
4. No 8-bit or 64-bit integer types, and no `f16tof32`.
5. Grids cap at 65535 per dimension. Long flat ranges fold across `y`
   (`Stream::fold1D` + `fold_index`); a kernel that keeps out-of-range threads
   alive for a groupshared reduction must gate its block-indexed writes,
   because the last folded row is padding. GEMM's row tiles hit the same cap at
   4.2 M rows — a full-resolution 1x1 conv over a 2048x2048 map — and split
   into several dispatches on shifted pointers rather than folding.
6. Reductions use a deterministic groupshared tree, not wave intrinsics, so
   wave32 and wave64 devices agree bit for bit.

## Testing

`tests/nn_ops_test.cpp` checks every kernel against an independent scalar CPU
implementation. A new op lands with a case there — the model layer treats this
API as trustworthy, and that is only true because of that file.
