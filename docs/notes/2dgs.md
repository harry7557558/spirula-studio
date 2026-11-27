# 2DGS: the surfel primitive

`--primitive 2dgs` trains flat Gaussian discs in the style of 2D Gaussian
Splatting (Huang et al., SIGGRAPH 2024) with the same engine, on both
backends. There is no `2dgs` preset: it had stopped being a faithful 2DGS
baseline. Runs here that name it used the [weights below](#preset-weights).

## How it fits

- **The eval3d raster path does the work.** 3DGUT's rasterizer already casts
  one world-space ray per pixel for every camera model and asks a fragment for
  its alpha and colour along that ray. A ray-disc intersection is a different
  fragment, not a different rasterizer, so fisheye, equirectangular,
  orthographic and lens distortion come with it.
- **Storage is 3DGS storage.** `scales` stay `float3` (log); a disc uses `x`
  and `y`. The third scale is seeded at `kSurfelLogScaleZ` (-20) and the
  optimizers keep it at or below the disc's larger axis, so splits and max-scale
  tests never pick it, a collapsed disc reads as dead to relocation and export,
  and 3DGS viewers draw the PLY as flat discs. Optimizer, densify, codecs,
  checkpoints, resume, `SplatPly` and the viewers need no layout change.
- **Screen buffer is 3DGUT's** (`SCRG_*`): projection writes the disc's screen
  bounds and sort depth, and the raster rebuilds the fragment from world data.
  Projection backward, qgrad and the fused projection-backward optimizer are
  3DGUT's, since the raster writes the geometry gradients to the world buffer.
- **The losses existed.** Depth distortion, normal and RGB distortion,
  depth-normal consistency (`normal_reg_weight`) and normal supervision were
  in the per-pixel loss with a `pred_normal` slot no primitive filled. 2dgs
  renders normals (`RenderOutputType::RGB_DN`) and fills it. 3dgut fills it
  too, with the normal of the surface its depth draws (the short axis once
  flat; `evaluate_color_3dgs_normal`), on a step one of those terms reads it
  (`engine_loss_reads_render_normal`, `Vanilla3DGUTNormal`);
  3dgs and mip render none, since their screen rows carry no orientation.

## The fragment

Shared Slang, `src/shaders/projection_utils.slang` (`evaluate_2dgs` and its
fused VJP); the screen bounds are `projection_3dgs<..., surfel>` in
`primitive_3dgs.slang`, exact for a disc under any lens
([footprint.md](footprint.md)).

- tangent frame `tu, tv, n` = columns of `R(quat)`, plane through `mean`
- `t = dot(mean - o, n) / dot(d, n)`, `p = o + t d`,
  `u = dot(p - mean, tu) / sx`, `v = dot(p - mean, tv) / sy`
- `G = exp(-(u^2 + v^2) / 2)`, the bare disc: the paper's screen-space
  low-pass is left out ([below](#no-screen-space-low-pass)), so a 2dgs model
  renders as 3dgut renders it
- depth is `t`, ray distance along unit rays as for 3DGUT; the normal is
  `-sign(dot(d, n)) n`, composited in world space and stored in camera space
  (the frame of `depth_to_normal_forward`)
- a grazing ray misses the disc rather than dividing by zero

One VJP gives alpha and colour from a single ray-disc hit per backward
fragment; it took the Vulkan raster backward from 12.5 to 9.8 ms a step.
`surfel_grad_test` checks the gradients against exact dual numbers of a host
model, holding the device to that model's own fp32 error, which near grazing
and for sub-pixel discs reaches about 1%. The parity tests (`projection_parity`, `render_parity`,
`raster_bwd_parity`, `optimgeo_parity`, `fpbo_parity`) carry 2dgs rows; the
optimizer ones allow the surfel rows no float violation.

## Preset weights

The paper's weights do not carry over: this engine's distortion is in log
depth (`W*S - C^2`), its normal loss adds `sqrt((1 - cos) / 2)` to the
paper's `(1 - cos) / 2` and so pulls harder near agreement, and both ramp in
over `distortion_reg_warmup` (6000 steps).

Sweep on an action-cam fisheye capture (30k steps, 1M splats, Vulkan, 61
held-out views). Meshes from `spirula mesh`, scored by
`reference/python/mesh_fscore.py` against the capture's dense matching cloud
(tolerance 0.5% of its 5-95% extent); one run each, and F moves by ~0.02
between runs of one setting.

| distortion | normal | PSNR | SSIM | mesh F | precision | recall |
|---|---|---|---|---|---|---|
| 0   | 0    | 24.40 | 0.908 | 0.539 | 0.684 | 0.445 |
| 0.1 | 0.01 | 24.03 | 0.904 | 0.625 | 0.764 | 0.530 |
| 0.1 | 0.02 | 23.88 | 0.902 | 0.607 | 0.791 | 0.492 |
| 0.3 | 0.02 | 23.84 | 0.902 | 0.606 | 0.796 | 0.490 |
| 0   | 0.04 | 23.79 | 0.900 | 0.674 | 0.799 | 0.583 |
| 0.1 | 0.04 | 23.75 | 0.899 | 0.694 | 0.829 | 0.597 |
| 1.0 | 0.04 | 23.67 | 0.898 | 0.667 | 0.843 | 0.552 |

The former preset took the best mesh, 0.1 / 0.04: `--primitive 2dgs
--sh-degree 0 --dc-reg 10 --overexposure-reg 10
--background-transparency-suppression noise --depth-distortion-reg 0.1
--normal-distortion-reg 0.01 --normal-reg-weight 0.04` and 0.01 for
`--mean-median-depth-weight`, `--median-depth-normal-reg-weight`,
`--median-normal-supervision-weight`, `--median-render-normal-reg-weight` and
`--erank-reg`. Read the table for its differences: later runs of the preset
on the same capture score 23.3 dB / 0.881 at the same mesh F, a 0.4 dB offset
from this batch that no setting or seed cloud explained. Distortion 0.1 adds
8% GPU time a step.

Plain `--primitive 2dgs` keeps the shared `normal_reg_weight` of 0.04: it is
most of the mesh gain above (F 0.539 to 0.674) for 0.6 dB, and a default that
changed with the primitive would be a rule nobody could see.

## Measurements

Mip-NeRF 360 (`images_4` garden, `images_2` bonsai, every 8th view held out,
30k steps, 1M-splat cap, Vulkan, one run each); PSNR / SSIM / LPIPS:

| scene | 3dgs | 2dgs, normal consistency 0 | `2dgs` preset |
|---|---|---|---|
| garden | 26.99 / 0.868 / 0.087 | 26.39 / 0.843 / 0.109 | 25.79 / 0.831 / 0.121 |
| bonsai | 29.75 / 0.931 / 0.115 | 29.80 / 0.932 / 0.115 | 28.97 / 0.919 / 0.131 |

Indoors the disc matches 3dgs; on garden's open foliage it gives up 0.6 dB,
as the paper's outdoor scenes do. The preset's geometry losses cost a further
0.6-0.8 dB on both, the price of its meshes. These runs had the screen-space
low-pass ([below](#no-screen-space-low-pass)), which on bonsai scores within
0.1 dB of the bare disc; 2dgs then trained in 1.5-1.6x the time of 3dgs
(garden 512 s against 772-823 s, bonsai 752 s against 1151-1210 s) with
14-33% more engine VRAM.

An action-cam fisheye capture, 7000 steps, 1M splats, longer side 1344 px:

| primitive | backend | PSNR | SSIM | train time | engine VRAM |
|---|---|---|---|---|---|
| 3dgs  | Vulkan | 24.43 | 0.900 | 135 s | 451 MB |
| 3dgut | Vulkan | 24.26 | 0.901 | 159 s | 482 MB |
| 2dgs  | Vulkan | 23.89 | 0.891 | 173 s | 536 MB |
| 2dgs  | CUDA   | 23.85 | 0.890 | 199 s | 544 MB |

- **Step cost** (`app/SettingCosts.h`): raster forward 1.73x / 1.35x and
  backward 1.96x / 1.48x against 3dgs on garden 1/4 / bonsai 1/2, a whole
  step 1.29x / 1.19x; two runs of each, 5000 steps to a 1M cap
  (docs/notes/step-timing.md). With the low-pass the raster read 2.08x /
  1.64x and 3.35x / 2.46x.
- **Collapsed discs**: before the third scale followed the disc, a 3M-splat
  30k-step model held 306k discs shrunk to the -40 floor that relocation never
  recycled. The fix is neutral on quality (23.31 / 0.881 / F 0.705 without,
  23.33 / 0.881 / F 0.697 with) and leaves 15 such discs instead of 7150.
- **Densify scores** need no 2dgs case: `densify_score_mode geom` only
  combines per-camera loss-map scores, and the world-gradient score
  (`|dL/dmean| * exp(max log scale)`) sees a disc axis. Sparse seed, 3000
  steps: 23.49 dB default, 23.44 `geom`, 23.60 / 23.51 world-gradient blend
  0.5 / 1.
- **PLY round trip**: a model reloaded with `--init-ply` loses 0.23 dB as
  2dgs, against 0.11 for a 3dgut model reloaded the same way (the appearance
  correction a PLY does not carry). Rendered as 3dgut the PLY renders as
  2dgs does, which is why meshing through the 3DGUT moments (`src/mesh/`)
  needs no 2dgs kernel. The paper's own extraction, TSDF fusion, is
  `spirula mesh --method tsdf` ([tsdf.md](tsdf.md)).

## Meshing

The opacity field (`src/mesh/`) samples each splat at its centre and at
`±k sigma` along each axis. A disc's third sigma (`e^-20`) puts both normal
samples on the centre, so a flat surface is a sample slab one point thick:
crossings land on long in-plane Delaunay edges (spike tips a median 20 mm from
any splat, against 0.8 mm for the mesh as a whole) and the surface tears.
`kSampleMinAxisRatio` (`src/mesh/MeshingDevice.h`) floors every axis's
offset at 0.25 of the largest. 3dgs models need it too: a quarter of a 1M-splat
model's splats had a smallest-to-largest sigma ratio of about 0.

Spike faces (longest edge over 10x the distance to the 8th-nearest splat) and
uncovered pixels in renders from training views, before and after the floor;
F is `reference/python/mesh_fscore.py` against the dessert dense cloud with
the mesh cropped to the cloud's box, as the whole-mesh score is diluted by
background the cloud never covered:

| model | spike faces | uncovered | F |
|---|---|---|---|
| dessert, 2dgs | 73-281 -> 0-1 | 4-13% -> under 1% | 0.718-0.731 -> 0.706-0.707 |
| dessert, 3dgs | 12-40 -> 0 (3 runs) | 1-2% -> 0.3-1.7% | 0.809-0.815 -> 0.813-0.814 |
| bicycle `images_4`, 2dgs | 47 -> 21 | 1.8-8.2% -> 0-4.8% | |

- **The ratio**: 0.1 meshed a test cube of discs as two sheets (area 1.98x),
  its normal samples too close for cameras at grazing angles; on dessert 3dgs,
  0.2, 0.3 and 0.5 left 31, 34 and 255 spike faces.
- **Low-pass models** (trained before the bare disc) lose background foliage
  with the floor: bicycle's hedges went from 6% to 25-35% uncovered. Their
  collapsed discs had been holding the hedge surface up.

## No screen-space low-pass

The paper renders `max(G, G_lowpass)`, a screen-space gaussian of sigma
sqrt(2)/2 px at the disc's centre, so a disc smaller than a pixel still draws
a pixel-sized blob. Here the disc renders bare. Low-pass -> bare, `2dgs`
preset, 30k steps, 1M cap, every 8th view held out, Vulkan, one run each,
scored from the saved PLY, LPIPS (AlexNet) / PSNR; dessert is a 480 px phone
capture of a plate (sparse seed, normal maps), bicycle `images_4`, bonsai
`images_2`:

| rendered as | dessert | bicycle | bonsai |
|---|---|---|---|
| 2dgs | 0.079 / 26.75 -> 0.078 / 26.75 | 0.205 / 24.54 -> 0.207 / 24.52 | 0.130 / 29.00 -> 0.132 / 29.05 |
| 2dgs, 2x resolution | 0.184 / 25.13 -> 0.185 / 26.03 | 0.304 / 23.43 -> 0.297 / 23.86 | 0.220 / 28.68 -> 0.221 / 28.94 |
| 3dgut | 0.105 / 25.05 -> 0.078 / 26.75 | 0.231 / 23.83 -> 0.207 / 24.52 | 0.132 / 28.71 -> 0.132 / 29.05 |
| 3dgs | 0.097 / 26.29 -> 0.146 / 23.68 | 0.283 / 22.42 -> 0.305 / 21.88 | 0.135 / 28.92 -> 0.139 / 28.87 |
| mip | 0.117 / 25.17 -> 0.091 / 26.92 | 0.293 / 21.86 -> 0.284 / 21.78 | 0.135 / 28.75 -> 0.134 / 29.08 |

- **At the trained resolution the low-pass leads slightly**: 0.002 LPIPS on
  bicycle and bonsai. On dessert its training eval leads by 0.005 (two runs:
  0.075 against four bare runs at 0.078-0.081), but its saved PLY scores
  0.079, inside the bare runs' spread (the PLY bullet below).
- **The low-pass's blobs are fixed in pixels.** A disc under it gets no
  gradient through its scale, and the model trains its dots at one
  resolution: at twice that they halve, 0.3-0.9 dB below the bare disc, the
  speckle a viewer at another size shows.
- **3dgut and the mesher see what trained**: the bare model scores the same
  as 2dgs and as 3dgut; the low-pass model loses 0.3-1.7 dB as 3dgut.
- **The PLY keeps the model.** The export skips discs at the -40 scale floor
  or under 1/255 opacity; the low-pass model on dessert lost 43k of 1M that it
  still drew, 0.17 dB and 0.0035 LPIPS below its training eval. The bare one
  lost none.
- **The cost is plain 3DGS viewers**: their 0.3 px^2 dilation draws the bare
  model's sub-pixel discs at full opacity, 2.6 dB below the low-pass model on
  dessert, where most discs are sub-pixel, and 0.5 dB on bicycle. Drawn as mip,
  whose dilation scales opacity down, the bare model scores LPIPS 0.091 /
  0.284 / 0.134 against 0.097 / 0.283 / 0.135 for the low-pass model in 3dgs.
- **Training is faster**: 431 against 523 s on bicycle, 640 against 768 s on
  bonsai, 507 against 499 s on dessert.
- **Meshes** from default `spirula mesh` look alike on bicycle and bonsai; on
  dessert the bare model's has far fewer holes and stray spikes, over a
  bumpier table top.

An area-compensated low-pass, its opacity scaled by the disc's screen area
over the low-pass's as Mip-Splatting's 2D filter does, scored best at 2x
(LPIPS 0.174 / 0.295 / 0.220) but no better at the trained resolution, trained
7-46% slower, and broke the bicycle mesh into flakes.
