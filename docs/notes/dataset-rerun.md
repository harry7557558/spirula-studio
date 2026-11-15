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
- **The videos the frames were cut from** (the frames step's `captures`: path,
  folder, what a stem counts), written when that step finishes. They are not
  fields, but their timing must be known to reuse existing video frames. They
  also give frames brought back their video's IMU and GPS: a run that keeps
  the frames reports these rather than what its own decoder would have
  numbered, and a photo input that is some dataset's `images/` takes that
  dataset's (`captures_behind`). A record without them, including a workspace
  with only the legacy stamp, cannot infer the timing from the current decoder.

The fields are a step's *inputs*, normalized so that the same output gives the
same fields however the panel was arranged:

- Frames: per input that writes images, its path, rate and source identity;
  the decoder, selection and unwrap settings only when a video is there to
  use them. Source identity covers the photo inventory or video and any
  file-backed video LUT (details below). Photos already gathered are kept
  whatever the import mode says, unless their source identity changes. The
  blur rule also applies to gathered photos: its threshold, neighbour rule
  and similarity threshold are fields wherever it applies, listed only when
  the rule is on, and a threshold set by hand is a value of its own because
  that run pauses.
- Masks: per input, masks it brought and the stencil drawn on it; the
  segmentation settings, split into those that make training masks and those
  that make `feature_masks/`.
- Reconstruction: per *camera folder* under `images/` (`cam0`, `cam1`, ...),
  the lens and focal it gets; rigs and sequences as their member folders; per
  input, a sensor switched off on its row (`imu` / `attitude` / `gps`, listed
  only when off, so a record from before the switches compares equal); then
  the engine's own settings. Folders, not input rows, because a video's one
  row and the same frames dropped back as a folder with two sub-folders are
  different rows over the same cameras.
- Depth and normals: the model and its options. The kinds of map asked for are
  not fields: asking for depth beside existing normals adds depth, it does not
  redo normals.

A workspace written before the record carries `.spirula-frames` and
`.spirula-recon`. The first can restore the old extraction settings, but is
not proof of source content or capture timing: external inputs require an
explicit frame rebuild rather than silently adopting those frames. Its
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
| no record | **redo, ask**¹ | reuse | reuse | check maps, add what is missing² |
| made from an older output of the step before | -- | redo | redo | redo |
| record incomplete, same fields | finish | finish | finish | finish |
| fields differ | **redo, ask** | redo | **redo, ask** | redo |
| same fields | reuse | reuse | reuse | reuse, or add kinds |

¹ External frame sources cannot be kept without a matching source record.
Dataset images and photographs read in place are reused by the first row.
² Geometry reuse also requires each map's completion marker, below.

"Ask" is the confirmation before the run: frames re-extracted or a
reconstruction rebuilt that nobody asked to redo. Its other answer -- keep
them, run the rest -- is `PlanRequest::keep_built`, also a checkbox in the
list, which turns both into **keep** until the output folder or the inputs
change.

Existing video frames without capture timing cannot be kept. Both runners
require `PlanRequest::redo_frames` before writing any step record or replacing
those frames, independently of `keep_built`. The confirmation's **Redo them**
sets that consent; **Keep them, run the rest** is disabled. A batch row has no
frame-redo consent and fails before updating the dataset record. Rebuild on
the dataset screen first, or use a fresh output folder for the batch.

### Frame source identity

An unchanged path is not proof of an unchanged capture. The panel compares a
cheap inventory (relative image names, sizes and modification times for a
photo folder; file metadata for a video and its file-backed LUT). Before
planning a run, the runners also hash the source bytes with SHA-256. This
detects a video, photo or LUT replaced in place even when its size and
timestamp were preserved, as well as photographs added, removed or renamed.
These identities are stored with the frames step.

A changed source inventory or content requires frame extraction again:
**Keep them, run the rest** cannot override it. An older record without the
identities also needs a rebuild for external sources. Confirm **Redo them**
on the dataset screen, or use a fresh workspace; a batch row cannot grant
that consent. Rebuilding frames also invalidates the downstream masks,
reconstruction and geometry. The panel's metadata preview can miss a
same-size, same-timestamp replacement, but the runner's content check cannot.

Sources are checked again before frame completion is recorded. For **Move**
photo imports, that check happens immediately after transfer, using the
original inventory names and each file's current source or destination
location, before blur sorting can move the only copy into `images_skipped/`.
The intentionally emptied source is not rehashed at final completion.
A detected source change fails the step rather than publishing completion;
rerun preparation. Unreadable sources fail identification. Dataset images
and photographs read in place are exempt from these external-source checks:
they are already the frames, not a capture to re-extract.

A laser scan's photographs are inputs only once the run has extracted them
from the E57 (`add_scan_photo_inputs`), after the first snapshot, so they are
identified right after that. For the same reason no step records them as
inputs: the panel plans before they exist. The masks step records a scan's
drawn areas under the scan (`scan_stencil`), and the reconstruction records
which scans bring photographs (`scan_photos`); everything else about them --
their lens, their order, their folders -- follows from the scan file.


### Photographs the blur rule left out

