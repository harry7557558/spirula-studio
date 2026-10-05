# Exposure for analysis

`--image-exposure` (`spirula sfm`, `spirula sam`, `spirula geometry`, and
**Exposure for analysis** under the dataset screen's colour space) brightens
the copy of each image that the detectors and the AI models look at. Training
never sees it: the trainer reads the files as they are, and its own exposure
handling (PPISP, `--image-color-*`) is untouched.

It exists for raw developers' exports that were pulled down to keep their
highlights (issue #127). Such a file is a perfectly good training image and a
poor detector input: SIFT's peak threshold is an absolute contrast (`0.02/3`
on `[0,1]`, `sfm/feature/Sift.h`), and after the 8-bit sRGB encode most of a
frame four stops under sits in the bottom ten codes.

## What it does

The value is `auto`, a signed number of stops (`2`, `+1.5`, `-1`), or empty /
`0` / `off` for none.

- **A number** multiplies linear light by `2^stops`.
- **`auto`** meters each image on its own: the median of linear Rec.709 luma
  over a grid of about 64k samples is brought to 0.1, which camera JPEGs of the
  issue's capture already sit above (0.11-0.21), so a well-exposed image is
  left alone. It never darkens past the point where the brightest sample
  reaches white -- an image with nothing above 1.0 is only ever brightened --
  and never brightens by more than 10 stops, past which it is lifting noise.
  `core/ColorSpace.h` (`exposure_gain`) has the rule; `image_color_test`
  checks it.

The gain applies in linear light **before** the 8-bit encode
(`colorspace::Srgb8Encoder`), so a 16-bit or float file keeps its shadow
precision. An 8-bit format (JPEG, PNG) only has its codes to stretch, so the
gain is a lookup over the 256 values it decoded to.

## What keeps the file's own values

- **Point colours.** SfM samples a colour at every keypoint for the sparse
  cloud the trainer seeds from. Under an exposure the loader keeps a second,
  unexposed colour buffer beside the one the detectors read
  (`sfm::GrayImage::color`), and samples from it, so a highlight that the
  analysis copy clips still seeds at its real value. The decode pool charges
  it to its memory budget (`planImageLoad`).
- **Everything the trainer reads.** The images are the files; masks, depth
  and normal maps are what the models made of the brighter copy, which is the
  point, but carry no brightness of their own.

## Measured

The issue's 25 frames (Fujifilm X-T2, 6000x4000), SIFT capped at 8192
features per image, everything else as the reporter ran it:

| input | exposure | features | registered | points |
|---|---|---|---|---|
| camera JPEG | none | 204,800 | 25/25 | 21,535 |
| camera JPEG | auto (+0.0 ... +0.2 EV) | 204,800 | 25/25 | 21,576 |
| Lightroom TIFF, -4 EV, read as its profile says | none | 10,642 | 19/25 | 648 |
| the same | auto (+7.4 ... +8.6 EV) | 204,800 | 25/25 | 22,621 |

MoGe-2's normals on the -4 EV frames lose the car's panels and the kerb
without it and keep them with it.

## Cost

A TIFF is decoded whole before it is encoded, so metering it is a pass over
64k samples. An EXR is decoded and encoded chunk by chunk, so `auto` first
decodes every eighth chunk to meter it -- an eighth of a decode, instead of
holding the image as float.

## Resume and reruns

The flag is in `spirula sfm`'s extract signature only when it is set, so
features extracted before it existed still match a run that does not use it.
The dataset screen records it among the settings of the masks, the model and
the geometry, again only when set.
