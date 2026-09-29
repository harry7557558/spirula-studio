# IMU and GPS for SfM: what the cameras record, and how to use it

Written 2026-09-08. Status: **section 5 is implemented** (same day):
`src/sfm/core/Telemetry.h` reads what a video file carries and checks that
the readings look like a working sensor; `sfm_telemetry_test FILE` prints the
result; `src/sfm/map/SensorGauge.h` consumes it through `--telemetry` and the
manifest's `captures:` list, and section 5.6 records what it does and what it
measured. This note records what was found in the captures on hand and lays
out how the readings give a reconstruction its metric scale and its
orientation, and, further off, make the reconstruction itself more robust.

The reader has a background in computer vision but not necessarily in inertial
navigation, so section 3 spells out the sensor facts the later sections rely
on. Skip it if you know what a gyro bias is.

## 1. Summary

- **Every Insta360 X5 capture carries a 1 kHz gyro+accelerometer log** with a
  clean gravity norm (9.80-9.89 m/s^2 over 43 files). Its GPS log is the
  phone's position pushed over Bluetooth: 10 Hz samples, updated about once a
  second outdoors, and simply the last known fix repeated for an entire
  indoor capture. Of the 44 files with a GPS log, 12 never move, 10 more
  hold one position for minutes at a time or barely spread, and 22 pass the
  checks: all five GoPro files and 17 X5 walks of up to 2.2 km. Two files
  that were trimmed or re-muxed lost the log entirely, and eight were
  recorded with no phone connected.
- **GoPro MAX `.360` files carry the full GPMF set**: 800 Hz gyro, 200 Hz
  accelerometer, an 18 Hz GPS with a fix flag and DOP, a per-frame gravity
  vector and a per-frame fused attitude. The GPS is real and moves; one of
  five files has a single wild fix (a 50 km jump) that the DOP does not flag
  and speed-gating removes.
- **DJI Osmo 360 `.OSV` files carry a 1 kHz fused attitude quaternion and one
  accelerometer reading per frame**, plus the lens intrinsics. No raw gyro is
  written and there is no GPS.
- The three makers put the sensor axes in three different frames, and GoPro
  puts two of its own streams in different frames from a third. None of them
  documents the rotation from the IMU to the lens. That rotation is the one
  calibration this work needs, and section 5.1 says how to get it from the
  reconstruction itself.
- **What to build first** (section 5): gravity alignment, because it needs no
  time synchronisation and fixes the one thing `--orient` guesses; then
  metric scale from the accelerometer, which needs synchronisation and a
  moving camera; then GPS scale through the existing `--metric-gps` path,
  which needs nothing new except a way to hand it a position per frame.

## 2. What is in the files

Numbers from `sfm_telemetry_test` over every `.insv`, `.OSV` and `.360` under
the local data directory (57 files). The reader is compared against
[telemetry-parser](https://github.com/AdrianEddy/telemetry-parser) (the
Rust library behind Gyroflow) on one file of each kind: sample counts and
timestamps agree exactly; the Insta360 values agree once its rotation into
the lens frame is undone (it applies the `offset_v3` roll of 90.7 degrees to
the IMU, this reader does not); exiftool's Insta360 accelerometer values are
2.4% larger because it scales by 1000 counts per g where the file's own
`gyro_cfg_info` says 32 g over 15 bits, i.e. 1024.

### 2.1 Insta360 X5 (`.insv`)

| stream | rate | span | notes |
|---|---|---|---|
| gyro | 1000 Hz | starts ~1 s before the first frame, ends with the video | raw int16 at 2000 deg/s full scale |
| accel | 1000 Hz | same record | raw int16 at 32 g full scale; norm median 9.80-9.89 |
| GPS | 10 Hz log | wall clock | position and speed update at ~1 Hz outdoors; stale indoors |
| lens | once | | `offset_v3`: per-lens fx, fy, cx, cy, yaw, pitch, roll, k1..k3, p1, p2 |
| rolling shutter | once | | 11.9 ms readout at 4K |

The trailer sits after the MP4's last box and starts with an offsets table;
ffprobe and the demuxer never see it, which is why an edited file loses it.
The camera has no GPS receiver of its own: the log is what the phone app or
a GPS remote pushed, which explains both the 1 Hz update and the 100 percent
stale indoor logs (one file holds a single fix for 26 minutes, another two
fixes 430 m apart). The stale fix's timestamp is also stale, so on those
files the GPS clock does not even overlap the video.

