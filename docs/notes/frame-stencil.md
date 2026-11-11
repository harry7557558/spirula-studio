# Fixed areas of the frame: the stencil, its tools and its file

"Remove fixed areas of the frame" on the dataset screen is `app::FrameStencil`:
per input, a fitted fisheye border plus shapes drawn by hand, optionally
different for each of the input's cameras. Both are geometry, not segmentation,
so they need no model and are the same on every frame of a camera. This note
covers the drawn half: what a shape is, the tools that draw one, per-camera
areas, the SVG file a set of them is saved as, the per-input table on the
dataset screen, and how a saved set reaches a dataset preset and a batch run.

## Shapes

`app::MaskShape` (`src/app/FrameMask.h`), all in normalized image
coordinates, (0,0) the top-left corner and (1,1) the bottom-right, x by width
and y by height:

| kind | fields | drawn by |
|---|---|---|
| `Rect` | two corners in `cx,cy` / `rx,ry` | Box |
| `Ellipse` | centre `cx,cy`, radii `rx,ry` | Ellipse |
| `Path` | `pts`: 3+ corners, closed, even-odd | Lasso, Path (livewire) |
| `Stroke` | `pts`: 1+ points; half-width `rx,ry` per axis | Brush, Eraser |
| `Bezier` | `pts`: 2+ anchors of six floats (in-handle, point, out-handle), closed, even-odd | Pen |

A `Bezier`'s segment i runs from anchor i's point through its out-handle and
anchor i+1's in-handle to that anchor's point, the last back to the first; a
corner has both handles on its point. The math (flattening by Wang's bound,
splitting, nearest point, the smooth-handle rule) is `core/CubicBezier.h`,
shared by the rasterizer, the pen tool and the editor.

Each shape either removes what is inside it or keeps it. Shapes apply **in
order**, the last one covering a pixel decides it, and the base is "keep"
unless the first shape keeps (then it defines the region). That rule is
`rasterize_frame_mask`, and it is the only rasterizer: the run, the CLI and the
panel's red overlay all call it.

A stroke's half-width is stored per axis so a brush that is round in pixels
stays round on a non-square frame: `rx = R / width`, `ry = R / height`.

The automatic fisheye circle is separate from this list. `SegmentPanel::resolved`
adds it at draw and run time. Its shrink slider ranges from -10% to 30% of the
detected radius; Ctrl+click allows numeric entry within that range. Preview and
batch processing both apply `app::shrink_border`.

**Edit border ellipse** converts the current adjusted circle into the first
keep-shape and turns off automatic detection; it is one undo step. The mask is
unchanged by conversion; Select can then move its centre and resize its two
axes. On an input with several cameras it first turns on **Separate areas for
each camera** (below), so the ellipse is the shown camera's alone and the other
lenses keep their own fitted borders. Automatic detection must receive
`shrink=0` before conversion; the stencil's shrink is applied once when the
ellipse is inserted.

## Tools

`SegmentPanel` (Try the mask...) has the mask editor's tool row over the
picture: Select `V`, Points `A`, Box `B`, Ellipse `E`, Lasso `L`, Pen `P`,
Brush `C`, Eraser `X`, Path `I`, and Add / Subtract. A plain stroke adds to
what is removed; Subtract, the eraser and Ctrl each flip that to keep, so Ctrl
with the eraser removes again, as in the mask editor. Undo and redo (Ctrl+Z,
Ctrl+Shift+Z, Ctrl+Y) cover every change to the stencil, including moves,
flips, loads, the border conversion and the per-camera switch. Under Select,
clicks on the picture still prompt the model; pick a shape in the list to move
or resize it.

The canvas navigates as the mask editor's does (`mask::View`): wheel zooms,
Alt+wheel sizes the brush, middle drag or Space+drag pans. The drawing tools
work on the empty canvas around the picture too, so a shape can run past the
frame's edge; its coordinates then fall outside 0..1, which the rasterizer and
the SVG file take as they are. Model clicks stay on the picture. The red
overlay is rasterized for the part of the frame on screen only, so it stays
sharp when zoomed in.

The tools are the 3D editor's `EditTool` producing a `ShapeStroke` in canvas
pixels; `stencil_shape_from_stroke` (`src/app/gui/StencilEdit.h`) turns that
into a `MaskShape`. That file also holds hit testing, moving, resize handles,
a pen shape's points and the undo history, with no ImGui, and
`stencil_edit_test` covers it.

### The pen

`P`, with the grammar Illustrator, Photoshop, Figma and Affinity share, so the
Polygon tool is gone from this row (a pen that is only clicked is a polygon):

