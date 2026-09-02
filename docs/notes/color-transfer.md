# The output transfer

A colour space is two independent halves: the **primaries** (which the trainer
calls the gamut) and the **transfer** — how a stored number relates to light.
`--image-color-gamut` / `--splat-color-gamut` answer the first. The second
splits again, into two flags that do different jobs:

| flag | question |
|---|---|
| `--image-color-is-linear`, `--splat-color-is-linear` | does the buffer *store* linear light? |
| `--image-color-transfer`, `--splat-color-transfer` | which curve turns that light into a display value? |

Keeping them apart is what lets a run train in linear light and still land
exactly on its photographs (`is-linear` on, transfer `srgb`), or train in
display values and still carry high dynamic range (`is-linear` off, transfer
`uncharted2`). One expression covers every combination:

```
display = TONE( M · (is_linear ? c : EOTF_sRGB(c)) )
```

`TONE` is the transfer:

| value | display encode |
|---|---|
| `none` (unset) | image side: `srgb`. splat side: whatever the image side resolved to |
| `srgb` | sRGB OETF, unbounded — the default, and what matches a photograph |
| `srgb-clamped` | clipped to `[0,1]`, then the OETF |
| `aces` | Narkowicz ACES fit, clipped, then the OETF |
| `filmic` | Hejl / Burgess-Dawson (its gamma is baked in) |
| `uncharted2` | Hable, white at 11.2, clipped, then the OETF |

One function does all of it, forward and backward, on both backends:
`working_to_display` in `src/shaders/pixel_wise.slang`, with the host mirror
(`tone_encode` / `tone_decode`) in `src/core/ColorSpace.h`. Transfer and
linearity are both compile-time axes — template parameters in CUDA,
specialization constants in Vulkan — so the curve and its decode fold away.
`colorspace::Transfer` fixes the numbering and every layer passes it as that
`int`: the engine API, the GUI viewport combo and the web viewer's `uTransfer`
uniform all agree on it.

## What each flag decides

**`*-color-is-linear`** is a statement about the buffer.

- On the image side it says how to read the files. `auto` takes it from an
  EXR, which declares itself scene-linear, and assumes display-encoded
  otherwise.
- On the splat side it is also the optimizer switch: linear storage turns on
  the `sqrt` DC reparameterization and the colour trust region
  (`OptimConfig::color_is_linear`). See `docs/` history and the measured A/B
  behind that parameterization.
- Unset on the splat side follows the images.

**`*-color-transfer`** is a statement about the curve out of that buffer, and
it does not reach the optimizer at all. Unset on the splat side follows the
image side, which is what keeps the render matching the photographs.

## Why a tone curve is worth having

Under `srgb`, the OETF is monotone and unbounded, so a photograph whose pixel
reads 0.99 is explained by a splat at linear 0.977 and nothing more. Every
capture that nearly clipped is reconstructed as if it had not been bright.

A shouldered curve moves the same 0.99 much further out:

| transfer | display 0.5 -> | display 0.99 -> | display 1.0 -> |
|---|---|---|---|
| `srgb` | 0.214 | 0.977 | 1.0 |
| `aces` | 0.149 | 4.29 | 7.24 |
| `filmic` | 0.174 | 11.2 (capped) | 11.2 (capped) |
| `uncharted2` | 0.646 | 10.1 | 11.2 |

(scene-linear values, for `is-linear` storage.) So
`--splat-color-transfer uncharted2` buys about 3.5 stops of headroom above
what a display-referred fit can represent. Two consequences:

- **The model must be viewed with the transfer it was trained with.** The
  numbers above are also the rescaling of every mid-tone, so the wrong choice
  in the viewer is a visibly wrong exposure, not a subtle shift. The GUI
  viewport and `viewer/` both offer the same list beside a linear toggle.
- **`--overexposure-reg` pulls the other way.** It penalizes working-space
  values outside `[0,1]`, which is exactly the range a tone curve exists to
  escape. It is 0 by default; leave it there (the `meshing` preset sets it to
  10 and should stay on `srgb`).

The `linear-color` preset deliberately stays on `srgb`: its point is to train
in linear light and reproduce the input images, which a tone curve would
break.

## The clip is straight-through

Where a curve is clipped, the forward value is clipped and the gradient is
not (`xfer_clamp01`, `xfer_max0`). A true clip has zero gradient above white,
which strands any splat that overshoots in a view where the target is *below*
white — it can never be pulled back. Straight-through is strictly better:
where the target is at white the loss gradient is zero either way, and where
it is below white the splat still gets pushed down.

`filmic` reaches 1.0 only at infinity, so its inverse is capped at
`kTransferWhite` (11.2, borrowed from Hable) rather than diverging. That is
the only place the curves are not exact inverses of each other.

## What else the pair decides

- **Seed colours.** An 8-bit point cloud is display values; they are decoded
  through the transfer before the gamut matrix and re-encoded only if the
  splats are stored display-side (`seed_splats`, gated on
  `--convert-initial-point-cloud-color`).
- **The noise background.** `--background-mode noise` draws in display space
  and inverts both halves per pixel, so mid-grey stays mid-grey on screen.

## `none` is unset, everywhere

Both front ends spell an unset string field `none`, and the GUI writes it
literally when a preset gave the field a value. `resolve_color` treats `""`
and `"none"` alike for all four colour fields; `colorspace::transfer_or` takes
the fallback the caller wants for that case.