**Frame:** the gyro and accelerometer are in the IMU's own frame. On the
files here gravity at rest sits along -Z with a large +X component when the
camera stands upright, which is not the lens frame. The `offset_v3` string
carries a yaw/pitch/roll per lens that telemetry-parser applies to the IMU
as the IMU-to-lens rotation; it is recorded but not applied, pending the
calibration in section 5.1.

**Time:** samples are stamped in microseconds on the camera clock, and the
metadata carries `first_frame_timestamp` on the same clock, so IMU time
relative to the first frame is a subtraction. The clip's absolute start is
the MP4 movie header's creation time, which on the X5 matches the first GPS
fix to the second; the metadata's own `creation_time` is a decimal
YYYYMMDDHHMMSS in local time and is not usable as an epoch.

### 2.2 GoPro MAX (`.360`) and any GoPro `.mp4`

| stream | rate | frame | notes |
|---|---|---|---|
| GYRO | 800 Hz | ORIN | rad/s; the spec's HERO frame after the `ORIN` permutation |
| ACCL | 200 Hz | ORIN | m/s^2; norm median 9.80-9.91 after a 0.5 s average |
| GRAV | 30 Hz | ORIN with Y and Z swapped | unit vector, per frame |
| CORI | 30 Hz | acts on (X, Z, Y) | per-frame attitude; `conj(q)` maps sensor to world |
| GPS5 | 18 Hz | | lat, lon, alt, 2D and 3D speed; fix and DOP once per second |
| MAGN | 25 Hz | raw columns | no ORIN written |

Each 1 s payload carries its own microsecond timestamp per stream (`STMP`);
the payload's place on the video clock comes from the sample table and
`STMP` spaces the readings inside it, which is what gpmf-parser does. The
different streams' first `STMP` values differ by up to 30 ms, which is a
real offset between sensors that this anchoring discards; section 5.2 is
where it would be recovered.

**The frame finding.** GoPro's spec says `ORIN`/`MTRX` take the raw columns
into the camera frame, and the reader applies it to ACCL and GYRO. GRAV and
CORI do not live in that frame. Searching every signed axis permutation for
the one that makes GRAV agree with the averaged accelerometer gives the same
answer on three files: GRAV = (X, Z, Y) of the ORIN frame, at 3-4 degrees
median disagreement (60-97 degrees before). CORI is the same: rotating the
(X, Z, Y)-ordered accelerometer by the conjugate of CORI gives a world vector
that holds to 2-8 degrees over a whole file, and no other combination does.
A swap of two axes is a reflection, so one of the two conventions is
left-handed with respect to the image; which one is not decidable from the
sensors alone and falls to the calibration in 5.1. The reader swaps GRAV into
the ORIN frame and marks the CORI order in `Telemetry::orientation_axes`.

A fourth file agreed on both counts with no swap, which looked like a
counter-example until its gravity vector turned out to lie on the Y = Z
diagonal for the whole ride: the mount made the swap invisible.

**GPS.** Real receiver, 18 Hz, DOP typically 1.5-2.5. One file
has a single fix 50 km away with the fix flag set and DOP under 10; the
reader's implied-speed gate (over 50 m/s from where the previous position was
first reported) drops it, leaving a 1.16 km path where the raw numbers said
8866 km. Another reports an altitude below -500 m for a while. Neither stops
the file from being usable; both say the DOP is not enough of a gate on its
own.

### 2.3 DJI Osmo 360 (`.OSV`)

| stream | rate | notes |
|---|---|---|
| attitude | 1000 Hz | `IMU_attitude_after_fusion`, ~33 quaternions per frame, each frame also carrying the previous frame's |
| accel | 30 Hz | one reading per frame, in g (the schema says "degree"); norm median 9.80-9.85 |
| lens | once | fx = 1061.6 px at 3840, four fisheye coefficients, 24.0 ms readout |

The `djmd` track is protobuf (`dvtm_oq101.proto`, in the telemetry-parser
tree); the second `djmd` track carries the same clip header with empty
sensor fields, and the two `dbgi` tracks are an undocumented per-frame
register dump (protobuf, `dbginfo_oq101.proto`, 2 Mbit/s) that was not
pursued. There is no raw gyro anywhere in the file; the attitude is the
camera's own fusion output. Rotating the per-frame accelerometer by the
quaternion gives a world vector steady to 2.8 degrees over a 146 s handheld
clip, so the quaternion maps sensor to world and the accelerometer shares its
frame. No GPS.

