#!/usr/bin/env python3
# /// script
# requires-python = ">=3.10"
# dependencies = ["numpy"]
# ///
"""Score a FlexGlue stage dump against the reference fixtures, one row per stage.

    uv run tools/flexglue_ref/compare.py check --dump DUMP --ref ROOT --pair NAME [--json OUT]
                                               [--expect-gated N]
    uv run tools/flexglue_ref/compare.py flatten --ref ROOT --pair NAME --out DUMP

DUMP is a flat directory: one <name>.npy per stage (names in README.md) plus a
manifest.json {"finished": true, "exit_status": 0, "files": [names]}. Only the
files the manifest lists are read, and an unfinished or failed dump is refused.
ROOT is either tier of flexglue_ref.py's output, and needs no torch. Each bar is
max(fixed bar, FLOOR_FACTOR x the torch-vs-torch floor recorded beside the
reference). Exit status is 1 when any gated row misses its bar.
"""
import argparse
import json
import pathlib
import sys

import numpy as np

SIZE = 512
FLOOR_FACTOR = 2.0
N_LAYERS = 9
DESC_STAGES = ("desc_norm", "f4", "f8", "f16", "f32", "head_stem", "head_fine", "head_x",
               "features")

# Fixed bars, fp32 against the fp32 reference. A stage with no entry is reported, not gated.
REL_L2 = 1e-4                        # descriptor stages, tokens, layers, sampled features
REL_L2_READOUT = 1e-3                # values downstream of the 3x3 entmax window
EPE_P50_PX, EPE_P99_PX = 0.01, 0.1   # predicted points, 512-frame pixels
COV_ABS = 1e-4
DISCRETE_FRAC = 0.002                # indices and masks: fraction of entries that differ
REQUIRED = ("colmap.matches0", "colmap.mscores0", "colmap.covisibility_score",
            "colmap.stop_layer", "colmap_decay02.stop_layer", "colmap_decay02.matches0",
            "kpts0", "kpts1")


def rel_l2(a, b):
    a = np.asarray(a, np.float64).ravel()
    b = np.asarray(b, np.float64).ravel()
    return float(np.linalg.norm(a - b) / max(np.linalg.norm(b), 1e-30))


def first_copy(k):
    """Row -> first row with identical coordinates; None when every row is unique.

    COLMAP's SIFT emits one keypoint per dominant orientation, so ~20% of real
    keypoints share a location. Their predictions are identical, so which copy wins
    the mutual NN is an argmin tie: any copy is correct, and indices are compared
    modulo copies.
    """
    _, first, inv = np.unique(k, axis=0, return_index=True, return_inverse=True)
    if len(first) == len(k):
        return None
    return first[inv.ravel()]


def kpt_roles(name):
    """(image whose rows the tensor has, image its values index) for keypoint indices."""
    base = name.rsplit(".", 1)[-1]
    if base in ("matches0", "m0", "nn0"):
        return 0, 1
    if base in ("m1", "nn1"):
        return 1, 0
    return 0, None


def canonical_diff(name, ours, ref, c_src, c_dst):
    """Mismatch fraction with duplicate keypoints identified."""
    ours = np.rint(np.asarray(ours, np.float64)).astype(np.int64)   # dumps may carry indices as f32
    ref = np.asarray(ref).astype(np.int64)
    n_src = len(ref)
    cs = c_src if c_src is not None else np.arange(n_src)
    base = name.rsplit(".", 1)[-1]
    if base.startswith("nn"):
        keep = cs == np.arange(n_src)
        diff = c_dst[np.asarray(ours)[keep]] != c_dst[np.asarray(ref)[keep]]
        denom = max(int(keep.sum()), 1)
        return float(diff.sum()) / denom, "fraction differing, mod copies", denom
    pairs = [{(int(cs[i]), int(c_dst[v])) for i, v in enumerate(np.asarray(m)) if v >= 0}
             for m in (ours, ref)]
    denom = max(len(pairs[1]), 1)
    return float(len(pairs[0] ^ pairs[1])) / denom, "pairs differing, mod copies", denom


def discrete_metric(name, ours, ref, canon):
    """(value, metric, denominator) of an index or mask stage.

    The one definition both sides use: compare.py scores a dump with it, and
    flexglue_ref.py records the torch-vs-torch floor with it, so a floor is in
    the units of the value it bounds. `canon` is first_copy() of both images'
    keypoints; indices are compared modulo duplicate keypoints when the image
    they index has any.
    """
    src, dst = kpt_roles(name)
    if dst is not None and canon[dst] is not None:
        return canonical_diff(name, ours, ref, canon[src], canon[dst])
    ours = np.rint(np.asarray(ours, np.float64)).astype(np.int64)
    ref = np.rint(np.asarray(ref, np.float64)).astype(np.int64)
    n = max(int(ref.size), 1)
    return float((ours.ravel() != ref.ravel()).sum()) / n, "fraction differing", n


