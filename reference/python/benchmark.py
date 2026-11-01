#!/usr/bin/env python3
"""Multi-scene benchmark driver.

Each scene runs as its own `spirula train` process, so the engine (world splats,
bilagrid/PPISP/background, optimizer moments, colour-space matrices, device
pool) starts clean. Several scenes in one process leak state between them and
silently degrade the later ones.

Each run writes its own metrics.json -- l1/psnr/ssim, LPIPS where the build
has the inference layer, their colour-corrected `cc_` variants,
`training_time` and `engine_vram`, all native. This script reads those back
and prints a table. With --lpips it also runs eval_lpips.py per scene, for a
build without native LPIPS; that needs torch + torchmetrics, and without it
this script needs nothing beyond the standard library.

    python3 reference/python/benchmark.py 360_v2 --data /path/to/360_v2
    python3 reference/python/benchmark.py zipnerf --data /path/to/zipnerf \\
        --preset academic-baseline --lpips

Extra arguments after `--` are appended to every `spirula train` command:

    python3 reference/python/benchmark.py 360_v2 --data ... -- --cap-max 3000000
"""

from __future__ import annotations

import argparse
import datetime
import json
import shutil
import subprocess
import sys
from pathlib import Path

# (scene, downscale factor). The rounding mode differs per scene because the
# published images_N folders were produced with different rounding.
BENCHMARKS: dict[str, dict] = {
    "360_v2": {
        "help": "7 permissively released scenes from Mip-NeRF 360",
        "url": "http://storage.googleapis.com/gresearch/refraw360/360_v2.zip",
        "scenes": [("bicycle", 4), ("garden", 4), ("stump", 4), ("bonsai", 2),
                   ("counter", 2), ("kitchen", 2), ("room", 2)],
        # garden's images_4 was rounded, the rest ceiled.
        "rounding": lambda scene: "round" if scene == "garden" else "ceil",
        "extra": lambda scene: [],
    },
    "tandt_db": {
        "help": "Truck and DrJohnson from the 3DGS benchmark",
        "scenes": [("tandt/truck", 1), ("db/drjohnson", 1)],
        "rounding": lambda scene: "floor",
        "extra": lambda scene: [],
    },
    "zipnerf": {
        "help": "4 fisheye scenes from Zip-NeRF",
        "url": "https://smerf-3d.github.io/#data",
        "scenes": [("alameda", 4), ("berlin", 4), ("london", 4), ("nyc", 4)],
        "rounding": lambda scene: "ceil",
        "extra": lambda scene: [],
    },
}


def find_trainer() -> str:
    exe = shutil.which("spirula")
    if exe:
        return exe
    local = Path(__file__).resolve().parents[2] / "build" / "spirula"
    if local.is_file():
        return str(local)
    raise SystemExit("`spirula` not found on PATH or in ./build -- build it first")


def run_scene(trainer: str, preset: str, data_dir: Path, out_prefix: Path,
              out_name: str, downscale: int, rounding: str,
              extra: list[str], want_lpips: bool) -> dict | None:
    metrics_path = out_prefix / out_name / "metrics.json"
    # A stale file from a previous run with the same name would masquerade as
    # this run's result.
    metrics_path.unlink(missing_ok=True)

    cmd = [
        trainer, "train", preset,
        "--data", str(data_dir),
        "--data-format", "colmap",
        "--colmap-recon-dir", "sparse/0",
        "--train-resolution-divisor", "1",
        "--image-dir", f"images_{downscale}" if downscale > 1 else "images",
        "--downscale-rounding-mode", rounding,
        "--eval-mode", "interval",
        "--save-eval-images", "1",
        "--load-depths", "0",
        "--load-normals", "0",
        "--steps-per-save", "0",
        "--disable-viewer", "1",
        "--keep-viewer-alive", "0",
        "--output-dir-prefix", str(out_prefix),
        "--output-dir-name", out_name,
    ] + extra

    print(">>>", " ".join(cmd), flush=True)
    if subprocess.call(cmd) != 0:
        print(f"!!! {out_name} exited non-zero", flush=True)
        return None
    if not metrics_path.exists():
        print(f"!!! {out_name} wrote no metrics.json", flush=True)
        return None

    if want_lpips:
        lpips = Path(__file__).with_name("eval_lpips.py")
        rc = subprocess.call([sys.executable, str(lpips),
                              str(out_prefix / out_name), "--write"])
        if rc != 0:
            print(f"!!! LPIPS failed for {out_name}; metrics are still valid "
                  f"without it", flush=True)

    return json.loads(metrics_path.read_text())


def print_table(results: list[tuple[str, dict | None]]) -> None:
    ok = [(name, m) for name, m in results if m is not None]
    failed = [name for name, m in results if m is None]
    if not ok:
        print("\nEvery scene failed.")
        return

    # Scalar keys only; avg_* is what a scene summarizes to.
    keys: list[str] = []
    for _, m in ok:
        for k, v in m.items():
            if isinstance(v, list) or k in keys:
                continue
            keys.append(k)

    w = max(len(k) for k in keys) + 2
    print("\n" + "metric".ljust(w) + "".join(n[:14].rjust(16) for n, _ in ok))
    for k in keys:
        row = k[len("avg_"):] if k.startswith("avg_") else k
        line = row.ljust(w)
        for _, m in ok:
            v = m.get(k)
            line += ("" if v is None else f"{v:.5g}" if isinstance(v, float)
                     else str(v)).rjust(16)
        print(line)
    if failed:
        print("\nfailed:", ", ".join(failed))


def main() -> None:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("benchmark", choices=sorted(BENCHMARKS))
    ap.add_argument("--data", type=Path, required=True,
                    help="folder holding the benchmark's scenes")
    ap.add_argument("--trainer", type=Path, help="trainer executable")
    ap.add_argument("--scenes", nargs="+", help="run only these scene names")
    ap.add_argument("--preset", default="academic-baseline",
                    help="spirula train preset (default: academic-baseline)")
    ap.add_argument("--output-dir-prefix", type=Path, default=Path("outputs"))
    ap.add_argument("--lpips", action="store_true",
                    help="also run eval_lpips.py per scene (needs torchmetrics)")
    argv = sys.argv[1:]
    split = argv.index("--") if "--" in argv else len(argv)
    args = ap.parse_args(argv[:split])
    args.rest = argv[split + 1:]

    spec = BENCHMARKS[args.benchmark]
    if not args.data.is_dir():
        raise SystemExit(f"dataset not found at {args.data}")

    trainer = str(args.trainer.resolve()) if args.trainer else find_trainer()
    tag = (f"{args.benchmark}-{args.preset}-"
           f"{datetime.datetime.now().strftime('%Y%m%d-%H%M%S')}")

    results = []
    selected = [(scene, scale) for scene, scale in spec["scenes"]
                if not args.scenes or scene in args.scenes or Path(scene).name in args.scenes]
    if not selected:
        raise SystemExit("no matching scenes")
    for scene, downscale in selected:
        print(f"\n=== {scene} ===", flush=True)
        results.append((scene, run_scene(
            trainer, args.preset, args.data / scene, args.output_dir_prefix,
            f"benchmark-{tag}-{scene.replace('/', '-')}", downscale, spec["rounding"](scene),
            spec["extra"](scene) + list(args.rest), args.lpips)))

    print_table(results)


if __name__ == "__main__":
    main()
