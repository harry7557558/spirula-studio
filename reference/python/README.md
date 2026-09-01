# reference/python

Python that is **not on any code path**. Nothing here is imported by the
engine, the apps or each other; nothing is packaged. These are references for
behaviour that has not been ported, and tools you run by hand.

| file | what it is |
|---|---|
| [eval_lpips.py](eval_lpips.py) | Adds LPIPS to a run's `metrics.json`. `spirula train --save-eval-images 1` writes `eval-gt-*.png` / `eval-render-*.png` pairs and scores everything except LPIPS natively; this reads those PNGs and fills in `lpips_alex`, `lpips_vgg` and their `cc_` variants. Needs only torch + torchmetrics + pillow — the colour correction is reproduced in the file, so `fused_bilagrid` is not required. |
| [equirect_ba_periodicity.ipynb](equirect_ba_periodicity.ipynb) | Demonstrates periodic equirectangular BA residuals, runs the native CPU/Vulkan seam regression, and provides a CLI harness for a real COLMAP sparse model. |

## Why LPIPS is not native

LPIPS is the one eval metric with a model behind it (AlexNet + VGG16 plus the
learned linear heads). Everything else — L1, PSNR, SSIM, and the colour
correction the `cc_` variants are computed on — is arithmetic, so it lives in
`src/app/EvalMetrics.{h,cpp}` and the trainer needs no Python to report it.

Scoring from the saved 8-bit PNGs rather than the float render is deliberate:
the numbers are then reproducible from the run directory alone, by anyone,
without re-rendering.

## The `normalize` asymmetry

`lpips_alex` is scored with `normalize=True` and `lpips_vgg` with
`normalize=False`, which is what the retired Python trainer did.
`normalize=False` means torchmetrics expects `[-1, 1]` but is handed `[0, 1]`,
so `lpips_vgg` is effectively measured over the upper half of the input range.
That is reproduced on purpose so the numbers stay comparable with previously
recorded benchmark runs. Changing it means rebaselining everything it is
compared against.