### 2.4 What the reader does with all this

`telemetry_read` picks the carrier by content, never by extension: the
Insta360 magic at the end of the file, else an MP4 whose sample table has a
`gpmd`, `camm` or `djmd` entry. CAMM (Google's camera-motion track, what a
phone app such as Insta360's own or Google's Street View writes) is
implemented from the spec and covered by the synthetic test, but no file here
uses it. Every reading comes out in SI units with a time in seconds from the
first video frame.

`telemetry_check` decides whether the readings look like a sensor rather
than whether they are precise:

- coverage of the video, sample-rate regularity, monotonic time, finite values
- accelerometer norm after a 0.5 s average, against 9.81: a reading in g is
  flagged, and so is a scale that is off (this is how the 1024-vs-1000
  question above was settled)
- gyro norm median under 10 rad/s, which catches deg/s mislabelled as rad/s
- quaternion norm and step size
- cross-stream gravity: GRAV against the accelerometer, and the accelerometer
  rotated by the attitude in both senses, reporting which sense holds
- GPS: fix fraction, distinct positions, the longest time one position was
  held (over 30 s or a fifth of the clip is stale), DOP and implied-speed
  outliers, spread and path length of what is left

`imu_usable` and `gps_usable` are the summary; the warnings say why not.

### 2.5 Looking at it

`viewer/telemetry.html` is a browser page over the same reader (compiled to
its own WASM module), for when the text report is not enough: drop files or
folders and get the GPS track over a street-level basemap in a 3D viewport
with real altitude, the attitude drawn along the track, and every stream as a
plot on a shared, scrubbable time axis. It reads a file in place through the
File API rather than buffering it, so a 4 GB capture is fine, and a
screenshot-safe toggle hides everything that says where the capture was taken.
`viewer/README.md` has the details.

## 3. Background: what the sensors measure

**A gyroscope** measures angular velocity of the body it is bolted to, in its
own axes, in rad/s. Integrating it gives orientation change, and the
integration drifts: every MEMS gyro has a **bias** (a constant offset of
order 0.01-0.1 deg/s that changes slowly with temperature) that integrates
into a linearly growing heading error, plus white noise that integrates into
a random walk. Over a second the drift is negligible; over a minute it is a
few degrees; over a capture it is not usable on its own. The camera makers
fuse the gyro with the accelerometer to hold roll and pitch (GoPro's CORI,
DJI's attitude); yaw has nothing to correct it and drifts.

**An accelerometer** measures **specific force**: acceleration minus gravity,
in body axes. At rest it reads +g pointing up (the reaction to gravity), so
its low-frequency content is the direction of "down" in the sensor's frame,
which is what sections 4 and 5 use. Its high-frequency content is real motion
plus vibration; on the MAX bike ride the raw norm had a median of 12.6 m/s^2
and a 0.5 s average brought it to 9.9. The accelerometer also has a bias (of
order 0.01-0.05 m/s^2) and a scale error (of order 0.1-1 percent); both
matter for double integration and neither matters for finding "down".

**Double integration** of the accelerometer gives position and is the thing
people expect to work and it does not: a 0.02 m/s^2 bias becomes 0.01 m after
1 s, 1 m after 10 s and 36 m after 60 s. Every usable inertial pipeline
therefore either integrates only over short windows between images (this is
**pre-integration**, section 5.2) or fuses with something that observes
position (GPS, or the images themselves).

**Scale from an IMU** comes from the fact that the accelerometer is metric:
if the cameras moved 1 m in the reconstruction's units and the integrated
accelerometer says 0.5 m, the reconstruction's unit is 0.5 m. This needs the
camera to actually accelerate: a camera moving at constant velocity, or not
at all, gives the accelerometer nothing to say beyond gravity. Handheld and
walking captures accelerate constantly (a walking step is a 1-2 m/s^2 event
at 2 Hz) and are the good case; a smooth dolly or a gimbal on a vehicle is
the bad one.

**GPS** gives position on the Earth to a few metres (consumer receivers, open
sky; the MAX reports a DOP around 2, which with a typical 3-5 m range error
means 6-10 m horizontal), updated at 1-18 Hz. Altitude is the worst
component, two to three times the horizontal error, which is why the existing
`--metric-gps horizontal` exists (D75). GPS error is **correlated** over
seconds to minutes (the same satellites, the same atmosphere), so consecutive
fixes agree with each other far better than with the truth; a fit's residual
under-states its error, which `MetricGauge.h` already records at 3.9-4.5x.

**Time synchronisation.** The IMU and the video run on the same clock inside
the camera, but the reading that corresponds to a frame is not at that
frame's timestamp: the exposure is spread over the readout (12-24 ms here),
the sensor pipeline has latency, and each stream has its own offset (30 ms
between GoPro streams in section 2.2). For gravity alignment a 30 ms error is
nothing (the camera turns less than a degree in that time). For scale from
pre-integration it matters: 30 ms at a walking pace's 1.5 m/s^2 is 2 mm of
position error per step, small, but the same offset applied to a rotating
camera moves where the accelerometer's gravity component lands, and that is
the limiting term. The standard fix is to estimate the offset as one more
parameter (Qin and Shen, "Online temporal calibration for monocular VIO",
2018), which section 5.2 includes.

**Frames and extrinsics.** Three frames matter: the IMU's, the lens's, and
the world's. IMU-to-lens is a fixed rotation (and a translation of a few
centimetres that only matters for pre-integration at high angular rates); it
is what none of the makers document in a usable form and what section 5.1
calibrates. World is whatever gravity and, if GPS is present, north define.
A right-handed/left-handed mismatch, which section 2.2 found between GoPro's
own streams, cannot be fixed by any rotation and has to be resolved once per
camera model.

