# Detection per lens — plan

Not started. Today every image is detected under one `--max-image-size` (the
longest edge) and one feature limit, whatever its lens. On a capture that
mixes a 360 camera with a pinhole one that leaves the two at very different
angular resolutions.

## The measurement that prompted it

A 678-image indoor capture at `--quality extreme` (2400 px, ALIKED 8192):

| | pinhole (opencv) | 360 (equirectangular) |
|---|---|---|
| source | 4016 x 6016 | 11904 x 5952 |
| field of view | about 64 x 46 degrees | 360 x 180, about 14x the solid angle |
| px per degree at source | about 93 | about 33 |
| **px per degree at 2400 px** | **about 37** | **about 6.7** |
| mean features per image | 6372 | 6148 |
| **features per square degree** | **about 2.2** | **about 0.15** |

The same wall is about 5.6x smaller in a 360 image than in the pinhole one,
which a learned detector covers only partly, and the 360's features are spread
15x thinner. All 37 360 images still aligned; what they lose is matches to the
pinhole images and the precision they add per direction.

## Option A: size and feature limits per lens

The manifest already carries one `cameras:` entry per folder prefix
(`sfm/core/Manifest.h`). Two optional keys per entry:

- `max_image_size` — or, better, `px_per_degree`, from which the size follows
  from the lens's field of view, so a 360 and a 64-degree lens can be asked
  for the same angular resolution in one setting.
- `max_features` — or a scale on the run's limit by solid angle.

The extractor reads them per image (`ImageLoadOptions` is already per path).
The GUI would show them per input, next to the lens it already picks.

Limits: LightGlue's attention is quadratic in the feature count, so a 360 at
the pinhole's density (about 90k) is out of reach on 8 GB; the cap has to stay
near the matcher's budget, which caps how far this option can go. A changed
limit re-detects those images and invalidates `matches.bin` once
(`DatasetPlan` must list the new keys in `model_fields`).

## Option B: equirectangular stills as a rig of pinhole views

What `app/Pano360` already does for 360 video: a ring of pinhole faces, each
resampled from the panorama (`Pano360Remap`), and registered as one rig so the
faces keep their fixed relative rotations (`sfm-rig-constraints.md`). For a
still, the source is a plain equirectangular image instead of a GoPro packing:

1. A `Pano360Layout` for "equirectangular still" (no tracks, no seam strip),
   and `pano360_views` for it — the same face set video gets.
2. Dataset prep writes the faces under one folder per face, as the video path
   does, and `build_manifest` (`SfmRunner.cpp`) emits the rig.
3. The face size follows the pinhole cameras' px per degree, capped by the
   source's 33.

Each face then has a pinhole detector's own geometry, about the other camera's
angular resolution and its own feature limit, which fixes the scale gap rather
than adding features. Costs: about 6-10 images per panorama to detect and
match (pair selection inside a rig is cheap: the faces' relative poses are
known), and training then sees faces, not panoramas, unless the model is
written back in the panorama's frame — decide that before building.

## Order

1. Option B for equirectangular stills, reusing Pano360 end to end.
2. Option A only if a capture needs it after B, for mixed pinhole lenses of
   very different focal lengths.
