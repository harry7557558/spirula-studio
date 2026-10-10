#!/usr/bin/env python3
"""Check src/moge/'s MoGe-3 refiner against an independent NumPy one.

MoGe-3 has no ONNX export and its own refiner needs FlexGEMM on Triton, so the
reference here is written from MoGe's v3.py / sparse_unet.py and FlexGEMM's
explicit-GEMM path: the same voxels, neighbour taps, pooling and upsampling,
in float64. It starts from OUR dumped head output and encoder features, so a
disagreement is in the refiner alone.

    SS_MOGE_F32_WEIGHTS=1 SS_NN_COOPMAT=0 SS_MOGE_DUMP=/tmp/ours \\
        ./build_vulkan/moge_test --model moge3-vitl --image IMG.jpg --num-tokens 1200
    python3 tools/moge/check_refiner.py --pt <the same moge3 .pt> --ours /tmp/ours

The .pt is read with a whitelisting unpickler; nothing in it is executed.
"""
import argparse
import collections
import itertools
import os
import pickle
import sys
import zipfile

import numpy as np


class _Stub:
    def __init__(self, storage, offset, shape, stride):
        self.storage, self.offset, self.shape, self.stride = storage, offset, shape, stride


class _Reader(pickle.Unpickler):
    def find_class(self, module, name):
        if (module, name) == ("collections", "OrderedDict"):
            return collections.OrderedDict
        if module == "torch._utils" and name.startswith("_rebuild_tensor"):
            return lambda storage, offset, shape, stride, *rest: _Stub(storage, offset, shape, stride)
        if module == "torch" and name.endswith("Storage"):
            return name
        raise pickle.UnpicklingError(f"refused {module}.{name}")

    def persistent_load(self, pid):
        return pid


def load_refiner(path):
    z = zipfile.ZipFile(path)
    pkl = next(n for n in z.namelist() if n.endswith("data.pkl"))
    prefix = pkl[: -len("data.pkl")]
    root = _Reader(z.open(pkl)).load()
    cfg = root["model_config"]
    out = {}
    for name, t in root["model"].items():
        if not name.startswith("refiner."):
            continue
        _, kind, key, _, _ = t.storage
        assert kind == "FloatStorage", kind
        buf = np.frombuffer(z.read(f"{prefix}data/{key}"), dtype=np.float32)
        a = np.lib.stride_tricks.as_strided(
            buf[t.offset:], shape=t.shape, strides=[s * 4 for s in t.stride])
        out[name[len("refiner."):]] = a.astype(np.float64)
    return out, cfg


class Level:
    def __init__(self, coords):
        self.coords = coords  # [M, 3] (i, j, z)
        span = coords.max(axis=0) + 3
        self.span = span
        keys = self._key(coords)
        self.order = np.argsort(keys)
        self.sorted = keys[self.order]

    def _key(self, c):
        c = c + 1  # room for the -1 a tap can step to
        return (c[:, 0] * self.span[1] + c[:, 1]) * self.span[2] + c[:, 2]

    def lookup(self, c):
        ok = (c >= 0).all(axis=1)
        k = self._key(np.where(ok[:, None], c, 0))
        pos = np.clip(np.searchsorted(self.sorted, k), 0, len(self.sorted) - 1)
        hit = ok & (self.sorted[pos] == k)
        return np.where(hit, self.order[pos], -1)

    def neighbours(self):
        if not hasattr(self, "_nbr"):
            taps = list(itertools.product((-1, 0, 1), repeat=3))
            self._nbr = np.stack([self.lookup(self.coords + np.array(d)) for d in taps], 1)
        return self._nbr


def conv(x, lvl, w, b):
    co, _, _, _, ci = w.shape
    w = w.reshape(co, 27, ci)
    nbr = lvl.neighbours()
    out = np.zeros((x.shape[0], co))
    for t in range(27):
        idx = nbr[:, t]
        g = np.where((idx >= 0)[:, None], x[np.maximum(idx, 0)], 0.0)
        out += g @ w[:, t, :].T
    return out + b


def silu(x):
    return x / (1.0 + np.exp(-x))


def layer_norm(x, w, b, eps=1e-6):
    m = x.mean(axis=1, keepdims=True)
    v = ((x - m) ** 2).mean(axis=1, keepdims=True)
    return (x - m) / np.sqrt(v + eps) * w + b


