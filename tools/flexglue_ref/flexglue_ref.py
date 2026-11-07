#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "torch>=2.4", "numpy", "opencv-python-headless", "onnx", "onnxruntime",
#   "flexglue @ git+https://github.com/cvg/FlexGlue.git@c6cbc7a933bb70ab58608e8835d99b6cc4adca1a",
# ]
# ///
"""Per-stage PyTorch reference of FlexGlue in COLMAP's sparse configuration.

The stages are read off upstream's own modules, and every run checks them
against upstream's end-to-end `FlexGlue.match(..., sparse=True)`. With
--check-onnx it also checks them against the ONNX graphs COLMAP runs.

    uv run tools/flexglue_ref/flexglue_ref.py dump --pairs SPEC.json --out DIR
    uv run tools/flexglue_ref/flexglue_ref.py fixtures      # the committed tier
    uv run tools/flexglue_ref/flexglue_ref.py time --pairs SPEC.json --device cuda

Names, layouts and the two tiers are described in tools/flexglue_ref/README.md.
"""
import argparse
import contextlib
import hashlib
import json
import os
import pathlib
import platform
import sys
import time
import urllib.request

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
FIXTURES = HERE / "fixtures"
FORMAT_VERSION = 1

UPSTREAM_COMMIT = "c6cbc7a933bb70ab58608e8835d99b6cc4adca1a"
RELEASE = "https://github.com/cvg/FlexGlue/releases/download/v0.1/"
CHECKPOINT = ("flexglue.pth", "d7371f743b8cdc4adf723c2aa05b181587fe4bc177fced139697975dff790068")
ONNX_DESCRIPTOR = ("flexglue_descriptor.onnx",
                   "93fab8a4ccde1b7331640915357d96cff0929ea87134872f37f7b0150d9914ff")
ONNX_MATCHER = ("flexglue_matcher_early_stop.onnx",
                "c86714cd8ffac70c17ee48c346270497e21846831336e12d307390591aa12ea7")

SIZE = 512                      # COLMAP's descriptor input, squashed
# COLMAP's FlexGlueMatchingOptions defaults, and the layers its graph checks.
COLMAP = dict(filter_threshold=0.1, max_distance_px=3.0, min_covisibility=0.5,
              early_stop_decay=0.5)
CHECK_LAYERS = (2, 4, 6, 8)
# (min_covisibility, early_stop_decay) per output prefix. COLMAP's defaults never stop a
# public pair at an odd layer, so they cannot tell 2/4/6/8 from every-layer checks;
# decay 0.2 stops lamar_lookalike at 3 under every-layer checks and at 4 under COLMAP's.
GATES = {"colmap": (COLMAP["min_covisibility"], COLMAP["early_stop_decay"]),
         "colmap_decay02": (COLMAP["min_covisibility"], 0.2)}
N_LAYERS = 9

PROBES = 4096
PROBE_MIN_NUMEL = 65536         # smaller tensors are committed whole

DESC_STAGES = ("desc_norm", "f4", "f8", "f16", "f32", "head_stem", "head_fine", "head_x",
               "features")


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(url, sha, cache):
    cache.mkdir(parents=True, exist_ok=True)
    path = cache / url.rsplit("/", 1)[1]
    if not path.exists():
        tmp = path.with_suffix(path.suffix + ".part")
        print(f"fetching {url}", file=sys.stderr)
        urllib.request.urlretrieve(url, tmp)
        tmp.rename(path)
    got = sha256(path)
    if got != sha:
        sys.exit(f"{path}: sha256 {got}, expected {sha}")
    return path


def weights_dir(a):
    return pathlib.Path(a.weights_dir or os.environ.get("FLEXGLUE_WEIGHTS")
                        or pathlib.Path.home() / ".cache" / "flexglue")


def checkpoint(a):
    return fetch(RELEASE + CHECKPOINT[0], CHECKPOINT[1], weights_dir(a))


# ---------------------------------------------------------------------------
# Inputs


def read_rgb(path):
    """Decoded RGB uint8 [H, W, 3]. A .png fixture is these exact bytes."""
    import cv2
    bgr = cv2.imread(str(path), cv2.IMREAD_COLOR)
    if bgr is None:
        sys.exit(f"{path}: cannot read")
    return np.ascontiguousarray(bgr[..., ::-1])


