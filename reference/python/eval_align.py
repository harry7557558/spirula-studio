#!/usr/bin/env python3
"""Alignment-corrected eval for a run saved with --save-eval-images 1.

For every held-out view: colour-correct the render (quadratic cc, as
src/app/EvalMetrics.cpp), fit a smooth flow (bicubic control grid,
coarse-to-fine, Charbonnier) that warps the render onto the GT, then score

  raw    render vs GT
  cc     colour-corrected render vs GT          (matches the trainer's cc_)
  al     aligned render vs GT
  alcc   aligned, then colour-corrected, vs GT

PSNR / SSIM / LPIPS-alex (normalize=True) / LPIPS-vgg (normalize=False),
same conventions as reference/python/eval_lpips.py. Pixels the warp pulls
from outside the image, and masked pixels (eval-mask-*.png), take the GT value.

    python3 reference/python/eval_align.py <run_dir> [--grid 16] [--save-flow]

The ranking of runs held at grids 8, 16 and 24 on a phone-video capture; 16 is the
default. A planted 4.7 px smooth warp is recovered to 0.08 px.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

import numpy as np
import torch
import torch.nn.functional as F
from PIL import Image
from torchmetrics.image.lpip import LearnedPerceptualImagePatchSimilarity
from torchmetrics.functional.image import structural_similarity_index_measure as tm_ssim


def color_correct(img, ref, num_iters=5, eps=0.5 / 255, valid=None):
    c = img.shape[-1]
    cur = img.reshape(-1, c).astype(np.float64).copy()
    ref_mat = ref.reshape(-1, c).astype(np.float64)
    v = np.ones(cur.shape[0], bool) if valid is None else valid.reshape(-1)

    def unclipped(z):
        return (z >= eps) & (z <= 1 - eps)

    mask0 = unclipped(cur)
    for _ in range(num_iters):
        a = np.concatenate([cur[:, i:i + 1] * cur[:, i:] for i in range(c)]
                           + [cur, np.ones_like(cur[:, :1])], axis=-1)
        warp = []
        for i in range(c):
            b = ref_mat[:, i]
            m = mask0[:, i] & unclipped(cur[:, i]) & unclipped(b) & v
            ma = np.where(m[:, None], a, 0.0)
            mb = np.where(m, b, 0.0)
            warp.append(np.linalg.solve(ma.T @ ma + 1e-9 * np.eye(ma.shape[1]), ma.T @ mb))
        cur = np.clip(a @ np.stack(warp, axis=-1), 0.0, 1.0)
    return cur.reshape(img.shape).astype(np.float32)


def load(path):
    return np.asarray(Image.open(path).convert("RGB"), np.float32) / 255.0


def gauss_blur(x, sigma):
    if sigma <= 0:
        return x
    r = max(1, int(round(3 * sigma)))
    t = torch.arange(-r, r + 1, device=x.device, dtype=x.dtype)
    k = torch.exp(-0.5 * (t / sigma) ** 2)
    k = k / k.sum()
    c = x.shape[1]
    x = F.conv2d(F.pad(x, (r, r, 0, 0), mode="replicate"), k.view(1, 1, 1, -1).repeat(c, 1, 1, 1), groups=c)
    x = F.conv2d(F.pad(x, (0, 0, r, r), mode="replicate"), k.view(1, 1, -1, 1).repeat(c, 1, 1, 1), groups=c)
    return x


def flow_from_grid(theta, H, W):
    return F.interpolate(theta, size=(H, W), mode="bicubic", align_corners=True)


def warp(img, flow, mode="bilinear"):
    """Sample img at p + flow(p); flow in pixels of img. Returns (warped, valid)."""
    _, _, H, W = img.shape
    ys, xs = torch.meshgrid(torch.arange(H, device=img.device, dtype=img.dtype),
                            torch.arange(W, device=img.device, dtype=img.dtype), indexing="ij")
    sx = xs + flow[:, 0]
    sy = ys + flow[:, 1]
    grid = torch.stack([(sx + 0.5) / W * 2 - 1, (sy + 0.5) / H * 2 - 1], dim=-1)
    out = F.grid_sample(img, grid, mode=mode, padding_mode="border", align_corners=False)
    valid = (sx >= 0) & (sx <= W - 1) & (sy >= 0) & (sy <= H - 1)
    return out, valid[:, None].to(img.dtype)


def fit_flow(src, ref, grid_long=16, levels=(8, 4, 2, 1), iters=80, smooth=0.5, l2=1e-3,
             weight=None):
    """Flow (pixels at full res) such that warp(src, flow) ~= ref; `weight`
    [1,1,H,W] leaves pixels out of the fit."""
    _, _, H, W = src.shape
    if W >= H:
        gx, gy = grid_long, max(2, int(round(grid_long * H / W)))
    else:
        gy, gx = grid_long, max(2, int(round(grid_long * W / H)))
    theta = torch.zeros(1, 2, gy + 1, gx + 1, device=src.device, requires_grad=True)
    opt = torch.optim.Adam([theta], lr=0.5)
    for s in levels:
        hs, ws = max(8, H // s), max(8, W // s)
        a = F.interpolate(src, size=(hs, ws), mode="area") if s > 1 else src
        b = F.interpolate(ref, size=(hs, ws), mode="area") if s > 1 else ref
        a = gauss_blur(a, 1.0)
        b = gauss_blur(b, 1.0)
        wl = None
        if weight is not None:
            wl = F.interpolate(weight, size=(hs, ws), mode="area") if s > 1 else weight
        for g in opt.param_groups:
            g["lr"] = 0.5 * s ** 0.5
        for _ in range(iters):
            opt.zero_grad()
            flow = flow_from_grid(theta, hs, ws) * torch.tensor([ws / W, hs / H], device=src.device).view(1, 2, 1, 1)
            w, v = warp(a, flow)
            if wl is not None:
                v = v * wl
            r = torch.sqrt((w - b) ** 2 + 1e-6) * v
            data = r.sum() / (v.sum() * 3 + 1)
            d2x = theta[..., 2:] - 2 * theta[..., 1:-1] + theta[..., :-2]
            d2y = theta[..., 2:, :] - 2 * theta[..., 1:-1, :] + theta[..., :-2, :]
            # The prior is in full-res pixels; the data term is a mean, so the
            # weights are per pixel of error per pixel of flow.
            reg = smooth * 1e-3 * ((d2x ** 2).mean() + (d2y ** 2).mean()) + l2 * 1e-3 * (theta ** 2).mean()
            (data + reg).backward()
            opt.step()
    with torch.no_grad():
        return flow_from_grid(theta.detach(), H, W)


def psnr(a, b):
    mse = float(((a - b) ** 2).mean())
    return 10 * np.log10(1.0 / max(mse, 1e-12))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("run_dir", type=Path)
    ap.add_argument("--grid", type=int, default=16)
    ap.add_argument("--save-flow", action="store_true")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--device", default="cuda")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    dev = args.device

    pairs = []
    for gt_path in sorted(args.run_dir.glob("eval-gt-*.png")):
        idx = re.search(r"(\d+)\.png$", gt_path.name).group(1)
        rp = args.run_dir / f"eval-render-{idx}.png"
        mp = args.run_dir / f"eval-mask-{idx}.png"
        if rp.is_file():
            pairs.append((idx, gt_path, rp, mp if mp.is_file() else None))
    if args.limit:
        pairs = pairs[:args.limit]
    if not pairs:
        sys.exit(f"no eval pairs in {args.run_dir}")

    alex = LearnedPerceptualImagePatchSimilarity(net_type="alex", normalize=True).to(dev)
    vgg = LearnedPerceptualImagePatchSimilarity(net_type="vgg", normalize=False).to(dev)

    def chw(a):
        return torch.from_numpy(np.ascontiguousarray(a)).permute(2, 0, 1)[None].to(dev)

    out = {}

    def put(k, v):
        out.setdefault(k, []).append(float(v))

    flows_dir = args.run_dir / "align_flow"
    if args.save_flow:
        flows_dir.mkdir(exist_ok=True)

    for idx, gp, rp, mp in pairs:
        g = load(gp)
        r = load(rp)
        mask = None
        if mp is not None:
            mask = load(mp)[..., 0] > 0.5
        cc = color_correct(r, g, valid=mask)
        gt_t, r_t, cc_t = chw(g), chw(r), chw(cc)
        # A masked pixel scores as the GT, as the trainer's masked_ metrics do.
        keep = None
        if mask is not None:
            keep = torch.from_numpy(mask.astype(np.float32))[None, None].to(dev)
            r_t = r_t * keep + gt_t * (1 - keep)
            cc_t = cc_t * keep + gt_t * (1 - keep)
        flow = fit_flow(cc_t, gt_t, grid_long=args.grid, weight=keep)
        # Bicubic for the scored warp so a sub-pixel shift costs little sharpness.
        al_t, v = warp(r_t, flow, mode="bicubic")
        al_t = al_t.clamp(0, 1)
        al_t = al_t * v + gt_t * (1 - v)
        al = al_t[0].permute(1, 2, 0).cpu().numpy()
        valid = v[0, 0].cpu().numpy() > 0.5
        if mask is not None:
            valid = valid & mask
        alcc = color_correct(al, g, valid=valid)
        alcc_t = chw(alcc)
        alcc_t = alcc_t * v + gt_t * (1 - v)
        if keep is not None:
            al_t = al_t * keep + gt_t * (1 - keep)
            alcc_t = alcc_t * keep + gt_t * (1 - keep)
        fm = flow[0].norm(dim=0)
        put("flow_mean_px", fm.mean())
        put("flow_max_px", fm.max())
        put("valid_frac", v.mean())
        for name, t in (("raw", r_t), ("cc", cc_t), ("al", al_t), ("alcc", alcc_t)):
            with torch.no_grad():
                put(f"{name}_psnr", psnr(t, gt_t))
                put(f"{name}_ssim", tm_ssim(t, gt_t, data_range=1.0))
                put(f"{name}_lpips_alex", alex(gt_t, t))
                put(f"{name}_lpips_vgg", vgg(gt_t, t))
        if args.save_flow:
            np.save(flows_dir / f"flow-{idx}.npy", flow[0].cpu().numpy().astype(np.float16))
        if not args.quiet:
            print(f"{idx}: flow {float(fm.mean()):.2f}px (max {float(fm.max()):.2f})  "
                  f"lpipsA raw {out['raw_lpips_alex'][-1]:.4f} cc {out['cc_lpips_alex'][-1]:.4f} "
                  f"alcc {out['alcc_lpips_alex'][-1]:.4f}  psnr cc {out['cc_psnr'][-1]:.2f} "
                  f"alcc {out['alcc_psnr'][-1]:.2f}", flush=True)

    avg = {f"avg_{k}": float(np.mean(v)) for k, v in out.items()}
    res = dict(out)
    res.update(avg)
    res["num_eval_images"] = len(pairs)
    # A --limit run is a peek and must not replace the full result.
    name = "metrics_align.json" if not args.limit else "metrics_align_limit.json"
    (args.run_dir / name).write_text(json.dumps(res, indent=1))
    print(f"{args.run_dir.name}  n={len(pairs)}  flow {avg['avg_flow_mean_px']:.2f}px")
    for m in ("psnr", "ssim", "lpips_alex", "lpips_vgg"):
        print(f"  {m:11s} " + "  ".join(f"{n}={avg[f'avg_{n}_{m}']:.4f}" for n in ("raw", "cc", "al", "alcc")))


if __name__ == "__main__":
    main()
