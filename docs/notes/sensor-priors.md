# Sensor priors inside the reconstruction

Written 2026-09-25. Status: **implemented** (`src/sfm/core/PriorSource.h`,
`src/sfm/ba/Priors.h`, `src/sfm/geometry/KnownRotation.h`,
`src/sfm/map/SensorPriors.h`, `src/sfm/map/ImuScale.h`). This note is the
algebra and the design; `docs/notes/imu-gps-for-sfm.md` is what the files
carry and how the *gauge* of a finished model is fixed from them, which came
first and is untouched by this.

The gauge fit uses the sensors after the model is built. This uses them while
it is built, in the four places a reconstruction goes wrong without them:

| where | what the sensor says | what it fixes |
|---|---|---|
| two-view verification | the rotation between two frames (gyro) | matches to a copy of the scene, or to equipment moving with the camera, that some other geometry explains |
| registration | the rotation between a placed neighbour and the new image | a PnP pose that turned the wrong way onto repeated structure |
| bundle adjustment | relative rotations, gravity, metric scale, GPS positions | drift of rotation, tilt and scale along a long sequence, and across a corridor two chunks hang on |
| pairing | GPS distance | loop closure the shortlist did not find |

## 1. The seam: `PriorSource`

The mapper, the verifier and the pair list never see a sensor. They see
`sfm/core/PriorSource.h`:

- `relativeRotation(i, j, R_ji, sigma)` -- the rotation taking image i's
  camera frame to image j's, with its 1-sigma in radians.
- `neighbours(img)` -- images the source relates to `img` directly, nearest first.
- `factors(posed images)` -- every factor the source can state about a set of
  posed images, in those poses' own gauge (section 3).
- `position(img, p)` -- a position in the source's metric frame (GPS in
  east-north-up), for pairing.

`TelemetryPriors` (`sfm/map/SensorPriors.h`) implements it from a video's IMU
and GPS. A LiDAR trajectory, wheel odometry or an ARKit pose stream would
implement the same four calls: `relativeRotation` from its own rotations,
`factors` with position or displacement constraints (section 3.3 covers a
relative displacement with two terms), `neighbours` from its own clock.
`RemappedPriorSource` is the same source seen through the renumbering a
bottom-up atom uses (`map/Atoms.h`), so a source is written once against
database ids.

Everything is keyed by *image*, never by pose block: a rig frame's two lenses
are two images with their own extrinsics, and the bundle layer folds a factor
on an image into the frame block it lives in (section 2.4).

## 2. Priors in bundle adjustment (`sfm/ba/Priors.h`)

### 2.1 Why on the host

A prior is a residual on one, two or three poses. There are O(frames) of
them against O(observations) reprojections, and the observation side of the
solver is the part that had to be engineered (`ba/README.md`). So the priors
are evaluated on the host in double, and only their *normal equations* reach
the device: a CSR over frames of 6x6 blocks (both orderings of a pair listed,
`block(c, r) = block(r, c)^T`) plus a gradient over the pose columns.

Each solver adds them where its own system exists:

- dense path: `prior_add_s` adds the lower-triangle blocks into the packed
  `S` after the Schur kernels, `prior_add_g` the gradient into `g`;
- CG path: `prior_add_m` adds the diagonal blocks into the block-Jacobi
  preconditioner before it is factored, `prior_matvec` applies the whole CSR
  in the matrix-free product `S p`, one thread per frame row, after
  `cg_scatter` has finished with those rows;
- host solver: the same three, on its own arrays.

The cost is evaluated on the host too: the device's cost readback is joined
by a readback of the trial poses (and member extrinsics), and the prior cost
at those parameters is added before the accept/reject decision. On an accept
the trial parameters become the host's mirror; on a reject they are
discarded with the device's. The blocks are re-assembled and re-uploaded
every LM iteration at the accepted parameters, since the damping on their
diagonal changes with the iteration (`(1 + lambda)`, as the observation
kernels damp theirs). With no priors none of this runs and neither solver
changes a byte of its behaviour; `sfm_prior_test` checks the device against
the host on the assembled `S`, `g` and the finished solve.

The coarse-correction preconditioner (`ba/README.md`, "Coarse correction")
does not see the priors. It is a preconditioner, so this costs iterations
rather than correctness; a chain of consecutive-frame rotation priors is
invariant under the rigid motion of a cluster of consecutive frames, so
within a cluster it would contribute nothing anyway.

### 2.2 Parameterization and perturbations