def sift_keypoints(rgb, n):
    """OpenCV SIFT locations, deduplicated, strongest first, corner origin."""
    import cv2
    gray = cv2.cvtColor(rgb, cv2.COLOR_RGB2GRAY)
    kps = cv2.SIFT_create(nfeatures=n).detect(gray, None)
    kps = sorted(kps, key=lambda k: -k.response)
    pts = np.array([k.pt for k in kps], np.float32).reshape(-1, 2)
    _, first = np.unique(pts, axis=0, return_index=True)
    return (pts[np.sort(first)][:n] + 0.5).astype(np.float32)


def load_spec(path):
    spec = json.loads(pathlib.Path(path).read_text())
    base = pathlib.Path(path).resolve().parent
    for im in spec["images"].values():
        if "path" in im:
            im["path"] = str((base / im["path"]).resolve())
    return spec


def resolve_image(name, im, cache):
    if "url" in im:
        return read_rgb(fetch(im["url"], im["sha256"], cache / "images"))
    return read_rgb(im["path"])


def border_keypoints(w, h):
    """32 points 0.5-3.5 px (512 frame) inside every edge and corner, in original pixels.

    The outer half-cell (< 4 px) is the only place grid_sample's border padding
    differs from zero padding, and no detector puts keypoints there.
    """
    along = np.linspace(16, SIZE - 16, 6)
    dist = np.array([0.5, 1.5, 2.5, 3.5, 1.0, 3.0])
    pts = []
    for t, d in zip(along, dist):
        pts += [(d, t), (SIZE - d, t), (t, d), (t, SIZE - d)]
    for d in (0.5, 3.0):
        pts += [(d, d), (SIZE - d, d), (d, SIZE - d), (SIZE - d, SIZE - d)]
    return (np.array(pts, np.float64) * [w / SIZE, h / SIZE]).astype(np.float32)


def resolve_keypoints(im, rgb, n_default):
    if "keypoints" in im:
        k = np.load(im["keypoints"]).astype(np.float32)
        k = k[: im.get("max_keypoints", len(k))]
    else:
        k = sift_keypoints(rgb, im.get("max_keypoints", n_default))
    if im.get("border_keypoints"):
        k = np.concatenate([k, border_keypoints(rgb.shape[1], rgb.shape[0])])
    return k


# ---------------------------------------------------------------------------
# Model


MUTANTS = ("no_qk_norm", "no_offset", "argmax_readout", "zero_padding", "backbone_ln_eps",
           "unshuffle_order", "check_every_layer", "fp16")


def build(a, device, mp=None):
    import torch
    from flexglue import FlexGlue
    torch.set_grad_enabled(False)
    mutant = getattr(a, "mutant", None)
    if mutant == "fp16":
        mp = "float16"
    m = FlexGlue(weights=str(checkpoint(a)), mp=mp, exact_nn=True).eval().to(device)
    for head in m.heads:
        head.query_chunk_size = None    # as onnx/export.py: every keypoint in one call
    if mutant:
        apply_mutant(m, mutant)
    return m


def apply_mutant(m, mutant):
    """A deliberately wrong model, for proving compare.py can fail (README)."""
    import torch
    import torch.nn.functional as F
    from flexglue.utils import ops
    global CHECK_LAYERS
    if mutant == "no_qk_norm":
        for b in m.transformers:
            b.self_attn.q_norm = b.self_attn.k_norm = torch.nn.Identity()
            b.cross_attn.qk_norm = torch.nn.Identity()
    elif mutant == "no_offset":
        for h in m.heads:
            h.offset_head.forward = lambda fq, fm, hw: torch.zeros(*fq.shape[:-1], 2,
                                                                   device=fq.device)
    elif mutant == "argmax_readout":
        def readout(logits, grid, hw, radius=1, k=512):
            idx = logits.argmax(-1)
            return grid[idx], torch.ones_like(idx, dtype=logits.dtype)
        ops.entmax15_window_readout = readout
    elif mutant == "zero_padding":
        def sample(features, pts):
            out = F.grid_sample(features, pts[:, None], "bilinear", padding_mode="zeros",
                                align_corners=False)
            return out[:, :, 0].transpose(1, 2)
        ops.sample_features = sample
    elif mutant == "backbone_ln_eps":
        for mod in m.descriptor.backbone.modules():
            if isinstance(mod, torch.nn.LayerNorm):
                mod.eps = 1e-5
    elif mutant == "unshuffle_order":
        orig = F.pixel_unshuffle
        F.pixel_unshuffle = lambda x, r: orig(x.transpose(-1, -2), r).transpose(-1, -2)
    elif mutant == "check_every_layer":
        CHECK_LAYERS = tuple(range(1, N_LAYERS))
    elif mutant != "fp16":
        sys.exit(f"unknown mutant {mutant}")