def res_block(x, lvl, W, p):
    h = silu(layer_norm(x, W[p + ".norm1.weight"], W[p + ".norm1.bias"]))
    h = silu(conv(h, lvl, W[p + ".conv1.weight"], W[p + ".conv1.bias"]))
    h = conv(h, lvl, W[p + ".conv2.weight"], W[p + ".conv2.bias"])
    return x + h


def linear(x, W, p):
    return x @ W[p + ".weight"].T + W[p + ".bias"]


def stage(x, lvl, W, name):
    n = 0
    while f"{name}.{n}.norm1.weight" in W:
        x = res_block(x, lvl, W, f"{name}.{n}")
        n += 1
    return x


def refine_once(coord, table, W, factors, bins):
    H, Wd, _ = coord.shape
    logz = coord[..., 2].astype(np.float32)
    zq = np.round(logz * np.float32(bins)).astype(np.int64)
    z = (zq - zq.min()).reshape(-1)
    ii, jj = np.meshgrid(np.arange(H), np.arange(Wd), indexing="ij")
    lv = [Level(np.stack([ii.reshape(-1), jj.reshape(-1), z], 1))]
    parents = []
    for f in factors:
        uniq, inv = np.unique(lv[-1].coords // f, axis=0, return_inverse=True)
        parents.append(inv.reshape(-1))
        lv.append(Level(uniq))

    x = linear(coord.reshape(-1, 3).astype(np.float64), W, "input_proj")
    skips = []
    for l in range(len(lv)):
        x = stage(x, lv[l], W, f"down_stages.{l}")
        if l + 1 == len(lv):
            break
        skips.append(x)
        m = len(lv[l + 1].coords)
        cnt = np.bincount(parents[l], minlength=m).astype(np.float64)
        pooled = np.zeros((m, x.shape[1]))
        np.add.at(pooled, parents[l], x)
        x = linear(pooled / cnt[:, None], W, f"downsample_blocks.{l}.linear")

    top = lv[-1].coords
    enc = table[top[:, 0], top[:, 1]].astype(np.float64)
    e = linear(enc, W, "encoder_fuse")
    h = silu(linear(np.concatenate([x, e], 1), W, "fuse_proj.0"))
    x = linear(h, W, "fuse_proj.2")
    x = stage(x, lv[-1], W, "bottleneck_stage")
    for i in range(len(lv) - 1):
        t = len(lv) - 2 - i
        y = linear(x, W, f"upsample_blocks.{i}.linear")
        x = y[parents[t]] + skips[t]
        x = stage(x, lv[t], W, f"up_stages.{i}")
    delta = linear(x, W, "out_proj").reshape(H, Wd)
    out = coord.copy()
    out[..., 2] = out[..., 2] + delta
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--pt", required=True, help="the MoGe-3 .pt checkpoint")
    ap.add_argument("--ours", required=True, help="SS_MOGE_DUMP directory")
    ap.add_argument("--steps", type=int, default=3, help="refine steps the C++ side ran")
    ap.add_argument("--tol", type=float, default=1e-3, help="max |log z| difference")
    args = ap.parse_args()

    W, cfg = load_refiner(args.pt)
    factors = cfg["refiner"].get("downsample_factors") or [2] * (len(cfg["refiner"]["model_channels"]) - 1)
    bins = cfg.get("refiner_depth_resolution", 256)
    raw = np.load(os.path.join(args.ours, "points_head_raw.npy")).astype(np.float64)
    table = np.load(os.path.join(args.ours, "refiner_table.npy"))
    ours = np.load(os.path.join(args.ours, "points_head_refined.npy"))

    coord = raw
    for _ in range(args.steps):
        coord = refine_once(coord, table, W, factors, bins)

    d = np.abs(coord[..., 2] - ours[..., 2])
    moved = np.abs(coord[..., 2] - raw[..., 2])
    print(f"refinement moved log z by median {np.median(moved):.3e}, max {moved.max():.3e}")
    print(f"ours vs reference: median {np.median(d):.3e}, p99 {np.percentile(d, 99):.3e}, "
          f"max {d.max():.3e} over {d.size} pixels")
    bad = d.max() > args.tol
    print("FAIL" if bad else "ok")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
