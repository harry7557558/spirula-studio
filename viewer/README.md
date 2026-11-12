# Spirula Studio Standalone Viewer

A dependency-free, client-side **WebGL2** viewer for 3D Gaussian Splats,
meshes, and **MVS datasets** (COLMAP / Nerfstudio / Metashape). It runs
entirely in the browser (no server) and can be hosted as a static site (e.g.
GitHub Pages). Performance-critical parsing, depth sorting, and statistics run
in **C++ compiled to WebAssembly** (built with CMake + Emscripten); rendering
is WebGL2.

The directory holds two independent pages: `index.html`, the model viewer
described below, and [`telemetry.html`](#telemetry-viewer--telemetryhtml), an
**IMU/GPS viewer** for captures. Each has its own WASM module.

The viewer's own runtime code is **independent** from the rest of
the trainer — nothing here imports from the training code at runtime. The
one deliberate exception is at build time: the WASM module compiles the
trainer's dataset parsers (`src/data/parsers/*Parser.cpp`)
**in place** — referenced by relative path, not copied — so COLMAP/Nerfstudio/
Metashape parsing has exactly one implementation in the repo. Those files are
plain C++17 with no CUDA dependency (see `csrc/CameraModel.h`).

## Features

- **3D Gaussian Splatting** (`.ply`, INRIA and Spirula Studio layouts, including very
  large files — binary PLY is parsed **streaming** through a small chunk
  buffer, non-position attributes are stored as half floats, rest SH
  coefficients are **8-bit quantized** (Gaussian-wise scale, `RGB8_SNORM`
  textures — half the VRAM and heap of f16, visually lossless), and the SH
  texture / mesh index buffers are split into chunks below per-resource GPU
  limits; tested with a 6.4 GB, 39M-splat SH2 scan (~3 GB VRAM). Splats are
  **Morton-reordered** at load so the depth-sorted draw order touches
  attribute textures cache-coherently (several-fold frame-rate gain on
  multi-10M-splat models); the async sort worker keeps only an xyz copy.
  Deep-linked `?model=` URLs stream straight into the parser (no Blob
  buffering, so multi-GB hosted models load). If the GPU runs out of memory
  on the SH textures, the viewer drops one SH degree at a time and retries
  instead of failing.
  - Primitive select: **3DGS**, **Mip** (antialiased), **3DGUT** (unscented
    transform projection; fragments evaluate the 3D Gaussian along per-pixel
    rays, "eval3d"). 3DGS/Mip use analytic projection Jacobians with the
    conventional out-of-bounds Jacobian clipping.
  - **Color gamut** (Rec.709 / DCI-P3 / Rec.2020 / AdobeRGB / ACEScg /
    ACES2065-1) and a **linear-color** toggle, matching the training pipeline's
    `rgb_to_srgb` conventions.
  - Spherical harmonics up to degree 4, spherical-beta lobes (`sb_params_*`,
    up to 8, activated at load into two half-float texels each and added to
    the clamped colour, with a toggle), exposure.
  - Depth sorting matches the training code's `get_sorting_depth`: planar for
    perspective/orthographic, distance for equirectangular, and a smooth
    |z|/radial blend for fisheye — content behind the camera composites
    correctly in >180° views. Sorting runs **asynchronously in a Web Worker**
    that hosts its own instance of the WASM module (native counting sort over
    a positions copy), so looking around never blocks the render loop; results
    apply latest-wins when ready.
  - GPU-friendly: attributes live in `TEXTURE_2D_ARRAY`s laid out so arbitrarily
    large models render **even under a small `MAX_TEXTURE_SIZE`** (tiles into
    array layers).
- **Meshes** (`.ply`, `.obj`, `.gltf`, `.glb`, `.stl`) as produced by the meshing code:
  vertex colors or a base-color texture atlas, shading toggles (shaded /
  unshaded, flat / interpolated normals, color on/off) with a view-following
  headlight so the surface reads from every angle. GLTF/GLB are parsed in JS;
  drop the model together with its external `.bin` / `.mtl` / image files for
  textured `.gltf` / `.obj`.
- Orbit / trackball / first-person / free-fly navigation (mouse, touch, keyboard,
  gamepad — matched to the training viewer), Y-up ↔ Z-up toggle (switching keeps
  the current view), and camera models **perspective / orthographic / fisheye
  (equidistant) / fisheye (equisolid) / equirectangular 360°** — all with
  analytic Jacobians / sigma-point projection; mesh triangles crossing a
  projection discontinuity (the equirect seam, the fisheye backward point) are
  discarded, and fragments outside a fisheye image circle are clipped.
- A rasterized, depth-tested **axes + grid** overlay in the model's native
  frame (power-of-10 cells that adapt to the zoom level; the line patch
  follows the orbit target while staying on the global lattice) and a
  configurable background.
- **Statistics** (splat count / vertices / edges / faces) and on-demand
  **parameter histograms** (opacity, scale, effective rank, RGB, anisotropy for
  splats; edge length, triangle area, coordinates for meshes), computed in WASM
  and cached.
- **MVS datasets** — drag & drop a **COLMAP** reconstruction
  (`cameras`/`images`/`points3D`, binary `.bin` or text `.txt`), a
  **Nerfstudio** `transforms.json` (+ point cloud `.ply`), or a **Metashape**
  camera export (`.xml` + `.ply`, optional `.psx`) — as a folder or as
  individual files:
  - Renders the **seed point cloud** and **camera frustums** with the true
    per-camera projection: the image border is discretized and unprojected
    through the camera model (pinhole / fisheye / equisolid / equirectangular)
    **including lens distortion** (Newton undistort over the parser's
    distortion tier: OpenCV `k1 k2 p1 p2` or thin-prism `k1–k4 p1 p2 sx1
    sy1`), so a fisheye camera's frustum visibly bulges. Wide cameras get an
    image-aligned wire dome (fisheye) or a lat/long wire globe
    (equirectangular) instead of a lone border ring. Point size and frustum
    size are adjustable; either layer can be hidden.
  - A compact info panel: source format, image / point counts, camera groups,
    per-model image counts.
  - **Component picker** when the dataset has several reconstructions
    (`sparse/0`, `sparse/1`, multiple Metashape `<component>` chunks, …).
  - **Hover** anywhere on a camera frustum (ray-picked, not just its apex) to
    see its intrinsics (model, fx/fy/cx/cy, distortion); **double-click** a
    camera to view the scene from it (average focal, cx/cy/distortion
    omitted); double-click the point cloud to recenter. Picking is
    depth-ordered: whatever is visually in front at the cursor — frustum or
    point cloud — wins.
  - Degrades gracefully: dropping only `sparse/`, only a `transforms.json`,
    only a Metashape `.xml`, or only a point-cloud `.ply` shows whatever is
    available (images are never required, or read at all — only the metadata
    files are parsed). Load failures and partial loads pop a visible error
    toast (no need to open the console).
- **Double-click** the viewport to center the view on the point under the
  cursor (MeshLab-style: the point becomes the orbit pivot and slides onto the
  optical axis). Splats use a one-pixel GPU depth pass; meshes use a WASM
  raycast; datasets pick the nearest point along the view ray. Works with every
  camera model.
- The **Center** menu (Scene section, next to the up-axis toggle) picks what
  the view orbits about and fits to: the model's origin, the point cloud's
  geometric median or mean, or over a dataset the camera positions' median or
  mean or the point the cameras look at. Camera position median is the
  default; over a splat or mesh file, which has no cameras, the camera entries
  are disabled and the point statistics stand in. The same six modes are the
  trainer's `--scene-center` and the native GUI viewport's menu
  (`src/data/SceneCenter.h` is the one implementation).
- One model at a time — dropping another replaces it and frees the previous GPU
  buffers. Replacing keeps the current viewpoint (the camera is only fitted for
  the first model; refresh the page to start over).

## Telemetry viewer — `telemetry.html`

A second, independent page in the same directory: drop videos, photos or whole
folders and see the **IMU, GPS and exposure** they carry. It reuses the trainer's reader
(`src/sfm/core/Telemetry.cpp`, compiled in place into its own WASM module) so
the browser sees exactly what the reconstruction pipeline sees —
`docs/notes/imu-gps-for-sfm.md` is what that is for.

- **Detection is by content, not extension.** A GoPro `gpmd` track (GPMF), the
  Insta360 trailer that follows the MP4, a DJI `djmd` protobuf track, a Google
  **CAMM** track, a Kandao `kvar` box, or **EXIF GPS or exposure** in a JPEG.
  Anything else is skipped silently, so a folder of a few thousand mixed files
  is a reasonable thing to drop.
- **Files stay on the machine and are never buffered whole.** The scan worker
  answers the parser's reads with `FileReaderSync` over a `File` slice behind a
  1 MB block cache, so a 4 GB capture is read in place — tested end to end on
  4 GB `.360` and `.insv` files, which no `ArrayBuffer` would hold. Up to three
  workers scan in parallel, each with its own module instance.
- **3D viewport** in a local metric frame (east / north / up, metres at the
  scene origin) with a **raster basemap** on the ground plane —
  OpenStreetMap, Esri satellite, CARTO dark, OpenTopoMap, or a custom
  `{z}/{x}/{y}` template for a keyed provider. Tiles load lazily by view and
  zoom, and a tile that has not arrived is drawn from its nearest loaded
  ancestor, so panning never shows holes. Altitude is real (with an
  exaggeration slider and optional drop lines to the ground), and the frame is
  Web Mercator scaled by cos(lat₀), which is the one frame where imagery and
  track agree without warping either.
- **The IMU is drawn where it was measured**: an attitude triad at the
  playhead, optional acceleration and gravity vectors, and an attitude trail
  every *n* seconds along the track. The track itself can be coloured by time,
  speed, altitude, DOP, or gyro/accel magnitude.
- **Stacked plots** under the viewport share a time axis with the viewport's
  playhead (drag to scrub, shift-drag to pan, wheel to zoom, space to play).
  Each channel keeps a min/max pyramid, each level 8× coarser than the one
  below, so a 400 000-sample gyro redraws in the cost of the canvas width
  rather than the capture length.
- **Exposure** -- shutter, ISO and aperture as a camera logged them per frame,
  or as each photo's EXIF records them -- is a plot in stops about the clip's
  mean (the sum the trainer's `--ppisp-exposure-from-exif` compares, with the
  factors that move beside it), plus shutter / ISO / aperture rows on log axes
  off by default. The track can be coloured by it, and the readout gives the
  values at the playhead.
- **Screenshot-safe mode** (one toggle, also in the header) hides the basemap,
  latitude/longitude, absolute altitude, wall-clock times and file names, and
  redacts the fix line from the report — shapes, distances and every IMU
  stream stay, so a plot can be shared without publishing where it was taken.
- Per-file verdicts come from the same checks the trainer runs: sample-rate
  regularity, the gravity norm, cross-stream gravity agreement, and whether the
  GPS moves rather than repeating one stale fix. The full text report is in the
  panel. Fixes at 0, 0 -- a receiver with no fix -- are dropped on read, as
  the trainer drops them.

English only, by design — this is a diagnostic, and the model viewer's i18n
would be overkill.

## Build

Requires the Emscripten SDK on `PATH` (`emcc`, `emcmake`) and CMake ≥ 3.16.

```bash
source ~/emsdk/emsdk_env.sh      # activate emsdk
./build.sh                       # both modules
./build.sh ssv_telemetry         # just the telemetry one
```

The two modules are separate CMake targets, so editing one page's sources
rebuilds only that page. This produces `js/ssv_wasm.{js,wasm}` and
`js/ssv_telemetry.{js,wasm}` (committed so the site is directly hostable
without a build step).

## Run

Serve the directory over HTTP (ES modules + WASM require it — `file://` will not
work):

```bash
python3 -m http.server -d .      # then open http://localhost:8000/
```

Drag a model or a dataset folder onto the canvas, or click to browse (the
picker takes multiple files; folder drops walk the directory tree). You can
also deep-link a hosted model: `index.html?model=<url>` (external
`.bin`/`.mtl`/image siblings are fetched automatically).

`telemetry.html` is the IMU/GPS page. Its basemap fetches tiles from a public
provider at view time, so that page needs network access and the provider's
usage policy applies (the built-ins are fine for interactive use; for anything
heavier, pick **Custom URL…** and paste your own `{z}/{x}/{y}` template with a
key). Choosing **None** — which screenshot-safe mode does anyway — leaves the
page fully offline.

## Test

`test/run.sh` generates synthetic COLMAP / Nerfstudio / Metashape datasets and
drives the viewer end-to-end in headless Chrome (needs `google-chrome` and
`node` ≥ 20):

```bash
test/run.sh            # all cases
test/run.sh metashape  # one case; see test/test_ds.html for the list
```

The telemetry page's parser is covered by the trainer's `sfm_telemetry_test`
(synthetic GPMF / Insta360 / DJI / CAMM fixtures). `test/telemetry_probe.mjs`
checks the page itself — WASM boot, the worker's `File` reads, the clip
model — against captures of your own, which is the only way to exercise the
multi-GB path:

```bash
python3 -m http.server 8098 &
node test/telemetry_probe.mjs http://localhost:8098/telemetry.html VIDEO.360 PHOTO.jpg
```

## Layout

```
index.html          model viewer: UI + panel
telemetry.html      IMU/GPS viewer: UI + panel
css/style.css        theme (adapted from the training viewer)
css/telemetry.css    the telemetry page's own layout and overlays
js/
  main.js            app wiring, input, render loop, sorting
  renderer.js        WebGL2 renderer (splat HDR pass, mesh, dataset, grid lines, tonemap)
  shaders.js         GLSL (splat / tonemap / mesh / points / frustum+grid lines)
  camera.js          camera + navigation modes
  dataset.js         dataset load orchestration + frustum geometry (undistort)
  sortworker.js      async depth-sort worker (own WASM instance)
  wasm.js            WASM bridge + streaming loader + GLTF/GLB (JS) + MEMFS mount
  colors.js          gamut matrices
  linalg.js          vec/quat/mat helpers
  histogram.js       canvas bar chart
  ssv_wasm.{js,wasm} built WASM module
  telemetry/
    main.js          telemetry app wiring, options, viewport render loop
    scanworker.js    per-file scan worker (own WASM instance, FileReaderSync)
    scene.js         WebGL2 viewport (instanced polylines, tiles, points)
    tiles.js         basemap tile cache with ancestor fallback
    charts.js        stacked plots with min/max decimation pyramids
    geo.js           Mercator frame, per-clip model, derived series
  ssv_telemetry.{js,wasm}  built telemetry WASM module
src/viewer.cpp       parsers (PLY/OBJ), depth sort, histograms
src/dataset_bridge.cpp  dataset C ABI over MEMFS (drives the trainer's parsers)
src/telemetry_bridge.cpp  telemetry C ABI (drives the trainer's Telemetry.cpp)
test/                end-to-end harness (headless Chrome) + data generators
CMakeLists.txt       Emscripten build (also compiles ../src/data/ parsers)
```

Frustum lines use the same seam handling as meshes: fragments whose
interpolated camera-space position re-projects far from the rasterized
position (a segment wrapped across the equirect ±180° seam or the fisheye
backward point) are discarded, and fisheye display models clip to the image
circle. Double-clicking a dataset camera switches the display projection to
that camera's model (pinhole → perspective, fisheye → equidistant, equisolid,
equirectangular) with the matching field of view.