def to_hwc(t):
    """[1, C, H, W] torch -> [H, W, C] float32 numpy."""
    return t[0].permute(1, 2, 0).float().cpu().numpy().copy()


def np32(t):
    return t.float().cpu().numpy().copy()


def preprocess(rgb, device, noise=0.0, seed=0):
    """upstream load_image's /255, then DINOv3Descriptor's (h, w) squash."""
    import torch
    from flexglue.utils import preprocess as pp
    img = pp.numpy_image_to_torch(rgb)[None].to(device)
    x = pp.resize_images(img, (SIZE, SIZE), 32)
    if noise:
        g = torch.Generator(device="cpu").manual_seed(seed)
        x = x + (torch.randn(x.shape, generator=g) * noise).to(device)
    return x


def describe(m, x512):
    """Every descriptor stage of one image; asserts they compose to encode()."""
    import torch
    import torch.nn.functional as F
    d = m.descriptor
    out = {"in512": to_hwc(x512)}
    x = (x512 - d.mean) / d.std
    out["desc_norm"] = to_hwc(x)
    feats = d.backbone(x)
    for name, f in zip(("f4", "f8", "f16", "f32"), feats):
        out[name] = to_hwc(f)
    head = d.head
    fine, *coarse = feats
    h, w = coarse[0].shape[-2:]
    stem = head.stem(fine)
    out["head_stem"] = to_hwc(stem)
    y = head.fine_proj(F.pixel_unshuffle(stem, 2))
    out["head_fine"] = to_hwc(y)
    for norm, proj, c in zip(head.norms, head.projs, coarse):
        c = proj(norm(c))
        if c.shape[-2:] != (h, w):
            c = F.interpolate(c, (h, w), mode="bilinear", align_corners=False)
        y = y + c
    out["head_x"] = to_hwc(y)
    feat = head.out_norm(head.down(y + head.mlp(y)))
    ref = d.encode(x512)
    if not torch.equal(feat, ref):
        sys.exit("internal: the staged descriptor does not reproduce encode()")
    out["features"] = to_hwc(feat)
    return feat, out


def colmap_gate(cov_layers, nostop, min_cov, decay):
    """COLMAP's early-stop graph: checks after CHECK_LAYERS only, then the final gate."""
    for k in CHECK_LAYERS:
        if cov_layers[k] < min_cov * (1 - decay) ** (N_LAYERS - k):
            n = nostop["matches0"].shape[0]
            return dict(matches0=np.full(n, -1, np.int64), mscores0=np.zeros(n, np.float32),
                        covisibility_score=np.float32(cov_layers[k]), stop_layer=np.int64(k))
    keep = cov_layers[N_LAYERS] >= min_cov
    return dict(matches0=np.where(keep, nostop["matches0"], -1).astype(np.int64),
                mscores0=np.where(keep, nostop["mscores0"], 0).astype(np.float32),
                covisibility_score=np.float32(cov_layers[N_LAYERS]),
                stop_layer=np.int64(N_LAYERS))


