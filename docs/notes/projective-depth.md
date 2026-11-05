# Projective depth optimization

Sparse distant geometry can fit one image at the wrong depth with a large
footprint. The optimizer adds a coordinate that moves position and scale
together, allowing other observed cameras to correct depth. It uses existing
raster gradients; no additional images, decoding, or rendering are required.

## Coordinate and gradient

Let $\mathbf{x}$ be a splat's world-space mean, $\mathbf{s}=\log\boldsymbol{\sigma}$
its three log-scales, and $\mathbf{a}$ its anchor camera center. The scalar
coordinate $t$ acts by

$$
\begin{aligned}
\mathbf{x}(t) &= \mathbf{a}+e^t(\mathbf{x}-\mathbf{a}),\\
\mathbf{s}(t) &= \mathbf{s}+t\mathbf{1}.
\end{aligned}
$$

Scaling the Gaussian about a central camera preserves its projection and
projected covariance. Its rendered contribution is invariant while visibility,
occlusion order, and the active raster support remain fixed. The anchor is
chosen from a contributing observation and retained across iterations; it does
not have to see the splat in every later iteration.

For the data loss $\mathcal{L}$, write
$\mathbf{g}_x=\nabla_{\mathbf{x}}\mathcal{L}$ and
$\mathbf{g}_s=\nabla_{\mathbf{s}}\mathcal{L}$. The chain rule gives

$$
g_t=(\mathbf{x}-\mathbf{a})^\mathsf{T}\mathbf{g}_x
    +\mathbf{1}^\mathsf{T}\mathbf{g}_s.
$$

For photometric views sharing a camera center $\mathbf{c}$, projection
invariance gives Euler's identity

$$
(\mathbf{x}-\mathbf{c})^\mathsf{T}\mathbf{g}_x
+\mathbf{1}^\mathsf{T}\mathbf{g}_s=0.
$$

The equivalent expression

$$
g_t=(\mathbf{c}-\mathbf{a})^\mathsf{T}\mathbf{g}_x
$$

avoids cancellation for distant points. Batches spanning different centers,
geometry supervision, or an accumulation containing geometry supervision use
the full chain-rule expression. Parameter regularizers remain in the ordinary
optimizer and do not drive this additional coordinate.

Implementation: `projective_depth_gradient` and `projective_depth_update` in
[projective_depth.slang](../../src/shaders/projective_depth.slang).
`projective_depth_state` and `engine_prepare_projective_depth` in
[EngineOptim.cpp](../../src/engine/EngineOptim.cpp) choose the stable expression.
`build_step_config` in [TrainerCore.cpp](../../src/app/TrainerCore.cpp) identifies
losses requiring the full derivative.

## Which cameras are used?

There are three distinct camera roles:

| Role | Selection and lifetime |
|---|---|
| Anchor $\mathbf{a}$ | First accepted observation of that splat; inherited during densification and saved in full checkpoints. |
| Information observations | Every accepted splat/camera observation in the current backward pass; combined with previous observations through an EMA. |
| Angular-bound reference $\mathbf{c}_{\mathrm{ref}}$ | First entry of the currently installed camera-index array; shared by all splats in that optimizer launch. |

The Vulkan parameter `depth_cameras` is the same index array as CUDA's
`ProjectiveDepthOptim::camera_indices`. If $\mathcal{C}$ is the table of world
camera centers and $k_0$ is its first index, then

$$
\mathbf{c}_{\mathrm{ref}}=\mathcal{C}[k_0].
$$

Here $k_0$ is `depth_cameras[0]`, not a newly sampled camera. The dataset
scheduler selects the actual training images; the projective optimizer reuses
their post-split camera IDs.

| Training mode | Data gradient used for $g_t$ | Reference for the angular bound |
|---|---|---|
| One image/view | Current image's gradient | That image's camera. |
| Unsplit batch, one shared center | Sum/mean of the batch gradients; stable identity applies | First virtual view, whose center is shared by the batch. |
| Unsplit batch, different centers | Full aggregated chain-rule derivative | First camera of that batch. |
| Split batch or heterogeneous passes | Accumulated gradients from all passes, with the driver's normalization | First camera of the **final** installed sub-batch/pass. |

Heterogeneous resolutions and unequal-size warped faces can require multiple
passes even without `--split-batch`. Virtual faces of one source image have
one physical center. Their centers are canonicalized to avoid differences of
an ulp from separately evaluating rotated extrinsics. The driver tracks the
common-center and photometric-only conditions across accumulated passes.

