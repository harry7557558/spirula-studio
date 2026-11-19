# Native hardware splat viewer

The standalone model viewer offers an **OpenGL** checkbox. It uploads PLY
attributes into array textures and draws sorted instanced quads directly into
an HDR framebuffer. Ordered fragment interlock accumulates front to back and
reproduces compute transmittance termination. The native hardware path requires
`GL_ARB_fragment_shader_interlock`; unsupported devices retain compute rendering.
No projected-splat or splat/tile intersection buffers are
allocated. Training keeps its compute renderer.

Compute remains the default. An allocation failure while opening or rendering
a standalone splat model switches that model to OpenGL after releasing its
compute scene and screen buffers. `SS_VIEWER_OPENGL=1` selects OpenGL on open,
which avoids first attempting a large compute allocation. Spherical-beta and
learned-background models retain compute rendering.

The native renderer embeds `viewer/js/shaders.js` with the existing CMake
embedding utility. Projection, SH evaluation, Gaussian evaluation and display
transfer therefore use the same shader source as the browser viewer. Pinhole,
orthographic, fisheye, equisolid and equirectangular cameras support 3DGS, Mip
and 3DGUT. Splats use compute's zero near plane: perspective and orthographic
centers require positive forward depth; wide cameras require nonzero radial
distance. Lens distortion is not exposed by the standalone viewport.

## Storage and scheduling

PLY reading uses bounded chunks of 65,536 rows. The attribute upload retains
only positions on the host; neither a full float model nor a full quantized
copy is assembled. Positions and opacity are float32; quaternions, log scales
and DC are float16. Rest SH uses signed 8-bit coefficients and one power-of-two
scale per Gaussian. Log scales preserve very thin Gaussians that would
underflow in a linear half-float scale texture. SH texture layers are split
across four resources to avoid multi-gigabyte single-resource limits.

The nominal SH3 attribute cost is 83 bytes per Gaussian before texture
padding; RGB internal formats may occupy RGBA storage on some drivers. The
draw order adds four device bytes per Gaussian. Device memory measurements,
rather than this nominal count, decide whether a model fits.

The host retains 12 bytes of position and 12 bytes of sort storage per
Gaussian. The RAM preflight also budgets driver texture backing: the measured
100M SH3 process reached 12.89 GiB RSS. It keeps at least a quarter of currently
available host memory in reserve. The HDR accumulator uses float32 RGBA. Stable
radix sorting uses full float depth keys and up to four CPU threads, with
4 KiB total histogram storage and no extra per-splat buffers. For models with at least one million
splats, sorting runs on a worker. Until the requested camera's order is ready,
the viewport holds the previous complete image; it never draws a new camera
using an older order. Perspective and orthographic pans or rolls that preserve
all four depth-row values reuse identical sort keys and update immediately.
Input remains active, but continuous depth-order-changing movement may hold
the image until the camera pauses. The GUI polls completed work even without
new input. Screenshots wait for the requested camera and resolution. Unchanged
frames reuse the color texture. The initial upload and first sort are
synchronous and can delay the interface on very large files.

## Editing and export

Entering editing or video export releases the GL model and reloads it into
the compute renderer, then completes the requested action. Those operations
still require enough memory for compute storage and scratch. Editing checks
available host memory before reading its full float document; both operations
check estimated compute storage against GPU capacity before releasing the GL view.
A failed promotion
reports its error and restores the hardware view. Screenshots and comparison
wipes render directly through OpenGL. The GL view
currently provides RGB navigation, primitive/SH/color controls and model
placement; grid, camera overlays and double-click surface picking remain on
the compute view. Models with spherical-beta lobes or learned backgrounds
also select compute to retain their appearance.

## Validation

`hardware_splat_test` compares hardware RGB with the native compute renderer
for all fifteen primitive/camera combinations, two camera poses and three
transfer/color settings. The arithmetic reference uses dequantized upload
attributes; original-float quality is measured separately. An opaque red/green
overlap fixture verifies compute transmittance termination. Its
fixture includes SH3, anisotropic and very thin splats, small projected
footprints, behind-camera points and a Gaussian crossing the near plane.
A separate near-plane fixture checks centers at zero, 2^-8 and 2^-14 depth
for all fifteen camera/primitive combinations.
A 1,000,001-splat fixture checks immediate planar pans with unchanged depth
keys, held images across other changing cameras, polling without new input,
and draining superseded sorts for a resized capture against a fresh
synchronous render.
The production sort helper also has a serial `std::stable_sort` oracle covering
all camera keys, ties, non-finite coordinates, uneven chunks and the parallel
threshold. The oracle passes with OpenMP enabled and with it compiled out.
The full test, including this coherence fixture, passed on both Vulkan and
CUDA. An isolated 10M London GUI check also navigated, updated during idle
polling, and captured a nonblank 1920×1080 image after the change.
It also supports bounded-memory generation and loading of a synthetic model:

```
hardware_splat_test --generate synthetic.ply 100000000
hardware_splat_test synthetic.ply
hardware_splat_test --sort-test
hardware_splat_test --sort-benchmark 100000000 3
```

Synthetic capacity results must be reported separately from image-quality and
rendering-speed results on trained models. The large-model test checks that
every primitive produces a nonempty image and reports load/render wall time.

## Measured results

