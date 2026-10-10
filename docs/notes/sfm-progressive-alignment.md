# Progressive alignment — plan

Status on `sfm-progressive-alignment`: the file-order weight (§7,
`--order-weight`, off by default), the error ladder with a final
retriangulation (§2–3, `--progressive`), the feature passes (§4–5), their GUI
(§8.2), `progressive.txt`, the time limit and `Stage::Progressive` are in. Not
yet: `spirula sfm progressive WORKSPACE` on a finished run, resume per
attempt, and keeping the putative matches so the tighter attempts re-verify
from them rather than from the loose inliers.

Measured, 260-image capture at defaults: 254/260 at 0.81 px mean against a
plain run's 252/260 at 0.83 px, 65 s against 43 s; the feature passes found
nothing to add for its last six images and were undone, as designed.

A run often ends with `sparse/0` plus a few smaller models (`sparse/1..N`) and
some unregistered images. This plan adds a loop that re-runs the parts of SfM
that decide *whether* an image gets in — the error tolerance, and feature
detection on the images that did not make it — until a few passes in a row
bring nothing new.

Scope, as agreed:

- **One frontend and one matcher per run.** A SIFT run stays SIFT + brute force;
  a LoMa run stays LoMa. The loop changes *settings*, never the algorithm.
- **No camera-repair code.** PR #138 is reverted on this branch (`d697d9ca`); nothing
  here depends on it.
- **The levers** are the ones that work in RealityScan in practice: a pixel
  error that starts very loose and is tightened pass by pass, and more image
  resolution and more features for the images that need them.
- **Effort goes to images that formed their own model**, then to unregistered
  images with real matches. Images with no verified matches are outliers and
  are left alone.

## 1. What the knobs do in this pipeline today

| knob | flag | where it acts | default (`--quality high`) |
|---|---|---|---|
| pixel tolerance | `--max-error` | ONE value (D47), fanned out by `SfmConfig::finalize`: two-view RANSAC (`twoview.ransac.max_error`), the mapper's registration, triangulation and filtering (`mapper.max_reproj_error`), and the merge filter | 3 px, in extraction pixels |
| resolution | `--max-image-size` | extraction | 2400 SIFT, 1600 learned |
| feature count | `--max-features` / `--aliked-max-features` / `--loma-max-features` | extraction, top-K by scale | 8192 SIFT |
| SIFT contrast | `--peak-threshold` | extraction; lower finds more features in flat or dark texture | 0.0067 |
| SIFT edges | `--edge-threshold` | extraction | 10 |
| ratio test | `--ratio` | brute-force matching | — |
| verified pair floor | `--min-inliers` | verification | — |

Two facts follow:

1. **The tolerance changes nothing about features**, so the error ladder can
   reuse one `features/` directory and keep model continuity: every pass
   indexes the same rows.
2. **Resolution, feature count and SIFT thresholds change the rows** of every
   image they are applied to. A model cannot carry across that change for
   those images. Applied *only to the target images*, though, the main model's
   images keep their rows and their poses.

## 2. Why "start at 20 px and come down" works, and what it needs here

At the start of a run the intrinsics are guesses: focal from EXIF or a default,
and distortion at zero. On a wide lens, a correct match can then sit tens of
pixels off any epipolar line or reprojection the pipeline computes. A 3 px gate
throws those matches away before the geometry that would explain them exists.
A 20 px gate keeps them, the model forms, bundle adjustment solves focal and
distortion, and then a tighter gate on the now-correct model discards the
outliers the loose gate let in. RealityScan's "realign continues from the
previous state" is exactly that: tighten, keep the cameras that still hold,
re-register the rest at the new tolerance.

Here that needs three things the pipeline does not do in one run today:

- **Verify once, at the loosest tolerance.** `matches.bin` verified at 20 px is
  a superset of what any tighter pass needs. Each later pass *re-verifies*
  from those stored inliers at its own tolerance. That is host RANSAC over
  matches already in memory, with no descriptor matching, so it costs seconds.
  The mapper never sees a pair verified looser than its own gate (D47 stays
  true per pass).
- **Continue, don't restart.** Pass k+1 takes pass k's models, drops
  observations over the new tolerance (`filterModel`), and refines. Images
  left with too little support are deregistered. Then the mapper grows again
  (`Mapper::continueFrom`) with the registration-failure counters reset, and
  `assembleModels` merges what now overlaps.
