# The `spirula` application: CLI trainer (`spirula train`) + native GUI

Working notes for the no-Python trainer under `src/app/`. Written 2026-07-10,
GUI added 2026-07-12; covers code structure, verification status, and
remaining ports. Long-term context: Phase 1 (standalone C++ trainer) and
Phase 2 (Dear ImGui/GLFW GUI) of the GUI-app plan are done; Phase 3 is
Windows/Linux packaging; kernels stay in Slang for the eventual
Vulkan/cross-vendor port.

## Build & run

```bash
cmake -G Ninja -B build && cmake --build build --target spirula
./build_vulkan/spirula train [<preset>] --data <colmap_dataset_dir> [--flag value ...]
```

- Presets = tyro subcommands: `3dgs` (default), `360-camera`, `in-the-wild`,
  `centered-object`, `hdr`, `synthetic`, `meshing`, `academic-baseline`.
- Flag conventions: flattened names (`--sh-degree`, not `--model.sh-degree`);
  `-`/`_` interchangeable; bools take a value (`--warp-to-pinhole 1`);
  `--key=value` works (arity-1 only); `none` clears optionals; tuples take N
  values (`--bilagrid-shape 8 8 4`). `--help` shows preset-resolved defaults.

### Building without Torch/Python

Torch + Python are only needed for the Python extension (`ext.cpp`); the
engine and app code are torch-free. CMake falls back automatically when
`import torch` fails or python3 is missing; `-DSS_NO_TORCH=ON` forces it:

```bash
cmake -G Ninja -B build_notorch -DSS_NO_TORCH=ON && cmake --build build_notorch
```

In this mode `csrc` is a STATIC lib (the engine API has no dllexport
annotations, and this yields a self-contained exe — deps are just libcudart +
system libs), CUDA archs come from `nvidia-smi --query-gpu=compute_cap`
(override with `-DTORCH_CUDA_ARCH_LIST`),
and the libpython link + static-libstdc++/nftw interposition workarounds are
skipped (they exist only because of libtorch). Generated headers
(`src/generated/`, `src/instantiations/`) are committed, so a fresh
checkout builds with no Python at all. With Torch present the extension build
is unchanged (shared `libcsrc`, same flags as before).

