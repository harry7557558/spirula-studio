# Text-prompted boxes, Grounding DINO (`src/gdino/`)

Grounding DINO (Liu et al., 2023) on the inference layer: an image and some
noun phrases in, boxes labelled with the phrase they matched out. Paired with a
SAM 2.1 checkpoint it is lang-segment-anything: the detector turns words into
boxes, SAM cuts each box out. That gives SAM 2 -- visual prompts only -- text,
at about a third of SAM 3's cost per frame (~250 ms against ~1 s on a laptop
GPU with SAM 2.1 Small).

`sam::Masker` drives it (MaskOptions::detector): every phrase, positive and
negative, goes into ONE detection pass ("person . car . tree ."), and each box
is decoded by the SAM session with a box prompt. Text is detected on every
frame afresh -- lang-segment-anything's behaviour, and what
`reference/scripts/mask.py` always did -- while clicked objects are still
tracked through SAM 2's memory bank.

## Checkpoints

| id | backbone | file | ms / frame (1333x750, RTX 5070 Laptop) |
|---|---|---|---|
| `gdino-tiny` | Swin-T | 689 MB fp32 (346 MB on device) | ~90 model, ~120 with pre-processing |
| `gdino-base` | Swin-B, window 12 | 933 MB fp32 (496 MB on device) | ~150 |

IDEA-Research's `model.safetensors` on Hugging Face, and bert-base-uncased's
`vocab.txt` from the same repository, both mirrored byte for byte by
IDEA-Research's ModelScope repositories. Apache-2.0. lang-segment-anything
defaults to base; tiny is the fast option.

## Layout

```
GroundingDino.h   the public surface: ids, resolve, Detector::detect
Tokenizer         BERT uncased WordPiece, no dependencies
Model             weights (fused q/k/v, folded layer scales) and the forward pass
Detector.cpp      ids, Hugging Face's resize, prompt assembly, post-processing
tests/            gdino_test: tokenizer golden, load, detect
```

The forward pass follows `modeling_grounding_dino.py`: BERT text tower, Swin +
input projections + sine position embeddings, six fusion-encoder layers
(bi-directional image-text attention, text self-attention within each phrase,
multi-scale deformable attention -- `nn::ms_deform_attn`), two-stage top-900
query selection on the host, and six decoder layers with iterative box
refinement. Position embeddings, reference points and proposals depend only on
the feature-map size and are cached per size; the text tower depends only on
the prompt and is cached per prompt, so a capture pays for it once.

## Two conventions that are not guessable

1. **The separator's position id.** Hugging Face's current vectorised
   `generate_masks_with_special_tokens_and_transfer_map` gives each "." position
   0; the loop it replaced -- and IDEA's original code, which trained these
   checkpoints -- numbers a phrase's tokens 0, 1, ... *through* its separator.
   We run the original.
2. **The ONNX export is only a reference for one phrase.** onnx-community's
   export traced that same Python loop for a single-phrase example, so the
   masks and position ids for any other phrase count are baked wrong in the
   graph. `tools/gdino/compare_ort.py` compares single-phrase prompts only.

## Testing

```bash
./build_vulkan/gdino_test                                        # tokenizer + load, or SKIP
./build_vulkan/gdino_test --model gdino-base --image IMG --text "shoe; hand"
python3 tools/gdino/compare_ort.py ...                           # parity, see the script
```

Against onnxruntime at 800x800: 3.0e-5 / 4.6e-5 relative L2 on logits / boxes
with `SS_GDINO_F32_WEIGHTS=1 SS_NN_COOPMAT=0`. As shipped the top-900 order can
shuffle between near-equal proposals, so rows do not line up, but matched
detections agree to ~0.01 in score and 1e-4 in box.