def match(m, feat0, feat1, kpts512):
    """Every matcher stage of one pair in COLMAP's frame (sizes 512 x 512)."""
    import torch
    import torch.nn.functional as F
    from flexglue.utils import ops
    dev = feat0.device
    out = {}
    size = torch.tensor([[float(SIZE), float(SIZE)]], device=dev)
    kp = [torch.from_numpy(k)[None].to(dev) for k in kpts512]
    for s in (0, 1):
        out[f"kpts{s}"] = kpts512[s]

    x0, x1, enc0, enc1 = m.encode_pair(feat0, feat1)
    hw = tuple(feat0.shape[-2:])
    pos = ops.cell_centers(*hw, scale=(max(hw),) * 2, device=dev)
    out["posenc_proj"] = np32(m.posenc.Wr(pos))
    out["tokens0"], out["tokens1"] = np32(x0[0]), np32(x1[0])
    cov = {}
    mp = m.conf.mp
    for k, block in enumerate(m.transformers, 1):
        with torch.autocast(dev.type, getattr(torch, mp or "float16"), enabled=mp is not None):
            x0, x1 = block(x0, x1, enc0, enc1)
        out[f"x0_layer{k}"], out[f"x1_layer{k}"] = np32(x0[0]), np32(x1[0])
        cov[k] = float(m.covisibility_score(x0, x1, k)[0])
        out[f"cov_layer{k}"] = np.float32(cov[k])
    ctx = m.pair_context(x0, x1, (hw, tuple(feat1.shape[-2:])), N_LAYERS)

    head = m.heads[N_LAYERS]
    for i in (0, 1):
        j = 1 - i
        cf_i, cf_j = ctx.direction(i)
        pts = ops.normalize_keypoints(kp[i], size)
        search = F.normalize(head.to_search(cf_j.flatten(2).mT), dim=-1)
        fq = ops.sample_features(cf_i, pts)
        q = F.normalize(head.to_search(fq), dim=-1)
        logits = q @ search.mT * head.temperature
        hwj = cf_j.shape[-2:]
        grid = ops.cell_centers(*hwj, scale=(hwj[1], hwj[0]), device=dev)
        coarse, mass = ops.entmax15_window_readout(logits, grid, hwj)
        topv, topi = logits.topk(512, dim=-1)
        tau = ops.sorted_entmax15_threshold((topv - topv[..., :1]) / 2)
        fm = ops.sample_features(cf_j, coarse)
        off = head.offset_head(fq, fm, hwj)
        p2d = coarse + off
        conf = torch.sigmoid(head.matchability(fq).squeeze(-1)) * mass
        ref_p2d, ref_conf = head(pts, cf_i, cf_j)
        if not (torch.equal(p2d, ref_p2d) and torch.equal(conf, ref_conf)):
            d = float((p2d - ref_p2d).abs().max())
            if d > 1e-5:
                sys.exit(f"internal: staged readout {i} differs from MatchHead by {d}")
        for name, t in (("search", search), ("fquery", fq), ("logits", logits),
                        ("coarse_p2d", coarse), ("mass", mass), ("fmatch", fm),
                        ("offset", off), ("p2d", p2d), ("conf", conf)):
            out[f"{name}{i}"] = np32(t[0])
        out[f"argmax{i}"] = topi[0, :, 0].cpu().numpy().astype(np.int64)
        out[f"tau{i}"] = np32(tau[0, :, 0])

    data = [dict(features=f, keypoints=k, image_size=size,
                 resized_size=m.resized_size(f, size)) for f, k in ((feat0, kp[0]), (feat1, kp[1]))]
    conf = m.resolve_conf({"min_covisibility": 0.0, "filter_threshold": COLMAP["filter_threshold"],
                           "max_distance_px": COLMAP["max_distance_px"]})
    pred = m.keypoint_correspondences(ctx, data[0], data[1], [(0, 1), (1, 0)], conf, False)
    for i in (0, 1):
        if not np.array_equal(np32(pred[f"p2d{i}_n{1 - i}"][0]), out[f"p2d{i}"]):
            sys.exit(f"internal: keypoint_correspondences p2d{i} differs from the staged one")
        out[f"valid{i}"] = pred[f"valid{i}"][0].cpu().numpy().astype(np.uint8)
    sp = m.sparse_matches(pred, data[0], data[1], conf)
    c0, c1 = size[:, None] / 2, size[:, None] / 2
    X = torch.cat([pred["p2d0_i1"] - c1, kp[0] - c0], -1)
    Y = torch.cat([kp[1] - c1, pred["p2d1_i0"] - c0], -1)
    sq = ops.sq_dist(X, Y)
    out["nn0"] = sq.argmin(2)[0].cpu().numpy().astype(np.int64)
    out["nn1"] = sq.argmin(1)[0].cpu().numpy().astype(np.int64)
    mm0, mm1 = ops.mutual_nn(sq)
    out["m0"] = mm0[0].cpu().numpy().astype(np.int64)
    out["m1"] = mm1[0].cpu().numpy().astype(np.int64)
    nostop = dict(matches0=sp["matches0"][0].cpu().numpy().astype(np.int64),
                  mscores0=np32(sp["matching_scores0"][0]))
    for k, v in nostop.items():
        out[f"nostop.{k}"] = v
    out["nostop.covisibility_score"] = np.float32(cov[N_LAYERS])
    for label, (mc, dec) in GATES.items():
        for k, v in colmap_gate(cov, nostop, mc, dec).items():
            out[f"{label}.{k}"] = v

    # Upstream end to end, as onnx/export.py traces it, must give the same answer.
    for label, mc in (("nostop", 0.0), ("colmap", COLMAP["min_covisibility"])):
        up = m.match({"features": feat0, "keypoints": kp[0], "image_size": size},
                     {"features": feat1, "keypoints": kp[1], "image_size": size}, sparse=True,
                     min_covisibility=mc, filter_threshold=COLMAP["filter_threshold"],
                     max_distance_px=COLMAP["max_distance_px"])
        want = out["nostop.matches0"] if label == "nostop" else (
            out["nostop.matches0"] if cov[N_LAYERS] >= mc else np.full_like(out["m0"], -1))
        if not np.array_equal(up["matches0"][0].cpu().numpy(), want):
            sys.exit(f"internal: upstream match() ({label}) disagrees with the staged matches")
    return out, cov


