# Fixed areas of the frame: the stencil, its tools and its file

"Remove fixed areas of the frame" on the dataset screen is `app::FrameStencil`:
per input, a fitted fisheye border plus shapes drawn by hand. Both are geometry,
not segmentation, so they need no model and are the same on every frame of a
camera. This note covers the drawn half: what a shape is, the tools that draw
one, the SVG file a set of them is saved as, and how a saved set reaches a
dataset preset and a batch run.

## Shapes

`app::MaskShape` (`src/app/FrameMask.h`), all in normalized image
coordinates, (0,0) the top-left corner and (1,1) the bottom-right, x by width
and y by height:

| kind | fields | drawn by |
|---|---|---|
| `Rect` | two corners in `cx,cy` / `rx,ry` | Box |
| `Ellipse` | centre `cx,cy`, radii `rx,ry` | Ellipse |
| `Path` | `pts`: 3+ corners, closed, even-odd | Lasso, Polygon, Path (livewire) |
| `Stroke` | `pts`: 1+ points; half-width `rx,ry` per axis | Brush, Eraser |

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
keep-shape, turns off automatic detection, and starts a new shape edit history.
The mask is unchanged by conversion; Select can then move its centre and resize
its two axes. This fixed ellipse is saved with the other shapes and applies to
the whole input, so use separate inputs for cameras with different borders.
Automatic detection must receive `shrink=0` before conversion; the stencil's
shrink is applied once when the ellipse is inserted.

## Tools

`SegmentPanel` (Try the mask...) has the mask editor's tool row over the
picture: Select `V`, Box `B`, Ellipse `E`, Lasso `L`, Polygon `P`, Brush `C`,
Eraser `X`, Path `I`, and Add / Subtract. A plain stroke adds to what is removed;
Subtract, the eraser and Ctrl each flip that to keep, so Ctrl with the eraser
removes again, as in the mask editor. Undo and redo (Ctrl+Z, Ctrl+Shift+Z,
Ctrl+Y) cover every change to the list, including moves, flips and loads.
Under Select, clicks on the picture still prompt the model; pick a shape in the
list to move or resize it.

The tools are the 3D editor's `EditTool` producing a `ShapeStroke` in canvas
pixels; `stencil_shape_from_stroke` (`src/app/gui/StencilEdit.h`) turns that
into a `MaskShape`. That file also holds hit testing, moving, resize handles and
the undo history, with no ImGui, and `stencil_edit_test` covers it.

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

The reader also takes hand-made SVG: `rect`, `circle`, `ellipse`, `polygon`,
`polyline`, `line` and `path` (all commands, curves and arcs flattened to 24
segments), `style=""` and presentation attributes, inherited through `<g>`,
and any `viewBox` (pixel coordinates are fine). A path's subpaths become one
shape each, so a hole is a later keep shape rather than an even-odd subpath.
`transform`, `<use>` and `<image>` are refused by name rather than misplaced.

## Where a saved set goes

- **Saved**: `<config>/presets/stencil/<name>.svg`, from **Save...** in the
  panel (`StencilPreset.h`). **Load...** there replaces the shapes on the input
  the panel is open on.
- **Every input**: the dataset screen's **Drawn areas** picker, under the
  fixed-areas checkbox, draws a saved set on every input, including inputs
  added afterwards. Editing the shapes in the panel drops the name, since it no
  longer describes what is drawn.
- **Presets and batch**: the name is `mask_frame_shapes` in a dataset preset.
  A batch row whose preset has the fixed-areas option on gets the border fit
  and that set on every input; a name that no longer resolves fails the row.
- **With the dataset**: a run writes each input's drawn shapes to
  `<dataset>/frame_stencil/<input>.svg` (the folder holds only the latest
  run's), so they survive a session nobody saved from. **Load...** lists them
  under "In this dataset", and **Other file...** loads any SVG, such as
  another dataset's.
- **CLI**: `spirula sam mask --shape <file>.svg` reads the same file.

## Adding a shape kind (the pen tool)

A pen tool with Bezier handles is the next kind. The places it touches:

1. `MaskShape::Kind` and its fields (e.g. anchors and control points in `pts`).
2. `rasterize_frame_mask`: flatten into the kind's own plane, as `Path` and
   `Stroke` do, so the ordering rule is untouched.
3. `parse_mask_shapes` / `format_mask_shapes`, the CLI spelling.
4. `write_mask_svg`: a `<path>` with `C` commands. `parse_svg_path` already
   reads them; keep the control points by mapping `C` to the new kind instead
   of flattening, when the element carries its `data-op`.
5. `StencilEdit`: `stencil_contains`, `stencil_move`, and handles for the
   anchors and control points.
6. `SegmentPanel`: the tool button and key, the outline, and the list label.
7. `frame_mask_test` (raster and SVG round trip) and `stencil_edit_test`.
