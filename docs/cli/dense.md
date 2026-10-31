# `spirula dense`

Builds a dense point cloud for a dataset with RoMa v2 matching; training then
seeds from it. Running it needs a build with `SS_BUILD_SAM=ON` and the RoMa
checkpoint. What the settings do, with measurements, is in
[../dense.md](../dense.md); this page lists every flag.

```text
spirula dense <dataset> [--preset fast|balanced|high|custom] [--config PATH] [--flag value ...]
spirula dense --write-config PATH [--flag value ...]
```

## Reading the command line

- Every flag takes a value. Booleans take `true`/`false`/`1`/`0`; numbers are
  plain JSON numbers.
- Flag names accept `-` or `_`: `--max-pairs`, `--max_pairs`.
- Flags apply in order. `--preset` and `--config` overwrite what they cover,
  so give them first.
- `--config PATH` reads a JSON object of the same settings (keys spelled with
  `_`), as the GUI and `--write-config` save them.

## Run control

| flag | default | meaning |
|---|---|---|
| `--preset <name>` | `balanced` | `fast`, `balanced` or `high` sets matching size, pairing and density together (table below); `custom` keeps the current values. The older matcher-only names `turbo`, `base` and `precise` are still read. |
| `--config PATH` | | Load settings from a JSON file. |
| `--write-config PATH` | | Write the resulting settings as JSON and exit without running. |
| `--source-workflow true` | | Shorthand for `--matching-space source --reference-fraction 0.8 --neighbors 3 --samples-per-reference 10000 --sampling-seed 0 --source-reprojection-error 1`. |
| `--checkpoint <id\|path>` | `romav2.0.1` | RoMa weights: the cached official checkpoint, or a path to a `.pt` file. The CLI does not download weights. |
| `--device <sel>` | `auto` | Vulkan device. |
| `--progress-dir DIR` | | Write live snapshots and memory reports for a front end to watch. |
| `--perf-dir DIR` | | Write a performance log of the run. |
| `--resume <bool>` | `true` | Reuse verified pair predictions a previous run left in `dense/cache/`. |
| `--rebuild <bool>` | `false` | Ignore cached predictions and match again. |
| `--keep-cache <bool>` | `false` | Keep the predictions after a successful run (about 10 MB a pair at 640, 80 MB at 1280 both ways). |
| `-h`, `--help` | | Print the flags with their defaults and exit. |

### Presets

| preset | `--low-width/height` | `--high-width/height` | `--bidirectional` | `--neighbors` | `--stride` | `--reference-coverage` |
|---|---|---|---|---|---|---|
| `fast` | 512 | 0 | false | 2 | 2 | 12 |
| `balanced` | 640 | 0 | false | 3 | 2 | 12 |
| `high` | 800 | 1280 | true | 4 | 1 | 16 |

Settings saved with an older preset's pairing (`--neighbors` 4, 6 or 8 with
`--reference-coverage` 2, 3 or 0) are read as that preset's current values.

## Inputs

| flag | default | meaning |
|---|---|---|
| `--image-dir <dir>` | `images` | Image folder, relative to the dataset. |
| `--recon-dir <dir>` | the largest model | COLMAP model to use. |
| `--metashape-xml <file>` | | Metashape camera export. |
| `--metashape-psx <file>` | | Metashape project. |
| `--metashape-component <n>` | -1 | Which component of a Metashape export to use, by index; -1 takes the largest. |
| `--exif-orientation none\|orient\|apply` | `none` | What EXIF orientation is worth, as for `spirula train`. |
| `--image-gamut <name>` | | Colour primaries of the images. |
| `--image-is-linear <bool\|null>` | `null` | Images are linear light; `null` uses the file's own declaration. |
| `--image-exposure <auto\|stops>` | | Brighten what the matcher sees, in linear light. |

## Matching

| flag | default | meaning |
|---|---|---|
| `--low-width`, `--low-height` | 640 | RoMa's coarse input size; multiples of 16. |
| `--high-width`, `--high-height` | 0 | Fine refinement input size; 0 turns the fine pass off. |
| `--bidirectional <bool>` | `false` | Match each pair both ways; needed for the cycle check. |
| `--overlap-saturation <x>` | -1 | Treat predicted overlap above this as 1; -1 turns it off. |
| `--memory-budget-bytes <n>` | 0 | Device memory RoMa may use; 0 is 80% of the device. |
| `--precision auto\|float32\|mixed` | `auto` | `auto` uses mixed FP16/FP32 where the device has cooperative matrices; `float32` pins the reference path. |
| `--matching-space rectified\|source` | `rectified` | Match undistorted pinhole views (wide lenses split into cube faces), or the original images. |
| `--split-views <bool>` | `true` | Split a wide lens into several pinhole faces rather than one view. |
| `--sparse-face-pairs <bool>` | `true` | Pair faces only when they share sparse points. |
| `--max-face-size <n>` | 1280 | Largest side of a split face, at least 16. |

