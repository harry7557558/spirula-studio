# Blur-tolerant SSIM

`--perceptual-loss ssim_blur_tolerant` trains SSIM that does not pull the scene
towards photos blurrier than it. Each 11x11 window scores the better of
SSIM(render, photo) and SSIM(B render, photo), B a Gaussian of sigma 1.5 px.
A photo blurrier than the render then costs nothing for the detail the render
has beyond it -- the blurred branch matches it -- while a render blurrier than
the photo gains nothing from more blur and is penalized as plain SSIM
penalizes it. `ssim_lambda`, masks, the clip test, the loss pyramid and the
maps work as for `ssim`.

## Why

Motion blur, missed focus and the soft frames of a video codec put blurry
photos among sharp ones. Blur in a photo is error the scene cannot and should
not reproduce; blur in the render is error more training or more splats can
remove. SSIM is symmetric and cannot tell the two apart. Its window
statistics average over views, so a minority of blurry photos drags every
surface they see towards their blur -- harder than it drags L1, whose
per-pixel median outvotes them (the toy fit below).

## What is computed

Per window and colour channel, with x the render and y the photo:

    S0 = SSIM(x, y),  S1 = SSIM(B x, y),  m = max(S0, S1)
    S  = m + (ln(e^(t (S0 - m)) + e^(t (S1 - m))) - ln 2) / t,   t = 50

a log-sum-exp less ln 2 / t, so two equal branches score what either does.
Its gradient splits between the branches by their softmax weights, which are
never negative; a branch 0.05 ahead takes 92% of it. A branch well ahead
scores ln 2 / 50 = 0.014 below itself, so the `ssim` the trainer prints reads
up to that much under plain SSIM's.

B is a normalized convolution over the kept pixels -- in the image, unmasked,
unclipped -- so a masked render pixel neither enters B x nor takes gradient
through it. Each coarser loss scale halves sigma: the tolerance stays fixed in
full-resolution pixels. sigma 1, 1.5 and 2 px did about equally well (below);
1.5 is a constant, not a flag.

`ssim_cs` (what `auto` picks) and `ssim_structure` map the two branches'
terms weighted by the same softmax, so a region where the photo is the
blurrier image asks for few splats. With blurry photos that map beat the
plain one under the same loss (bonsai: LPIPS-A 0.093 against 0.099, +0.2 dB).

## Kernels

