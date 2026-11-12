# Placing a model: frames, conventions and what gets saved

Phase 4 of [gui-editing-plan.md](gui-editing-plan.md): moving, turning and
resizing a whole model in the editor, and the helpers that find the frame a
scene wants (its ground, a corner, one-click auto align). The scene is taken to
be RIGID: one similarity -- a rotation, a uniform scale, a translation -- for
everything in the document, cameras included. `src/core/Similarity.h` is that
object (`Sim3`, double throughout: a geo-referenced model sits millions of
units from its origin).

## The decision that shapes everything: the viewer applies it

A placement is NOT written into the elements while it is being edited. The
document holds one `Sim3` (`EditDoc::placement()`), the viewport applies it by
moving the CAMERA the other way (`ViewportPanel::set_edit_transform`, the same
trick `set_model_transform` already played for the comparison view), and only
a save bakes it into the file.

What that buys:

- A drag costs nothing. No re-upload of a million Gaussians per frame, no
  second copy of the model, and the view-dependent colour is right for free --
  rendering a model from a moved camera IS the rotated model, SH and all.
- Undo is two `Sim3`s (`make_placement_op` stores both ends rather than the
  step between them, so walking the history never accumulates rounding).
- Every selection tool keeps working unchanged: the elements never left the
  frame they were loaded in, and `view_camera()` already reports the camera in
  that frame.

What it costs is that there are now two frames on screen, which is the rest of
this note.

## The frames

| frame | what is in it | who defines it |
|---|---|---|
| file | the elements as the file stores them | the file |
| positions() | the same, normalized for navigation | `EditDoc::view_frame()` = N |
| shared | what the camera navigates | the panel: `base` x `placement` x positions |
| saved | the file's coordinates AFTER the placement | what a save writes |

With `E` the placement (a similarity of the positions() frame) and `B` the
panel's base transform (owner placement and levelling):

    shared = B E p                 a model point on screen
    saved  = N^-1 E p              where that point will be in the file
    shared = (B N) saved           so B N maps saved coordinates to the screen

`B N` does not contain `E`. That is the whole point: the grid, the axes, the
pivot called "origin", the numbers in the Placement fields and everything the
alignment helpers compute live in SAVED coordinates, which stand still on
screen while the model moves through them.

Two kinds of step, and how each becomes a new placement:

    a step D made in the shared frame (the modal operator, a handle drag):
        E' = B^-1 D B E
    a step D made in saved coordinates (every alignment helper, the fields):
        E' = N D N^-1 E

and at save time the file gets `N^-1 E N` (`EditDoc::file_placement()`).

## The grid has to stand still

The renderers draw their grid in the MODEL's frame -- the engine ray-traces it
as capsules inside the scene, the GL preview builds it from `_t2n`. That is
right until the model is what is moving, and then it is exactly wrong: the
grid would ride along with the thing being aligned to it.

So once a document has been moved (or an operator is running) the session
takes the grid over (`ViewportInteractor::draws_world_grid`): the renderers
are told not to draw theirs, and `edit/WorldGrid.cpp` draws one as an overlay
in saved coordinates through `B N`. It is not depth-tested against the model.
Giving the engine's grid a transform was the alternative; it is a kernel-level
change on both backends plus a BVH rebuild per dragged frame, for a line
overlay.

## The view follows an alignment

An alignment defines the WORLD under the model; it is not the model being
carried somewhere. Done naively, "click the ground" on a wall you are facing
lays the wall flat five units below the camera and the model vanishes from the
screen -- which was the first thing the first test did. So the alignment steps
(`place_saved(..., carry=true)`) take the view along: the camera's position
and pivot go through the same step and the view is then stood upright again
(`ViewportPanel::carry_view`). What the user sees is the model staying put and
the grid arriving under it. Undo and redo across such a step carry the view
back the same way (`EditOp::carries_view`, `EditSession::follow_history`).
Manual moves, turns and quarter turns do not: there the model is what is meant
to be seen moving.

## The modal operator

`edit/TransformTool.{h,cpp}`. The grammar is Blender's, because that is what
the people asking for this already have in their hands:

    G / R / S         move / rotate / scale, following the pointer
    X / Y / Z         constrain to that axis; again: the model's own axis; again: free
    Shift+X / Y / Z   (move) constrain to the plane across that axis
    digits . -        type the value: file units, degrees, or a factor
    Shift             precision: the pointer counts a tenth from here on
    Ctrl              snap: one grid cell, 5 degrees, 0.1 (a tenth of each with Shift)
    Enter / click     confirm            Esc / right-click    cancel

The on-screen handles are the same operator started with its constraint already
chosen and confirmed by letting go -- one code path, two ways in. Scale is
uniform only, by construction.

Three details that are easy to get wrong:

- **Axis-constrained move is measured on screen.** The pointer's travel is
  projected onto the axis AS DRAWN (pixels per unit along it at the pivot).
  Intersecting the pointer ray with the axis line is the textbook answer and it
  blows up as the axis turns toward the eye.
- **Precision mode accumulates.** Shift slows the pointer tenfold from where
  it is, so the operator follows a virtual pointer, not the real one; a
  rotation likewise accumulates its angle so a drag can pass 180 degrees.
- **`S` is also "fly backwards".** Under the Navigate tool WASDQE belong to the
  camera, so there `S` does not start a scale (`G` and `R` still work). The
  Transform tool takes the letter keys, and pressing `G`/`R` enters it.

## Orthographic projection

The viewport offers Orthographic first, with Perspective still the default.
The side widget and numpad 5 toggle back to the previous camera model; choosing
Orthographic directly uses Perspective as the return model. Axis snaps enter
orthographic temporarily and orbiting returns to the previous lens.

