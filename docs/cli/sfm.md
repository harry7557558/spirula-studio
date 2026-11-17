# `spirula sfm`

Structure from motion: photos in, a COLMAP sparse model out. Vulkan builds
with `SS_BUILD_SFM=ON` only. Each subcommand is one stage and reads and writes
COLMAP-compatible files, so any stage can be swapped for COLMAP's own; `auto`
runs them all. Background is in `src/sfm/README.md`.

```text
spirula sfm <command> [options]
spirula sfm <command> --help        (or: spirula sfm help <command>)
spirula sfm --help | -h | help
spirula sfm --version | -V | version
```

| command | does |
|---|---|
| `auto` | reconstruct a sparse model from a directory of images, in one command |
| `extract` | detect features in an image or a directory of images |
| `match` | match and geometrically verify a directory of feature files |
| `map` | incremental reconstruction: matches to a COLMAP sparse model |
| `merge` | fuse the models of a fragmented capture into fewer |
| `ba` | bundle-adjust a sparse model |

Most flags come from one table and are listed, with the commands that accept
each, in [sfm-flags.md](sfm-flags.md). The rest, parsed by each command
itself, are below. Rules that hold for all of them:

- `--flag=value` and `--flag value` are the same, for every flag.
- In a table flag's name `-` and `_` are interchangeable.
- A table switch takes no value: `--name` turns it on, `--no-name` off.
- A flag given on the command line always beats a preset or a manifest.

## `auto`

```text
spirula sfm auto [IMAGE_DIR|DATASET_DIR] -o WORKSPACE [options]
```

Runs extract, match, map and merge and writes `WORKSPACE/features/`,
`WORKSPACE/matches.bin` and `WORKSPACE/sparse/0` (plus `sparse/1`, ... when the
capture falls apart into several models). The positional defaults to
`images`; a dataset folder holding `images/` is accepted, and a `masks/`
beside the images is used automatically.

| flag | meaning |
|---|---|
| `-o`, `--output DIR` | Required. The workspace to write. |
| `--manifest FILE` | Read the capture's description from a YAML or JSON file: where the images and masks are, how the cameras are grouped, each group's lens and focal length, the rigs, and the videos whose IMU and GPS cover them. Command-line flags beat it. |
| `--rig [KIND=]PREFIX,PREFIX,...` | Images under these path prefixes with the same file name are one rig frame with a fixed relative pose (`cam0,cam1` pairs `cam0/x.jpg` with `cam1/x.jpg`). `dual-fisheye=` marks the first two as the back-to-back lenses of one 360 camera. Repeatable. |
| `--sequence PREFIX[,PREFIX...]` | Images under these prefixes were taken in file-name order (video frames, a walk). Neighbours are matched and trusted first in mapping. Images with one name under two members share a position, so a rig's lenses form one sequence. `.` is the image directory itself. Repeatable. |
| `--camera-model MODEL` or `PREFIX=MODEL` | Lens model for the whole capture, or for the camera group whose images are under `PREFIX`. Repeatable. Models: `simple-pinhole`, `pinhole`, `radial`, `opencv`, `full-opencv`, `opencv-fisheye`, `thin-prism-fisheye`, `equirectangular`. |
| `--focal F` or `PREFIX=F` | Focal length in pixels, for all images or one group. |
| `--distortion k1,k2,...` or `PREFIX=k1,k2,...` | Starting distortion coefficients, for all images or one group. |
| `--no-masks` | Ignore a masks directory even if one sits beside the images. |
| `--no-manage` | Skip merging, growing, reseeding, auditing and splitting; keep the mapper's raw models. |
| `--progress-dir DIR` | Write snapshots of the run (the current model, the pair matrix) for a front end to watch. |
| `-h`, `--help` | Show help and exit. |

### Presets

`auto` applies `--quality` and `--data-type` before the run and prints what
they moved. A flag named on the command line is never moved.

| `--quality` | `--max-image-size` (SIFT / learned features) | `--max-features` | `--aliked-max-features`, `--loma-max-features` | `--prefilter-neighbors` |
|---|---|---|---|---|
| `low` | 1000 / 800 | 2048 | 1024 | 16 |
| `medium` | 1600 / 1200 | 4096 | 2048 | 24 |
| `high` (default) | 2400 / 1600 | 8192 | 4096 | unchanged |
| `extreme` | 3200 / 2400 | 16384 | 8192 | 48 |

| `--data-type` | sets |
|---|---|
| `individual` (default) | nothing |
| `video` | `--pairs sequential` (below 100 images; pair selection takes over above), `--init-min-tri-angle` halved |
| `internet` | `--camera-mode image`, `--no-prefilter-sequential` |

With `--progressive`, `--quality` also sets the progressive-alignment steps:

| `--quality` | `--progressive-error-start` | `--progressive-error-steps` | `--progressive-features` | `--progressive-feature-steps` | `--progressive-patience` |
|---|---|---|---|---|---|
| `low` | 12 | 5 | off | 1 | 1 |
| `medium` | 16 | 8 | on | 2 | 2 |
| `high` | 20 | 12 | on | 3 | 3 |
| `extreme` | 24 | 15 | on | 4 | 4 |

Learned features (ALIKED, LoMa) with a non-learned matcher also get
`--ratio 0.92`. A learned matcher (`lightglue`, `loma-*`) gets `--ratio 1`,
`--min-similarity 0` and `--pairs prefilter`.

### Exit status

| code | meaning |
|---|---|
| 0 | a reconstruction that looks sound |
| 1 | usage or runtime error |
| 2 | no reconstruction at all |
| 3 | partial: under half the images registered, or over 2 px mean reprojection error |
| 4 | the model was written, but not in the metric frame that was asked for |
| 130 | interrupted with Ctrl-C |