## 4. What the reconstruction needs, and what each sensor can give

`spirula sfm` today writes a model in an arbitrary gauge and then fixes it
one of two ways: `--orient` (default) makes the mean camera up axis +Z and
the scene unit-sized, a statistical guess about how the camera was held; or
`--metric-gps` / `--metric-positions` fit a Sim(3) from camera centres to
reference positions (`map/MetricGauge.h`, D74/D75). What is missing:

| want | source | needs sync | needs a moving camera | what it replaces |
|---|---|---|---|---|
| **up** (2 DOF: roll and pitch of the world) | accelerometer average, or GRAV/attitude | no | no | the `--orient` guess, which fails on a capture tilted on purpose or carried upside down |
| **heading** (1 DOF: yaw) | GPS track, magnetometer, CORI on cameras that init to north | loose | GPS: yes | nothing today |
| **scale** (1 DOF) | GPS over a path, or accelerometer pre-integration | GPS: loose; IMU: tight | yes | `--metric-positions` from an outside source |
| **place** (3 DOF) | GPS | loose | no | `--metric-gps` |

Everything in the table is a **gauge** fix: it changes how the finished model
is written, not how it is built, and it plugs into the point where
`Orient.h` and `MetricGauge.h` already act. That is the low-risk half. The
high-risk half, using the sensors to change what the mapper does, is section
6 and is out of the present scope.

## 5. Metric scale and orientation: the approaches, in build order

### 5.1 Gravity alignment (up), and the IMU-to-lens calibration it needs

**What it does.** Replaces `--orient`'s "mean camera up" with "measured
down". For each registered frame, take the gravity direction in the IMU
frame (the accelerometer averaged over the half-second around the frame; or
GRAV; or the attitude's third column), rotate it into the lens frame with
the calibrated extrinsic `R_ci`, and into the world with the frame's pose
`R_wc`. Every frame now votes for the world's down direction; the votes
should agree to a degree or two, and their mean is the axis `Orient.h`
rotates to -Z. Frames whose vote is far from the consensus are frames whose
pose is wrong, which is a free registration audit.

**What it costs.** No time synchronisation to speak of (a 30 ms error is
under a degree of camera rotation), no requirement on motion, one extrinsic
rotation per camera model. It works on a single frame in principle and on
the whole model in practice.

**The calibration.** `R_ci` is unknown for every camera here. It is
observable from any reconstruction of a capture that was not held level:
with `g_i(k)` the gravity vector in the IMU frame at frame k and `R_wc(k)`
the frame's pose, `R_wc(k) R_ci g_i(k)` must be one constant vector for all
k. That is an orthogonal Procrustes problem in `R_ci` once the constant is
eliminated (pairs of frames: `R_wc(j) R_ci g_i(j) = R_wc(k) R_ci g_i(k)`), and
a handful of frames with different roll and pitch solve it; a capture that
also revisits the same orientations lets a left-handed hypothesis be told
from a right-handed one because only one of them fits with a proper rotation.
Do it once per model on a capture chosen for it (tilt the camera every way
for a minute), store the result keyed on the camera model string the file
carries ("Insta360 X5", "GoPro Max", "Osmo 360"), and ship it. The Insta360
`offset_v3` yaw/pitch/roll and telemetry-parser's per-model axis tables are
starting guesses to be verified, not answers.

**Where it lands.** `map/Orient.h`: `uprightTransform` takes an optional
per-image down vector; the manifest (`sfm/core/Manifest.h` already reserves
the slot) carries one per image, computed at extraction time from the
telemetry and the frame's source index (the frame files are named by that
index, `%05d`, so the time is index over fps). The extraction side is
`app/FrameExtract.cpp` and its ffmpeg fallback, both of which know the
source index of every frame they write.