- **End at the user's tolerance.** The last pass is always `--max-error` as
  given (3 px by default), so the written model is held to the same standard
  as a normal run. The ladder is how the model gets there, not a looser result.

Compaction (`--compact-unused-features`) is planned from the loosest database,
whose rows are a superset of every tighter pass's, so one compaction serves the
whole ladder and the models never need re-indexing between passes.

## 3. The loop

```
features/  extracted once with the run's settings
matches    verified once at e_0 (default 20 px)
models     = map at e_0

ladder:  e_0 = 20 → 12 → 8 → 5 → e_final (= --max-error, 3)
for each e_k after e_0:
    db_k    = reverify(matches, e_k)
    models  = [filterModel(m, e_k) → refine]      for each model
    mapper  = Mapper(db_k, ..., max_reproj_error = e_k)
    models  = assembleModels(mapper, models)        # grow with reg_trials reset, merge, finish

then, while targets remain and patience is not used up:     (feature passes, §4)
    pick the next feature step for the target images
    re-extract ONLY the targets, drop their old pairs, match them against partners
    re-run the tail of the ladder (e.g. 8 → 5 → e_final) on the result
    keep the pass if accepted (§5), else restore models, features and matches
```

## 4. Feature passes on the targets

### Targets

- **Component**: the images of `sparse/k` (k ≥ 1) that `sparse/0` does not hold.
  Priority is size, then link evidence (verified matches to `sparse/0`).
- **Near miss**: an unregistered image with at least `--progressive-min-matches`
  (default 50) verified inliers to any registered image.
- **Outlier**: everything else. Never re-extracted, never matched. This is the
  "don't spend time on outliers" rule.

### Steps

Each step re-extracts the targets with the run's own frontend and changed
settings, in this order (the cheapest step that might work goes first):

| step | SIFT | ALIKED / LoMa |
|---|---|---|
| F1 more features | `max-features ×2` | `*-max-features ×2` |
| F2 more resolution | `max-image-size` up one quality rung (up to the source size) | same |
| F3 lower contrast floor | `peak-threshold ÷2`, `edge-threshold ×1.5` | `aliked-min-score ÷2` (LoMa: no floor to move) |
| F4 both | F1 + F2 + F3 together | F1 + F2 |

Mixing settings per image within one run is safe for matching. SIFT and the
learned descriptors are scale-normalized, and each `FeatureSet` already
carries its own `extract_width/height`, so `pixelScale()` and every tolerance
derived from it stay correct per image (D46/D47).

Matching for the targets:

- **Partners**: the images of `sparse/0` most likely to see a target. They are
  ranked by file order first, measured from the component's boundary frames
  (§7.3). Then comes pair selection restricted to target × registered, with a
  wider neighbour count than the run's; this brings back a small
  `prefilterPairsFor`, which went out with #138. GPS proximity is added when
  there is GPS.
- **Pairs inside a component** are re-matched too, so the component can still
  seed on its own if growth from `sparse/0` does not reach it.
- Matched with the run's matcher, verified at the ladder's current tolerance.

## 5. Accepting or undoing a pass

A pass (one ladder step, or one feature step) is kept only if all of these hold:

- distinct images registered in `sparse/0` went up, or nothing went down while
  a sub-model merged in;
- `sparse/0`'s mean reprojection error, measured at the pass's own tolerance,
  is no more than 10% worse than the previous pass at that tolerance;
- the fold detector (D45/D67) finds no new fold;
- no image that was in `sparse/0` before is out after (when the ladder
  tightens, losing an image is expected, so for ladder passes this is a count
  reported in the log, not a veto).

A refused feature pass restores the targets' old feature files and pairs, so
the next step starts from the same place. Each pass writes its own
`features.progressive/<pass>/`; a kept pass's targets are read from there by
the passes after it, never from `features/` again.

A pass that fails outright (most often the GPU running out of memory at the
larger size) is undone the same way and ends the passes; the ladder's model is
kept and the run finishes. With `--progressive-image-size-end 0` the learned
frontends stop at twice the run's `max-image-size`, not at the source size:
LoMa's working set at 3869 px is 14.6 GB.

Every attempt and every pass ends with a model snapshot, so the GUI's model
view follows the run. Beside `model.bin` the run writes `images.bin`
(`sfm/core/Progress.h`): per image, placed or not, its features, the 3D points
it sees and its mean and max reprojection error. The workspace keeps a copy,
`image_stats.bin`, which outlives `.progress`. The dataset screen's Images
view lists it; picking an image shows the photo and can put the model view
at its camera. The model's cameras are coloured red to green by an image's
mean error -- the measure the run reports and passes are judged by -- linear
from a third of the ladder's start (red) to a third of its end (green): an
image's mean sat near a third of its gate (1.5 px at 20, 0.9 px at 3). Or by
3D points against the placed images' median (an eighth of it up to it).

