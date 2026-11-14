# Named PLY exports and Y-up delivery

Every checkpoint retains `splat.ply` as the canonical native model used by
Spirula and existing scripts. It also publishes `<run>.ply`, where `<run>`
is the output directory name, both inside the checkpoint and at the run root:

```text
outputs/example/
  config.json
  example.ply
  example_y_up.ply           # only with --export-y-up
  step-000030000.ckpt/
    splat.ply
    example.ply
    state.tar
```

The named native files refer to the latest saved checkpoint. Hard links avoid
duplicating large PLYs where supported; other filesystems use a copy. Each
publication replaces a temporary file, so an interrupted export leaves the
previous delivery intact. Keeping only the latest checkpoint does not remove
the run-root delivery. Checkpoint resume still uses `state.tar`.

Enable `--export-y-up true` (or **Export Y-up PLY** in the training options)
to also publish `<run>_y_up.ply` at every checkpoint. The default is off.
This separate delivery rotates Z-up to Y-up with the right-handed transform
`(x, y, z) -> (x, z, -y)`. The existing splat transform rotates Gaussian
orientations, spherical harmonics and spherical-beta lobe axes together.
Export reads bounded chunks rather than loading another full model into RAM.

Training, native PLYs, cameras, `scene_transform.json` and `gauge.txt` retain
their original coordinates. The `_y_up` file is for external Y-up consumers;
use the native model for camera-based meshing with the run's dataset. The
export applies only the stated axis rotation, with no format-specific heading
compensation.

Opening a run still prefers its newest checkpoint and canonical `splat.ply`.
Named-only checkpoint folders and run folders are also supported. Without a
canonical or matching run-name file, there must be exactly one splat PLY;
ambiguous folders require an explicit file path. GUI folder detection uses
the same resolver.