**Expected accuracy.** The accelerometer's low-pass direction on the X5
holds to about a degree over a step; averaged over hundreds of frames the
world's up should be good to well under a degree, against the several
degrees a mean-camera-up guess is off on any capture that looks up or down a
lot.

### 5.2 Metric scale from the accelerometer (IMU pre-integration)

**What it does.** Between consecutive registered frames j and k, integrate
the gyro to get the rotation `ΔR`, then integrate the accelerometer, rotated
by the running gyro orientation, twice to get `Δv` and `Δp` in the frame of
j, with gravity subtracted. This is the pre-integration of Forster et al.
("On-manifold preintegration for real-time visual-inertial odometry", 2017),
and its point is that `Δp` depends only on the IMU readings between the two
frames plus the biases, not on the absolute pose, so it can be computed once
and reused while the poses and the scale are being solved.

With the reconstruction's poses `(R_wc(k), p_wc(k))` in unknown units and
the lens-IMU extrinsics, the relation for every pair is

    s (p(k) - p(j)) = R(j) Δp_jk + v(j) Δt + 0.5 g Δt^2

with `s` the scale, `v(j)` the velocity at frame j and `g` gravity in the
world frame, all unknown, all linear. Stacking every consecutive pair gives a
linear least squares problem in `s`, the velocities and `g` (Mur-Artal and
Tardós, "Visual-inertial monocular SLAM with map reuse", 2017, section IV),
which also yields the gyro bias from the rotation residuals and, with a second
pass, the accelerometer bias. Gravity comes out of it too, so 5.1 is
subsumed, but 5.1 is worth having on its own because it works where this
does not.

**What it costs.**
- Time synchronisation to a few milliseconds, or the offset as an unknown:
  add `t_d` and linearise (Qin and Shen 2018). The Insta360 and DJI files
  stamp the IMU on the video clock; GoPro's 30 ms inter-stream offset is the
  kind of thing this absorbs.
- Rolling shutter: a frame's pose is the pose at mid-readout, which the
  readout time in the file (12-24 ms) places.
- Motion: the camera must accelerate. Walking captures are fine; a smooth
  pan from a fixed spot has no baseline and no scale to recover and the
  linear system says so through its conditioning, which must be reported
  the way `MetricGauge.h` reports collinearity, not silently accepted.
- A raw gyro. The DJI writes none, and the fused attitude stands in for the
  rotation part (it is the camera's own gyro integration with gravity
  correction), which leaves no gyro bias to estimate. `SensorTimeline`
  pre-integrates from it directly at the accelerometer's own instants.
- A sample rate the position integral can live with. A per-frame
  accelerometer aliases the vibration a 1 kHz stream resolves and integrates
  away, and that noise lands in the *regressor*: 0.06 (m/s^2)/sqrt(Hz) on
  the DJI, which over 1 s pairs attenuates the fitted 1/s by 10 per cent.
  The noise density is measured per capture from the second differences of
  the accelerometer, and its own contribution to the normal matrix is taken
  back out (corrected least squares) in both the closed form and the joint
  solve. Decimating an X5 log to 30 Hz is the check: the attenuation is 0.83
  per cent measured against the full-rate answer, 0.86 predicted.
- Frame rate: the pair spacing in a dataset is 2-10 fps after extraction,
  and pre-integration over 0.1-0.5 s is well inside where biases are
  harmless. Extraction keeps the source index so the exact interval is known.

**Expected accuracy.** VIO literature reports 1-3 percent scale error from
initialisation windows of a few seconds; over a whole capture with hundreds
of pairs it should be under one percent when the motion is good, and the
residuals say when it is not. GPS over a 100 m walk gives 0.2 percent (the
number in the README) but only outdoors and only with a real receiver, so
the two are complementary: IMU scale for every indoor X5 capture, GPS scale
for the outdoor ones as a check.

