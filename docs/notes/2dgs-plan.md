# 2DGS primitive — implementation plan

Goal: `--primitive 2dgs`, a surfel primitive in the style of 2D Gaussian
Splatting (Huang et al., SIGGRAPH 2024), trained by the same engine at close
to 3DGS speed and memory, on both backends.

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
   gets zero gradient; it is set to `log(1e-3 * min(sx, sy))`-ish on export so
   3DGS viewers draw flat discs.
2. **Screen buffer = 3DGUT's layout** (`SCRG_*`). The projection computes the
   disc's screen bounds and sorting depth; the fragment is rebuilt from world
   data in the raster kernel, as 3DGUT does. Reusing the layout keeps
   `projective_depth`, the qgrad path and the fused projection-bwd optimizer
   reachable with a new primitive type rather than a new buffer format.
3. **Fragment math** (shared Slang, `src/shaders/primitive_2dgs.slang`):
   - tangent frame `tu, tv, n` = columns of `R(quat)`; plane through `mean`
   - `t = dot(mean - o, n) / dot(d, n)`, `p = o + t d`,
     `u = dot(p - mean, tu) / sx`, `v = dot(p - mean, tv) / sy`
   - `G = max(exp(-(u²+v²)/2), G_lowpass)` where `G_lowpass` is the paper's
     screen-space fallback: ray-to-centre distance divided by the pixel
     footprint at that depth, so it works without a pinhole `K`
   - outputs: `rgb`, `depth = t` (same convention as 3DGUT's eval — check
     ray vs. axial below), `normal = -sign(dot(d, n)) * n` in world space
   - grazing rays (`|dot(d, n)|` small) fall to the low-pass term, not a NaN
4. **Normal frame**: decide once whether the rendered normal is world or camera
   space by matching what `depth_to_normal_forward` and the GT normal loaders
   produce; convert in the fragment, not in the loss.

## Phases

### 0. Baseline
- Fork, branch `2dgs`.
- Record 3DGS and 3DGUT numbers on a Mip-NeRF 360 scene and a DTU scan (PSNR,
  step time, peak VRAM) on both backends with `reference/python/benchmark.py`,
  so every later phase has something to compare against.

### 1. Shared math + gradient test
- `primitive_2dgs.slang`: bounds/projection, `evaluate_alpha_2dgs`,
  `evaluate_color_2dgs` (rgb, depth, normal) and their `_vjp`s.
- Unit test: VJPs against central finite differences, including grazing and
  near-centre rays, compiled for both backends.

Both backends are required: Vulkan is where training is iterated on, CUDA is
where long runs happen. Phases 2 and 4 therefore land together, and the
parity tests (CUDA vs Vulkan on one machine) gate each step rather than
coming at the end.

### 2. CUDA backend
- `primitives/Primitive2DGS.cuh`: `pixelType = RGB_DN`, `World` / `Screen`
  from the 3DGUT bases, `FragmentFwd` / `FragmentBwd` calling the Slang math.
- Projection fwd / packed fwd / bwd / qgrad for the new type; fused
  projection-bwd optimizer dispatch.
- Raster: instantiate eval3d fwd/bwd and moments for 2DGS, with `DN` and
  `RGB_DN` distortion enabled for this primitive only.
- Run codegen (`generate_headers.py`, `generate_kernel_instantiation.py`,
  `generate_backend_api.py`); watch the instantiation count (FPBO is already
  133 TUs).
- `long_axis_split`: accept `2dgs`; splitting along the longest axis is
  already in-plane because `z` is tiny.

### 3. Engine wiring
- `EngineForward` / `EngineLoss` / `EngineOptim` / `EngineTrainStep`: add the
  `2dgs` branches next to `3dgut`; allocate the normal render; pass it as
  `pred_normal`.
- Densify scoring: confirm the loss-map accumulation (`densify_accum_mode`)
  behaves; `densify_score_blend_world_grad` and `geom` mode read projection
  gradients and need a check.
- First trainable milestone, on both backends: Mip-NeRF 360 within ~0.5 dB
  of 3DGS, step time within ~15%, CUDA and Vulkan renders in parity.

### 4. Vulkan backend
- `rasterize_fwd.slang` / `rasterize_bwd.slang`: a `2dgs` entry beside
  `rasterize_fwd_3dgut`; the shader header says every primitive is RGB_D, so
  the normal output channel and `dist_spec` DN variants are new work.
- Projection shaders and launchers (`backend/vulkan/kernels/*.cpp`).
- A row in `engine/StepTimingTable.h` per new kernel.
- Parity: extend `engine_render_parity` and the raster parity tests to the
  new primitive; `generate_vulkan_stubs.py` stays clean.

### 5. Config, GUI, i18n
- `TrainConfig.h`: `primitive` choices gain `2dgs`; a preset with 2DGS defaults
  (paper: depth distortion 1000 bounded / 100 unbounded, normal consistency
  0.05 from step 7000), re-tuned to this engine's loss normalisation.
- `TrainFields.h` and the GUI primitive combo: names and help in all 13
  languages, within the UI text budget.
- `SettingCosts.h`, `TrainForecast` (the normal render is 3 more floats per
  pixel), resume compatibility in `checkpoint/Resume.cpp`.

### 6. Export and meshing
- PLY export with the flattened `scale.z`; reload round-trip.
- Meshing: feed rendered depth + normals of a 2DGS model to `src/mesh/`;
  compare DTU Chamfer against meshing a 3DGS model.

### 7. Validation and docs
- Final table: 3DGS vs 2DGS on both backends — PSNR / SSIM / LPIPS, DTU
  Chamfer, step time, peak VRAM.
- Move what stays true from this plan into `docs/architecture.md`
  ("Primitives") and a `docs/notes/2dgs.md`; delete this plan.

## Open questions

- Depth convention: is eval3d `depth` ray distance or axial `z`, and what do
  the GT depth loaders and `depth_to_normal_forward` expect?
- Is the screen-space low-pass needed with the engine's existing
  antialiasing / `kernel_cut`, or does it fight it?
- Sorting: per-splat centre depth (as 3DGS) is what the paper uses; keep it.
