# Focus weights (`spirula focus`, `focus/`)

Photos taken with a real lens are sharp only near one distance. Pixels outside
the depth of field still enter the photometric loss at full strength, and the
trainer answers them with blurry Gaussians and floaters. `spirula focus`
writes, per image, an 8-bit weight map to `focus/` (same relative path as the
image, `.png`): 255 where the pixel is as sharp as the lens gets, falling to 0
as its defocus blur exceeds what the training resolution can tell apart.
Training multiplies it into the dataset's mask (`masks/`), so a person masked
out stays masked out and the two never fight.

## Method

1. **Depth.** `spirula geometry --depth` (MoGe-2; `--model moge2-vitl` is the
   one the numbers below come from). Only inverse depth `u = 1/z` up to scale
   is used; the scale cancels.
2. **Edge blur, in pixels.** On the photo downsampled by an integer factor to
   ~3000-4000 px (`focus_measure_factor`), each non-maximum-suppressed edge is
   re-smoothed at s = 1, 1.4, 2, 2.8, 4 px. For a step edge blurred by sigma the
   squared gradient peak is `C^2 / (2 pi (sigma^2 + s^2))`, so `1/m^2` is linear
   in `s^2` and intercept/slope is `sigma^2`. High-ISO noise inflates the fine
   scales and makes blurry regions read sharp; the expected noise energy of
   every scale is measured on the flattest 32 px blocks of the same image per
   brightness level (correlated JPEG noise included), subtracted, and a scale
   enters the fit only where the edge clears it by 2x in amplitude.
3. **Defocus curve.** Thin-lens defocus grows linearly with `|u - u_f|`. Per
   64 inverse-depth slices, the weighted 25th percentile of edge blur (the
   sharpest content at that depth bounds the optics; shading only adds blur)
   is fitted with
   `b(u) = sqrt(b0^2 + (k_near (u - u_f)+)^2 + (k_far (u_f - u)+)^2)`
   by a grid over `u_f` and non-negative least squares with Huber reweighting.
   Slices above ~2 px are down-weighted: the estimator saturates there and the
   plateau they form dragged `u_f` behind the true minimum.
4. **Weight.** Defocus `sqrt(b(u)^2 - b0^2)`, converted to pixels at the
   training resolution, against one threshold `--allowed` (default 1.4 px at a
   5760 px long side) with a tanh transition: weight 0.5 at the threshold.

## Numbers

Validated outside this tree on three museum photographs (high ISO, 35-70 mm,
f/5.6-7.1, 6000-8200 px) against 28 hand-labelled regions (sharp / slightly
blurry / blurry): 28/28 with MoGe-2 vit-large depth, 26/28 with vit-base. With
depth from the fp16 Metric3D export the same method was unusable: its output
was quantized to ~0.1 m, 10-25 distinct values per image. On a 1648-image
capture with three lenses the default threshold needed no per-image or
per-lens tuning, and training with the weights left no holes in the subject.

## Known limits

- Glass (crystal balls, display cases) has refracted or reflected content at a
  virtual depth; depth decides there, local sharpness would mislead.
- Two objects at the same depth with different sharpness cannot be separated.
  Local blur evidence can, but it also cannot tell a soft shadow from a
  defocused edge and falsely suppressed in-focus, textureless table tops.
- An image with too few sharp edges to fit keeps weight 1 everywhere.