def discrete_bar(ref, name, metric_name, n):
    """(bar, floor in the metric's units or None) for a discrete stage.

    A reference written with discrete floors in metric units says so
    (floor.discrete == "metric"). An older one recorded a raw count of differing
    rows: that converts exactly on the plain path, whose denominator is the row
    count, and not at all once copies are identified, so there it is not used.
    """
    fl = ref.floor.get(name)
    if fl is None:
        return DISCRETE_FRAC, None
    if ref.floor_units != "metric":
        if metric_name != "fraction differing":
            return DISCRETE_FRAC, None
        fl = fl / n
    return max(DISCRETE_FRAC, FLOOR_FACTOR * fl), fl


def scalar(x):
    """Scalars are stored as shape [1] (np.ascontiguousarray promotes 0-d); accept either."""
    x = np.asarray(x)
    if x.size != 1:
        raise ValueError(f"expected one value, got shape {x.shape}")
    return float(x.ravel()[0])


class Ref:
    """One pair of a reference tree, full or probe tier."""

    def __init__(self, root, pair):
        self.root = pathlib.Path(root)
        self.pdir = self.root / "pairs" / pair
        mp = self.pdir / "manifest.json"
        if not mp.exists():
            sys.exit(f"{mp}: no such reference pair")
        self.man = json.loads(mp.read_text())
        if not self.man.get("finished") or self.man.get("exit_status") != 0:
            sys.exit(f"{mp}: the reference run did not finish cleanly")
        self.floor = (self.man.get("floor") or {}).get("worst", {})
        self.floor_units = (self.man.get("floor") or {}).get("discrete", "rows")
        self.entries = {}
        for name, tier in self.man["files"].items():
            self.entries[name] = (self.pdir, name, tier)
        for s, stem in enumerate(self.man["images"]):
            for name, tier in self.man["image_files"][stem].items():
                self.entries[f"img{s}.{name}"] = (self.root / "images" / stem, name, tier)

    def names(self):
        return sorted(self.entries)

    def get(self, name):
        """(array or probe values, probe indices or None, full shape)."""
        d, stem, tier = self.entries[name]
        if tier == "full":
            a = np.load(d / f"{stem}.npy")
            return a, None, a.shape
        st = json.loads((d / f"{stem}.stats.json").read_text())
        return (np.load(d / f"{stem}.probe_val.npy"), np.load(d / f"{stem}.probe_idx.npy"),
                tuple(st["shape"]))


def load_dump(path):
    path = pathlib.Path(path)
    mp = path / "manifest.json"
    if not mp.exists():
        sys.exit(f"{path}: no manifest.json")
    m = json.loads(mp.read_text())
    if not m.get("finished"):
        sys.exit(f"{path}: the run that wrote it never finished")
    if m.get("exit_status") != 0:
        sys.exit(f"{path}: the run that wrote it exited {m.get('exit_status')}")
    files = set(m["files"])
    return {n: path / f"{n}.npy" for n in files if (path / f"{n}.npy").exists()}, m


def floor_bar(ref, key, fixed):
    fl = ref.floor.get(key)
    return fixed if fl is None else max(fixed, FLOOR_FACTOR * fl), fl


def classify(name):
    base = name.split(".", 1)[1] if name.startswith(("img0.", "img1.")) else name
    if base in DESC_STAGES or base in ("in512",) or name.startswith(("tokens", "x0_", "x1_")) \
            or base.startswith(("search", "fquery", "fmatch", "logits", "posenc")):
        return "dense"
    if base.startswith(("coarse_p2d", "p2d")):
        return "points"
    if base.startswith(("cov_layer",)) or base.endswith("covisibility_score"):
        return "scalar"
    if base.startswith(("mass", "offset", "conf", "tau")) or base.endswith("mscores0"):
        return "readout"
    if base.endswith("stop_layer"):
        return "exact"
    if base.startswith(("argmax", "nn0", "nn1", "m0", "m1", "valid")) or base.endswith("matches0"):
        return "discrete"
    if base.startswith(("kpts", "size")) or base.endswith(".size") or base.endswith("u8"):
        return "input"
    return "other"