`--quality` sets the ladder and the passes (`sfm/ProgressivePresets.h`), and
the GUI's quality control fills the same values in: start 12/16/20/24 px over
5/8/12/15 attempts, no feature pass at low, then 2/3/4 passes, patience
1/2/3/4. Provisional: on the 260-image capture above nothing was placed past
five attempts and a feature pass cost about one attempt, so these leave
headroom for harder captures, which has not been measured yet.

Stopping: `--progressive-patience` (default 2) feature passes in a row with no
gain, all steps tried for all targets, `--progressive-time`, or cancel.

## 6. Better matchers for repeated and look-alike structure

The problem splits in two, and the two halves need different fixes.

**a) Real matches that are hard to find** (texture repeats, wide baseline,
other side of the object). A stronger matcher helps here, chosen at the start
of the run and kept for the whole run:

| candidate | in this repo | for this problem |
|---|---|---|
| LoMa-G (`--matcher loma-g`, 256-D DeDoDe-G) | yes, `src/loma/` | the most accurate LoMa variant; the authors report it ahead of RoMa v2 on WxBS and HardMatch. Not yet measured here against `loma-b128`. **First thing to measure.** |
| LoMa-R | yes | rotation-invariant: rolled drone and handheld shots, upside-down views of an object |
| ALIKED + LightGlue | yes | the previous state of the art; slower per pair than brute force, faster than LoMa-G |
| RoMa v2 (dense) | inference yes (`src/roma/`, used by `dense/`), not as an SfM matcher | strong under extreme viewpoint change. Needs a dense-to-sparse step (sample matches at shared keypoint locations, as Dense-SfM does) before it can feed this mapper. A bigger project; worth it only if LoMa-G falls short |
| RDD (CVPR 2025) | no | deformable-transformer detector and descriptor aimed at robustness; unproven against LoMa |

**b) Wrong matches that look right** (two identical facades, the front and
back of a symmetric object). A better matcher makes this *worse*: it finds
more confident matches between the two look-alike sides. The fixes are
disambiguation, not matching:

- What exists: the fold detector (D45/D67), the seam checks (D68), and
  sequence trust (D79), which uses file order as evidence a duplicate cannot
  fake. For a walk around an object, declaring the capture a sequence is the
  cheapest strong defence there is.
- **Doppelgangers++ (CVPR 2025)**: a pair classifier on MASt3R features that
  removes look-alike edges from the match graph before mapping. It is the
  state of the art for exactly this case. Cost: a MASt3R encoder (ViT-L) ported
  onto `nn/`, run per verified pair or per suspicious pair. That is a project
  of its own; it would sit between verification and mapping and be judged by
  the fold detector's numbers.
- **Geometry-aware matching (3DV 2025)**: feeds geometric cues back into
  matching for repeated texture. It is a matching change, so it fits only as
  the run's matcher.

The error ladder interacts with (b): a loose 20 px gate admits more look-alike
matches early. The fold detector runs at every merge already, and a feature
pass is undone when it reports a new fold (§5). Test 3 below is the check.

## 7. File order as a weight on which pairs are matched

### 7.1 What the code does with file order today (`dev` at `0856b86d`)

