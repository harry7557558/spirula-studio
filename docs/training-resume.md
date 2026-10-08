# Stopping and resuming training

A training run can be stopped and continued later, in the GUI or on the command
line. Nothing about the result differs from an uninterrupted run except the
wall-clock gap: the optimizer state, step counter and learning-rate schedule
all carry on from where they stopped.

## In the desktop app

1. While training, press **Stop** and choose **Stop and Save**. The current
   step finishes and a checkpoint is written into the run's output folder, for
   example `outputs/<run>/step-000012000.ckpt/`, next to its `config.json`.
2. Later, choose **File > Resume Training...** or **Resume Training...** on the
   Home screen, and pick that run folder. You can also pick a specific
   `step-*.ckpt` folder inside it.
3. The run's dataset opens on the training screen with the run's own settings,
   and the seed line reads that the Gaussians come from the checkpoint. Press
   **Start** to continue from the saved step toward the run's original step
   count. New checkpoints, logs and evaluation images go into the same run folder.

If a folder cannot be resumed, a dialog explains why. Typical reasons are a
checkpoint saved without optimizer state (see below), a missing `config.json`,
or a dataset folder that has since been moved or deleted.

Opening a different dataset clears the pending resume, so a checkpoint is never
applied to the wrong scene. Settings can still be changed before pressing
Start. Layout changes such as a different SH degree or splat cap are adapted on
load, as with `--resume`.

## What a save contains

A checkpoint written **when a run is stopped before its last step** always
holds everything needed to continue: the world parameters, optimizer state and
`full_resume` marker. That is true whatever **Save resumable checkpoints**
(`save_full_checkpoint`) is set to, because stopping early means you intend to
come back.

While only the newest checkpoint is kept (`save_only_latest_checkpoint`, the
default), each periodic checkpoint holds the optimizer state too, so a run that
crashed or was killed continues from its last save. It is replaced by the next
one, so this costs disk space only until the run ends: with a 1M-splat cap it
is about 330 MB, since it stores every splat slot.

The final checkpoint of a run that reached its step count, and the older
checkpoints kept when only the newest is not, follow `save_full_checkpoint`.
With it off they are smaller, without optimizer state: they hold the finished
splats for viewing, export and meshing, but cannot continue training.

**Stop without saving** writes nothing, and `steps_per_save = 0` disables
checkpoints entirely, including the one on stop.

## On the command line

```text
spirula train --resume outputs/<run>
spirula train --resume outputs/<run>/step-000012000.ckpt
```

The first form picks the latest checkpoint in the run. Settings come from the
run's `config.json`; any flag given explicitly on the command line overrides
them, and `--preset` re-applies a preset's overrides. The GUI uses the same
code path (`ckpt::build_resume_config`, `src/checkpoint/Resume.h`).

Ctrl-C during `spirula train` works like **Stop and Save**: the current step
finishes, a resumable checkpoint is written and the command prints the line
that continues it. A second Ctrl-C quits at once without saving. With
`steps_per_save = 0` there is nothing to save, and Ctrl-C quits at once.
