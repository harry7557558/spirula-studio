#!/usr/bin/env python3
"""Score a mesh against a reference point cloud: precision, recall and F.

Both sides are sampled (the mesh by area, the cloud uniformly); a point counts
when its nearest neighbour on the other side is within the tolerance, a
fraction of the diagonal of the cloud's 5-95 percentile box. Precision is the
mesh share within it, recall the cloud share. Repeated runs of one training
setting moved F by ~0.02 (docs/notes/2dgs.md, "Preset weights").

    python3 reference/python/mesh_fscore.py <mesh.ply> <cloud.ply> [--samples N] [--tol F]

Both PLYs must be binary little-endian and in the same frame, as `spirula mesh`
and `spirula dense` write them. Needs numpy and scipy.
"""
from __future__ import annotations

import argparse

import numpy as np
from scipy.spatial import cKDTree

TYPES = {"char": "i1", "int8": "i1", "uchar": "u1", "uint8": "u1",
         "short": "<i2", "int16": "<i2", "ushort": "<u2", "uint16": "<u2",
         "int": "<i4", "int32": "<i4", "uint": "<u4", "uint32": "<u4",
         "float": "<f4", "float32": "<f4", "double": "<f8", "float64": "<f8"}


def read_ply(path: str) -> dict[str, np.ndarray]:
    with open(path, "rb") as f:
        header = []
        while True:
            line = f.readline().decode("ascii", "replace").strip()
            header.append(line)
            if line == "end_header":
                break
        if not any(l.startswith("format binary_little_endian") for l in header):
            raise SystemExit(f"{path}: only binary little-endian PLY is read")
        elements = []
        for line in header:
            t = line.split()
            if t and t[0] == "element":
                elements.append((t[1], int(t[2]), []))
            elif t and t[0] == "property":
                elements[-1][2].append(t[1:])
        out = {}
        for name, n, props in elements:
            if props and props[0][0] == "list":
                dt = np.dtype([("n", TYPES[props[0][1]]), ("i", TYPES[props[0][2]], 3)])
                rows = np.fromfile(f, dtype=dt, count=n)
                if (rows["n"] != 3).any():
                    raise SystemExit(f"{path}: faces must be triangles")
                out[name] = rows["i"]
            else:
                rows = np.fromfile(f, dtype=np.dtype([(p[1], TYPES[p[0]]) for p in props]), count=n)
                out[name] = np.stack([rows[k] for k in "xyz"], 1).astype(np.float64)
    return out


def sample_triangles(v: np.ndarray, faces: np.ndarray, n: int, rng) -> np.ndarray:
    a, b, c = v[faces[:, 0]], v[faces[:, 1]], v[faces[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    pick = rng.choice(len(faces), size=n, p=area / area.sum())
    s, r = np.sqrt(rng.random(n)), rng.random(n)
    return ((1 - s)[:, None] * a[pick] + (s * (1 - r))[:, None] * b[pick]
            + (s * r)[:, None] * c[pick])


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("mesh")
    ap.add_argument("cloud")
    ap.add_argument("--samples", type=int, default=1_000_000, help="points drawn from each side")
    ap.add_argument("--tol", type=float, default=0.005,
                    help="tolerance as a fraction of the cloud's 5-95%% diagonal")
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    mesh = read_ply(args.mesh)
    if "face" not in mesh:
        raise SystemExit(f"{args.mesh}: no faces")
    cloud = read_ply(args.cloud)["vertex"]
    if len(cloud) > args.samples:
        cloud = cloud[rng.choice(len(cloud), args.samples, replace=False)]
    lo, hi = np.percentile(cloud, 5, axis=0), np.percentile(cloud, 95, axis=0)
    tol = args.tol * float(np.linalg.norm(hi - lo))

    points = sample_triangles(mesh["vertex"], mesh["face"], args.samples, rng)
    d_mesh, _ = cKDTree(cloud).query(points, k=1, workers=-1)
    d_cloud, _ = cKDTree(points).query(cloud, k=1, workers=-1)
    p, r = float((d_mesh < tol).mean()), float((d_cloud < tol).mean())
    f = 2 * p * r / (p + r) if p + r > 0 else 0.0
    print(f"tolerance {tol:.4g}  precision {p:.3f}  recall {r:.3f}  F {f:.3f}")


if __name__ == "__main__":
    main()
