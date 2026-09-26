# Subject masks, BiRefNet (`src/birefnet/`)

BiRefNet (Zheng et al., "Bilateral Reference for High-Resolution Dichotomous
Image Segmentation", CAAI AIR 2024) on the inference layer: an image in, the
mask of its main subject out, with no prompt. It is what an object capture --
a turntable, something held in hand -- wants from masking, and it needs no
words, which also makes it the one masking model that does not care what
language the user writes.

`sam::Masker` drives it (MaskOptions::model = a BiRefNet id or file), so the
CLI, the GUI's preview and the dataset run all reach it the same way; its mask
becomes one detection and goes through the same margin and polarity as a
prompted object. BiRefNet's natural polarity is *keep*: the CLI keeps the
subject unless `--remove-prompted`, and the GUI selects "Keep the subject" when
the model is picked.

## Checkpoints

| id | backbone | file | on device | ms / frame (RTX 5070 Laptop, 1024²) |
|---|---|---|---|---|
| `birefnet` | Swin-L, window 12 | 444 MB (fp16) | 445 MB + 1.1 GB arena | ~290 |
| `birefnet-lite` | Swin-T, window 7 | 178 MB (fp32) | 178 MB + 0.7 GB arena | ~135 |

Both are ZhengPeng7's own `model.safetensors` on Hugging Face, read in process
by `nn/io/Safetensors.cpp` with their PyTorch names; the fallback mirrors are
ModelScope repositories holding the same bytes (same SHA-256). MIT licence.

## The forward pass

`BiRefNet.cpp`, read against the Hugging Face repository's `birefnet.py`:

- the backbone runs twice, on the 1024² input and on a 512² copy
  (`mul_scl_ipt='cat'`), and each stage is the two concatenated;
- the three finer stages are resized onto the coarsest and concatenated
  (`cxt`), then squeezed by a `BasicDecBlk`;
- four decoder levels, each a `BasicDecBlk` with an `ASPPDeformable`
  (modulated deformable convs, `nn::modulated_deform_conv2d`), fed the image cut
  into patches (`dec_ipt`) and gated by the gradient-attention branch
  (`out_ref`), with lateral 1x1 convs between levels.

Load-time folding keeps the device work plain: every eval-mode BatchNorm goes
into its conv; the ASPP's global-pool branch is constant per pixel, so it
becomes the bias of `conv1`, whose other four column blocks accumulate branch
by branch instead of over a 1280-channel concat; and `conv_out1` is folded into
both of its inputs -- the 240-channel 1024² concat it reads would be 960 MB.

## The one convention that is not guessable

**The patch order.** `dec_ipt` stacks the image's blocks as channels. The
current reference code does it with einops, `(c hg wg)`; the code these
checkpoints were trained with (before November 2024, `get_patches_batch`) split
columns first, then rows, giving `(wg hg c)`. Both checkpoints predate the
change and were never retrained, and the onnx-community exports carry the old
order too. We run the order the weights were trained on
(`nn::BlockOrder::ColumnsThenRows`); the einops order still produces a
plausible mask (99.96% of pixels agree on the test image), which is exactly why
it is easy to "fix" and wrong.

## Testing

```bash
./build_vulkan/birefnet_test                          # cached checkpoint, synthetic image, or SKIP
./build_vulkan/birefnet_test --model birefnet --image IMG --out mask.png --repeat 3
python3 tools/birefnet/compare_ort.py ...             # parity, see the script
```

Against onnxruntime on onnx-community's `BiRefNet_lite-ONNX` (same weights):
3.1e-6 relative L2 on the logits with `SS_BIREFNET_F32_WEIGHTS=1
SS_NN_COOPMAT=0`, 7e-4 as shipped, and every mask pixel identical. The full
model compares at 2e-4 in f32 -- its safetensors are fp16, the export was made
from the fp32 original.
