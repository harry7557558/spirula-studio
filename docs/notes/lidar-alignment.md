# Aligning a reconstruction with a laser scan

A laser scan (E57, LAS or PLY) is an input of the dataset screen, next to
photos and videos. The reconstruction runs as usual; then `spirula lidar`
moves it into the scan's frame with a similarity -- rotation, translation and
scale, any extrinsics -- and writes it back as one COLMAP model whose seed
points are the scan's, each with a track of the images that see it. Depth and
normal maps are rendered from the scan in place of the monocular geometry step.

Code: `data/PointCloudFile.h` (the reader), `app/LidarAlign.h` (the fit),
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
   are recorded in `<dataset>/lidar/anchors.json` (OpenCV camera-to-world,
   plus the COLMAP camera for photographs). In the GUI, photographs become
   inputs of their own under `lidar/photos/`, so they are imported and masked
   like any other photo folder; rendered views are written straight into
   `images/scan_views/` after the frames step.
2. **SfM** runs over everything. A reconstruction that fails is not the end of
   a run whose scan has photographs: those keep the scanner's poses.
3. **The fit, per model** (`sparse/0`, `sparse/1`, ...): each model with an
   anchor is fitted on its own, so a capture that fragments still lands whole.
   - Rotation: the chordal mean of `R_scan R_model^T` over the anchors that
     agree with the best-supported one to 3 degrees.
   - Scale and translation: RANSAC over pairs of anchors standing apart, then
     least squares with the rotation held. Anchors that all stand in one place
     (one station's faces) leave the scale open; ICP then starts at 1.
   - Point-to-plane ICP of the model's points (tracks of 3+) against the scan
     thinned to 2M points: a 7-parameter Gauss-Newton step, Geman-McClure
     weights whose sigma shrinks from 3x the median distance to
     max(1.5 voxel, 1 cm, 3 MAD). Refused when it moves the anchors further
     than 5% of their spread (and 5x their own disagreement).
4. **One model** in the scan frame: the models merged with fresh ids, rendered
   views dropped, and every scan photograph the reconstruction did not place
   added at its scanner pose.
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
   (`resolve_ray_depth`'s vote), as PR 101's E57 converter wrote them.

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
  the scanner's position.
- **XGRIDS** (LCC Studio `developer_data/`): `raw/` (the original fisheye and
  OpenCV lenses) and `perspective/` (pinhole crops) are COLMAP datasets in the
  LAS's frame. "Add dataset..." on the export, `developer_data/` or `raw/`
  takes `raw/` in place, adds the LAS, ticks "already in the scan's frame"
  (`--mode keep`) and reads its masks flipped: they are white where the
  operator is.

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
