# Sky model

`--create-sky-splats` prepends a fixed sky model to the field, so the background
renders as part of the model from the first step.
`--sky-resolution` sets its size: low 32,768, medium (default) 65,536, high
131,072, ultra 262,144 rows. Off by default; `spirula train --help` lists both.

Rows are denser near the horizon: a latitude warp gives 10x the uniform row
density at the equator; splats there are at least 0.45x the base spacing, and
1.7x near the poles. At every tier the horizon band therefore gets more
samples than an even sphere would. The bake then median-filters the horizon
band's DC colours, so an isolated splat whose colour is an outlier becomes
its neighbours' colour instead of a pit or spike.

`src/app/SkyModel.{h,cpp}` owns geometry and CPU color accumulation, with no
image I/O or engine dependency. `src/app/SkyBake.{h,cpp}` loads the dataset's
training images and drives that accumulation. `TrainerCore` attaches the baked
rows to the seeds and reports progress.

The core separates sphere sampling, per-view contributions and uncovered-row
color completion. Both modules use `src/app/SkySampling.h` for RGB interpolation.
Baking clamps to the final cell; resizing permits a repeated edge pixel. The
resize map caches half-pixel coordinates by source/target dimensions and reuses
its axis storage when dimensions change.

## Recipe

`src/app/SkyModel.{h,cpp}`. From the sparse cloud:

- `radius = 1.5 * max |p|` over the seed points. An empty cloud (or one at the
  origin) builds nothing and logs why.
- Positions on a golden spiral (`theta = (3 - sqrt(5)) * pi * i`, pole axis
  +Y) at that radius; identity rotation; one log scale for every row,
  `ln(1.5 * sqrt(4 r^2 / N))`; opacity `+inf`, which is `sigmoid(+inf) = 1`
  exactly, so the rows are fully opaque with no NaN path.
- SH DC colour: each training image is projected through the camera model its
  frame declares, sampled bilinearly at the pixel centre, and averaged per row.
  A row no image covers copies the nearest covered row; if no image covered
  anything the DC is 1.

The projection is the renderer's own: the camera record is `ParsedDataset::c2w`
(OpenGL, three rows of four floats), the ray is taken in CV axes (`x`, `-y`,
`-z` of its columns), and the row is `p - centre` rotated into that basis.
Passing the record through unconverted, or with the wrong axis signs, still
covers rows and still produces a colourful sphere -- it just samples the wrong
pixels, so nothing looks broken until you compare the colours with the
photographs. `sky_model_test` pins the conversion with a real camera record and
a row on its axis.

The bake runs once at field creation, before the engine is seeded. OpenMP
distributes independent rows within each image; images and each row's additions
remain ordered. Small inputs, nested parallel regions and builds without OpenMP
use the serial loop. A fully covered bake skips nearest-neighbour preparation
after colour normalization; a partially covered bake distributes independent
nearest-neighbour queries when at least 4,096 rows need filling. The photo loader
retains decoder-owned pixels through accumulation and reuses two transform
buffers, keeping the same decoder, quarter turns and bilinear resampling.
A resumed run restores the whole field, sky rows included, from its checkpoint
without rebaking.

The bake averages display-referred sRGB/Rec.709 samples. Before insertion,
`TrainerCore` converts that color to the resolved splat gamut, transfer and
storage encoding through the existing seed-color conversion. Default sRGB
storage skips this conversion.

Seed buffers include the sky's extra rows from their first allocation. Ordinary
seeding keeps its original indices and RNG order; the live rows move backwards
in place when the sky prefix is filled. Capacity and zero-filled padding match
the layout without this allocation optimization, including PLY warm starts and
appended point seeds.

`sky_model_test` checks geometry, projection, sampling, serial/parallel sums,
hit counts and nearest-colour fills. It links CPU sky/camera math and optional
OpenMP. `--benchmark` times row batches and full/partial-coverage
finalization.

`sky_setup_test` checks mixed PNG/TIFF loads, all quarter turns, fractional
up/downscaling including edge pixels, decode
failures and reserved seed layouts against ordinary point/PLY seeding.
It also checks display-color preservation across splat color spaces and
rejects an unfused optimizer when a checkpoint supplies sky rows.
`--benchmark-seeds` exercises a one-million-row seed capacity. Both tests accept
`--snapshot <file>` to write their buffers for before/after comparison.

## The frozen prefix

Sky rows are rows `[0, num_sky_splats)` and nothing else identifies them -- no
per-splat flag, no predicate. The count is engine state
(`EngineState.h::num_sky_splats`), validated 256-aligned at setup, and written
to the checkpoint; a checkpoint without the record loads as 0.

Training treats those rows as static geometry:

- **optimizer**: the fused projection-bwd+optim kernels
  (`FusedProjectionBwdOptim_kernel.cuh`, `backend/vulkan/shaders/fpbo.slang`)
  and the geometry kernels (`FusedGeometryOptim.cu`,
  `optim_geometry.slang`) return early for every 256-row block below the
  prefix, before any reduction or bounds write.
- **densification**: every kernel that could move, split, revive, clip or fade
  a row takes the prefix boundary (`first_trainable_splat`) and skips it --
  see `src/kernels/densify/*.cu` and `backend/vulkan/shaders/densify.slang`.
  Scores and draw weights of frozen rows are held at 0. Split draws rank
  frozen rows after every trainable row and cap additions at the number of
  trainable parents, including when positive weights run out. Relocation
  only uses the sampler's strictly positive eligible weights.
  Learned-score updates also zero the prefix scores and exclude those rows
  from the prior statistics.
- **region**: sky rows are always "inside", so the region test costs them no
  work and the outside-opacity decay never touches them.

`src/backend/tests/densify_parity.cpp` checks the invariant directly: with a
frozen prefix, the noise kernels must leave those rows bit-identical.
It also covers relocation with opacity and peak-weight death tests;
sparse-weight split draws leave the prefix unchanged. `optim_parity` checks
learned-score exclusion and reduction counts.

All four sizes are multiples of 256, the block the optimizer and densify
kernels skip. A resumed run keeps the checkpoint's prefix; the setting applies
to a fresh run.

## Restriction

Sky splats require the fused projection-bwd+optim path
(`--use-fused-proj-bwd-optim`, on by default) because only those kernels carry
the block-level freeze. `train_config_unsupported()` refuses
`--create-sky-splats` together with the non-fused optimizer (the
`academic-baseline` preset) rather than half-supporting it.
Insertion also validates the actual prefix, so disabling creation on resume
does not bypass the optimizer requirement for restored sky rows.