Windows (VS2022 + CUDA toolkit): run `build_develop.bat` from any cmd prompt —
it locates VS via vswhere and ALWAYS calls vcvars64 (an ambient cl/INCLUDE may
reference an uninstalled SDK), picks the newest installed CUDA toolkit
(ambient CUDA_PATH may pin one too old for the MSVC in use), falls back to
VS-bundled cmake/ninja, and configures with `-DSS_NO_TORCH=ON` (a broken
torch install would abort configure from inside TorchConfig.cmake — QUIET
can't suppress errors raised inside a found package config). Manual
equivalent from a vcvars64 shell:
`cmake -G Ninja -B build -DSS_NO_TORCH=ON -DCMAKE_BUILD_TYPE=Release`.

## Native GUI (`spirula` with no arguments, Phase 2)

Dear ImGui + GLFW + OpenGL 3.2-core desktop app ("Spirula Studio").
On by default; `-DSS_BUILD_GUI=OFF` leaves a command-line-only binary that
needs neither a display nor GL, and fetches nothing.

```bash
cmake -G Ninja -B build   # composes with -DSS_NO_TORCH=ON
cmake --build build --target spirula
```

GLFW 3.4 and imgui v1.92.8 are pinned and fetched at configure time via
FetchContent (network needed once; on WSL drvfs mounts git may need
`safe.directory` entries for the build tree's `_deps/{glfw,imgui}-src`). Only system dep
is OpenGL + X11/Wayland dev packages on Linux; Windows (MSVC) and macOS
(GL 3.2 forward-compat) code paths are in place — macOS still needs the
CUDA→Vulkan port to actually run the engine.

Design: novice path is Home → "Open a Dataset" (or "Create Dataset from
Photos/Video", which drives external `colmap`/`ffmpeg` CLIs with live log +
cancel) → preset dropdown + a curated Basic Options list → Start Training →
live native viewport. Advanced path: "All Options" editor **generated from
the `SS_CONFIG_FIELDS` X-macro** — all config fields, under the field
table's `section` headings, filtered by its `tier` (Basic / Advanced /
Everything), searchable across the filter, help text as tooltips,
modified-from-preset highlighting with right-click reset; a new row in the
field table appears automatically. Web viewer can be additionally served from the
GUI (Basic Options) for remote monitoring.

| File | Role |
|---|---|
| `gui/GuiMain.cpp` | GLFW window + GL 3.2 core context + ImGui bootstrap, dark style, DPI scale, frame loop, close-confirm flow. **Drag-and-drop** (glfwSetDropCallback → `GuiApp::handle_drop`, which takes the **whole drop at once** — several videos dropped together are the inputs of one dataset, and `spirula <path>...` goes through the same call): each path is auto-detected as an SfM dataset folder (transforms.json / sparse/ / colmap/ marker, or a Metashape camera .xml + point-cloud .ply pair → open; only honoured when dropped alone), a photo folder (contains images), or a video file (extension; the .insv preset applies per file). Files from inside a dataset (transforms.json, .db/.bin/.txt/.xml) open their parent. A **preset** — a saved one or a run's `config.json`, recognised by content rather than by name — is applied to the trainer screen instead (refused while training, since applying one re-parses the dataset). Anything dropped onto the Batch screen becomes a row of the kind it is: a dataset trains, a model meshes, videos and photo folders build a dataset and then train it. A preset lands on the screen its own kind belongs to. Raw input dropped onto the dataset screen is **added** to the list there; dropped anywhere else it starts a new dataset. Video/photo drops are ignored while training (datasets go through the stop-confirm flow). |
| `gui/GuiApp.h/.cpp` | Screens (Home / New Dataset / Train / Viewer / Batch / Mesh), layout, wiring; the recent list, tool paths and the directory each kind of file pick starts in (`dialog_dir.<kind>`) persisted to `~/.config/spirula-studio/gui.conf` (`%APPDATA%` on Windows). Session-destroying navigation (Home / open-dataset / quit during training) goes through a stop-and-save confirm modal with a deferred pending-action; output folder defaults to `<dataset>/outputs` with a Browse button + resolved-run-path preview. Editing any dataset-parsing option (`SS_DATASET_PARSE_FIELDS` via the generated `parse_settings_equal`) marks the dataset dirty and auto-reloads it once the edited widget loses focus. NOTE: `open_dataset`/`remember` take `std::string` **by value** — callers pass paths out of `_recent` and `remember` reorders it (a const& dangles; this was a real bug that corrupted the path to ""). |
| `gui/RecentList.h/.cpp` | The home screen's **recent list**: datasets opened in the trainer, reconstructions built on the dataset screen (clicking one restores its inputs from the workspace's `.spirula-dataset.json` and its settings with them), models opened in the viewer, training runs that saved a model, and camera projects -- newest first, `kPerKind` of each, one `recent.<kind>=<unix time> <path>` line per entry in `gui.conf` (the untimed `recent=` / `recent_model=` lines of older builds still read). A probe on a detached thread drops whatever is no longer on disk and reads each model's header to say whether it is splats, a mesh or a point cloud; detached because a share that has gone away blocks a stat for the network timeout, and neither the UI nor quitting should wait that out. Drawn by `GuiApp::draw_home_recent` (one tab per kind plus "Recent" for all of them, beside the buttons when the window is wide enough) and by File > Open Recent. |
| `gui/ConfigUI.h/.cpp` | The X-macro-generated options editor. Zero per-field special cases (only `data` is hidden, managed by the dataset picker). |
| `gui/ViewportPanel.h/.cpp` | Native viewport with two backends behind the browser-identical NavCamera navigation: **Preview** = GL point cloud + frusta as soon as the dataset parses (PreviewRenderer), **Engine** = RenderWorker once training starts, with all four web-viewer camera models (Pinhole / Fisheye-equidistant / Fisheye-equisolid / Equirectangular; `fovToIntrinsics` + per-model FOV ranges ported from viewer.html). Buffer picker, camera-frusta overlay with a live **frustum-size slider** (`ViewRequest::cam_size_scale`), render-scale + live-refresh throttle (0.15 s). Initial framing: seed-point centroid target, median camera distance, camera-centroid direction. **Double-click centering** (viewer.html `recenterAt`): pan laterally so the 3D point under the cursor sits on the optical axis and make it the orbit pivot (rotate toward it when behind the camera plane, >180° models); preview mode picks the nearest displayed point along the cursor ray (3% angular cone, `PreviewRenderer::pick_point`), engine mode attaches `pick_px/py` to the next render and gets the point back in the ViewResult — depth-channel readback, no extra VRAM or render pass; all four display camera models via `viewer_pixel_ray`. |
| `gui/CompareView.h/.cpp` + `gui/SplatViewer.h/.cpp` | **Up to four finished models under one camera** — the viewer screen and the meshing preview are both this. SplatViewer loads a file (3DGS PLY / point cloud / mesh, auto-detected) on a worker thread; splats land in an **engine scene slot** (`engine_scene_set_data_3dgs` / `engine_scene_activate`, Engine.h) so several stay resident and the RenderWorker binds one per render — a pointer swap instead of a per-pane re-upload. CompareView lays the panes out (1 / 2 / 3 / 2x2 by count), owns the ONE engine mutex every pane's worker takes, links the navigation, and gives each pane a similarity from its model's own normalized frame into the shared one — applied to the CAMERA, not the geometry, so the per-model position / rotation / size controls are free. That similarity defaults to "sit in the first model's frame", which is what aligns two reconstructions of one scene exactly and what places a mesh in its splats' frame. Design: `docs/notes/compare-view.md`. |
| `gui/NavCamera.h/.cpp` | 1:1 port of viewer.html's `cam`/`quat`/`Nav`: quaternion camera, four modes (Turntable / Trackball / First Person / Free Fly), same sensitivities and mappings for **mouse** (LMB orbit-or-look, RMB/MMB/Shift pan, wheel dolly), **keyboard** (WASD/arrows, E/Q up-down or Fly-roll, active while the pointer is over the viewport), **gamepad** (GLFW gamepad API: left stick move, right stick look, triggers up-down/roll -- including the browser quirk that triggers only translate while the left stick is deflected), and **touch** via the OS's pointer/gesture emulation (single finger = orbit; system pinch/pan gestures arrive as wheel; GLFW exposes no raw multitouch). Keep in sync with viewer.html's Nav. The initial pose replicates `cam.reset()` verbatim (target = client-frame origin = the CAMERA-POSE center via center_method="poses", pos=[0,0,1], orbit(0,-250)) -- verified pixel-equivalent against a `/render` fetch from `spirula train`'s web viewer at the client's default c2w. `SS_NAV_DEBUG=1` logs per-frame mouse-drag nav decisions (button/pan/target/pos) to stderr. |
| `gui/PreviewRenderer.h/.cpp` + `gui/GlLoader.h/.cpp` | Offscreen-FBO GL renderer for the dataset preview (sparse points, vertex-colored + per-input camera frusta as center+offset lines scaled by a uniform). Frustum wireframes are built from each camera's TRUE model + distortion via `frustum_template` — a C++ port of `viewer/js/dataset.js` `frustumTemplate`/`generateRay`/`undistort` (itself a port of `fill_frustum_segments_kernel` / `projection_utils.cuh`): pinhole = classic border pyramid, wide models (fisheye/equisolid/equirect) = dome/globe with dimmed interior gridlines, Newton undistortion with the `is_valid_distortion` bail-out + bisect-toward-center for out-of-domain border pixels. (Before this, frusta were tan-based pinhole pyramids — a ~200° THIN_PRISM_FISHEYE dataset drew as giant crisscrossing triangles.) The display-side vertex shader implements the same camera models as the engine viewer (pinhole / fisheye-equidistant / equisolid / equirectangular; `u_s` = fx/(W/2), fy/(H/2), linear view-distance depth), so the preview -> engine transition at training start keeps pose AND intrinsics with no jump (ViewportPanel `maybe_frame` re-frames only when the dataset identity changes; navigation done in the preview carries into training, verified pixel-aligned at fisheye 333°). The projection function is shared with the fragment shader, which **re-projects the interpolated view-space position and discards fragments whose reprojection error exceeds 5% of the viewport** — the web viewer LINE_FS seam fix, killing the full-width streaks that segments crossing the equirect ±180° seam / fisheye backward point / pinhole near plane used to rasterize as. That check alone still keeps ~5%-viewport stubs at both ends of a wrapped segment (the error vanishes at the endpoints), which stack into ladder artifacts at the equirect edges — so line vertices also carry `a_delta` (this-endpoint − other-endpoint, pre-scale) and the vertex shader kills the WHOLE segment when its chord crosses the equirect seam half-plane (x=0, z>0 view space); same mechanism as the web viewer's `aDelta`/`vKill`. Optional **grid + axes** (viewport "grid" checkbox): power-of-10 ground grid + colored positive half-axes generated in the **training (saved-splat) frame** — lines mark round coordinates of the exported model — then mapped into the preview's normalized frame; the cell decade adapts to the nav distance and the finite line patch recenters on the orbit target (lattice-snapped, with hysteresis), cell edges subdivided 4x so they curve correctly under the non-pinhole projections; depth-tested like all preview geometry (`ensure_grid`). GlLoader = minimal namespaced (`glx::`) proc loader via glfwGetProcAddress; no GLEW/GLAD. |
| `gui/RoiEditor.h/.cpp` | **The region a dataset trains in**, drawn over its point cloud: boxes, ellipsoids, cylinders and outlines (extruded polygons drawn from above) added to, cut from or overlapped with each other (`data/RoiDocument.h`), with quick starts (box around the scene, cylinder or ellipsoid around an orbited subject, an outline drawn in the current view), an Adjust mode where hovering the selected shape lights the side under the pointer and dragging pushes or pulls that side alone, then Move / Resize (stretch or scale) / Rotate, every side also settable by number, undo, and the combined region shown live as the trainer will see it (boundary mesh, dimmed points, counts). Saves `<dataset>/roi/<name>.json`, which the trainer picks up unless told otherwise; opened beside Partition on the dataset screen and from the training screen's region row. docs/notes/roi-editor.md |
| `gui/TrainRunner.h/.cpp` | TrainerSession on a worker thread: phases Idle→Loading→Ready→Preparing→Training→Done, pause/stop, metric history for plots, optional ViewerServer. Lifetime rule: viewport detaches before the session is replaced. |
| `gui/PresetFile.h/.cpp` | The half of a saved preset that is the same whatever it is a preset OF: the `<config_dir>/presets` folder (one sub-folder per kind), the header every file carries (`spirula_preset` / `kind` / `name` / `description`), the probe that says which kind a dropped file is, and the listing that fills a dropdown. A file with no `kind` reads as a **train** preset -- they predate the key, and a run's `config.json` has never had one. `delete_preset_file()` refuses any path whose kind does not match, so nothing else can be removed through it. Written with `data/JsonWrite.h` (the writing half of `data/Json.h`: the commas, the indent and the string escape, once). |
| `gui/TrainPreset.h/.cpp` | **Saved training presets**: the whole training config under a user-chosen name. Encoded with `config/TrainConfigJson.h`, so a missing key keeps the field's default and an unknown key is ignored -- a preset survives both directions of version skew. A run's own `config.json` loads as a preset too, which is the likeliest reason anyone wants the feature. The file also carries `touched`, the set of flags the user set by hand: without it the macro options (`--quality` and friends) would undo the tuning the moment it was loaded, and for a `config.json` (which has no such list) it is derived as "everything that differs from the base preset". `SS_PRESET_CONTEXT_FIELDS` -- `data`, `resume`, `output_dir_prefix`, `output_dir_name`, `roi_region` -- is dropped on save inside `save_preset()` rather than at the call site, so no caller can bake a dataset path into a shared preset. Every place a preset is named (both combos, open or closed, and every row in them) hovers to a tooltip with its description **and its path** -- two presets may share a name, and the path is the only thing on screen that tells them apart. |
| `gui/DatasetPreset.h/.cpp` | **Saved dataset-creation presets**: everything the New Dataset screen decides about HOW a dataset is built -- frame extraction, the 360 plan, colour space, masking, depth and normals, the built-in reconstruction's flags and COLMAP's. One hand-written `X(key, member)` row per setting, over `SfmJob` / `ColmapJob` / `MaskSettings` in place, with `data/JsonField.h` doing the value encoding `TrainConfigJson.h` already used. What it never carries is WHERE: the inputs, the output folder, the clicks drawn on one capture's frames and the border fitted to one lens all describe a particular capture. `sanitize_dataset_settings()` clamps every bounded field on load, because a preset file is text a user can edit and an unattended queue is the wrong place to find out. `preset_roundtrip_test` moves every field the table names off its default and compares after a round trip -- a field added to one of the job structs and forgotten in the table is otherwise silent. The built-ins (`kDatasetPresets`: **general**, **360-camera**, **internet-photos**) are code rather than files, the same shape `config/TrainConfig.h` gives training's: `general` applies nothing, which is what makes it the row a capture starts on. A built-in may also have to ask the capture itself -- a 360 camera writes either two fisheye circles or a 2:1 panorama, and only the frames say which -- which is `dataset_adapt_preset()`, and why it lives in `SourceList` with the other answers that need a probe. |
| `gui/MeshJob.h/.cpp` | What to extract and what to write it as, split from the process that runs it: the two SETS (`colors` x `formats`) a run asks for, and `mesh_job_outputs()`, which turns them into the files that will land on disk, richest first. It is the GUI's copy of `meshing::plan_mesh_outputs()`'s answer, computed from the same function -- which is how the preview knows to open `mesh_textured.glb` rather than guessing at a name the child chose. Here rather than in `MeshRunner.h` so a preset, a batch row and `preset_roundtrip_test` can all reason about a job without linking a child process. |
| `gui/MeshPreset.h/.cpp` | The same for meshing: `MeshJob` minus `checkpoint`, `data_dir` and `output`, which are what the preset is applied TO. `sanitize_mesh_job()` also settles the colour/format pairs the child refuses outright (PLY cannot carry a texture, OBJ has no standard place for vertex colors, STL has neither) so a hand-edited file cannot start a run that writes nothing. |
| `gui/SourceList.h/.cpp` | What a picked path means as an input, and what a LIST of them decides: the lens a file implies, the 360 packing and track count only a probe can answer, the `images/<subdir>` each input's frames go to, the output folder the list implies, and the reconstruction defaults the capture's own kind sets (a video is a capture in order, a dual-lens file is a rig, a 360 file is one camera warped into views). It lives here rather than on the dataset screen because a batch row builds the same list from the same paths with nobody watching, and two answers to "which lens is this capture" would be one too many. `resolve_source_lenses()` is the part that must survive a preset applied afterwards: a preset decides everything except the lens a capture is KNOWN to need, because a dual-fisheye clip fitted with a rectilinear model reconstructs into nothing. |
| `gui/MaskSettings.h` | The mask prompt and its thresholds, split out of `SegmentPanel.h` so a preset module does not depend on a UI panel. |
| `gui/BatchProcess.h/.cpp` | **Batch processing**: a list of rows, each of which can build a dataset, train it any number of times, and mesh what came out -- and none of those three has to exist when the queue is started. A row carries the videos or photo folders to build from, the dataset folder (which the Dataset stage WRITES and the Train stage READS, so the two cannot disagree), a dataset preset, a list of `BatchRun`s and a `BatchMeshOptions`. A **run** is one training pass: its preset, and text overrides of that preset's **max splats / SH degree / steps** -- text because "unset" and "0" are different answers and 0 is a legal `--sh-degree` -- plus whether the row's Mesh stage covers it, which is what lets one row train a large model for its appearance and a cheap one beside it for the mesh. The **mesh options** are a preset plus the colors and formats to write over what it says; empty means "leave the preset alone" rather than "write nothing". `batch_plan()` expands the rows into the tasks they will run as -- one per stage per run, and one mesh per run that asked for it -- which is both what the screen previews before the start and what the driver walks. `batch_progress()` turns the same list into the two bars and the estimate the screens draw, from what the finished tasks of each stage actually took. The list is data, not a runner: `GuiApp::advance_batch()` launches a task, waits for the runner that owns that stage to go idle, records what happened and launches the next -- driving the live screens rather than being a fourth implementation of what `SfmRunner`, `TrainRunner` and `MeshRunner` already are. A task that fails is an ordinary transition: it is recorded, the tasks downstream of it in the same row are passed over (what they needed was never produced), and the next row still gets its turn. `batch_check_row()` is the "warn before anything starts" half, and it is what makes a weekend-long queue worth setting up: an input that is not there, a preset file that has gone missing, a segmentation or geometry checkpoint not downloaded (a batch cannot stop to accept a licence), masking with no prompt (a batch cannot be prompted with clicks), a flag `train_config_unsupported()` refuses, a folder that already holds a reconstruction the run would ADD to rather than rebuild, two rows building into one folder, a row training a dataset a LATER row builds, a preset made for video pointed at photographs, a meshing stage with no run ticked for it, and colors and formats with no pair between them. Nothing waits for a button: the check runs on every edit and on a timer, so a preset file deleted while the screen is open is reported where it happened. Fatal issues block the start (with a "skip the bad rows" way past); the rest are said out loud. The list is kept in `<config_dir>/batch.json` -- losing a queue to a crash three hours in is the one failure a user cannot recover from by trying again -- and a file from the train-only era still loads. A finished queue can **run a command** (`batch_command` in `gui.conf`, so it belongs to the machine rather than to the rows): `kBatchMessageToken` -- `{message}` -- is replaced by a one-line summary of how it went and always arrives as a single argument, braces because `<>`, `%..%` and `$..` are all punctuation some shell would eat. It runs however the queue ended, and the Test button beside the field sends a test message instead, which is the only honest way to find out whether a notifier works before leaving a queue overnight. The field is a box that grows with what is in it (one line while empty, up to twelve), because the real command is a `curl` with a JSON body that nobody writes on one line -- `gui.conf` is one setting per line, so the breaks are escaped on the way in and out. |
| `gui/ColmapRunner.h/.cpp` | images/video → COLMAP dataset. Requires **COLMAP >= 4.x** (version probed from `colmap help`; 3.x-era `use_gpu` flags are gone — flags follow `reference/scripts/run_colmap.bash`): feature_extractor (**SIFT or ALIKED** via `FeatureExtraction.type`; camera model = any the parser supports; single camera / per-folder / per-image; optional **initial focal length** — `init_focal_factor` × width probed via `stbi_info`, composed into `ImageReader.camera_params` with centered principal point + zero distortion, or a raw `camera_params` string) → **explicitly chosen** exhaustive / sequential / vocab-tree matcher (no "auto"; the GUI presets sequential for video, exhaustive for photos). Sequential supports overlap + **quadratic overlap** (on by default) + **loop closure** (`SequentialMatching.loop_detection` via the vocab tree — SIFT only; the tree is auto-found near the workspace/cache or downloaded via curl into `~/.cache/spirula-studio`). **LightGlue** matching (`FeatureMatching.type` `SIFT_LIGHTGLUE`/`ALIKED_LIGHTGLUE`, default for ALIKED) → mapper (`ba_use_gpu` — **forced OFF for fisheye models**, COLMAP's GPU BA doesn't support them; `Mapper.ba_refine_extra_params 0` for perspective models by default — distortion held fixed for stability and recovered in the final BA, per run_colmap.bash's advice; `min_num_matches`) → best-effort **model_merger** when the mapper splits (kept only if the merged model registers more images; written as the next `sparse/<N>` — on a real X5 capture this fused 86+39 partials into 116/118 frames) → optional bundle_adjuster refinement **on the largest/merged model** (was hardcoded `sparse/0`), with a **verify-and-revert guard**: mean reprojection error before/after via `model_analyzer`, refinement discarded when worse/non-finite (releasing pp + 8 thin-prism coefficients on a ~200° fisheye reliably diverges to ~1e150 px — pp additionally stays fixed for fisheye). `.insv` preset: THIN_PRISM_FISHEYE, per-folder cameras, focal factor 0.269 (Insta360 X5: fx=fy≈0.269·width), **exhaustive** matcher (the two lens tracks are concatenated, so sequential misses cross-lens pairs: 116/118 exhaustive vs 68/118 sequential+loop on the same capture). **Resume** (`ColmapJob::resume`, GUI checkbox appears when the workspace holds a previous run): extracted frames are kept per track, masks already written are kept, COLMAP skips features/matches already in database.db, and existing `sparse/<N>` models skip the mapper (the mapper only writes models on completion, so partials are never trusted); with resume off a non-empty workspace is refused rather than mixed into. A measured resume replay of a full .insv run took 39 s vs 246 s. **Repetitive-scene knobs** (Advanced → "Repetitive scenes", with an Off/Low/Medium/High preset combo; all 0 = default): `SiftMatching.max_ratio`, `TwoViewGeometry.min_num_inliers`, `Mapper.abs_pose_min_num_inliers` / `abs_pose_min_inlier_ratio` / `abs_pose_max_error` — stricter matching + registration so similar-looking rooms don't weld together. The GUI's default workspace auto-suffixes `_2`, `_3`, ... instead of pointing at an existing non-empty folder. Optional **AI masking** (the shared `DatasetPrep` stage; in-process SAM 2 / SAM 3, `src/sam/`): masks feed `ImageReader.mask_path` + the trainer's masks dir. Photo-folder inputs are indexed **in place**, recursively (no copy) — the absolute image dir is handed to the GUI in-memory for the immediate open; no marker file is written, so on later re-opens set `data.image_dir` in the dataparser options (video datasets use the default `images/`). Multi-track `.insv` videos split into `images/cam<N>/` + per-folder cameras. The prep stage is shared, so a COLMAP run can take several inputs too — but `feature_extractor` fits ONE camera model to the whole run, so the per-input lens models only mean anything on the built-in path (the panel says so). The mapper may emit several partial models — the trainers auto-pick the largest (below). |
| `gui/FrameSelect.h/.cpp` | Blur-aware video frame selection (multithreaded C++ port of `reference/scripts/extract_frames.py`): ffmpeg extracts candidates at (target fps x sharpness window) with `-nostdin` (**without it ffmpeg can hang reading stdin** — the "stuck at ffmpeg" bug), then the sharpest per window is kept (`FrameSharpness`, scored across all cores). With the adaptive rate on it extracts at (target fps x max(window, spread)) instead and hands the candidates to `FrameMotion`, which decides which of them to keep -- so the fallback adapts without a second decode. |
| `gui/FileDialog.h/.cpp` | Built-in ImGui file/folder browser (no native-dialog dep; works over WSLg/remote X). File mode takes an optional **multi-select** (`open(..., multi_select=true)`, `results()`): clicking a file toggles it, so a dataset can be built from several clips in one pick. |
| `gui/DatasetPrep.h/.cpp` | Everything between "the user picked an input" and "there is an image directory ready for SfM", shared by both reconstruction paths: frame extraction (in-process VK video decode → ffmpeg fallback), sharpest-frame selection, `.insv` track split, AI masking, and the resume rules. A job takes a **list of inputs** (`PrepInput`: path, video-or-folder, the `images/<subdir>` its frames go to, the masks that came with it, and the lens + focal factor that belong to it). **A picked photo folder is resolved by the project's own layout conventions** (`resolve_photo_folder`, matching `spirula sfm auto`'s probing and the dataparsers' `mask_dir = "masks"`): `<picked>/images` is the folder to index when it exists, and the `masks/` beside it holds masks that are **already made** -- those are used as they are and that input is never AI-masked (generating over them would write into a folder we were only asked to read). A folder named `masks` is never an input of its own: picked or dropped, it attaches to the input whose images it sits beside. **Every image walk follows directory symlinks** -- a prepared capture whose `images/`+`masks/` are links into the raw one is an ordinary layout, and the default iterator returns nothing for it -- and never descends into a `masks/` nested under the images, which would otherwise double the dataset with PNGs that are not views. One folder of photos is still read where it is; anything else is gathered into the dataset's own `images/`, one sub-folder per input (a multi-track video adds `cam0/`, `cam1/` under that), which is what makes the inputs separate cameras. `camera_subfolders` reports the folders that hold images **directly**, at any depth and including the root, so a capture handed over as `1/cam0`, `1/cam1`, `2/cam0` ... is as many camera groups as `--camera-mode folder` will make of it; it is depth- and count-bounded because each one becomes a row in the dataset panel. Photo folders in a multi-input job are **hard-linked** into place (masks too, so the two trees keep mirroring), copied when the filesystem refuses. `probe_workspace` reports what the output folder already holds, split into what a run can **reuse** (extracted frames / features / matches / masks -> the resume checkbox) and what it would **replace** (`sparse/` -> a warning): an existing reconstruction is never treated as resumable, since a folder holding one opens in the trainer when dropped, so anything that gets to this screen with a `sparse/` in it is someone else's dataset or a finished one. The input's own images and masks never count as leftovers -- which matters because the output folder for a capture that already has `images/` **is that folder**: `sparse/` belongs beside `images/` and `masks/`, which is exactly where every parser looks. Masks mirror the image tree and are generated **per input**, so the tracker's memory bank never crosses from one capture into the next; a clicked object prompts only the input it was drawn on (`MaskClick::source`), and a clicks-only job whose inputs are not all prompted is **refused** rather than run -- a dataset where three of four camera folders kept the subject reads as a masking run that worked. |
| `gui/SfmRunner.h/.cpp` | images/video → dataset with the built-in SfM, by re-running this executable as `spirula sfm auto` (see SfmRunner.h for why it is a child process). Runs `DatasetPrep` first, then maps the panel onto flags. Per-input lenses become **`--camera-model DIR=MODEL` / `--focal DIR=PX`** overrides, `DIR` being the group's path under `images/` — the input's sub-folder, or a camera folder inside it at any depth (`1/cam0`), which is exactly how `--camera-mode folder` groups. A video input keeps one row: a dual-lens file's `cam0/`+`cam1/` sit under its prefix and are still two camera groups. The rows are built once (`camera_groups` in DatasetPrep), so what the panel draws and what the child is handed cannot drift, and an **empty model on a row means "same as above"** — a dozen clips off one camera are one choice, and the overrides come out fully resolved. The focal is carried as a fraction of the image width and resolved to pixels once the frames exist (`first_image_dims`); an explicit focal typed into Advanced wins for a lone input. Masks reach the run as `--masks <dir>` (or `--no-masks`, so a stale `masks/` beside the images is never picked up silently); `PrepResult::mask_dir_cfg` and `image_dir_cfg` are handed to `GuiApp::open_dataset` in memory, so photos read where they are keep both their image and mask folders when the dataset opens in the trainer. Camera sharing switches to per-folder on its own when `images/` came out with sub-folders. Exit code 3 = reconstructed but partial (reported, not failed). |
| `gui/Subprocess.h/.cpp` | Cross-platform subprocess with merged stdout/stderr line streaming + kill-on-cancel (fork/execvp + process group on POSIX, CreateProcess + pipe on Windows). Bare `\r` ends a log line (ffmpeg/curl progress). `split_args()` splits a typed-in argument string the way a shell would for the simple cases -- a line break separates like any other space and a backslash before one continues the line, so a multi-line command pasted out of a terminal splits into what its author meant -- and `command_argv()` builds an argv from a typed-in command line with a token replaced by `safe_arg(value)` -- the value lands in ONE argument whether or not the user quoted the token, and `safe_arg` leaves nothing in it that a JSON payload or a command line would have to escape (quotes curl to `”` / `’`, a backslash becomes `/`, control characters become spaces). Guarded by `command_argv_test`. `ProcessReader` is the other direction: a child's stdout read as bytes (stderr still line by line), which is how `FfmpegVideo` takes decoded pictures out of ffmpeg. GUI-free, and linked into the CLI for that. |
| `gui/CommandRunner.h/.cpp` | One external command the user asked for, run on a thread because the GUI cannot block on it: the same `drain_log()` shape as the other runners, plus `take_exit_code()`, which reports an outcome once so the caller can say "not found" (`kSpawnFailed`), "exited with code N" or "finished". It knows nothing about what it runs -- the caller hands it an argv. |

The frustum-size control also reaches the web viewer: viewer.html gained a
"Camera Size" slider sending `camera_size_scale` per /render; the render
worker applies it via the new `engine_viewer_set_camera_size()` (Engine.h /
Visualizer.cu), which updates the scale without re-running
`engine_viewer_init` (that would wipe the thumbnail cache).

Verified 2026-07-12 (WSLg, RTX 5070, COLMAP 4.2, driven via synthetic X
input): garden → dataset preview (points + frusta + size slider) → train
400-600 steps (8-24 ms/step incl. live viewport) → orbit/zoom/depth-buffer/
frusta-with-thumbnails → checkpoint saved → Train Again; Home-during-training
confirm modal; video (60-frame test clip) → ffmpeg candidates → sharpness
selection → COLMAP sequential → mapper → BA refine → "Open in Trainer" →
trained; `mask.py` invocation verified against a real lang-sam install
(24 masks). CLI re-verified unchanged after the TrainerCore/RenderWorker
extraction (see below).

## Files

| File | Role |
|---|---|
| `main.cpp` | CLI parsing (`--help`, flag table), stdout progress printing, web-viewer wiring. The engine plumbing lives in TrainerCore (below). |
| `TrainerCore.h/.cpp` | **Shared trainer session** (extracted from main.cpp 2026-07-12, code moved verbatim): color-space resolution, `seed_splats`, `build_step_config`/`build_loss_weights`, `scheduled_lr`, `save_config_json`, and `TrainerSession` = check_config / load_dataset / setup_engine / train(callbacks) / save_checkpoint / progress_json + the engine mutex + pause/stop/render-pending atomics + `make_viewer_config/hooks`. Used by both `spirula train` and the GUI. |
| `RenderWorker.h/.cpp` | **Shared interactive render path** (extracted from Viewer.cpp 2026-07-12): latest-wins worker thread, c2w remap + engine render + display transforms + `engine_blit_view`, returns RGB8; `viewer_upload_cameras()` + `viewer_upload_grid()` (axes/grid overlay axis-aligned in the **engine/saved-splat frame**, drawn when `ViewRequest::show_grid`; `grid_dist` + `grid_target` — nav distance and orbit target in the client's normalized frame, remapped like the c2w — drive the engine's zoom-adaptive cell decade and lattice-snapped patch recentering per render). Buffer keys: distortion buffers are offered/computed **only when a distortion regularizer is configured** — they are full-resolution never-freed pool allocations, so zero reg weights = zero extra VRAM (previously `distortion_reg_on` forced the distortion channels on EVERY viewer render). `sh` and `refinement_score` debug renders (`engine_debug_forward` with a **max_num_splats-sized** DC override — cur-sized throws a tensor-size mismatch; `engine_copy_accum_buffer` col0 for the score). **Pick** (`ViewRequest::pick_px/py` -> `ViewResult::pick_hit/pick_point`): 3D point under a pixel from the already-downloaded ray-depth channel (alpha-unpremultiplied like the blit kernel, alpha > 0.1 to reject background), CV pixel ray via `viewer_pixel_ray` (host generate_ray port, 4 display models) through the remapped c2w, result mapped back to the client's normalized frame. Viewer.cpp adds HTTP+JPEG on top; the GUI viewport uploads to a GL texture. |
| `DatasetParser.h` | Public dataset structs: `DatasetParserConfig`, `ParsedDataset` (per-INPUT cameras), `PostSplitCameras` + `bake_post_split()` (warp expansion), parse fns, shared `dsparse::` helpers. |
| `ColmapParser.cpp` | COLMAP binary + text reader + format auto-detect dispatcher (`parse_dataset`). Auto-detect **identifies** the format from its markers — transforms.json → nerfstudio, a COLMAP model → COLMAP, a `<document><chunk>` camera-export `.xml` → Metashape — then runs that one parser, so the error the user sees is the one for the format they actually have; a directory that matches nothing gets a "does not look like a supported dataset" listing of what was probed (plus the closest partial COLMAP model, if any) instead of whatever the last-tried parser happened to complain about. `--data` is existence/is-directory checked up front. When `colmap_recon_dir` is not set, models under `sparse/*` and `colmap/sparse/*` are enumerated and the one with the most registered images wins (sparse/0 is NOT necessarily the largest; count read cheaply from the images.bin header / images.txt comment). |
| `NerfstudioParser.cpp` | transforms.json reader + self-contained PLY point reader (ascii + binary_little_endian). Split into `parse_nerfstudio_dataset` (reads the file) and `parse_nerfstudio_meta` (consumes a transforms-shaped `JsonValue`; the Metashape front-end feeds this). |
| `MetashapeParser.cpp` | Metashape camera-export `.xml` + `.ply` reader (port of `_parser_metashape_data` + `metashape_utils.py`): sensor intrinsics (`calibration[@class!='initial']`, p1/p2 swap, b1/b2÷f), component transforms, OpenCV→OpenGL flip, camera→image matching (photo-path suffix via the optional `.psx` project's zipped camera table, else label substring). Builds a transforms-shaped meta → `parse_nerfstudio_meta`. |
| `DatasetCommon.cpp` | Shared bakes: normalized-frame scale, eval/val splits, aux-file discovery, geometric-median outlier filter. Also built into the WebAssembly viewer. |
| `PostSplit.cpp` | **`bake_post_split`**: the POST-split camera arrays, one pinhole face per K from `camhost::plan_split_faces` (`data/CameraMath.h`) -- each face cropped to what the lens holds, see docs/datasets.md "The split". |
| `Json.h` | Minimal dependency-free JSON parser (handles Python `Infinity`/`NaN`). |
| `Xml.h` | Minimal dependency-free XML parser (ElementTree subset: `attr`/`find`/`findall`/recursive `iter`; comments/PI/CDATA/DOCTYPE skipped, standard entities). Metashape nests `<camera>` in `<group>` and `<camera_ids>` in `<partition>` — the recursive `iter` is load-bearing. |
| `Knn.h` | Exact kd-tree kNN (multi-threaded queries) for `seed_splats` scale init; replaced the hash-grid approx that degenerated to O(N²) on SfM outliers. |
| `FrameMotion.h/.cpp` | How fast a video's view is changing, and the frame spacing that follows. A grid of ~500 points tracked between small grey frames by pyramidal Lucas-Kanade, one global model fitted by RANSAC -- a 2D affine for an ordinary video, a rotation of the SPHERE for a `.360` or a dual fisheye -- and two numbers out: how much of the view left it (0 on a sphere, which is what makes turning a 360 camera free) and how much of the flow the model could not explain (parallax, which is what a walk past something close produces). `plan_by_motion` then spaces the kept frames at equal cumulative cost, bisecting for the step that lands on the wanted count -- over SEVERAL videos at once when they share a rate, so the clip that moves more takes more of the budget. Portable C++, no device: it runs in front of both of `FrameExtract`'s decoders (from a grey frame the GPU reduces, or the same box over ffmpeg's Y plane on the host) and behind the GUI's ffmpeg path (`gui/FrameSelect`, from the candidate JPEGs). `docs/datasets.md` "Frames out of a video" has the weights and why. |
| `FrameExtract.h/.cpp` | Video in, sharp (optionally masked) frames out, for `spirula sam extract` and the GUI alike: which decoder runs, the pick of each window, masking, and the writers for one track, a synced rig (`cam0/`, `cam1/` at the same instants) and a 360 file's views. Always built with SAM; everything under `SS_HAVE_VIDEO` in the header is the in-process decoder's own. |
| `FrameDecode.h`, `FrameDecodeVulkan.cpp`, `FrameDecodeFfmpeg.cpp` | The two decoders under `FrameExtract`, one `SelectClock` between them. Vulkan Video is patent-gated (`SS_ENABLE_PATENTED`); ffmpeg is one child per track writing PPM to a pipe, read in lockstep, a fixed rate dropped by ffmpeg's `select` before the colour conversion. `--decoder auto` takes the first when the build and device have it (MoltenVK has no video queue) and falls back to the second. Both name files by source frame index, and the ffmpeg motion pass reproduces `video.slang`'s grey thumbnail from the Y plane on the host, so the two make the same adaptive plan -- see `docs/datasets.md` "Frames out of a video". |
| `FfmpegVideo.h/.cpp` | ffmpeg as a decoder for any tool without the in-process one: `ffmpeg_probe_video` (what `ffmpeg -i` says: tracks, their sizes and stream indices, rate, duration) and `FfmpegFrames`, pictures over a pipe. |
| `FrameSharpness.h/.cpp` | The variance of the 3x3 Laplacian of the mean-subtracted 512^2 grey image, which both ffmpeg paths rank frames by; `video.slang` holds the GPU copy. |
| `Pano360.h/.cpp` | A 360 camera's own frame layout and the views a dataset wants out of it. Two packings, and the camera's own `PRJT`/`PMOD` is what tells them apart because the frame sizes do not: a **GoPro MAX** `.360` is one 3x2 EAC cubemap across both tracks with a 32 px lens-seam overlap at the centre of each track's side faces, unwrapped into ten seam-free perspective views (five per lens, so that no view spans both) or one 2:1 panorama; a **MAX 2** writes a whole stitched equirectangular panorama per track, side-padded to 5952x1920, and gets a six-face cube stood on a corner (no face centred on a pole, where a panorama is a starburst) or the panorama itself. The resampler that applies either is the same. **Both decode paths go through this one implementation** -- the in-process decoder hands it two decoded tracks, and the ffmpeg fallback runs a filter graph generated from the same layout that only crops and stacks. ffmpeg is never asked to warp: its own `v360=eac` insets every face by 2 px, which puts a 4 px step across the seam between the two tracks. The canvases the two paths build are bit-identical. `docs/datasets.md` records where the layout numbers were measured. |
| `WriterPool.h` | Bounded-queue worker threads that JPEG/PNG-encode and write frames and masks off the calling thread. Used by `FrameExtract`, `spirula sam track` and the GUI's folder-masking loop. Encoding a 1080p mask through stb's deflate is ~75 ms — a third of a SAM 2.1 Tiny frame — and none of it needs the GPU, so a caller that writes inline sets the frame rate with zlib. The queue bound is what keeps a slow disk applying back-pressure instead of growing until memory runs out. |
| `HttpServer.h/.cpp` | Minimal HTTP/1.0 GET server (POSIX sockets; winsock shim compiles but untested). Serial request handling — parity with Python's non-threading `HTTPServer`. |
| `Viewer.h/.cpp` | Web-viewer server: latest-wins render worker, the viewer buffer subset, `engine_blit_view` GPU annotation/colormap, stb JPEG encode. Serves the **unchanged** `viewer.html` (embedded at configure time via CMake hex; `SS_VIEWER_HTML=<path>` env overrides for dev). `/pick?px=&py=&<camera params>` returns the 3D point under a pixel as JSON for viewer.html's double-click centering (the client treats non-OK responses as a no-op). |
| `../config/TrainConfig.h` | Hand-written, the training config's single source of truth: the `SS_CONFIG_FIELDS(X)` X-macro flag table (184 rows), `struct TrainConfig` expanded from it, `SS_DATASET_PARSE_FIELDS`, `kTrainPresets` + `train_apply_preset()`, and `train_resolve_macros()`. |
| `../external/` | All vendored third-party code (marked `linguist-vendored` in `.gitattributes` along with the generated dirs): `stb_image.h`/`stb_image_write.h` (images), `npy.hpp` (checkpoints), `miniz.c/.h` (zip reading for the Metashape `.psx` camera table; compiled into `spirula` only). |

Debug: `SS_DUMP_CAMERAS=<path> spirula train ...` dumps parsed + post-split
camera arrays as JSON and exits before engine setup — used to diff against
the Python dataparser/trainer algebra (see verification notes below).

## Mesh extraction (`spirula-mesh`)

Drives `Meshing.h` / `MeshingHost.cpp` / `MeshUV.cpp` / `MeshExport.cpp`. Built
with the rest of the command-line tools; `mesh_main.cpp` + the dataset parsers,
no HTTP/viewer.

```bash
./build_vulkan/spirula-mesh <ckpt> [--data <dir>] [--format ply,obj,gltf,glb,stl] \
    [--color none,vertex,texture] [--texture-size 0] [--flag value ...]
```

- `<ckpt>` = run dir (config.json + `step-*.ckpt/`), a `*.ckpt` dir, or a
  `splat.ply` directly. `--data` defaults to config.json's `data`; the
  dataparser settings (image_dir, recon dir, metashape paths, numeric
  rescale) and `model.relative_scale` are honored from config.json.
- Color modes: `none`, `vertex` (per-vertex RGB), `texture` (LSCM UV atlas +
  baked image). Both flags take a LIST, and one run writes every pair of them
  a format can carry -- the surface is extracted once and only the color is
  redone, so a textured GLB to look at and a plain STL to print cost one run.
  A pair no format can carry (PLY or STL with a texture, OBJ or STL with
  vertex color) is skipped with a line saying so, and an empty result is an
  error up front. With more than one color the outputs are suffixed
  (`mesh_nocolor.ply`, `mesh_vertexcolor.ply`, `mesh_textured.glb`); with one,
  the names are unchanged.
- With `--color texture`, a format token may carry the texture encoding:
  `glb+png` (default), `glb+jpg` (JPEG q95), `glb+jpeg75` (JPEG q75) — works
  for obj/gltf/glb; JPEG is part of core glTF (`image/jpeg`).
- Output: one file per `--format` next to the checkpoint's splat.ply
  (`mesh.ply` / `.obj`+`.mtl`+tex / `.gltf`+`.bin`(+tex) / `.glb` / `.stl`), all
  emitted dependency-free (PNG/JPEG via vendored stb_image_write). `.glb`
  embeds the texture; validated clean with gltf_validator.
- glTF compatibility: plain PBR material (no KHR_materials_unlit — partially
  supporting viewers rendered it solid white), uint16 indices when they fit,
  float VEC3 COLOR_0 — matching the Khronos sample-model conventions.
- `splat.ply` reader expects float32 binary-little-endian properties (what
  both the Python trainer and `EngineCheckpoint.cpp` write).

## The config table (source of truth = `src/config/TrainConfig.h`)

Hand-written, one `SS_CONFIG_FIELDS` row per flag:
`X(type, member, default, section, tier, choices)`. `struct TrainConfig` is
expanded from the same table, so the declaration and the metadata cannot
drift. Add a row and the flag appears in the CLI parser, `--help`, the GUI's
"All Options" editor and `config.json`.

What the row does *not* carry is what the flag is called or what it does in
words: that text is translated, so it lives in `../i18n/catalog/TrainFields.h`
as `SS_MSG(<member>, ...)` and `SS_MSG(<member>_help, ...)`. Consumers paste
the two together while expanding the table (`fld::member##_help`), so a row
with no entry there is a compile error naming the flag.

- **Flag names**: `member` stringified. `-` and `_` are interchangeable, so
  `--sh-degree` sets `sh_degree`. A flag cannot drift from its member.
- **`config.json` keys**: the flag name, at the top level — the file is
  flat. It used to nest under `group`, which quietly made a presentational
  choice part of an on-disk format: move a flag to another heading and its
  key moved with it, and the reader (`spirula mesh`, `--resume`) fell back to
  the default without saying so. `section` and `tier` never reach disk.
- **`section` / `tier`**: which heading a flag is listed under, and how
  specialist it is (`basic` / `advanced` / `expert` / `stub`). `--help` shows
  the basic ones and points at `--help-all`; the GUI has the same filter as a
  dropdown. Rows must stay contiguous per section — both consumers stream
  headings as they walk the table.
- **Macro options**: `quality`, `floater_suppression`,
  `distraction_robustness` are ordinary rows that stand in for several
  specialist flags each. `train_resolve_macros()` applies them after the
  preset and never over a flag the user set by hand; a macro at its default
  writes nothing. The CLI passes its `seen` set, the GUI its
  `ConfigUIState::touched`.
- **Commas**: macro arguments split on them, so `std::array<T, N>` fields use
  the `TrainVec3i` / `TrainVec3f` aliases and the `train_v3i()` /
  `train_v3f()` makers.

The table is hand-written. It was generated from Python dataclasses until
2026-08-04; the migration was verified byte-identical across `train --help`
for all 7 presets and a run's `config.json`.

## Where the training driver lives

`TrainerCore.cpp` is the whole training driver: `scheduled_lr()`,
`build_loss_weights()`, `seed_splats()` and `build_step_config()`, plus the
loop and checkpoint save. The warp expansion is `DatasetCommon.cpp`'s
`bake_post_split()`; the dataset readers are `data/parsers/`.

This section used to be a two-column table mapping each of those to the
Python function it was ported from, because for a while both ran and had to
agree. Only the C++ exists now, so the table said nothing the code does not.

## Verified working

- All presets train and converge (banana COLMAP set — 3dgs: PSNR 26.7 @ 500
  steps); hdr exercises color-space init + trust region; meshing exercises
  3dgut + median losses; academic-baseline exercises interval eval split +
  non-FPBO + fp32.
- **360-camera preset works end-to-end** (SharkWipf_SampleDataset, 400×3840²
  nerfstudio fisheye + masks): 5-face warp (400→2000 post cameras), warped
  GT upload, synthetic FOV masks, bilagrid/PPISP at n_post slots. Behavior
  parity vs Python trainer at 300 steps: PSNR 17.7 vs 17.1, SSIM 0.775 vs
  0.737, same ~72 ms/step (deltas are RNG-level: seeding/batch order).
- Numeric verification (2026-07-10): C++ `SS_DUMP_CAMERAS` dump vs
  Python — nerfstudio c2w/points/order/train_frame_scale AND the full warp
  expansion (K, offsets, 2000×viewmats/intrins/dist, input intrins) all
  match to float32 precision.
- **Metashape parser** (2026-07-10, dye_alley 732-frame dual-fisheye rig):
  all 732 cameras match the Python metashape path to float32 precision
  (c2w, intrins, dist incl. p1/p2 swap + b1/b2 scaling, points, order,
  train_frame_scale); `.psx` camera-table disambiguation exercised (labels
  alone are ambiguous across `cam1`/`cam2`); trains end-to-end and
  auto-detect falls through COLMAP→Metashape correctly. Deliberate
  deviation: with multiple components we train on the **largest** (Python's
  reversed-sort quirk picks the smallest).
- Checkpoint cadence + `save_only_latest_checkpoint` pruning + final save.
- **Web viewer** (2026-07-10, SharkWipf run): browser client unchanged; `/`,
  `/render` (rgb / depth / alpha / depth_normal / distortion buffers, JPEG,
  training-camera frusta + thumbnails via `engine_blit_view`), `/buffers`,
  `/progress`, `/pause-toggle` all exercised live during training —
  step latency stayed ~72 ms/step with concurrent renders, pause froze and
  resumed the loop, `keep_viewer_alive` serves after training completes.
  Headless-friendly: `ssh -L 7007:localhost:7007 <box>`.

## ⚠️ Gotchas

- **libtorch interposes `std::filesystem`**: any exe linked against `csrc`
  binds `std::filesystem::remove_all` to libtorch.so's ABI-incompatible copy
  → segfault (jump to null). `-static-libstdc++` does NOT fix it (torch libs
  precede the archive on the link line). Checkpoint pruning uses POSIX
  `nftw` instead. Other fs calls (create_directories, directory_iterator,
  exists) currently bind compatibly but carry the same risk until no-torch.
- `libpython` is linked only because csrc's pybind layer leaves CPython
  symbols undefined; drops out with no-torch.
- Engine auto-resolves `split_batch`+FPBO conflicts via
  `max_input_batch_size` (prints a warning) — pass both through as Python does.
- **Pre-existing**: the process can dump core during exit teardown after a
  completed run (observed on both pre- and post-refactor `spirula train`
  2026-07-12; likely a DataManager/engine thread racing static destruction).
  Harmless — all output is already on disk — but worth fixing before
  packaging (Phase 3).

## TODOs (rough priority)

1. **Eval pass + metrics** — iterate `next_val_batch` / render train views,
   PSNR/SSIM from engine buffers; then `validation_fraction` early-stop (the
   `overfit_score_*` / `early_stop_*` fields were parsed but unused and have
   been removed; re-add them when the pass lands).
2. **Resume** — `engine_load_checkpoint` after skeleton setup; config.json
   round-trip.
3. ~~ImGui native viewport (Phase 2)~~ DONE 2026-07-12 (the GUI, see
   above). Still open within it: CUDA-GL interop upload (currently D2H +
   glTexImage2D — fine at viewport sizes), debug-only `sh` /
   `refinement_score` buffers (engine_debug_forward) not ported, COLMAP
   progress bar is stage-based only, no mesh-export UI (use `spirula-mesh`).
4. Seeding fidelity: jitter repeated seed points toward a neighbor instead of
   exact duplication; `suppress_initial_scales`. (Exact kNN: DONE, `Knn.h`.)
5. COLMAP **text** format fallback. (Metashape parser: DONE,
   `MetashapeParser.cpp`. Camera-to-image resolution fit: DONE,
   `dsparse::fit_camera_resolution`.)
6. Non-default orientation/center methods (`pca`/`vertical`/`gsplat`/`focus`)
   — currently approximated as `up`/`poses` with a warning (only affects
   `train_frame_scale`).
7. Windows: MSVC+nvcc build DONE (2026-07-10, VS2022 + CUDA 12.8 on an
   RTX 3090: no-torch static build links `spirula.exe` and trains
   mipnerf360/garden; only source fix needed was an MSVC branch for a GCC
   atomic builtin in `MeshingHost.cpp`). Remaining: `cudart_static`, CI,
   installer (Phase 3).

## Resume

`--resume <run_dir|step-*.ckpt>` is native (`src/checkpoint/Resume.h`). The
checkpoint's `config.json` is the base config — it carries the architecture
the saved state was built for, so `--data` is not needed on a resume — with
the resume path, an output dir defaulting back to the checkpoint's own run
folder, a preset named on the command line, and every explicitly-passed flag
layered on in that order. The restore itself runs at the end of
`setup_engine()`, after the world is seeded at `max_num_splats` and the
appearance channels exist as restore targets.

A checkpoint saved without `--save-full-checkpoint` holds no world/optimizer
state and is refused up front, before any loading.

Resuming into a **different layout** — smaller `cap_max`, a different
`sh_degree`, bilagrid/PPISP added, dropped or resized — is handled by
`src/checkpoint/Adapt.h`, which rewrites the checkpoint's buffers to the
target layout on the host and hands the engine an ordinary `state.tar`. It
runs buffer-at-a-time and allocates no VRAM, because the motivating case is
resuming a run that just ran out of it. Splat reduction drops the tail by up
to the checkpoint's unsaturated slack, then the lowest-opacity remainder.
Quantized buffers are decoded and re-encoded through the engine's own codecs
in `core/Tensor.h` — those are `__device__` functions, but `__device__` is an
empty macro in host translation units, so the host path calls the same code
the kernels do rather than a copy of it.

## Warm start

`--init-ply <file.ply|run_dir|step-*.ckpt>` seeds the run from an
already-trained 3DGS PLY instead of the dataset's point cloud
(`seed_splats_from_ply()` in `TrainerCore.cpp`). Everything the file carries —
means, scales, rotations, opacities, DC and SH — goes straight into
`set_data_3dgs()`; only the layout is re-fitted to this run. Splats past
`cap_max` are dropped by lowest opacity, the SH past the DC is truncated or
zero-padded to `sh_degree`, `relative_scale` scales the means and shifts the
log-scales, and the DC converts from the `point_color_*` space to the splat
space where the two differ (the view-dependent terms ride an encoded curve and
cannot follow, so they come across as they are).

`--init-ply-add-points` adds the dataset's point cloud **on top** rather than
replacing it: the PLY goes in first and `append_point_seeds()` fills what is
left up to `cap_max` with an ordinary `seed_splats()` over the cloud. It is the
flag for a dataset that has grown since that model was trained; where the two
overlap the extra splats are redundant and refinement prunes them.

This is **not** a resume: the step counter starts at 0, the optimizer starts
empty and the refinement schedule runs from the beginning. The PLY has to be in
this run's training frame — same dataset, same scene settings
(`orientation_method`, `center_method`, `auto_scale_poses`, `train_frame`,
`relative_scale`) — because nothing registers it. When both are given
`--resume` wins with a warning, since a resumed run's `config.json` carries the
original `--init-ply` too.

## Eval

Runs after training when `eval_mode != "all"` and the eval split is non-empty.
The dataset is re-parsed with `split = "eval"` (the parser computes the split
over all frames, so this is the exact complement of what training saw) and the
engine's DataManager is replaced with one over it — which is why eval is last
and nothing may train afterwards. Each view goes through `engine_eval_forward`:
the same decode, mask and fisheye/equirect warp path training uses, then a
forward with no loss or backward. Eval GT is therefore the warped GT, not a
host-side reconstruction of it.

`src/app/EvalMetrics.{h,cpp}` scores each view: `l1`, `psnr`, `ssim`, and the
`cc_` variants on the colour-corrected render. Results go to `metrics.json` as
per-image lists plus `avg_*` scalars. Verified against torchmetrics on 13 real
eval pairs: l1 and psnr agree to 1e-7, ssim to 1e-5. `color_correct` is closer
to a float64 reference than the torch implementation it replaces (3e-8 vs
1.4e-3 relative) — torch accumulates the normal equations in float32.

Two things worth knowing about the SSIM: torchmetrics reflect-pads and
averages over the **full** H×W, cropping the border back off only on its
`return_contrast_sensitivity` path. A valid-interior SSIM is ~0.5% different,
which is enough to change a benchmark comparison.

Rendering is serial (one process-global engine), but scoring a view is pure
host work, so it runs on a small pool while the GPU gets on with the next view
— results land in a slot indexed by view, so `metrics.json` does not depend on
who finished first. The pool is capped by cores *and* by a memory budget: at
4K each worker holds ~725 MB of image and SSIM scratch. That scratch is reused
across views rather than reallocated (see the AGENTS.md gotcha); at 4K the
faulting otherwise costs more than the arithmetic.

The metrics themselves are memory-bandwidth-bound at 4K, so the pool mostly
buys overlap with PNG encoding rather than raw metric throughput — 30 views of
a 3115×2076 scene take ~90 s of scoring however the threads are arranged, and
`--save-eval-images` adds ~35 s on top instead of ~115 s.

LPIPS is not native. `--save-eval-images 1` writes `eval-gt-NNNNN.png` and
`eval-render-NNNNN.png` per view; `reference/python/eval_lpips.py` reads those
and merges `lpips_*` into `metrics.json`.

## Unsupported-by-design (guarded with clear errors)

`--use-bvh`, `--use-camera-optimizer`,
`--deblur-training-images`, `--optimizer-offload`,
`--cache-images gpu`,
`--train-frame` ≠ points,
direct-equirect (`--warp-spherical-to-pinhole 0`) with depth/normal
supervision. `--num-downscales` warns and is ignored (Python-data-path
feature).