A frame's pose is the solver's `(a, t)`: `R = Exp(a)` the world-to-camera
rotation as an angle-axis vector, `t` the translation, `x_cam = R X + t`, and
the camera centre `c = -R^T t`. The step is applied as `a <- a - da`, so every
Jacobian is with respect to the raw angle-axis, through the left Jacobian of
SO(3):

    Exp(a + da) = Exp(J_l(a) da) Exp(a) + O(da^2),   J_l(a) = J_r(-a) = J_r(a)^T

so for a residual `r(R)` with derivative `dr/dδ` under the left perturbation
`R <- Exp(δ) R`, `dr/da = (dr/dδ) J_l(a)`. `so3LeftJacobian`,
`so3LeftJacobianInv` and their right-handed twins live in `core/Pose.h`.

A rig image composes the member on top of the frame, `R_cam = R_m R_f`,
`t_cam = R_m t_f + t_m`; a perturbation of the frame moves the camera by
`Exp(R_m δ_f)`, so `dδ_cam = R_m δ_f`. The member's own columns are left out
of the prior's Jacobian: an estimated extrinsic is good to a fraction of a
degree and the prior's coupling through it is that small, while the
reprojection terms pin the member outright. `sfm_prior_test`'s gauge case
runs with refined members and converges to rounding.

### 2.3 The three factors

Every factor's residual is divided by its sigma, and each factor carries a
Huber weight on `|r|` with `k = 1.345` sigma (the same robust loss the gauge
fit uses), so one wrong prior -- a synchronisation glitch, a wild GPS fix --
bends nothing. Gauss-Newton then adds `w J^T J` to the blocks and `w J^T r`
to the gradient.

**Relative rotation** `R_j ~ R̂ R_i`:

    E = R_j R_i^T R̂^T,   r = Log(E) / σ
    Exp(δ_j) E           = Exp(e + J_l^{-1}(e) δ_j)         ->  dr/dδ_j =  J_l^{-1}(e) / σ
    R_j (Exp(δ_i) R_i)^T R̂^T = E Exp(-R̂ δ_i) = Exp(e - J_r^{-1}(e) R̂ δ_i)  ->  dr/dδ_i = -J_r^{-1}(e) R̂ / σ

**Up** `R_i ĝ ~ û_i` (ĝ the world up, û_i the measured up in the camera):

    r = (R_i ĝ - û_i) / σ,   Exp(δ) g = g + δ x g = g - [g]_x δ   ->  dr/dδ = -[R_i ĝ]_x / σ

Three residuals with a rank-2 Jacobian: the radial component is second
order and carries no gradient, which is harmless.

**Centre combination** `Σ_k A_k c_{i_k} ~ b`, per axis `1/σ_m`, a zero
sigma dropping the axis:

    c = -R^T a,  a = t_f + R_m^T t_m  (a = t without a rig)
    dc/dt_f = -R_f^T,   dc/dδ_f = -R_f^T [a]_x        (from -R_f^T Exp(-δ) a = c - R_f^T [a]_x δ)

One term with `A = I` is an absolute position; two terms are a displacement;
three terms are the inertial triple of section 3.3. The 3x3 `A_k` is what
lets a GPS factor carry the similarity from the model into metres, and an
inertial triple its interval durations.

`sfm_prior_test` differentiates all three numerically, on plain and rigged
problems.

### 2.4 From image ids to pose blocks

The mapper states factors on reconstruction image ids; `buildBundle`
(`map/Bundle.h`) remaps them to BA indices, drops any naming an image the
problem lacks, and keeps one rotation factor per pair of frames -- a rig's
lenses each carry the chain and both name the same two pose blocks. The
joint solve over several models (`runJointBA`) offsets each model's factors
with its images; the up axis is per model, so the stacked problem keeps the
first model's up factors and drops the others' (their gauges differ).

## 3. What the IMU and GPS state (`sfm/map/SensorPriors.h`)

### 3.1 Calibration, in two stages

The one calibration this needs is the IMU-to-lens rotation `X` per (capture,
camera group), and it is not written in any file (`imu-gps-for-sfm.md`,
section 2). It is fitted from the reconstruction itself:

1. **Before mapping, from verified pairs.** A sample of time-adjacent pairs
   (same group, 0.02-3 s apart, spread over the capture, up to 900) is
   matched and their relative rotations recovered from the essential matrix
   on bearings. Each is one hand-eye constraint `A X = X B` with `A` the
   pair's rotation and `B` the gyro's over the same interval;
   `calibrateImuExtrinsicFrom` (`map/ImuExtrinsic.h`) solves the stacked
   linear system in the nine entries of `X` as the gauge fit does, with the
   gyro sign hypotheses and the clock-offset search
   (`estimateTimeOffsetFrom`) running first on the rotation angles, which
   are invariant to `X`. What two-view rotations cannot settle is the sign of
   `X` (`X` and `-X` satisfy every constraint) and, on a capture that only
   ever turns about one axis, the rotation about that axis. The first does
   not matter for relative rotations (`X B^T X^T` is even in `X`); the
   second marks the group `degenerate` and no rotation prior is taken from it.
2. **During mapping, with gravity.** `factors()` refits each group once it
   has 20 posed frames and again each time that count has doubled, now with
   the gravity-pair constraints the poses make available. That settles the
   sign (the votes must agree with the cameras' mean up, then with the
   consensus of every group, exactly as the gauge fit does) and the
   one-axis degeneracy.

Both stages share every function with the gauge fit
(`map/ImuExtrinsic.h`, `map/ImuScale.h`); the gauge fit was refactored onto
those files rather than duplicated.

### 3.2 Rotation and up

`relativeRotation(i, j)` for two timed images of one capture within
`--sensor-max-dt` seconds: `B = R_i(t_i) <- i(t_j)` from the gyro (or the
fused attitude on a camera that writes no gyro), and

    R_ji = X_j B^T X_i^T

with sigma the calibration's own rotation residual plus a bias drift of
0.03 deg/s times the gap, floored at 0.2 deg. `factors()` emits one such
factor between consecutive frames of each lens's chain.

The up factor is the accelerometer's specific force averaged over half a
second around the frame, de-rotated to the frame's instant, mapped into the
camera by `X`; the world up it is measured against is the robust consensus
of `R_i^T û_i` over every frame (`consensusUp`), refitted per solve. Sigma is
the calibration's gravity residual, floored at 1 deg.

### 3.3 Metric scale from the accelerometer

Over two consecutive intervals `j -> k -> l` of durations `d1`, `d2` with
pre-integrated velocity and position increments (`core/Preintegration.h`),
eliminating the velocity at `j` between the two position equations gives
the velocity-free constraint of Mur-Artal and Tardós (2017, sec. IV):

    s [ (c_l - c_k) d1 - (c_k - c_j) d2 ] = Q + Gs g,
    Q  = R_j (Δv_1 d1 d2 - Δp_1 d2) + R_k Δp_2 d1,   Gs = ½ d1 d2 (d1 + d2)

with `R_j = R_wc(j) X` the world-from-IMU rotation, `g` gravity, and `s`
metres per model unit. `solveImuScale` (`map/ImuScale.h`) fits `1/s`, both
biases and the gravity check linearly, with the accelerometer's own noise
taken back out of the regressor (corrected least squares) and Huber
weights; the derivation and the numbers behind it are in
`imu-gps-for-sfm.md`, section 5.6.

The scale then goes into the solve as one centre factor per triple, in the
model's own units, with `s`, the biases and `g = -9.81 ĝ` frozen at the fit:

    A_j = d2 I,   A_k = -(d1 + d2) I,   A_l = d1 I,   b = (Q + Gs g) / s

and sigma the fit's robust residual scale. `Q` depends on the rotations,
which the factor treats as constant: the rotation and up factors hold them,
and refitting per solve keeps the frozen quantities current. The factor is
taken only when the fit is worth it -- at least ten triples, a relative
sigma under 20 %, the refitted gravity within 20 deg of up -- so a camera
that only pans, or a lens whose calibration is degenerate, adds no scale.

This is the alternating scheme: gauge quantities (up, scale, biases, the
metric frame) are estimated on the host from the current poses before each
solve and frozen inside the factors, so the bundle adjustment never carries
a global parameter block and the priors stay in the model's own gauge.

### 3.4 GPS

Each timed frame's position from the log (interpolated between distinct
fixes) in a local east-north-up frame; a similarity from the model onto
them by `fitMetricGauge` (`map/MetricGauge.h`), horizontal when an up axis
exists (D75), full otherwise; then one centre factor per inlier frame with
`A = s R`, `b = p - t`, sigma twice the fit's RMS with a 3 m floor on the
level axes and the vertical dropped (or three times looser on a full fit).
The inflation is D74's finding that a receiver's error is correlated and
the residuals under-state it.

`position()` serves the pair list: `gpsProximityPairs`
(`feature/GpsPairs.h`) adds every positioned image's twenty nearest others
within `--sensor-pair-radius` metres.

