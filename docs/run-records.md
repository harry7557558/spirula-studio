# Run records

Every training run, from the GUI or from `spirula train`, leaves two records
on disk.

## `<dataset>/logs/train_<yyyyMMddHHmmss>.log`

Written by the front end (`src/app/RunLog.h`). `<dataset>` is the `--data`
folder, or its parent when `--data` names the `images` folder itself. The file
starts with a settings snapshot of `key = value` lines under headings: the
`[Run]` section (`run_started`, `spirula_version`, the device, the preset; the
CLI adds the full `command`), then the training config grouped as the options
editor groups it. The GUI also records its dataset preparation, SfM and
geometry panels. After the `End of settings` rule come the run's log lines,
each stamped with local time, including `Output directory: ...` and
`Training complete. Steps: N   Time: HH:MM:SS`. The CLI also writes its
progress lines and the error that ended a failed run.

The headings and log lines follow `--lang`; the keys and values do not. A
dataset folder that cannot be written to gets no log, and the run goes on.

## `<output dir>/run.json`

Written by the trainer itself (`TrainerSession`, `src/app/TrainerCore.h`) next
to `config.json`, so both front ends produce the same file. It is written when
the output folder is made and again when the run ends, replacing the file in
one step, so a reader never sees half of it.

| key | meaning |
|---|---|
| `status` | `running`, `completed`, `stopped` (ended before `num_iterations`) or `failed` |
| `error` | the failure's message; `null` otherwise |
| `started`, `ended` | UTC, ISO 8601 (`2026-11-22T13:03:05Z`); `ended` is `null` while running |
| `training_time_s` | wall clock of the step loop, paused time excluded (the log's `Time`) |
| `start_step`, `final_step`, `num_iterations` | where this run began (non-zero on resume), the steps completed, and the target |
| `final_checkpoint` | whether the end-of-run checkpoint was saved |
| `num_splats` | splats at the last completed step |
| `device`, `backend` | the GPU's name and `vulkan` or `cuda` |
| `spirula_version`, `preset`, `data`, `resume`, `log` | the build, the preset, the dataset, the checkpoint resumed from, the log file above |

A run killed outright (power loss, `SIGKILL`) leaves `status: running`. A
resumed run reuses its folder; the records already in `run.json` move into
its `earlier_runs` array, oldest first. Eval results stay in `metrics.json`.