def run_pair(m, rgb, kpts_px, device, noise=0.0, seed=0, cache=None):
    """All stages of one pair: (per-image stage dicts, pair stage dict)."""
    imgs, feats = [], []
    for s in (0, 1):
        key = None if cache is None or noise else id(rgb[s])
        if key is not None and key in cache:
            f, st = cache[key]
        else:
            f, st = describe(m, preprocess(rgb[s], device, noise, seed * 2 + s))
            if key is not None:
                cache[key] = (f, st)
        imgs.append(st)
        feats.append(f)
    k512 = []
    for s in (0, 1):
        h, w = rgb[s].shape[:2]
        k512.append((kpts_px[s] * np.array([SIZE / w, SIZE / h], np.float32)).astype(np.float32))
    pair, cov = match(m, feats[0], feats[1], k512)
    for s in (0, 1):
        pair[f"kpts{s}_px"] = kpts_px[s]
        pair[f"img{s}.size"] = np.array(rgb[s].shape[1::-1], np.float32)
    return imgs, pair


def check_onnx(a, rgb, imgs, pair):
    """The same pair through COLMAP's ONNX graphs on ONNX Runtime's CPU provider."""
    import onnxruntime as ort
    wd = weights_dir(a)
    so = ort.SessionOptions()
    so.log_severity_level = 3
    desc = ort.InferenceSession(str(fetch(RELEASE + ONNX_DESCRIPTOR[0], ONNX_DESCRIPTOR[1], wd)),
                                so, providers=["CPUExecutionProvider"])
    mat = ort.InferenceSession(str(fetch(RELEASE + ONNX_MATCHER[0], ONNX_MATCHER[1], wd)),
                               so, providers=["CPUExecutionProvider"])
    res = {}
    feats = []
    for s in (0, 1):
        x = imgs[s]["in512"].transpose(2, 0, 1)[None]
        (f,) = desc.run(None, {"image": np.ascontiguousarray(x)})
        ref = imgs[s]["features"]
        res[f"features{s}_rel_l2"] = rel_l2(f[0].transpose(1, 2, 0), ref)
        feats.append(f)
    for label, mc, dec in (("nostop", 0.0, 1.0),
                           ("colmap", COLMAP["min_covisibility"], COLMAP["early_stop_decay"])):
        feeds = dict(features0=feats[0], features1=feats[1],
                     kpts0=pair["kpts0"][None], kpts1=pair["kpts1"][None],
                     image_size0=np.array([[SIZE, SIZE]], np.float32),
                     image_size1=np.array([[SIZE, SIZE]], np.float32),
                     filter_threshold=np.array(COLMAP["filter_threshold"], np.float32),
                     max_distance_px=np.array(COLMAP["max_distance_px"], np.float32),
                     min_covisibility=np.array(mc, np.float32),
                     early_stop_decay=np.array(dec, np.float32))
        m0, s0, cv = mat.run(None, feeds)
        want_m = pair[f"{label}.matches0"]
        res[f"{label}.matches0_disagree"] = int((m0[0] != want_m).sum())
        res[f"{label}.n_matches_onnx"] = int((m0[0] >= 0).sum())
        res[f"{label}.n_matches_torch"] = int((want_m >= 0).sum())
        res[f"{label}.covisibility_abs"] = abs(float(cv[0]) - float(pair[f"{label}.covisibility_score"]))
        both = (m0[0] >= 0) & (want_m >= 0) & (m0[0] == want_m)
        res[f"{label}.mscores0_max_abs_on_shared"] = (
            float(np.abs(s0[0][both] - pair[f"{label}.mscores0"][both]).max()) if both.any() else 0.0)
    return res