| mechanism | where | default | what it does |
|---|---|---|---|
| sequential window | `sequentialPairs` (`feature/Pairing.h`), `--pairs sequential`, `--overlap` | `--data-type video` below 100 images | each image with the next `overlap` in its folder; a hard window |
| quadratic overlap | `--quadratic-overlap` | **off** | also i + 2^k for k < overlap (COLMAP's `quadratic_overlap`) |
| loop closure | `--loop-closure`, `matchFeatureDir` | on | sequential mode only: unions the content shortlist (`prefilterPairs`) into the window, the role COLMAP's vocab-tree `loop_detection` plays |
| prefilter + sequential | `--prefilter-sequential` | **off** (the `internet` preset forces it off too) | prefilter mode only: unions the whole window into the shortlist |
| `auto` mode switch | `run_auto` | at ≥ 100 images | exhaustive / sequential become prefilter unless `--pairs` was given |
| sequence matching window | `matchFeatureDir`, under `#if 0` | **disabled** (c579f7ef: "less robust for datasets that already have a lot of pairs") | would always match each declared sequence's window |
| sequence trust in mapping | `MapperOptions::sequence_window` (D79) | **2**, no longer tied to `--overlap` (c579f7ef) | seed among neighbours, grow along the sequence, prefer the near pose |
| track merging | `Mapper::mergeTracks` | on | fuses two tracks of one feature: the *mapping* half of a loop closure |
| GPS proximity pairs | `gpsProximityPairs` (`feature/GpsPairs.h`), `--sensor-pairs` | off | loop closure from position, whatever the images look like |

The consequence that matters here: **a video of 100 frames or more is matched
with no file-order pairs at all.** `auto` switches it to prefilter, and
`--prefilter-sequential` is off. Only content decides. A weak step that the
content score ranks just outside an image's top `prefilter-neighbors` is
never matched, which is exactly the kind of break that leaves a sub-model.
Declaring a sequence does not add those pairs either, because the matching
window is disabled. D79's mapper then trusts neighbour pairs that may not
exist.

### 7.2 Documentation that disagreed with the code

Fixed in the same commit as this plan:

- `src/sfm/README.md` (pairing section) said `--prefilter-sequential` is "on
  by default" and `--quadratic-overlap` is "(on)". Both default to `false`
  in `SfmConfig.h` and in `SfmJob`, since 8c76625f.
- `src/sfm/README.md` ("Sequences") said `--overlap` is "the one number": the
  matcher's window along each sequence, "matched whatever `--pairs` chose",
  and the mapper's neighbour distance. Since c579f7ef the window is not
  matched (`#if 0`), and the mapper's distance is a fixed 2.
- `quadratic_overlap_help` (and the README) said the extra links are 16, 32,
  64 ... apart; `sequentialPairs` adds 1, 2, 4 ... 2^(overlap−1).
- `overlap_help` said `--overlap` also sets the mapper's neighbour distance.

Still open, deliberately not changed:

- `DatasetPlan.cpp` `apply_legacy_recon` treats a run with no
  `--no-prefilter-sequential` flag as having had it on. That is right for runs
  from before 8c76625f and wrong for runs after it, where the default is off.
  Check this with whoever owns the dataset record before changing it.

### 7.3 The weighting

A soft prior inside pair selection, replacing neither the window nor the
shortlist:

```
rank(i, j) = score(i, j) · (1 + α · exp(−gap(i, j) / τ))     if i, j share an order
           = score(i, j)                                       otherwise
```

- `score` is pair selection's own asymmetric mini-match count
  (`scoreOrderedPairs`), and `min_score` still applies to it *before* the
  boost. A neighbour with no content match gets no pair. That is the
  difference from the hard window, and the likely reason the window hurt on
  pair-rich captures: it forces every neighbour pair through verification.
- `gap` is the distance in positions within one folder (`folderRuns`) or one
  declared sequence (`SequenceTable::pos`, so a rig's lenses share
  positions). Pairs across folders or sequences get no boost.
- **Near pairs are boosted; far pairs are never penalized.** A walk that comes
  back on itself needs its far pairs; that is what loop closure is.
- It changes the *rank* inside each image's `num_neighbors` budget
  (`TopPartners::add`), so the number of pairs matched stays the same. A
  variant reserves `--order-slots` (for example 4) extra slots for the
  nearest positions that pass `min_score`. Which variant to ship is decided
  by measurement (§10, phase 0b).
- When it applies: `--data-type video`, a declared sequence, or a folder
  marked "Shot in order". Never for `internet`, whose file order means nothing.
  An `individual` folder gets it only when its names sort numerically (camera
  counters, `IMG_0001`), and the run says so.
- Defaults to start measuring from: α = 1 (an adjacent frame's score counts
  double), τ = `--overlap` / 2.

Config rows (group `pipeline`, next to `prefilter-sequential`):
`order-weight` (α, 0 = off), `order-decay` (τ, in positions), `order-slots`.

Cost is small: one position per image, computed once in `matchFeatureDir`,
and a multiply in `TopPartners`. The coarse shortlist pass (`coarse_neighbors`,
over all N² pairs) should take the same weight, or a near pair can be cut
before the reliable score ever sees it.

### 7.4 In progressive alignment

A sub-model spanning positions 340–385 broke away at its ends, so its
partners are ranked first by distance from **its first and last positions**
(300–339 and 386–420), then by the boosted content score, then by GPS
distance. A near-miss single image is ranked by distance from its own
position. This replaces "widen the shortlist everywhere" with "look where the
break happened", and costs far fewer pairs.

### 7.5 Against look-alike structure

The two sides of a symmetric object are usually far apart in a walk around it.
Boosting near pairs spends the matching budget on true overlap first, so
fewer of an image's slots go to its look-alike. That is a nudge, not a
guarantee: D79's sequence trust in the mapper and the fold detector remain
what actually refuse a fold.

## 8. Where it plugs in

The feature is called **progressive alignment**: one run that starts loose
and tightens, the way RealityScan's repeated realignment does.

```
src/sfm/Progressive.h / .cpp           the ladder, the feature passes, acceptance, progressive.txt
src/sfm/feature/PairSelection.h        prefilterPairsFor(feats, opt, targets, partners), restored
src/sfm/feature/Verification.h         reverify(db, feats, cams, tol): RANSAC over stored inliers
src/sfm/tests/sfm_progressive_test.cpp §9
```

- **`spirula sfm auto --progressive`** runs the ladder inside `run_auto`. It
  replaces the single `runMapper` call; matching verifies at the start error
  instead of `--max-error`, and everything after `finishModels` is unchanged.
  The feature passes come after the ladder.
- **`spirula sfm progressive WORKSPACE`** runs only the feature passes on an
  existing workspace (its `features/`, `matches.bin`, `sparse/`), for a run
  already made. Reading a written model back needs the row mapping that
  compaction makes necessary; with #138 gone, that is written fresh and small
  (match model rows to feature rows by keypoint position).

### 8.1 Settings: a start and an end for each thing that moves

Every ladder is given by where it starts, where it ends and how many attempts
lie between them. The steps are spaced geometrically, so 20 → 3 px in 5
attempts is 20, 12.4, 7.7, 4.8, 3. A list of values is not part of the
interface.

| flag | default | meaning |
|---|---|---|
| `progressive` | off | the switch |
| `progressive-error-start` | 20 | pixel error of the first attempt |
| `progressive-error-end` | 0 = `--max-error` | pixel error of the last attempt, and of the written model |
| `progressive-error-steps` | 5 | attempts from start to end, both included |
| `progressive-features` | on | the feature passes on images still unaligned after the ladder |
| `progressive-max-features-end` | 0 = 4 × the run's | feature count the passes end at; they start at the run's own |
| `progressive-image-size-end` | 0 = the source size | working resolution the passes end at; they start at the run's own |
| `progressive-feature-steps` | 3 | feature passes from start to end |
| `progressive-patience` | 2 | stop after this many attempts in a row that add no camera |
| `progressive-min-matches` | 50 | verified matches an unaligned image needs to be worked on (§4) |
| `progressive-time` | 0 = no limit | minutes |

Each row is in group `progressive` of `SFM_CONFIG_FIELDS`, with its help in
`i18n/catalog/SfmFields.h`. The SIFT contrast step (F3) runs on the last
feature pass rather than as a setting of its own.

### 8.2 GUI

One checkbox on the reconstruction panel, off by default. Ticking it shows the
rest, indented beneath it; unticked, none of it is on screen:

```
[x] Progressive alignment                                      (?)
      Pixel error        from [ 20.0 ]  to [  3.0 ]  in [ 5 ] attempts
      [x] Re-detect features on images still unaligned
            Features     from  8192 (quality)  to [ 32768 ]
            Image size   from  2400 (quality)  to [ source ]
            in [ 3 ] attempts
      Stop after [ 2 ] attempts that add no camera
      Time limit [ 0 ] minutes (0: none)
```

- The "from" values of the feature passes are the quality preset's own (or
  the advanced editor's), shown read-only, so the range reads as a range.
- "to" for the pixel error defaults to the run's `--max-error`. Set
  explicitly, it is the tolerance the written model is held to (`finalize`
  hands it to the mapper); a start at or below it is refused.
- `SfmJob` gets one field per row: `progressive`, `progressive_error_start`,
  `progressive_error_end`, `progressive_error_steps`, `progressive_features`,
  `progressive_max_features_end`, `progressive_image_size_end`,
  `progressive_feature_steps`, `progressive_patience`, `progressive_time`.
  Each gets its `X(...)` row in `DatasetPreset.cpp` (`preset_roundtrip_test`
  guards this) and an entry in `model_fields()` (`DatasetPlan.cpp`), and is
  passed by `SfmRunner` as flags only when the box is ticked.
- Every label and hover help is a `Msg` in `i18n/catalog/Dataset.h`, in all
  13 languages.
- The advanced options editor lists the same rows under a "Progressive
  alignment" group, from the table, as it does every other flag.

### 8.3 Run-time plumbing

- **Resume (D76)**: one signature per attempt, so an interrupted run picks up
  at the attempt it was on.
- **Events**: `Stage::Progressive`, appended so `status.bin` numbering holds
  (as `Stage::Focal` was). The progress line reads "attempt k of n: 7.7 px,
  +12 cameras".
- **Output**: `progressive.txt`, one line per attempt (kind, setting, images
  worked on, registered before and after, mean error, seconds, kept or
  undone), plus one summary line in the run report.

## 9. Tests

`src/sfm/tests/sfm_progressive_test.cpp`, synthetic scenes from `SyntheticRegister.h`:

1. **Unknown distortion.** A wide-lens scene with strong k1, started at zero
   distortion. At 3 px it fragments; with the ladder it comes out as one
   model, ending at 3 px with correct intrinsics.
2. **Ladder stops cleanly.** A scene that already reconstructs: `--progressive`
   gives the same images and poses within tolerance, and no feature pass runs
   (there are no targets).
3. **Look-alike guard.** Two identical facades with the true link removed. Run
   with and without the ladder; neither may merge them.
4. **Outliers untouched.** Images with no verified pair are never re-extracted
   or matched (count the extractions and pairs).
5. **Per-image settings.** Targets re-extracted at a different
   `max-image-size` keep their tolerances in their own pixel scale; the main
   model's rows and poses are unchanged byte for byte.
6. **Order weighting (no GPU).** On a synthetic score table, a neighbour just
   below an image's top-k moves into it; a neighbour under `min_score` does
   not; a far pair's rank is unchanged; pairs across folders get no boost; and
   with α = 0 the pair list is identical to today's.
7. **Weak step in a walk.** A synthetic sequence where one adjacent pair's
   content score sits just outside the shortlist. Without the weight it maps
   as two models, with it as one. This goes in `sfm_sequence_test`.

End to end, `tools/sfm/eval_poses.py` on captures that fragment: registration
rate, AUC@10 (capped by registration², so report both), time.

## 10. Phases

| # | deliverable | done when |
|---|---|---|
| 0 | **Baseline and matcher check.** Fragmenting captures run at 3 px, and by hand with `--max-error 20`, and with `--matcher loma-g` | a table of where the images are lost, and whether LoMa-G alone changes it |
| 0b | **File-order check.** The same captures with `--prefilter-sequential` on, which needs no new code, and per sub-model the content score of the pairs across its boundary | whether the breaks are pairs the shortlist dropped; if so, the weighting goes first |
| 0c | **Doc fixes** from §7.2 | README matches the defaults |
| W | **Order weighting** in `TopPartners` (both passes), config rows, run log line | tests 6–7; no capture loses images against `--prefilter-sequential` off or on |
| 1 | **Error ladder** in `auto --progressive`: `reverify`, continuation, acceptance | tests 1–3; no capture loses AUC |
| 2 | **Feature passes**: targets, `prefilterPairsFor`, per-target re-extraction, undo | tests 4–5; gain on at least one phase-0 capture |
| 3 | **`spirula sfm progressive`** on an existing workspace | runs on a workspace from before the feature |
| 4 | **GUI, presets, resume, docs** (README section, `sfm-design.md` D80) | `preset_roundtrip_test`; guictl run |
| later | Doppelgangers++ as a graph filter; RoMa v2 as a sparse SfM matcher | only if phase 0 says matching, not tolerance, is what loses the images |

Phase W is independent of the ladder and helps every run, not only
`--progressive`, so it can land first if phase 0b says so.

## 11. Open questions

1. Ladder default `20, 12, 8, 5, 3`: does that match what works in RealityScan,
   or do you step down differently?
2. Should the ladder be on by default for `--quality high`, once phase 1 has
   numbers?
3. Name: "progressive alignment" (`--progressive`), or something else?
4. Should a folder of numbered photos (`IMG_0001...`) get the order weight by
   default, or only when "Shot in order" is ticked?
5. Is the disabled sequence matching window (`#if 0`) and the mapper's
   neighbour distance of 2 a settled decision, or a tuning step to revisit?
   The weight is the soft version of that window.