**The reference camera is not selected per splat.** In a batch it can have zero
contribution to a particular splat that receives gradients from another camera.
It is therefore not guaranteed to see that splat. For different-center batches,
this camera affects the angular safeguard, while the gradient still includes
all contributing views. That safeguard bounds motion for its reference camera;
it does not guarantee a bound over every contributing camera in the batch.

Camera scheduling and mapping:

- `DataManager::next_train_step`,
  `DataManagerImpl::next_train_step_spec`, and
  `DataManagerImpl::build_train_schedule_locked` in
  [DataManager.cpp](../../src/data/DataManager.cpp) select scheduled training
  images.
- `engine_train_step_managed` in
  [EngineDataManager.cpp](../../src/engine/EngineDataManager.cpp) maps source
  images and virtual faces to post-split camera IDs.
- `_set_cur_cam_indices` in
  [EngineBilagrid.cpp](../../src/engine/EngineBilagrid.cpp) installs the current
  map and detects a common camera center.
- `engine_train_step`, `engine_train_step_hetero`,
  `_engine_train_step_split_one_per_camera`, and
  `_engine_train_step_split_warped` in
  [EngineTrainStep.cpp](../../src/engine/EngineTrainStep.cpp) implement batching
  and accumulation.
- `fused_optim_3dgs_geometry` in
  [Optimizer.cpp](../../src/backend/vulkan/kernels/Optimizer.cpp) and
  `launch_fpbo_vk` in
  [FusedProjectionBwdOptim.cpp](../../src/backend/vulkan/kernels/FusedProjectionBwdOptim.cpp)
  pass the camera map to Vulkan. The CUDA kernels read `depth.camera_indices[0]`.

### Contribution and occlusion

An observation is accepted when its current **screen-opacity gradient is
nonzero**. This is a contribution-dependent gate, not a physical visibility
test, an SfM track, or a minimum opacity/transmittance threshold.

Consequently:

- Exactly zero raster contribution supplies no observation.
- Small nonzero leakage through a modeled wall can supply an observation.
- A forward contribution can be missed if its opacity gradient cancels to zero.
- An anchor accepted earlier is not revalidated against the current image.

There is no guarantee of an unoccluded or substantial contribution, including
for the reference camera. The optimizer additionally requires nonzero position
or scale data gradients and positive accumulated radius before applying the
coordinate. These guards do not turn the shared reference into a per-splat
visibility selection.

Implementation: `engine_observe_projective_depth` in
[EngineOptim.cpp](../../src/engine/EngineOptim.cpp) reads existing raster
gradients. `projective_observe_kernel` in
[ProjectiveDepth.cu](../../src/kernels/optim/ProjectiveDepth.cu) and `observe` in
[projective_depth.slang](../../src/backend/vulkan/shaders/projective_depth.slang)
perform the acceptance test, anchor initialization, and information update.
`_engine_raster_proj_backward` in
[EngineLoss.cpp](../../src/engine/EngineLoss.cpp) invokes observation before
packed projection indices can be reordered.

## Geometric information

For an accepted camera center $\mathbf{c}$, define

$$
\begin{aligned}
d &= \|\mathbf{x}-\mathbf{c}\|_2,&
\mathbf{r} &= \frac{\mathbf{x}-\mathbf{c}}{d},\\
\mathbf{b} &= \mathbf{c}-\mathbf{a},&
P &= I_3-\mathbf{r}\mathbf{r}^\mathsf{T}.
\end{aligned}
$$

Let $\Sigma$ be the world-space covariance, and let the average transverse
variance be

$$
v_\perp=\frac{1}{2}\operatorname{tr}(P\Sigma).
$$

Approximate the angular footprint by an isotropic two-dimensional Gaussian
with covariance

$$
\Lambda=\frac{v_\perp}{d^2}I_2.
$$

Freeze transverse shape at the current viewing direction, retaining scale and
distance changes. The bearing derivative is $P\mathbf{b}/d$, and the derivative
of log angular variance is $-2\mathbf{r}^\mathsf{T}\mathbf{b}/d$. This local
Gaussian model's Fisher information for $t$ is

$$
J=
\frac{\|P\mathbf{b}\|_2^2}{v_\perp}
+4\left(\frac{\mathbf{r}^\mathsf{T}\mathbf{b}}{d}\right)^2.
$$

The terms measure bearing motion relative to the splat footprint and
magnification, respectively. This is a geometric conditioning metric, not a
calibrated posterior uncertainty for photometric reconstruction. It is
invariant to a common translation, rotation, or positive rescaling of the
scene before finite-precision safeguards. The implementation floors transverse
variance at $10^{-30}$. Transverse variance is evaluated through nonnegative terms in the
covariance's eigenbasis, avoiding cancellation for thin Gaussians.

