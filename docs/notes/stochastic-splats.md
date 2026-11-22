# Stochastic splats

`SS_VIEWER_STOCHASTIC=1` renders the viewer as StochasticSplats does
(Kheradmand et al., ICCV 2025): no depth order, a Monte Carlo estimate of the
composite, averaged over frames while the view holds still. Nothing in the
interface offers it yet; it exists to be measured against the sorted renderers.
It covers both viewers and every primitive, with the buffers they offer: rgb,
depth, alpha, sh and the rendered normal, not depth-derived normals.

| variable | default | |
|---|---|---|
| `SS_VIEWER_STOCHASTIC` | off | on for every viewer frame; exports and effect renders stay sorted |
| `SS_VIEWER_STOCHASTIC_SPP` | 4 | samples a pixel per frame, 1 to 8 |
| `SS_VIEWER_STOCHASTIC_FRAMES` | 64 | frames a still view averages before the GUI stops asking for more |
| `SS_VIEWER_STOCHASTIC_WINNER` | 0 | compute: 1 for the paper's estimator (below); OpenGL always uses it |
| `SS_VIEWER_STOCHASTIC_SORTED` | 1 | compute: 0 bins without the depth order |
| `SS_VIEWER_STOCHASTIC_STRATIFIED` | 1 | 0: each sample draws its own uniform (below) |

## What converges to what

A sample of a pixel keeps the nearest splat that accepted it, each splat
accepting with probability equal to its alpha there. When "nearest" is the
sorted renderer's order, the chance that splat `i` is the one kept is `alpha_i`
times the chance that nothing before it accepted, `prod (1 - alpha_j)`: the
sorted composite's weight `w_i`, exactly. So:

- **The order is the sort key, not the depth along the ray.** Compute samples
  compare `|depth|` bits and then the flatten index, which is what the tile
  intersector's stable radix sort orders by; OpenGL depth-tests the CPU sort's
  key (`HardwareSplatSort.h`) with ties to the lower index. The paper's
  per-ray depth would converge to a different, popping-free image; this one
  converges to the sorted render, popping and all.
- **No covariance changes.** Every pixel samples its centre, as the sorted
  rasterizers do, and every fragment's alpha is theirs (the same 2D conic with
  its dilation and Mip compensation, the same 3DGUT / 2DGS ray evaluation, the
  1/255 cut and the 0.999 clamp). Jittering the sample inside the pixel would
  converge to a box-filtered image instead, and no change of covariance turns
  that back into the point-sampled one: the composite is not linear in any
  one splat.
- **What does not carry over** is compute's termination: it drops the splat
  that would take T under 1e-4, and everything after it. The estimator sums
  every splat, so a converged pixel differs by at most that splat's share,
  `T alpha` with `T` under 0.1 (alpha is clamped at 0.999). The sorted OpenGL
  view draws that splat too.
- **The random draws are independent** across pixel, splat and frame (a PCG
  hash of the three), so each frame is unbiased and the running average
  converges. Within a pixel the samples are stratified: splat `i` draws one
  uniform `u` and sample `s` tests `frac(u + s / S)`. Each sample still sees an
  independent uniform per splat, so every sample is unbiased, but a splat now
  lands in `floor(alpha S)` or `ceil(alpha S)` of the samples rather than a
  binomial count: 17-20% less error on `stochastic_render`'s semi-transparent
  fixture, 0.3-0.4 dB on London.

## Estimators (compute)

