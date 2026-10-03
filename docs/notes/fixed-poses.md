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

The New Dataset screen shows "Keep the imported cameras (add points only)"
beside "Reconstruct again" when the output folder holds a binary COLMAP model
where the trainer would look for one (`find_colmap_poses`). Ticked, the
reconstruction step runs `--poses` on that model with `dense/` inside the
folder as its workspace -- features, matches and `dense/sparse/0` -- and the
imported model is never written to. The lens, camera sharing, mapper, sensor
and bundle adjustment settings are greyed out, since nothing reads them, and
the record (`DatasetPlan.h`) compares only the settings that run does read.
Depth and normal maps are not redone for it: the cameras did not move.

Opening the folder for training, from that screen or from the home screen,
reads `dense/sparse/0` (`colmap_recon_dir`) as long as it is newer than the
model it was made from and no other model was chosen by hand; the log says
which. A solve exported again afterwards is newer, so it is read instead,
with a line saying `dense/` is out of date -- training on points made for the
previous solve is the failure this rule exists to prevent. `spirula train`
has no such rule; pass it `--colmap-recon-dir dense/sparse/0`.

## Not done

- Pairing and verification that use the known poses (an epipolar test from
  the given relative pose instead of RANSAC on a fundamental matrix).
- Text models.
- A points-only refinement after triangulation: no bundle adjustment of any
  kind runs, by request.
