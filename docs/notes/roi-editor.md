# The region-of-interest editor

The trainer could already keep a run to a region of space (`--roi-region`,
`TrainerSession::setup_region`): splats outside draw for growth and relocation
with `--roi-outside-weight`, and with `--roi-mask-pixels` the images are masked
to what they show of the region. The only way to get a region was a
partition's part (docs/notes/scene-partition.md) or a hand-written JSON. The
editor (`app/gui/RoiEditor.h`) is the way to draw one, and the trainer finds
what it saves without being told.

## What a region is to the editor

An ordered list of shapes (`data/RoiDocument.h`), each one **added** to what
the shapes above it made, **cut** from it, or **overlapped** with it (kept only
where both are). The list folds top to bottom into one `CsgRegion`
(`roi_region`); consecutive shapes with the same operation share one n-ary
node, so the device program is a left fold whose evaluation stack stays two
deep however long the list is (the shader's limit is 32). A list that *opens*
with a cut starts from all of space -- "everything but the crowd in the
corner" is one shape.

Four kinds of shape, each in a frame of its own: a **pivot** it moves by and
turns about, three axes, and where its two **sides** sit along each axis,
measured from the pivot (`lo`, `hi`). A box is the box between the sides, an
ellipsoid or a cylinder the one inscribed in it; an outline's x and y sides are
its polygon's bounds, so moving one stretches the corners (`roi_set_extent`).
Keeping the sides rather than a centre and a half size is what lets one side
move -- by a handle or by a typed number -- and the opposite one stay put,
however the shape has been moved and turned; `roi_center_pivot` puts the pivot
back in the middle without moving the shape.

| shape | region | device node |
|---|---|---|
| box | `BoxRegion` | 0 (existing) |
| ellipsoid (a sphere is one with equal radii) | `EllipsoidRegion` | 10 |
| cylinder, elliptic, along its third axis | `CylinderRegion` | 11 |
| outline: a polygon in the frame's first two axes, extruded | `PrismRegion` | 12 + payload |

Union, difference and intersection of these reach any topology a user asks
for -- holes, separate islands, an L-shaped yard, a building minus its
courtyard -- without a mesh, which matters because `MeshRegion` has no device
form. The outline is the shape that carries the "complex" part: any simple or
self-crossing polygon (even-odd rule), drawn in whatever view the user has --
a plan from above, a facade from the side.

The prism's polygon does not fit the fixed six-float4 node, so its vertices
follow it as whole payload nodes (`RegionProgram::push_prism`,
`payload_nodes`), which `region_contains` (both backends) and the host mirror
step over; `apply_similarity` scales them too. `region_parity` evaluates a
program of all four kinds, the star-shaped outline spilling into a second
payload node, on the device against the host.

## The file

`<dataset>/roi/<name>.json`, written by `write_roi_file`. The file *is* a region
JSON -- `region_from_json` reads it unchanged, and so does anything else that
takes `--roi-region` -- with the list beside it under `"editor"` (names, the
disabled shapes, the operations, and each shape's `origin`, `rotation`, `lo`,
`hi` and `polygon` as drawn). Opening a region with no `"editor"`
key imports it when it is a fold of leaf shapes (`roi_from_region`); anything
else (a label field, a half-space, a mesh) is shown as not editable and still
trains as it is. Every coordinate is in the dataset's own frame -- the frame
the reconstruction's files are in, before any centring -- and is written with
`json_number_exact`, since a geo-referenced model sits millions of units out
where `%.9g` rounds to centimetres. For the same reason the trainer moves the
region into the training frame while compiling it (`compile_region`'s
similarity, in double), not after the nodes are float.

## Which region a run uses

`resolve_roi_setting`, called from `TrainerSession::load_region` right after
the dataset is parsed (so the training screen's preview shows the region before
anything trains):

- `--roi-region` unset: the first file in `<dataset>/roi/`, by name ignoring
  case. The run writes the file it picked into its `config.json`
  (`roi/<name>.json`), or `off` when there was none, so a resume does not pick
  up a region drawn afterwards.
- `off`: no region. Not `none`: the command line reads `none` as unset for
  every string flag.
- a bare name: `<dataset>/roi/<name>.json`.
- a path: relative to the dataset when it is there, else the working directory.

A partitioned run trains in the intersection of its part and the region, and
the pixel masks follow the same intersection. `roi_region` is a dataset-parse
field in the GUI (`SS_DATASET_PARSE_FIELDS`), so changing it re-reads the
preview, and a preset context field (`SS_PRESET_CONTEXT_FIELDS`), so no saved
preset carries one dataset's region to another. Opening a different dataset
resets it to automatic.

The training screen's dataset section has a row for it: Automatic (naming the
file that is), each saved region, or None, and "Edit Region". Saving in the
editor re-reads the preview when the training screen holds a parsed dataset
and no run.