The first finite positive observation initializes $\bar J$. Subsequent finite
positive observations update it by

$$
\bar J\leftarrow 0.9\bar J+0.1J.
$$

Zero-baseline observations do not dilute this estimate. Unobserved cameras do
not contribute merely because their poses are present in the camera table.

Implementation: `projective_depth_information` in
[projective_depth.slang](../../src/shaders/projective_depth.slang), with the
observer kernels linked above.

## Adam step and angular safeguard

For eligible coordinate updates, the scalar moments and observation count are

$$
\begin{aligned}
m_n &= \beta_1m_{n-1}+(1-\beta_1)g_{t,n},&
\beta_1 &= 0.9,\\
q_n &= \beta_2q_{n-1}+(1-\beta_2)g_{t,n}^2,&
\beta_2 &= 0.999,\\
\hat m_n &= \frac{m_n}{1-\beta_1^n},&
\hat q_n &= \frac{q_n}{1-\beta_2^n}.
\end{aligned}
$$

The information-conditioned proposal is

$$
\delta_0=
-\frac{\eta_t\hat m_n}
{(\sqrt{\hat q_n}+\varepsilon)(1+\bar J)},
\qquad \varepsilon=10^{-15}.
$$

Using $\mathbf{c}_{\mathrm{ref}}$ in the definitions of $d,\mathbf{r},\mathbf{b},P$,
let

$$
\mathbf{h}=\frac{P\mathbf{b}}{d},
\qquad
\delta=
\frac{\delta_0}
{\sqrt{1+\left(\|\mathbf{h}\|_2\delta_0/\alpha\right)^2}}.
$$

This gives the linearized reference-camera bound

$$
\|\mathbf{h}\delta\|_2\leq\alpha.
$$

The angular budget follows the position learning rate $\eta_x$:

$$
\alpha=
\begin{cases}
\frac{10}{3}\eta_x,
&\text{scale-agnostic position updates},\\
\frac{10}{3}\frac{\eta_x}{d},
&\text{world-unit position updates}.
\end{cases}
$$

The update applies the coordinate with $t=\delta$. The projective learning rate
defaults to $0.01$, decaying to $0.0001$. For a photometric common-center batch,
an anchor-center observation supplies no coordinate update. Splats with zero aggregate position and scale data gradients are skipped.

Implementation: `projective_depth_update` in
[projective_depth.slang](../../src/shaders/projective_depth.slang).
`fused_projection_bwd_optimizer_3dgs_kernel` in
[FusedProjectionBwdOptim_kernel.cuh](../../src/kernels/optim/FusedProjectionBwdOptim_kernel.cuh)
and `fused_optim_3dgs_geometry_kernel` in
[FusedGeometryOptim.cu](../../src/kernels/optim/FusedGeometryOptim.cu) capture the
data derivative before ordinary updates and regularization, then apply the
coordinate after position and scale updates. The derivative uses pre-update
parameters; the retraction and angular safeguard use the ordinary optimizer's
output. Vulkan uses `fpbo` in
[fpbo.slang](../../src/backend/vulkan/shaders/fpbo.slang) and
`fused_optim_3dgs_geometry` in
[optim_geometry.slang](../../src/backend/vulkan/shaders/optim_geometry.slang).

## Storage, disabled mode, and resume

The current projective state remains FP32 at both quantization levels:

| Field | Storage | Bytes per splat |
|---|---|---:|
| Anchor camera ID plus one | `states.x`, integer represented as FP32 | 4 |
| First moment $m$ | `states.y` | 4 |
| Second moment $q$ | `states.z` | 4 |
| Update count $n$ | `states.w`, integer represented as FP32 | 4 |
| Recent information $\bar J$ | `information` | 4 |
| Total | | 20 |

Allocation follows the splat **capacity**, not just the live count: one million
slots cost about 19.1 MiB. Enabled training also uploads 16 bytes per post-split
camera center.

With `--projective-depth-lr 0`, the native trainer allocates none of these
projective GPU buffers, including the camera table. Camera centers remain
available on the CPU for lazy enablement. Turning the feature off frees its
three pool slots at the next projective preparation/optimizer call; re-enabling
starts fresh moments unless a checkpoint restores them.

When resuming with a zero projective learning rate,
`TrainerSession::restore_checkpoint` passes
`restore_projective_depth=false` to `engine_load_checkpoint`, which skips
projective state restoration. The low-level loading API defaults that argument
to `true`; callers bypassing the trainer must explicitly disable restoration
or apply a zero-rate optimizer configuration afterward.