**Where it lands.** A new `map/InertialGauge.h` next to `MetricGauge.h`,
producing the same `Sim3` and the same "refused with a reason" contract; the
pre-integration itself is a self-contained header on `Telemetry.h`'s types.
Nothing in the mapper changes.

### 5.3 Metric scale and place from GPS, through what exists

`--metric-gps` already reads per-image EXIF GPS and fits a Sim(3) with
LO-RANSAC. Video frames have no EXIF, so the change is a way to hand the
fit a position per image from the telemetry: interpolate the GPS log at each
frame's time and write it into the manifest, and let `MetricGauge.h` do what
it does. Two things learned here should go in with it:

- The reader's `gps_usable` and its outlier gate should decide whether to
  offer a fix at all; the stale-fix X5 logs would otherwise put every frame
  at one point, which the existing spread gate refuses, but the two-point
  lobby file would pass the spread gate with a nonsense scale.
- The 1 Hz update behind the 10 Hz log means a position is up to a second
  old; at walking pace that is 1.5 m, on the order of the receiver's own
  error, and interpolating between distinct positions (by their first
  appearance) rather than between samples halves it.

`horizontal` should stay the default for the same reason as before
(D75); with 5.1 giving the vertical from gravity, the GPS never needs to.

### 5.4 Heading (which way is north)

Gravity fixes two of the three orientation degrees of freedom; the last, yaw,
needs a direction reference. Options, none urgent:

- **GPS track** over a walk: the direction of travel, which the GPS speed
  and track fields (both recorded by the X5 log and the MAX) or the fitted
  Sim(3) from 5.3 give. Free once 5.3 exists.
- **Magnetometer**: GoPro writes MAGN at 25 Hz. A magnetometer inside a
  camera next to a motor and a battery reads the camera as much as the
  Earth, and needs a hard/soft-iron calibration; a project that does not
  need true north should not start here.
- **CORI**: GoPro's attitude has a yaw origin that is either the start of the
  clip or north depending on the model and firmware; not verified here.

For splatting, heading only matters when a model has to sit in a map; the
GPS route is the one to take then.

### 5.5 Which to build

1. **5.1** first: smallest, no sync, no motion requirement, fixes a real
   weakness, and the calibration step it needs is the same one 5.2 needs.
2. **5.3** second, because it is plumbing on an existing path, and because
   the outdoor X5 walks and every MAX file get a metric scale from it that
   5.2 can then be validated against.
3. **5.2** third, for the indoor captures, which are most of them.

### 5.6 What was built, and what it measured

The three of 5.1-5.3 shipped together as one estimator rather than three
passes, because they share the calibration and the same unknowns:

- `core/SensorTimeline.h` answers time-indexed queries over one file's
  telemetry: the IMU rotation between two instants (from the gyro, or from the
  fused attitude where there is no gyro), the up direction at an instant (the
  specific force averaged over half a second after each sample is de-rotated
  into the frame at that instant), a pre-integration over an interval
  (`core/Preintegration.h`, Forster et al. with first-order bias Jacobians),
  and the GPS position at an instant, interpolated between the first
  appearances of distinct fixes. Frame time is the source frame index in the
  stem over the file's frame rate, plus half the readout.
- `map/ImuExtrinsic.h` calibrates the IMU-to-lens rotation per camera group
  from the reconstruction: the hand-eye constraint `A X = X B` between the
  poses' relative rotations and the gyro's, and the gravity-pair constraint
  `R_j^T X g_j = R_k^T X g_k`, are both linear in the nine entries of `X`; the
  null vector of the stacked system, projected onto the orthogonal matrices,
  is the answer. Two things the note above got wrong: the constraints cannot
  tell `X` from `-X`, so handedness is not observable from them alone (the
  sign is settled by the cameras' mean up axis, then by the sign of the
  accelerometer scale, which must be positive); and a left-handed sensor frame
  needs the gyro integrated with the opposite sign, so the two signs are run
  as hypotheses and compared on the gyro pairs' residual in degrees. The IMU
  clock offset against the video is searched for first, on the rotation angle
  between consecutive frames, which is invariant to `X`.
