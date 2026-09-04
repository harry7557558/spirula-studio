#!/usr/bin/env python3
"""Compare src/loma/'s forward passes against onnxruntime on the same bytes.

Both sides read the same .onnx files, so a disagreement here is ours. Dump our
side first, then point this at the same image and checkpoints:

    pip install onnx onnxruntime pillow numpy
    ./build/loma_test --image A.jpg --out /tmp/a.bin --max-image-size 1600
    ./build/loma_test --image B.jpg --out /tmp/b.bin --max-image-size 1600
    python3 tools/loma/compare_ort.py --image A.jpg --ours /tmp/a.bin
    python3 tools/loma/compare_ort.py --image A.jpg --ours /tmp/a.bin \\
        --image2 B.jpg --ours2 /tmp/b.bin --matches /tmp/m.bin

The dump carries the size the C++ side actually ran at, so this resizes the
same way loma_test does (nearest neighbour to --max-image-size) rather than
guessing.
"""
import argparse
import os
import struct
import sys

import numpy as np

CACHE = os.path.expanduser("~/.cache/spirulae-splat/models")
IMAGENET_MEAN = np.array([0.485, 0.456, 0.406], np.float32)
IMAGENET_STD = np.array([0.229, 0.224, 0.225], np.float32)
DEFAULTS = {
    "detector": "loma_detector.onnx",
    "descriptor": "loma_descriptor_dedode_b.onnx",
    "matcher": "loma_matcher_B128.onnx",
}


def read_dump(path):
    with open(path, "rb") as f:
        magic = f.read(8)
        assert magic == b"LOMAFEAT", f"{path}: not a loma_test dump"
        version, w, h, count, dim = struct.unpack("<IiiII", f.read(20))
        assert version == 1, f"{path}: version {version}"
        kp = np.frombuffer(f.read(count * 12), dtype=np.float32).reshape(count, 3)
        desc = np.frombuffer(f.read(count * dim * 4), dtype=np.float32)
        desc = desc.reshape(count, dim)
    return dict(w=w, h=h, xy=kp[:, :2], score=kp[:, 2], desc=desc)


def read_matches(path):
    with open(path, "rb") as f:
        (n,) = struct.unpack("<I", f.read(4))
        raw = np.frombuffer(f.read(n * 12), dtype=np.uint8).reshape(n, 12)
    i = raw[:, 0:4].copy().view(np.uint32).ravel()
    j = raw[:, 4:8].copy().view(np.uint32).ravel()
    s = raw[:, 8:12].copy().view(np.float32).ravel()
    return i, j, s


def load_rgb(path, max_size):
    from PIL import Image

    img = Image.open(path).convert("RGB")
    a = np.asarray(img)
    m = max(a.shape[0], a.shape[1])
    if max_size > 0 and m > max_size:
        # loma_test's own shrink: nearest neighbour, so the two sides see the
        # identical bytes and the filter is not what is under test.
        s = max_size / m
        nh, nw = max(1, int(a.shape[0] * s)), max(1, int(a.shape[1] * s))
        ys = np.minimum(a.shape[0] - 1, ((np.arange(nh) + 0.5) / s).astype(int))
        xs = np.minimum(a.shape[1] - 1, ((np.arange(nw) + 0.5) / s).astype(int))
        a = a[ys][:, xs]
    return np.ascontiguousarray(a)


def to_square(rgb, size):
    """PIL bicubic to size x size, which is what LoMa's read_image does."""
    from PIL import Image

    return np.asarray(
        Image.fromarray(rgb).resize((size, size), Image.BICUBIC), dtype=np.float32
    ) / 255.0


