# Datasets

## Supported layouts

Three formats, auto-detected in this order: **Nerfstudio**, then **COLMAP**,
then **Metashape**.

The native parsers (`src/data/parsers/*Parser.cpp`) are the one
implementation, shared by the CLI trainer, the GUI and the WASM viewer.

| format | inputs | parser |
|---|---|---|
| COLMAP | `cameras`/`images`/`points3D` in `.bin` or `.txt` | `ColmapParser.cpp` |
| Nerfstudio | `transforms.json` + a PLY point cloud | `NerfstudioParser.cpp` (PLY reader lives here) |
| Metashape | camera-export `.xml` + `.ply`, optionally a `.psx` project for filename disambiguation | `MetashapeParser.cpp` (XML via `app/Xml.h`, zips via `external/miniz`) |

Default subdirectory names: `images/`, `masks/`, `depths/`, `normals/`.
The COLMAP reconstruction directory is auto-detected over
`{sparse/0, colmap/sparse/0, sparse, colmap, .}` unless `recon_dir` is set.

## Camera models

Perspective (with full radial / tangential / thin-prism distortion),
equidistant and equisolid fisheye (including >180° FOV as produced by 360
cameras), and equirectangular/spherical. See `src/core/CameraModel.h` — it is
plain C++17 with no CUDA dependency, which is why the WASM viewer can reuse
it.

COLMAP writes 18 camera models and `ColmapParser.cpp` accepts every one.
`EQUIRECTANGULAR` (id 17) is the spherical one: its params are `(w, h)`
rather than a calibration, because the image *is* the calibration. It reaches
the same `CameraModelType::EQUIRECTANGULAR` as a Metashape `spherical` sensor,
with the same convention — +Z forward at the image centre, azimuth wrapping at
the left/right edge — so the two formats describe an identical camera and
`bake_post_split` treats them identically. Note the engine's canonical panorama
intrinsics assume a 2:1 (360°×180°) image; the parser warns when one is not.

## Lens distortion tiers

