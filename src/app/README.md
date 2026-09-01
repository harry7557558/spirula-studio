# The `spirula` application: CLI trainer (`spirula train`) + native GUI

Working notes for the no-Python trainer under `src/app/`. Written 2026-07-10,
GUI added 2026-07-12; covers code structure, verification status, and
remaining ports. Long-term context: Phase 1 (standalone C++ trainer) and
Phase 2 (Dear ImGui/GLFW GUI) of the GUI-app plan are done; Phase 3 is
Windows/Linux packaging; kernels stay in Slang for the eventual
Vulkan/cross-vendor port.

## Build & run

```bash
cmake -G Ninja -B build -DSS_BUILD_CLI=ON && cmake --build build --target spirula
./build/spirula train [<preset>] --data <colmap_dataset_dir> [--flag value ...]
```

- Presets = tyro subcommands: `3dgs` (default), `360-camera`, `in-the-wild`,
  `linear-color`, `synthetic`, `meshing`, `academic-baseline`.
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
system libs), `SS_BUILD_CLI` is forced ON, CUDA archs come from
`nvidia-smi --query-gpu=compute_cap` (override with `-DTORCH_CUDA_ARCH_LIST`),
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
Off by default; enable with:

```bash
cmake -G Ninja -B build -DSS_BUILD_GUI=ON   # composes with -DSS_NO_TORCH=ON
cmake --build build --target spirula
```

