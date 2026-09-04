# LoMa (`src/loma/`)

The second learned SfM frontend: DaD keypoints, DeDoDe descriptors and the
LoMa matcher, on the inference layer (`src/nn/`), with no onnxruntime, no
PyTorch and no converter. Upstream is <https://github.com/davnords/LoMa>.

Status: **done and wired in.** All three networks match onnxruntime on the same
bytes (see "Parity"); `spirula sfm --features loma-b128 --matcher loma-b128`
runs the whole pipeline, and the GUI offers LoMa-B128 and LoMa-B in the same
two combos ALIKED is in.

## Rules

- **Vulkan only**, like `src/sfm/`, `src/sam/` and `src/aliked/`. Built with
  `SS_BUILD_SAM` (which is what builds `ss_nn`).
- **Model-specific things live here, never in `nn/`.** The one general op this
  needed — bicubic resize — went into `nn/` and is tested there. What is left
  in `shaders/loma.slang` is DaD's suppression rule and the matcher's
  dual-softmax assignment, both reductions over something that should not be
  materialized.
- **The checkpoints are the author's own ONNX exports**, fetched from the
  release COLMAP fetches them from and parsed in process. We host nothing and
  convert nothing.

## What LoMa is

Five matchers over one frozen detector and two frozen descriptors:

| variant | descriptor | embed x heads | matcher on disk |
|---|---|---|---|
| `loma-b128` | DeDoDe-B, 128-D (53 MB) | 256 x 4 | 48 MB |
| `loma-b` | DeDoDe-G, 256-D (1.29 GB) | 256 x 4 | 48 MB |
| `loma-r` | DeDoDe-G | 256 x 4 | 48 MB |
| `loma-l` | DeDoDe-G | 512 x 8 | 184 MB |
| `loma-g` | DeDoDe-G | 1024 x 16 | 726 MB |

`loma-r` is the rotation-robust one; `loma-b128` is the only variant that
avoids the 1.29 GB descriptor, and the only one whose whole model set is
~130 MB. The detector (`loma-dad`, 26 MB) is shared by all five.

## The one network, three times

DaD and both DeDoDes are the *same* architecture at different widths: a VGG
encoder tapped before every MaxPool, then a cascade of `ConvRefiner`s from the
coarsest tap to the finest, each adding into a running head and handing the
next level a context. `model/Pyramid.h` is that network, and the three
checkpoints differ only in numbers it reads off the file:

|  | DaD | DeDoDe-B | DeDoDe-G |
|---|---|---|---|
| encoder | VGG-11, 6 convs | VGG-19, 12 convs | VGG-19 **plus DINOv2 ViT-L/14** |
| levels | 8, 4, 2, 1 | 8, 4, 2, 1 | **14**, 8, 4, 2, 1 |
| hidden blocks | 3 | 5 | 5 |
| head | 1 logit channel | 128-D | 256-D |
| input | the image, any size | a fixed 784 square | a fixed 784 square |
| head upsample | **bicubic** | bilinear | bilinear |

Everything in that table is derived, not spelled: the taps are where the VGG
conv indices jump by 4 rather than 3 (a MaxPool sits there), the head width is
`out_conv - context` and every level has to agree on it, and the fixed square
is the export's `image` input shape. `model/Weights.cpp` does the deriving and
refuses a checkpoint whose numbers do not close.

## Five conventions that are not guessable

Each cost real time, and each looked like a working network rather than a
broken one.

1. **The detector's ImageNet normalization is INSIDE the graph.** `det.normalizer.mean`
   and `.std` are initializers feeding a Sub and a Div before the first conv,
   so the ONNX input is the raw `[0, 1]` image. Normalizing before handing it
   to onnxruntime — which is the obvious way to write the parity script —
   applies it twice and drops keypoint agreement from 100% to 52%.
