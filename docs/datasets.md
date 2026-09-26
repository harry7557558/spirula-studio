# Datasets

## Supported layouts

Three formats, auto-detected in this order: **Nerfstudio**, then **COLMAP**,
then **Metashape**.

The native parsers (`src/data/parsers/*Parser.cpp`) are the one
implementation, shared by the CLI trainer, the GUI and the WASM viewer.

| format | inputs | parser |
|---|---|---|
| COLMAP | `cameras`/`images`, and `points3D` if there is one, in `.bin` or `.txt` | `ColmapParser.cpp` |
| Nerfstudio | `transforms.json`, and a PLY point cloud if there is one | `NerfstudioParser.cpp` (PLY reader lives here) |
| Metashape | camera-export `.xml`, a `.ply` if there is one, optionally a `.psx` project for filename disambiguation | `MetashapeParser.cpp` (XML via `app/Xml.h`, zips via `external/miniz`) |

The point cloud is optional in every format: a dataset without one, or with an
empty one, parses to poses alone and the trainer seeds it at random (below).

Default subdirectory names: `images/`, `masks/`, `depths/`, `normals/`.
The COLMAP reconstruction directory is auto-detected over
`{sparse/0, colmap/sparse/0, sparse, colmap, .}` unless `recon_dir` is set.
An image name in `images.txt` / `images.bin` is relative to the image folder,
as COLMAP writes it; a name relative to the dataset instead
(`images/frame_000001.png`, which some exporters write) is accepted when that
is where the file is. Masks, depths and normals are then looked up by the
name relative to the image folder, as always.

A finished dataset -- any of the layouts above, a COLMAP model sitting at its
root included -- is an input like any other on the GUI's dataset screen: its
model is reused, and the run only adds what was asked for (masks, depth and
normals). `spirula geometry <dataset>` does the depth-and-normal half from the
command line. Both write `depths/` and `normals/` beside `images/`, where the
parsers already look, and touch nothing else.

## Masks

A training image's mask comes from up to two places:

- **a mask file** in `masks/`, found by the image's name (`frame.png`,
  `frame.jpg.png`, `frame_mask.png`), white where the image is kept;
  `flip_mask` swaps that for files that paint what to remove;
- **the image's own alpha channel**, for an RGBA (or gray + alpha) image whose
  alpha is not opaque everywhere -- a render or a cut-out with a transparent
  background. Opaque from 128 up, the gate the dataset screen's JPEG
  conversion uses when it turns alpha into a mask file.

With both, a pixel is kept only where both keep it, and `flip_mask` applies
to the file alone: alpha always means "transparent is not the subject". The two
are ANDed at whichever size is finer -- the image's training size or the mask
file's -- with the alpha area-resampled before the gate, and
`mask_boundary_offset` then moves the edge of the result. A mask file whose
aspect ratio differs from its image's is stretched onto it with a warning.
`load_masks` off ignores both.

Against a constant background (`background_mode` `color`), a cut-out image's
colour is also composited onto `background_color` by its alpha as it is
decoded: a transparent pixel's ground truth is the background it is rendered
on, which is what eval scores a render against over the whole frame, what the
Images tab shows, and what a soft edge renders as. The other background modes
have no one colour to composite onto and keep the stored one.

