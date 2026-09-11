# Rig constraints in the mapper and BA — a survey

Written 2026-09-08 while adding GoPro `.360` support (`docs/datasets.md`, "360
cameras"). **Implemented 2026-09-10 as item 4 below, with items 1 and 2 on top
of it**: `src/sfm/core/Rig.h` is the definition, `src/sfm/README.md` "Rigs"
what the run does with it, `src/sfm/ba/README.md` "Rigs" the solver layout.
The survey is kept as written; the question it answered was what it would take
to tell the reconstruction that a set of images has a fixed — possibly
optimizable — relative pose, and whether that would be a better answer than the
current one for a 360 capture.

`src/sfm/README.md` lists rig constraints as deliberately out of scope, and
`docs/notes/sfm-in-process-plan.md` §4 already says where they would arrive
from (a `sfm::Manifest` carrying rig definitions, instead of the
directory-of-JPEGs-plus-prefix-flags interchange we have). This note is about
the other half: what would consume them.

## Why it comes up

A `.360` unwraps into ten perspective views per frame. Within one lens, the
five views share an optical centre **exactly** and their relative rotations are
ours by construction — they are a pure-rotation rig with no unknowns at all.
Between the two lenses there is one unknown 6-DOF (about 3 cm of baseline and a
180-degree turn), constant for the whole capture and, for that matter, for the
camera model.

The reconstruction is told none of this. Every one of the ten views is
registered on its own, and views of the same frame share no features with each
other — they only meet at their edges — so what holds them together is pairs
across *time*. On a 78-second handheld walk at 2 fps this reconstructs, but as
several components (measured: 645 of 930 images over 3 components, 0.84 px);
the pieces are usually different views of the same frames, which a rig would
have fused for free.

The same shape of problem is the reason a single equirectangular image per
frame "aligns more easily": it is one camera, so the connection between what
the six directions see is not something the mapper has to discover.

## What the code assumes today

| | |
|---|---|
| pose parameterization | 6 per image, angle-axis + t, `map/Bundle.h:180`; the reduced camera system's columns are `6 * image_index` (`ba/Problem.h`, `pose_dim = 6 * num_images`) |
| intrinsics | per *group*, shared across images, with a free/fixed split (`Group::n_intr`, D50) — the one place sharing already exists |
| points | Schur-eliminated; the reduced system is camera-side only |
| linear solves | dense-by-observation, dense-by-pair, or matrix-free PCG (`ba/Solver.h`). The pair path needs `exclusiveGroups()`: **every column of S owned by at most one image pair** |
| preconditioner | one block per image over `[pose | intrinsics]`, or a split partition when a group is shared (`buildPrecBlocks`) |
| registration | 2D-3D PnP RANSAC per image, then nonlinear refinement and inlier gates (`map/Mapper.h`) |
| merging | Sim3 from **shared images** only (`map/Merge.h`); two models sharing no image cannot be related — and `src/sfm/README.md` lists exactly that as out of scope |
| interchange | path-prefix flags: `--camera-model cam0=pinhole`, `--focal cam0=752`. A rig is not expressible |

The one structural fact that matters most: **poses are per image and their
columns are indexed arithmetically**. Intrinsics sharing was designed in;
pose sharing was not.

## Four places a rig could bite, cheapest first

### 1. Rig-aware model merging (no BA change)

`alignReconstructions()` needs shared images to get a Sim3. Given a rig, two
models that share *no* image can still be related: if model A holds
`cam0/00042` and model B holds `cam3/00042`, and the rig says how those two
views sit relative to each other, that is a pose correspondence — and a handful
of them across the overlap gives the same Sim3 the shared-image path computes,
by the same RANSAC over predicted centres.

This is the smallest change with the largest effect on what a `.360` capture
currently produces, because the components it fails to merge are usually
different views of the *same frames*. It needs: a rig table, a way to ask "what
is the rig-mate of this image in that model", and one more candidate-pair
source in `MergeSession`. It touches no kernel and no parameter layout.

**Effort: small.** A day or two, mostly in `map/Merge.h` and the plumbing to
carry the rig in.

### 2. Rig completion at registration (no BA change)

When PnP registers one view of a frame, the other nine follow from the rig
without any 2D-3D correspondence of their own. Today each has to earn its own
inliers, and a view pointing at sky or at the operator's chest never does — the
per-folder registration counts in the measured run are 88 to 153 out of 155
depending on which way the view points.

The mechanics are already there: `registerImage` sets a pose and then filters
and triangulates. A rig-completed pose would be seeded rather than solved, and
then subjected to the same gates (an image whose rig-implied pose has no
support is still refused). The risk is the obvious one — a wrong seed pose that
passes the gates poisons the model — which argues for admitting a completed
image only when it triangulates.

**Effort: small-to-medium.** Contained in `map/Mapper.h`.

### 3. Rig residuals as a soft prior in BA

Add a residual per rig edge: the deviation of `pose_b` from `rig_ab * pose_a`,
weighted by how much you trust the rig. This leaves the parameter layout alone
— poses stay per image, columns stay where they are — and only adds terms to
the reduced system.

The catch is *where* those terms land. The pair-Schur path's tables are built
from co-visibility (`pair_entries`, one entry per shared point), and two views
of one frame share no point, so the pair simply does not exist in the table.
A rig prior would have to inject pairs, and `exclusiveGroups()`/`buildPrecBlocks`
would need to keep holding. The PCG path is friendlier: it never forms S, so a
prior is one more contribution to the matrix-free product.

There is also no constant-parameter mask for poses (only for intrinsics), which
`docs/notes/sfm-port-plan.md` §9 already lists as missing and which a hard rig
would want.

**Effort: medium.** Host + Slang, both linear solvers, and a new residual type
in a solver that currently knows only reprojection.

### 4. Reparameterization — the real thing

`pose_image = pose_rig ⊕ extrinsic_member`, with the rig pose and the member
extrinsics as the parameters. Six columns per *frame* instead of per image, plus
six per rig member for the whole capture. This is what COLMAP 3.10's rig support
does and what a fully general "optimizable relative pose between arbitrary pairs"
means.

Everything in the table above moves: the column layout (`6 * i` is no longer the
address of an image's pose), the Jacobian blocks (an observation now
differentiates against two pose blocks, so `kMaxCamDof` and the `2 x dof` block
shape change), the pair-Schur ownership test, the preconditioner partition, the
Slang kernels in `sfm/shaders/ba/ba.slang`, and the host mirror in
`ba/SolverCpu.h`. The mapper's PnP would want a rig-aware variant
(generalized-camera absolute pose) to exploit it fully.

**Effort: large.** This is the one that is out of scope for a reason: it is a
rewrite of the solver's addressing, not a feature on top of it.

## What a `.360` rig would actually contain

Worth stating because it is much stronger than the general case:

- Within one lens, five views, **zero relative translation and known relative
  rotation**. No parameters at all. A rig prior here is a hard constraint that
  cannot be wrong.
- Between the two lenses, one 6-DOF, **constant for every frame of every
  capture from that camera model**. It could be calibrated once from a good
  reconstruction and then reused — or left as the single optimizable rig
  parameter, which is the smallest useful instance of item 3 or 4.

So a 360 capture does not need "arbitrary optimizable relative poses" to
benefit. Items 1 and 2, with a fixed known rig, would take the ten views of a
frame from ten independent guesses to one pose plus a table.

## The dual-fisheye caveat

`.insv` extraction runs `extract_track` **per track**,
and each track picks its own sharpest frame within a window
(`FrameExtract.cpp`). Two tracks can therefore keep frames a few tens of
milliseconds apart, and on a moving capture that is not a rig — it is a rig plus
an unknown, time-varying offset. Any rig work on dual-fisheye needs the paired
extraction the `.360` path already does (`extract_pair`, one window over both
tracks, one decoded index for both), or a recorded per-image timestamp so the
mapper can decline the constraint when the pair is not simultaneous.

The `.360` path is already rig-ready in this sense: the ten views of a frame
come from one decoded frame pair and carry the same file stem, so the rig group
is recoverable from the filename alone — which is exactly what item 1 needs and
is why it is cheap.