def run(session, feeds):
    return session.run(None, feeds)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--image", required=True)
    ap.add_argument("--ours", required=True, help="dump from loma_test --out")
    ap.add_argument("--image2")
    ap.add_argument("--ours2")
    ap.add_argument("--matches", help="match dump from loma_test --match ... --out")
    ap.add_argument("--detector", default=os.path.join(CACHE, DEFAULTS["detector"]))
    ap.add_argument("--descriptor", default=os.path.join(CACHE, DEFAULTS["descriptor"]))
    ap.add_argument("--matcher", default=os.path.join(CACHE, DEFAULTS["matcher"]))
    ap.add_argument("--max-image-size", type=int, default=1600)
    ap.add_argument("--tol-px", type=float, default=1.0)
    ap.add_argument("--dump", help="SS_LOMA_DUMP directory for --image; feeds ORT "
                                   "the bytes we ran on, which takes the JPEG "
                                   "decoder and the resampler out of the numbers")
    ap.add_argument("--dump2", help="the same for --image2")
    args = ap.parse_args()

    import onnxruntime as ort

    so = ort.SessionOptions()
    so.log_severity_level = 3
    det = ort.InferenceSession(args.detector, so, providers=["CPUExecutionProvider"])
    desc = ort.InferenceSession(args.descriptor, so, providers=["CPUExecutionProvider"])

    sq = desc.get_inputs()[0].shape
    side = int(sq[2]) if isinstance(sq[2], int) else 784

    def extract(image_path, ours_path, dump):
        ours = read_dump(ours_path)
        rgb = load_rgb(image_path, args.max_image_size)
        h, w = rgb.shape[:2]
        assert (w, h) == (ours["w"], ours["h"]), (
            f"our dump ran at {ours['w']}x{ours['h']}, this script made {w}x{h}"
        )
        # The graph carries its own ImageNet Sub/Div, so it wants the raw
        # [0, 1] image. With --dump, undo ours instead: PIL and stb_image
        # decode a JPEG differently and +-1 in a byte swamps everything here.
        if dump:
            norm = np.load(os.path.join(dump, "det_input.npy"))
            chw = (norm * IMAGENET_STD + IMAGENET_MEAN).transpose(2, 0, 1)
        else:
            chw = rgb.transpose(2, 0, 1).astype(np.float32) / 255.0
        chw = np.ascontiguousarray(chw, dtype=np.float32)
        n = len(ours["score"])
        kp, prob = run(
            det, {"image": chw[None], "num_keypoints": np.array([n], np.int64)}
        )
        if dump:
            din = np.load(os.path.join(dump, "desc_input.npy")).transpose(2, 0, 1)[None]
        else:
            din = to_square(rgb, side).transpose(2, 0, 1)[None]
        din = np.ascontiguousarray(din, dtype=np.float32)
        (d,) = run(desc, {"image": din, "keypoints": kp})
        # Again at OUR keypoints, so the descriptor can be compared without the
        # detector's own disagreement moving the sample point.
        mine_kp = np.stack(
            [2 * ours["xy"][:, 0] / w - 1, 2 * ours["xy"][:, 1] / h - 1], -1
        )[None].astype(np.float32)
        (d_at_ours,) = run(desc, {"image": din, "keypoints": mine_kp})
        px = np.stack([(kp[0, :, 0] + 1) * 0.5 * w, (kp[0, :, 1] + 1) * 0.5 * h], -1)
        return ours, px, prob[0], d[0], d_at_ours[0], (w, h)

    def report(tag, ours, px, prob, d, d_at_ours):
        print(f"\n{tag}: {len(px)} ORT keypoints, {len(ours['score'])} ours")
        # Nearest-neighbour agreement in pixels, both ways round.
        a, b = ours["xy"], px
        k = min(len(a), len(b))
        if k == 0:
            return
        dist = np.linalg.norm(a[:, None, :] - b[None, :, :], axis=-1)
        near = dist.argmin(1)
        dd = dist[np.arange(len(a)), near]
        hit = dd <= args.tol_px
        print(f"  matched <= {args.tol_px} px: {hit.mean() * 100:.1f}%"
              f"   mean offset {(a[hit] - b[near[hit]]).mean(0)} px")
        ds = np.abs(ours["score"][hit] - prob[near[hit]])
        rel_s = ds / (prob[near[hit]] + 1e-30)
        print(f"  score at those: |ours - ort| max {ds.max():.3e}, "
              f"median relative {np.median(rel_s):.3e}")
        # Descriptors at the SAME positions: this is the descriptor network on
        # its own, with the detector's disagreement held out.
        u, v = ours["desc"], d_at_ours
        cos = (u * v).sum(1) / (np.linalg.norm(u, axis=1) * np.linalg.norm(v, axis=1) + 1e-30)
        rel = np.linalg.norm(u - v, axis=1) / (np.linalg.norm(v, axis=1) + 1e-30)
        print(f"  descriptor at our keypoints: cosine mean {cos.mean():.6f} "
              f"min {cos.min():.6f}, relative L2 mean {rel.mean():.3e} max {rel.max():.3e}")

    oursA, pxA, probA, dA, dAo, wh = extract(args.image, args.ours, args.dump)
    report("A", oursA, pxA, probA, dA, dAo)

    if not (args.image2 and args.ours2):
        return 0

    oursB, pxB, probB, dB, dBo, wh2 = extract(args.image2, args.ours2, args.dump2)
    report("B", oursB, pxB, probB, dB, dBo)

    # The matcher, fed OUR features on both sides: this compares the matcher
    # alone, with the detector's and descriptor's disagreement held out.
    mat = ort.InferenceSession(args.matcher, so, providers=["CPUExecutionProvider"])

    def norm(o):
        return np.stack(
            [2 * o["xy"][:, 0] / o["w"] - 1, 2 * o["xy"][:, 1] / o["h"] - 1], -1
        )[None].astype(np.float32)

    outs = mat.run(
        None,
        {
            "kpts0": norm(oursA),
            "kpts1": norm(oursB),
            "desc0": oursA["desc"][None],
            "desc1": oursB["desc"][None],
        },
    )
    names = [o.name for o in mat.get_outputs()]
    m0 = outs[names.index("m0")][0]
    ms0 = outs[names.index("mscores0")][0]
    ort_pairs = {int(i): int(j) for i, j in enumerate(m0) if j >= 0}
    print(f"\nmatcher (our features on both sides): ORT {len(ort_pairs)} matches")

    if not args.matches:
        return 0
    oi, oj, osc = read_matches(args.matches)
    ours_pairs = {int(i): int(j) for i, j in zip(oi, oj)}
    print(f"  ours {len(ours_pairs)} matches")
    both = set(ours_pairs) & set(ort_pairs)
    same = sum(1 for i in both if ours_pairs[i] == ort_pairs[i])
    union = set(ours_pairs) | set(ort_pairs)
    print(f"  same partner on {same}/{len(both)} shared rows, "
          f"{len(union) - len(both)} rows matched by only one side")
    if both:
        sc = {int(i): float(s) for i, s in zip(oi, osc)}
        d = np.array([abs(sc[i] - float(ms0[i])) for i in both])
        print(f"  score |ours - ort|: max {d.max():.3e}, mean {d.mean():.3e}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
