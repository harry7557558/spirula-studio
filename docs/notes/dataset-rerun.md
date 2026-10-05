# What a dataset run reuses and what it redoes

A dataset run is four steps -- frames, masks, the reconstruction, depth and
normals -- and pointing it at a folder that already holds some of them is the
common case: a capture somebody reconstructed gets masks added, a finished
dataset gets depth and normals, a cancelled run is picked up. Each step either
reuses what is on disk or makes it again, and getting that wrong is expensive
in both directions: a needless rebuild costs the hour the reconstruction took,
a wrong reuse leaves frames or a model that no longer match the panel.

The answer comes from one function, `plan_dataset()` (`src/app/gui/
DatasetPlan.h`). The panel calls it every frame to list, above the button,
what pressing it will do; both runners call it again at each step, with the
settings the screen holds by then (a masking prompt can still be edited while
frames are extracted). Same function, same job: the list on screen is what
runs.

## The record

`<workspace>/.spirula-dataset.json` (`DatasetRecord.h`) holds two things:

- **The panel's settings** -- the preset field table plus the input rows
  (lens per row, rate, rig, order, clicks) -- kept once as the last run
  started and once per step as the run that *made* that step started. Reading
  merges them by step: a setting that feeds the reconstruction is taken from
  the run that built the reconstruction, so a run that kept the model with
  other settings on screen does not speak for it. Which step a preset key
  feeds is read off its spelling (`settings_step()`, `mask_*`, `geometry_*`,
  `sfm_*`/`colmap_*`, the rest frames). When the panel arrives at a folder
  that has a record -- a drop, a pick, a typed path -- it restores them, once
  per arrival; "Use the settings this dataset was made with" does it again on
  demand. That is what lets a dataset made from a video be dropped back in as
  its `images/` and updated without re-entering a setting.
- **Per step, the settings its output was made with**, as `(key, scope,
  value)` fields, plus an id for the output and the ids of the outputs it was
  made from. A step writes its section when it starts (`complete: false`) and
  marks it complete when it finishes.

The fields are a step's *inputs*, normalized so that the same output gives the
same fields however the panel was arranged:

- Frames: per input that writes images, its path and rate; the decoder,
  selection and unwrap settings only when a video is there to use them. A
  folder of photos depends on nothing else -- photos already gathered are kept
  whatever the import mode says.
- Masks: per input, masks it brought and the stencil drawn on it; the
  segmentation settings, split into those that make training masks and those
  that make `feature_masks/`.
- Reconstruction: per *camera folder* under `images/` (`cam0`, `cam1`, ...),
  the lens and focal it gets; rigs and sequences as their member folders; then
  the engine's own settings. Folders, not input rows, because a video's one
  row and the same frames dropped back as a folder with two sub-folders are
  different rows over the same cameras.
- Depth and normals: the model and its options. The kinds of map asked for are
  not fields: asking for depth beside existing normals adds depth, it does not
  redo normals.

A workspace written before the record carries `.spirula-frames` and
`.spirula-recon`. Its frames step is read from the first, so the old guard
against keeping frames unwrapped some other way still holds there. Its
reconstruction has no record and is always kept; the second only fills the
panel (`read_legacy_settings()`): the flags back onto the settings, and the
manifest's per-folder lenses, rigs and sequences onto the camera folders.

## The rules

Per step, in order, the first that applies:

| | frames | masks | reconstruction | depth & normals |
|---|---|---|---|---|
| the input already is the dataset's own | reuse | reuse | -- | -- |
| nothing on disk | run | run | run | run |
| asked to redo | redo | redo | redo | redo |
| the frames are made again | -- | redo | redo | redo |
| the reconstruction is made again | -- | -- | -- | redo |
| no record | reuse | reuse | reuse | add what is missing |
| made from an older output of the step before | -- | redo | redo | redo |
| record incomplete, same fields | finish | finish | finish | finish |
| fields differ | **redo, ask** | redo | **redo, ask** | redo |
| same fields | reuse | reuse | reuse | reuse, or add kinds |

"Ask" is the confirmation before the run: frames re-extracted or a
reconstruction rebuilt that nobody asked to redo. Its other answer -- keep
them, run the rest -- is `PlanRequest::keep_built`, also a checkbox in the
list, which turns both into **keep** until the output folder or the inputs
change.

### Masks

Changing a mask setting redoes the masks and **keeps** the reconstruction,
which the list says ("built before the current masks"). A reconstruction that
kept feature points out of the old masks would come out slightly different
with the new ones, but not by enough to spend the hour, and "Reconstruct
again" is one tick away. The fields that say whether masks reach the
reconstruction at all are soft for the same reason.

Mask files are also written only when their bytes change, so a pass that
produces the same mask -- the border stencil applied again -- leaves its
modification time alone, and with it every feature file `spirula sfm` would
otherwise consider stale.

## Restoring onto the dataset's own images/

Dropping `<dataset>/images` gives the panel one photo folder with a
sub-folder per camera. `restore_record_inputs()` gives each sub-folder the
lens, focal, rig and order the reconstruction's fields recorded for that
folder, so the reconstruction's fields come out the same and it is reused.
A folder shot in order whose camera folders nest (`a/cam0`, `a/cam1`, `b`) is
one sequence per top-level folder (`input_sequences()`), which is what a
dataset made from several videos is. What cannot be carried over is noted
rather than guessed: a video's telemetry does not come with its frames (it
never was a field), and a 360 file's rig rotations, which only the file knows,
become a free rig -- a difference the plan shows, and that "keep" answers.

## The output folder

The panel derives the output folder from the inputs until the user types or
picks one, and `default_workspace()` never points at a folder with content in
it. Re-derived on an edit of a row after a run, it would move the output to
`<name>_dataset_2`, and the next run would start from nothing. So while the
inputs still name the folder (`workspace_named_by()`), it stays. A fresh session still starts a new folder
beside an existing one; pointing the output at the old one is how to update it.