Implementation:

- `ProjectiveDepthOptim` in
  [ProjectiveDepthState.h](../../src/core/ProjectiveDepthState.h) describes the
  device views; the `EngProjective*` entries in
  [PoolSlots.h](../../src/core/PoolSlots.h) own the allocations.
- `engine_setup_data_manager` in
  [EngineDataManager.cpp](../../src/engine/EngineDataManager.cpp) retains the
  host camera table. `projective_depth_state` lazily uploads it;
  `engine_clear_projective_depth` frees all three slots in
  [EngineOptim.cpp](../../src/engine/EngineOptim.cpp).
- `TrainerSession::restore_checkpoint` in
  [TrainerCore.cpp](../../src/app/TrainerCore.cpp) and `engine_load_checkpoint`
  in [EngineCheckpoint.cpp](../../src/engine/EngineCheckpoint.cpp) control
  restoration. Full checkpoints preserve enabled state; older checkpoints
  without those slots begin with empty state.
- `engine_densify_step` in [EngineDensify.cpp](../../src/engine/EngineDensify.cpp)
  passes the state to both long-axis and MCMC parent-copy operations.

## Quantization opportunities

`--quantization-level 1` currently does **not** quantize these 20 bytes.
`build_step_config` in [TrainerCore.cpp](../../src/app/TrainerCore.cpp) enables
quantized ordinary optimizer state, while projective state is allocated
separately in `projective_depth_state`.

The easiest candidate is to reuse the existing 16-bit-per-primitive Adam codec
for the two moments. It stores

$$
u=\frac{m}{\sqrt q+\varepsilon},
\qquad
\ell=\log\left(1+\frac{\sqrt q}{\varepsilon}\right),
$$

with per-block bounds. This avoids directly converting a potentially tiny
second moment to FP16, which can underflow. Two 16-bit codes replace the two
FP32 moments; one 16-byte bound record per 256 splats gives

$$
\begin{aligned}
B_{\mathrm{current}} &=20,\\
B_{\mathrm{moment\ codec}}
&=4_{\mathrm{anchor}}+4_{\mathrm{count}}+4_{\mathrm{information}}
  +4_{\mathrm{moments}}+\frac{16}{256}\\
&=16.0625\ \text{bytes per splat}.
\end{aligned}
$$

That saves about 19.7%, or 3.76 MiB per million splats, excluding tail padding.
The normalized update and logarithmic magnitude representation make this a
plausible low-error option. Minimal end-to-end LPIPS loss is an expectation to
test, not an established result.

Before lossy moment quantization, the metadata also has a behavior-preserving
packing opportunity. Up to 65,535 post-split cameras, the anchor ID and a
saturating bias-correction age fit in one 32-bit word. For the fixed Adam
coefficients in `projective_depth_update`,

$$
\beta_2^{65535}<10^{-28}<2^{-25}.
$$

Both FP32 bias-correction denominators already round to exactly one at that
age. Saturating the age at 65,535 therefore preserves subsequent optimizer
arithmetic; it must not wrap. Larger camera tables need a wider anchor-ID
fallback.

Keeping FP32 moments and information, that metadata packing permits 16 bytes
per splat. Combining it with the moment codec above permits

$$
B_{\mathrm{combined}}
=4_{\mathrm{metadata}}+4_{\mathrm{information}}
 +4_{\mathrm{moments}}+\frac{16}{256}
=12.0625\ \text{bytes per splat},
$$

about 40% below the current state. The metadata arithmetic can be preserved
exactly; the moment codec remains lossy and needs LPIPS validation. Encoding
and block-reduction costs also need timing measurements.

Further storage choices need more care:

| Candidate | Condition or risk |
|---|---|
| 16-bit anchor IDs | Lossless up to 65,535 post-split cameras with the zero sentinel retained; larger datasets need a wider fallback. |
| 16-bit update counts | A saturating age preserves current FP32 bias corrections; migration must avoid wrapping and distinguish age from an exact diagnostic count. |
| BF16 or log-coded information | Wide dynamic range is important; rounding changes the step through $1+\bar J$. |
| Plain FP16 moments | Tiny gradients, especially distant-depth gradients, can underflow in the second moment. |

A packed/SoA layout is needed to realize smaller metadata sizes; retaining the
current `float4` row would waste the saved bytes. Densification inheritance,
checkpoint compatibility, and both backend implementations must change
together. None of this lossy compression is enabled by this change.