| input | does |
|---|---|
| click | a corner anchor |
| drag | a smooth anchor: the out-handle follows the pointer, the in-handle mirrors it |
| Shift | the new anchor, or the handle, snaps to 45-degree steps |
| Alt while dragging | the in-handle stays put; only the out-handle moves (a cusp) |
| Space while dragging | moves the anchor being placed; the handles resume from there |
| click the last anchor | takes its out-handle back, so the next segment leaves straight |
| Ctrl+drag | moves an anchor or handle already placed (the "direct selection for now") |
| click the first anchor, Enter, right click | closes the path; a drag on the first anchor also sets its handles |
| Ctrl+Z, Backspace | take the last anchor back |
| Esc | cancels the path |

A badge beside the pointer shows a ring (close), a caret (straighten) or an
arrow (edit), where a vector editor would change the cursor. With a pen shape
selected, the pen also edits it: a click on its outline adds an anchor there
(the curve does not change), a click on an anchor deletes it, and Alt+click on
an anchor makes it a corner, or Alt+drag pulls new symmetric handles out of it.

`PenTool` (`src/app/gui/mask/PenTool.h`) is the same class in the mask editor,
where the closed path is flattened and painted. Its points are kept in frame
pixels through `PathSpace`, as the livewire path's are.

**Points** (`A`, direct selection) shows a pen shape's anchors as squares and
handles as circles. Drag either; a handle of a smooth anchor swings the other
to stay opposite at its own length, and Alt+drag moves it alone. Shift keeps
45-degree steps. Delete or Backspace removes the picked anchor (never below
two). A click inside another shape picks it; other kinds move and resize as
under Select.

## The file: SVG in normalized coordinates

`app/FrameMaskSvg.h` writes and reads a shape list as SVG:

```xml
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1 1" width="1024" height="1024"
     preserveAspectRatio="none">
  <title>Selfie stick</title>
  <rect data-role="base" x="0" y="0" width="1" height="1" fill="#fff"/>
  <rect data-op="remove" x="0" y="0.9" width="1" height="0.1" fill="#000"/>
  <path data-op="remove" d="M0.1 0.1 L0.4 0.1 L0.25 0.4 Z" fill="#000" fill-rule="evenodd"/>
  <polyline data-op="keep" data-rx="0.01" data-ry="0.0178" points="0.6,0.1 0.9,0.3"
            fill="none" stroke="#fff" stroke-width="0.0267"
            stroke-linecap="round" stroke-linejoin="round"/>
</svg>
```

- The base rect is the rasterizer's base, so a browser shows the mask the file
  makes: black is removed, white kept. The reader skips it.
- `data-op` is authoritative. Without it, the paint decides: dark removes,
  light keeps, and SVG's default fill (black) removes.
- `data-rx` / `data-ry` appear only on a stroke whose half-widths differ per
  axis. A viewer draws `stroke-width`, their geometric mean doubled, which is
  as close as one SVG width can get.
- `<title>` is the name the set is listed under.
- `data-camera` on the root, when present, says the file is one camera's
  part of a per-camera set (below).

A `Bezier` is a `<path>` of `C` commands, one per segment, straight ones
included, ending on the first anchor; the reader folds that last anchor back
into the first, so a file round-trips to the same anchors.

The reader also takes hand-made SVG: `rect`, `circle`, `ellipse`, `polygon`,
`polyline`, `line` and `path` (all commands), `style=""` and presentation
attributes, inherited through `<g>`, and any `viewBox` (pixel coordinates are
fine). A filled subpath with a curve (`C`, `S`, `Q`, `T`; quadratics raised to
cubics) and no arc becomes a `Bezier`, so a shape drawn in Inkscape or
Illustrator stays editable with Points; one with an arc is flattened to 24
segments per curve. A path's subpaths become one shape each, so a hole is a
later keep shape rather than an even-odd subpath. `transform`, `<use>` and
`<image>` are refused by name rather than misplaced.

## Per camera

One input can write several cameras: the two lenses of a dual-fisheye `.insv`
or `.osv` (`cam0/`, `cam1/`), a folder of `.insp` photos cut in two, the views
a GoPro `.360` is unwrapped into (ten `cam0/`..`cam9/` faces by default, the
two EAC tracks with 360 unwrapping off, one image for equirectangular), or a
folder of photos in subfolders. By default the shapes are the input's and every
camera gets them, and the border is fitted per camera.

**Separate areas for each camera**, under the shapes, gives each camera its own
`app::CameraStencil`: border on/off, shrink and shapes. Turning it on copies
the input's stencil to every camera the panel knows, so nothing changes until
you draw; the **Camera** picker then chooses which one the tools edit. Turning
it off keeps the shown camera's as the input's. `FrameStencil::cameras` is
keyed by the folder the frames land in, exactly as `group_frames_by_camera`
and DatasetPrep's `StencilRaster` key them, and a camera it does not list uses
the input's. The panel keys a packed photo's lens as `cam<k>` plus the photo's
own subfolder, which is where the split writes it.