On an RTX 5070 with 12 GiB VRAM and 30 GiB system RAM, at 1280×720:

| Model | Added GPU memory | Load | 3DGS raster | Mip raster | 3DGUT raster |
|---|---:|---:|---:|---:|---:|
| London, 10M SH3, scene overview | 1,129 MiB | 5.95 s | 279 ms | 95 ms | 653 ms |
| Synthetic, 100M SH3 | 9,983 MiB | 61.03 s | 1.48 s | 1.39 s | 1.60 s |

GPU memory is measured with `nvidia-smi`, above the desktop baseline. Raster
figures average five changing pan views with `glFinish` from the earlier
asynchronous-order implementation, excluding sorting. They are raster costs
for planar pans that preserve depth keys, not general navigation frame rates.
These are different scenes and cannot establish scaling by themselves.
The 100M process peaked at 12.89 GiB RSS, and all three primitives produced
nonempty images. Its first sort took 3.76 s; a rotated view settled in 6.41 s,
including a 3.38 s worker sort. The coherent-frame guard added after commit
`fee923ec` changes scheduling, not model allocations; the 100M capacity run
was not repeated for that guard or the subsequent parallel sort. A
depth-order-changing camera refresh requires sorting plus rasterization,
potentially with an additional in-flight sort to drain. Continuous depth-order-changing movement
can hold the prior image. Capacity is demonstrated; smooth navigation at 100M is not.

The production CPU sorter was separately measured on 100M synthetic positions,
without a PLY or GPU allocation. Three alternating serial/four-thread runs,
all pinned to CPUs 0–3, gave medians of 4.055 s and 1.107 s (3.66× speedup).
The measured teams were one and four threads; all six complete-order hashes
matched and passed stable-order checks. Peak process RSS was 2.24 GiB.
This measures the sort component only; the full 100M camera refresh and raster
cost were not remeasured after this optimization.

Calibrated exterior and interior camera poses of the trained London 10M SH3
model give the following warmed medians over five frames. Both paths render
the same settled camera and full SH3; the hardware frame cache is invalidated
without changing the image. These cameras use undistorted fisheye projection.

| View | Primitive | Compute | OpenGL | PSNR versus float compute |
|---|---|---:|---:|---:|
| Exterior | 3DGS | 19.38 ms | 147.80 ms | 58.78 dB |
| Exterior | Mip | 19.31 ms | 86.72 ms | 58.75 dB |
| Exterior | 3DGUT | 25.65 ms | 266.89 ms | 58.68 dB |
| Interior | 3DGS | 8.51 ms | 65.09 ms | 58.82 dB |
| Interior | Mip | 8.10 ms | 46.14 ms | 58.82 dB |
| Interior | 3DGUT | 10.54 ms | 118.93 ms | 58.77 dB |

OpenGL is 4.5–11.3 times slower on these views, so compute remains the default.
The hardware path is useful when the model and compute intermediates do not fit.

Against the original float London model on the same overview camera, hardware
output measured 58.76/58.76/58.61 dB PSNR, with mean absolute RGB error near
0.0010. These figures include RGBA8 display quantization. Half geometry can
move a response across a visibility boundary: the near-camera synthetic
fixture contains one such pixel whose best-response ray depth changes from
+5.59e-7 to -8.48e-7 after quantization. Comparing dequantized attributes
isolates this from shader arithmetic errors.

The native 135-case parity suite and three opaque-overlap checks pass against
both Vulkan and CUDA compute backends. A
separate Chromium WebGL2 smoke compiled all 31 display/pick/tone programs and
rendered/picked a known Gaussian for all 15 camera/primitive combinations.
Its 45 near-plane cases pass; control runs catch both the old browser clipping
of positive depths below 0.01 and the old inclusion of zero-depth orthographic
centers.
The isolated native GUI loaded the real model, navigated, promoted to editing
with one click, switched back and closed. Both OpenGL/OpenGL and mixed
compute/OpenGL comparison wipes render correctly, and capture writes a nonblank
image for each hardware pane. Opening the 100M file with compute
selected exercised allocation fallback after attributes loaded: the GUI
reopened it in OpenGL, displayed SH3 correctly and rendered a nonblank image.
The final GUI build also loaded the 100M fixture directly in OpenGL, peaking at
12.29 GiB process RSS and 10,508 MiB total GPU use (479 MiB desktop baseline).
This is a separate GUI measurement from the direct harness figures above.
Its editing and export preflights refused the oversized operations with RAM
and GPU-capacity explanations, respectively, while retaining the visible GL
model. Closing the isolated GUI returned device use to the desktop baseline.
An isolated export check also confirmed that clicking OpenGL during frame
export leaves the compute renderer and export active; cancellation remains
available. The final hardware screenshot matched the earlier nonblank capture
byte for byte. Readback errors now drain all GL error flags and report failure
before image conversion; this error path was reviewed, not fault-injected.

For a camera from a dataset, the test accepts an array of JSON objects with
`camera` (`model`, `width`, `height`, `params`) and
`opencv_w2c_row_major` (16 numbers), followed by an optional array index:

```
hardware_splat_test --compare model.ply cameras.json 0
```

Model 5 selects an undistorted equidistant fisheye; other model values select
pinhole. Distortion coefficients are not applied by this standalone-viewer
comparison. It writes compute and hardware PNGs for each primitive.
