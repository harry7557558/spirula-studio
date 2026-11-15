# FlexGlue: architecture, shapes and constants

Reference: [cvg/FlexGlue](https://github.com/cvg/FlexGlue) at `c6cbc7a`, code MIT.
Weights: release `v0.1`. `flexglue.pth` holds `extractor.backbone.*`, a DINOv3
ConvNeXt-B under the DINOv3 License, and MIT heads. The weights are never
committed or bundled.

Everything below was read from the source at that commit, and the parts marked
**[checked]** were confirmed against the release files. Anything marked
**[guess]** is an inference that has not been verified.

## What COLMAP runs, which is the target

COLMAP's `src/colmap/feature/flexglue.cc` uses two ONNX graphs from the release:

| file | sha256 | role |
|---|---|---|
| `flexglue_descriptor.onnx` | `93fab8a4…d9914ff` | image `[1,3,512,512]` -> features `[1,256,64,64]`, once per image |
| `flexglue_matcher_early_stop.onnx` | `c86714cd…a12ea7` | fp32 sparse matcher with early-stop checks after layers 2, 4, 6 and 8 |
| `flexglue_matcher_early_stop_fp16.onnx` | `01286e69…a339e20` | same with fp16 transformer layers. COLMAP uses it only on CUDA compute capability >= 7.0; older GPUs and the CPU get the fp32 graph. |
| `flexglue.pth` | `d7371f74…dff790068` | the PyTorch checkpoint. 906 f32 tensors, 141.76 M parameters. |

COLMAP's defaults: `filter_threshold 0.1`, `max_distance_px 3.0`,
`min_covisibility 0.5`, `early_stop_decay 0.5`. COLMAP resizes the uint8 RGB
image to 512x512 itself, with half-pixel bilinear sampling, no antialias and a
`/255` scale, the same as `F.interpolate(bilinear, align_corners=False)`. It
scales keypoints into the 512 frame (`x * 512 / width`) and passes
`image_size0 = image_size1 = (512, 512)`. The matcher therefore sees
everything in descriptor-input pixels. Keypoints use the corner origin, so the
centre of the top-left pixel is at (0.5, 0.5). COLMAP feeds the keypoints of
its own extractor (SIFT here) and ignores their descriptors.

**[checked]** Every float initializer in the two fp32 graphs is a checkpoint
tensor byte for byte, possibly transposed, apart from three groups:
- `descriptor.mean` and `descriptor.std` are the ImageNet constants below.
- `self_attn.Wqkv` weight and bias in every layer are the checkpoint rows
  permuted from `(H, D, 3)` to `(3, H, D)` (`use_blocked_qkv`).
- 233 checkpoint tensors are absent from the graphs: `extractor.descriptor_head`
  (the sparse mode never uses descriptors), heads 0-8 apart from the
  covisibility linears of heads 2, 4, 6 and 8, and the covisibility linears of
  heads 1, 3, 5 and 7.

So a port that reads the ONNX initializers, as `src/aliked/` does, runs the same
bytes as COLMAP.

Weights not in the graphs never run in COLMAP's configuration. That covers
`n_layers < 9`, the per-layer correspondence heads, the dense and sampled modes,
the RoMa refiner and the cycle check.

## Module tree

```
FlexGlue
├─ descriptor: DINOv3Descriptor                           extractor.*
│  ├─ mean/std (ImageNet)                                  buffers
│  ├─ backbone: ConvNeXt-B (DINOv3)                        extractor.backbone.*
│  │  ├─ downsample_layers[0] = Conv2d(3,128,k4,s4) -> LayerNorm2d(128, eps 1e-6)
│  │  ├─ downsample_layers[i=1..3] = LayerNorm2d(d_{i-1}, 1e-6) -> Conv2d(d_{i-1}, d_i, k2, s2)
│  │  └─ stages[i]: depths (3, 3, 27, 3), dims (128, 256, 512, 1024)
│  │     └─ ConvNeXtBlock(d):  dwconv 7x7 pad 3 (bias) -> LayerNorm(d, 1e-6)
│  │                            -> pwconv1 Linear(d,4d) -> GELU(erf) -> pwconv2 Linear(4d,d)
│  │                            -> * gamma[d] -> + residual        (no drop-path in eval)
│  ├─ head: LinearConvHead                                 extractor.head.*
│  │  ├─ stem = LayerNorm2d(128, 1e-5) -> Conv2d(128,128,3,pad 1) -> LayerNorm2d(128, 1e-5) -> GELU
│  │  ├─ fine_proj = Conv2d(4*128=512, 512, 1)      (on pixel_unshuffle(stem, 2))
│  │  ├─ norms[0..2] = LayerNorm2d(256 / 512 / 1024, 1e-5)
│  │  ├─ projs[0..2] = Conv2d(256 / 512 / 1024, 512, 1, bias=False)
│  │  ├─ mlp = LayerNorm2d(512) -> Conv1x1(512,512) -> GELU -> Conv1x1(512,512)
│  │  ├─ down = Conv2d(512, 256, 1)
│  │  └─ out_norm = LayerNorm2d(256, 1e-5)
│  └─ descriptor_head Linear(256, 64)                     unused in sparse mode
└─ matcher                                                matcher.*
   ├─ input_proj Linear(256, 512)
   ├─ posenc.Wr Linear(2, 32, bias=False)                  (LearnableFourierPositionalEncoding)
   ├─ transformers[0..8]: TransformerLayer(512, 8 heads, head_dim 64)
   │  ├─ self_attn: Wqkv Linear(512,1536), q_norm/k_norm LayerNorm(64), out_proj Linear(512,512),
   │  │             message_norm LayerNorm(512), ffn
   │  └─ cross_attn: to_qk, to_v, to_out Linear(512,512), qk_norm LayerNorm(64),
   │                 message_norm LayerNorm(512), ffn
   │     ffn = Linear(1024,1024) -> LayerNorm(1024) -> GELU(erf) -> Linear(1024,512)
   └─ heads[0..9]: MatchHead(512, search_dim 64); heads[k] reads the tokens after k layers
      ├─ temperature (scalar parameter; head 9: 10.296425819396973)
      ├─ to_search Linear(512, 64)
      ├─ matchability Linear(512, 1)          (heads 1..9)
      ├─ covisibility Linear(512, 1)          (heads 1..9; checkpoint name `disambiguate`)
      └─ offset_head: OffsetHead(512, hidden 256, bins 16, max_offset 1.0)
         ├─ query_encoder = Linear(512,256) -> LayerNorm(256) -> ReLU -> Linear(256,256)
         ├─ match_encoder = same shape
         └─ head = Linear(512,512) -> LayerNorm(512) -> ReLU -> Linear(512, 36)
```

All `LayerNorm` epsilons are 1e-5 except the backbone's, which are 1e-6
**[checked against the ONNX `LayerNormalization` nodes]**. Every GELU is the exact
erf form.

## Descriptor, stage by stage (512x512 input)

Shapes are written PyTorch-style `[C, H, W]`. The batch of 1 is dropped.

| stage | op | shape |
|---|---|---|
| input | bilinear squash of the uint8 image / 255, `align_corners=False`, no antialias | `[3, 512, 512]` |
| norm | `(x - mean) / std`, mean `(0.485, 0.456, 0.406)`, std `(0.229, 0.224, 0.225)` | `[3, 512, 512]` |
| stem | conv 4x4 s4 + LN 1e-6 | `[128, 128, 128]` |
| stage 0 (3 blocks) | -> **f4** | `[128, 128, 128]` |
| down 1 + stage 1 (3) | -> **f8** | `[256, 64, 64]` |
| down 2 + stage 2 (27) | -> **f16** | `[512, 32, 32]` |
| down 3 + stage 3 (3) | -> **f32** | `[1024, 16, 16]` |
| head.stem | LN, conv3x3, LN, GELU on f4 | `[128, 128, 128]` |
| pixel_unshuffle(2) | out channel `c*4 + dy*2 + dx` | `[512, 64, 64]` |
| fine_proj | 1x1 | `[512, 64, 64]` (= x) |
| coarse i | `projs[i](norms[i](f))`, then bilinear `align_corners=False` to 64x64 if smaller (f16, f32) | `[512, 64, 64]` each, summed into x |
| mlp residual | `x = x + mlp(x)` | `[512, 64, 64]` |
| down + out_norm | -> **features** | `[256, 64, 64]` |

`pixel_unshuffle` followed by the 1x1 `fine_proj` is exactly a 2x2 stride-2
convolution whose `[512, 128, 2, 2]` kernel is `fine_proj.weight`'s memory read
in place, because the unshuffled channel index `c*4 + dy*2 + dx` is the
`[Ci, kh, kw]` row-major index. **[guess, follows from the index arithmetic, not
run]**

The 1x1 projection runs before the upsample, so the bilinear resize acts on 512
channels instead of 1024. That is linear, so the order is free, but the
reference does it this way and parity is cheapest if the port matches.

## Matcher, stage by stage (COLMAP's sparse mode, n_layers 9)

`n = 64*64 = 4096` tokens per image, `D = 512`, `H = 8`, `hd = 64`. `M` and `N`
are the keypoint counts of images 0 and 1. Tokens are row-major over the
feature map, `t = y*64 + x`.

### Tokens and positional encoding
- `x_i = input_proj(features_i.flatten(2).T)`: `[4096, 512]`.
- Positions: `cell_centers(64, 64, scale=(max(h,w), max(h,w)))`, so
  `pos_t = ((x + 0.5)/64 * 2 - 1, (y + 0.5)/64 * 2 - 1)` in (x, y) order,
  `[4096, 2]`. The scale is the longer side on both axes, so a non-square map
  keeps its aspect.
- `proj = pos @ Wr^T`: `[4096, 32]`. The encoding is `cos(proj)` and `sin(proj)`,
  each `repeat_interleave(2)` to 64.
- The RoPE rotates adjacent pairs: `rotate_half` pairs elements `(2i, 2i+1)`, so
  `out[2i] = q[2i] cos_i - q[2i+1] sin_i` and `out[2i+1] = q[2i+1] cos_i + q[2i] sin_i`.
  That is `nn::rope`'s `[n, head_dim/2, 2]` (cos, sin) table, as in the LightGlue
  port. One table serves every head. The same table goes to both images because
  both maps are 64x64.

### TransformerLayer (x9): self(x0), self(x1), then cross(x0, x1)
Self block, per image:
1. `qkv = Wqkv x`: `[4096, 1536]`. The checkpoint orders the rows `(H, D, 3)`,
   with q, k and v interleaved per element. The ONNX graph orders them
   `(3, H, D)`, which gives contiguous q | k | v blocks.
2. `q`, `k` -> **LayerNorm over head_dim (64)**, per head, with `q_norm` and
   `k_norm` weights and eps 1e-5, **before** the RoPE. v is not normed.
3. RoPE on q and k. Then SDPA, scale `1/sqrt(64)`, no mask, 4096 x 4096 per head.
4. `message = out_proj(attn)`, then
   `x = x + ffn(cat[x, message_norm(message)])` (`[4096, 1024]` -> `[4096, 512]`).

Cross block:
1. `qk_i = to_qk(x_i)` and `v_i = to_v(x_i)` for both images. Then
   `qk_i = qk_norm(qk_i)`, a LayerNorm over 64 per head with ONE weight set
   shared by both images. No RoPE.
2. `m0 = to_out(attn(qk0, qk1, v1))` and `m1 = to_out(attn(qk1, qk0, v0))`. Both
   are computed from the pre-update x.
3. `x_i = x_i + ffn(cat[x_i, message_norm(m_i)])`. The ffn and message_norm
   weights are shared between the two images.

The covisibility score after layer k (1..9) is computed in fp32:
`score_k = mean_over_8192_tokens(sigmoid(heads[k].covisibility(cat[x0; x1])))`.
It is a scalar and the mean is unweighted over both images.

Early stop (COLMAP's graph, k in {2, 4, 6, 8}): stop if
`score_k < min_covisibility * (1 - early_stop_decay)^(9 - k)`. With COLMAP's
defaults that is k=2: 0.00390625, k=4: 0.015625, k=6: 0.0625, k=8: 0.25. A stop
returns `matches0 = -1`, `mscores0 = 0` and `covisibility_score = score_k`, and
skips the later layers and the readout. Without a stop, `covisibility_score =
score_9`, and if `score_9 < min_covisibility` every match is invalidated
(`valid &= score >= min_covisibility`).

### Readout with heads[9] (both directions, i -> j for (0,1) and (1,0))
`cf_i` is `x_i` after layer 9, viewed as a `[512, 64, 64]` map, in fp32.

| step | op | shape |
|---|---|---|
| points | `kpts_n = kpts / image_size * 2 - 1` (COLMAP's frame: `kpts_512 / 512 * 2 - 1`) | `[M, 2]` |
| search | `l2norm(to_search(cf_j tokens))` | `[4096, 64]` |
| f_query | `grid_sample(cf_i, kpts_n)`, bilinear, `padding_mode='border'`, `align_corners=False` | `[M, 512]` |
| query | `l2norm(to_search(f_query))` | `[M, 64]` |
| logits | `query @ search^T * temperature` | `[M, 4096]` |
| readout | 1.5-entmax window readout (below) | p2d `[M, 2]` normalized, mass `[M]` |
| f_match | `grid_sample(cf_j, p2d)`, same mode as f_query | `[M, 512]` |
| offset | `OffsetHead(f_query, f_match)` (below) | `[M, 2]` normalized |
| p2d | `p2d + offset` | `[M, 2]` |
| conf | `sigmoid(matchability(f_query)) * mass` | `[M]` |

**1.5-entmax window readout** (`ops.entmax15_window_readout`, radius 1, k 512).
Since 4096 > 512 it uses this path:
1. `topv, topi = topk(logits, 512)`, sorted descending. `vmax = topv[0]` and
   `idx = topi[0]`, where idx is the argmax cell.
2. `y = (logits - vmax) / 2`, and `tau = threshold(sorted (topv - vmax) / 2)`,
   computed as follows over rho = 1..512:
   `mean = cumsum(ys)/rho`, `mean_sq = cumsum(ys^2)/rho`,
   `tau_rho = mean - sqrt(max(0, (1 - rho*(mean_sq - mean^2)) / rho))`.
   `support = #{rho : tau_rho <= ys_rho}`, and `tau = tau_{support}`.
   tau is exact only when the entmax support has at most 512 cells. The
   reference makes the same approximation, so match it rather than fix it.
3. The 3x3 window around idx: `(cy + dy, cx + dx)` for `dy, dx in {-1, 0, 1}`.
   Indices are **clamped** into the map, so a border argmax repeats an edge
   cell. `inside` marks the unclamped cells.
4. `p = max(0, y[cell] - tau)^2` for the 9 window cells.
5. `position = sum(p * grid[cell]) / sum(p)`, summed over all 9, so a clamped
   duplicate counts twice. `mass = sum(p * inside)`, so a duplicate counts once.
   `grid[cell]` is the normalized cell centre `((x+0.5)/64*2-1, (y+0.5)/64*2-1)`.

**OffsetHead.**
- `h = head(cat[query_encoder(f_query), match_encoder(f_match)])` gives
  `[M, 36]`, viewed as `[M, 2, 18]`, and only the first 17 bins are kept.
- `delta = ((softmax_17(h) . arange(17)) / 16 - 0.5) * 2 * max_offset(1.0)`.
  That is in cells, in [-1, 1].
- `offset = (delta_x * 2 / w, delta_y * 2 / h)`, with w = h = 64.

**Sparse matching** (`sparse_matches`, `exact_nn=True` in the ONNX graph):
1. `p2d{i}_i{j} = (p2d + 1) / 2 * image_size_j` gives pixels. In COLMAP's frame
   `to_resized` is the identity.
2. Validity per keypoint:
   `valid_i = score >= min_cov & inside(kpts_i) & inside(p2d_i)`.
   `inside(p)` is `0 < p < size` on both axes, strict at border 0.
3. 4-D points centred on each image (`c = size/2`):
   `X_m = [p2d0_i1[m] - c1, kpts0[m] - c0]` and
   `Y_n = [kpts1[n] - c1, p2d1_i0[n] - c0]`.
   `sq[m, n] = sum over 4 dims (X_m - Y_n)^2`, formed from differences, not as
   one GEMM.
4. `m0 = argmin_n sq` and `m1 = argmin_m sq`; first index on ties. Mutual means
   `m1[m0[m]] == m`.
5. `dist0[m] = 0.5 * (|p2d0_i1[m] - kpts1[m0]| + |kpts0[m] - p2d1_i0[m0]|)`.
   This is the exact mean of two Euclidean norms, not `sqrt(sq)`.
6. `score0[m] = sqrt(conf0[m] * conf1[m0[m]])`.
7. `valid = mutual & score0 >= filter_threshold & valid0[m] & valid1[m0] &
   dist0 < max_distance_px`.
8. `matches0 = valid ? m0 : -1` and `mscores0 = valid ? score0 : 0`.

In PyTorch `max_distance_px = 0` or None disables the distance check. In the
ONNX graph inf disables it and 0 rejects everything.

### Precision
The PyTorch default is `mp="float16"`, an autocast over the transformer layers
only. The heads, covisibility and readout always run in fp32. The fp32 ONNX
graph corresponds to `mp=None`. COLMAP runs that graph on the CPU and below
compute capability 7.0, and it is the configuration this port implements, so
the **parity reference is `mp=None`**. The `fp16` mutant of `flexglue_ref.py`
runs the autocast for comparison; the fixtures do not carry it.

## Reuse map onto the existing models

**`src/roma/model/Descriptor.cpp`: NOT reusable for the backbone.** It is DINOv3
**ViT-L/16**: a patch embed, prefix tokens, axial RoPE and LayerScale attention
blocks. FlexGlue's backbone is DINOv3 **ConvNeXt-B**, a pure CNN with no
attention, tokens or RoPE. Only the general pieces carry over: `nn::layer_norm`
used channel-last, `nn::linear` with `Act::GeluErf`, `nn::mul` for a
gamma/LayerScale multiply, and the arena scoping pattern.

**`src/aliked/model/LightGlue.cpp`: the matcher layers are mostly reusable.** It
already has the fused `Wqkv`, an in-place adjacent-pair `nn::rope` inside the
fused buffer with q/k/v strides, the Fourier posenc table built on the host, the
self/cross split with shared `to_qk` and `to_v`, and the residual ffn of
`Linear(2D,2D) -> LN -> GELU -> Linear(2D,D)` on `cat[x, msg]`. It also already
reads COLMAP's ONNX with its permuted qkv rows. Changes needed:
- **q_norm / k_norm / qk_norm**: a LayerNorm over each 64-wide head slice, on q
  and k only, which in self-attention are slices of the `[N, 1536]` fused
  buffer. `nn::layer_norm` has no row stride. Two options: give it one, or note
  that in the blocked layout q is `[N*8, 64]` contiguous rows at offset 0 and k
  at offset 512 with row stride 1536. **New op or op change.**
- **message_norm**: `ffn(cat[x, LN(msg)])`. LightGlue has no LN on the message.
  It is one extra `layer_norm` before the concat.
- **dims**: D = 512 (LightGlue 256), hd 64, ffn width 1024. Token count is a
  dense 4096 per image instead of keypoints. Positions are cell centres, not
  keypoints, and there is no keypoint normalisation in posenc.
- **heads**: LightGlue's assignment head (`assign_logsumexp`/`assign_argmax` in
  `aliked.slang`) does not apply. The FlexGlue readout is new.
- Early stopping is new. LightGlue's early exit is not in the COLMAP LightGlue
  graph either.

**`src/nn/` ops.** Reusable as-is:
- `linear` (+ `GeluErf`/`Relu`/`Sigmoid` epilogues, `alpha` for the temperature scale)
- `matmul_nt` (logits `[M, 4096]`)
- `layer_norm` (contiguous rows: every channel-last LayerNorm2d)
- `conv2d` (4x4 s4 stem, 2x2 s2 downsamples, 3x3 head stem, and the
  fine_proj-as-2x2-s2 fold above)
- `conv2d_depthwise` (7x7 pad 3; zero padding is what torch does; bias via `ConvOpts`)
- `resize_bilinear(align_corners=false)` (16->64 and 32->64; the 512 input squash
  if done on the GPU)
- `attention` (4096 x 4096 x 8 heads, q/k/v strides)
- `rope`, `l2_normalize_rows`, `add`, `mul`, `copy`, `strided_copy` (concat)

Needs changes:
- `grid_sample_points` reads **zero** outside (`padding_mode='zeros'`).
  FlexGlue samples with `padding_mode='border'` and `align_corners=False`. That
  needs a border mode. Keypoints inside the image never leave the map at
  `align_corners=False`, but predicted points can.
- `layer_norm` needs a row stride, or per-head q/k norms (see above).
- `blocks_to_channels` is NOT `pixel_unshuffle`. It cuts contiguous blocks, not
  an interleaved 2x2, so use the conv fold above instead.

New, FlexGlue-specific (belongs in the model's own shader, as `aliked.slang` does):
- the entmax-1.5 window readout (`top-512` sort + cumsum threshold + 3x3 clamped
  window), one row per keypoint
- the covisibility mean over 8192 sigmoids, read back to the host for the
  early-stop branch
- OffsetHead's 17-of-18-bin softmax expectation
- mutual NN over a 4-D point distance `[M, N]` with tie-to-first argmin in both
  directions, plus the validity, score and distance gates

**[guess]** Fusing `to_search` + l2norm into the logits GEMM, and keeping the
`[M, 4096]` logits only per chunk (the reference chunks at 4096 queries), is
where the per-pair time will go. At M = 8192 the logits are 128 MiB per
direction in fp32.