## The editor

The window is the Partition panel's layout: controls on the left, the
dataset's point cloud and cameras on the right in the same `ViewportPanel`
preview, with the editor installed as its `ViewportInteractor`.

**Getting started** is four buttons, each adding a shape, and a row with one
button per kind that adds it where the view is looking:

- *Box around the scene*: the 1st..99th percentile of the points along the
  horizontal principal axis and up, padded 5%.
- *Cylinder around the subject*: for a capture that circles something. The
  centre is where the cameras' optical axes come closest (least squares),
  accepted when 80% of cameras face it and they surround it with no azimuth
  gap over 180 degrees; for a 360 rig, whose lenses face every way, the middle
  of the camera circle, accepted with no gap over 135 degrees and at least 15%
  of the points inside (a hand-held forward capture's cameras also surround
  their own middle, with the scene somewhere else). Radius 0.7 of the median
  camera distance; height from the points inside. Mip-NeRF 360 garden takes the
  first branch, a 360-rig orbit of a windmill the second.
- *Ellipsoid around the subject*: the same place and sides, rounded.
- *Outline drawn in this view*: the view turns orthographic, keeping its
  direction, so a straight edge on screen is a straight wall through the
  scene. Each click places a corner on the plane through the orbit pivot
  square to the view (`set_draw_frame`; it follows the camera until the first
  corner pins it); Enter, a right click, a double-click or the first corner
  closes it, and the projection goes back to what it was. The outline runs
  along the line of sight over the 2nd..98th percentile of the depths of the
  points inside the footprint, padded 10%.

The view frames a start shape, since a box around the scene usually swallows
the camera.

**Editing** works the same on every shape, in the levelled frame the viewport
navigates (+Z up), so a box drawn level stays level whatever the
reconstruction's own axes:

- click a shape to select it; drag it to move it along the ground (along the
  view plane when the ground is seen edge-on); a shape the camera stands
  inside is not grabbed by its body, or every click would take it;
- four modes, as buttons and keys. **Adjust** (A, what a new shape opens in)
  is the one for shaping: hovering the selected shape lights the side under
  the pointer -- a box's face, an ellipsoid's pole, a cylinder's wall or cap,
  an outline's wall or end -- and dragging pushes or pulls that side alone
  (`side_under`; Shift moves the opposite side too). An outline's wall moves
  along its own outward normal, carrying its two corners; its corners drag,
  an edge midpoint drags out a new corner, and a corner double-clicked goes.
  The same sides have handles too, for one seen edge-on. A click on another
  shape only selects it here, so a slip does not move it. **Move** (G) drags
  a shape along the ground and adds axis arrows and a pivot handle. **Resize**
  (S) stretches both sides of an axis at once from its side handles, and
  scales the whole shape about its middle from its eight corner handles (Ctrl
  in tenths). **Rotate** (R) shows a ring per axis about the pivot, Ctrl for
  15-degree steps;
- the side panel has the same numbers: the pivot in dataset coordinates, turn
  / tilt / roll in degrees, and a row per axis with both sides and the length
  between them (a new length keeps the middle), plus Level (keeps the turn,
  drops the tilt), Center Pivot, and for an outline its corners one by one;
- Delete, Ctrl+D, Ctrl+Z / Ctrl+Y (an undo step per gesture or committed
  field edit), Esc to cancel a drag or let go of the selection, Ctrl+S.

**Feedback** is the trainer's own: the combined region as the translucent
boundary mesh (`region_boundary_mesh`, 96 cells, rebuilt off the GUI thread
whenever the list changes), the points outside it dimmed, and the share of the
points and cameras inside. Each shape's own wireframe is drawn over the image
in its operation's colour (add green, cut red, overlap amber).

## Not done

- No snapping to the grid while moving, and no numeric entry during a drag;
  the side panel is the precise path.
- No region from the edit screen's selection tools (a lasso over the splats
  becomes a set of points, not a closed shape).