What a masked-out pixel means is `apply_loss_for_mask` (the GUI's Mask mode):
ignored ("Ignore distractors") or trained as empty space ("Cut out
background"). Left unset it resolves per dataset: cut out when the only masks
are the images' alpha, ignore otherwise -- a mask file is as likely to mark a
passer-by as a background. `config.json` records the resolved value.

Which files carry alpha is read from their headers, then settled by decoding
the first, middle and last of them: an RGBA export that is opaque everywhere is
no mask. Training decodes each such image twice, once for its colour and once
for its alpha.

## Seed points

The splats start from the dataset's point cloud. `random_init` decides when
they start from points drawn at random around the cameras instead: `auto` (the
default) when the dataset has no point cloud or an empty one, `always` in place
of whatever it has, `never` not at all -- a dataset without points is then an
error, as it was before the option existed. `src/data/RandomPoints.h` draws
them, `TrainerSession::load_dataset()` puts them in `ds.points`, so the GUI's
preview shows the cloud that will be used and `seed_splats()` treats it like
any other.

- **How many:** `random_init_fraction` of `cap_max` (0.1: 100k of 1M).
- **Colour:** uniform random 8-bit RGB, which then goes through the same
  seed-colour conversion (`point_color_*` to `splat_color_*`) a
  reconstruction's colours do.
- **Centre** (`random_init_center`): the median, focus or mean of the camera
  positions -- `dsparse::scene_center`, the modes `--scene-center` uses -- or
  the origin of the training frame.
- **Spread:** the cameras' second moment about that centre, `M = mean(d dT)`,
  gives the principal axes. Along each, `random_init_spread` takes the mean of
  the squared projections (the eigenvalues of M) or their median, which
  ignores a few far-off cameras; the isotropic variance is a third of the mean
  or median squared distance. `random_init_std` multiplies every standard
  deviation, so below 1 packs the cloud inside the camera positions (an object
  they circle) and above 1 spreads it past them (a room they stand in).
- **Shape** (`random_init_distribution`): `isotropic-gaussian`,
  `anisotropic-gaussian` along those axes, or a uniform `ellipsoid` or oriented
  `box`, sized so their covariance is the anisotropic Gaussian's (semi-axes
  `sqrt(5)` and half-extents `sqrt(3)` standard deviations). Cameras that all
  sit at one height have no vertical spread, and every shape but the isotropic
  one comes out flat.

The draw is seeded, so the same settings give the same cloud. Cameras that do
not spread about the centre at all -- one camera, or `origin` placed exactly on
a lone one -- are an error rather than a cloud of zero size. The log line
`Seed points drawn at random` gives the count, shape, centre and the three
standard deviations it used.

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

## EXIF orientation

A phone held upright records a **landscape file plus a tag** that says to turn
it. Nothing in a reconstruction pipeline reads that tag by default -- COLMAP
does not, and neither did this one -- so a portrait capture arrives as a scene
lying on its side, and the images look wrong in any viewer that does read it.

Three things now deal with it, at three different points:

1. **Frames pulled out of a video** are written already turned by the
   container's display matrix (`app/FrameExtract.h`, `auto_rotate`, on by
   default; `spirula sam extract --no-autorotate` declines). ffmpeg does the
   same for the fallback path, so both decoders write the same pixels. The
   files carry no orientation metadata afterwards, so nothing downstream has
   to agree about anything.
2. **Photos copied into a dataset** with "Copy, re-encoded as JPEG" are
   re-encoded when their tag asks for a turn, with the turn baked into the
   pixels and the tag reset to 1. The rest of the EXIF block is carried over
   unchanged -- the focal-length prior and the GPS are what the reconstruction
   would otherwise lose.
3. **Photos read as they are** keep their tag, and `--exif-orientation` says
   what it is worth. It is a flag of `spirula sfm` and of `spirula train`, and
   **the two must be given the same value**: it decides what frame the model
   and the images share.

| `--exif-orientation` | the pixels | the model | reads correctly in |
|---|---|---|---|
| `none` | untouched | levelled by the image's own up | -- (the scene is on its side) |
| `orient` (default) | untouched | levelled by the tag's up | software that ignores EXIF, and software that reads it |
| `apply` | turned on load | levelled by the image's own up, which is now the tag's | software that reads EXIF |

`orient` is the default because it is the only one that leaves the pair usable
either way: the files on disk are untouched, so a reader that ignores EXIF
still sees images that match the cameras, while the scene itself stands up.
`apply` is for a pipeline whose every consumer applies the tag; a model built
with it describes the TURNED frame, so a training run over it needs `apply`
too, and a run that forgets is told so by name (the warning that the image is
its camera transposed).

**A mirrored tag (2, 4, 5, 7) is a compromise in `apply`.** Mirroring an image
cannot be undone by moving the camera -- the pose that fits mirrored pixels is
the mirror image of the real one -- so only the rotation is applied and the run
warns. `orient` needs no compromise: a mirror does not move which way is up.

**Every model sees the picture upright; every map it produces is written in
the stored frame.** SAM and the geometry networks were trained on pictures the
way up they are meant to be shown, and a photo case 3 left alone is not that
way up. So masking and `spirula geometry` turn what goes IN by the tag
(`app::load_upright`) and turn the mask, the depth and the normals back out by
its inverse (`app::inverse_turn`) -- a normal map's vectors along with its
pixels, x and y being image axes. The files beside an image are then in the
image's own frame, which is the pair `orient` and `none` need; `apply` turns
both together on load, so it needs the same pair.