### Per-camera sets

A saved set (`app::MaskSet`) is one shape list for every camera, or one list
per camera. On disk a per-camera set is one SVG per camera, all with the same
`<title>`, each naming its camera in `data-camera`; a camera with nothing drawn
has no file. `app::mask_set_of` decides which an input is: one list when it has
no separate areas or when every camera draws the same, per camera otherwise.
`app::apply_mask_set` puts a set back, each named camera its own list and every
other camera the set's shared one. The fitted border is never in a set, but a
list that starts with a keep shape (an edited border ellipse, say) is the kept
region already, and the fit would only widen it, since keep shapes add up; so
loading one turns that camera's fit off. Every other border setting stays.

`app::load_mask_svg_set` reads a file as its set: a file naming no camera on
its own, a file naming one together with every file beside it that has its
title and names a camera. So any one file of a set, picked anywhere a file is
picked, loads the whole set. A camera name is a folder key (`cam1`, `cam0/sub`
for a packed photo), and a set only means something on an input whose cameras
are keyed the same way.

## Where a saved set goes

- **Saved**: `<config>/presets/stencil/<name>.svg`, or `<name>-<camera>.svg`
  per camera, from **Save...** in the panel (`StencilPreset.h`). With separate
  areas on it saves the whole input's set, not the shown camera's. Saving a
  name again removes every file of the old set first. **Load...** puts a
  one-list set where the tools draw (the input's, or the shown camera's with
  separate areas on); a per-camera set turns separate areas on and goes on
  the cameras it names, and is refused, with its camera names, on an input
  that has none of them. The list shows a per-camera set once, as
  `Name [cam0, cam1]`.
- **Any row of the table** (below): its **Drawn areas** combo lists every
  saved set, a per-camera set only on an input with one of its cameras, and
  puts it on that row and the rows following it, each its camera's list. A
  lens the set does not name gets its first camera's list, so a set drawn on
  `cam0` alone, the usual way, reaches a following `cam1` too. A dataset
  preset names a set only when `apply_mask_set` would draw what the rows
  draw, so not that one.
