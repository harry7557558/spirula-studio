# Checkpoint PLY and SuperSplat export

Each save writes one PLY and one TAR directly into the run folder:

```text
outputs/example/
  config.json
  example.ply
  example.tar
```

With `save_only_latest_checkpoint=false`, each pair includes its step:
`example_step-000030000.ply` and `example_step-000030000.tar`. There are no
PLY copies, hard links or per-step checkpoint directories. The latest-only
mode replaces the named pair and prunes retained step pairs and legacy
checkpoint directories after the new save succeeds.

With `--export-y-up false` (default), the PLY uses native coordinates.
Enable `--export-y-up true` or **Export Y-up PLY** to save that same file for
SuperSplat. Geometry, SH and spherical-beta lobe axes rotate together.

The desired viewer transform is Rx(-90 degrees), mapping (x,y,z) to
(x,z,-y). PlayCanvas applies Rz(180 degrees) when importing PLY, so the
export bakes its inverse after that transform: (x,y,z) -> (-x,-z,-y).
The composition on import is Rx(-90 degrees), keeping native +Z upright.

The header marks this convention with
`comment spirula_coordinate_system supersplat_y_up`. Spirula's shared PLY
reader reverses it while decoding, including when meshing or initializing a
new training run. Unmarked PLY files retain their existing interpretation.
Positions and quaternions use direct formulas; SH uses the nonzero terms of
fixed band matrices, and lobe axes use the same axis swap. No general
similarity transform or second file pass is needed.

Saving stages bounded GPU chunks and overlaps packing/device transfers with
disk writes. Reading overlaps disk reads with decoding. Training state in the
TAR, cameras, `scene_transform.json` and `gauge.txt` remain in native coordinates.
Resume reads the TAR directly, without a PLY conversion or loss of filtered
splats and optimizer state.

Run-folder resolution selects the greatest saved step from archive metadata;
an explicit TAR or paired PLY selects that checkpoint. Legacy `state.tar`,
`splat.ply` and `step-*.ckpt` folders remain readable.