**The GUI's previews are the same pictures.** "Try the mask" goes through
`gui/PreviewFrames.h`, which applies the input's `app::FrameLook` -- the turn
above, the downscale, a 360 file's unwrap into views -- and for a video
decodes through `app::extract_frames_at`, the entry point extraction itself
uses. So the frame on screen is the one the masker sees, and a click on it
names a pixel the run will read. The geometry preview reads its frames STORED,
because those come with a camera that describes the stored pixels, and turns
the warped FACE instead -- exactly as `spirula geometry` does.

One thing this does not reach: `spirula sfm merge` has no features to read the
tag from, so it reads it back off the image files (`sfm/map/Orient.h`,
`fillExifOrientations`); without `--image-dir` it falls back to the image's
own up.

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

## 360 cameras (GoPro MAX and MAX 2 `.360`)

The ten views of a frame share one file stem under `cam0/` .. `cam9/`, so
`--rig cam0,cam1,...,cam9` (Spirula Studio: the input's rig row, on by
default for a `.360`) reconstructs them as one pose per frame. Spirula Studio
also hands the reconstruction the rotation it cut each view at, as the start
of a refinement in which the five views of one lens keep its centre --
`src/sfm/README.md` "Rigs". Without that (a bare `--rig`) the inter-view poses
are calibrated from the capture instead.

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
of them plus **two overlap strips**, inserted at the centre lines of its
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

### The MAX 2 layout, which is a different thing entirely

A MAX 2 writes 5952x1920 tracks, which the arithmetic above happily reads as
`face=1920, strip=96` -- and that is wrong. Its tracks are **not** a cube map
between them. Each one is a whole **equirectangular panorama of the sphere**,
already stitched, with the sides padded to fill the frame:

```
5952 = 1008 + 3936 + 1008          3936 px = 360 deg, 10.933 px/deg
1920 rows out of the 1968 a 2:1 panorama would want: +-87.8 deg of elevation
```

The two tracks hold the same sphere in different orientations (the second is
rolled about 90 degrees), so either alone is complete; the first is the one
standing upright and is the only one read. The 1008 px of side padding repeat
the far side of the panorama from the other lens, so they are dropped.

Nothing about the frame size says where the panorama sits inside it, so the
camera's own `PMOD` is what places it -- rows cut, then padding -- and both
numbers have to agree about where the quarter turn lands or the file is
refused. `PRJT` is "EACO" on both generations and cannot tell them apart; the
MAX 2 writes a fourth `PMOD` entry and the MAX does not.

These numbers were measured, not documented: the padding boundaries are the
only strong column discontinuities in the frame (x=1008 and x=4944), which
fixes the period at 3936; matching the two tracks against each other as one
rotated sphere peaks at 175.6 degrees of elevation, which is what 10.933
px/deg over 1920 rows predicts; and 60 frames of the cropped panorama
reconstruct at **100% registered, 1.02 px mean reprojection** under the
fixed-geometry equirectangular camera model, which no wrong projection does.

### What it is unwrapped into

`--360 faces` (the default) cuts **ten perspective views, five per lens**;
`--360 equirect` makes one 2:1 panorama; the GUI offers the same two under
"Unwrap into". (A MAX 2 is stitched already, so it gets **six** views instead
-- see below.) Faces are the default because a MAX file is **not stitched**:
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
sequential, which would give ten disconnected chains. The rig ties them
together directly -- `docs/notes/sfm-rig-constraints.md`.

Equirectangular remains available and is one image per frame, but it puts both
lenses into one spherical camera, seam and all, and reconstruction downsamples
it to `--max-image-size` (1600 px for the learned front ends at `--quality
high`) -- about 4.4 px per degree, against 16.7 for a 1504 px face.

### A MAX 2's views

A stitched panorama has no seam to keep views off, so faces mode gives it a
plain **cube of six** at 100 degrees, and equirect mode hands the panorama
over as it is. The cube is stood on a corner -- face centres at +-35.26
degrees of elevation, 120 apart in azimuth -- rather than axis-aligned:

- A panorama's poles are its own weak point, a few rows of source stretched
  around the whole frame. A face pointed straight up or down is a starburst
  through most of its area. Stood on a corner, no face centre is near a pole
  and the smear is confined to face corners.
- Every face then holds some horizon, which is what carries the features.
  Measured on 38 frames of an 8K clip: **56% of images registered**
  axis-aligned, **74%** stood on a corner, at 0.61 px mean reprojection. The
  two faces that still lag are the ones looking at the operator and the mount.

### What it cannot do for you

- **Orientation.** A `.360` records nothing about how the camera was mounted,
  so an inverted mount comes out upside down until `--360-orient 0,0,180` says
  otherwise. That knob is deliberately CLI-only: the `gpmd` stream carries
  GRAV, CORI, IORI and GPS5, so levelling belongs to a reader of those rather
  than to three sliders nobody can visualise. `src/sfm/core/Telemetry.h` reads
  them and `viewer/telemetry.html` plots them, but nothing consumes them yet;
  `docs/notes/imu-gps-for-sfm.md` is the plan for what will.
- **The operator.** Whoever is holding it is in the downward and rearward views
  of every frame of most captures, and wants masking out.

## Frames out of a video

One frame every `skip` source frames, the sharpest of a window of `keep`
around each. Both decode paths choose the same frames (`app/FrameExtract.h`),
and the GUI's rate is **per input**: a capture shot as several clips is rarely
shot at one pace, so `PrepInput::fps` overrides the job's for that video.

The rate is a column of the input list rather than a field in the settings, so
it sits beside the video it describes. The first video's box holds the
dataset-wide rate -- that is what a preset carries and what every item of a
batch starts from, and it is why the box is there even for a lone video; every
row below it shows `^`, the convention the lens column already uses, so a dozen
clips off one camera stay one decision. Typing the rate above back into a `^`
row returns it to following (`normalize_source_fps`).
The rows on one rate are also one **budget** when the rate is adaptive: they
are measured together and spaced against one view-change-per-frame, so of two
clips at "2 fps" the one that walks briskly gets the denser frames and the one
shot from a bench gets fewer. Each still keeps its own rate bounds. Rows
measured by different models are NOT one budget -- see below.

