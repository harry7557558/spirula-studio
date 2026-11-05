# LPIPS (`src/lpips/`)

LPIPS (Zhang et al. 2018) on the inference layer (`src/nn/`), computed the way
torchmetrics' `LearnedPerceptualImagePatchSimilarity` computes it: torchvision's
AlexNet or VGG16 `features`, the LPIPS v0.1 linear layers, fp32 throughout.
The eval pass (`TrainerSession::eval`) scores every held-out view with both
nets into `metrics.json` as `lpips_alex`, `lpips_vgg` and their `cc_` variants
-- the keys `reference/python/eval_lpips.py` writes, with the same values.

## Rules

- **Vulkan only**, like the other `nn/` models; built with `SS_BUILD_SAM`.
  `TrainerCore` is in the engine library and cannot link this, so it calls
  through `LpipsHooks` (`app/TrainerCore.h`), which `app/EvalLpips.cpp`
  installs from `Main.cpp`. A build without it logs that LPIPS is left out and
  `eval_lpips.py` is still the way to get it.
- **Model-specific code stays here.** The one kernel of its own is the tap
  distance (`shaders/lpips.slang`): channel-normalize both taps, weight the
  squared difference by the linear layer, sum per workgroup. What it needed that
  is general went into `nn/`: `maxpool(kernel, stride)` for AlexNet's 3x3/2
  pools, and the legacy (pre-1.6, non-ZIP) `torch.save` format in
  `nn/io/TorchCheckpoint.cpp`, which `vgg16-397923af.pth` and both linear-layer
  files are in.
- **The bytes torchmetrics runs.** Nothing is converted or re-exported.

## What is scored

Three choices decide the number, and each is the one the project's recorded
LPIPS was measured with:

- **alex with `normalize=True`, vgg with `normalize=False`** (`app/EvalLpips.cpp`).
  The second means vgg reads [0, 1] input as the top half of [-1, 1]. It is what
  the retired Python trainer did; changing it means rebaselining.
- **The 8-bit images**, quantized exactly as `--save-eval-images` writes them,
  with the `cc_` render re-fitted from those -- what `eval_lpips.py` and 3DGS's
  `metrics.py` score. l1/psnr/ssim stay on the float render. On bonsai
  (`images_4`, 3000 steps) the float render averaged 1.8% (alex) and 0.9% (vgg)
  higher, which is the size of the differences LPIPS gets compared on.
- **torchmetrics' `_normalize_tensor`**: `x / sqrt(1e-8 + |x|^2)`, not the
  original package's `x / (|x| + 1e-10)`.

Measured agreement:

| against | worst relative difference |
|---|---|
| torchmetrics 1.9.0 on the CPU, synthetic views 64x48 to 1000x700 (`lpips_test`) | 4e-7 |
| torchmetrics on real eval PNG pairs (`lpips_test --pair`) | 1e-7 |
| `eval_lpips.py --device cpu` over a run's 37 views (bonsai) | every value, to the 6 digits `metrics.json` prints |
| itself, in 64- and 256-pixel tiles | 2e-8 |

fp16 weights (the cooperative-matrix GEMM) were tried: 1.46x faster on VGG and
3e-4 off torchmetrics, so the weights stay fp32.

## Tiles

A view whose working set does not fit the workspace (by default half the
device's free memory, at most 3 GiB) is scored in square tiles. Two things make
that exact rather than approximate:

- **Every tile starts on a multiple of 16**, both nets' total stride, so a
  tile's tap positions are the whole image's, shifted by a whole number.
- **Every tile reads a halo** as wide as the deepest tap's receptive field
  reaches past its own pixels -- 81 px for AlexNet, 90 for VGG, both rounded to
  96 -- so each tap position a tile owns sees the same pixels and the same zero
  padding it would in the whole image. `halo()` derives it from the layer table.

A tile owns the tap positions between its two boundaries (`tap_bound`), so the
tiles partition each tap and the per-tap sums add up to the whole image's
before the spatial mean.

Memory: each stage's outputs are allocated on top of its inputs, which shrink
geometrically, so the peak is VGG's first stage -- the reference's tap, one
prediction's tap and the conv between them, 64 channels each at full
resolution: about 1 KB per pixel of tile for a reference and two predictions.
`tile_bytes()` sizes the arena allocation for allocation, and `nn::vk::Arena`
refuses (rather than corrupts) if it is ever short.

## Checkpoints

| file (cache name) | from | fallback | size |
|---|---|---|---|
| `alexnet-owt-7be5be79.pth` | download.pytorch.org | ModelScope `AI-ModelScope/alexnet` | 244 MB |
| `vgg16-397923af.pth` | download.pytorch.org | ModelScope `VoyagerX/vgg16-imagenet1k` | 553 MB |
| `lpips-v0.1-alex.pth` | `richzhang/PerceptualSimilarity` on GitHub | the project's mirror | 6 KB |
| `lpips-v0.1-vgg.pth` | `richzhang/PerceptualSimilarity` on GitHub | the project's mirror | 7 KB |

Every file is held to its SHA-256 (`Fetch.cpp`); ModelScope stores LFS objects
under it, so a fallback cannot serve other bytes. Only `features.*` of the
backbones is read -- 2.5 M and 14.7 M parameters of the 61 M and 138 M in the
files.

`--lpips-checkpoints <folder>` reads the four from a folder instead and
downloads nothing; the linear layers may carry their upstream names there
(`alex.pth`, `vgg.pth`), so torch's hub cache (`~/.cache/torch/hub/checkpoints`)
works as it is. Otherwise `setup_engine()` fetches what is missing before
training starts, and a failed download fails the run then rather than after it.
The GUI fetches the same files through `FileDownload` in `TrainRunner`, with a
progress bar, before it parses the dataset.

## Speed

RTX 3070 Laptop, fp32, a reference and two predictions (what one eval view is):

| view | AlexNet | VGG16 |
|---|---|---|
| 333x257 | 12 ms | 72 ms |
| 1000x700 | 35 ms | 524 ms |

VGG runs ~2.5 TFLOP/s on the fp32 GEMM. The eval pass scores one view at a time
on the inference device while the engine renders the next, and the host
metrics run beside it.

## Testing

```
./build_vulkan/nn_ops_test               # maxpool among the general ops
./build_vulkan/torch_checkpoint_test     # the legacy format, as fixtures
./build_vulkan/lpips_test                # cached checkpoints, or SKIP
./build_vulkan/lpips_test --fetch        # download what is missing first
./build_vulkan/lpips_test ~/.cache/torch/hub/checkpoints
./build_vulkan/lpips_test --pair eval-gt-00000.png eval-render-00000.png
python3 tools/lpips/compare_torchmetrics.py eval-gt-00000.png eval-render-00000.png
python3 tools/lpips/compare_torchmetrics.py --golden   # lpips_test's kGolden
```
