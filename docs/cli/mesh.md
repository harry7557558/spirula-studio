# `spirula mesh`

Extracts a surface mesh from a trained model.

```text
spirula mesh <checkpoint> [--flag value ...]
```

`<checkpoint>` is a run folder (with `config.json` and a `step-*.ckpt/`), a
`*.ckpt` folder, or a `splat.ply`. The dataset's cameras are used for
occupancy carving and colour unless `--no-data`.

Flag names accept `-` or `_` between words and may start with one or two
dashes. Every flag except `--no-data`, `--no-cull-unseen` and `--quiet` takes
a value.

## Input and output

| flag | default | meaning |
|---|---|---|
| `--data <dir>` | `data` in the run's `config.json` | Dataset for camera-based occupancy and colours. |
| `--no-data` | | Mesh from the Gaussian densities alone. |
| `--data-format <fmt>` | detected | `colmap`, `nerfstudio` or `metashape`. |
| `--output <path>` | `<checkpoint>/mesh` | Output base path; a known extension is stripped. One file per format is written next to it. |
| `--device <sel>` | `auto` | Vulkan: an index, part of a name, `auto` or `uuid:<32 hex digits>`. CUDA: a device index. |
| `--format <list>` | `ply` | Comma list of `ply`, `obj`, `gltf`, `glb`, `stl`. With `--color texture` a format may carry a texture encoding: `glb+png` (default), `glb+jpg` (JPEG q95), `glb+jpeg75` (JPEG q75). |
| `--color <list>` | `vertex` | Comma list of `none`, `vertex`, `texture`. A pair no format can carry, such as PLY with a texture, is skipped. |
| `--texture-size <n>` | 0 | Texture atlas resolution; 0 takes it from the observed detail. |
| `--tex-gutter-px <n>` | 4 | Spacing between UV charts in the atlas, in texels. |
| `--chart-angle-deg <a>` | 60 | Largest normal deviation within one UV chart. |
| `--quiet` | | Less progress output. |

## Surface extraction

| flag | default | meaning |
|---|---|---|
| `--iso <v>` | 0.5 with cameras, 0.2 without | Isosurface level. |
| `--merge-factor <v>` | 1 | Short-edge merge threshold multiplier; 0 or less turns merging off. |
| `--bisection-iters <n>` | 3 | Bisection steps per cut edge. |
| `--max-cameras <n>` | -1 | Cap on the cameras used; -1 uses all of them. |
| `--max-grid-res <n>` | 512 | Per-axis cap on the acceleration grid. |
| `--grid-cell-factor <v>` | 2 | Grid cell size, as a multiple of the mean Gaussian radius. |
| `--num-threads <n>` | 0 | Host threads; 0 uses every hardware thread. |
| `--carve-k <n>` | 1 | Occupancy is the k-th smallest over the cameras. |
| `--cull-unseen <0\|1>` | 1 | Drop mesh vertices no camera saw. `--no-cull-unseen` is `--cull-unseen 0`. |
| `--merge-max-flip-deg <a>` | 60 | Fold guard for merge collapses. |

## Clean-up

| flag | default | meaning |
|---|---|---|
| `--floater-min-faces <n>` | 10 | Drop connected components with fewer faces. |
| `--fill-hole-ratio <v>` | 0.05 | Fill hole loops smaller than this fraction of their component. |
| `--fill-hole-max-edges <n>` | 20 | Always fill loops with at most this many edges. |
| `--degenerate-angle-deg <a>` | 15 | Repair triangles with angles below this. |
| `--quality-iters <n>` | 3 | Valence-flip and tangential-relaxation iterations. |

`--help` prints the meshing library's defaults for `--floater-min-faces`,
`--fill-hole-max-edges` and `--degenerate-angle-deg` (100, 12 and 2); the
values above are what the command actually starts from.

Exit status is 0 on success and 1 on any error.