# ---------------------------------------------------------------------------
# Metrics shared with compare.py


def rel_l2(a, b):
    a = np.asarray(a, np.float64).ravel()
    b = np.asarray(b, np.float64).ravel()
    return float(np.linalg.norm(a - b) / max(np.linalg.norm(b), 1e-30))


def probe_indices(name, numel):
    seed = int.from_bytes(hashlib.sha256(name.encode()).digest()[:8], "little")
    rng = np.random.default_rng(seed)
    return np.sort(rng.choice(numel, size=min(PROBES, numel), replace=False)).astype(np.int32)


# ---------------------------------------------------------------------------
# Writing


def write_tensor(d, name, arr, tier, written):
    arr = np.asarray(arr)
    d.mkdir(parents=True, exist_ok=True)
    if tier == "probe" and arr.dtype.kind == "f" and arr.size >= PROBE_MIN_NUMEL:
        idx = probe_indices(name, arr.size)
        np.save(d / f"{name}.probe_idx.npy", idx)
        np.save(d / f"{name}.probe_val.npy", np.ascontiguousarray(arr.ravel()[idx]))
        stats = dict(shape=list(arr.shape), dtype=str(arr.dtype),
                     l2=float(np.linalg.norm(arr.astype(np.float64).ravel())),
                     max_abs=float(np.abs(arr).max()), mean=float(arr.astype(np.float64).mean()))
        (d / f"{name}.stats.json").write_text(json.dumps(stats, indent=1) + "\n")
        written[name] = "probe"
    else:
        np.save(d / f"{name}.npy", np.ascontiguousarray(arr))
        written[name] = "full"


def floor_draws(a, rgb, kpts, ref_imgs, ref_pair, models):
    """Torch against itself: the second device, then perturbed CPU inputs."""
    draws = []
    for label, model, dev, noise in models:
        imgs, pair = run_pair(model, rgb, kpts, dev, noise=noise, seed=len(draws) + 1)
        draws.append(dict(label=label, metrics=metrics(imgs, pair, ref_imgs, ref_pair)))
    return draws


def metrics(imgs, pair, ref_imgs, ref_pair):
    """Same metrics compare.py reads: rel L2 for floats, and for indices and masks
    compare.discrete_metric itself, so each floor is in its row's units."""
    import compare
    canon = [compare.first_copy(np.asarray(ref_pair[f"kpts{s}"])) for s in (0, 1)]
    out = {}
    for s in (0, 1):
        for st in DESC_STAGES:
            out[f"img{s}.{st}"] = rel_l2(imgs[s][st], ref_imgs[s][st])
    for name, r in ref_pair.items():
        v = pair[name]
        if np.asarray(r).dtype.kind == "f":
            out[name] = rel_l2(v, r) if np.asarray(r).size > 1 else abs(float(v) - float(r))
        else:
            out[name] = compare.discrete_metric(name, v, r, canon)[0]
        if name.startswith("p2d") or name.startswith("coarse_p2d"):
            e = np.linalg.norm((np.asarray(v, np.float64) - r) * (SIZE / 2), axis=-1)
            out[name + ":epe_p50_px"] = float(np.percentile(e, 50))
            out[name + ":epe_p99_px"] = float(np.percentile(e, 99))
            out[name + ":epe_max_px"] = float(e.max())
    return out