Gathering photos copies whatever is not in `images/` yet, which would bring
back the ones the blur rule deleted from it. So the run that sorts them
writes their names to `.spirula-skipped`, and a run that keeps the frames
(`PrepJob::make_frames` false) takes those out again after gathering and
judges nothing: the frame set a kept reconstruction was made from stays the
frame set. A run that makes the frames again judges every photo afresh and
rewrites the list.

### Sorting frames that are already there

Frames made with the blur rule off are every frame the extraction chose, and
the reconstruction has a camera for each. Turning the rule on afterwards, or
moving its threshold, therefore makes nothing again: the frames step stays
"reuse" with `StepPlan::sort` set, `DatasetPrep::sort_frames` measures the
images where they lie and moves the ones left out to `images_skipped/`, and
the trainer skips a camera whose image is not in `images/`. The frames record
keeps the fields the frames were made with; what they are sorted by is the
first line of `.spirula-skipped`, which the plan compares with the rule on
screen. Turning the rule off moves everything back.

Frames made WITH the rule work the same way as long as the rule on screen is
no looser than the one they were made with: it can then only leave out more
of the frames the dataset has. A looser one wants frames that were never
reconstructed, so that still makes frames and reconstruction again. The two
kinds of left-out frame share `images_skipped/` and are told apart by the
list: `- path` never had a camera and stays aside, a bare path was moved
aside by a sort and comes back with the next one. Frames of several files --
a video's lens folders, or photographs whose camera folders hold the same file
names -- are judged by their worst lens and move together; folders that do
not match (`day1/`, `day2/`) are one sequence. A dataset added as an input, or
photos read in place, are never sorted: nothing of the user's is moved.

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

### The reconstruction's stages

With the built-in engine, a reconstruction that is made again is not made
from nothing. `spirula sfm` keeps `features/` and `matches.bin` while the
settings that made them still read the same -- its own signatures in
`.resume/`, compared before it trusts either -- re-extracts an image whose
mask file is newer than its features, and always maps again; `spirula lidar`
keeps an alignment made from the same scans. The list shows this as one line
per stage under the reconstruction (`DatasetPlan::parts`). The plan cannot
compute those signatures without running the tool's own setup, so it
predicts them: each model field is filed under the first stage whose
signature carries the flag it becomes (`field_part()`), and the signature
files' presence says whether there is anything to reuse at all. "Reconstruct
again" on unchanged settings therefore reads as extraction and matching
reused, mapping redone -- minutes, where the line above it says the hour.

Each stage's line carries a checkbox: ticked, it runs. Unticking one the plan
would redo keeps what is on disk (`PartChoice::Keep`, `spirula sfm
--reuse-features|--reuse-matches keep`, or no `spirula lidar` at all), and
ticking one it would reuse makes it again (`redo`, `--overwrite`). A kept
stage is recorded as made with the current settings -- its signature is
stored again, and a kept feature file older than its mask is dated now -- so
the next run reuses it without being told. What cannot come out right is
refused by `StepPlan::lock`, not left to the user: a stage after one that
runs, mapping on its own, feature points across new frames or another
frontend (its matcher cannot read them), and matches across a lens change,
because `matches.bin` carries the camera setup verification used and keeping
it would leave the new lens unused. `spirula sfm` refuses on its own to keep
matches over a feature file the same run rewrote.

### Depth and normal map completion

The geometry tool does not trust a nonempty output file by itself. Each map
has a sibling `<map filename>.geometry-complete` record, published only after
the encoded map is fully written and closed. Reuse requires a recognized
record matching the current request and the map's SHA-256 checksum. The
request includes the input image content, camera and face plan, model and
inference options, image colour/exposure settings, and map kind and encoding.
Depth and normals are checked independently, so a completed normal map does
not hide a missing depth map or an interrupted sibling write.

Missing or empty maps and recognized records with a stale request or damaged
output are regenerated when the geometry tool runs. A nonempty map with no
recognized completion record is different: it is preserved, and the tool
stops with an instruction to use **`spirula geometry <dataset> --overwrite`**
to replace it. This includes legacy maps and maps whose marker was removed.
The GUI's geometry overwrite option supplies the same consent. Merely
finding `depths/` or `normals/` is not proof that every face was processed or
that the encoder completed. Keep each marker with its map when moving a
dataset; removing it does not request a safe automatic repair.

The dataset plan still makes its step-level decision from the workspace
record. Per-map marker and checksum validation happens when the geometry
tool runs, not while the panel previews a fully reused geometry step.


## Restoring onto the dataset's own images/

Dropping `<dataset>/images` gives the panel one photo folder with a
sub-folder per camera. `restore_record_inputs()` gives each sub-folder the
lens, focal, rig and order the reconstruction's fields recorded for that
folder, so the reconstruction's fields come out the same and it is reused.
A folder marked Sequential whose camera folders nest (`a/cam0`, `a/cam1`, `b`)
is one sequence per top-level folder (`input_sequences()`), which is what a
dataset made from several videos is. A video row restored from a record made
before its Sequential switch existed comes back on when the model's fields
hold a sequence under its folder: every video went in order then. What cannot be carried over is noted
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
