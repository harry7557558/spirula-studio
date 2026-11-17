# 2DGS primitive — implementation plan

Goal: `--primitive 2dgs`, a surfel primitive in the style of 2D Gaussian
Splatting (Huang et al., SIGGRAPH 2024), trained by the same engine at close
to 3DGS speed and memory, on both backends.

## Status

Phases 1-4 and the phase-5 preset done (branch `native-2dgs`): `--primitive
2dgs` and `spirula train 2dgs` train end to end on both backends, and every
parity test passes CUDA vs Vulkan.

- `23100e62` surfel math in shared Slang + `surfel_grad_test`
- `bdf5a0eb` regenerated `projection_utils.cuh`
- `4e5d0c4f` projection and rasterizer, both backends, parity rows
- `a460ad56` engine wiring, normals in the loss, surfel regularizers
- `ed2caea0` fused alpha + colour VJP (`evaluate_2dgs_vjp`): one ray-disc
  hit per backward fragment instead of two
- `eb8d9c92` `2dgs` training preset (see "Preset weights")

First measurements (an action-cam fisheye capture, 427 views, 1M splats, 7000
steps, longer side 1344 px, 61 held-out views):

| primitive | backend | PSNR | SSIM | train time | engine VRAM |
|---|---|---|---|---|---|
| 3dgs  | Vulkan | 24.43 | 0.900 | 135 s | 451 MB |
| 3dgut | Vulkan | 24.26 | 0.901 | 159 s | 482 MB |
| 2dgs  | Vulkan | 23.89 | 0.891 | 173 s | 536 MB |
| 2dgs  | CUDA   | 23.85 | 0.890 | 199 s | 544 MB |

Quality is where the paper puts 2DGS against 3DGS. The fused VJP took the
Vulkan raster backward from 12.5 to 9.8 ms a step (`SS_PROFILE=1`, 2000
steps) and the 2dgs rows from 206 / 224 s, so Vulkan is 1.09x 3dgut, inside
the 15% target.

## Next session

1. **Validation (phase 7)** on Mip-NeRF 360 / DTU, then docs and the PR;
   re-check the preset weights and the `SettingCosts.h` row there (both
   come from one capture).
2. **The sweep's 0.4 dB.** Today's `spirula train 2dgs` scores 23.31-23.33
   dB where the sweep's 0.1 / 0.04 row says 23.75, with or without the
   dead-disc fix; find what the sweep ran differently before quoting it.

Build and test (both trees on one machine):

```bash
bash build_develop.bash -DSS_BACKEND=vulkan -DSS_BUILD_BACKEND_TESTS=ON
bash build_develop.bash -DSS_BACKEND=cuda -DSS_BUILD_BACKEND_TESTS=ON
./build_vulkan/surfel_grad_test && ./build_vulkan/reg_loss_underflow
./build_vulkan/train_config_test && ./build_cuda/train_config_test
for t in projection_parity render_parity raster_bwd_parity optimgeo_parity fpbo_parity; do
  ./build_cuda/$t dump /tmp/$t.bin && ./build_vulkan/$t compare /tmp/$t.bin; done
```

Editing anything under `src/shaders/` needs `src/shaders/build.bash` (see
docs/codegen.md) with the slangc that made the committed headers, 2026.2.1:
the pinned 2026.12.0.1 rejects `primitive_3dgs.slang:587` for the CUDA target.

Short benchmark runs need `--num-iterations N`: `--max-steps` does not stop
training. `SS_PROFILE=1` prints GPU time per kernel at the end of a run; wall
time on a laptop GPU varies 5-40% with temperature, so compare GPU time.

## Why it is cheaper than it looks

- The **eval3d raster path** (`kernels/raster/RasterizationEval3D*`, Vulkan
  `rasterize_fwd_3dgut`) already generates one world-space ray per pixel for
  every camera model and asks a fragment for `evaluate_alpha(ray_o, ray_d)` and
  `evaluate_color(ray_o, ray_d)`. A ray–surfel intersection is a different
  fragment, not a different rasterizer. Fisheye, equirect, orthographic and
  lens distortion come with it.
- The **2DGS losses exist**: depth distortion (log-depth `W*S - C^2`),
  normal / rgb distortion, depth→normal consistency (`normal_reg_weight`),
  normal supervision. The per-pixel loss already has a `pred_normal` slot that
  is null because no primitive renders normals.
- `RenderOutputType::RGB_DN` and `DistortionType::DN / RGB_DN` are defined and
  handled in the CUDA kernels; they are only not instantiated.
- Slang autodiff gives the per-fragment VJPs (`bwd_diff`), as it does for
  `evaluate_alpha_3dgs`.

## Design decisions