- **Winner** (`SS_VIEWER_STOCHASTIC_WINNER=1`, the paper's): each sample shows
  its kept splat alone.
- **Visibility** (default): pass 2 of the rasterizer adds every splat at or in
  front of a sample's kept one, weighted `alpha_i * (samples whose kept splat
  is not in front of i) / S`, an unbiased estimate of its share (stochastic
  transparency, Enderton et al. 2010). Unnormalized it is noisier than the
  winner in deep stacks (half-opaque layers sum to a geometric count), so the
  weighted colour, depth and normal are divided by their own weight and scaled
  by the share of samples that kept any splat. Each of the three is summed over
  the frames before dividing, so the average converges to the sorted composite.

Depth is the weighted depth over the weight, as the sorted rasterizer
normalizes it. Alpha is the kept share in both.

## How it runs

**Compute.** The projection and the tile intersector are the sorted renderer's;
`SS_VIEWER_STOCHASTIC_SORTED=0` sorts the pairs by tile alone (radix passes over
the tile bits only), leaving each tile's splats in index order. The rasterizer
(`rasterize_stochastic.slang`, `RasterizationStochastic.cu`) walks a tile's
list twice, one 64-thread group a micro tile: pass 1 fills each pixel's
samples, pass 2 sums. A splat behind every sample's kept one changes nothing,
so both passes skip it from its sort key alone, the group skips loading it
when it is behind the tile's farthest sample, and a depth-ordered list ends the
walk at the first such splat. The draws do not depend on visiting order, so
both list orders give the same image to the bit.

Each pixel's sums run on in a per-channel buffer (9 floats a pixel, `Viewer`
VRAM) while its key holds: the request's camera, intrinsics, primitive, SH
degree and training step from the render worker, and the frame's size, splat
count and a revision every viewer-side edit of the splats moves. A training
session refines nothing: its splats move every step. The GUI asks for another
frame while a still view is short of `SS_VIEWER_STOCHASTIC_FRAMES` and draws at
its busy rate meanwhile. Depth-derived normals, median depth and the distortion
buffers are not offered: a stochastic depth's finite differences are noise.

**OpenGL.** No CPU sort and no order upload: a frame draws every splat in
storage order into a multisampled RGBA16F target with the depth test on. With
`GL_ARB_sample_shading` each fragment draws `SS_VIEWER_STOCHASTIC_SPP` (rounded
down to a power of two the device samples) uniforms and keeps the samples whose
draw is under its alpha through `gl_SampleMask`; without it a frame is a
sample, drawn that many times. The samples are averaged and join the running
average through a constant blend factor of `1 / (n + 1)`. A depth or normal
buffer averages the kept splat's value the same way; the coverage that
normalizes depth is one minus the averaged background share.

## Measurements

London (10M SH3 splats), COLMAP view DSC00005 (fisheye), 1280x720, RTX 5070,
Vulkan; frame times are medians, PSNR is of the running average against the
sorted compute render.

| compute | 3dgs | mip | 3dgut | 2dgs |
|---|---|---|---|---|
| sorted | 18.8 ms | 16.9 ms | 26.7 ms | 24.1 ms |
| stochastic, 1 spp | 32.4 ms | 30.0 ms | 40.0 ms | 39.4 ms |
| stochastic, 4 spp | 38.6 ms | 35.8 ms | 45.5 ms | 45.3 ms |
| stochastic, 8 spp | 50.6 ms | 42.9 ms | 53.1 ms | 52.1 ms |
| 4 spp, PSNR after 1 / 4 / 16 / 64 frames | 25.8 / 31.6 / 37.5 / 43.5 dB | 25.2 / 31.1 / 37.0 / 43.0 | 25.2 / 31.1 / 37.1 / 43.0 | 23.8 / 30.1 / 36.2 / 42.2 |

- The winner estimator is 3.5-4 dB worse than visibility at every frame count
  (40.0 against 43.5 dB at 4 spp and 64 frames, 3dgs) for the same time.
- Unstratified samples score 0.3-0.4 dB lower (43.2 dB, 3dgs) in the same time.
- Sort-free tile lists save the sort's depth passes and lose the walk's early
  end: 46.0 against 38.6 ms (3dgs, 4 spp), 43.7 against 45.3 (2dgs).
- A stochastic frame is 1.5-2.7 times a sorted one (1 to 8 spp), and reaches
  the sorted image's last 8-bit level only after hundreds of frames: in
  compute the sorted renderer is both faster and exact.

| OpenGL | 3dgs | mip | 3dgut | 2dgs |
|---|---|---|---|---|
| sorted (the GL view, not counting the CPU sort) | 21.5 ms | 18.7 ms | 19.0 ms | 15.5 ms |
| stochastic, 1 spp a frame | 16.2 ms, 20.5 dB | 16.0 ms, 20.3 dB | 29.3 ms, 20.3 dB | 26.2 ms, 18.8 dB |
| stochastic, 8 MSAA spp a frame | 26.5 ms, 29.4 dB | 22.8 ms, 29.1 dB | 41.8 ms, 29.2 dB | 36.5 ms, 27.6 dB |

The OpenGL stochastic view needs no sort, so a camera that changes the depth
order shows its first frame at once (a rotation settles in 47 ms at 8 spp,
against 54 ms for the sorted GL view, of which 23 are the CPU sort), and
then refines while the view holds still. Eight MSAA samples cost 1.6 times one.


## Tests

`stochastic_render` holds both estimators and both list orders to the sorted
forward for every primitive and several lenses; `stochastic_render model.ply
cameras.json [index]` times a real model. `hardware_splat_test --stochastic`
converges the OpenGL view's every buffer on every lens and primitive and holds
it to sorted compute.