## 4. Two-view verification with the rotation known (`sfm/geometry/KnownRotation.h`)

With `R` given, `b2^T [t]_x R b1 = 0` is linear in `t`: every correspondence
gives a normal `m_k = (R b1_k) x b2_k` that `t` must be orthogonal to, two
of them fix it (`t = m_1 x m_2`), and the least-squares refit is the
smallest-eigenvalue direction of `Σ m m^T` (Kneip, Chli and Siegwart, BMVC
2011). The RANSAC samples two points instead of seven, scores with the
Sampson error on the sphere, and picks the sign of `t` by cheirality. A
correspondence with `|m_k|` under the inlier angle is one the rotation alone
explains; when those are most of the inliers the pair is a panorama
(`max_H_inlier_ratio`, as the free path decides it).

What this rejects, and the free estimate cannot: features on equipment that
moves with the camera project to the same pixels in every frame, which is
consistent with `R = I` and any `t` -- a skew-symmetric fundamental matrix
-- and a free RANSAC on a pair where they are a large share of the matches
finds exactly that geometry and throws the scene away. Under the gyro's
`R`, `b2 = b1` cannot satisfy `b2^T [t]_x R b1 = 0` for any `t`, so they are
outliers whatever their share. A copy of the scene elsewhere fails the same
way when the camera did not turn the way a match to the copy requires.

The given rotation is a prior, not a fact: the pair stage's calibration
agrees with the images to 1-2 degrees on a real capture, hundreds of times
the inlier radius, and a rotation taken as exact lost half its pairs on the
first run. So the two-point RANSAC runs under a gate widened by 2.5 sigma of
the prior, `(R, t)` are then refined together on those inliers
(`refineNearRotation`), and the inliers are re-judged at the strict radius
with the refined geometry. The refinement is an IRLS Levenberg-Marquardt
over five parameters -- `d` with `R = Exp(d) R_prior`, and `t` moved in its
own tangent plane so the unit constraint leaves no null direction in the
normal matrix -- minimising `sum log(1 + (e_k / r)^2) + |d|^2 / sigma^2`
with `e_k` the Sampson error and `r` the strict radius: a Cauchy loss,
because the widened gate admits residuals thirty times the radius whose
Huber pull (constant per point) still steered the solution when they were
coherent, as equipment is; under Cauchy their influence falls as `r / e`.
It runs twice, from the loose RANSAC's own `t` and from the free estimate's
pose, and the result with more strict inliers wins, which is the free
estimate's own criterion. On the dome-gate capture this keeps the strict
set within 6% of the free one's over the pairs it accepts (the borderline
matches a maximum-inlier fit includes and a robust fit does not).

`verifyPairs` runs both estimates on a pair the source covers and keeps the
refined inlier set when it explains at least `prior_agree` (0.7) of the free
one's inliers -- so a wrong prior (a clock the offset search
missed, a lens the calibration got wrong) loses to the images and is
counted as a disagreement, which the stage reports.

The same two-point form serves registration (`ransacPnPKnownRotation`, and
`ransacRigPnPKnownRotation` for a whole frame): `b x (R X + t) = 0` is linear
in `t`. The mapper predicts an image's rotation from its tightest placed
neighbour, and a PnP pose more than `tol = max(2 deg, 3 sigma)` off it is
re-solved the same way -- the translation under a radius widened by `tol`,
the pose refined freely from there and kept when it stays within `tol`,
the inliers counted at the image's own radius -- and refused when that
finds fewer than the registration's own inlier floor. The audit's alternative-pose test and
the growth passes' rejection use the same prediction as a voucher: a pose
that turns as the gyro says is not unseated by one that does not.

## 5. What it costs

Per bundle adjustment: `factors()` is O(frames) apart from the
pre-integrations, which are cached per pair; the assembly is
O(factors x 36); the device takes one extra upload (36 doubles per block
pair) and one readback (6 doubles per frame) per LM iteration. On a
4000-frame capture that is ~2 MB each way. Verification pays one more
RANSAC per covered pair, with a two-point sample and typically far fewer
trials than the seven-point one. With no telemetry none of it runs.

## 6. Options

`--sensor-verify`, `--sensor-map`, `--sensor-pairs` (all on, inert without
`--telemetry` or a manifest's `captures:`), `--sensor-pair-radius` (20 m),
`--sensor-max-dt` (3 s). `SS_SFM_PRIOR_DUMP=1` prints every registration the
gyro overruled. The run reports the calibration per group, the verification
tally and, at the end of mapping, how many registrations were re-solved or
refused and how many factors the last solve held.