Distortion is a separate axis from the camera model, and a COMPILE-TIME one:
a template argument in CUDA, a `kDistortion` specialization constant on Vulkan.
The three tiers, cheapest first, are `None`, `OpenCV` (`k1 k2 p1 p2`) and
`ThinPrism` (`k1 k2 k3 k4 p1 p2 sx1 sy1`, COLMAP's `THIN_PRISM_FISHEYE`).
A slot index does NOT mean the same thing across tiers. `core/CameraModel.h`
is the one definition; `shaders/projection_utils.slang` is the one
implementation, generic over an `ICameraDistortion`.

There is deliberately **no rational tier**. OpenCV's 8-coefficient rational
lens (COLMAP `FULL_OPENCV`, where `k4..k6` divide) has a pole wherever its
denominator crosses zero, and twelve weakly-constrained parameters with it: a
93-image COLMAP run on that model diverged to `fy = 3.4 fx`, and `spirula sfm`
resets its runaway coefficients. Compiling it also cost one of eleven (model,
tier) pairs -- 29 of the 318 generated CUDA instantiation units, all on the
perspective arm. So it is handled like the other unrepresentable models:
fitted onto `ThinPrism` and re-distorted. A `FULL_OPENCV` camera whose
`k4..k6` are all zero is a plain polynomial and is read straight into
`ThinPrism`, with no fit and no resampling.

The parser picks the CHEAPEST tier that represents the source camera exactly,
so a PINHOLE dataset costs no distortion registers at all
(`camera_distortion_demote`). Only ten (model, tier) pairs are compiled —
EQUIRECTANGULAR carries no distortion.
`camera_distortion_is_compiled()` is that list, and it must stay in step with
`kCameraVariants` in `tools/codegen/generate_kernel_instantiation.py`,
`SS_FOR_EACH_CAMERA_VARIANT` in `src/kernels/projection/CameraVariants.cuh`
and the export list in `shaders/primitive_3dgs.slang`.

A `transforms.json` is ambiguous about what `k4` means -- OpenCV's first
rational DENOMINATOR term, or Kannala-Brandt's / Metashape's fourth RADIAL one.
An explicit `camera_distortion` key settles it (`MetashapeParser` always writes
one); failing that, a fisheye camera is Kannala-Brandt, `k5`/`k6` mean rational
on their own, and the mere PRESENCE of `b1`/`b2`/`sx1`/`sy1` -- keys a rational
camera never carries -- makes `k4` radial. That last rule is what a
Metashape-converted `transforms.json` needs: reading its `k4` as a denominator
is a different lens, not a small error.

`FULL_OPENCV`, `FOV`, `SIMPLE_DIVISION`, `DIVISION`, `EUCM` and
`RAD_TAN_THIN_PRISM_FISHEYE` have no exact tier, and neither does a Metashape
sensor skew (`b2`), which is an off-diagonal pixel term where every tier's
pixel map is diagonal. They are fitted onto a (model, `ThinPrism`) pair by
near-minimax regression (`data/DistortionFit.h`).

`fit_camera_auto` chooses the camera model from the source's MEASURED field of
view, not from its name: a COLMAP `FOV` lens is perspective at omega 0.3 and a
180-degree fisheye at omega 0.87, and forcing the second onto a pinhole target
puts `tan(85 deg) = 11.4` into a degree-8 polynomial and fails. It then walks a
coefficient ladder (all eight, no thin prism, no `k3`/`k4`, ..., none) until the
fitted distortion is invertible everywhere sampled. **It never fails**: a
dataset that took hours to reconstruct must not refuse to load over a lens
model, so the worst case is a plain fisheye and a warning, not an exception.

A fit closer than `dsfit::kExactFitPx` (0.1 px) is left alone -- the fitted
camera already reproduces the source to better than bilinear resampling can
resolve, so re-distorting would only cost VRAM and blur. In practice that
covers `FOV`, both division models, `EUCM` and a mild `FULL_OPENCV`;
`RAD_TAN_THIN_PRISM_FISHEYE` and a real `b2` do not reach it. When it is not reached, the source model goes into
`ParsedDataset::redistort`, which makes `bake_post_split` set `any_warp` even at
K = 1 so the images route through the warp path's staging and get resampled.

The resampling reads the TRUE source projection (`shaders/camera_source.slang`,
the one place those models live on device; `data/SourceCamera.h` is the host
mirror, and the two must agree exactly) rather than the fit -- going through the
fit would be a no-op. Source model ids 0..17 are COLMAP's own `CameraModelId`
values with COLMAP's parameter array verbatim; ours start at 1000 so COLMAP can
keep appending to its enum.

### The skewed source camera

Model 1000, `srccam::kSkewed`, is the one source model that is not a COLMAP
model: any supported camera plus a sensor skew. Its parameter layout, which
`NerfstudioParser` writes and both mirrors read, is

| slot | meaning |
|---|---|
| `p[0..3]` | `fx fy cx cy` (Metashape's `b1` already folded into `fx`) |
| `p[4]` | skew — pixels of `u` per unit distorted `y` |
| `p[5..12]` | coefficients: `ThinPrism` slot order, or `k1..k6 p1 p2` when `p[14]` is set |
| `p[13]` | base camera: 0 perspective, 1 fisheye, 2 equisolid |
| `p[14]` | 0 = polynomial radial, 1 = rational radial (`k4..k6` divide) |

The polynomial radial and the two fisheye mappings repeat `DistThinPrism` /
`fisheye_proj` / `equisolid_proj` in `shaders/projection_utils.slang`; a change
there has to be made here too. The rational radial lives only in this model and
in `source_project_full_opencv`, since no tier carries it.

A source model is sampled only where its image still grows outward as the ray
tilts off axis. Past that it has folded -- at the lens border for a polynomial
fisheye, well inside the frame for the division and unified models -- and two
directions share a pixel, so a warped face would be ringed by a mirrored copy
of the image instead of ending at the lens. Those rays are dropped, and the
synthesized FOV mask drops them by the same test. The fit domain stops earlier
still, where the radial rate falls below a quarter of its on-axis value: at the
fold itself no fitted distortion is invertible, so the coefficient ladder would
degrade a camera the tier otherwise reproduces to a fraction of a pixel.

Two paths, both gathering per destination pixel:

- **K = 1** (`kernels/pixelwise/ImageRedistort.cu`): destination pixel ->
  ray through the fitted camera -> source pixel. The fit leaves the pose alone,
  so a destination pixel and its source pixel are the SAME ray: depth (linear or
  ray) and camera-frame normals transfer unchanged and only the sampling
  coordinate moves. None of GtDepthNormalWarp.cu's point-space handling applies.
- **K > 1** (warp_to_pinhole): the face ray projects STRAIGHT through
  the source camera, so the fitted camera is never materialized and the two
  passes cost one kernel and no intermediate image. `RayToPixel<D, kFromSource>`
  is the seam; `kFromSource` is a template argument (a specialization constant
  on Vulkan) so an ordinary dataset pays neither the branch nor the 16 registers
  the source parameters occupy.

## Two-stage parse

1. **`parse_dataset`** → `ParsedDataset`: per-**input** cameras in the raw
   `train_frame="points"` frame — poses and points exactly as stored. The
   normalized-frame similarity is computed only to obtain the
   `train_frame_scale` scalar.
2. **`bake_post_split`** (`data/PostSplit.cpp`) → the post-split arrays
   `engine_setup_data_manager` consumes. This is either an identity (K=1)
   pass-through, or the split `camhost::plan_split_faces` plans for a wide
   camera when `warp_to_pinhole` is enabled (`warp_spherical_to_pinhole` for a
   panorama).

## The split

A wide camera is rendered as pinhole faces, one per frame of a fixed table:
five around the optical axis for a fisheye (front, +x, +y, -x, -y), the six
cube faces for a panorama. **Never more than one face per frame.** A frame is
one 90-degree view, and a view is the unit the per-image appearance models are
sized by -- bilagrid and PPISP hold one slot per post camera -- so cutting a
frame into tiles would cost a slot per tile and hand each model a piece of a
view.

Every face has the focal a 90-degree face of `ceil(sqrt(W*H/K))` pixels would
have -- the density of an uncropped split -- and is **cropped to the rays the
lens holds**: the planner rasterizes each frame's visibility (a valid
projection, by the GPU warp's own fold test, that lands inside the image),
takes the bounding box, rounds it up to 32 px and to no less than half the
frame -- a side band is at least the 45..90-degree ring, never a sliver --
and seats the face over it inside the frame. A frame holding under 0.2% of
the image's rays is dropped, the tolerance `spirula geometry --check` accepts
as not a hole.

What the crop does NOT buy is rendering time, which is why the faces of one
camera share a size by default. Masked tiles are skipped (below), so a masked
pixel costs almost nothing, while every render pass costs 1-2 ms a step --
a projection of every splat, a sort, the loss -- and the fused
projection-backward optimizer, which needs a single pass, is worth another
10%. Measured on an RTX 5070 laptop: going from 2 to 5 passes added 21% to a
step on a 200-degree capture, and from 3 to 4 added 15% on a cropped one.

### The frame behind the lens

`--warp-back-face` (off by default) admits a sixth face pointing backwards,
and then only when a quarter of that frame is visible -- which needs a lens
seen past 135 degrees. On a real fisheye what fills that direction is the lens
**folded over itself**, and the face is not free: it is another face's worth of
pixels in every pass, one more appearance slot per image (a 20% larger
bilateral grid, since bilagrid and PPISP are sized per post camera) and a
mirrored image for the optimizer to fit splats to. It is usually masked out
anyway. A panorama always takes all six.

### One face size, or one per lens

`--warp-face-fit` decides whether the faces of one camera share a size.

- **`uniform`** (default): one size for every face, the largest crop per axis,
  so a batch stays one tensor, renders in one pass, and the fused optimizer
  applies. A 180-degree or a cropped fisheye draws five full faces whose side
  faces are about half masked; the rasterizer skips those tiles.
- **`per-face`** gives each frame's crop its own face and renders **one pass
  per distinct size**, accumulating gradient across the passes. Every pass is
  weighted by its face count, so a face carries exactly the weight it would
  in one batch -- the loss, the gradient and the densification score are the
  uniform plan's, only the pixels differ. It draws 15-40% fewer pixels, which
  is the lever for a GPU that cannot hold the uniform batch, at the cost
  above. Without the fused optimizer the world-gradient buffers come back, so
  on a lens whose side bands are nearly full it can even use MORE memory.

Measured, 2000 steps on an RTX 5070 laptop, wall time with load and eval:

| capture | fit | faces | passes | time | peak VRAM |
|---|---|---|---|---|---|
| 1000x1500 fisheye, 108x162 deg | uniform | 5 x 548x548 | 1 | 27.6 s | 804 MiB |
| | per-face | 548x548, 2 x 548x320, 2 x 548x274 | 3 | 31.1 s | 692 MiB |
| 960x960 fisheye, ~200 deg | uniform | 5 x 430x430 | 1 | 32.3 s | 818 MiB |
| | per-face | 430x430, 4 x 430x352 | 2 | 35.4 s | 858 MiB |

For the record, the tiled plan this replaced -- the front cut in two and four
half-height bands, six tiles of 548x298 -- ran the first capture in 25.3 s at
736 MiB: 9% faster and 9% smaller than five full faces, paid for with a sixth
appearance slot per image and two slots on one view.

With `--split-batch` active on a step of several images the engine renders
one pass per input image anyway, and `per-face` adds its passes on top: the
first capture at two images a step runs 54.8 s / 886 MiB uniform against
57.6 s / 696 MiB per-face. A lens whose visible rays fit one face is not split
at all.

### Skipping masked tiles

Cropping a face to its lens leaves the corners of the crop masked out anyway --
a rectangle cannot follow a circle -- and the rasterizer runs over those pixels
in both directions. So the intersector leaves them out: a tile every pixel of
which the mask excludes emits no (tile, splat) pair, which shrinks the sort and
gives the raster an empty range to render, forward and backward.

This is exact because a masked pixel reaches nothing: the per-pixel terms are
gated by the mask, the fused SSIM's window statistics are conditional on it,
and the multi-scale pyramid pools over the unmasked children only. Two terms
would read a masked pixel -- alpha supervision, which is what "cut out
background" means -- and where either is on, tiles are not skipped. Nothing is
skipped for a render with no mask, for the eval split, or for the GUI's compare
view, all of which render every tile. `SS_TILE_SKIP_LOG=1` reports what a run
skips and `SS_NO_TILE_SKIP=1` renders everything, which is how the saving was
measured.

A tile is kept when an unmasked pixel is within ONE pixel of it, not only
inside it: the depth-to-normal stencil reads its neighbour, so a tile flush
against the boundary still feeds a live one. At zero margin the warped
`engine_train_parity` case moves; at one it is bit-identical, which is the
check that keeps this exact.

Measured, 600 steps on an RTX 5070 laptop, `SS_NO_TILE_SKIP=1` against the
default:

| capture | tiles skipped | raster tiles | + appearance |
|---|---|---|---|
| 960x960 fisheye, ~200 deg, 5 faces | 44% | 8.9 s -> 8.0 s | -> 7.7 s |
| 1000x1500 fisheye, 108x162 deg, 5 faces | 52% | 31.7 s -> 28.0 s (2000 steps) | |

Skipping 44% of the tiles buys 10% of the step, not 44%: projection, the sort,
the optimizer and the loss are all still paid in full, and the tiles that go
are the cheap ones -- a masked corner holds few splats.

The appearance backward takes the same pixels a second way. A masked pixel's
incoming gradient is zero, and every gradient bilagrid and PPISP produce from a
pixel -- grid, parameter and image alike -- is LINEAR in it, so those pixels are
skipped outright: `bilagrid_parity`/`ppisp_parity` dumped before the skip and
compared after are bit-identical (max_abs 0 tight, 9.5e-07 loose from atomic
order). That is 16% off the bilagrid backward here, 2.63 -> 2.20 ms a step.

It is not the 44% the mask would suggest, and the reason is worth recording:
the v1 gather runs one thread per (grid cell, luminance slice), only ~10k
threads in 160 blocks, all resident at once -- so the kernel takes as long as
its slowest thread, and the slowest thread is a cell whose pixels are all live.
Skipping frees threads that then idle. The lever there is not the mask but the
scan itself (every thread re-reads each pixel's RGB to bin it by luminance); the
per-pixel halves of the backward, which do parallelize, take the full saving.

One statistic is NOT identical, and deliberately so: the densification error
map is the one consumer that is not mask-gated, so without skipping a splat
seen only through masked pixels still earns a densify score, and with skipping
it does not. Gating that map to match costs about 3 dB PSNR here -- densifying
against the masked neighbourhood evidently helps the pixels that do count --
so the map stays as it is. Across seven 3000-step runs each the resulting
difference stayed inside the run-to-run spread (SSIM 0.917 +- 0.010 without
skipping against 0.905 +- 0.021 with, t = 1.3).

One behaviour follows from this and is worth knowing: the per-pixel
REGULARIZERS (alpha, normal, distortion) used to apply to masked-out pixels
while the supervision terms did not. They no longer do -- "ignore" now means
what the mask option says it means, every per-pixel term and its pixel count
alike -- which is also what makes an unrendered tile cost nothing. A run
without a mask is unaffected, bit for bit.


`spirula geometry --check` verifies the planner: every visible ray of each
test camera must land in a face, and the faces must not exceed the uncropped
pixel count.

That normalized-frame similarity is where `orientation_method` and
`center_method` act — and only `up` / `poses` is implemented natively;
anything else is approximated with a warning. Since `train_frame="points"`
leaves splats in the raw frame, the choice moves `train_frame_scale` and the
viewer's default camera, not the training coordinates. The unported methods
(`pca`, `vertical`, `gsplat`, `focus`) still have a working Python reference:
[notes/pose-normalization.md](notes/pose-normalization.md).

`--scene-center` is the one option that DOES move the training coordinates:
it translates every camera and seed point so the chosen statistic sits at the
origin, in double precision, before anything is narrowed to float. That is
what a geo-referenced reconstruction needs -- a model millions of units from
its origin loses metres to single precision otherwise. The modes are
`point-median` (geometric median of the seed cloud), `camera-median`,
`camera-focus` (the point the optical axes converge on), `point-mean` and
`camera-mean`; `none` (the default) keeps the frame the files came in. The
shift, and the identity rotation and scale that go with it, are written to
`scene_transform.json` in the run folder in every common spelling (4x4
matrices, quaternions, Euler angles), so a downstream tool can put the
splats back into the dataset's frame without converting anything by hand.
The centre is taken over every frame before the train/eval split and over
the whole seed cloud, so both splits, `spirula mesh` and the viewers -- all
of which re-read `config.json` -- land in the same frame.

The same six modes are also a *view* setting, offered by all three viewers as
a "center" menu (camera position median by default) that moves the orbit
pivot and nothing else. `src/data/SceneCenter.h` is the one implementation;
the table of centres travels to a viewer on `ViewerRenderConfig::centers`,
and the training viewer's browser client fetches it from `/scene`.

## Train/eval split

`eval_mode` selects the strategy:

- `all` (default) — every image trains.
- `fraction` — linspace-spread `ceil(N * train_split_fraction)` train images.
- `interval` — index `% eval_interval == 0` is eval, rest train.
- `filename` — basename contains `train` / `eval`.

`validation_fraction` additionally holds out a linspace-spread slice for
validation. Frames whose camera position exceeds `outlier_threshold` MADs from
the geometric median of all camera positions are rejected (default: off).

`require_image_files=false` keeps frames whose image file is missing — the
standalone viewer uses this to load camera poses from a dataset shipped
without pixels.

## Resolution

Every camera trains at its own image file's resolution. The parser probes each
file (`data/ImageProbe.h`, handed in as `DatasetParserConfig::probe_image_size`)
and takes the per-axis minimum of that and the reconstruction's stored size,
scaling `fx/fy/cx/cy` to match — so a reconstruction built at full size against
an `images_2` / `images_4` folder needs no flag, and never renders pixels the
images cannot supply. It warns once per distinct (image, camera) size pair, and
separately when the two disagree in *shape* by more than a pixel, which means
the images do not belong to the reconstruction.

`train_resolution_divisor` then divides that further (`downscale_rounding_mode`
picks floor/ceil/round per side): 2 halves each side, 0 or 1 trains at the
images' own size. This is the "train smaller to go faster" knob — the GUI's
Image resolution combo writes it — and it is relative to the images, not to the
reconstruction.

A caller with no image decoders leaves `probe_image_size` null and gets the
reconstruction's resolution unchanged; the WebAssembly viewer does exactly that.

## 360 cameras (GoPro MAX `.360`)

The ten views of a frame share one file stem under `cam0/` .. `cam9/`, so
`--rig cam0,cam1,...,cam9` (Spirula Studio: the input's rig row, on by
default for a `.360`) reconstructs them as one pose per frame with the
inter-view poses calibrated from the capture -- `src/sfm/README.md` "Rigs".

`src/app/Pano360.h` is the one implementation: it recognises the packing,
plans the views, and resamples them. Both decode paths go through it -- ffmpeg
is asked only to decode and to cut the strips out, never to warp, because its
own `v360=eac` insets every face by 2 px, which puts a 4 px step across the
seam between the two tracks.

### The frame layout

A `.360` is an MP4 with **two HEVC video tracks** (streams 0 and 5 for the
video modes, 0 and 4 for timelapse -- enumerate them, do not hardcode).
Together they are a YouTube-style **EAC 3x2 cubemap**: the first track is the
top row (LEFT, FRONT, RIGHT), the second the bottom (DOWN rot270, BACK rot90,
UP rot270). Faces are square and as tall as a track. Each track carries three
of them plus **two 32 px overlap strips**, inserted at the centre lines of its
two side faces, which is where the two lenses meet:

| mode | track | face | strips | canvas |
|---|---|---|---|---|
| 5.6K | 4096x1344 | 1344 | 2 x 32 | 4032x2688 |
| 3K   | 2272x736  | 736  | 2 x 32 | 2208x1472 |

So `face = H` and `strip = (W - 3H)/2`, and the canvas is the frame with each
strip cut at its middle -- which is exactly where each lens's copy of the seam
ends, so a bilinear tap either side of the cut stays on its own lens.
Within a face the mapping is equi-angular: `u_cube = tan(pi/4 * u_face)`.

These numbers were measured on the sample captures, not taken from a
specification: the hard content cuts sit at x=688 and x=3408 in a 5.6K frame,
and a scan of the seam energy between the two tracks minimises at
face=1344/strip=32 (1.24 against 1.50 for the constants in ffmpeg's
unmerged `gopromax_opencl` patch, which are wrong). The canvas the filter
graph builds is bit-identical to the one the in-process path assembles.

### What it is unwrapped into

`--360 faces` (the default) cuts **ten perspective views, five per lens**;
`--360 equirect` makes one 2:1 panorama; the GUI offers the same two under
"Unwrap into". Faces are the default because the raw file is **not stitched**:
the two lenses meet at azimuth +-90 degrees -- the centre line of the side
faces -- with real parallax across that seam, and no single camera model
describes both sides of it.

So the split is per LENS, not per cube. Each lens owns the hemisphere in front
of it, and that hemisphere is cut the way a cube cuts it: the face on the lens
axis, plus the near half of each of its four side faces. Every view then reads
one lens alone and is a true pinhole camera whose pose is that lens's.

| view | size | field of view |
|---|---|---|
| on the lens axis | `S` x `S` | 90 x 90 degrees |
| above / below it | `1.08 S` x `0.41 S` | 97 x 45 |
| left / right | `0.41 S` x `1.08 S` | 45 x 97 |

Three details are load-bearing:

- The side views are **tilted 67.5 degrees** off the lens axis, so their inner
  edge meets the axial face's edge and their outer edge lands on the seam.
- They are **wider than 90 degrees** along the seam. A view tilted off the axis
  narrows in azimuth as it approaches the seam, so at 90 it would leave a wedge
  uncovered between neighbours; `atan(1 / cos 22.5deg)` = 47.3 degrees of
  half-angle circumscribes the half face exactly. Widening that axis is free --
  it runs parallel to the seam.
- Each view keeps the sphere's own up, which is why four of them are portrait:
  all ten then agree on which way gravity is, and a learned detector never sees
  the same wall rotated 90 degrees.

Measured over a 8-million-direction sweep: no view sees both lenses, and
0.100% of the sphere is in no view -- a 0.03-degree hairline at the seam, which
is the rounding margin that keeps a bilinear tap off the other lens. Views
overlap on 5.9% of the sphere.

All ten share one focal length in pixels (`S/2`), so a single `--focal` covers
them; `--camera-mode single` then leaves three cameras, one per image shape.
The pixel budget is 12.6 MP a frame at the default `S`, against 13.6 MP for the
six-face cube this replaced.

`--360-size` sets `S` (the GUI has it under Advanced); the default is 1.125x
the source face, 1504 px at 5.6K. Holding the density at the *centre* of a
rectilinear face would take 4/pi = 1.27x, and the extra goes to the corners.

Ten views per frame multiply the image count, and the views of one frame share
no features: they are held together by pairs across TIME, so pairing must stay
content-based (`--pairs auto`, or COLMAP's vocabulary tree) rather than
sequential, which would give ten disconnected chains. A rig constraint would
tie them together directly -- `docs/notes/sfm-rig-constraints.md` surveys what
that would take.

Equirectangular remains available and is one image per frame, but it puts both
lenses into one spherical camera, seam and all, and reconstruction downsamples
it to `--max-image-size` (1600 px for the learned front ends at `--quality
high`) -- about 4.4 px per degree, against 16.7 for a 1504 px face.

### What it cannot do for you

- **Orientation.** A `.360` records nothing about how the camera was mounted,
  so an inverted mount comes out upside down until `--360-orient 0,0,180` says
  otherwise. That knob is deliberately CLI-only: the `gpmd` stream carries
  GRAV, CORI, IORI and GPS5, so levelling belongs to a reader of those rather
  than to three sliders nobody can visualise. `src/sfm/core/Telemetry.h` reads
  them and `viewer/telemetry.html` plots them, but nothing consumes them yet;
  `docs/notes/imu-gps-for-sfm.md` is the plan for what will.
- **Click prompts for masking.** Clicks are recorded on the camera's own
  frame, which the unwrap reshapes. Prompt a 360 capture with text.
- **The operator.** Whoever is holding it is in the downward and rearward views
  of every frame of most captures, and wants masking out.

## Preprocessing tools

`reference/scripts/` holds standalone Python utilities that produce these
layouts: frame extraction with blur skipping, COLMAP/GLOMAP driving,
Metashape conversion, downscaling, undistortion, masking (including a SAM2
GUI), monocular depth/normal prediction, and raw conversion. See
`reference/scripts/README.md`. These are *preprocessing*, separate from the
training data path, and stay on the Python side.

`reference/scripts/batch_process_data.bash` needs a COLMAP vocabulary tree; set
`SS_VOCAB_TREE` to its path.

## Benchmarking

`reference/python/benchmark.py` drives multi-scene runs over standard
academic sets (Mip-NeRF 360 `360_v2`, ZipNeRF) by calling `spirula train`.
Dataset roots are passed as arguments — no paths are hardcoded. **Each scene runs in its own subprocess**: the engine is
a process-global singleton, and running several scenes in one process leaks
state between them and silently degrades metrics.

## Do not hardcode local paths

Dataset locations belong in arguments or environment variables. See the
"Do not commit" section of [`../AGENTS.md`](../AGENTS.md) and
`tools/check_private_paths.sh`.