Three passes per call: the blur (writing B x and its kept weight), the fused
forward and backward, and the blur's adjoint. The adjoint needs the blurred
branch's gradient beyond a tile, which the main kernel does not have, so it
cannot be folded in. The main kernel (`FusedSSIM.cu`'s `_ssim_bt_kernel`,
`fused_ssim.slang`'s `ssim_bt_bwd`) runs the render's window sums first and
keeps their moments in registers while the blurred render's sums take the
same shared buffer, then runs the backward one branch at a time. That fits
where plain SSIM fits: 30 KB at a 16x16 tile, 47.7 KB at 24x24. Vulkan takes
24 where the device has the shared memory, as for plain SSIM; CUDA stays at
16, where 24 needed 198 registers and ran 10% slower.

Vulkan unrolls both forward passes and adjoint branches, retaining their
window arithmetic and shared overlays. Its 24x24 tile uses 512 threads when
the device's workgroup and pinned-subgroup limits support it, reducing the
number of window centres each thread retains; 256 threads remain the
fallback. On an RTX 5070, the L1/blur-tolerant SSIM stack with fp16 render
and uint8 photo at 4946x3286 measured 31.766 ms against 36.246 ms with 256
threads, medians of three interleaved pairs (12.4% less time). At 1237x822
it measured 2.061 against 2.317 ms. Shared storage is unchanged; the final
scalar reduction reuses dead moment storage. Plain SSIM retains its
existing shader and workgroup size.

Per loss call at 780x520 on an RTX 3070 Laptop, everything a call runs
(`compute_multi_scale_per_pixel_losses`, L1 and the `ssim_cs` map),
interleaved:

| loss | Vulkan ms | CUDA ms |
|---|---|---|
| `ssim` | 1.16 | 1.10 |
| `ssim_blur_tolerant` | 1.88 | 2.12 |

The blurred branch roughly doubles SSIM's window arithmetic; the blur and its
adjoint add 0.06 and 0.07 ms. Garden at `images_4` with 1M splats trained
30k steps 7% slower than with `ssim` (Vulkan, 1222 s against 1138 s); the
share grows where splats are few for the pixel count.

## Results

mip-NeRF 360 bonsai (`images_4`, 780x520) and garden (`images_8`, 648x420),
10k steps, `--refine-stop-iter 6000`, splats capped at 300k and 200k (about
the density the benchmark resolutions get from the default cap), Vulkan, one
run each; the SSIM baseline repeated on clean bonsai moved 0.12 dB, 0.0015
SSIM and 0.001 LPIPS. Every 8th photo is held out and kept clean; the
training photos carry one of:

- `blur`: a third of them blurred, half by a Gaussian of sigma 1-2.5 px and
  half along a 4-12 px line (motion), in linear light;
- `noise`: Poisson-Gaussian noise, sigma 0.02 in sRGB at mid grey;
- `jpeg`: JPEG quality 30, 4:2:0;
- `mix`: a quarter blurred, noise sigma 0.012, JPEG quality 50.

Held-out PSNR / SSIM / LPIPS-A / LPIPS-V (`reference/python/eval_lpips.py`):

| scene, photos | `ssim` | `ssim_blur_tolerant` |
|---|---|---|
| bonsai, clean | 30.13 / 0.945 / 0.079 / 0.125 | 30.13 / 0.944 / 0.078 / 0.125 |
| bonsai, blur | 29.06 / 0.923 / 0.116 / 0.156 | 29.71 / 0.935 / 0.093 / 0.139 |
| bonsai, noise | 30.12 / 0.937 / 0.081 / 0.124 | 29.91 / 0.934 / 0.080 / 0.126 |
| bonsai, jpeg | 29.30 / 0.920 / 0.138 / 0.177 | 29.38 / 0.920 / 0.134 / 0.173 |
| bonsai, mix | 29.00 / 0.910 / 0.139 / 0.178 | 29.48 / 0.922 / 0.117 / 0.162 |
| garden, clean | 26.95 / 0.871 / 0.086 / 0.132 | 26.87 / 0.870 / 0.085 / 0.131 |
| garden, blur | 26.03 / 0.806 / 0.175 / 0.187 | 26.65 / 0.845 / 0.116 / 0.157 |
| garden, noise | 26.80 / 0.866 / 0.090 / 0.138 | 26.89 / 0.866 / 0.088 / 0.137 |
| garden, jpeg | 25.92 / 0.793 / 0.263 / 0.235 | 25.96 / 0.793 / 0.261 / 0.233 |
| garden, mix | 25.83 / 0.776 / 0.247 / 0.231 | 26.08 / 0.802 / 0.216 / 0.212 |

Blurry photos cost plain SSIM 0.9-1.1 dB and 0.04-0.09 LPIPS-A; the tolerant
loss gets back about two thirds of the LPIPS and of the dB. Where nothing is
blurry it is within run-to-run spread, except 0.2 dB on noisy bonsai (LPIPS
unchanged). At the benchmark length -- garden `images_4` (1297x840), 30k
steps, the default 1M cap, blurry photos -- `ssim` reached 25.98 / 0.805 /
0.170 / 0.181 and `ssim_blur_tolerant` 26.35 / 0.828 / 0.128 / 0.160.

sigma, on bonsai (blur / mix, LPIPS-A): 1 px 0.097 / 0.120, 1.5 px 0.094 /
0.119, 2 px 0.095 / 0.119 -- against SSIM's 0.116 / 0.139.

## The study behind it

The aim was a loss and a map that are smooth to train, agree with perception
(LPIPS, which nothing here trains on), shrug off error the scene cannot fix
(sensor noise, compression, blurry or misregistered photos) and stay
sensitive to error it can (missing detail, misplaced structure), with the map
blind to exposure and the loss not. Everything was tried first in a torch
lab, then as a Vulkan prototype behind an environment switch, on the
degraded scenes above.

What decides LPIPS. On every bonsai set, SSIM had the best LPIPS of the four
trained terms by a wide margin (clean LPIPS-A: SSIM 0.079, L1 0.093,
S-CIELAB 0.094, FLIP 0.098), though S-CIELAB matched or beat its PSNR on
noisy and JPEG photos. SSIM's normalized structure term is what buys texture.
So the change kept SSIM's sensitivity and made it asymmetric, rather than
trading sensitivity for robustness:

- A larger C2 (x3) makes SSIM shrug off noise -- the noisy-photo response,
  relative to a sigma-1 render blur, drops from 0.61 to 0.42 -- and in a toy
  fit to noisy views it halved LPIPS-A. In training, C2 x3 under blur
  tolerance lost 0.006 LPIPS-A on clean bonsai (0.011 with the luma map
  below as well).
- A smaller C2 (/4) is the other trade: clean bonsai LPIPS-V 0.117 against
  0.125, but -0.2 dB, and worse with noise (+0.007 LPIPS-A) and blur.
- `ssim_lambda` 0.3-0.4 barely moves clean LPIPS (-0.002 LPIPS-V at 0.4) for
  -0.2 dB, and makes blurry photos worse (0.124-0.126 LPIPS-A).
- Weighting SSIM's structure term down (0.5) for tolerance to small shifts
  wrecked noisy bonsai (-0.6 dB, +0.058 LPIPS-A).
- Two loss scales on these sizes (`--num-loss-scales 2`): -0.2 dB, worse
  LPIPS on clean and blurry photos.
- An asymmetric contrast term alone (half the penalty where the render has
  more contrast than the photo) helped slightly with blurry photos (+0.12 dB,
  -0.001 LPIPS-A; mix -0.003): a fraction of blur tolerance, because SSIM's
  structure term still pulls towards the blur.

Sensitivities, relative to blurring the render by sigma 1 px (8 crops of
mip-NeRF 360 views): LPIPS-A shrugs off a half-pixel shift (0.07), photo
noise (0.14) and exposure (0.00); SSIM is six times more sensitive to the
shift (0.45) and four times to noise (0.61); FLIP is the most sensitive to
both and to exposure (0.77, 0.67, 0.91). Blur tolerance leaves those about
where SSIM has them and makes a photo blurrier than the render cheap when the
tolerance covers it (a sigma-1 photo blur at sigma 1: 0.13; sigma 2 at sigma
2: 0.20; SSIM 1.0 and 2.1), which no symmetric loss can.

Toy fit (a pixel image fitted to 16 shifted views of 4 crops, (1 - 0.2) L1 +
0.2 P, LPIPS-A against the clean image): with a third of the views blurred,
L1 alone 0.068, SSIM 0.082, blur-tolerant SSIM 0.008-0.033 (sigma 2 to 1);
with noisy views SSIM 0.0035 against L1's 0.0018, blur tolerance 0.0045; with
0.35 px pose error, blur-tolerant SSIM was best or tied on every metric
(LPIPS-A 0.0081 against SSIM's 0.0093).

Maps. A synthetic test planted reducible defects (blurred patches, doubled
edges, floaters) in clean crops and scored where each map puts its fourth
power (the trainer's default) against a photo carrying noise, JPEG, blur or
an exposure shift. The default `ssim_cs` map puts half its mass on photo
noise of sigma 0.02; `s_cielab` and `ssim_cs` on luma with C2 x3 keep 97%
on the defects, and both shrug off a +15% exposure (99-100%, `flip` 50%).
Every map fails when the photo is blurrier than the render, which is where
the tolerant `ssim_cs` map helps (7% to 47% at sigma 2). LPIPS's own spatial
maps are no reference here: they score flat regions highest, so the SSIM map
"agreed" with them more once noise was added. In training the robust maps did
not carry over. Under plain SSIM the luma C2 x3 map cost 0.002-0.009 LPIPS-A
on every bonsai set, noisy photos included (+0.004), and was within spread on
noisy garden; the per-channel `ssim_cs` map with C2 x3 cost 0.002-0.005
everywhere, for up to +0.15 dB. A map that
stops seeing faint structure stops densifying where LPIPS still sees error,
and photo noise did not waste enough splats here to matter: SSIM lost 0.002
LPIPS-A to noise on bonsai, L1 nothing.
