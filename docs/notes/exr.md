# Reading OpenEXR

`src/core/ExrImage.{h,cpp}` is this repository's OpenEXR reader. It is ~1300
lines with no dependency beyond the vendored miniz, and it exists instead of
`FetchContent(openexr)` for two reasons: OpenEXR drags in Imath and a large
build, and it is slower — on the two 6000×4000 half-float captures this was
written against, decode-to-`float32` measures

| file | this reader (32 cores) | OpenEXR 3.4 (python binding) |
|---|---|---|
| 24 MPix, PIZ | 174 ms | 370 ms |
| 24 MPix, ZIP | 130 ms | 174 ms |

(best of three, warm page cache) — and OpenEXR hands back `float16` while these
numbers include the widening to `float32`. Single-threaded the same files take
858 ms and 468 ms; chunks are independent, so the pool is over chunks and one
image scales across cores.

## What it reads

- **Storage** — scanline and tiled parts. Mipmapped and ripmapped tiles decode
  at level 0; the other levels are skipped.
- **Compression** — `NONE`, `RLE`, `ZIPS`, `ZIP`, `PIZ`, `PXR24`, `B44`,
  `B44A`.
- **Pixel types** — `HALF`, `FLOAT`, `UINT`, mixed freely within one file.
- **Channels** — `R`/`G`/`B`(`/A`); a single named layer (`diffuse.R`, …) when
  there are no bare ones; `Y` alone as greyscale; `Y`/`RY`/`BY` with subsampled
  chroma; and a one-channel file of any name as greyscale.
- **Windows** — the decoded image is the **display** window. A data window
  smaller than it leaves the surrounding pixels at zero, which is what keeps a
  cropped render aligned with the intrinsics that describe the full frame.
- **Multi-part** — the first part with colour channels wins, so a beauty pass
  beside a depth or ID part reads correctly. A part list with no colour part
  falls back to the first single-channel part.
- **Broken offset tables** — an all-zero chunk table (an interrupted write) is
  reconstructed by walking the chunks. Individual zero entries are skipped.

Correctness is checked against OpenEXR itself: 51 generated cases across that
matrix decode **bit-exactly**, single- and multi-threaded, as do the two
6000x4000 production captures.

## What it refuses, and what to do about it

Each of these fails with a sentence naming the problem rather than producing
wrong pixels.

| Not supported | Message says | Fix |
|---|---|---|
| `DWAA` / `DWAB` | which one the file uses | re-save as ZIP or PIZ |
| deep (`deepscanline`, `deeptile`) | it is deep | flatten it |
| several layers, no bare `R`/`G`/`B` | the layer names it found | write one layer to its own file |
| `B44` channels flagged `pLinear` | that flag | re-save as ZIP |
| big-endian hosts | a `#error` at compile time | — |

DWA is the one worth knowing about: it is a DCT codec with its own Huffman
stage and a channel classifier, ~800 lines on its own, and nothing in the two
captures this was built for uses it.

Two more limits are silent because they do not affect what we do with the
pixels: `pixelAspectRatio` is ignored, and subsampled `RY`/`BY` chroma is
upsampled nearest rather than through OpenEXR's 27-tap filter.

## Colour space

EXR pixels are scene-linear by specification, and `Info::is_linear` is
therefore always true. `Info::gamut` comes from the file's `chromaticities`
attribute, matched against the six gamuts in `core/ColorSpace.h` within 0.002
on all eight numbers — **including the white point**, so P3-D65 does not
silently become the theatrical-white `DCI-P3` (that would be a visible green
shift). An unmatched set leaves `gamut_known` false and falls back to Rec.709,
which is also what a file with no `chromaticities` gets, per the spec.

`decode_srgb8()` takes the caller's declaration and falls back to the file's
own only when the caller made none, since an EXR holding sRGB code values is a
mistake rather than a convention. That fallback is what makes an EXR readable
by the SfM front end and the AI models with no flags at all.

**A file can lie.** One of the two captures this was built against is an EXR of
display-encoded pixels — γ≈2.2, nothing above 1.0 — despite the format's
promise. Nothing in the header says so, so the override has to be reachable:
the trainer's `--image-color-is-linear` is a tri-state (`auto` / `true` /
`false`, the same shape as `--splat-color-is-linear`) and `--image-color-gamut`
lists **both** `none` (take it from the file) and `Rec.709` (sRGB primaries,
whatever the file says). Which curve those linear values then leave through is
a separate flag again -- `docs/notes/color-transfer.md`. The two halves are adopted independently, so declaring
the transfer still leaves the primaries to the file and vice versa. The same split runs all the way down: `spirula sfm`, `spirula geometry` and
`spirula sam` all take `--no-image-linear` beside `--image-linear`, the dataset
screen offers **From the file / Linear light / Display-encoded** next to the
gamut's own **From the file**, and its runners state on the child's command
line only the half the user answered. The give-away for such a file is a
maximum of exactly 1.0 across a whole scene-linear capture.

## Where it is wired in

- `spirula sfm` — `sfm::loadGrayImage`, decoding to sRGB on the decode pool
  with `threads = 1` (the pool above already owns every core).
- `spirula sam` / `spirula geometry` — `nn::load_image`, same conversion.
- Training — `DataManager`'s RGB decode. An EXR probes as `FLOAT32`, never
  `UINT16`: half-float carries values above 1 and 16-bit normalized would clip
  every one of them. That costs 12 B/px in the batch buffer, so
  `--cache-images disk` (the default) is the mode to stay in for large frames.
- GUI — frame selection, the stencil detector, the dataset previews, and
  `ImageCompare`'s "Original file" pane, which quantizes the file's own values
  with no transfer curve so it shows what is stored rather than a display of it.

The one path that cannot read an EXR is the **external masking fallback**,
`reference/scripts/mask.py`: Pillow has no EXR reader. It now counts the files
it could not open and says so at the end of the run rather than leaving the
capture silently unmasked. The built-in masking (`SS_BUILD_SAM`) reads them.

## Testing

`src/core/tests/exr_decode.cpp` builds as `exr_decode` and checks the reader
against a set of EXRs generated by `tools/gen_exr_cases.py` (which needs the
`OpenEXR` python package; the cases are not committed). Run:

```bash
python tools/gen_exr_cases.py /tmp/exr_cases
./build/exr_decode /tmp/exr_cases
```