1. **Storage = 3DGS storage.** Keep `scales` as `float3` (log); the surfel uses
   `x, y` and ignores `z`. Then the optimizer, densify, codecs, checkpoints,
   `SplatPly`, `SplatMerge` and both viewers need no layout change. `scale.z`
   gets zero gradient and is seeded at `kSurfelLogScaleZ` (-20), below any disc
   axis, so splits and max-scale tests never pick it and 3DGS viewers draw flat
   discs. The per-splat regularizers take a `surfel` flag and see `x, y` only.
2. **Screen buffer = 3DGUT's layout** (`SCRG_*`). The projection computes the
   disc's screen bounds and sorting depth; the fragment is rebuilt from world
   data in the raster kernel, as 3DGUT does. Reusing the layout keeps
   `projective_depth`, the qgrad path and the fused projection-bwd optimizer
   reachable with a new primitive type rather than a new buffer format.
3. **Fragment math** (shared Slang, `src/shaders/projection_utils.slang`; the
   bounds are `projection_3dgs<..., surfel>` in `primitive_3dgs.slang`):
   - tangent frame `tu, tv, n` = columns of `R(quat)`; plane through `mean`
   - `t = dot(mean - o, n) / dot(d, n)`, `p = o + t d`,
     `u = dot(p - mean, tu) / sx`, `v = dot(p - mean, tv) / sy`
   - `G = max(exp(-(u²+v²)/2), G_lowpass)` where `G_lowpass` is the paper's
     screen-space fallback: ray-to-centre distance divided by the pixel
     footprint at that depth, so it works without a pinhole `K`
   - outputs: `rgb`, `depth = t` (ray distance: `generate_ray` returns unit
     directions, as for 3DGUT), `normal = -sign(dot(d, n)) * n` in world space
   - grazing rays (`|dot(d, n)|` small) fall to the low-pass term, not a NaN
4. **Normal frame**: normals composite in world space and the rasterizer
   stores them in camera space, the frame of `depth_to_normal_forward`; the
   backward rotates the gradient back. The normal distortion stays in world
   space, which the loss allows because it reads only the channel mean.

## Phases

### 0. Baseline (partly done: 3dgs / 3dgut measured on one capture)
- Fork, branch `native-2dgs`.
- Record 3DGS and 3DGUT numbers on a Mip-NeRF 360 scene and a DTU scan (PSNR,
  step time, peak VRAM) on both backends with `reference/python/benchmark.py`,
  so every later phase has something to compare against.

### 1. Shared math + gradient test (done)
- `evaluate_2dgs` (alpha, rgb, depth, normal) and its `_vjp` in
  `projection_utils.slang`; `evaluate_alpha_2dgs` / `evaluate_color_2dgs`
  for the forward.
- `surfel_grad_test` (Vulkan): exact dual-number gradients of a host model;
  the device is held to the error distribution of the same model in fp32,
  because near grazing and for millimetre low-pass footprints fp32 itself
  loses up to ~1%. Central differences were too coarse there and were
  dropped as the reference. CUDA is covered by the raster parity rows.

Both backends are required: Vulkan is where training is iterated on, CUDA is
where long runs happen. Phases 2 and 4 therefore land together, and the
parity tests (CUDA vs Vulkan on one machine) gate each step rather than
coming at the end.

### 2. CUDA backend (done)
- `primitives/Primitive2DGS.cuh`: `pixelType = RGB_DN`, `World` / `Screen`
  from the 3DGUT bases, `FragmentFwd` / `FragmentBwd` calling the Slang math.
- Projection fwd / packed fwd / bwd / qgrad for the new type; fused
  projection-bwd optimizer dispatch.
- Raster: instantiate eval3d fwd/bwd and moments for 2DGS, with `DN` and
  `RGB_DN` distortion enabled for this primitive only.
- Projection backward, qgrad and the fused optimizer are 3DGUT's: the eval3d
  raster writes geometry gradients to the world buffer, so the screen
  gradients are only opacity and colour, identical for both.
- 136 new instantiation TUs (89 are the raster backward).
- `long_axis_split`: accepts `2dgs`; the seeded third scale is never longest.

### 3. Engine wiring (done)
- `EngineForward` / `EngineLoss` / `EngineOptim` / `EngineTrainStep`: add the
  `2dgs` branches next to `3dgut`; allocate the normal render; pass it as
  `pred_normal`.
- Densify scoring needs no 2dgs case: `geom` only combines the per-camera
  loss-map scores, and the world-grad score (`|dL/dmean| * exp(max log
  scale)`, both backends) sees a disc axis, the third scale being -20.
  Sparse seed, 3000 steps to 1M, 61 held-out views: 2dgs at 23.49 dB with
  the default, 23.44 `geom`, 23.60 / 23.51 world-grad blend 0.5 / 1; 3dgut
  24.02 / 24.04 default / blend 1. No NaNs.
