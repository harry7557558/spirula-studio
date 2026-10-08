# Training performance recording

Finding out what limits a training run: the GPU, image decoding on the CPU,
the disk, or other per-step host work.

## Record from the app

Tick **Log performance stats** in the run settings (`--log-performance true`
on the command line). From setup to the end of training, Spirula records once
a second into a new `<run folder>/perf/<date-time>/` folder, and the log names
the folder and the report command when the run ends:

```text
python tools/perf/perf_report.py <run folder>/perf/<date-time>
```

Resuming a run in the same folder starts another session beside the earlier
ones. Given `<run folder>/perf` itself, the report reads the newest session and
names the others.

It prints a per-stage summary, including the loading phase before the first
step and the run's key settings, and writes `report.html` in that folder.
The recorder (`src/app/SystemRecorder.cpp`) uses about 1.5% of one CPU core.
It covers Windows fully; on Linux it reads `/proc` and NVIDIA's driver but has
no per-adapter GPU engines, and on macOS it records memory only.

## Record a dense step

Tick **Log performance stats** in the dense step's options (`spirula dense
DATASET --perf-dir DIR` on the command line). The step records the same machine
files plus `dense_perf.csv`: once a second, and at every change, the phase
(prepare, match, refine, fuse) and how far it has got. Each run gets a new
folder under the dataset's `dense/perf/`, which the log names when the step
ends, finished or not. The report then gives one row per phase, with a
verdict: GPU-bound, CPU-bound on all cores, CPU-bound on one thread (a serial
part that more threads would speed up), disk-bound, or waiting. The option
does not change the cloud or whether a finished one is reused.

## Record with the script

`record_run.ps1` records the same files from outside Spirula, for runs the
checkbox does not cover (a dense step on its own, or an older build). Windows
only.

```text
pwsh tools/perf/record_run.ps1
```

This starts `build_vulkan\spirula.exe` with `SS_TRAIN_PERF` set, and records once
a second into `build_vulkan\perf\<date-time>\` until Spirula exits. Train as
usual, close Spirula when the run is done, then:

```text
python tools/perf/perf_report.py build_vulkan/perf/<date-time>
```

Options: `-Exe PATH` for another build, `-Out DIR` for the folder, `-Attach`
to record an already running Spirula (machine counters only, unless it was
started with `SS_TRAIN_PERF`), and anything after `--` is passed to Spirula,
e.g. `-- train --data <dataset folder>` for a command-line run.

## What is recorded

| File | Source | Contents |
|---|---|---|
| `dense_perf.csv` | the dense step (`DensePerfLog` in `src/app/DenseProcessing.cpp`) | per second and at each change: phase, done, total, rate, process RAM |
| `train_perf.csv` | the trainer (`src/app/TrainPerfLog.h`) | per second: steps, step time, time waiting for the data loader, sampled GPU time per step, resolution stage, splats, pool VRAM, process RAM |
| `system.csv` | Windows performance counters | CPU per core and total, RAM, disk throughput and busy time, and the CPU, memory, threads and I/O of every `spirula.exe` (a dense step's child included) |
| `gpu_engines.csv` | Windows GPU engine counters | Spirula's utilization of each engine on each adapter, integrated and discrete |
| `nvidia.csv` | NVML (the app) or `nvidia-smi` (the script) | utilization, VRAM, clocks, power, temperature, clock-limit reasons |
| `meta.json` | | CPU, RAM, GPUs, OS, how Spirula was started |

The app counts its own process and any it started; the script counts every
`spirula.exe`.

`SS_TRAIN_PERF=1` alone, without the recorder, writes `train_perf.csv` into the
run's output folder.

## Reading the verdicts

Each resolution stage gets one:

- **GPU-bound**: the GPU is busy for most of each step. The expected state.
- **data-bound: CPU**: steps wait for images while the CPU is saturated, so
  decoding and resizing is the limit. Typical with `cache_images disk` in the
  low-resolution stages of progressive resolution.
- **data-bound: disk**: steps wait for images while the disk is busy.
- **data-bound**: steps wait though neither is saturated: too little decode
  parallelism or prefetch for the step rate.
- **host overhead**: the GPU is idle, but not for lack of data: other host work
  in the step (synchronization, densification, the viewer) dominates.

"GPU busy" comes from the trainer's own GPU timer, which brackets one step in
ten. It is the share of step time the GPU spent rendering and training. The
NVIDIA and per-adapter utilization lines are the driver's view and include
other work.