def check(a):
    ref = Ref(a.ref, a.pair)
    dump, dman = load_dump(a.dump)
    rows = []

    def row(stage, metric, value, bar, floor=None):
        ok = None if bar is None else bool(np.isfinite(value) and value <= bar)
        rows.append(dict(stage=stage, metric=metric, value=float(value),
                         bar=None if bar is None else float(bar), floor=floor, ok=ok))

    for name in REQUIRED:
        if name not in dump:
            rows.append(dict(stage=name, metric="missing", value=float("nan"), bar=0.0,
                             floor=None, ok=False))
    for name in ("kpts0", "kpts1"):
        if name in dump:
            ours, r = np.load(dump[name]), np.load(ref.pdir / f"{name}.npy")
            same = ours.shape == r.shape and np.array_equal(ours, r)
            if not same:
                sys.exit(f"{name}: the dump ran on different keypoints than the reference "
                         f"({ours.shape} vs {r.shape}); the scores below would mean nothing")
            if len(r) == 0:
                sys.exit(f"{name}: zero keypoints in the reference")

    canon = [first_copy(np.load(ref.pdir / f"kpts{s}.npy")) for s in (0, 1)]
    for name in ref.names():
        if name not in dump:
            continue
        kind = classify(name)
        ours = np.load(dump[name])
        rv, idx, shape = ref.get(name)
        if tuple(ours.shape) != tuple(shape):
            row(name, f"shape {tuple(ours.shape)} != {tuple(shape)}", float("inf"), 0.0)
            continue
        if ours.dtype.kind == "f" and not np.all(np.isfinite(ours)):
            row(name, "non-finite entries", float((~np.isfinite(ours)).sum()), 0.0)
            continue
        ov = ours.ravel()[idx] if idx is not None else ours
        if kind == "dense":
            bar, fl = floor_bar(ref, name, REL_L2)
            row(name, "rel L2" + (" (probes)" if idx is not None else ""), rel_l2(ov, rv), bar, fl)
        elif kind == "readout":
            bar, fl = floor_bar(ref, name, REL_L2_READOUT)
            row(name, "rel L2", rel_l2(ov, rv), bar, fl)
        elif kind == "scalar":
            bar, fl = floor_bar(ref, name, COV_ABS)
            row(name, "abs", abs(scalar(ov) - scalar(rv)), bar, fl)
        elif kind == "points":
            e = np.linalg.norm((np.asarray(ov, np.float64) - rv) * (SIZE / 2), axis=-1)
            for q, fixed in ((50, EPE_P50_PX), (99, EPE_P99_PX)):
                bar, fl = floor_bar(ref, f"{name}:epe_p{q}_px", fixed)
                row(name, f"EPE p{q} px", float(np.percentile(e, q)), bar, fl)
            row(name, "EPE max px", float(e.max()), None)
        elif kind == "discrete":
            value, metric, n = discrete_metric(name, ov, rv, canon)
            bar, fl = discrete_bar(ref, name, metric, n)
            row(name, metric, value, bar, fl)
        elif kind == "exact":
            row(name, f"ours {int(scalar(ov))} ref {int(scalar(rv))}",
                float(int(scalar(ov)) != int(scalar(rv))), 0.0)
        elif kind == "input":
            row(name, "max abs", float(np.abs(np.asarray(ov, np.float64) - rv).max()), None)
        else:
            row(name, "rel L2", rel_l2(ov, rv), None)

    cm = np.load(dump["colmap.matches0"]) if "colmap.matches0" in dump else None
    if cm is not None:
        n_ours = int((cm >= 0).sum())
        n_ref = int((np.load(ref.pdir / "colmap.matches0.npy") >= 0).sum())
        row("colmap.matches0", f"count ours {n_ours} ref {n_ref}", 0.0, None)

    gated = [r for r in rows if r["ok"] is not None]
    bad = [r for r in gated if not r["ok"]]
    unscored = sorted(set(ref.names()) - set(dump))
    if a.expect_gated is not None and len(gated) != a.expect_gated:
        rows.append(dict(stage="harness", metric=f"{len(gated)} gated rows, expected "
                         f"{a.expect_gated}", value=float("inf"), bar=0.0, floor=None, ok=False))
        gated.append(rows[-1])
        bad.append(rows[-1])
    print(f"pair {a.pair}: {len(rows)} rows, {len(gated)} gated, {len(bad)} failed; "
          f"{len(unscored)} reference stages not in the dump")
    for r in rows:
        flag = "" if r["ok"] is None else ("ok" if r["ok"] else "FAIL")
        bar = "" if r["bar"] is None else f"<= {r['bar']:.3g}"
        fl = "" if r["floor"] is None else f"  (floor {r['floor']:.3g})"
        print(f"  {r['stage']:<28} {r['metric']:<26} {r['value']:.3e} {bar:<11} {flag}{fl}")
    if a.json:
        pathlib.Path(a.json).write_text(json.dumps(dict(pair=a.pair, rows=rows, failed=len(bad),
                                                        unscored=unscored), indent=1))
    return 1 if bad else 0


def flatten(a):
    """A full-tier reference pair rewritten as the flat dump layout the C++ side writes."""
    ref = Ref(a.ref, a.pair)
    out = pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    names = []
    for name in ref.names():
        arr, idx, _ = ref.get(name)
        if idx is not None:
            sys.exit(f"{name}: a probe-tier reference cannot be flattened")
        np.save(out / f"{name}.npy", arr)
        names.append(name)
    (out / "manifest.json").write_text(json.dumps(dict(finished=True, exit_status=0,
                                                       files=names)) + "\n")
    print(f"{out}: {len(names)} stages")
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("command", choices=("check", "flatten"))
    ap.add_argument("--dump")
    ap.add_argument("--ref", default=str(pathlib.Path(__file__).resolve().parent / "fixtures"))
    ap.add_argument("--pair", required=True)
    ap.add_argument("--out")
    ap.add_argument("--json")
    ap.add_argument("--expect-gated", type=int)
    a = ap.parse_args()
    if a.command == "flatten":
        if not a.out:
            sys.exit("flatten needs --out")
        return flatten(a)
    if not a.dump:
        sys.exit("check needs --dump")
    return check(a)


if __name__ == "__main__":
    sys.exit(main())