GLFW 3.4 and imgui v1.92.8 are pinned and fetched at configure time via
FetchContent (network needed once; on WSL drvfs mounts git may need
`safe.directory` entries for `build/_deps/{glfw,imgui}-src`). Only system dep
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
| `gui/GuiMain.cpp` | GLFW window + GL 3.2 core context + ImGui bootstrap, dark style, DPI scale, frame loop, close-confirm flow. **Drag-and-drop** (glfwSetDropCallback → `GuiApp::handle_drop`, which takes the **whole drop at once** — several videos dropped together are the inputs of one dataset, and `spirula <path>...` goes through the same call): each path is auto-detected as an SfM dataset folder (transforms.json / sparse/ / colmap/ marker, or a Metashape camera .xml + point-cloud .ply pair → open; only honoured when dropped alone), a photo folder (contains images), or a video file (extension; the .insv preset applies per file). Files from inside a dataset (transforms.json, .db/.bin/.txt/.xml) open their parent. A **preset** — a saved one or a run's `config.json`, recognised by content rather than by name — is applied to the trainer screen instead (refused while training, since applying one re-parses the dataset). Dataset folders dropped onto the Batch screen are appended to the queue. Raw input dropped onto the dataset screen is **added** to the list there; dropped anywhere else it starts a new dataset. Video/photo drops are ignored while training (datasets go through the stop-confirm flow). |
| `gui/GuiApp.h/.cpp` | Screens (Home / COLMAP / Train / Viewer / Batch), layout, wiring; recents + tool paths persisted to `~/.config/spirula-studio/gui.conf` (`%APPDATA%` on Windows). Session-destroying navigation (Home / open-dataset / quit during training) goes through a stop-and-save confirm modal with a deferred pending-action; output folder defaults to `<dataset>/outputs` with a Browse button + resolved-run-path preview. Editing any dataset-parsing option (`SS_DATASET_PARSE_FIELDS` via the generated `parse_settings_equal`) marks the dataset dirty and auto-reloads it once the edited widget loses focus. NOTE: `open_dataset`/`add_recent` take `std::string` **by value** — callers pass `_recents` elements and `add_recent` mutates that vector (a const& dangles; this was a real bug that corrupted the path to ""). |
| `gui/ConfigUI.h/.cpp` | The X-macro-generated options editor. Zero per-field special cases (only `data` is hidden, managed by the dataset picker). |
| `gui/ViewportPanel.h/.cpp` | Native viewport with two backends behind the browser-identical NavCamera navigation: **Preview** = GL point cloud + frusta as soon as the dataset parses (PreviewRenderer), **Engine** = RenderWorker once training starts, with all four web-viewer camera models (Pinhole / Fisheye-equidistant / Fisheye-equisolid / Equirectangular; `fovToIntrinsics` + per-model FOV ranges ported from viewer.html). Buffer picker, camera-frusta overlay with a live **frustum-size slider** (`ViewRequest::cam_size_scale`), render-scale + live-refresh throttle (0.15 s). Initial framing: seed-point centroid target, median camera distance, camera-centroid direction. **Double-click centering** (viewer.html `recenterAt`): pan laterally so the 3D point under the cursor sits on the optical axis and make it the orbit pivot (rotate toward it when behind the camera plane, >180° models); preview mode picks the nearest displayed point along the cursor ray (3% angular cone, `PreviewRenderer::pick_point`), engine mode attaches `pick_px/py` to the next render and gets the point back in the ViewResult — depth-channel readback, no extra VRAM or render pass; all four display camera models via `viewer_pixel_ray`. |
| `gui/CompareView.h/.cpp` + `gui/SplatViewer.h/.cpp` | **Up to four finished models under one camera** — the viewer screen and the meshing preview are both this. SplatViewer loads a file (3DGS PLY / point cloud / mesh, auto-detected) on a worker thread; splats land in an **engine scene slot** (`engine_scene_set_data_3dgs` / `engine_scene_activate`, Engine.h) so several stay resident and the RenderWorker binds one per render — a pointer swap instead of a per-pane re-upload. CompareView lays the panes out (1 / 2 / 3 / 2x2 by count), owns the ONE engine mutex every pane's worker takes, links the navigation, and gives each pane a similarity from its model's own normalized frame into the shared one — applied to the CAMERA, not the geometry, so the per-model position / rotation / size controls are free. That similarity defaults to "sit in the first model's frame", which is what aligns two reconstructions of one scene exactly and what places a mesh in its splats' frame. Design: `docs/notes/compare-view.md`. |
| `gui/NavCamera.h/.cpp` | 1:1 port of viewer.html's `cam`/`quat`/`Nav`: quaternion camera, four modes (Turntable / Trackball / First Person / Free Fly), same sensitivities and mappings for **mouse** (LMB orbit-or-look, RMB/MMB/Shift pan, wheel dolly), **keyboard** (WASD/arrows, E/Q up-down or Fly-roll, active while the pointer is over the viewport), **gamepad** (GLFW gamepad API: left stick move, right stick look, triggers up-down/roll -- including the browser quirk that triggers only translate while the left stick is deflected), and **touch** via the OS's pointer/gesture emulation (single finger = orbit; system pinch/pan gestures arrive as wheel; GLFW exposes no raw multitouch). Keep in sync with viewer.html's Nav. The initial pose replicates `cam.reset()` verbatim (target = client-frame origin = the CAMERA-POSE center via center_method="poses", pos=[0,0,1], orbit(0,-250)) -- verified pixel-equivalent against a `/render` fetch from `spirula train`'s web viewer at the client's default c2w. `SS_NAV_DEBUG=1` logs per-frame mouse-drag nav decisions (button/pan/target/pos) to stderr. |
| `gui/PreviewRenderer.h/.cpp` + `gui/GlLoader.h/.cpp` | Offscreen-FBO GL renderer for the dataset preview (sparse points, vertex-colored + per-input camera frusta as center+offset lines scaled by a uniform). Frustum wireframes are built from each camera's TRUE model + distortion via `frustum_template` — a C++ port of `viewer/js/dataset.js` `frustumTemplate`/`generateRay`/`undistort` (itself a port of `fill_frustum_segments_kernel` / `projection_utils.cuh`): pinhole = classic border pyramid, wide models (fisheye/equisolid/equirect) = dome/globe with dimmed interior gridlines, Newton undistortion with the `is_valid_distortion` bail-out + bisect-toward-center for out-of-domain border pixels. (Before this, frusta were tan-based pinhole pyramids — a ~200° THIN_PRISM_FISHEYE dataset drew as giant crisscrossing triangles.) The display-side vertex shader implements the same camera models as the engine viewer (pinhole / fisheye-equidistant / equisolid / equirectangular; `u_s` = fx/(W/2), fy/(H/2), linear view-distance depth), so the preview -> engine transition at training start keeps pose AND intrinsics with no jump (ViewportPanel `maybe_frame` re-frames only when the dataset identity changes; navigation done in the preview carries into training, verified pixel-aligned at fisheye 333°). The projection function is shared with the fragment shader, which **re-projects the interpolated view-space position and discards fragments whose reprojection error exceeds 5% of the viewport** — the web viewer LINE_FS seam fix, killing the full-width streaks that segments crossing the equirect ±180° seam / fisheye backward point / pinhole near plane used to rasterize as. That check alone still keeps ~5%-viewport stubs at both ends of a wrapped segment (the error vanishes at the endpoints), which stack into ladder artifacts at the equirect edges — so line vertices also carry `a_delta` (this-endpoint − other-endpoint, pre-scale) and the vertex shader kills the WHOLE segment when its chord crosses the equirect seam half-plane (x=0, z>0 view space); same mechanism as the web viewer's `aDelta`/`vKill`. Optional **grid + axes** (viewport "grid" checkbox): power-of-10 ground grid + colored positive half-axes generated in the **training (saved-splat) frame** — lines mark round coordinates of the exported model — then mapped into the preview's normalized frame; the cell decade adapts to the nav distance and the finite line patch recenters on the orbit target (lattice-snapped, with hysteresis), cell edges subdivided 4x so they curve correctly under the non-pinhole projections; depth-tested like all preview geometry (`ensure_grid`). GlLoader = minimal namespaced (`glx::`) proc loader via glfwGetProcAddress; no GLEW/GLAD. |
| `gui/TrainRunner.h/.cpp` | TrainerSession on a worker thread: phases Idle→Loading→Ready→Preparing→Training→Done, pause/stop, metric history for plots, optional ViewerServer. Lifetime rule: viewport detaches before the session is replaced. |
| `gui/TrainPreset.h/.cpp` | **Saved presets**: the whole training config under a user-chosen name, one JSON file (`<config_dir>/presets/*.json` by default, any path allowed). Encoded with `config/TrainConfigJson.h`, so a missing key keeps the field's default and an unknown key is ignored — a preset survives both directions of version skew. A run's own `config.json` loads as a preset too, which is the likeliest reason anyone wants the feature. The file also carries `touched`, the set of flags the user set by hand: without it the macro options (`--quality` and friends) would undo the tuning the moment it was loaded, and for a `config.json` (which has no such list) it is derived as "everything that differs from the base preset". `SS_PRESET_CONTEXT_FIELDS` — `data`, `resume`, `output_dir_prefix`, `output_dir_name` — is dropped on save inside `save_preset()` rather than at the call site, so no caller can bake a dataset path into a shared preset. Every place a preset is named (both combos, open or closed, and every row in them) hovers to a tooltip with its description **and its path** — two presets may share a name, and the path is the only thing on screen that tells them apart. `delete_preset()` removes the file behind a confirmation naming both; it refuses any path `is_preset_file()` does not accept, so nothing else can be deleted through it, and the options on screen are left alone (what went is the saved copy, not the work). |
| `gui/BatchTrain.h/.cpp` | **Batch training**: a list of (dataset, preset, output folder) rows, the pre-flight that checks them, and the config each one resolves to. The list is data, not a runner — the queue is driven by `GuiApp::advance_batch()` in a handful of lines, so every row is an ordinary `TrainRunner` session with the live viewport, plots and log the Start button gives. A failing row is just `Phase::TrainError`, recorded on the row while the next one starts. `batch_check()` is the "warn before anything starts" half: a folder with no reconstruction in it, a preset file that has gone missing, a flag `train_config_unsupported()` refuses, two rows on the same dataset, an output path that cannot be created. Fatal issues block the start (with a "skip the bad rows" way past); the rest are said out loud. Two rows are "duplicates" only when they would do the **same work** (`same_work`: dataset + preset + overrides) — running one capture through several presets, or sweeping the splat budget, is what a batch is for and gets no warning. Three per-row columns override the preset's **max splats / SH degree / steps**, so the common sweeps need no near-identical preset per combination; they are stored and edited as **text** because "unset" and "0" are different answers and an integer box cannot spell the first (0 is a legal `--sh-degree`), the pre-flight rejects anything that is not a usable number, and a number typed in joins the `touched` set so `--quality` cannot overwrite it. Rows are added by picker, by drag-and-drop, or from the **recents** the trainer screen already knows (`add_batch_row`, which seeds each row with the preset currently selected). The list itself is kept in `<config_dir>/batch.json` so a queue survives a restart. |
| `gui/ColmapRunner.h/.cpp` | images/video → COLMAP dataset. Requires **COLMAP >= 4.x** (version probed from `colmap help`; 3.x-era `use_gpu` flags are gone — flags follow `reference/scripts/run_colmap.bash`): feature_extractor (**SIFT or ALIKED** via `FeatureExtraction.type`; camera model = any the parser supports; single camera / per-folder / per-image; optional **initial focal length** — `init_focal_factor` × width probed via `stbi_info`, composed into `ImageReader.camera_params` with centered principal point + zero distortion, or a raw `camera_params` string) → **explicitly chosen** exhaustive / sequential / vocab-tree matcher (no "auto"; the GUI presets sequential for video, exhaustive for photos). Sequential supports overlap + **quadratic overlap** (on by default) + **loop closure** (`SequentialMatching.loop_detection` via the vocab tree — SIFT only; the tree is auto-found near the workspace/cache or downloaded via curl into `~/.cache/spirula-studio`). **LightGlue** matching (`FeatureMatching.type` `SIFT_LIGHTGLUE`/`ALIKED_LIGHTGLUE`, default for ALIKED) → mapper (`ba_use_gpu` — **forced OFF for fisheye models**, COLMAP's GPU BA doesn't support them; `Mapper.ba_refine_extra_params 0` for perspective models by default — distortion held fixed for stability and recovered in the final BA, per run_colmap.bash's advice; `min_num_matches`) → best-effort **model_merger** when the mapper splits (kept only if the merged model registers more images; written as the next `sparse/<N>` — on a real X5 capture this fused 86+39 partials into 116/118 frames) → optional bundle_adjuster refinement **on the largest/merged model** (was hardcoded `sparse/0`), with a **verify-and-revert guard**: mean reprojection error before/after via `model_analyzer`, refinement discarded when worse/non-finite (releasing pp + 8 thin-prism coefficients on a ~200° fisheye reliably diverges to ~1e150 px — pp additionally stays fixed for fisheye). `.insv` preset: THIN_PRISM_FISHEYE, per-folder cameras, focal factor 0.269 (Insta360 X5: fx=fy≈0.269·width), **exhaustive** matcher (the two lens tracks are concatenated, so sequential misses cross-lens pairs: 116/118 exhaustive vs 68/118 sequential+loop on the same capture). **Resume** (`ColmapJob::resume`, GUI checkbox appears when the workspace holds a previous run): extracted frames are kept per track, mask.py resumes on its own, COLMAP skips features/matches already in database.db, and existing `sparse/<N>` models skip the mapper (the mapper only writes models on completion, so partials are never trusted); with resume off a non-empty workspace is refused rather than mixed into. A measured resume replay of a full .insv run took 39 s vs 246 s. **Repetitive-scene knobs** (Advanced → "Repetitive scenes", with an Off/Low/Medium/High preset combo; all 0 = default): `SiftMatching.max_ratio`, `TwoViewGeometry.min_num_inliers`, `Mapper.abs_pose_min_num_inliers` / `abs_pose_min_inlier_ratio` / `abs_pose_max_error` — stricter matching + registration so similar-looking rooms don't weld together. The GUI's default workspace auto-suffixes `_2`, `_3`, ... instead of pointing at an existing non-empty folder. Optional **AI masking**: the CMake-embedded `reference/scripts/mask.py` runs via external Python (lang-segment-anything / SAM-3), masks feed `ImageReader.mask_path` + the trainer's masks dir; missing-package output is detected and surfaced with install instructions. Photo-folder inputs are indexed **in place**, recursively (no copy) — the absolute image dir is handed to the GUI in-memory for the immediate open; no marker file is written, so on later re-opens set `data.image_dir` in the dataparser options (video datasets use the default `images/`). Multi-track `.insv` videos split into `images/cam<N>/` + per-folder cameras. The prep stage is shared, so a COLMAP run can take several inputs too — but `feature_extractor` fits ONE camera model to the whole run, so the per-input lens models only mean anything on the built-in path (the panel says so). The mapper may emit several partial models — the trainers auto-pick the largest (below). |
| `gui/FrameSelect.h/.cpp` | Blur-aware video frame selection (multithreaded C++ port of `reference/scripts/extract_frames.py`): ffmpeg extracts candidates at (target fps x sharpness window) with `-nostdin` (**without it ffmpeg can hang reading stdin** — the "stuck at ffmpeg" bug), then the sharpest per window is kept (variance of 3x3 Laplacian on mean-subtracted 512^2 gray, scored across all cores). |
| `gui/FileDialog.h/.cpp` | Built-in ImGui file/folder browser (no native-dialog dep; works over WSLg/remote X). File mode takes an optional **multi-select** (`open(..., multi_select=true)`, `results()`): clicking a file toggles it, so a dataset can be built from several clips in one pick. |
| `gui/DatasetPrep.h/.cpp` | Everything between "the user picked an input" and "there is an image directory ready for SfM", shared by both reconstruction paths: frame extraction (in-process VK video decode → ffmpeg fallback), sharpest-frame selection, `.insv` track split, AI masking, and the resume rules. A job takes a **list of inputs** (`PrepInput`: path, video-or-folder, the `images/<subdir>` its frames go to, the masks that came with it, and the lens + focal factor that belong to it). **A picked photo folder is resolved by the project's own layout conventions** (`resolve_photo_folder`, matching `spirula sfm auto`'s probing and the dataparsers' `mask_dir = "masks"`): `<picked>/images` is the folder to index when it exists, and the `masks/` beside it holds masks that are **already made** -- those are used as they are and that input is never AI-masked (generating over them would write into a folder we were only asked to read). A folder named `masks` is never an input of its own: picked or dropped, it attaches to the input whose images it sits beside. **Every image walk follows directory symlinks** -- a prepared capture whose `images/`+`masks/` are links into the raw one is an ordinary layout, and the default iterator returns nothing for it -- and never descends into a `masks/` nested under the images, which would otherwise double the dataset with PNGs that are not views. One folder of photos is still read where it is; anything else is gathered into the dataset's own `images/`, one sub-folder per input (a multi-track video adds `cam0/`, `cam1/` under that), which is what makes the inputs separate cameras. Photo folders in a multi-input job are **hard-linked** into place (masks too, so the two trees keep mirroring), copied when the filesystem refuses. `probe_workspace` reports what the output folder already holds, split into what a run can **reuse** (extracted frames / features / matches / masks -> the resume checkbox) and what it would **replace** (`sparse/` -> a warning): an existing reconstruction is never treated as resumable, since a folder holding one opens in the trainer when dropped, so anything that gets to this screen with a `sparse/` in it is someone else's dataset or a finished one. The input's own images and masks never count as leftovers -- which matters because the output folder for a capture that already has `images/` **is that folder**: `sparse/` belongs beside `images/` and `masks/`, which is exactly where every parser looks. Masks mirror the image tree and are generated **per input**, so the tracker's memory bank never crosses from one capture into the next; a clicked object prompts only the input it was drawn on (`MaskClick::source`), and a clicks-only job whose inputs are not all prompted is **refused** rather than run -- a dataset where three of four camera folders kept the subject reads as a masking run that worked. |
| `gui/SfmRunner.h/.cpp` | images/video → dataset with the built-in SfM, by re-running this executable as `spirula sfm auto` (see SfmRunner.h for why it is a child process). Runs `DatasetPrep` first, then maps the panel onto flags. Per-input lenses become **`--camera-model DIR=MODEL` / `--focal DIR=PX`** overrides, `DIR` being the input's sub-folder — a prefix that covers a dual-lens file's `cam0/`+`cam1/` while still leaving them two camera groups. The focal is carried as a fraction of the image width and resolved to pixels once the frames exist (`first_image_dims`); an explicit focal typed into Advanced wins for a lone input. Masks reach the run as `--masks <dir>` (or `--no-masks`, so a stale `masks/` beside the images is never picked up silently); `PrepResult::mask_dir_cfg` and `image_dir_cfg` are handed to `GuiApp::open_dataset` in memory, so photos read where they are keep both their image and mask folders when the dataset opens in the trainer. Camera sharing switches to per-folder on its own when `images/` came out with sub-folders. Exit code 3 = reconstructed but partial (reported, not failed). |
| `gui/Subprocess.h/.cpp` | Cross-platform subprocess with merged stdout/stderr line streaming + kill-on-cancel (fork/execvp + process group on POSIX, CreateProcess + pipe on Windows). Bare `\r` ends a log line (ffmpeg/curl progress). |

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
| `WriterPool.h` | Bounded-queue worker threads that JPEG/PNG-encode and write frames and masks off the calling thread. Used by `FrameExtract`, `spirula sam track` and the GUI's folder-masking loop. Encoding a 1080p mask through stb's deflate is ~75 ms — a third of a SAM 2.1 Tiny frame — and none of it needs the GPU, so a caller that writes inline sets the frame rate with zlib. The queue bound is what keeps a slow disk applying back-pressure instead of growing until memory runs out. |
| `HttpServer.h/.cpp` | Minimal HTTP/1.0 GET server (POSIX sockets; winsock shim compiles but untested). Serial request handling — parity with Python's non-threading `HTTPServer`. |
| `Viewer.h/.cpp` | Web-viewer server: latest-wins render worker, the viewer buffer subset, `engine_blit_view` GPU annotation/colormap, stb JPEG encode. Serves the **unchanged** `viewer.html` (embedded at configure time via CMake hex; `SS_VIEWER_HTML=<path>` env overrides for dev). `/pick?px=&py=&<camera params>` returns the 3D point under a pixel as JSON for viewer.html's double-click centering (the client treats non-OK responses as a no-op). |
| `../config/TrainConfig.h` | Hand-written, the training config's single source of truth: the `SS_CONFIG_FIELDS(X)` X-macro flag table (184 rows), `struct TrainConfig` expanded from it, `SS_DATASET_PARSE_FIELDS`, `kTrainPresets` + `train_apply_preset()`, and `train_resolve_macros()`. |
| `../external/` | All vendored third-party code (marked `linguist-vendored` in `.gitattributes` along with the generated dirs): `stb_image.h`/`stb_image_write.h` (images), `npy.hpp` (checkpoints), `miniz.c/.h` (zip reading for the Metashape `.psx` camera table; compiled into `spirula` only). |

Debug: `SS_DUMP_CAMERAS=<path> spirula train ...` dumps parsed + post-split
camera arrays as JSON and exits before engine setup — used to diff against
the Python dataparser/trainer algebra (see verification notes below).

## Mesh extraction (`spirula-mesh`)

Drives `Meshing.h` / `MeshingHost.cpp` / `MeshUV.cpp` / `MeshExport.cpp`. Built by
the same `SS_BUILD_CLI` block; `mesh_main.cpp` + the dataset parsers, no
HTTP/viewer.

```bash
./build/spirula-mesh <ckpt> [--data <dir>] [--format ply,obj,gltf,glb] \
    [--color none|vertex|texture] [--texture-size 0] [--flag value ...]
```

- `<ckpt>` = run dir (config.json + `step-*.ckpt/`), a `*.ckpt` dir, or a
  `splat.ply` directly. `--data` defaults to config.json's `data`; the
  dataparser settings (image_dir, recon dir, metashape paths, numeric
  rescale) and `model.relative_scale` are honored from config.json.
- Color modes: `none`, `vertex` (per-vertex RGB), `texture` (LSCM UV atlas +
  baked image). Unsupported pairs error out up front: PLY+texture, OBJ+vertex.
- With `--color texture`, a format token may carry the texture encoding:
  `glb+png` (default), `glb+jpg` (JPEG q95), `glb+jpeg75` (JPEG q75) — works
  for obj/gltf/glb; JPEG is part of core glTF (`image/jpeg`).
- Output: one file per `--format` next to the checkpoint's splat.ply
  (`mesh.ply` / `.obj`+`.mtl`+tex / `.gltf`+`.bin`(+tex) / `.glb`), all
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
  steps); linear-color exercises color-space init + trust region; meshing
  exercises 3dgut + median losses; academic-baseline exercises interval eval
  split + non-FPBO + fp32.
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
