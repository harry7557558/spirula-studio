# Named checkpoint PLY and SuperSplat export

Each saved checkpoint contains exactly two PLY files:

```text
outputs/example/step-000030000.ckpt/
  splat.ply
  example.ply
  state.tar
```

`splat.ply` remains the canonical native model for Spirula, camera-based
meshing and existing tools. `<run>.ply` is the delivery, named after the run
folder. No PLY copies are published at the run root. For a run called `splat`,
the delivery is `splat_export.ply` to avoid overwriting the canonical model.

With `--export-y-up false` (default), the named delivery uses native
coordinates. Hard links avoid duplicate storage where supported, with a copy
fallback. Enable `--export-y-up true` or **Export Y-up PLY** to rotate this
same named delivery for SuperSplat instead of creating another file.

The desired viewer transform is Rx(-90 degrees), mapping (x,y,z) to
(x,z,-y). PlayCanvas applies Rz(180 degrees) when importing PLY, so the
export bakes its inverse after that transform: (x,y,z) -> (-x,-z,-y).
The composition on import is Rx(-90 degrees), keeping native +Z upright.
Gaussian orientations, SH and spherical-beta lobe axes rotate together via
`SplatTransform`. Export reads bounded chunks and publishes atomically.

Training, cameras, scene_transform.json, gauge.txt and splat.ply retain
their original coordinates. Use splat.ply for camera-based meshing; the named
Y-up delivery follows SuperSplat's file convention. Resume uses state.tar.

The shared model resolver prefers canonical splat.ply and newest checkpoints.
Named-only folders require one unambiguous splat PLY. GUI folder detection
uses the same resolver. Existing files from older versions are not deleted;
use a fresh run directory for the two-file output layout.