- First trainable milestone, on both backends: Mip-NeRF 360 within ~0.5 dB
  of 3DGS, step time within ~15%, CUDA and Vulkan renders in parity.

### 4. Vulkan backend (done)
- `kSurfel` on the eval3d raster entries rather than new entries (no new
  `StepTimingTable.h` rows); `kEval3d` 0/1/2 in projection. 2dgs never takes
  the `nodepth` backward, which skips the colour VJP that carries normals.
- Parity rows in `projection_parity`, `render_parity`, `raster_bwd_parity`.

### 5. Config, GUI, i18n (done)
- `TrainConfig.h`: `primitive` choices gain `2dgs`; the `2dgs` preset
  (`spirula train 2dgs`) sets depth distortion 0.1 and normal consistency
  0.04 (done, see "Preset weights").
- `TrainFields.h` and the GUI's primitive lists (training, viewer, render
  panel): names and help in all 13 languages. The render panel's automatic
  choice keeps a 2dgs model as 2dgs on any lens.
- `SettingCosts.h`: 2.07 raster forward, 2.24 backward against 3dgs, from
  one fisheye capture (docs/notes/step-timing.md); re-measure on garden /
  bonsai in phase 7. The VRAM forecast needs nothing: the normal render and
  its gradient are Image-category pool slots, which it measures.
- Resume needs nothing: the primitive comes back from `config.json` and the
  storage is 3DGS's (checked: a 2dgs run resumed at step 300 trains on).
- Plain `--primitive 2dgs` keeps the shared normal consistency 0.04: it is
  most of the mesh gain in the sweep below (F 0.539 to 0.674) for 0.6 dB,
  and a default that changed with the primitive would be a hidden rule.

### 6. Export and meshing (done)
- PLY export (done): the checkpoint's `splat.ply` keeps `scale.z`, and
  `--init-ply` re-seeds it for 2dgs. A 3000-step model (23.49 dB, 61
  held-out views) reloaded for one step scores 23.26 as 2dgs, 23.46 as
  3dgut, 22.71 as 3dgs; a 3dgut model loses 0.11 dB the same way.
- Dead discs (fixed): relocation and the export call a splat dead when its
  largest log scale is at the -40 floor, which a disc never reached with
  `scale.z` at -20. A 3M-splat 30k-step model held 306k collapsed discs
  (10%). The optimizers now keep `scale.z` at or below the disc's larger
  axis, so a collapsed disc reads as dead.
- The fix is neutral: `spirula train 2dgs`, 30k steps, 61 held-out views,
  23.31 dB / 0.881 / mesh F 0.705 without it and 23.33 / 0.881 / 0.697
  with it, collapsed discs 7150 and 15. Both sit 0.4 dB under the sweep's
  23.75 / 0.899 below; that gap predates the fix and is unexplained.
- Meshing (done): `src/mesh/` renders moments through 3DGUT whatever the
  primitive. A flattened 3DGUT splat is evaluated at the ray-disc hit, and
  the meshes above reach the sweep's F, so no 2dgs moments kernel. DTU
  Chamfer against meshing a 3DGS model is left to phase 7.

### 7. Validation and docs
- Final table: 3DGS vs 2DGS on both backends — PSNR / SSIM / LPIPS, DTU
  Chamfer, step time, peak VRAM.
- Move what stays true from this plan into `docs/architecture.md`
  ("Primitives") and a `docs/notes/2dgs.md`; delete this plan.

## Preset weights

The paper's weights do not carry over: this engine's distortion is in log
depth (`W*S - C^2`), its normal loss adds `sqrt((1 - cos) / 2)` to the
paper's `(1 - cos) / 2` and so pulls far harder near agreement, and both
ramp in linearly over `distortion_reg_warmup` (6000 steps).

Sweep on the action-cam capture (30k steps, 1M splats, Vulkan, 61 held-out
views). Geometry: `spirula mesh` scored by `reference/python/mesh_fscore.py` against the capture's dense matching cloud,
tolerance 0.5% of its 5-95% extent, 1M points sampled each way, precision
= mesh points within it, recall = cloud points within it; one run each, so F moves by ~0.02
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

The preset takes the best mesh, 0.1 / 0.04. Distortion 0.1 adds 8% GPU time
a step (`SS_PROFILE=1`, 2000 steps; the raster backward +26%). To validate
on Mip-NeRF 360 / DTU (phase 7) before the PR.

## Open questions

- Is the screen-space low-pass needed with the engine's existing
  antialiasing / `kernel_cut`, or does it fight it?