- `map/SensorGauge.h` puts it together: up as the robust mean of the frames'
  votes; scale from the accelerometer through the velocity-free triple form
  of Mur-Artal and Tardos, solved linearly together with both biases (on a
  gentle walk the bias error is as large as the position signal, so scale
  alone comes out 50 percent wrong); scale, heading and place from the GPS
  through `fitMetricGauge` in horizontal mode; then one Levenberg-Marquardt
  solve over the Sim(3), the biases, a lever arm and an extrinsic increment
  per group, with Huber weights, numerical Jacobians and a per-family sigma
  taken from the closed forms' residuals. Two scale sources combine by
  information; beyond three sigma of disagreement the more certain one wins
  and the run says so.

Measured on a 118 s Insta360 X5 walk (231 frames, two lenses, 1 fps):
0.26 s for the whole fit; IMU clock offset -14.6 ms; the extrinsic from 114
frames with the gyro pairs agreeing to 0.43 deg; up votes agreeing to 4.9
deg median with 24 of 231 outliers over 10 deg; accelerometer scale 7.30 at
0.3 percent with the solved gravity at 9.83 m/s^2 and 0.3 deg from up; GPS
scale 6.32 at 19 percent (the phone's fix, 146 of 195 within 5 m), so the
joint answer 7.42 is the IMU's. The two lenses' centres in the metric model
sit 2.9 cm apart at the same source index, which is the camera body. The
cameras' mean up axis came out 11 deg from the IMU's, which is the error
`--orient` was making on this capture.

On a 146 s DJI Osmo 360 clip (292 frames, two lenses, 1 fps, attitude and
a 30 Hz accelerometer, no gyro, no GPS): the extrinsic from the attitude
agrees to 0.20 deg on the rotation pairs and 1.0 deg on the gravity votes,
and the 292 up votes agree to 0.23 deg with no outlier. The two lenses'
accelerometer scales, fitted separately, are 31.01 and 30.93 at 1.5 per cent
each (0.27 per cent apart) with the solved gravity at 9.83 m/s^2 and 0.0 deg
from up; the joint solve, with each lens's IMU lever arm free, ends at 28.91
at 1.1 per cent. The levers come out 4.7 and 4.3 cm and place the IMU within
1.4 cm of one point, which is a camera body 2.1 cm across; that lever is
worth 6.6 per cent of the scale here. Nothing outside the capture measures
it: the camera sits 1.9 m over the ground on a stick above the operator's
head, and 94.6 m of path over 146 s is the stroll the video shows.

On a GoPro MAX handheld walk (78 s, ten seam-free views per
frame at 2 fps, 1237 of 1550 registered): every view calibrates with the
gyro pairs agreeing to 0.2 deg; the IMU clock is 11 ms off the video from
1077 pairs; up over 1237 votes agrees to 3.1 deg. This capture is what found
the regression direction: the ten views' centres scatter 5 cm about their
frame's mean (the mapper places each view on its own), the second difference
of position over half a second is about 6 cm, and a slope fitted with that
noise in the regressor came out 10-50 percent low per view. Fitting the
INVERSE scale with the centres as the response (the pre-integrated prediction
is the precise side) put every view at 1.02-1.09 of the GPS scale, combined
1.046 at 0.7 percent against the GPS's 1.000 at 0.3 percent (18 Hz receiver,
1237 of 1237 within 5 m, 1.6 m RMS). The remaining 4.6 percent is flagged as
a disagreement and the GPS wins; whether it is the stick's vibration (8 m/s^2
RMS on the accelerometer) coupling into the 200 Hz accelerometer's
integration, or the receiver, is open.

On a GoPro MAX bike ride (11 m/s, 2 fps) the reconstruction
itself collapses into single-lens fragments whose centres sit on two points,
so the GPS fit is refused on every fragment and the calibration is
yaw-degenerate (the ride turns about the vertical only): up is still taken,
the accelerometer scale is withheld, and the handedness flag is not reported
because the two gyro signs cannot be told apart there. That case is what
added the `degenerate` path.

Synthetic coverage (`sfm_sensor_gauge_test`): IMU + GPS, IMU alone, a stale
GPS with mirrored IMU axes and a 37 ms clock offset (recovered to 41 ms), a
camera that only pans (up, no scale), a camera that never moves (declined),
GPS alone. Scale within 0.05 percent, up within 0.35 deg, the extrinsic
within 0.25 deg.

### 5.7 What the result says about itself

