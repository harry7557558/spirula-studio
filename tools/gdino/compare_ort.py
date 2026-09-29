#!/usr/bin/env python3
"""Compare src/gdino/'s head against onnxruntime on onnx-community's
grounding-dino-tiny export (the same weights as IDEA-Research's safetensors).

Two limits of that export: its input is fixed
at 800x800, and the text-mask loop was traced for ONE phrase, so only a
single-phrase prompt ("[CLS] shoe . [SEP]") is a fair comparison.

    pip install onnx onnxruntime numpy pillow tokenizers
    python3 tools/gdino/compare_ort.py --onnx model.onnx --image shoe.jpg \
        --text shoe --vocab vocab.txt --out /tmp/ref      # prints the --ids
    SS_NN_COOPMAT=0 SS_GDINO_F32_WEIGHTS=1 ./build_vulkan/gdino_test \
        --model gdino-tiny --npy /tmp/ref/input.npy --ids <ids> --out /tmp/ours
    python3 tools/gdino/compare_ort.py --ref /tmp/ref --ours /tmp/ours

Measured: 3e-5 / 5e-5 relative L2 on logits / boxes in f32. As shipped the
query order can differ (the top-900 selection is sensitive to rounding), so
detections are matched by box rather than by row.
"""
import argparse
import os

import numpy as np


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--onnx")
    ap.add_argument("--image")
    ap.add_argument("--text")
    ap.add_argument("--vocab")
    ap.add_argument("--out")
    ap.add_argument("--ref")
    ap.add_argument("--ours")
    a = ap.parse_args()

    if a.onnx:
        import onnxruntime as ort
        from PIL import Image
        from tokenizers import BertWordPieceTokenizer
        mean = np.array([0.485, 0.456, 0.406], np.float32)
        std = np.array([0.229, 0.224, 0.225], np.float32)
        im = Image.open(a.image).convert("RGB").resize((800, 800), Image.BILINEAR)
        x = ((np.asarray(im, np.float32) / 255.0 - mean) / std).transpose(2, 0, 1)[None]
        ids = np.array([BertWordPieceTokenizer(a.vocab, lowercase=True)
                        .encode(a.text.strip(" .") + " .").ids], np.int64)
        s = ort.InferenceSession(a.onnx, providers=["CPUExecutionProvider"])
        lg, bx = s.run(None, {"pixel_values": x.astype(np.float32), "input_ids": ids,
                              "token_type_ids": np.zeros_like(ids),
                              "attention_mask": np.ones_like(ids),
                              "pixel_mask": np.ones((1, 800, 800), np.int64)})
        os.makedirs(a.out, exist_ok=True)
        np.save(os.path.join(a.out, "input.npy"), x.astype(np.float32))
        np.save(os.path.join(a.out, "logits.npy"), lg[:, :, :ids.shape[1]])
        np.save(os.path.join(a.out, "boxes.npy"), bx)
        print("--ids", ",".join(str(i) for i in ids[0]))
        return

    lr, br = np.load(os.path.join(a.ref, "logits.npy")), np.load(os.path.join(a.ref, "boxes.npy"))
    lo, bo = np.load(os.path.join(a.ours, "logits.npy")), np.load(os.path.join(a.ours, "boxes.npy"))
    print("logits relL2 %.3g  boxes relL2 %.3g" % (np.linalg.norm(lo - lr) / np.linalg.norm(lr),
                                                   np.linalg.norm(bo - br) / np.linalg.norm(br)))
    pr, po = 1 / (1 + np.exp(-lr[0])), 1 / (1 + np.exp(-lo[0]))
    sr, so = pr.max(-1), po.max(-1)
    for i in np.argsort(-sr)[:10]:
        j = np.abs(bo[0] - br[0][i]).sum(-1).argmin()
        print("ref %.3f  ours %.3f  box L1 %.4f" % (sr[i], so[j], np.abs(bo[0][j] - br[0][i]).sum()))


if __name__ == "__main__":
    main()