## `extract`

```text
spirula sfm extract <IMAGE|DIR> [-o OUT] [options]
```

GPU feature detection over one image or, recursively, a directory. Feature
files mirror the image tree; keypoints are in the source image's pixels even
when `--max-image-size` downscaled it.

| flag | meaning |
|---|---|
| `-o`, `--output DIR\|FILE` | Output directory for a directory of images, or the `.bin` file for one image. Default `features`. |
| `-h`, `--help` | Show help and exit. |

## `match`

```text
spirula sfm match <FEATURE_DIR> -o MATCHES.BIN [options]
```

Descriptor matching on the GPU, then two-view geometric verification. The
camera grouping and focal lengths it settles on are stored in the match file
for `map`.

| flag | meaning |
|---|---|
| `-o`, `--output FILE` | Match database to write. Without it the run reports and writes nothing. |
| `--sequence PREFIX[,PREFIX...]` | As for `auto`. |
| `--camera-model`, `--focal`, `--distortion` | As for `auto`, including the `PREFIX=` forms. |
| `--progress-dir DIR` | As for `auto`. |
| `-h`, `--help` | Show help and exit. |

## `map`

```text
spirula sfm map <MATCHES.BIN> <FEATURE_DIR> -o SPARSE_DIR [options]
```

Incremental reconstruction. Models go to `SPARSE_DIR/0`, `SPARSE_DIR/1`, ...
largest first. With no camera flags, the setup recorded by `match` is used as
it stands.

| flag | meaning |
|---|---|
| `-o`, `--output DIR` | Directory to write the models into. |
| `--rig [KIND=]PREFIX,PREFIX,...` | As for `auto`. |
| `--sequence PREFIX[,PREFIX...]` | As for `auto`. |
| `--camera-model`, `--focal`, `--distortion` | As for `auto`. |
| `--audit` | Audit every pose against the correspondence graph before anything else and re-register what the model cannot support. Use with `--resume` on models from elsewhere. (This wins over the table's `--audit` switch.) |
| `--no-manage` | Skip merging, growing, reseeding, auditing and splitting. |
| `--progress-dir DIR` | As for `auto`. |
| `-h`, `--help` | Show help and exit. |

## `merge`

```text
spirula sfm merge <SPARSE_DIR|MODEL_DIR> [more...] -o DIR [options]
```

Merges models on the images they share. A directory holding `0/`, `1/`, ...
is expanded to all of them. A single model is accepted only with
`--metric-positions` or `--metric-gps`, which re-gauge it in metres. A bundle
adjustment runs across each seam unless `--no-ba`.

| flag | meaning |
|---|---|
| `-o`, `--output DIR` | Directory to write the merged models into. Required unless `--in-place`, which takes exactly one input directory. |
| `-h`, `--help` | Show help and exit. |

## `ba`

```text
spirula sfm ba <INPUT_MODEL_DIR> <OUTPUT_MODEL_DIR> [options]
spirula sfm ba <BAL_PROBLEM.TXT> <OUTPUT.PLY> [options]
```

Runs the mapper's global bundle adjustment on a COLMAP sparse model and writes
the refined model (the output may be the input directory). If the GPU fails
partway, the CPU continues from the last checkpoint. Given a problem in the
Bundle Adjustment in the Large format, it solves that and writes the points as
a PLY. `ba` does not read the table in [sfm-flags.md](sfm-flags.md).

| flag | default | meaning |
|---|---|---|
| `--real {float\|double\|df\|cpu}` | `double` | Scalar the solver works in; `df` is an emulated double-float for devices without fp64, `cpu` runs on the host. |
| `--loss {trivial\|huber\|cauchy}` | `huber` on a model, `trivial` on BAL | Robust loss. |
| `--loss-param X` | 2 on a model | Huber delta or Cauchy c, in pixels. |
| `--model {snavely\|snavely_f}` | `snavely` | BAL camera model. |
| `--shared-intrinsics` | off | One intrinsics block for every camera (BAL). |
| `--rig [KIND=]PREFIX,PREFIX,...` | the model's `rigs.txt` | Rigs to constrain, as for `auto`. |
| `--solver {auto\|dense\|cg}` | `auto` | Dense Cholesky or implicit-Schur PCG. |
| `--max-iters N` | 50 | Levenberg-Marquardt iteration cap. |
| `--damping X` | 0.01 | Initial LM damping. |
| `--rtol X` | 1e-6 | Relative cost improvement to stop below. |
| `--patience N` | 10 | Steps below `--rtol` before stopping. |
| `--cg-iters N` | 100 | PCG iteration cap per LM step. |
| `--cg-tol X` | 0.1 | PCG relative residual tolerance. |
| `--cg-fallback {auto\|on\|off}` | `auto` | Fall back to dense when PCG stalls. |
| `--vram-budget MB` | 90% of device memory | Device memory the solver may use. |
| `--device <index\|name\|auto\|-1\|uuid:hex>` | `auto` | Vulkan device. |
| `--validate` | off | Check the assembled system against a reference. |
| `--profile` | off | Per-kernel timings. |
| `--quiet` | off | Print only the result lines. |
| `--spv-path FILE` | built in | Load the solver kernels from this SPIR-V file. |
| `-h`, `--help` | | Show help and exit. |

A `--real` and `--loss` pair left out of the build (`SS_SFM_REALS`,
`SS_SFM_LOSSES`) reports "variant not built into this binary".

## Environment

| variable | effect |
|---|---|
| `SS_SFM_MAP_PROF=1` | print a per-stage breakdown of the mapper's time |
| `SS_SFM_EVENT_TRACE=1` | print the progress event stream a front end sees |
