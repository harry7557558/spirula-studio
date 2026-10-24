# Command-line reference

Every command-line flag of `spirula`, the one executable this project builds.
Each subcommand also prints its own help (`spirula <command> --help`), in the
language `--lang` picks; this reference is the English, all in one place.

| page | covers |
|---|---|
| this page | the top-level command, global flags, the desktop app, environment variables |
| [train.md](train.md) | `spirula train`: presets, macro flags, `--device`, resume |
| [train-flags.md](train-flags.md) | every training flag (generated) |
| [sfm.md](sfm.md) | `spirula sfm auto / extract / match / map / merge / ba` |
| [sfm-flags.md](sfm-flags.md) | every SfM table flag (generated) |
| [sam.md](sam.md) | `spirula sam devices / segment / track / mask / video / extract` |
| [geometry.md](geometry.md) | `spirula geometry`: depth and normal maps |
| [dense.md](dense.md) | `spirula dense`: the RoMa dense point cloud |
| [mesh.md](mesh.md) | `spirula mesh`: surface extraction from a trained model |
| [scans.md](scans.md) | `spirula e57` and `spirula lidar`: laser scans |
| [partition.md](partition.md) | `spirula partition split / merge` |
| [encode.md](encode.md) | `spirula encode`: GPU video encoding from stdin |

`train-flags.md` and `sfm-flags.md` are written by
`python3 tools/gen_cli_reference.py` from the flag tables in
`src/config/TrainConfig.h` and `src/sfm/SfmConfig.h`. Rerun it after adding or
changing a row there; `--check` reports a stale file without writing it. The
other pages are written by hand from the parsers in `src/app/cli/`.

## The top-level command

```text
spirula                          open the desktop app
spirula <file-or-folder>...      open the desktop app on what was named
spirula <command> [options]      run one tool
spirula --help | -h | help       list the commands this build has
spirula --version | -v           print the version
```

A first argument that is not a command goes to the desktop app untouched, the
same as dropping it on the window. A build without the GUI
(`-DSS_BUILD_GUI=OFF`) reports an unknown command instead.

The executable also answers to its file name: a copy or symlink called
`spirula-sfm` (or `ssplat-sfm`, the pre-rename spelling) behaves as
`spirula sfm`, `--help` and `--version` included. An explicit command still
wins: `spirula-sfm train ...` runs the trainer.

### Which commands a build has

| command | built when |
|---|---|
| `gui` (and no command) | `SS_BUILD_GUI=ON` |
| `train`, `mesh`, `dense`, `partition`, `e57`, `lidar` | always (`dense` needs `SS_BUILD_SAM=ON` to run) |
| `sfm` | `SS_BUILD_SFM=ON` (Vulkan backend) |
| `sam`, `geometry` | `SS_BUILD_SAM=ON` (Vulkan backend) |
| `encode` | `SS_BUILD_SAM=ON` and `SS_ENABLE_PATENTED=ON` |

`spirula --help` lists only the commands the running binary contains.
`docs/build.md` has the build options.

## Global flags

These are taken out of the command line before any tool sees it, so they work
anywhere on it and with every command.

| flag | meaning |
|---|---|
| `--lang <code>`, `--lang=<code>` | Language for help text and messages: `en`, `ja`, `zh-Hans`, `zh-Hant`, `ko`, `de`, `fr`, `es`, `pt`, `it`, `nl`, `ru`, `tr`. Wins over `SS_LANG`, the GUI's saved choice and the OS language. Flag names are never translated. |
| `--accept-license <spec>` | Accept a model licence from a terminal, for every command that downloads a licensed model. `<spec>` is a comma list of families (`sam3`, `sam2`, `gdino`, `birefnet`, `roma`): a bare family prints its terms and asks on the terminal; `family=yes` accepts without asking. Repeatable; `--accept-license=<spec>` works too. Builds with `SS_BUILD_SAM=ON` only. The acceptance is recorded in the GUI's settings file, which the GUI reads too. |

Without a terminal (a script, a pipe) a licence that has not been accepted is
refused with a message naming the `--accept-license family=yes` to pass.

## The desktop app

```text
spirula [<path>...]
spirula gui [<path>...]
```

The app takes no flags of its own beside the global ones. Each path is opened
as if it had been dropped on the window: a dataset folder, a training run, a
`.ply`, a video or a folder of photos lands on the screen that handles it, and
several videos named together become the inputs of one dataset.

| environment variable | effect |
|---|---|
| `SS_GUI_AUTOMATION=1` | start the loopback control server `tools/guictl.py` drives (`docs/notes/gui-automation.md`) |
| `SS_GUI_AUTOMATION_PORT` | its port (default 7777) |
| `SS_GUI_AUTOMATION_TOKEN` | a fixed token instead of a random one |
| `SS_GUI_OFFSCREEN=1` | create the window invisible |

## Conventions shared by the tools

- `-h` / `--help` prints the tool's help and exits 0.
- `--device` picks the GPU. On Vulkan it takes an index, part of a device
  name, `auto`, or `uuid:<32 hex digits>`; a CUDA build takes an index only.
  Without it the `SS_VK_DEVICE` environment variable is used, then `auto`.
- A run interrupted with Ctrl-C stops cleanly where the tool supports it and
  exits 130; a second Ctrl-C ends the process.
- Every tool writes a stack trace to `<config dir>/crash.log` if it crashes.

## Environment variables

| variable | read by | effect |
|---|---|---|
| `SS_LANG` | all | interface language when `--lang` is not given |
| `SS_VK_DEVICE` | Vulkan tools | default device selector, same forms as `--device` |
| `SS_NO_AUTO_FETCH` | all | never ask on the terminal before downloading a model |
| `SS_NN_LOG=0..3` | `sam`, `geometry` | inference log verbosity |
| `SS_PROFILE=1` | `sam`, `geometry` | per-kernel timings |
| `SS_VK_VALIDATION=1` | `sam` | Vulkan validation layers |
| `SS_NN_DEBUG_SYNC=1` | `sam` | synchronise after every dispatch |
| `SS_SFM_MAP_PROF=1` | `sfm` | per-stage breakdown of the mapper's time |
| `SS_SFM_EVENT_TRACE=1` | `sfm` | print the progress event stream the GUI reads |
| `SS_DUMP_CAMERAS=<path>` | `train` | write the parsed camera arrays as JSON and exit (debugging) |
