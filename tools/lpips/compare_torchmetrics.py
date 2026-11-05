#!/usr/bin/env python3
"""Score images with torchmetrics' LPIPS, for src/lpips/ to be compared against.

    pip install torch torchvision torchmetrics pillow numpy
    python3 tools/lpips/compare_torchmetrics.py --golden
    python3 tools/lpips/compare_torchmetrics.py gt.png render.png

--golden prints lpips_test.cpp's kGolden table: the synthetic views that test
builds, made here with the same integer hash, scored on the CPU in fp32. With
two PNGs it prints what `lpips_test --pair gt.png render.png` should print:
alex with normalize=True and vgg with normalize=False, the pairing
spirula's eval and reference/python/eval_lpips.py use.
"""
import argparse

import numpy as np
import torch
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity

# The rows of lpips_test.cpp's kGolden: every size with every (net, normalize).
SIZES = [(64, 48), (333, 257), (1000, 700)]
CASES = [("alex", True), ("vgg", False), ("vgg", True)]


def hash_u(x, y, c, seed):
    """lpips_test.cpp's uniform(): a [0, 1) float from 24 bits of a hash."""
    m = np.uint32
    h = (x.astype(m) * m(73856093)) ^ (y.astype(m) * m(19349663)) ^ \
        (c.astype(m) * m(83492791)) ^ m((seed * 2654435761) & 0xFFFFFFFF)
    h ^= h >> m(13)
    h *= m(0x5BD1E995)
    h ^= h >> m(15)
    return (h >> m(8)).astype(np.float64) / 16777216.0


def views(w, h):
    """The reference and the two predictions lpips_test.cpp's make_views builds."""
    y, x, c = np.meshgrid(np.arange(h), np.arange(w), np.arange(3), indexing="ij")
    pattern = ((x * 7 + y * 13 + c * 29) % 256) / 255.0
    ref = (0.6 * pattern + 0.4 * hash_u(x, y, c, 1)).astype(np.float32)
    noisy = np.clip(ref.astype(np.float64) + 0.3 * (hash_u(x, y, c, 2) - 0.5),
                    0.0, 1.0).astype(np.float32)
    shifted = ref[:, np.minimum(np.arange(w) + 3, w - 1), :]
    blend = (0.7 * ref.astype(np.float64) + 0.3 * shifted.astype(np.float64)).astype(np.float32)
    return ref, [noisy, blend]


def chw(a):
    return torch.from_numpy(np.ascontiguousarray(a)).permute(2, 0, 1)[None]


def metric(net, normalize):
    return LearnedPerceptualImagePatchSimilarity(net_type=net, normalize=normalize).eval()


def golden():
    print("const Golden kGolden[] = {")
    for (w, h) in SIZES:
        ref, preds = views(w, h)
        for net, normalize in CASES:
            m = metric(net, normalize)
            with torch.no_grad():
                s = [float(m(chw(ref), chw(p))) for p in preds]
            print(f'    {{{w}, {h}, "{net}", {str(normalize).lower()}, '
                  f'{{{s[0]:.9g}, {s[1]:.9g}}}}},')
    print("};")


def pair(gt_path, render_path):
    from PIL import Image

    def load(p):
        return np.asarray(Image.open(p).convert("RGB"), np.float32) / 255.0

    gt, render = chw(load(gt_path)), chw(load(render_path))
    with torch.no_grad():
        for net, normalize in [("alex", True), ("vgg", False)]:
            print(f"lpips_{net}: {float(metric(net, normalize)(gt, render)):.9g}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--golden", action="store_true")
    ap.add_argument("images", nargs="*", help="gt.png render.png")
    args = ap.parse_args()
    if args.golden:
        golden()
    elif len(args.images) == 2:
        pair(*args.images)
    else:
        ap.error("--golden, or two images")


if __name__ == "__main__":
    main()