def write_pair(root, pname, pair_spec, imgs, pair, tier, extra):
    written_img = {}
    for s in (0, 1):
        stem = pair_spec["images"][s]
        d = root / "images" / stem
        w = {}
        for st in ("in512",) + DESC_STAGES:
            if tier == "probe" and st == "in512":
                continue
            write_tensor(d, st, imgs[s][st], tier, w)
        written_img[stem] = w
    d = root / "pairs" / pname
    w = {}
    for name, arr in pair.items():
        write_tensor(d, name, arr, tier, w)
    man = dict(format=FORMAT_VERSION, tier=tier, finished=True, exit_status=0,
               images=pair_spec["images"], files=w, image_files=written_img,
               layout=dict(maps="HWC f32 (channel-last)", tokens="[4096, C] f32, t = y*64 + x",
                           points="512-frame pixels / normalized [-1, 1], corner origin",
                           indices="int64", masks="uint8 0/1"),
               settings=dict(colmap=COLMAP, check_layers=list(CHECK_LAYERS), n_layers=N_LAYERS,
                             gates={k: dict(min_covisibility=v[0], early_stop_decay=v[1])
                                    for k, v in GATES.items()},
                             descriptor_size=SIZE, reference_precision="fp32 (mp=None)",
                             exact_nn=True),
               upstream=dict(commit=UPSTREAM_COMMIT, checkpoint=CHECKPOINT[0],
                             checkpoint_sha256=CHECKPOINT[1]),
               **extra)
    (d / "manifest.json").write_text(json.dumps(man, indent=1, default=float) + "\n")
    return man


def summary(pair):
    return dict(cov_layers=[float(pair[f"cov_layer{k}"]) for k in range(1, N_LAYERS + 1)],
                n_kpts=[int(len(pair["kpts0"])), int(len(pair["kpts1"]))],
                nostop_matches=int((pair["nostop.matches0"] >= 0).sum()),
                colmap_matches=int((pair["colmap.matches0"] >= 0).sum()),
                colmap_covisibility=float(pair["colmap.covisibility_score"]),
                colmap_stop_layer=int(pair["colmap.stop_layer"]))


# ---------------------------------------------------------------------------
# Commands


def floor_models(a):
    import torch
    models = []
    if a.floor_device != "none":
        dev = a.floor_device
        if dev == "auto":
            dev = "mps" if torch.backends.mps.is_available() else (
                "cuda" if torch.cuda.is_available() else None)
        if dev is None:
            sys.exit("no second torch device for the floor; pass --floor-device none")
        models.append(("torch, a second device", build(a, dev), dev, 0.0))
    cpu = build(a, "cpu")
    for k in range(a.floor_perturb_draws):
        models.append((f"torch cpu, input + N(0, {a.floor_perturb:g})", cpu, "cpu", a.floor_perturb))
    return models


def cmd_dump(a, spec, root, tier):
    import torch
    torch.manual_seed(0)
    m = build(a, "cpu")
    fmods = floor_models(a) if a.floor else []
    cache_img, cache_kp = {}, {}
    rgb_store = {}
    for stem, im in spec["images"].items():
        rgb_store[stem] = resolve_image(stem, im, weights_dir(a))
        cache_kp[stem] = resolve_keypoints(im, rgb_store[stem], a.max_keypoints)
        if tier == "probe":
            import cv2
            d = root / "images" / stem
            d.mkdir(parents=True, exist_ok=True)
            cv2.imwrite(str(d / "image.png"), rgb_store[stem][..., ::-1].copy(),
                        [cv2.IMWRITE_PNG_COMPRESSION, 9])
        else:
            d = root / "images" / stem
            d.mkdir(parents=True, exist_ok=True)
            np.save(d / "u8.npy", rgb_store[stem])
    report = {}
    sel = set(a.only.split(",")) if a.only else None
    for pname, ps in spec["pairs"].items():
        if sel and pname not in sel:
            continue
        rgb = [rgb_store[s] for s in ps["images"]]
        kp = [cache_kp[s] for s in ps["images"]]
        t0 = time.time()
        imgs, pair = run_pair(m, rgb, kp, "cpu", cache=cache_img)
        extra = dict(summary=summary(pair), expect=ps.get("expect"))
        if a.check_onnx:
            extra["onnx_check"] = check_onnx(a, rgb, imgs, pair)
        if fmods:
            draws = floor_draws(a, rgb, kp, imgs, pair, fmods)
            extra["floor"] = dict(n_draws=len(draws), draws=[d["label"] for d in draws],
                                  discrete="metric",
                                  worst={k: max(d["metrics"][k] for d in draws)
                                         for k in draws[0]["metrics"]})
        write_pair(root, pname, ps, imgs, pair, tier, extra)
        report[pname] = extra
        s = extra["summary"]
        print(f"{pname}: kpts {s['n_kpts']} cov9 {s['cov_layers'][-1]:.4f} "
              f"nostop {s['nostop_matches']} colmap {s['colmap_matches']} "
              f"stop@{s['colmap_stop_layer']} ({time.time() - t0:.0f}s)"
              + (f" onnx {json.dumps(extra['onnx_check'])}" if a.check_onnx else ""),
              flush=True)
    top = dict(format=FORMAT_VERSION, tier=tier, pairs=sorted(report), upstream_commit=UPSTREAM_COMMIT,
               torch=__import__("torch").__version__)
    (root / "manifest.json").write_text(json.dumps(top, indent=1) + "\n")
    return report