## Pairing

| flag | default | meaning |
|---|---|---|
| `--pair-mode automatic\|sequential\|exhaustive\|explicit` | `automatic` | How image pairs are chosen. |
| `--neighbors <n>` | 3 | Automatic mode: each reference takes partners until every surface cell of its view is seen by this many of them, at most twice this many in all. In source mode, neighbours per reference. Needs `neighbors + 1 >= min-source-images`. |
| `--reference-coverage <n>` | 12 | Automatic mode: references are added until every surface cell is seen by this many of them; 0 makes every view a reference. |
| `--sequence-window <n>` | 8 | Neighbours each image pairs with in sequential mode. |
| `--max-pairs <n>` | 0 | Stop after this many pairs; 0 is no cap. |
| `--pair-list <file>` | | Explicit mode: two registered image names per line. |
| `--max-baseline <x>` | -1 | Automatic mode: refuse a neighbour farther than this; 0 is automatic (3x the median planned baseline on a metric model), negative turns it off. |
| `--reference-fraction <x>` | 0.8 | Source mode: share of views chosen as references. |

## Masks

| flag | default | meaning |
|---|---|---|
| `--use-masks <bool>` | `true` | Use any masks at all; `false` bypasses the three sources below. |
| `--training-masks <bool>` | `true` | Use the training masks in `--mask-dir`. |
| `--alpha-masks <bool>` | `true` | Use the images' alpha channel. |
| `--feature-masks <bool>` | `true` | Use the feature masks in `--feature-mask-dir`. |
| `--mask-dir <dir>` | `masks` | Training mask folder. |
| `--feature-mask-dir <dir>` | `feature_masks` | Feature mask folder. |
| `--invert-masks <bool>` | `false` | Masks are white where the image is not kept. |
| `--mask-threshold <x>` | 0.5 | Mask value, 0..1, at or above which a pixel is kept. |
| `--mask-boundary <px>` | 0 | Grow (positive) or shrink (negative) the kept area by this many pixels. |

## Points and filters

| flag | default | meaning |
|---|---|---|
| `--stride <n>` | 2 | Use every n-th matching-grid pixel. |
| `--samples-per-reference <n>` | 10000 | Source mode: samples drawn per reference view. |
| `--sampling-seed <n>` | 0 | Seed for that sampling. |
| `--min-overlap <x>` | 0.5 | Least predicted overlap a match needs, 0..1. |
| `--cycle-check <bool>` | `true` | Reject matches that do not map back within `--max-cycle-error` (only with `--bidirectional true`). |
| `--max-cycle-error <x>` | 1 | Cycle-check tolerance, in matching-grid pixels. |
| `--min-angle-degrees <x>` | 1 | Least triangulation angle. |
| `--max-reprojection-error <x>` | 2 | Largest reprojection error, in working-view pixels. |
| `--max-relative-depth-error <x>` | 0.01 | Largest relative depth disagreement. |
| `--min-source-images <n>` | 3 | Original images a point must be seen in. |
| `--source-reprojection-error <x>` | 1 | Source mode: reprojection tolerance, in matcher cells. |
| `--two-image-points auto\|off` | `off` | Admit points seen in exactly two images under the extra checks in [../dense.md](../dense.md#point-filters). |
| `--free-space-test <bool>` | `true` | Drop a two-image point that a three-image point is seen through. |
| `--max-depth-error-per-cell <x>` | -1 | Depth-precision bar per matcher cell; 0 is automatic, negative off. |
| `--far-isolated <bool>` | `true` | Drop isolated points far outside the sparse points' bounds. |
| `--reprojection-check <bool>` | `false` | Measure the written cloud's reprojection error into the manifest; removes nothing. |
| `--voxel-size <x>` | 0 | Fusion cell size in dataset units; 0 is automatic. |
| `--point-limit <n>` | 0 | Most points to keep; 0 is no limit. |
| `--remove-outliers <bool>` | `false` | Statistical outlier removal after fusion. |
| `--outlier-neighbors <n>` | 16 | Neighbours it looks at, at least 2. |
| `--outlier-stddev <x>` | 2 | Standard deviations past which a point is an outlier. |

## Resources

| flag | default | meaning |
|---|---|---|
| `--image-cache-bytes <n>` | 0 | Host memory for decoded images and views; 0 is the smallest of an eighth of RAM, a third of free RAM and a fifth of the free commit. |
| `--cpu-workers <n>` | 0 | CPU threads; 0 uses every hardware thread. |

Exit status is 0 on success and 1 on any error. The first Ctrl-C (or
`SIGTERM`) stops at the next check and keeps the finished pairs for a resume;
a second one ends the process.
