# `spirula train`

Trains a Gaussian-splat model on a dataset. Every training flag is listed,
with its default and meaning, in [train-flags.md](train-flags.md); this page
covers how the command line is read and the few flags outside that table.

```text
spirula train [<preset>] --data <dataset_dir> [--flag value ...]
spirula train --help | --help-all
```

## Command-line rules

- **Preset.** An optional first word that does not start with `-` names a
  preset: `3dgs` (the default), `360-camera`, `in-the-wild`,
  `centered-object`, `hdr`, `synthetic`, `meshing`, and the hidden
  `academic-baseline`. What each changes is in
  [train-flags.md](train-flags.md#presets).
- **Flag names** are the config fields with `-` or `_` between words; both
  spellings work (`--sh-degree`, `--sh_degree`).
- **Every flag takes a value**, booleans included: `--warp-to-pinhole 1`.
  Booleans take `1`/`0`/`true`/`false`/`True`/`False`.
- **`--flag=value`** works for flags with one value.
- **Three-value flags** (`--background-color`, `--long-axis-split-opacity-k`,
  `--bilagrid-shape`, `--bilagrid-shape-geometry`) take three separate
  values: `--bilagrid-shape 8 8 4`.
- **`none`** (also `None`, `null`) clears an optional number or a text field.
- **Choices** are checked: a flag with a list of values rejects anything else.
- **`inf`** is accepted where a number is expected.
- `--data` is the one required flag, except with `--resume`.

Resolution order is: the base defaults, then the preset, then the macro flags
below, then every flag given on the command line.

## Flags outside the table

| flag | meaning |
|---|---|
| `-h`, `--help` | Print the presets and the `basic` flags with their defaults after the preset is applied, then exit. |
| `--help-all` (`--help_all`) | The same, with every flag. |
| `--device <sel>` | Vulkan build: the GPU to train on, as an index, part of a name, `auto` or `uuid:<32 hex digits>` (default `auto`, after `SS_VK_DEVICE`). CUDA build: a CUDA device index only (default: the current CUDA device). The device list prints at startup, with `*` on the one in use. `--device=<sel>` works too. |

## Macro flags

Three `basic` flags each move several specialist flags at once. A macro never
overrides a flag set on the command line, and at its default it changes
nothing, so a preset's own tuning survives. Naming it (`--quality medium`)
does write its values, which is how a preset's tuning is put back to stock.

| flag | value | sets |
|---|---|---|
| `--quality` | `low` | `--cap-max 200000`, `--num-iterations 15000` |
| | `medium` (default) | `--cap-max 1000000`, `--num-iterations 30000` |
| | `high` | `--cap-max 3000000`, `--num-iterations 50000` |
| | `ultra` | `--cap-max 10000000`, `--num-iterations 80000` |
| `--distraction-robustness` | `mild` | `--densify-loss-map-power 1`, `--densify-score-power 1`, `--ssim-lambda 0.1` |
| | `strong` | the above, plus `--densify-loss-map-mode robust_edge_aware`, `--densify-robust-edge-aware-quantile 0.75`, `--densify-score-mode median` |
| `--floater-suppression` | `mild` | `--depth-distortion-reg 0.01`, `--rgb-distortion-reg 0.01`, `--sh-reg 0.01`, `--max-screen-size 0.2`, `--max-screen-size-clip-hardness 1.25` |
| | `strong` | `--depth-distortion-reg 0.03`, `--rgb-distortion-reg 0.05`, `--sh-reg 0.05`, `--max-screen-size 0.1`, `--max-screen-size-clip-hardness 1.1` |

## Resuming and warm starts

- `--resume <run_dir|step-*.ckpt>` continues a run. The checkpoint's
  `config.json` becomes the base, so `--data` and the model settings come from
  it; a preset or any flag named on the command line (including one a macro
  wrote) is layered back on top. The checkpoint has to hold the training
  state: periodic checkpoints do while `--save-only-latest-checkpoint` is on
  (the default), so does the one an interrupted run writes, and a finished
  run's final checkpoint does only with `--save-full-checkpoint 1`.
- `--init-ply <file.ply|run_dir|step-*.ckpt>` seeds a new run from a trained
  model instead of the dataset's points; it is not a resume (the step count
  and optimizer start fresh). `--init-ply-add-points 1` adds the dataset's
  points on top. `src/app/README.md` has the details of both.

## While it runs

- Progress prints every 100 steps: step, splat count, elapsed time, ETA and
  the losses.
- Unless `--disable-viewer 1`, the web viewer listens on all interfaces at
  `--viewer-port` (7007). `--keep-viewer-alive 1` keeps the process and the
  viewer up after training until it is killed.
- Ctrl-C stops after the current step and saves a resumable checkpoint, unless
  `--steps-per-save 0`. A second Ctrl-C ends the process at once.

## Exit status

| code | meaning |
|---|---|
| 0 | training (and the held-out evaluation) finished |
| 1 | an error: bad flag, missing dataset, a device or engine failure |
| 130 | stopped with Ctrl-C; the message names the run folder to `--resume` |