- **Presets and batch**: when every row draws one saved set (each row its
  camera's list), its name is `mask_frame_shapes` in a dataset preset
  (`GuiApp::refresh_frame_shapes`, `common_set`); otherwise the preset keeps
  none. Applying a preset with a name draws that set on every input, and so
  does adding inputs while one is named. A batch row whose preset has the
  fixed-areas option on gets the border fit and that set on every input; a
  name that no longer resolves fails the row.
- **With the dataset**: a run writes each input's set to
  `<dataset>/frame_stencil/`: `<input>.svg`, or `<input>-<camera>.svg` per
  camera when its cameras differ, titled `<dataset> - <input file>`. Two
  inputs of one name get `<input>-2` and a `(2)` title, so their camera files
  never load back as one set. The folder holds only the latest run's, so they
  survive a session nobody saved from. **Load...** lists them under "In this
  dataset", and **Other file...** loads any SVG, with its set.
- **Changing a saved set**: right-clicking one in **Load...** or a row's
  **Drawn areas** combo offers Edit, Rename and Delete (`StencilSetMenu.h`).
  Edit loads it into the panel, on that row's input and camera, and **Save
  changes** writes it back into the folder it came from; it keeps a one-list
  set one list even with separate areas on. Rename changes the title and the
  files, and refuses a name in use or one that would write over another set's
  file (names differing only in case or punctuation share one). Delete asks
  first, and takes every camera's file of the entry and nothing else of its
  name. An input already drawing that set keeps its shapes; a preset or batch
  row naming it by the old name no longer resolves.
- **CLI**: `spirula sam mask --shape <file>.svg` reads the same files; one
  camera's file brings its set, each camera's list for its own folder under
  `--frames`.

## The table on the dataset screen

Under **Remove fixed areas of the frame**, while it is ticked, the dataset
screen lists a row per input, or per camera of an input with several
(`stencil_cameras`: its camera folders, a dual-fisheye file's lenses, and any
camera its stencil already names; a 360 file's views share one row until the
panel splits them). Each row has the border fit (Cut away / Off, with the
shrink slider) and its drawn areas (None, a saved set, or Draw..., which opens
Try the mask on that input). The logic is `app/gui/StencilRows.h`, with no
ImGui; `stencil_rows_test` covers it.

Each row follows a row above it. An input's first row follows the same
camera of the input directly above when that input has one, else the row
before it; its other rows follow the row before them. So the two lenses of a
dual-fisheye file share by default, as they usually do (the same pole, the
same operator), each file's `cam0` follows the `cam0` above, and nothing
reaches past the input above: a GoPro sphere under a dual-fisheye file follows
that file's last lens, not a sphere further up.

Whether each part of a row follows is stored, per camera, in
`PrepInput::stencil_follow`. A row seen for the first time follows each part
that matches the row above (`settle_follow`), which is what makes a capture
shot on one camera read "(same as above)" all the way down -- its drawn areas
only when the border fit is alike too, since a row whose fit differs is
another kind of lens (that sphere again), whose areas would mean nothing on
the one above. From then on it
is the user's choice: picking anything other than "(same as above)" makes the
part the row's own even when it equals the row above -- a row set to None
under a row drawing nothing stays None and ignores what is drawn above it
later -- and picking "(same as above)" copies the row followed and follows it
again. A following row that stops matching keeps its value as its own: the
panel drew on it, or an input was removed and it now follows another row.
Comparing values instead, as the lens column does, turned every explicit
choice that happened to equal the row above back into "(same as above)".

Changing a part changes it on every row following it, through any number of
rows (`set_row_border`, `set_row_areas`). The border and the areas follow and
are set separately: drawn areas never turn the border fit on or off, even a
list starting with a keep shape, which the panel's Load does (above). A border
that is off matches any other that is off whatever its shrink, which it keeps
for when it is turned back on.

Rows write `PrepInput::stencil`, so the run, the plan and the saved
`frame_stencil/` files see nothing new. An input whose rows all agree is one
stencil; one whose rows differ gets an entry per camera, as Separate areas for
each camera would.

Edits in Try the mask are carried the same way: the screen snapshots the rows
before the panel draws and, after, passes each changed row of that input to
the rows of other inputs that follow it (`follow_edit`). The input's own
other rows stay as the panel left them, so drawing on `cam0` with separate
areas on does not reach `cam1`. An input added later takes the drawn areas
and the shrink of the row it follows when both are, or both are not, fisheye
files (`adopt_rows_above`).

### A laser scan's photographs

An E57 that carries photographs, while they join the reconstruction (not
"All cameras are scan-native"), gets rows after the inputs: one, or one per
camera folder once Try the mask splits it (`pinhole` and `panorama` when it
has both). They are a chain of their own, whose first row follows nothing,
so a scan's photographs never take a video's areas (issue #99); the rows edit
`LidarInput::stencil`. A row works on owners rather than inputs
(`StencilOwner`: a stencil and its "same as above" flags) for exactly this.

**Draw...** on a scan row extracts its photographs where a run puts them,
`<workspace>/lidar/photos/<scan folder>/` (`extract_scan_photos`, on a
worker, which counts as native work so a run cannot start over it; the
anchors go to a file of their own), then opens Try the mask on that folder.
A run reuses the files, and a clicked prompt drawn there names the folder
the run reads, unless the scan has both kinds and is split. The input picker
beside the Try the mask button lists these scans too (`_mask_try_scan`).

An XGRIDS capture holds about 1300 frames of 4000x3000 in 110 s, so Draw...
on one of its rows decodes only a spread of the frames the run keeps of that
row's camera: 24, the number the border fit reads
(`BorderDetectOptions::samples`). The panel shows one camera at a time,
because each lens has its own circle: the slider and the border fit are all
that camera's, and the input picker beside Try the mask lists each camera.
The frames go to a folder in the system's temporary directory under the run's
names, never into the dataset. A camera already decoded stays there while the
panel shows the capture, and the GUI removes the folder when the panel closes
or shows another input (`GuiApp::_scan_scratch`).

The run hands each scan's stencil to the inputs its photographs become
(`add_scan_photo_inputs`; a split scan gives each folder its camera's), which
are marked with `PrepInput::scan`. `masks_fields` records the areas once,
under the scan (`scan_stencil`), from `PrepJob::scan_stencils`, not per
photo input: the plan before the run has no input for the photographs yet,
and a field only the run sees would make every later plan redo the masks.
The dataset's `frame_stencil/` keeps them as `<scan file>.svg`.

A lone video keeps `images/` to itself (no subfolder), so the photographs'
folders sit inside its own. Every pass over one input's images skips the
other inputs' folders in it (`nested_inputs`), or the video's segmentation
and stencil would take the photographs first, and theirs would never apply.

## Adding a shape kind

`Bezier` is the worked example. The places a kind touches:

1. `MaskShape::Kind` and its fields.
2. `rasterize_frame_mask`: fill the kind's own plane, as `Path`, `Stroke` and
   `Bezier` do, so the ordering rule is untouched.
3. `parse_mask_shapes` / `format_mask_shapes`, the CLI spelling.
4. `write_mask_svg` and `read_mask_svg`.
5. `StencilEdit`: `stencil_contains`, `stencil_move`, and its handles.
6. `SegmentPanel`: the tool button and key, the outline, and the list label.
7. `frame_mask_test` (raster and SVG round trip) and `stencil_edit_test`.
