# Bundle adjustment solver (`src/sfm/ba/`)

Levenberg-Marquardt with a Schur complement over the points. The reduced camera
system is solved either by a from-scratch, fully in-place blocked dense
Cholesky, or -- for large camera counts -- by a matrix-free implicit-Schur PCG
that never forms the reduced matrix, keeping VRAM linear in the observation
count. Selection is automatic by problem size and VRAM budget (`--solver` to
force). All GPU code is [Slang](https://shader-slang.org/) compiled to SPIR-V;
Jacobians come from Slang's autodiff. The same solver also exists on the host,
for devices that can run none of the scalar configurations -- see "Host
fallback".

This is the last stage of the SfM pipeline and the one it was built around; the
mapper drives it through `sfm/map/Bundle.h`. For the pipeline as a whole see
[../README.md](../README.md).

## Layout

```
sfm/shaders/
  ba/ba.slang        kernels: cost, Jacobian+assembly, Schur, updates; bindings & push constants
  ba/cholesky.slang  in-place packed dense Cholesky + triangular solves
  ba/cg.slang        implicit-Schur block-Jacobi PCG (device-side loop, no host round-trips)
  common/camera.slang  pose + ICameraModel interface + every camera model
  common/loss.slang    ILoss: trivial / Huber / Cauchy (selected at shader compile, param at runtime)
  common/real.slang    Real = float | double | DF, atomic accumulation API, rexp/rsin/rcos/rlog
  common/df.slang      emulated double ("double-float" fp32 pair) with differentiable ops
  common/dmath.slang   double-precision exp/log/sin/cos (GLSL.std.450 lacks them for fp64)
  common/linalg.slang  RVec2/RVec3, 3x3 inverse
sfm/ba/
  Problem.h          model registry, camera groups, column layout, per-model obs lists,
                       BAL problem loading
  Options.h          scalar config, solver selection, options and stats
  Solver.h           LM driver (records each iteration in watchdog-sized submits)
  SolverCpu.h        the same solver on the host -- see "Host fallback" below
  CpuCamera.h        host mirror of the camera and loss models, forward-mode duals
  CpuDense.h         packed blocked Cholesky for the host path
  CpuParallel.h      the process-wide worker pool the host path runs on
src/app/cli/sfm_ba.cpp   the `spirula sfm ba` subcommand: a model's global BA, or a BAL problem
```

`spirv_tool nocontract` (src/backend/vulkan/shaders/) is the SPIR-V post-pass
the `df` config requires -- see "Scalar configs" below. `tools/sfm/genpoly.py`
regenerates the minimax transcendental coefficients in `df.slang` /
`dmath.slang` (`--survey` for error tables).

## Build & run

```bash
bash build_develop.bash -DSS_BACKEND=vulkan
./build_vulkan/spirula sfm ba /path/to/sparse/0 refined/0        # a COLMAP model
./build_vulkan/spirula sfm ba /path/to/sparse/0 /path/to/sparse/0 # ... in place
./build_vulkan/spirula sfm ba /path/to/sparse/0 refined/0 --real cpu   # ... on the host
./build_vulkan/spirula sfm ba problem-16-22106-pre.txt out.ply --real df --loss huber
./build_vulkan/sfm_cholesky_test 500 --real df          # dense solver unit test
./build_vulkan/sfm_ba_cpu_test                          # host solver vs a written-out reference
```

`ba` reads a COLMAP sparse model, runs the global BA the mapper runs on it
(`sfm/map/Bundle.h`, Huber 2 px unless `--loss` says otherwise, the rigs in the
model's `rigs.txt` unless `--rig` names others) and writes the model to the
output directory, which may be the input. It goes through the mapper's own
device-failure path (`solveBundle`), checkpoint resume included. That is also
how the solver is profiled on real captures instead of on BAL, which has
neither shared intrinsics groups nor any camera model but Snavely's; a BAL file
as the input writes the refined points as a PLY instead.

The shader variant matrix can be trimmed for faster iteration:
`-DSS_SFM_REALS=df -DSS_SFM_LOSSES=trivial`. slangc is taken from PATH
or downloaded into the build tree (`cmake/SsSlang.cmake`).

Options: `--real float|double|df|cpu`, `--loss trivial|huber|cauchy`,
`--loss-param X`, `--model snavely|snavely_f`, `--shared-intrinsics`,
`--max-iters N`, `--damping X`, `--rtol X`, `--patience N`, `--rig SPEC`,
`--solver auto|dense|cg`, `--vram-budget MB`, `--cg-iters N`, `--cg-tol X`,
`--cg-fallback auto|on|off`,
`--device I`, `--validate`, `--quiet`, `--profile` (per-kernel GPU time
breakdown from timestamp queries, printed after the solve),
`--spv-path FILE` (load a hand-compiled module instead of the embedded one).

## Design notes

### Parameterization & camera groups

Each *frame* has a 6-DOF pose (angle-axis + translation); an image is a frame
unless a rig says several images share one (below). Intrinsics live in
*groups*: any number of images can share one intrinsics block, and each group
chooses its own camera model (models may have different parameter counts).
The camera-side unknown vector is `[frame poses (6 each) | free member
extrinsics (6 each) | all free group intrinsics]`;
the Schur-reduced normal matrix over that vector is assembled directly into a
packed dense lower triangle via per-observation global column maps
(`sfm/ba/Problem.h` builds them). Only the cost/Jacobian kernels are specialized per
model; Schur assembly, the dense solver, and all updates are model-agnostic
(dynamic block widths, bounded by `kMaxCamDof`). `--shared-intrinsics` (all
images in one group) exercises the pose/intrinsics cross-coupling path.

### Rigs

A rig member is a `cam_from_rig` shared by every frame of the rig, so an
image's pose is `member ∘ frame` and its Jacobian block is `[frame 6 | member
6 (when refined) | intrinsics]` -- up to 24 wide, one more dof tier (`_x`)
that only a rigged problem selects; the CG kernels gain the same two tiers
(`_w` = every problem without a refined member, `_x`) so a rig-free problem
runs the same code it always did. Observations are bucketed by (model, rig)
and the rigged bucket runs a `*_rig` entry point that composes the member
before projecting; the plain bucket's kernels are unchanged.

Two things follow for the linear solvers. A multi-image frame or a refined
member is a shared column set, so a rigged problem is never *exclusive* and
takes the atomic Schur kernel or CG, as every shared-intrinsics capture already
does. The CG preconditioner's shared partition is one 6x6 block per frame, one
per member and one per group; `cg_bmul` stores a frame's rows only when the
frame has one image and accumulates otherwise. The host solver's tasks own
whole frames (a split inside one would race on the frame's rows), the member
columns join the shared-row buffer beside the shared intrinsics, and two
images of one frame fold both orderings of their cross term onto the diagonal
frame block. `sfm_ba_cpu_test` checks all of it against the written-out normal
equations; `sfm_rig_test` checks the device against the host.

The fp64 kernels and the host agree on the assembled `S` and `g` to about
1e-7 relative, not to rounding, on rig-free problems as much as rigged ones
(measured before rigs existed); final costs agree to seven digits.

### What the Jacobian pass stores (and what it does not)

Everything downstream of the Jacobian evaluation wants the camera-point block
`A_cp = Jc^T Jp` (`dof x 3` per observation). It is never materialized. The
pass stores what it evaluated -- `Jc` (2 x dof), `Jp` (2 x 3) and the weighted
residual -- and every consumer factors its product through the 2-vector
residual dimension instead of the 3-vector point dimension:

    A_cp^T x       = Jp^T (Jc x)                      (cg_gather, dp_accum)
    A_cp v         = Jc^T (Jp v)                      (cg_scatter)
    A_cp W A_cp'^T = Jc^T (Jp W Jp'^T) Jc'            (every Schur kernel)

Since `dof > 3` this is smaller *and* cheaper: `3*dof` scalars per observation
become 6, the prepass product `T = A_cp W` (another `3*dof`) becomes the 6
scalars of `Y = (Jp W)^T`, and the inner product of a Schur block element drops
from three terms to two. At `dof = 16` it is 2.7x less per-observation memory
on the dense path and 2x on CG. The 2x2 middle matrix `Jp W Jp'^T` is formed
once per contributing pair of observations, in shared memory, and contracted
with `Jc` before the element loop touches it (`Z = Q^T Jc_i`).

The pass also does *no* `S`/`g` accumulation on any path -- the Schur kernels
own it (below), which is what lets a rejected step re-run the whole assembly
from the stored Jacobians instead of restoring a snapshot of `S`.

### Schur assembly (pair-aggregated)

The Schur products `S -= A_up (App+l)^-1 A_up^T` are accumulated per unordered
*image pair*: when every image owns its intrinsics group, each element of S
belongs to exactly one pair, so one workgroup (warp) per pair sums the
contributions of all points seen by both cameras in registers and writes its
`dof_i x dof_j` block back with plain read-modify-writes -- no atomics.
`sfm/ba/Problem.h` builds the per-pair `(obs_i, obs_j)` entry lists host-side (a
counting sort by pair key); pairs with more than 1024 entries are split into
chunks that fall back to atomic accumulation, as does the whole kernel when
groups are shared between images (`--shared-intrinsics`), when the entry list
would be unreasonably large, or when it would not fit the VRAM budget
(`schur_obs`, one thread per observation -- the entry lists and `Y` are the
only memory the pair path costs, so it degrades to the atomic kernel rather
than to CG). A prepass (`point_prep`/`y_prep`) caches `W = (App+l)^-1` per
point and `Y` per observation, so the pair kernel is a dot-product sweep over
its staged blocks. This replaced a per-observation kernel issuing ~45 global
atomics per obs pair and was worth 3-6x on the Schur stage alone.

The `S += Jc^T Jc` / `g += Jc^T r` accumulation is folded into the same
kernels: a diagonal pair's entries are exactly the observations of that camera,
so the diagonal-pair workgroup sums the blocks from the stored `Jc` and
residual (folding in the `(1+lambda)` damping, so no separate `damp_S` pass
exists), and `schur_obs` does the same per observation. Keeping it out of the
Jacobian kernel matters enormously for the emulated `df` config, where atomics
are CAS loops: ~63 per obs onto ~50 hot addresses per camera made the Jacobian
kernel ~9x slower than the same kernel with atomics removed (measured);
float/double use native atomic-add hardware and barely noticed.

### Groupshared budget

The module has three groupshared pools -- the Cholesky tiles, `schur_pair`'s
staging, and a small one everything else reduces through -- and not one, even
though their sum is what counts against `maxComputeSharedMemorySize`. A driver
gives a pipeline the shared memory its entry point *reaches*, and the Cholesky
pool is 16.9 kB at fp64: a 32-thread workgroup carrying it fits about five to
an SM. That costs nothing for a kernel that never touches shared memory, and
2.2x for `schur_pair`, which lives on it. The three together are ~26 kB at
fp64, inside every device's 32 kB floor.

Adding a camera model = a struct conforming to `ICameraModel` (project +
static param count) + two one-line entry points in `ba.slang` + one registry
line in `sfm/ba/Problem.h`. Different intrinsic counts need no other changes.

### Scalar configs

Everything on the device is written against `Real`, selected per-module with a
`-D` flag (`cpu` is not one of them -- it is the host solver, below):

- `float` — fp32 + native f32 buffer atomic add (`VK_EXT_shader_atomic_float`)
- `double` — fp64 + native f64 atomic add. GLSL.std.450 does not define
  exp/sin/cos/log for fp64 (slangc silently emits invalid SPIR-V), so
  `dmath.slang` provides argument-reduction + polynomial versions with custom
  autodiff derivatives.
- `df` — emulated double: an fp32 (hi, lo) pair with ~49-bit significand
  (QD-style error-free transforms), for GPUs with weak/absent fp64. All ops
  carry custom backward derivatives, so the same autodiff camera code runs
  unchanged. Atomic accumulation packs the pair into a `uint64` CAS loop.
  Transcendentals use near-minimax (Chebyshev) fits on the reduced range
  instead of Taylor series (exp: degree 10 vs 14, sin/cos: 6 vs 8 at the same
  interval accuracy), with the high-order tail terms (contributing < ~6e-9)
  evaluated in plain fp32; `rsincos` computes sin and cos together, forward
  and backward. The double config's `dmath.slang` gets the same treatment.
  **Important:** drivers may contract/simplify float expressions, which
  silently destroys two_sum/two_prod (observed on NVIDIA: results collapse to
  fp32 accuracy). `spirv_tool nocontract` decorates every float arithmetic op
  with `NoContraction` in the df SPIR-V (a build step, applied only to the df
  blobs); with it, the df Cholesky selftest hits ~1e-11 relative error at
  n=500 vs ~4e-4 without it and ~7e-4 for fp32.

Buffers that receive atomic accumulation are bound twice (Real view + atomic
word view of the same `VkBuffer`), so the emulated config needs no separate
code paths in the kernels.

### Dense solver (from scratch, in place)

The reduced system (dimension `n = 6*images + total intrinsics`) is stored as
a packed lower triangle (`n(n+1)/2` scalars — half the memory of a square
matrix) and factored in place with a blocked right-looking Cholesky
(block 32): `chol_panel` (row-tile per workgroup trsm) → `chol_update`
(32x32-thread tile SYRK/GEMM from shared memory; the workgroup owning the
next diagonal tile also factors it in place, so the standalone `chol_diag`
only runs for the first block). Shared-memory tiles are padded to a 33-wide
pitch (bank-conflict-free; 2x on the fp32 update). The forward/backward
substitutions are one fused dispatch per block: every workgroup redundantly
solves the 32x32 diagonal system in shared memory (there is no cross-workgroup
ordering inside a dispatch) and updates its slice of the RHS, ping-ponging
between the gradient vector and a small scratch vector. The step ends up in
the gradient vector in place. `sfm_cholesky_test N` validates factor+solve
against a CPU reference on a random SPD system.

At fp64 the trailing update runs at ~80% of the RTX 4080's (1/64-rate) fp64
ALU peak, i.e. the factorization is compute-bound and further blocking would
not help; at fp32 it is bandwidth-bound.

On the dense path, peak VRAM is dominated by the packed S, the
per-observation Jacobians (`2*dof + 8` scalars/obs) and, when the pair
assembly is on, `Y` (6 more) and the Schur pair-entry lists.

### Implicit-Schur PCG (large camera counts)

The dense factor is cubic in the number of cameras (measured ~91% of total
GPU time at 1936 cameras, fp64) and the packed S is quadratic in VRAM, so
above `n_dim = 8192` (~900 cameras) the solver switches to a matrix-free
preconditioned conjugate gradient on the same reduced system (`cg.slang`,
`--solver` to override). S is never formed:

    S x = B x - sum_p Acp_p W_p (Acp_p^T x)

is applied through the per-observation `Jc`/`Jp` and per-point `W` that the
assembly already produces (`A_cp` is implicit -- see above) -- a per-point
gather (`cg_gather`, one thread per point over its contiguous track) and a
per-camera scatter (`cg_bmul` + `cg_scatter`, one warp per chunk of a camera's
observation list with lane-per-element coalesced `Jc` loads and a handful of
atomic adds per chunk). `B` (the block-diagonal-per-image camera Gram matrix,
which is exact: no observation couples two images' poses) and the diagonal
blocks of `S` come from `cg_cam_diag` -- the same per-camera accumulation as
`schur_pair`'s diagonal pairs, chunked over an obs-grouped-by-image CSR built
host-side.

The preconditioner is block Jacobi over a *partition* of the camera columns,
which is where sharing shows up. With per-image intrinsics groups the partition
is one block per image over `[pose | intrinsics]`, and the blocks are the exact
diagonal blocks of `S`. When a group is shared -- which is every ordinary
capture, one camera behind thousands of images -- those blocks would overlap on
the shared columns, so the partition splits at the seam: a 6x6 block per image
pose plus one block per group, with the pose/intrinsics coupling dropped and
the group block's cross-observation terms approximated. It stays symmetric
positive definite, which is all a preconditioner owes anyone, and it costs a
few CG iterations. The operator itself is unaffected: `cg_cam_diag` and
`cg_bmul` accumulate the shared columns atomically instead of storing them.

Before this, sharing forced the dense path, and with it a cubic factorization
of an `n_dim` that a real capture drives into the tens of thousands: a
4194-image reconstruction (`n_dim = 25184`) took 94 s and 8.4 GB per global BA
against 2.6 s and 3.2 GB on CG, at the same final cost.

The entire CG loop is recorded in the iteration's command buffer: scalar
state (rho, alpha, beta, tolerance) lives in a small device buffer, dot
products are two-stage reductions, and every kernel no-ops once the
convergence flag is set, so a fixed iteration cap is recorded (adapted each
LM iteration from the previous count) and the LM loop keeps its single
cost readback.

Stopping rule: relative residual `--cg-tol` (default 0.1, inexact-Newton
style -- LM's accept/reject guards the step quality), or, whichever comes
first, the last step lowering the quadratic model `Q = x^T S x / 2 - g^T x` by
less than `SolverOptions::cg_model_tol` (default 0.1) of the average step so
far -- Nash and Sofer's truncated-Newton test, `i (Q_i - Q_i-1) / Q_i`, which
is how Ceres stops its CG under LM. `Q` costs nothing to track: a CG step
lowers it by `alpha rho / 2`. The residual test alone keeps iterating after
the steps have stopped mattering: on the 4102-image capture below the model
test takes 40% fewer iterations (2376 against 3954 over a 50-iteration solve,
34 s against 55 s) to the same final cost, 1.879209721e6 to all ten digits
printed. It settles for a residual near the square root of its tolerance,
though, so a caller that wants the exact step (the tests, `SS_SFM_CMP_STEP`)
sets it to 0. `--cg-iters` caps both (default 100).

### Coarse correction (two-level preconditioner)

Block Jacobi sees one camera at a time, and the slow modes of a long capture
are not local: a stretch of the trajectory bending, twisting or drifting in
scale *as a whole*, which every camera block finds nearly free. On a
4102-image dataset, CG hit its 100-iteration cap from the
fourth LM iteration on, and the truncated steps fell behind the dense
solver's: 1.879283e6 after 10 LM iterations against its 1.879271e6. Holding
the intrinsics fixed changed nothing, so it is not the dropped pose/intrinsics
coupling.

So the preconditioner has a second level: `M^-1 = M_J^-1 + P A_c^-1 P^T` with
`A_c = P^T S P`. `P` gives every cluster of `k` consecutive frames 7 dofs, a
similarity motion of the world (rotation `w`, translation `tau`, scale `s`),
mapped onto each frame's pose parameters exactly: `d(angle-axis) = -Jr^-1 w`,
`dt = s t - R tau` (`tc_basis`, in fp32 -- the basis only has to span the slow
modes). A constant pose delta per cluster instead was measured and is clearly
worse (66 and 100 CG iterations where the similarity basis takes 35 and 50):
it cannot move a cluster rigidly unless every frame in it faces the same way.

`A_c` comes from the same per-observation Jacobians as everything else: the
`B` part per image from `cg_cam_diag`'s Gram blocks (`tc_bpart`), the Schur
part per pair of *runs* -- a track's observations inside one cluster, which
are contiguous since tracks are sorted by image -- grouped by cluster pair on
the host so that one warp sums a coarse block and writes it once
(`tc_schur`; issuing its sums as atomics per point was 145 ms a build against
80). It is factored by the dense kernels in the packed-`S` buffer the CG path
otherwise leaves empty, and the factor is then inverted in place (`tc_dinv`,
`tc_inv_row`), so applying it is two triangular matrix-vector products
(`tc_lmul`, `tc_ltmul`, ~0.17 ms at 3591 dofs) rather than 2 n / 32 dependent
solve dispatches (~4 ms, more than half a CG iteration).

Size and cost: `k` is the smallest cluster that keeps `A_c` under 4096 dofs
and the run-pair table under two entries per observation (long tracks span
many small clusters) -- 8 frames on the 4102-image capture, 38 on a
22042-image one. The extra VRAM is the table and the frame basis; `A_c`
itself (67 MB at most) reuses packed `S`. A build is the size of a dense
factor, so it happens only when the last CG solve took more than 12
iterations, and is reused for up to three solves unless the damping has moved
tenfold -- a stale `A_c` only costs iterations.

One global BA each (`spirula sfm ba`, defaults, RTX 5070), before and after
this correction and the stopping rule above:

| capture | solver | CG its/solve | solve | final cost |
|---|---|---|---|---|
| 4102-image (well-conditioned) | cg | 89.8 -> 50.4 | 29.1 s -> 31.6 s (30 -> 49 LM its) | 1.879241e6 -> 1.879210e6 |
| 6946-image (dual-fisheye rig) | cg | 81.0 -> 41.8 | 97.6 s -> 51.2 s | 4.659347e6 -> 4.659219e6 |
| 22042-image ([campus](https://repo-sam.inria.fr/fungraph/hierarchical-3d-gaussians/datasets/)) | cg | 47.5 -> 73.7 | 120.2 s -> 193.4 s (47 -> 50 LM its) | 1.046347e7 -> 1.044832e7 |
| 1068-image (linear trajectory) | dense -> cg | -- -> 8.1 | 56.7 s -> 3.6 s | 1.822009e6 both |

Where a run got longer it is because it no longer stalls: the old 4102-image
solve stopped on tie-zone patience, and the campus one sat at 1.0483e7 from
iteration 12 to 20 where the new one passes its final cost at iteration 14
(about 55 s) and goes on improving.

On the 4102-image dataset the coarse-corrected CG tracks the dense solver's cost to
seven digits iteration for iteration (1.879220953e6 against 1.879220943e6 at
iteration 29), which the plain one never does; it then keeps improving where
the plain one stops on its tie-zone patience. `SS_SFM_BA_COARSE=0` turns it
off; the host solver has the same correction (`coarseBuild`).

The global similarity is a gauge freedom, so `A_c` is singular up to the
damping. Its diagonal is scaled by `1 + 1e-8`, and a pivot under 1e-6 of its
row's original diagonal is replaced by that diagonal (`pivot()` in
`cholesky.slang`), which keeps the factor bounded however far rounding has
made the matrix indefinite. If a CG solve still stops before its first step,
the iteration is redone without `A_c`, and it stays off for the solve when
that was the difference.

The CG path allocates no packed S, no Y, and no pair-entry lists, so VRAM
stays linear in observations. Preprocessing also skips the pair-table sort (3.6 s -> 1.2 s
there). Solver selection and the dense fallback are VRAM-aware: the budget
is `--vram-budget` (default 90% of the device-local heap); `auto` picks
dense for small problems, CG when the dense estimate exceeds the budget,
`n_dim > 8192`, or CG's estimated iteration costs under half the dense one's.
That last test is about track length: the dense assembly is quadratic in it
(`schur_obs` walks the track per observation), CG linear. A 1068-image
capture of a robot drifting slowly through one module, 70 observations per
track by `sum t^2 / sum t`, spent 3.7 s an iteration in `schur_obs` alone and
59 s a solve; on CG it is 6.3 s, to the same final cost. With `--cg-fallback on` (or `auto`, which enables it only
when dense+CG together fit in half the budget) the dense machinery is kept
allocated; a truncated-CG step is kept if it still lowered the cost, and
only a cost-raising capped solve is re-solved densely from the reused
assembly (three consecutive stalls demote the run to dense for good).
VRAM-constrained runs should use `--cg-fallback off`: LM's reject path
(escalating lambda re-solves against the same assembly) recovers from
inexact steps on its own, at zero extra memory.

`SS_SFM_CMP_STEP=1` (env) solves one assembly with both paths and prints the
step difference: with `--cg-tol 1e-8` the CG step matches the dense step to
~1e-8 at fp64.

Two guards keep a badly scaled problem from breaking CG. A 22042-image model
with a point 2e-9 from a camera centre has camera Gram entries of 1e23, and
below damping ~1e-9 rounding makes `S` indefinite there: the LM damping is
floored at 1e-8, which is still Gauss-Newton to eight digits, and a CG solve
that stops before its first step counts as a failed step (damping up) rather
than a zero one. The block-Jacobi factor floors a pivot under 1e-10 of its
block's largest diagonal and decouples it, where it used to cascade to NaN.

### Host fallback (`--real cpu`)

`pickRealForDevice` steps down `double` -> `cpu`, and `cpu` is the same solver
in fp64 on the host: same parameterization, same LM loop, same two linear
solvers, same automatic selection between them, same `BAOverBudget` contract.
Final costs match the fp64 GPU path to seven digits on real captures, and the
assembled `S` and `g` match an independently written unreduced normal-equation
reference to ~1e-15 (`sfm_ba_cpu_test`).

Neither fp32-based configuration is in that chain. `float` stalls the normal
equations above ~1e-7 relative accuracy, which is looser than the tolerance the
mapper's finishing passes ask for, and `df` pays for its ~48 bits with CAS-loop
atomics and emulated transcendentals. Both are still there to ask for --
`--ba-real df` is the right answer on a large GPU with no fp64 atomics -- but a
host solver that is fp64 throughout is the better default for a device that
cannot run `double`.

Three things differ from the GPU path, all of them consequences of running on
a CPU rather than choices:

- **No pair tables.** The Schur assembly is one task per *image*: for each of
  its observations it walks that point's track prefix, so an element of `S`
  belongs to the image pair that produced it and the pose rows are written with
  no atomics and no aggregation buffer. Columns owned by an intrinsics group
  that several images refine are the exception -- every element touching one
  lands in an intrinsics *row* after symmetrization -- and those go to a
  per-task `(shared columns) x n_dim` buffer that is summed afterwards. That is
  ~800 kB a task on a real capture, and zero when each image owns its
  intrinsics. The GPU's pair-entry tables, which cost 3.6 s and hundreds of MB
  to build, are never built.
- **Jacobians by forward-mode duals** (`CpuCamera.h`), against Slang's reverse
  mode on the device. The widths are small (3 + intrinsics) and it needs no
  generated code. The angle-axis block gets a dual pass of its own and the point
  block comes from the rotation matrix, because the axis norm has no derivative
  at a zero rotation -- which is every seed pair's first image -- and one pass
  over both would spread that 0/0 across the point and translation blocks
  instead of leaving it where reverse mode does.
- **A process-wide worker pool** (`CpuParallel.h`), not one per solver: the
  bottom-up phase runs a mapper per atom worker and a pool inside each would
  oversubscribe the machine by that factor. Solves too small to be worth
  splitting -- which is every atom -- run inline on the calling thread instead.
  `--threads` caps the tasks one solve splits into; `SS_SFM_BA_THREADS`
  overrides the pool's own width.

The dense factorization is `CpuDense.h`: the same packed lower triangle, a
blocked right-looking Cholesky (block 48) whose trailing update copies the
current block column out to a contiguous panel once per step, so both operands
of every tile update are unit-stride. 132 GFLOP/s at `n_dim = 6102` on an
i9-14900HX (32 threads), 17.6 single-threaded, which is ~88% of the SSE2
baseline's peak; `-march=native` measured 1.27x on top and is deliberately not
taken, since nothing else in the tree is built for it.

Measured against the fp64 GPU path on an RTX 5070 Laptop, same machine, one
global BA: a 152-image capture 1.85 s against 0.81 s, a 1015-image one
(`n_dim = 6102`) 3.6 s against 2.4 s, both at identical final cost. Peak
memory is slightly *below* the GPU path's, which uploads a copy of the problem
tables. `SS_SFM_MAP_PROF=1` prints the per-phase breakdown.

### LM loop

Multiplicative damping `diag *= (1+λ)` on both the camera and point blocks;
stop after `patience` non-improving iterations. λ update: /3 on a real
improvement (beyond `rtol`), held on a tie-zone accept, ×2 on the first
reject and doubling on consecutive rejects (up to ×32). Two details earned
their keep the hard way: shrinking λ on micro-improvements lets it collapse
until the solver stalls in an ill-conditioned plateau, and a flat gentle
reject multiplier recovers λ too slowly on ill-conditioned configs — hence
hold-on-tie plus escalate-on-reject.

Each iteration records assembly → Schur → factor+solve → point back-sub →
parameter updates → cost and reads back a single cost scalar for the
accept/reject decision. A small problem's iteration is one submit; a large one
is cut into submits of about 0.25 s of GPU time (see "Watchdog" below). Rejects restore parameters from
device-side backups — and since the restored parameters still match that
iteration's per-observation Jacobians (`Jc`/`Jp`/`res`/`App`, plus a `Bp`
snapshot), the retry skips the Jacobian pass entirely and re-solves with the
new λ -- and since the Schur kernels rebuild `S` and `g` from those on every
path, nothing else has to be snapshotted (the packed `S` snapshot the atomic
path used to keep was the single largest buffer on that path). Rejected steps are therefore cheap, which is what makes the fine λ
search affordable.

### Watchdog

A submit that runs past the driver's watchdog (2 s under Windows TDR and for
amdgpu on Linux) loses the device. One LM iteration of a 6946-image rig capture
(13 M observations, CG at ~80 iterations per solve) is 2.4 s of GPU time on an
RTX 5070, and a forced-dense one at `n_dim = 20864` is ~8 s, so both used to
reset any GPU with a watchdog, however fast. The solver now records through a
per-device `SubmitBudget` (`core/SubmitBudget.h`, `docs/notes/gpu-submit-budget.md`):
it cuts at barriers, splits the big per-observation, per-chunk and per-tile
launches into ranges (push constant `u4`), and reads the CG convergence flag
back at each cut so a converged loop stops recording. The cost model is
per-kernel weights measured on the RTX 5070 at fp64; since other devices differ
per kernel (df `schur_obs` on a 2-CU iGPU costs 21x its weight, because its
CAS atomics contend), each big kernel's first launch on a device is a 1/32-budget
range timed alone, and its weight is rescaled by what it took.

On a 22042-image capture (29.5 M observations, CG) the unsplit iterations were
40 of 42 submits over 2 s, 5.0 s the longest, on that same RTX 5070; split, the
longest of 678 is 0.32 s, and the solve takes 151 s instead of 212 s, since a
converged CG loop is no longer recorded out to its iteration cap.

A device solve also downloads its accepted parameters every 5 s
(`SolverOptions::checkpoint`), so when a device does fail, the host solve that
takes over (`solveBundle`) starts from that iterate and damping, not from the
beginning.

## Results (RTX 4080 Super + i9 32 threads, 50 LM iterations cap)

Trivial loss, Snavely (log-f) model, on BAL problems. The Ceres 2.2 reference
these were measured against (`DENSE_SCHUR`,
`dense_linear_algebra_library=CUDA`, 32 threads, autodiff) is not part of this
repository -- it needs a CUDA-enabled Ceres build, and the numbers below are
what it produced. The table itself was never committed; the notes are.

Notes:

- Final costs agree with Ceres to ~6 digits for `double` and `df` (and are
  usually slightly lower at the 50-iteration cap); `float` matches on small
  problems but stalls above ~1e-7 relative accuracy on larger ones (fp32
  normal equations), which is exactly the gap the emulated `df` type closes
  at fp32-class hardware cost.
- Per-kernel GPU timing is available via `--profile`. After the
  pair-aggregated Schur assembly, the deferred Jacobian accumulation and the
  fused Cholesky dispatches, `df` runs at fp64-config speed or better on
  every problem here (its Cholesky is cheaper than fp64's ALU-bound one, its
  Jacobian ~1.5x the fp64 one), which is the point of the emulated type.
- On problems with gross outlier observations (871's near-camera-plane
  points produce huge cancelling gradient terms), df's effective gradient
  noise is ~1e-2 on the affected components (identical before/after these
  changes, verified by dumping S/g), so which local basin a 50-iteration run
  lands in is trajectory luck at that precision; double is unaffected.
- The 1936-camera problem (auto -> CG): fp64 solves in ~4.4 s / 6.4 GB with
  the dense fallback allocated, or ~4.4 s / 2.2 GB with `--cg-fallback off`,
  vs ~228 s / 6.4 GB dense -- a ~50x end-to-end speedup at the same final
  cost (4.65037e6 vs 4.65035e6, both in the tie-zone plateau). df with
  `--cg-fallback off` reaches the best final cost of any config there
  (4.650363e6) in ~17 s / 2.2 GB; previously df could not run this problem
  at all (host OOM building pair tables alongside Ceres).
- **The VRAM figures above predate the implicit `A_cp`**, which cut
  per-observation memory ~2x on CG and ~2.7x on the dense path; the times are
  unchanged to within noise. Measured after, on a synthetic 300-camera /
  1.39M-observation problem: dense 1119 -> 675 MB, CG 553 -> 331 MB, final
  costs identical to seven digits.

### On real captures

One global BA (`spirula sfm ba <sparse dir>`, Huber 2 px, fp64), same machine.
The interesting axis is camera *sharing*: all of these have one camera group
behind every image, which is what used to force the dense path.

| capture | images / obs | before | after |
|---|---|---|---|
| 529 img | 0.52 M | dense, 0.80 s, 308 MB | dense, 0.75 s, 242 MB |
| 998 img | 3.43 M | dense, 2.83 s, 1531 MB | dense, 2.79 s, 1288 MB |
| 4194 img | 7.87 M | dense, 93.6 s, 8392 MB | **cg, 2.6 s, 3157 MB** |
| 6281 img | 12.48 M | **out of device memory** (needs 15.4 GB) | cg, 4356 MB |

Final costs are identical to seven digits on the first three. On the 4194-image
one, refining with each path and comparing the two models pose by pose gives
median rotation and translation errors of 0.0000 deg -- the CG step is the dense
step, to the precision the poses are written at.

The 6281-image capture is the one this work was aimed at: it is a 6446-image set
whose global BA could not run at all on a 16 GB card, because sharing forced the
dense path onto an `n_dim` of 37692. It now reconstructs end to end in a 6.0 GB
peak (the largest single BA being 6372 images / 12.7 M observations).

## Known limitations / next steps

- Dense path: pair-entry lists grow as the sum of `track^2/2` (above a
  400M-entry cap it falls back to the atomic per-observation Schur kernel),
  and packed indexing is 32-bit: `n(n+1)/2 < 2^31` (n ≲ 65k camera DOF).
  Neither limit applies to the CG path.
- CG iteration counts are conditioning-dependent. The coarse correction
  takes care of the long-wavelength modes of a long capture, but its clusters
  are consecutive frames, so it assumes image order follows the trajectory
  (true of video and of most photo walks); a shuffled collection gets a
  valid but weaker coarse space. At damping ~1e-7 the 4102-image dataset still
  needs 80-100 iterations per solve.
- fp32 CG inherits the documented fp32 normal-equation stall (block-Jacobi
  is nearly exact at high damping, so it still descends, but final cost is
  looser than fp64/df -- same as fp32 dense, slightly amplified).
- `cg_gather`'s per-camera `x` gathers are the remaining uncoalesced reads
  (~2x off bandwidth-bound at 1936); chunking it like `cg_scatter` is the
  obvious follow-up if the matvec ever dominates again.
- The LM loop synchronizes on one cost readback per iteration (same as the
  CUDA prototype).
- The BAL loader covers the Snavely models only. Every other camera model
  reaches the solver through `sfm/map/Bundle.h` instead, which is what the
  mapper and a model given to `spirula sfm ba` use.
