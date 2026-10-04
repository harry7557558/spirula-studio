# Fixed poses: points for a camera solve you keep

`spirula sfm auto IMAGES --output WS --poses MODEL` takes the cameras and poses
of a COLMAP model as given and only adds points. It exists for matchmove work:
a solve from SynthEyes, 3DEqualizer or PFTrack is authoritative because CG is
already aligned to it, and its tracker cloud (a few hundred points) is a poor
seed for training. The result is an SfM-style cloud -- every verified feature
track that triangulates -- in exactly the frame the solve is in. It is not a
multi-view stereo cloud.

## What runs

| stage | with `--poses` |
|---|---|
| extract | as usual |
| pair, match, verify | as usual |
| incremental mapping | replaced by `Mapper::triangulateFixed` |
| `finishModels` (merge, audit, grow, global BA) | not run |
| `fixGauge` (upright, ground level, centring, metric scale) | not run |
| `recolorPoints` | as usual |
| `splitCamerasBySize` | not run |
| `writeModels` | replaced by `writeFixedModel` |

`triangulateFixed` adopts the model, then runs the mapper's own
retriangulation (`completeAndRetriangulate`: new tracks from every image,
completion, merging) until a pass adds nothing or three passes have run, then
`filterPoints` once. Every one of those touches points and tracks only; there is
no bundle adjustment, no registration and no de-registration on the path.

Verification still settles its own camera grouping and focal guess, and the log
says so. Rectilinear pairs are verified in pixels, where the focal plays no part;
nothing from that stage reaches the written cameras or poses.

## What comes out unchanged

- `cameras.bin`: the input file, copied.
- `images.bin`: each image's id, camera id, name, and the seven doubles of its
  pose, in the input's order. The pose is kept as the file's own bytes and never
  goes through arithmetic: a quaternion -> matrix -> quaternion round trip moves
  the last bits, renormalizes a quaternion that was not unit length, and can
  flip its sign.

What does change: each image's `points2D` (this run's keypoints; the tracker
observations an export carries are dropped, since they index the exporter's 2D
lists and not these features) and `points3D.bin`. No `gauge.txt` is written,
because nothing about the frame was measured; a stale one is removed.

## The check that makes it a guarantee

After writing, the run reads the input model again from disk and compares it
with what it wrote: `cameras.bin` byte for byte, and every image's id, camera,
name and pose bytes. Any difference removes the written model and exits 2.
Everything else above only keeps that check passing; it is the check that would
catch a later change to the pipeline that moves a pose.

## Refused

Usage errors, before anything runs:

- `--camera-model`, `--focal`, `--distortion` (also per folder, also from a
  manifest), `--rig` and manifest rigs: they would change a camera or a pose.
- `--mapper`, `--metric-gps`, `--metric-positions`, `--level`, `--orient`,
  `--sensor-gauge`, `--exif-attitude`, `--telemetry`: only the mapper or the
  gauge fix reads them, and neither runs.
- An `--output` whose `sparse/0` is the `--poses` model itself.

Run errors, exit 1:

- A text model (`cameras.txt`): save it as `.bin`.
- A camera model with no equivalent here (FOV, RAD_TAN_THIN_PRISM_FISHEYE).
  SIMPLE_RADIAL, SIMPLE_RADIAL_FISHEYE and RADIAL_FISHEYE are read as RADIAL
  and OPENCV_FISHEYE with the missing coefficients at zero, which is the same
  projection; the file is still copied as it was.
- A posed image that is not in the image folder (checked before extraction;
  names match by stem, so `frame_0001.jpg` finds `frame_0001`).
- An image whose size is not its camera's.

Images in the folder that the model does not pose are extracted and matched
but get no pose and no points; the run says how many.

## In the GUI

The training screen has "Recompute Sparse Pointcloud" between Change...
and the region of interest row when the open dataset holds a binary COLMAP model where the
trainer would look for one (`find_colmap_poses`). It opens a short list of
settings (quality, features, the two counts, and the dataset's masks when it has
some) and runs `spirula sfm auto --poses` on that model as a child, with
`densification/` inside the dataset as its workspace. The panel is
`app/gui/RecomputePanel`.

When the run succeeds, its `points3D.bin` and `images.bin` replace the model's
own, which are renamed `points3D.bin_original` and `images.bin_original` first.
`images.bin` has to go with the points: the new tracks index this run's
keypoints, not the exporter's 2D lists. Its poses are the input's bytes (the
check above). `cameras.bin` is not touched. `densification/` is then removed,
and the training screen re-reads the dataset.

A second recompute keeps the first originals. It knows the files in the model
are its own from `.spirula_recompute`, which records their sizes and times; a
solve exported over them since does not match, so that export becomes the new
`*_original`. "Restore the Original Points" moves the originals back while the
model still holds a recompute.

## Not done

- Pairing and verification that use the known poses (an epipolar test from
  the given relative pose instead of RANSAC on a fundamental matrix).
- Text models.
- A points-only refinement after triangulation: no bundle adjustment of any
  kind runs, by request.
