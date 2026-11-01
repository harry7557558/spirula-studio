#!/usr/bin/env python3
"""Add LPIPS to a run's metrics.json.

`spirula train --save-eval-images 1` writes one `eval-gt-NNNNN.png` /
`eval-render-NNNNN.png` pair per held-out view and a metrics.json holding
l1 / psnr / ssim and their colour-corrected `cc_` variants. A build with the
inference layer (SS_BUILD_SAM, the Vulkan default) adds LPIPS itself
(src/lpips/), scored on these same 8-bit images, so the two agree; this script
is for builds without it.

    python3 reference/python/eval_lpips.py <run_dir> [--write]

Prints lpips_alex / lpips_vgg / cc_lpips_alex / cc_lpips_vgg (per-image lists
plus avg_*), and with --write merges them into the run's metrics.json.

Requires only torch + torchmetrics + pillow. The colour correction is
reproduced below rather than imported, so `fused_bilagrid` is not needed.

NOTE on `normalize`: alex is scored with normalize=True and vgg with
normalize=False, which is what the retired Python trainer did. normalize=False
means torchmetrics expects [-1, 1] but is handed [0, 1], so lpips_vgg is
effectively measured on the upper half of the input range. That is reproduced
deliberately, so these numbers stay comparable with previously recorded runs.
Do not "fix" it without rebaselining every benchmark it is compared against.
"""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

import numpy as np
import torch
from PIL import Image
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity


def color_correct(img: np.ndarray, ref: np.ndarray,
                  num_iters: int = 5, eps: float = 0.5 / 255) -> np.ndarray:
    """Warp `img`'s colours onto `ref` by least squares over a quadratic
    expansion of each pixel, re-solved `num_iters` times while updating which
    pixels count as unsaturated. Matches src/app/EvalMetrics.cpp."""
    c = img.shape[-1]
    cur = img.reshape(-1, c).astype(np.float64).copy()
    ref_mat = ref.reshape(-1, c).astype(np.float64)

    def unclipped(z):
        return (z >= eps) & (z <= 1 - eps)

    mask0 = unclipped(cur)
    for _ in range(num_iters):
        a = np.concatenate(
            [cur[:, i:i + 1] * cur[:, i:] for i in range(c)]   # quadratic
            + [cur, np.ones_like(cur[:, :1])],                 # linear + bias
            axis=-1)
        warp = []
        for i in range(c):
            b = ref_mat[:, i]
            m = mask0[:, i] & unclipped(cur[:, i]) & unclipped(b)
            ma = np.where(m[:, None], a, 0.0)
            mb = np.where(m, b, 0.0)
            warp.append(np.linalg.solve(ma.T @ ma, ma.T @ mb))
        cur = np.clip(a @ np.stack(warp, axis=-1), 0.0, 1.0)
    return cur.reshape(img.shape).astype(np.float32)


def load(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), np.float32) / 255.0


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("run_dir", type=Path, help="a training run's output dir")
    ap.add_argument("--write", action="store_true",
                    help="merge the results into <run_dir>/metrics.json")
    ap.add_argument("--device", default="cuda" if torch.cuda.is_available() else "cpu")
    args = ap.parse_args()

    pairs = []
    for gt_path in sorted(args.run_dir.glob("eval-gt-*.png")):
        idx = re.search(r"(\d+)\.png$", gt_path.name).group(1)
        render = args.run_dir / f"eval-render-{idx}.png"
        if render.is_file():
            pairs.append((gt_path, render))
    if not pairs:
        raise SystemExit(
            f"no eval-gt-*.png / eval-render-*.png pairs in {args.run_dir} -- "
            f"train with --save-eval-images 1")

    nets = {
        # (net_type, normalize) -- see the module docstring on the asymmetry.
        "lpips_alex": LearnedPerceptualImagePatchSimilarity(
            net_type="alex", normalize=True).to(args.device),
        "lpips_vgg": LearnedPerceptualImagePatchSimilarity(
            net_type="vgg", normalize=False).to(args.device),
    }

    out: dict[str, list[float]] = {k: [] for k in nets}
    out.update({f"cc_{k}": [] for k in nets})

    for gt_path, render_path in pairs:
        gt_np, pred_np = load(gt_path), load(render_path)
        cc_np = color_correct(pred_np, gt_np)

        def chw(a: np.ndarray) -> torch.Tensor:
            return torch.from_numpy(a).permute(2, 0, 1)[None].to(args.device)

        gt_t, pred_t, cc_t = chw(gt_np), chw(pred_np), chw(cc_np)
        with torch.no_grad():
            for name, metric in nets.items():
                out[name].append(float(metric(gt_t, pred_t)))
                out[f"cc_{name}"].append(float(metric(gt_t, cc_t)))

    for key in list(out):
        avg = sum(out[key]) / len(out[key])
        out[f"avg_{key}"] = avg
        print(f"{key}: {avg}")

    if args.write:
        path = args.run_dir / "metrics.json"
        metrics = json.loads(path.read_text()) if path.is_file() else {}
        metrics.update(out)
        path.write_text(json.dumps(metrics, indent=4) + "\n")
        print(f"merged into {path}")


if __name__ == "__main__":
    main()