Existing codec references: `QuantizedAdamState` in
[Tensor.h](../../src/core/Tensor.h), `NonShQuantState` in
[NonShQuantState.h](../../src/core/NonShQuantState.h), `_NonShQ` in
[FusedProjectionBwdOptim_kernel.cuh](../../src/kernels/optim/FusedProjectionBwdOptim_kernel.cuh),
`_OptimNonShQ` in
[FusedGeometryOptim.cu](../../src/kernels/optim/FusedGeometryOptim.cu), and
`oq_state_read`/`oq_state_encode` in
[optim_quant.slang](../../src/backend/vulkan/shaders/optim_quant.slang).

## Evaluation and tests

The comparison uses 30,000 steps, one million splats, and every eighth sorted
image held out. Academic image folders are `images_4` for garden,
`images_2` for kitchen, and original images for truck and drjohnson. The
landscape capture uses precomputed quarter-resolution images and matching masks.
The loader fits intrinsics to the selected folder; no additional training
resolution divisor is applied. Builds and timed runs are sequential.

The compared preset is `3dgs`. Its depth ablation sets
`--projective-depth-lr 0`; an original-default control also precedes the
independent exploration-key correction. The separate `academic-baseline`
preset disables the coordinate along with its other reference settings.

LPIPS-A uses saved eight-bit RGB values. Ignored-pixel masks additionally produce
valid-pixel L1/PSNR, mask-normalized SSIM, and reference-filled LPIPS. For the
latter, ignored prediction pixels are replaced with reference pixels before
scoring, retaining full-image normalization and reference context. It is not an
average over masked feature locations. Masked color correction fits valid
pixels only; fully ignored views are excluded.

Implementation: `TrainerSession::eval` in
[TrainerCore.cpp](../../src/app/TrainerCore.cpp), `image_l1`, `image_psnr`,
`image_ssim`, and `color_correct_into` in
[EvalMetrics.cpp](../../src/app/EvalMetrics.cpp), and `engine_eval_forward` in
[EngineDataManager.cpp](../../src/engine/EngineDataManager.cpp). Evaluation fetches
each requested view by index independently of asynchronous decode order.

Matched comparisons use fixed camera seeds and `--cache-images cpu`.
Disk-prefetch completion can change camera order even with a fixed seed;
floating-point GPU reductions can still introduce variation.

| Test file | Coverage |
|---|---|
| [projective_depth_test.cpp](../../src/backend/tests/projective_depth_test.cpp) | Projection/footprint invariants, descent, angular bounds, information, packed observations, and backend agreement. |
| [fpbo_parity.cpp](../../src/backend/tests/fpbo_parity.cpp), [fpbo_split_parity.cpp](../../src/backend/tests/fpbo_split_parity.cpp) | Fused/separate updates and compressed ordinary optimizer state. |
| [projective_camera_batches.cpp](../../src/backend/tests/engine/projective_camera_batches.cpp) | Camera maps, coincident virtual views, accumulated passes, and geometry-supervision fallback. |
| [projective_memory.cpp](../../src/backend/tests/engine/projective_memory.cpp) | Zero disabled VRAM, lazy allocation, released pool capacity, and enabled/disabled checkpoint restoration. |
| [projective_resume.cpp](../../src/backend/tests/engine/projective_resume.cpp), [densify_parity.cpp](../../src/backend/tests/densify_parity.cpp) | Persisted and inherited state. |
| [masked_eval_metrics.cpp](../../src/backend/tests/masked_eval_metrics.cpp), [eval_views_once.cpp](../../src/backend/tests/engine/eval_views_once.cpp) | Masked scores and exact CPU/disk evaluation traversal. |

## Exploration noise

Noise keys hash the training step, splat index, and component as integers.
Box–Muller transforms uniformly spaced 24-bit values in $[0,1)$ into Gaussian
samples. Identical splats receive independent samples. Positions, scales,
rotations, and opacities determine covariance and amplitude, not the key.
Each displacement has zero mean conditional on that state.

Implementation: `densify_normal`, `mcmc_add_noise_3dgs`, and
`revised_add_noise_3dgs` in
[densify.slang](../../src/shaders/densify.slang).
[noise_moments.cpp](../../src/backend/tests/noise_moments.cpp) checks identical
splats, covariance, reproducibility, invisible splats, cross-step independence,
and backend agreement.

The keys are constructed by `mcmc_add_noise_3dgs_kernel` and
`revised_add_noise_3dgs_kernel` in
[McmcRelocation.cu](../../src/kernels/densify/McmcRelocation.cu), and
`densify_mcmc_add_noise`/`densify_revised_add_noise` in
[densify.slang](../../src/backend/vulkan/shaders/densify.slang).
`engine_densify_step` supplies the training-step seed.
