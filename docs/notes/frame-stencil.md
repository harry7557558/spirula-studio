# Fixed areas of the frame: the stencil, its tools and its file

"Remove fixed areas of the frame" on the dataset screen is `app::FrameStencil`:
per input, a fitted fisheye border plus shapes drawn by hand, optionally
different for each of the input's cameras. Both are geometry, not segmentation,
so they need no model and are the same on every frame of a camera. This note
covers the drawn half: what a shape is, the tools that draw one, per-camera
areas, the SVG file a set of them is saved as, and how a saved set reaches a
dataset preset and a batch run.

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
- **Every input**: the dataset screen's **Drawn areas** picker, under the
  fixed-areas checkbox, draws a saved set on every input, per camera for a
  per-camera set, including inputs added afterwards. Editing the shapes in the
  panel drops the name, since it no longer describes what is drawn.
- **Presets and batch**: the name is `mask_frame_shapes` in a dataset preset.
  A batch row whose preset has the fixed-areas option on gets the border fit
  and that set on every input; a name that no longer resolves fails the row.
- **With the dataset**: a run writes each input's set to
  `<dataset>/frame_stencil/`: `<input>.svg`, or `<input>-<camera>.svg` per
  camera when its cameras differ, titled `<dataset> - <input file>`. Two
  inputs of one name get `<input>-2` and a `(2)` title, so their camera files
  never load back as one set. The folder holds only the latest run's, so they
  survive a session nobody saved from. **Load...** lists them under "In this
  dataset", and **Other file...** loads any SVG, with its set.
- **CLI**: `spirula sam mask --shape <file>.svg` reads the same files; one
  camera's file brings its set, each camera's list for its own folder under
  `--frames`.

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