2. **DINOv2's positional-embedding resample is not `out/in`.**
   `interpolate_pos_encoding` passes `scale_factor = (56 + 0.1) / 37`, and
   torch sizes the output from that scale but *also maps coordinates with it*.
   Using 56/37 instead moves every sample by 0.2% of a cell, which is a
   descriptor relative L2 of 8e-3 rather than 6e-4.
3. **The descriptor takes no normalization at all**, only `1/255` — unlike the
   detector, which is the same file's other half.
4. **Wqkv's rows are `[head][dim][3]`**, q/k/v interleaved per element, which no
   stride can express. `permute_qkv` repacks them to `[3][head][dim]` at load,
   which is the layout `nn::attention`'s strides already address. (LightGlue's
   export has the same layout, and `src/aliked/` says so too.)
5. **Keypoints are normalized PER AXIS**, `2x/W - 1`, not by the longer side.
   LightGlue's are aspect-preserving; copying that here rotates every position
   encoding on a non-square image.

## The matcher

LightGlue's block structure — nine layers of self- then cross-attention, one
assignment — with four differences, all in `model/Matcher.cpp`:

- an **input projection** only when the descriptor is narrower than the
  embedding (`loma-b128`, `loma-l`, `loma-g`; `loma-b` and `loma-r` have none);
- the assignment is a plain **dual softmax**, `softmax(sim, 2) * softmax(sim, 1)`,
  with no matchability term — LoMa supervises one in training and drops it at
  inference;
- so the score is `exp(2*sim - lse_row - lse_col)`, which separates into two
  vectors exactly as LightGlue's does, and `shaders/loma.slang` never
  materializes the N0 x N1 score matrix;
- the threshold applies to the **probability**, not to its log.

Everything else it needed — `linear`, `layer_norm`, `attention`, `rope` — was
already there, and our RoPE frequency layout already *is* LoMa's rotary
encoding, as it was LightGlue's.

## Layout

```
Loma.h                the public surface: Extractor, Matcher, and the
                        matcher -> descriptor mapping
Common.h              the names borrowed from nn/, as aliked/Common.h does
model/Fetch.{h,cpp}   URL + SHA-256 table, cache, download through curl
model/Weights.{h,cpp} initializers -> device tensors; the hparam derivation;
                        the f16 decision for DINOv2's matrices
model/Pyramid.h       the VGG encoder and the ConvRefiner cascade, shared by
                        the detector and both descriptors
model/Model.h         Detector, Descriptor and Dino behind the public glue
model/Detector.cpp    DaD: the cascade, the map-wide softmax, NMS, top-K and
                        the sub-pixel soft-argmax
model/Descriptor.cpp  DeDoDe: the cascade and one grid sample
model/Dino.cpp        DeDoDe-G's frozen DINOv2 ViT-L/14
model/Matcher.cpp     the nine layers and the assignment
model/Extract.cpp     the public Extractor, and the host image resample
model/Dump.{h,cpp}    SS_LOMA_DUMP=<dir>, one .npy per stage
shaders/loma.slang    the peak collector and the assignment reduction
tests/loma_test.cpp   checkpoint shapes, extraction, matching
```

On the SfM side, `sfm/feature/Extractor.h` and `sfm/feature/LearnedMatcher.h`
are the seams this plugs into; neither includes a `loma/` header, so
`src/sfm/` still builds without the inference layer and says so at run time.

## Sub-pixel refinement needed no kernel

