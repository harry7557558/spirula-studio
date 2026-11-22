# Exact footprints for 3DGUT and 2DGS

The eval3d primitives (`3dgut`, `2dgs`) cast a ray per pixel, so a splat's
footprint is whatever set of pixels its rays reach, under any lens. Binning and
culling used to guess that set from an unscented-transform ellipse; they now use
the footprint itself. Shared Slang: `shaders/projection_utils.slang` ("Exact
eval3d footprint") and `shaders/primitive_3dgs.slang` ("Exact eval3d bounds").
`footprint_exact` holds every camera model to a brute-force render.

## What was wrong

Measured on Mip-NeRF 360 bicycle (`images_4`, 100k splats, 25 held-out views),
against a build that puts every splat in every tile and culls nothing:

| model | old binning, PSNR to brute force | pixels off by more than 2/255 | worst |
|---|---|---|---|
| 3dgut | 41.4 dB (min 33.1) | 15% | 199/255 |
| 2dgs | 32.1 dB (min 27.8) | 42% | 180/255 |

Four separate defects, all false negatives:

- **The ellipse was a moment match.** Seven sigma points give a covariance, not
  a bound; a large or near splat's real outline is far from it.
- **Its centre was the box's centre, after the box was clamped to the image.**
  A splat hanging off an edge was culled with an ellipse shifted inward by half
  the overhang.
- **One sigma point behind the camera culled the whole splat**, where the
  rasterizer would have drawn the part in front.
- **A box that reached only the last pixel row or column was dropped**: the
  projection clamped to `extent - 1` and then called the box empty. This one
  applied to every primitive.

The same ellipse cost time where it was too large (it also carried the 0.3 px^2
dilation the 2D primitives need and these do not).

## The footprint

A 3DGUT fragment is `opacity * exp(-d^2 / 2)`, `d` the ray's distance from the
mean in the splat's whitened frame; it passes 1/255 where `d <= r`,
`r^2 = 2 ln(255 opacity)`. Those rays form the tangent cone of the `r`-sigma
ellipsoid. A 2DGS fragment is the same test on the ray's hit with a plane: the
cone over a disc. Both are one object:

- **Every ray that reaches a splat passes through one planar ellipse**
  (`FootprintDisc`): the surfel scaled by `r`, or the circle where the cone
  touches the ellipsoid. In whitened space that circle has centre
  `h (1 - r^2/|h|^2)` and radius `r sqrt(1 - r^2/|h|^2)`, `h` the mean.
- **In ray space it is a quadratic** (`footprint_ray`): with `g = M (o - mean)`
  and `e = M d`, a gaussian is hit where `|g x e|^2 <= r^2 |e|^2` and a surfel
  where `(g_x e_z - g_z e_x)^2 + (g_y e_z - g_z e_y)^2 <= r^2 e_z^2`, both with
  the hit in front. The form is quadratic along the segment between two rays.
- **A camera inside the ellipsoid** sees the splat along every forward ray.

No lens is involved yet. A lens only decides which pixel a ray belongs to.

## Projection: the bounds

`projection_3dgs<..., eval3d>` now projects bounds and nothing else: a box, a
slab and the cull. Its backward is the opacity's sigmoid.

| view | box | how |
|---|---|---|
| undistorted pinhole, disc in front of the camera plane | exact | the footprint is the conic with dual `c0 c0^T - a a^T - b b^T` (`footprint_pinhole_ellipse`) |
| the same, disc crossing the camera plane | conservative | the footprint is unbounded: the circumscribed octagon cut at a near plane |
| orthographic | exact | the gaussian's marginal, or the disc's projection |
| any other lens | fitted | eight points of the rim, projected |
| camera inside the ellipsoid | the image | |

Under an affine map the rim is an ellipse, and the first harmonic of eight
samples gives it exactly; what the lens adds is what the samples leave over,
and the box allows for twice that. Three things break the fit, and each has
its own rule:

- **Only what lands in the circle around the image can be drawn.** A rim point
  past it is pulled back onto it, and the footprint cut by the circle is
  bounded by the samples' box, half their second difference, and the circle's
  own bow between two pulled-back points.
- **A distortion polynomial is trusted out to where its radius first turns
  back** (`footprint_lens_trust`). Past that it folds points back into the
  image, where `is_valid_distortion`'s Jacobian test can pass again; an
  out-of-view splat then lands anywhere. Points past the trusted radius are
  pulled back to it.
- **A footprint over half a radian or a quarter of its distance wide, one with
  no sample in sight, and one crossing a distorted pinhole's camera plane**
  take 32 samples instead (`footprint_wide_box`). With none of the 32 in sight
  a long splat can still cross the image between two, so it is culled only
  when all of it lies outside the cone the image fills.

A footprint in sight nearer a fisheye's back pole than four times its width,
one that holds the pole or holds the lens axis with its whole rim out of
sight, and a panorama's footprint taller than half a radian take the whole
image; a panorama's pole or seam takes the full width. The tiles sort those
out.

- **The slab** rides in the screen row's old conic slot (`SCRG_SCALE`): the
  footprint's extent across its narrow side, `|slab.xy . p - slab.z| <= 1`. Box
  and slab together bound a thin diagonal splat about as well as an ellipse
  does, in three floats and without a centre.
- **A box holding no pixel centre is culled.** Rays are point samples, so this
  is exact here, and would not be for the dilated 2D primitives.
- **The centre-only depth cull is unchanged** (`cull_z`): a splat whose centre
  is behind a pinhole is not drawn, as in other renderers.

## Tiles

`do_intersect_tile_generic` takes a `FootprintView` (the splats and cameras)
and bins exactly:

- **Planar views sweep the ellipse.** For an undistorted pinhole or an
  orthographic view the footprint is an ellipse in pixels, recomputed from the
  splat in the kernel (`footprint_planar_cov`) and swept a tile row at a time.
- **The sweep works on the covariance.** `ellipse_range_bound` takes the
  inverse covariance, whose determinant cancels for a thin ellipse: a rounded
  `ym` lands past the ellipse's tip, the square root goes NaN and the whole
  tile row is dropped. `ellipse_range_bound_cov` has no such difference. The 2D
  primitives still use the conic form, where the 0.3 px^2 dilation keeps
  ellipses fat.
- **Everything else is tested against rays** (`footprint_rays_coverage`): the
  tile is a quad of four rays, hit when a corner is inside the footprint, an
  edge crosses it, or it holds the footprint's centre. Exact for a quad with
  straight edges, which a pinhole's tile is, so a pinhole derives its rays in
  the kernel.
- **A lens bends a tile's edges.** On a 230 degree fisheye 256 px wide, the
  corner-ray quad of a 64 px tile misses the tile by 7 px.
  `pixel_rect_quad` moves each edge that bows outward to twice the bow of its
  midpoint, and gives up past 60 degrees across, where four rays stop bounding
  anything; a tile without a quad keeps every splat whose box reaches it.
- **Blocks of 4 x 4 tiles go first.** Footprint and quad are both convex, so
  four corners inside means every tile of the block is hit, and a miss means
  none is. A box inside one tile needs no test at all.
- **The write pass reuses the count's tests.** A range of 32 tiles or fewer
  has its hits kept as a bit mask in the slot the cumulative counts used,
  which the scan no longer needs: it runs in place. On a 10M-splat fisheye
  view the write pass went from 2.7 to 1.6 ms on Vulkan and from 5.3 to 2.4
  on CUDA; capping CUDA's kernel at 64 registers then took the count from 5.5
  to 4.2 ms and the write to 1.4.

The edge test evaluates the quadratic at its minimum from the interpolated
ray. The closed form `q_a - lin^2 / quad` subtracts corner values of order 1e12
for a splat a thousandth of a pixel thin, to get a number of order 10.

## Rasterizers

Forward, backward and the meshing moments cull a splat against a micro tile on
the box and the slab alone, before loading the fragment; each pixel then tests
its own alpha, so a bound that keeps too much costs time only.

The exact test was tried there and dropped. Each thread loads one candidate a
batch, so a ~350 flop test per candidate costs as much as the per-pixel work it
saves: the forward took 1432 ms against 914 over 1500 steps of bicycle.

## Measurements

`footprint_exact`: nine views (pinhole, fisheye and equisolid with and without
distortion, panorama, orthographic) x both primitives x 16 and 64 px tiles,
with and without a third of the tiles switched off, 1500 splats, a tenth of
them large and a fifth thin. Every image is
bit-identical to the brute-force one on both backends, which also bin the same
number of pairs.

Rendered from the old runs' PLYs, the new binning gives the brute-force image
in all 25 views, to the last bit of the 8-bit PNGs.

Bicycle, 100k splats, Vulkan, RTX 5070, 30k steps, HEAD and the change built
alike; PSNR / training time, two runs a side (one for the meshing preset):

| run | before | after |
|---|---|---|
| 3dgut | 23.23 dB / 194.6 s | 23.27 dB / 193.5 s |
| 2dgs preset | 21.25 dB / 246.9 s | 21.33 dB / 247.7 s |
| meshing preset | 21.58 dB / 325.8 s | 21.64 dB / 322.7 s |

Under `SS_PROFILE=1` the 3dgut run spends 167.6 s in kernels before and 164.2 s
after (three runs each, within 0.4 s). At 1M splats 3dgut trains in 377 s
against 376 s, and the 2dgs preset in 393 s against 408 s (one run each,
against a build one commit older).

Kernel time over 1500 steps of the 100k 3dgut model, binning pinned at
16 / 32 / 64 px:

| kernel | before | after |
|---|---|---|
| raster forward | 871 / 1011 / 1420 ms | 839 / 988 / 1386 ms |
| raster backward | 2567 / 2608 / 2751 ms | 2479 / 2568 / 2677 ms |
| intersect count + write | 94 / 37 / 20 ms | 152 / 69 / 43 ms |

A lens with distortion pays more: an indoor capture of 239 distorted pinhole
and 291 fisheye frames (thin-prism coefficients, 300k splats, 12k steps) spends
165.0 s in kernels against 143.3 s. The backward is 6.4 s of that, the forward
6.2 s, the ray tests 5.4 s and the sampled bounds 3.9 s; the sort is 0.4 s
cheaper. With whole-image boxes for every footprint the fit could not bound,
as a first version had, the same run took 347.6 s: per view 4000 of 250k
splats then had such a box, none of them wider than a tenth of the image.

## Not done

- **An ellipse sweep for other lenses.** The fitted ellipse of a splat away
  from the image border could be swept as a planar view's is, in place of the
  ray tests; it would bound rather than equal the footprint.
- **A cheap reject before the bounds.** The projection samples the rim of
  every splat, including the half of a scene behind the camera.
- **A panorama's own closed form.** Along a column the footprint's latitude
  solves a quadratic, so a row sweep exists. The ray test is already exact
  there, and a panorama's wide footprints still take the whole image.
- **Thin gaussians vanish before the binning matters.** `evaluate_alpha_3dgs`
  overflows fp32 for an axis under about `e^-21`, and the splat draws nothing;
  1% of the 100k bicycle model sits at the `e^-40` floor.
- **The screen row keeps its width.** With the conic gone the row needs opacity
  and colour only; the slab fills the slot rather than changing `SCRG_STRIDE`.
