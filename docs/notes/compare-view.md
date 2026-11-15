# Looking at several models at once

`src/app/gui/CompareView.{h,cpp}` is the GUI's model viewer: up to four
finished models — splats, meshes, point clouds, in any mix — laid out in one
pane, two, three, or a 2×2 grid, on one camera. It is what both the viewer
screen and the meshing preview draw.

Looking at one model answers "is this good?". Looking at two answers "is this
better?", which is the question anyone tuning a reconstruction is actually
asking, and it is only answerable if the two pictures differ in nothing but the
model.

The **Wipe** checkbox shows every open model in one pane: vertical sections
for two or three models, and a 2×2 arrangement for four, in row order. Each
section has a name along its bottom edge. Drag a divider to resize sections;
in the 2×2 view, drag the intersection to move both dividers together. Sections
start at equal sizes when the model count changes. Vertical dividers in the
three-model view stop at their neighbors.

Wipe forces linked navigation and supports splats, meshes and point clouds
in any mix. Editing or rendering a model temporarily suspends the wipe.

The other viewports render without controls at the first viewport's size,
then their textures are cropped at the dividers. Engine renders are asynchronous,
so the sections can briefly show different frames during navigation; they
converge when the camera stops. Preview textures use the opposite vertical UV
direction to engine textures.

Three pieces make that work.

## Several splat models resident at once

The engine is a process-global singleton with one set of world splats
(`AGENTS.md`, "the engine is a process-global singleton"). Re-uploading a set
per pane is not an option: a 274k-splat model with SH degree 3 is 65 MB on
device, and a camera drag would re-upload every model every frame.

So `Engine.h` grows **scene slots**: `engine_scene_set_data_3dgs(slot, …)`
uploads a complete splat set into its own device buffers, and
`engine_scene_activate(slot)` points `engine().world` at one of them.
Activation is a pointer swap plus four scalars, so `RenderWorker` does it per
render (`ViewerRenderConfig::scene_slot`) rather than leaving it to whoever
touched the engine last.

The buffers go through `DevicePool::acquire_dynamic` with a `scene<i>.` name
prefix rather than through a `PoolSlot`, because the number of live sets is a
runtime property — the compile-time slot table has one `world.means`, and that
one is still the training run's. `engine_reset()` frees them and forgets the
table together, so a slot cannot outlive the memory under it.

A slot carries no optimizer, densify or gradient state. It is for looking at,
not for training on.

Everything else the panes share stays shared: one `ForwardCache`, one set of
screen buffers, one BVH. They are scratch, resized per render, so the pool
settles at the high-water mark of the largest model rather than the sum.

### SH storage

A slot keeps its SH rows packed (`QuantizedTensor`, the per-cell-block layout
in `sh-quant-layout.md`), not fp32: 16 bits by default, the precision training
already stores them at, and 8 bits when the free VRAM could not also hold a
render of every splat. `sh_value_bits` on `engine_scene_set_data_3dgs` forces
one (`scene_sh_storage` tests all three against each other).

At SH degree 2 a splat is 152 bytes in fp32, 105 at 16 bits and 81 at 8; a
render with every splat in view adds about 100 bytes per splat on top (99 for
small synthetic splats at 64 px bins, 105 for a trained SH3 model at the
viewer's 16 px; `kRenderBytesPerSplat` allows 110). The fp32 store is what
put a 55.3M-splat SH2 merge of four partitions out of reach on a 12 GB RTX
5070, while the WebGL viewer, which packs to about 64 bytes, opened it.
RTX 5070, all synthetic splats in view at 1080p, 64 px bins:

| SH2 splats | SH storage | after upload | after a render | render |
|---|---|---:|---:|---:|
| 10M | fp32 | 1.42 GiB | 2.38 GiB | 19.8 ms |
| 10M | 16-bit | 0.98 GiB | 1.94 GiB | 18.6 ms |
| 10M | 8-bit | 0.75 GiB | 1.72 GiB | 18.6 ms |
| 55.3M | fp32 | 7.83 GiB | out of memory | -- |
| 55.3M | 8-bit (auto) | 4.16 GiB | 9.27 GiB | 65 ms |

CUDA timings. Vulkan memory is identical, and on both backends a packed slot
renders 3-6% faster than fp32 (paired runs in one process). Against the fp32
render, 16 bits differs by at most 2e-5 and 8 bits by 0.5/255 on random SH2
rows (σ 0.15), 1.3/255 on SH3 rows (σ 0.3). Packing adds about 0.2 s of
upload per 10M SH2 splats.

### Large models

A splat file is streamed into its slot rather than read whole:
`read_splat_ply_chunks` hands `SplatViewer` 2^18 splats at a time, and
`engine_scene_begin_3dgs` / `engine_scene_upload_3dgs` place each run, packed
SH included. Of the means only the stride `scene_extent` samples is kept, so
the frame is the one a whole read would give. A 50M-splat SH3 file is 11.8 GB
as floats; streamed, the GUI's resident memory stays near 400 MB and the load
takes 13 s.

A frame of the plain view never leaves the device: `RenderWorker` blits the
engine's own rgb, depth and transmittance (`engine_blit_render`) and reads
back the 8-bit result. The host path, which the other views (depth, normals,
the region overlay, debug colours) still take, downloads all three as floats,
builds the coverage on the host and uploads them again: 13 ms of a 1600x800
frame, growing with the viewport. `viewer_blit_paths` holds the two paths to
the same bytes.

The viewer bins at 16 px (`binning-tile-size.md`). Together, 1600x800, the
model in view, RTX 5070, Vulkan:

| SH3 splats | before | binned at 16 px | blit on the device |
|---:|---:|---:|---:|
| 29.9M (16-bit SH) | 364-456 ms | 78 ms | 63-70 ms |
| 49.9M (8-bit SH) | -- | -- | 101 ms |

Past that, the render's own buffers set the limit: 59.9M SH3 splats are 5.7
GiB packed at 8 bits and a full view needs about 6 GiB more, which a 12 GB
card does not have at any bin size. The frame cost is now mostly per splat
(sort, projection, intersect) rather than per pixel, so the half-resolution
frames of a moving camera are no cheaper: 64-69 ms at 800x400 for the 29.9M
model.

## One camera, four frames

Each pane carries a similarity mapping its model's own normalized frame into
the **shared** frame the camera navigates (`ViewportPanel::set_model_transform`).
It is applied to the CAMERA, not to the geometry — `model_c2w()` maps the nav
pose through the inverse before either renderer sees it — so dragging a model
into place is free, and it works identically for the engine renderer and the
GL preview.

By default that similarity puts every model in the FIRST one's frame: two
reconstructions of one scene share world coordinates, so aligning their fitted
frames (`SplatViewer::frame_center` / `frame_unit`) lines them up exactly. That
is also what places a mesh in its own splats' frame on the meshing screen,
which used to be a special case there. A model with nothing to do with the
first one gets "place in the first model's frame" turned off and is framed on
its own; the position / rotation / size controls adjust it by hand either way.

The axes-and-grid overlay is the first model's, in that model's coordinates:
the models in one view are meant to be one scene, so one grid keeps the panes
comparable.

## One mutex

Every pane's `RenderWorker` takes the SAME engine mutex (`CompareView` owns
it). Two panes rendering concurrently would race over which scene is bound,
which is not a subtle failure — it is one model rendered with another's camera.