A D-Log M clip's frames are 16-bit PNG, not JPEG, unless the job says 8
(`frame_bits`; see [notes/dlog-m.md](notes/dlog-m.md#16-bit-frames)). They go
through ffmpeg, keep the same instants on both lenses of a 360 file, and take
about 60 times the disk.

A workspace records what its frames were extracted with (`.spirula-frames`,
`gui/ReconStamp.h`). A re-run whose answer differs -- a different rate, a
different unwrap, another clip in the list -- goes back to the video instead of
keeping them, and drops the features and matches that describe the old ones.

### Adaptive spacing

`--adaptive` (the GUI: "Adapt the rate to the motion") spaces the kept frames
by how much the **view** changed instead of by how much time passed. The rate
asked for becomes the average; the realized rate stays within `--adaptive-range`
either side of it, and the frame count comes out the same.

`app/FrameMotion.h` measures it. A grid of about 500 points is tracked between
small grey frames by pyramidal Lucas-Kanade, one global model is fitted to the
flow by RANSAC, and two numbers come out:

- **coverage** -- the share of the frame's content that left it, as the area of
  the frame carried over by the fitted model and cut back to the frame. Pan and
  zoom cost; roll costs only its corners, which is right, because a rolled
  frame still sees what it saw.
- **parallax** -- the flow the global model could not explain, as the 75th
  percentile of the residual. This is what a translation past something close
  produces and what a translation towards something far does not.

`cost = coverage + 2 * parallax`. The weight is the only hand-set number: a
tenth of the frame of unexplained disparity is a harder match than a tenth of
the frame of pan, and carries the triangulation the pan does not.

That per-step number is what the Motion view draws, but it is **not** what the
plan adds up. Coverage summed over a gap counts a wobble every time it passes:
a wrist that turns the frame a degree left and a degree back has moved nothing
and scores twice. So each step also carries the affine it fitted, in
unit-square frame coordinates (`MotionStep`), and the plan **composes** them
from the last kept frame to the candidate and takes the coverage of the
result -- the wobble composes back to the identity and costs nothing. Parallax
is a residual and composes through nothing, so it is still summed. Measured
over one-second gaps on two handheld DJI clips, composing drops the typical
window to 0.65-0.70 of its summed coverage and the shakiest tenth to 0.0-0.4:
that tenth used to buy frames and now does not. A sphere's steps compose to the
identity by construction (nothing leaves a sphere), so its plan is unchanged,
bit for bit.

The global model depends on what the frames are pictures of:

| capture | model | coverage |
|---|---|---|
| ordinary video | 2D affine, in the image | the frame carried over |
| `.360` | a rotation of the SPHERE, through the packing's own mapping | 0 |
| dual fisheye (`.insv`, `.osv`) | a rotation of the sphere, equidistant lens | 0 |

A camera that sees every direction keeps every direction when it turns, so a
360 capture spinning on the spot scores nothing and is given frames at the
slowest rate the bounds allow. That is the whole point of fitting the rotation
in 3D rather than fitting a homography per track: on a sphere, turning is free
and only moving is not.

That also means a sphere's cost and a flat capture's are **not the same
number**. A sphere's is a residual and nothing else; a flat one's is 70%
coverage on a handheld walk, because every wobble of the wrist turns the frame
over. Measured across five clips of one walk, a `.osv` scored 0.07-0.17 per
second against 0.70-0.83 for the DJI flat clips beside it -- so pooled into one
budget the flat clips took 2.4x the rate asked for and the 360s were left at
0.28x, far enough apart to stop matching. A budget is therefore shared only
among the inputs one model measured against one angle (`same_scale`), and the
budgets themselves split by the frames the fixed schedule would have given
each. The same five clips then land between 0.66x and 1.48x.

The price is that a flat clip shot from a bench can no longer hand its frames
to a 360 that is walking. Nothing makes those two numbers comparable: turning
is free on a sphere and is most of the cost on a flat frame, so there is no
scale factor between them to find.

What does NOT change the cost is the source resolution. Every source is reduced
to the same grey frame before anything is tracked (`motion_frame_size`), which
holds to 2% end to end: one fisheye clip at 3840 and 1920 px scored 0.0904 and
0.0909 per second, one flat clip at 2688, 1344 and 672 px scored 0.720, 0.707
and 0.716.

Two details that were measured rather than chosen:

- A sphere frame is analyzed at **four times the pixels** of a flat one. It
  spans three times the angle, and at flat resolution the tracking floor is
  itself a degree wide and drowns the parallax.
- The step is found by **bisection** on the wanted count, not as
  `total / count`. A burst of motion swallows several steps' budget within one
  sample and can spend only one frame of it, which left plans a third short.

It costs one extra decode pass over the first track of each video: measured at
+21% on a dual-fisheye `.osv` (whose main pass already decodes two tracks) and
up to +100% on a 1080p clip, with the tracking itself overlapped with the
decode. Nothing is buffered: a video's worth of pictures does not fit, and a
plan cannot be made until the whole cost curve is known.

Without the built-in decoder the same plan is made from the candidate frames
ffmpeg already extracts, at `fps x max(window, range)` instead of
`fps x window` so there are enough of them for the fastest rate it may ask for
(`gui/FrameSelect.h`). That path plans one video at a time -- the candidates of
a whole group are not on disk at once -- and it numbers the frames it keeps by
the candidate they were, not by how many it has kept. The stem is what times a
frame against the video's IMU and GPS (`sfm/map/SensorGauge.h`), and an
adaptive plan leaves nothing evenly spaced for a frame rate to recover it from.

The pass reports as it goes, in two places. It enters the Frames step itself --
nothing else has, since a whole rate group is measured before any of it is
written, and the panel draws no bar at all for a step that is not running -- and
moves a bar across the group's total length, naming the video it is on. A
13-minute 1080p clip scans at about 165 frames a second, so without the bar the
screen sat unchanged for over two minutes with the GPU pinned, which reads as a
hang.

The **Motion** view beside the run's other previews is the rest of it: one row
per input, in the order they were given, each drawing the view change along that
capture as it is measured and, under it, the rate the plan settled on. Every row
is on ONE scale, because a clip that moves twice as much as its neighbour is
exactly why it took the frames off it. A folder of photographs is a row that
says it has no motion rather than a gap in the list.

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