DaD's is a softmax over the 3x3 patch of the *logits* at temperature 0.5, times
the ±1 pixel offsets. That is `patch_gather` (which reads zero outside, exactly
as the export's constant pad does), then `unary` for the temperature, then
`softmax_rows`, then a `linear` against a constant `[2, 9]` offset matrix. Four
general ops, no shader.

The one thing that *is* a shader is the peak collection, and only because DaD's
NMS is `probs * (probs == max_pool(probs, 3))` with no suppression rounds
(ALIKED's has two), so the mask never has to exist and the compaction shares
the pass that computes it.

## Parity

Against onnxruntime on the same checkpoint, one 1077x575 image, feeding ORT the
bytes we ran on (`--dump`, below) so the JPEG decoder and the 784 resample are
out of the numbers:

| | |
|---|---|
| keypoints matched ≤ 1 px | **100.0%** (2048 / 2048) |
| mean keypoint offset | (5e-6, 9e-6) px |
| detection score, max abs difference | 5e-10 |
| DeDoDe-B descriptor, cosine / relative L2 | 1.000000 / 5.9e-7 |
| DeDoDe-G descriptor, cosine / relative L2 | 1.000000 / 5.5e-4 |
| matcher, same partner on shared rows | 971 / 971 |
| matcher score, max abs difference | 2.1e-2 |

Two of those numbers deserve a sentence.

**DeDoDe-G's 5.5e-4 is the tensor cores, not a bug.** `nn::attention`'s
cooperative-matrix path takes fp16 operands with an fp32 accumulator (see
`src/nn/README.md`), and over 24 ViT-L blocks that reaches 8.5e-4 on the DINOv2
map. With `SS_NN_COOPMAT=0` the same comparison is **7.6e-6** on the map and
3.6e-6 on the descriptor.

**The matcher's 26 rows that only one side matches** are at the 0.1 threshold,
which nine layers of fp32 accumulation move across. `src/aliked/`'s LightGlue
reports the same shape of disagreement at the same magnitude.

Without `--dump` the same run reports 99.0% and a descriptor relative L2 of
2.3e-2 — that difference is entirely PIL versus `stb_image` decoding the JPEG
and PIL versus our own bicubic reaching 784, not arithmetic.

```bash
./build/loma_test                       # cached checkpoints, or SKIP
./build/loma_test --fetch               # download from the LoMa release
./build/loma_test --image IMG.jpg --out /tmp/a.bin
./build/loma_test --matcher loma-b --image IMG.jpg     # the DeDoDe-G path
./build/loma_test --match /tmp/a.bin /tmp/b.bin --out /tmp/m.bin

SS_LOMA_F32_WEIGHTS=1 SS_LOMA_DUMP=/tmp/d ./build/loma_test --image A.jpg --out /tmp/a.bin
python3 tools/loma/compare_ort.py --image A.jpg --ours /tmp/a.bin --dump /tmp/d
```

`SS_LOMA_F32_WEIGHTS=1` is not a quality knob: it holds DINOv2's matrices in
f32 so the comparison is tight enough that a real bug cannot hide in rounding.

## Cost

On an RTX 5070 Laptop, over a 152-image capture at `--quality medium`
(1200 px longest edge, 2048 keypoints):

| per image / per pair | `loma-b128` | `loma-b` (DeDoDe-G) | ALIKED + LightGlue |
|---|---|---|---|
| extraction | 330 ms | 780 ms | 42 ms |
| matching, 2048 x 2048 | 59 ms | 59 ms | 58 ms |
| weights on device | 125 MB | 725 MB (DINOv2 in f16) | 49 MB |

**The matcher is not the expensive half** — it costs what LightGlue costs, and
for the same reason: nine transformer layers over 2048 + 2048 tokens. What is
expensive is the extraction, because DaD runs a VGG-11 at full resolution and
DeDoDe a VGG-19 (and, for -G, a ViT-L) at 784² — eight to nineteen times
ALIKED. Like LightGlue, the matcher is for a SHORTLIST: `--pairs exhaustive` on
a thousand images is half a million pairs and many hours.

## Against the other two frontends

Two captures, `--quality medium`, everything else default, on an RTX 5070
Laptop. The first is a 152-image video-like sweep, the second an 83-image
internet photo collection (wide baselines, 76 different cameras):

| | SIFT | ALIKED + LightGlue | LoMa-B128 | LoMa-B |
|---|---|---|---|---|
| **152-image sweep** | | | | |
| registered | 152 | 152 | 152 | 152 |
| inliers | 1.10 M | 1.73 M | 1.86 M | 1.89 M |
| AUC@5 vs the reference | 99.45 | 98.67 | 99.07 | 99.10 |
| wall clock | 17 s | 140 s | 193 s | 257 s |
| **83-image collection** | | | | |
| registered | 76 | 79 | **81** | 79 |
| inliers | 125 k | 173 k | 378 k | **387 k** |
| AUC@5 vs the reference | 71.5 | 63.5 | 70.4 | **72.1** |
| wall clock | 14 s | 47 s | 116 s | 160 s |

Read the AUCs carefully. Both references are COLMAP SIFT reconstructions, so
SIFT is being scored against its own family; and the first capture is saturated
(every arm above 98.6), where nothing is distinguishable. The second is the one
that says anything, and what it says is that **LoMa-B is the only arm that
beats SIFT on SIFT's own reference** (72.1 against 71.5, where ALIKED +
LightGlue scores 63.5), and that **LoMa-B128 registers the most of the capture**
(81 of 83 images, against 79 and 76). Both find three times SIFT's inliers
there, for eight to eleven times its wall clock.

Two captures is not a benchmark. What is missing is the scored comparison on a
public set that `src/sfm/README.md` asks for.

## Memory

The arena is planned before anything runs and refuses to grow mid-pass, so the
plan has to be right rather than close: `Pyramid::planLevels` and
`Pyramid::planPyramid` sit next to `encode()` and `cascade()` for that reason,
and `loma_test` asserts the arena's high water lands inside the plan on every
extraction. It runs at 85-98%; the rest is the 96 MiB slack the convolution's
column workspace comes out of.

Two things set it, and which one wins depends on the working resolution.
**The descriptor is a floor** -- it always runs at a 784 square, and its head
is 128 or 256 channels there -- and **the detector grows with the image**,
because DaD sees it at full resolution. Measured on square frames, which is
the worst case at a given `--max-image-size`:

| longest edge | `loma-b128` | `loma-b` | bound by |
|---|---|---|---|
| 800 | 2.3 GB | 3.6 GB | the descriptor |
| 1200 | 2.3 GB | 3.6 GB | the descriptor |
| 1600 | 4.1 GB | 4.1 GB | the detector |
| 2000 | 5.8 GB | 5.8 GB | the detector |

Add the weights on top -- 125 MB for `loma-b128`, 754 MB for `loma-b` -- and
`--features loma-b` at 1600 px on a square capture is 4.8 GB, which is what a
dual fisheye or an equirectangular frame asks for and roughly a third more
than the 4:3 the 1600 default was picked against. If the reserve fails, the
error quotes what the next step down would cost; `--max-image-size` is the knob.

## Not done yet

1. **The f16 decision is DINOv2's only.** The VGG encoders, the refiners and
   every activation stay f32 -- which is what keeps the DeDoDe-B path at 6e-7,
   and also what makes the arena what it is. The four encoder taps alone are
   1.2 GB of the 4.1 GB at 1600 x 1600.
2. **The map-wide softmax runs in one workgroup.** `nn::softmax_rows` puts one
   workgroup on a row, and the detector's row is the whole image (2 M elements
   at 1600 px). It is a few ms next to a ~200 ms backbone, so it has not been
   worth a second kernel.
3. **The assignment tail is already small.** It was three passes over the
   N0 x N1 similarity with the column one uncoalesced; making it two coalesced
   passes moved a 2048 x 2048 pair from 61 to 59 ms, so what is left is the
   nine transformer layers and not the tail.
4. **`loma-l` and `loma-g` are untested end to end.** They are the same code
   path at a wider embedding — everything is read off the checkpoint — but no
   reconstruction has been run on them.
5. **The 1.29 GB DeDoDe-G checkpoint is read whole.** `nn::read_onnx` decodes
   every initializer to host f32 before uploading; the streaming pair
   (`read_onnx_structure` + `read_onnx_initializers`) exists and would cut the
   host peak, as `src/metric3d/` does for its giant2.