def cmd_time(a, spec):
    import torch
    dev = a.device

    def sync():
        if dev == "mps":
            torch.mps.synchronize()
        elif dev == "cuda":
            torch.cuda.synchronize()

    rows = []
    for mp in ([None, "float16"] if a.with_fp16 else [None]):
        m = build(a, dev, mp=mp)
        for pname, ps in spec["pairs"].items():
            rgb = [resolve_image(s, spec["images"][s], weights_dir(a)) for s in ps["images"]]
            kp = [resolve_keypoints(spec["images"][s], r, a.max_keypoints)
                  for s, r in zip(ps["images"], rgb)]
            size = torch.tensor([[float(SIZE)] * 2], device=dev)
            k512 = [torch.from_numpy(k * np.array([SIZE / r.shape[1], SIZE / r.shape[0]],
                                                   np.float32))[None].to(dev) for k, r in zip(kp, rgb)]
            x = [preprocess(r, dev) for r in rgb]
            td, tm = [], []
            for it in range(a.repeats + 1):
                sync(); t0 = time.perf_counter()
                f = [m.descriptor.encode(xi) for xi in x]
                sync(); t1 = time.perf_counter()
                p = m.match({"features": f[0], "keypoints": k512[0], "image_size": size},
                            {"features": f[1], "keypoints": k512[1], "image_size": size},
                            sparse=True, **COLMAP)
                p["matches0"].cpu()
                sync(); t2 = time.perf_counter()
                if it:
                    td.append((t1 - t0) / 2)
                    tm.append(t2 - t1)
            rows.append(dict(pair=pname, mp=mp or "fp32", kpts=[len(k) for k in kp],
                             describe_s=float(np.median(td)), match_s=float(np.median(tm)),
                             stop_layer=int(p["stop_layer"])))
            print(json.dumps(rows[-1]), flush=True)
        del m
    return rows


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("command", choices=("dump", "fixtures", "time"))
    ap.add_argument("--pairs", help="pair spec JSON (default: fixtures/pairs.json)")
    ap.add_argument("--out", help="output root (dump)")
    ap.add_argument("--only", help="comma-separated pair names")
    ap.add_argument("--max-keypoints", type=int, default=2048,
                    help="SIFT cap where the spec gives none (COLMAP caps at 8192)")
    ap.add_argument("--weights-dir", help="release files cache (env FLEXGLUE_WEIGHTS)")
    ap.add_argument("--check-onnx", action="store_true",
                    help="also run COLMAP's ONNX graphs on ONNX Runtime CPU")
    ap.add_argument("--no-floor", dest="floor", action="store_false")
    ap.add_argument("--floor-device", default="auto", help="auto | mps | cuda | none")
    ap.add_argument("--floor-perturb", type=float, default=1e-6)
    ap.add_argument("--floor-perturb-draws", type=int, default=3)
    ap.add_argument("--mutant", choices=MUTANTS, help="dump: a deliberately wrong model")
    ap.add_argument("--device", default="cpu", help="time: torch device")
    ap.add_argument("--repeats", type=int, default=3)
    ap.add_argument("--with-fp16", action="store_true", help="time: also mp=float16")
    a = ap.parse_args()
    spec = load_spec(a.pairs or FIXTURES / "pairs.json")
    if a.command == "time":
        cmd_time(a, spec)
    elif a.command == "fixtures":
        a.check_onnx = True
        cmd_dump(a, spec, FIXTURES, "probe")
    else:
        if not a.out:
            sys.exit("dump needs --out")
        cmd_dump(a, spec, pathlib.Path(a.out), "full")


if __name__ == "__main__":
    main()