Camera model 4 preserves the IDs of existing saved lenses. Both compute backends
project Gaussian means and covariances linearly in the forward path; the GL
preview uses the same projection for meshes, points, grids and lines. The camera
stays at the navigated position and geometry behind it is culled. Depth is axial, and picking uses
parallel rays with per-pixel origins.

Like the WebGL viewer, the orthographic scale is the perspective focal length
divided by orbit distance. The native GUI retains its horizontal FOV convention.
Focal values are pixels per world unit, so they scale with the model-to-navigation
similarity. Saved views with the legacy orthographic flag still load.
Orthographic is a compile-time camera variant in the CUDA forward kernels.
Its instantiations live in separate files to preserve existing build caches;
backward and fused-training variants remain unchanged.

## Saving: what each format needs

**Splats** (`checkpoint/SplatTransform.h`): `mean -> s R mean + t`,
`quat -> q_R * quat`, `log scale -> + ln s`, SH bands rotated
([sh-rotation.md](sh-rotation.md)); opacity and DC untouched.
`write_splat_ply` applies it row by row as it writes, so baking costs no second
copy of the model.

**Meshes**: vertices through the similarity, normals through the rotation. A
linked save moves the sibling files too -- faces are matched in the coordinates
the file was written in, then moved.

**COLMAP** (`data/SparseEdit.cpp`, `move_w2c`). A pose is world-to-camera,
`x_cam = R x + t`, and a camera is rigid: it cannot carry the scene's scale.
With the world moved by `x' = s Q x + u`:

    R' = R Q^T          t' = s t - R' u

The camera frame grows by `s` and nothing about any image changes. Points are
moved directly. `images.bin`/`.txt`, `points3D.bin`/`.txt` are handled, and
`frames.bin` (COLMAP 3.12+, where `rig_from_world` is what COLMAP itself reads)
is moved too when its layout accounts for every byte of the file -- a file this
cannot read exactly is one it must not rewrite. A multi-sensor rig's internal
baselines (`rigs.bin`) are NOT rescaled; with `s = 1` there is nothing to do.

**Nerfstudio**. The parser hands out poses with `applied_transform` UNDONE
(`p_raw = A^-1 (p_json - b)`), so a placement `T` made in that raw frame is the
conjugate `A T A^-1` in the frame the file is written in. The frames'
`transform_matrix` (camera-to-world, OpenGL axes) and the point PLY are both
moved by that conjugate; the camera axes are only ROTATED, because a
transform_matrix whose columns stopped being unit would be a lens. The file's
`applied_transform` is left exactly as it was: what the parser undoes is then
still the axis convention and nothing else, and both this trainer and
nerfstudio see the same scene, moved.

**Metashape** is not ours to rewrite, so as before the edit lands beside it as
a Nerfstudio dataset, moved the same way.

`src/data/tests/sparse_transform_test.cpp` writes each format, moves it,
re-parses it and requires the parser's cameras and points to be the originals
under the same similarity AND every point to land on the pixel it did before
(measured: 5e-5 px).

### Opening it again

The dataset preview levels a reconstruction by a GUESS at its up axis
(`ViewportPanel::adopt_gauge`). On a model somebody has placed by hand that
guess is a second, unasked-for rotation on top of theirs, so the parsers report
`ParsedDataset::edited_in_place` -- a `.orig` beside any file an edit replaces
-- and the preview then starts with auto-level off, as it already did for a
model whose `gauge.txt` says its orientation was measured.

### The session baseline

A row filter indexes the rows of the file it reads, and a placement starts from
the poses in it. A SECOND save of one session therefore cannot read the file
the first save wrote -- it would filter already-filtered rows by the original
indices and move already-moved poses. `SparseBaseline` holds the files as the
session first found them and every save starts from it. (The double filter was
a latent bug in phase 1; the transform made it impossible to miss.)

## Finding the frame a scene wants

`core/SceneAlign.{h,cpp}`: pure geometry over a point array, tested without a
window (`app/gui/tests/align_fit_test.cpp`, a tipped-over room with a box in
it, noise and floaters). `spirula sfm` levels every model it writes with the
same auto align (`sfm/map/Orient.h`).

- **Auto align.** Up to six planes by sequential RANSAC, each refit by least
  squares. The ground is the best-supported plane with the scene ON it (which
  is what tells a floor from a ceiling and, more often, from the biggest wall).
  With cameras, their mean up axis is a trusted prior and the ground must face
  it; WITHOUT cameras the file's +Z is only a soft hint and up is whichever side
  of the plane the scene is on -- a splat file that arrived lying on its side
  (the Mip-NeRF 360 bicycle is 104 degrees off) has to work. Then the walls:
  everything upright votes for a heading with its angle multiplied by four, so
  the four faces of a room agree; from normals where the document has them (a
  flat Gaussian's thin axis, a mesh vertex normal), from the RANSAC planes
  where it has not. Then the median of the footprint goes to the origin.
- **Click the ground.** The plane through the click with the smallest
  35th-percentile residual (least quantile of squares), then grown outward
  while the wider patch keeps agreeing. No tolerance to choose -- a lawn is
  centimetres thick and a tabletop is not, and a click cannot say which -- and
  a wall beside the click can hold most of the neighbourhood without winning,
  because it does not pass through the click. Up is the side the eye is on.
- **Click a corner.** Up to three mutually square planes near the click, made
  exactly orthogonal, each sent to the axis it is already nearest so the model
  turns as little as it can.
- **Haze is not a surface.** For splats the fits skip faint (opacity < 0.3) and
  oversized Gaussians (`EditDoc::solidity`), and the pick walks the pixel's ray
  front to back until half the light is gone -- the renderer's own answer to
  "what is here" -- instead of taking the nearest centre, which in a trained
  scene is a floater.