The fit is worth nothing to a viewer that cannot tell it happened. Every
written model now carries `sparse/N/gauge.txt` — `oriented`, `metric`, and
which source settled each — and the same two bits travel in the progress
snapshot the GUI draws while a run is going (`model.bin` version 3).

Two things read it. The dataset parsers fill `ParsedDataset::gauge_oriented`
/ `gauge_metric`, and the viewport uses the first to default its **auto-level**
switch off: the parsers rotate every dataset so the mean camera up axis
becomes +Z, which on the X5 walk is 11.4 deg from the IMU's answer, and that
rotation is what turntable and first-person navigation orbit about. The switch
is offered wherever the rotation is not the identity, and undoes only that
rotation — an `applied_transform` a file came with is left alone. Next to it
sits the **center** menu, which picks what the view orbits about (camera
position median by default; `src/data/SceneCenter.h`) and moves only the
camera. All three viewers offer it: the native viewport reads the table off
`ViewerRenderConfig`, and the browser clients ask for it — `/scene` for the
training viewer, `ssv_ds_fit_sphere` for the standalone one. The second bit
puts the grid's cell size on screen as a length, so a metric model can be
measured by looking at it.

The GUI also stopped hiding the inputs. Each video row says whether the file
carries IMU, GPS, both or nothing (read on its own thread, `TelemetryProbe`),
each photo folder says how many of its files have an EXIF position, and a
**Sensors** block under Advanced holds both controls: `--sensor-gauge` for the
video track and `--metric-gps` for the photographs.

## 6. Improving the reconstruction itself

**Implemented 2026-09-25** as `docs/notes/sensor-priors.md`: the first,
second, fourth and fifth items below, plus the accelerometer's scale as a
bundle-adjustment factor. The list is kept as the plan it was; the third item
(full visual-inertial BA with velocity states) and the sixth are still open.

- **Gyro-predicted relative rotation for pair verification and seeding.**
  Two frames a second apart have a relative rotation the gyro knows to a
  fraction of a degree. Handing it to the two-view estimator turns the
  5-point essential problem into a 2-point translation-only one (Kneip et
  al., "Robust real-time visual odometry with a single camera and an IMU",
  2011), which is faster, needs far fewer inliers, and cannot pick the wrong
  rotation on a scene of repeated texture. This is the cheapest robustness
  gain available: it touches `geometry/Essential.h` and the verification
  worker, and nothing in BA.
- **Rotation priors in bundle adjustment.** A residual per consecutive pair
  penalising the difference between the BA relative rotation and the gyro's,
  weighted by the gyro's noise over that interval. Holds a sequence together
  across textureless stretches and fixes drift-free the thing monocular BA
  drifts on. Needs the residual type `sfm-rig-constraints.md` item 3
  describes, and the same pair-Schur ownership care.
- **Full visual-inertial BA** with pre-integrated IMU factors between every
  consecutive frame (velocity and bias states per frame): the VINS/ORB-SLAM3
  formulation. Gives metric scale and gravity inside the optimisation rather
  than after it, and makes a sequence survive frames with no features at all.
  It is a different solver (per-frame velocity and bias columns, IMU factors
  with their own Jacobians) and belongs after `sfm-port-plan.md` §9's
  constant-parameter masks.
- **GPS in pair selection.** Frames within 20 m of each other are candidate
  pairs regardless of appearance, which is loop closure for free on an
  outdoor walk; the pair-selection stage already has a shortlist mechanism
  to union into.
- **GPS as a weak position prior in BA.** A residual per frame with a
  covariance of the receiver's error, which removes the long-baseline drift
  of a kilometre-long sequence. The correlated-error caveat applies twice
  over here: a prior that trusts consecutive fixes' agreement will pull the
  model into the receiver's slow wander.
- **Dual-fisheye rig timing.** The rig note's caveat about the two `.insv`
  tracks choosing frames tens of milliseconds apart is exactly what the
  telemetry can resolve: the IMU gives the rotation between the two chosen
  instants, so the rig constraint can be applied with the offset compensated
  instead of declined.

## 7. Open questions

- The IMU-to-lens rotation for each camera model (5.1's calibration). Until
  measured, every frame-dependent claim above is a plan.
- Whether GoPro's CORI yaw origin is north on the MAX (5.4).
- What the DJI `dbgi` track holds; if it is the raw IMU, 5.2 becomes
  available on the Osmo 360.
- Whether the X5's `gyro_timestamp` field (1.6 ms on the sample file) is the
  IMU-to-frame offset the synchronisation needs, or something else.
