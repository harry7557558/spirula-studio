# Aligning a reconstruction with a laser scan

A laser scan (E57, LAS, PLY, or an XGRIDS capture's `.xbin`) is an input of
the dataset screen, next to photos and videos, and there may be several. The
reconstruction runs as usual; then `spirula lidar` moves it into the scans'
frame with a similarity -- rotation, translation and scale, any extrinsics --
and writes it back as
one COLMAP model whose seed points are the scans', each with a track of the
images that see it. Depth and normal maps are rendered from the scans in
place of the monocular geometry step. When the scans' own photographs are
the only images, no reconstruction runs at all (below).

Code: `data/PointCloudFile.h` (the reader; `data/XbinReader.h` for a
capture), `app/LidarAlign.h` (the fit and the scans' frames),
`app/LidarDataset.h` (the dataset), `app/cli/lidar_main.cpp` (`spirula
lidar`), `app/gui/LidarStep.h` and `app/gui/LidarSources.cpp` (the GUI).

## The pipeline

```
scan.e57 ──► photographs + their scan poses ─┐   (anchors.json)
scan.las ──► views rendered from where the   ├─► SfM ─► spirula lidar ─► sparse/0 in the scan frame
             scanner stood (no photographs)  │          (fit, merge,        + depths/ normals/
photos / videos ─────────────────────────────┘           seeds, tracks)
```

1. **Anchors.** An image whose pose in the scan frame is known. They come
   from the scan's own photographs (`extract_e57_anchors`), or, for a scan
   that has none, from views of the scan rendered where its scanner stood
   (`render_anchor_views`: E57 scan poses, a PLY `camera` element, or a
   `<name>.trajectory.las` beside the cloud, one position every 2 m). They
   are recorded in `<dataset>/lidar/anchors.json` (OpenCV camera-to-world in
   the frame of the file named by `scan`, plus the COLMAP camera for
   photographs). In the GUI, photographs become inputs of their own under
   `lidar/photos/`, so they are imported and masked like any other photo
   folder; rendered views are written straight into `images/scan_views/`
   after the frames step -- for a scan without photographs, unless another
   scan in its frame has them.
2. **SfM** runs over everything, except when the scans' photographs are the
   only images and the scans share a frame by their files (below): then they
   keep the scanner's poses and the step is `--scanner-poses` directly ("Use
   the scanner's camera poses as they are", on by default; untick it for a
   scanner whose camera poses are worse than a reconstruction would be). A
   reconstruction that fails is not the end of a run whose scan has
   photographs either: those keep the scanner's poses.
3. **The fit, per model** (`sparse/0`, `sparse/1`, ...): each model with an
   anchor is fitted on its own, so a capture that fragments still lands whole.
   - Rotation: the chordal mean of `R_scan R_model^T` over the anchors that
     agree with the best-supported one to 3 degrees.
   - Scale and translation: RANSAC over pairs of anchors standing further
     apart than the inlier tolerance, then least squares with the rotation
     held. Anchors that all stand in one place (one station's faces) leave the
     scale open, and it comes from the scan's depth (`scale_from_depth`): the
     median, over the model points the anchor images see, of the scan's range
     along each point's ray over the model's. ICP from a guessed 1 lands
     wherever it lands, unchecked.
   - Point-to-plane ICP of the model's points (tracks of 3+) against the scan
     thinned to 2M points: a 7-parameter Gauss-Newton step, Geman-McClure
     weights whose sigma shrinks from 3x the median distance to
     max(1.5 voxel, 1 cm, 3 MAD). Refused when it moves the anchors further
     than 5% of their spread (and 5x their own disagreement).
4. **One model** in the scan frame: the models merged with fresh ids, rendered
   views dropped, and every scan photograph the reconstruction did not place
   added at its scanner pose. An image whose own points mostly miss the
   scan's surface (under half within 5% + 20 cm, of 20 or more with scan
   depth behind them) is left out after the maps are rendered: the
   reconstruction misplaced it, and the fit cannot move part of a model.
5. **Seeds with tracks.** The scan thinned by voxels to `--points` (500k).
   For every image the scan is rendered into a 1600 px depth map through the
   image's real lens (the parser's camera, distortion included); a seed is
   *seen* by the image when it lies on that depth to 3% + 5 cm. Each seed
   keeps 12 of the images that see it, chosen by a hash so they spread and a
   rerun keeps the same ones; their pixels are appended to `images.bin` as
   observations. The model's own points survive only where the scan has
   nothing (the majority of their observations land on a pixel with no scan
   depth): sky, and what is past the scanner's range.
   A scan without colour (XGRIDS' LAS is point format 1) takes its colours
   from the photographs, through the masks.
6. **Depth and normals** from the same maps: 16-bit millimetres and
   `127.5 + 127.5 n`, ray depth when most cameras are split into faces
   (`resolve_ray_depth`'s vote), as PR 101's E57 converter wrote them. They
   are blank wherever the image's mask takes it out (to the edge of every map
   pixel a masked pixel touches), and no scan point is seen through a masked
   pixel: a passer-by in front of a wall is not given the wall's depth.

## Several scans

Scan files are in one frame when their files say so: `scans_share_frame`
holds them to it when no two put a scanner in one place and at most one has
no station of its own (a registered export like Oxford's 19 RTC360 files
places every setup apart; two mobile scans, or two unregistered setups at
their own origins, do not). `--scan-frames shared|separate` overrides it.

Files that are not are placed through their images, before the fit:

1. `group_scan_frames`: in each model, the scans whose anchors one anchor fit
   holds (a majority of each scan's) are in one frame; the rest are fitted
   again. Scans no model saw are left out, unless all the others turned out
   to be in one frame -- then nothing says they are not, and they join it.
2. The frame with the most registered anchors is the dataset's. Each model
   is fitted to that frame's anchors and clouds alone (anchors, then ICP).
3. `place_frames`: every other frame through the fitted model holding most
   of its anchors, at that model's scale -- so the move is rigid, as two
   metric scans are related. Clouds and anchors are moved into the first
   frame, and the run goes on as for one scan. `lidar_alignment.json`'s
   `scans` records where each cloud went.

`sparse/0/gauge.txt` says `oriented 1`, `metric 1`.

**Files.** The result is built in `sparse.lidar_tmp/` and only swapped in once
complete, so a run killed half way (the GUI cancels by killing the child)
leaves the dataset as it found it. Then the numbered models it started from
are moved to `sparse_unaligned/` (never deleted; a rerun reads them there),
and the result becomes `sparse/0`. A model written flat into `sparse/` (an
export's) is read where it is and never touched; `sparse/0` goes beside it,
and the parser prefers it. A model it cannot read (COLMAP text) is refused and
left alone. `sparse/0/lidar_alignment.json` holds the transforms, a signature
of the inputs (a rerun with the same ones is skipped) and a checksum of the
`images.bin` it wrote, so a new reconstruction that overwrites `sparse/0` but
leaves the marker is recognised. Models that share images (the mapper allows
20) keep each image once, in the larger model. A reconstruction that fails in
the GUI runs the step with `--scanner-poses`, so an earlier run's model is not
taken for it. Every case above was exercised on the FJD workspace
(kill, rerun, model replaced, text model, duplicated model, flat export).

## Formats

- **E57**: PR 101's reader. Scan poses are stations when there are several or
  one is not the identity (a mobile scan's single pose says nothing).
- **LAS** 1.0-1.4, point formats 0-10, uncompressed. Colour is 16-bit by the
  standard but writers disagree on scaling, so the shift is taken from 64
  spans of the file. LAZ is refused with a message.
- **PLY**, ascii or binary, streamed. A `camera` element (PCL, ETH3D) gives
  the scanner's position; NavVis writes one at the origin, which is where its
  walk began. Dropped on the dataset screen, or together with
  photos, videos or other scans, a PLY whose header is neither a splat's nor
  a mesh's is a scan; alone anywhere else it opens in the viewer. A splat's
  PLY is refused as a scan from "Add LiDAR scan..." and the path box too, with
  the reason: it is a finished reconstruction, with its colour in `f_dc_*`
  and no scanner position, so no view of it is rendered and the run would
  end in "No model could be aligned" (an LCC Studio `ply-result` export is
  one).
- **XGRIDS** (LCC Studio `developer_data/`): `raw/` (the original fisheye and
  OpenCV lenses) and `perspective/` (pinhole crops) are COLMAP datasets in the
  LAS's frame. `nCore_data/` holds the same two under `colmap/`, with the LAS
  under `lidar/`. Both are found by layout, so a renamed copy works too.
  "Add dataset..." or a drop of the export, either of the two, or a folder
  inside one takes `raw/` in place, adds the LAS, ticks "already in the
  scan's frame" (`--mode keep`) and reads its masks flipped: they are white
  where the operator is.

- **XGRIDS capture** (`.xbin`, PortalCam, the file the scanner writes before
  LCC Studio sees it): the device's points, its trajectory, its factory
  calibration and the H.264 frames of its four cameras, in one container
  (`data/XbinReader.h`). The points read as a scan, each placed by the
  trajectory at its own time: the 10 Hz poses for position, and for attitude the
  poses with the 200 Hz gyro (`/imu`, 1e-4 rad/s per count) turning between
  them, its gap to the next pose spread over the interval. The spread also
  takes out the gyro's bias: subtracting `Bg` of `imu.yaml` changed nothing.
  Against LCC Studio's 200 Hz trajectory of a 975 s capture, the rotation
  inside an interval was off by 0.14 deg (median) interpolated and by 0.012 deg
  gyro-filled. On a 300 s capture the deskewed map has 2 % fewer occupied 5 cm
  voxels than with interpolation, and the camera poses at the frames' times
  differ from the interpolated ones by 0.17 deg (median), 0.42 deg at the 90th
  percentile. An interval the gyro does not cover, or whose gap exceeds 2 deg,
  keeps the interpolation, as does a capture without a readable `/imu`. The
  photographs are extracted like an E57's (`app/XbinAnchors.h`: ffmpeg decodes
  each camera's frames, which are all intra-coded), with poses from the same trajectory and
  their own lenses -- two `OPENCV_FISHEYE`, two `OPENCV` -- so "Use the
  scanner's camera poses" needs no reconstruction. "Add LiDAR scan..." takes
  the `.xbin`, and a capture folder dropped or added as a dataset is taken as
  its `.xbin`. Each camera has a line under the scan's row: a checkbox takes
  it (`camera_3` starts unticked, because LCC Studio takes no frame of it
  either), and a rate box in the video rows' rate column thins its frames
  (`FrameRate`: the first frame of each 1/rate s, so cameras at one rate keep
  the same instants). The camera's own rate comes from the index (fields 4
  and 5, its first and last frame time). Measured on one 110 s indoor capture: the device's poses,
  deskewed, gave a sharper map (occupied 5 cm voxels 137k) than KISS-ICP on the
  raw sweeps (266k, 29 cm from the device's path), and the photographs'
  colours agree across frames under `inverse(extrinsic_camera_lidar)`, not under
  the file as written. The field layout is measured, not documented. The
  device message after the file header names the device (model, a device
  code -- `AA` on the PortalCam -- and firmware). A table at byte 4096 names
  each topic (`/imu`, `/lidar`, `/camera_left_main/h264` ...), and the index
  and each chunk header name their topic by its id, so the reader takes each
  stream by name. No key in the file joins a camera topic to its
  `camera.yaml` section, so the reader keeps a measured table for each device
  code and model, the PortalCam's alone so far. Its firmware is not checked:
  another firmware of the same device is read with the same table. Every
  block ends in the CRC-32 of its stored bytes, which the reader checks.
  When the file opens, the reader refuses a device message that names no
  device or a device that was not measured, a topic the device does not
  have, and a `camera.yaml` section the device does not have. A damaged block
  is refused when it is read, except in `/imu`, which then leaves the poses
  interpolated. When the photographs are extracted, it refuses a frame whose
  size is not its calibration's. These refusals name the device as far as the
  message does.

  The frames are named by their frame counter (`000123.jpg`), which all four
  cameras share: one instant has one name in each camera folder, at most
  51 µs apart, which is what a rig groups on. The reconstruction gets each
  camera's complete factory lens (manifest `params`), because the mapper holds
  the principal point at the image centre otherwise, and the pinholes' is
  241 px from it (7 deg of tilt).

  A capture alone is not placed by its reconstruction but by its own poses,
  refined (`--refine`, `app/AnchorRefine.h`): the run's verified matches are
  triangulated with the device's poses (`sfm/map/KnownPoses.h`), then one
  bundle adjustment holds each instant as one rig pose, near the device's
  (2 cm and 0.5 deg priors on its centre and attitude), turns the factory rig
  only, and refines each camera's whole lens, principal point included when
  the camera has 20 placed photos or more (`pp_min_images`). Every
  photograph keeps a pose and no fit to the scan is needed. Measured on one
  300 s capture (888 photos) against 12,000 tracks of a free reconstruction
  from the same matches (854 photos in one model, factory lenses in its
  manifest): the device's poses as they are reproject at 1.9 px median but
  22 px at the 90th percentile; refined, 1.19 / 3.7 px. With the principal
  point held it was 1.57 / 5.0 px, and with 24 photos for each camera
  0.97 / 2.6 px held against 0.53 / 1.6 freed. The free model gives
  1.07 / 2.5 px on the same tracks, which are its own, so it is favoured. All
  these reprojection errors come from one sample of 12,000 tracks.
  `BundleOptions` holds the principal point for several lenses because its
  shift is almost a turn of the lens, which makes each lens drift its own way
  in a model's relative orientation. Here every pose leaves with its own lens,
  so a principal point's drift turns the pose with it and the rays stay where
  the photos put them. Each camera's mean turn against the device changed by
  close to its principal point's shift over its focal: `camera_0` 0.36 deg
  (predicted 0.46), `camera_1` 0.82 (0.83), `camera_2` 0.39 (0.40),
  `camera_3` 0.43 (0.42). So a refined pose is right only with its own
  refined lens. An instant that sees no point stays on its priors, at the
  device's frame, so its photographs keep the device's poses and, to match
  them, their factory lens. The refinement moved the photos 6.6 cm and
  turned them 0.97 deg (median), the lens trade included; with the principal
  point held it was 5.7 cm and 0.66 deg. Next to other photographs the
  capture keeps the joint reconstruction and its fit.

## Measurements (2026-10-04, RTX 5070)

| capture | anchors | anchor fit | after ICP | notes |
|---|---|---|---|---|
| FJD Trion P2 E57 (51 panoramas) + X5 `.insv` every frame (224) | 51 photos | 0.16 deg / 3.5 cm | 1.3 cm to the surface | 62 s, 1.3 GB, 275 depth maps |
| FJD LAS (no photos) + the same `.insv` | 7 of 32 views on `trajectory.las` | 0.16 deg / 1.1 cm | 0.8 cm | insv poses within 2.2 cm / 0.19 deg of the row above |
| ETH3D courtyard, DSLR + merged FARO PLY (no photos) | 32 of 32 views | 0.14 deg / 2.0 cm | vs ground truth 3.0 cm / 0.11 deg | best rigid fit of that model: 3.2 cm |
| ETH3D office, same | 12 of 32 views | 0.24 deg / 1.1 cm | vs ground truth 1.0 cm / 0.45 deg | best rigid fit of that model: 0.75 cm / 0.52 deg |
| Leica BLK360 E57 alone (30 faces) | SfM 0/30 | -- | -- | every face at its scanner pose; 76% of seeds tracked |
| Matterport lobby E57 alone (54 faces) | SfM 2/54 | 0.11 deg / 1.1 cm | -- | 52 at scanner poses; 84% tracked |
| XGRIDS export (2138 images, LAS 83M points) | -- (`keep`) | -- | -- | 9 min 22 s (17 min 50 s before the LOD below), 2.8 GB |
| Oxford Spires: 525 handheld images + 19 RTC360 E57s (108 faces, 462M points) | 26 of 108 faces | 0.59 deg / 36 cm | 12.8 cm | 82 faces left out (below); handheld images' colour correlation with a station's scan 0.58, the dataset's own reference poses 0.08 |
| Oxford Spires: the 19 E57s alone, GUI (2026-10-05) | -- (scanner's poses) | -- | -- | no SfM; 108 of 108 faces, 84% of seeds tracked, 46 s for the step |
| NavVis M6 (LaMAR HGE session): 1884 rig images + its 127M-point PLY, GUI (2026-10-05) | 6 of 16 views, all at the one `camera` | scale from depth 24.875 (best Sim3 to the truth: 24.849) | 2.7 cm | model 1 (539 images) half misplaced by the SfM: 206 of its 253 wrong images left out, none of its 280 right ones; the kept are 1.9 cm from the session's poses (median). Model 0 (1046, consistent) has no anchor and is lost |
| FJD LAS + a copy turned 35 deg and moved 8 m, as PLY, with the insv model (2026-10-05) | 4 + 3 of 32 views | 0.30 deg / 2.3 cm | 0.7 cm | the copy placed through its 3 views to 0.36 deg / 4.9 cm of the truth |

**Partitioning** the XGRIDS export into four (`spirula partition split
--parts 4`): with the export's own SfM tracks (64k points) the view graph is
159 pieces and no split is made; with the scan's points and their tracks
(527k, 1.27M covisible camera pairs) it splits into 4 parts with 32% of the
covisibility cut, in 4.5 s.

**The scanner's own poses on FJD**: the 11 X5 panoramas in the second E57,
reconstructed with the insv frames and aligned by the 51 internal-camera
anchors only, land 2.0 cm / 0.32 deg (median) from the poses FJD recorded for
them -- its camera-to-scanner calibration is sound.

**Training** (7k steps, every 8th image held out, the FJD E57 + insv dataset):
held-out PSNR 17.27 / SSIM 0.533 with the scan's seeds, normals and depth,
17.37 / 0.526 with the same images on the unaligned SfM model and its own
points -- within this capture's run-to-run noise (about 1 dB).

Pose truth for FJD: SfM of the 51 panoramas alone agrees with their E57 poses
to 0.15 deg / 2.2 cm (measured 2026-09-28), and the photo-anchored and
LAS-anchored routes agree with each other as above. Overlays of the scan's
colours on insv frames line up (AC unit, pipe, brick courses).

## What was tried

- **Centres-only Umeyama** on the anchors: 1.9 deg off the anchors' own
  rotations on the FJD walk, which is nearly a line -- the rotation about it
  is free. Pose-based (rotations first): 0.16 deg. Kept the latter.
- **The misplaced-image check's tolerance** (NavVis model 1 against the
  session's poses; FJD's two runs, all placed right): 3% + 5 cm, the seeds'
  test, drops 216 of 253 wrong images but 5 of FJD's 15 right ones (its X5
  cloud is sparse); 5% + 20 cm at half drops 206 and no right one anywhere.
  Spreading "misplaced" along covisibility found 10 more; not kept.
- **ICP convergence basin** (ETH3D courtyard, from the ground-truth Sim3
  perturbed): converges from ~5 deg / 10% scale / 10% of the scene's size,
  fails at ~10 deg. Anchors land well inside it; ICP alone from nothing does
  not.
- **Rendered views inside the SfM bend it.** With 32 renders in the
  reconstruction, the best rigid fit of the DSLR cameras to ground truth is
  3.2 cm (courtyard) and 0.52 deg (office), against 1.0 cm / 0.08 deg for the
  DSLR images alone. Using the renders only to place the model, then moving
  a photos-only model there (by shared cameras) and running ICP: 0.86 cm. Not
  done in the tool: it needs a second SfM, or the renders localized into the
  first. **Bundle-adjusting the joint model again without the renders** (`sfm
  ba`, Huber) does not help (3.04 cm / 0.42 deg): it stays in the same basin.
- **Refining an XGRIDS export with ICP** (`--mode refine`): moves it 0.38 deg,
  3 cm, scale 1.0004. Whether that is better cannot be told: the LAS has no
  colour, and intensity against the photographs correlates at 0.23 either way
  (better in 22 of 43 images). `keep` is the default for exports.
- **Oxford's 36 cm is anchor noise, not drift.** Faces of one station share
  an optical centre, yet the SfM placed them up to 0.5 m apart: they matched
  the small handheld images with 23-810 points each. A per-camera rigid ICP
  of each camera's neighbourhood against the scan (prototype) found nothing
  to correct (1 mm median). So the photographs the reconstruction did not
  place are put at their scanner poses only when the anchors agree with the
  fit to 5 cm and 0.5 deg; otherwise they are left out and the run says so.
- **Level of detail in the depth renderer**: a cell whose points are closer
  than 0.7 px is drawn from every k-th point. Maps are identical on FJD
  (median difference 0 mm, 0.15% of pixels over 5 cm).

## Not done yet

- **A model with no anchor is lost**, however good. On NavVis the views at
  the PLY's one camera registered only into the second model; the largest
  (1046 images, 99% of it held by one similarity to the truth) was dropped.
  A trajectory beside the cloud (`<name>.trajectory.ply`) gives views along
  the walk; ICP from nothing does not converge (below). The check against
  the scan also keeps 47 of NavVis's misplaced images: a chunk whose points
  land on a surface, only the wrong one.

- **Registering clouds to each other.** A scan in a frame of its own is
  placed only through its images in the reconstruction, to their accuracy
  (the row above); one whose images the reconstruction did not place is left
  out. Cloud-to-cloud ICP would refine the first and place the second:
  `place_frames` is where its placements would come from, and the frames'
  moves are applied in one place in `write_lidar_dataset`. The model's own
  ICP afterwards moves the model, not the frames placed through it.

- **Refinement from the scanner's poses as priors** (the popular request):
  `anchors.json` is already the per-image pose file it needs. It would be a
  `PriorSource` (`sfm/core/PriorSource.h`) giving each anchor a `PriorCentre`
  and `PriorRotation`, so the poses constrain BA rather than only placing the
  result; TLS cube faces, which SfM cannot reconstruct alone, are where it
  matters.
- Localizing rendered views into a photos-only model (above), and a BA with
  the scan as a constraint (model points pulled onto its surface), which would
  also correct the reconstruction's own drift rather than only its pose.
- A colourless scan cannot render useful views (intensity is not what a photo
  sees), so it needs photographs or `keep`.
- The COLMAP engine of the dataset screen does not run the step.
