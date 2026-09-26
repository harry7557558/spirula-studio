# D-Log M input

DJI's D-Log M is a log encoding: the camera stores `log(light)` so that a
10-bit file keeps highlights an ordinary Rec.709 clip would clip. Trained as
if it were an ordinary photo, a log clip gives a flat, washed-out model with
the wrong exposure. `--image-color-log dlogm-osmo360` (Osmo 360) or
`dlogm-avata360` (Avata 360) decodes it back to scene light first.

## What the flag does

```
--image-color-log dlogm-osmo360     # the training images are D-Log M from an Osmo 360
--image-color-log dlogm-avata360    # ... from an Avata 360 (our fit, see below)
--point-color-log none | off | dlogm-osmo360 | dlogm-avata360
```

The decode takes a code value to scene-linear light (mid grey 0.18) and
through the camera's primaries matrix to **linear Rec.2020**. The two curves
have the same form and differ only in their constants. That output is
fixed, so the flag also fixes the other two halves of the image side:

| resolved | value | set explicitly |
|---|---|---|
| `--image-color-is-linear` | on | `1` agrees; `0` gives way (plain sRGB, below) unless the gamut says Rec.2020 |
| `--image-color-gamut` | `Rec.2020` | `Rec.2020` agrees; `Rec.709` gives way unless linear is on; anything else is refused |
| `--image-color-transfer` | unchanged (`srgb` unless set) | allowed |

"Plain sRGB" -- gamut unset or `Rec.709`, and not linear -- is what an
ordinary photo is, and it is what the `hdr` preset states for its input. The
log flag is the more specific statement, so it wins over it silently:
`spirula train hdr --image-color-log dlogm-osmo360` decodes the images to
linear Rec.2020 and keeps the preset's linear ACEScg splats. A statement that
names another space (`ACEScg` input, linear Rec.709, display-encoded Rec.2020)
contradicts the decode and is refused.

After the decode, the existing conversion in `docs/notes/color-transfer.md`
runs unchanged: `display = TONE(M · c)` with `M` the Rec.2020 -> Rec.709
matrix. The splat side follows the image side as always, so a run trained
this way stores **linear Rec.2020** splats, which the GUI viewport and
`viewer/` both display correctly.

**For delivery through a viewer that expects sRGB splats**, pass
`--splat-color-gamut Rec.709 --splat-color-is-linear 0`. The images are
still decoded; only the splats' storage changes.

### Exposure: `--image-color-log-exposure`

```
--image-color-log-exposure <stops>   # default 0
```

A gain of 2^stops on linear light, right after the decode, on the training
images and on the seed points alike, so the two stay in agreement (+1
doubles both). 0 keeps the decode scene-linear. Without a log curve it does
nothing, and a seed side set to `off` is not brightened.

DJI Studio's D-Log M -> Rec.709 LUT is brighter than the decode because it
bakes in a display exposure. **+0.45 approximately matches its brightness**,
as measured on clip 0129: after a global gain of about +0.45 stops the decode
matched the LUT's output to within about 3 display levels. That is one clip,
not a calibration.

For the Avata 360, **-0.20** matches DJI Studio's export through the BT.709
transfer its rendering uses, and **-0.45** is the best single value through
spirula's own sRGB display (see the Avata 360 fit below). One clip again.

On the images the gain rides on the GT's Rec.2020 -> Rec.709 matrix, the
first linear step after the decode, so both backends and the host mean-luma
mirror get it without a kernel change. The compare panel's source pane
applies it too.

`--point-color-log` is the seed cloud's curve. Unset (`none`) follows the
images, which is right for a cloud the SfM sampled from the same log frames:
without the decode those seeds start about 1.15 stops too bright (code 0.4 is
0.18 decoded, 0.4 read as linear). A log point side is forced to linear
Rec.2020 the same way the image side is.

`off` says the colours are **ordinary sRGB**, for a cloud made from other
footage: the point side then stops following the (decoded) image side and is
read exactly as it would be in a run with no log flag at all, unless
`--point-color-is-linear` / `--point-color-gamut` say otherwise.

An `--init-ply` DC goes through the point conversion too. For a PLY trained by
an earlier D-Log M run with default splats (linear Rec.2020), pass
`--point-color-log off --point-color-is-linear 1 --point-color-gamut Rec.2020`.
One point setting covers both sources, so `--init-ply-add-points` cannot mix
such a PLY with log SfM points; seed from one or the other.

## Where it runs

- **Training images:** on the device, in `_engine_color_space_apply_to_gt`,
  after upload and before `working_to_display`, on both the plain and the
  warped upload path. The kernel is `input_curve_decode_forward`
  (`ImageColorOps.cu`, and `input_curve_decode_fwd` in
  `pixel_wise_render.slang`); the math is `src/shaders/dlogm.slang`.
- **Host mirrors:** `src/core/DlogM.h` holds the same constants for the seeds
  (`PointToSplat`) and for the mean-luma features (`_engine_color_space_gt_pixel`,
  which `luminance_normalization` and `background_match_luminance` read).
  `gt_decode_dlogm` holds the device to both.
- **Not decoded:** SfM, masking and `spirula geometry` load frames through
  `stbi_load` and see the flat log images. Features and masks on flat footage
  are likely somewhat worse (not measured).
- **Image compare:** the GT pane shows the decoded GT. The "source file" pane
  decodes a log file into the space the render pane shows (the splat working
  space, or display values), so the pair compares light with light
  (`source_pixel_for_compare`, `TrainerCore.cpp`).

`InputCurve` (`core/ColorSpace.h`) is its own axis, deliberately not a value of
`colorspace::Transfer`: `Transfer` is an *output* curve, and its numbering is
shared with the viewport's tone menu and the web viewer's `uTransfer`.

## Bit depth: what 8-bit frames cost

The Osmo 360 records 10-bit HEVC, but both frame extractors write 8-bit
(the built-in decoder packs `uint8`; the ffmpeg path writes JPEG). Measured
on the curve over 2M uniform codes in [0.05, 0.95], decoded then sRGB-encoded:

| extraction | error in stops, RMS / max | sRGB display error in 8-bit levels, RMS / max | largest display gap between adjacent codes |
|---|---|---|---|
| 8-bit | 0.013 / 0.086 | 0.53 / **1.6** | 2.1 levels |
| 10-bit (or a 16-bit PNG of it) | 0.004 / 0.025 | 0.16 / 0.48 | 0.6 levels |

An 8-bit sRGB JPEG quantizes at 0.29 RMS / 0.5 max levels, so 8-bit D-Log M
is about 3x coarser. On one real 3840² Osmo 360 frame, encoded to D-Log M
codes and decoded by the trainer's own GT upload, the 8-bit error came to 0.49
RMS / 1.3 at p99.9 / 3.9 max display levels (16-bit codes: 0.002 RMS). The max
exceeds the table's because the table stops at code 0.95 and uses grey only:
on the grey axis the gap between adjacent 8-bit codes reaches 3.6 levels in the
highlights above code 0.5, where display values pass 1.0. On footage shot in
D-Log M (a frame of `..._0130_D.OSV`, 230,400 pixels) 8-bit codes cost 0.47
RMS / 1.77 at p99.9 / 2.73 max display levels.

### 16-bit frames

`PrepJob::frame_bits` (the GUI's "Video frame depth"; preset key `frame_bits`)
is 0 = auto, 8 or 16. Auto is 16 for a clip whose djmd reads D-Log M and 8
for everything else, so a Normal clip costs nothing extra. A 16-bit input
always goes through ffmpeg, whatever the decoder setting: the built-in path
packs `uint8`. The candidates stay JPEG and the sharpest of each group is
chosen on **track 0 alone**; every track is then decoded again and only those
candidates are written, as 16-bit PNG named by candidate index. Both lenses
hold the same stems, so the capture is recorded lockstep.

The conversion is ffmpeg's `colorspace` filter with a 12-bit 4:4:4
intermediate, then `rgb48be`. swscale's own 10-bit to RGB48 conversion is not
used: on grey it is a 0.9962 gain (Y=940 lands on 65283, not 65535), and on
saturated chroma it misses BT.709 by up to 8.2% of full scale. With the filter
Y=64/502/940 land on 0/32768/65520 and colour stays within 0.22%.

Cost, measured on the Osmo 360's 3840² fisheye frames: 38-40 MB per D-Log M
frame (33 MB for a Normal clip) against 0.6 MB for JPEG, and a second decode
of every track. The 187 s D-Log M clip at 1 fps (374 frames) wrote 15.0 GB in
18 min on an M4 Max, with the two 1.95 GB track copies on top while it ran.
The prep log states the estimate, at 40 MB a frame, before the second pass. Keep `cache_images = disk` for such a dataset: `cpu`
holds every frame decoded, 88.5 MB each.

## Telling a D-Log M clip from a normal one

From the clip's metadata, never from its pixels. The Osmo 360 records the mode
in the `.OSV` file's `djmd` metadata track: in sample 0, top-level field 2
(stream meta), then field 4, then field 1 is the colour mode -- **19 is D-Log
M**, 0 is Normal (an empty field 4 decodes to 0). The video stream's own tags
say `bt709` either way. The Avata 360 (`dvtm_AVATA360.proto`) keeps the same
wrapper one level deeper, at field 2, then 2, then 4, then 1: 19 for D-Log M,
and an empty field 2.2.4 for Normal. `sfm::video_color`
(`src/sfm/core/Telemetry.cpp`) reads both; five real Osmo clips read as recorded
(three D-Log M, two Normal), and three real Avata clips too (two D-Log M, one
Normal).

What it returns:

| what the file carries | mode |
|---|---|
| `dvtm_oq101.proto`, colour mode 19 | D-Log M |
| `dvtm_oq101.proto`, colour mode 0 or an empty field 4 | Normal |
| `dvtm_oq101.proto`, any other colour mode (D-Log 2, D-Log2 22, HLG 9...) | log, unsupported |
| `dvtm_oq101.proto`, field 4 missing, of the wrong wire type, unparseable, or holding anything but a varint field 1 | unknown |
| `dvtm_AVATA360.proto`, 2.2.4.1 = 19 | D-Log M |
| `dvtm_AVATA360.proto`, an empty 2.2.4 | Normal |
| `dvtm_AVATA360.proto`, any other 2.2.4.1 (no sample has shown one), or 2.2.4 missing or malformed as above | unknown |
| any other DJI layout | unknown |
| a `djmd` track whose first 8 samples hold no readable clip header | unknown |
| no `djmd` track | not recorded |

Only an empty wrapper is proto3's unwritten 0. Any other shape the reader was
not written against is unknown, never Normal. The Avata 360 has `fov_type` at
field 2.4, where the Osmo has the colour mode, and it is empty on every Avata
sample, so reading the Avata the Osmo way would call every clip Normal.

### The record

Extracted frames carry no mode, so the GUI's dataset preparation writes each
input's mode to `.spirula-color` in the dataset root (`src/data/DatasetColor.h`),
one line per input: `<mode> <code> <layout or -> <file name>`, where mode is
`dlogm`, `normal`, `unsupported-log`, `unknown` or `unrecorded` (photos, a
non-DJI video). Each unknown input is logged with its layout as it is read.

- It is written to `.spirula-color.tmp` and renamed into place. If that fails,
  the old record is removed and the failure logged. If even the removal fails,
  preparation stops and names the file, because a stale record would decode
  every frame the wrong way.
- A line that does not parse, and a record that is there but cannot be read,
  both read as an unknown input.
- When the inputs change, the frames of inputs no longer in the job are removed
  from the dataset's `images/`, so nothing is trained that the record does not
  describe. A folder an input is read from is never removed.
- The same prune runs when `images/` holds frames but `.spirula-frames` is
  missing, as after an interrupted prep. Those frames are not called stale, so a
  resume still keeps what the interrupted run finished; a lone input keeps its
  root frames and its camera folders (`cam<k>/`, or its own photo subfolders).
- A stale entry that is a directory symlink is unlinked and logged with its
  target. The link is never followed, so what it points at is never touched.
  Anything that cannot be removed is logged.
- Clear project removes the record.
- `spirula sam extract` writes no record, and warns when the video is D-Log M,
  in an unsupported log profile, or unreadable. A Normal clip stays silent.

### What training does with it

`--image-color-log` defaults to `auto`, which `TrainerSession::load_dataset`
settles from that record and logs:

| record | `auto` becomes |
|---|---|
| every input D-Log M from an Osmo 360 | `dlogm-osmo360` |
| every input D-Log M from an Avata 360 (`dvtm_AVATA360.proto`) | `dlogm-avata360` |
| D-Log M from both cameras | refused: the curves differ; split the dataset, or set the flag only if every input is that camera's |
| D-Log M beside anything else (Normal, another profile, unknown, photos) | refused: split the dataset, or set the flag only if every input is D-Log M |
| any input in another DJI log profile | refused: prepare without it, or set `none` to train it undecoded |
| an `unknown` input, no D-Log M | no curve, and a line naming the input and asking for the flag |
| every input Normal | no curve |
| no record | no curve, nothing logged |

Any explicit value, `none` included, is kept and logged as set. The seed
points follow the images as before.

`config.json` keeps what was asked for under `image_color_log` (`auto`), and
the curve it settled on under `image_color_log_resolved` (`dlogm-osmo360`,
`dlogm-avata360` or `none`). That key is not a flag: a resume reads it and never re-detects, while
a preset or a batch row made from the file ignores it, so one dataset's answer
does not follow the preset to the next dataset. A `null` `image_color_log`, as
older files have, is an explicit `none`.

## Where the numbers come from

The Osmo 360 curve and matrix are OpenOSV's (Apache-2.0,
https://github.com/Kemerd/OpenOSV, read at commit
`3a39776272efb5dfdc1d29711ae746e855383084`), ported as 16 constants:

- `kDlogMOsmo360`: `lin = mgs · (t < cut ? t·slope + intercept : t·slope2)`,
  `t = 2^(scale·code + yShift) + xShift`, `cut = intercept / (slope2 − slope)`
  (the branches meet at code 0.1252; the slope kinks there, the value does
  not). Anchors: code 0.40 -> 0.18000, 0.714 -> 0.95775, 1.0 -> 3.76470.
  Input is full-range R'G'B' from narrow-range BT.709 Y'CbCr.
- `kNativeToRec2020_Osmo360`: rows sum to 1 (white stays white),
  determinant 0.873349, all three implied primaries have positive luminance.

Both are OpenOSV's least-squares fits to DJI's publicly distributed Osmo 360
D-Log M -> Rec.709 LUT: the curve to its 33 neutral-axis entries, the matrix
to all 35937. So "scene-linear" here means linear under that model of DJI's
rendering, not a radiometric calibration of the sensor; DJI ships the same LUT
for the Pocket 3. The curve's residual against the LUT is 0.016 RMS (HLG code)
above code 0.24, and its toe below 0.24 is loosely constrained. The matrix
cannot reproduce DJI's gamut mapping of saturated colours.

Not ported, on purpose: OpenOSV's Pocket 3 curve and matrix (a different
camera, a matrix with a negative-luminance blue primary, and taken from an
unlicensed repository), its older `kDlogMDjiRefit`, its Rec.709 "look", and
its tests' table of measured DJI LUT values. The tests here check the curve's
own anchors instead.

### The Avata 360 curve: our fit, not DJI's

DJI publishes no D-Log M LUT for the Avata 360, and the Osmo 360 curve does
not match what DJI Studio makes of Avata footage. `kDlogMAvata360` and
`kAvata360ToRec2020` (`src/core/DlogM.h`) were **fitted by us** from the
operator's paired Avata 360 footage against DJI Studio's own export of the
same clip with DJI's D-Log M LUT applied. They are a model of that export, not
a DJI curve and not a radiometric calibration.

Data: `DJI_20260712122503_0001_D.OSV` (21.3 s, D-Log M, indoors) against its
DJI Studio export (6000x3000 equirect, 10-bit BT.709), and as a control
`..._0003_D.OSV` (Normal) against its export. Both were decoded to 16-bit
through the same `colorspace` filter the 16-bit prep uses.

Method:

1. **Geometry.** Each fisheye was registered to DJI's equirect by dense optical
   flow and a robust fit of a Kannala-Brandt lens (f, centre, k1..k4) plus a
   per-frame rotation. On the Normal pair the fit residual is 1.1 px (lens 1)
   and 2.0 px (lens 0) at the median, 2.1 / 3.7 px at p90, in 3840 px fisheye
   pixels; the rotation between the two lenses came out the same on all 7
   frames to within 0.02 degrees. Both lenses fit f = 1047 px with the centre
   within 15 px of the frame's, so the Osmo 360 preset (f = 1049.5) is close.
   Export frame n+1 pairs with fisheye frame n.
2. **Noise floor.** With that geometry the Normal fisheye and DJI's Normal
   export agree to **0.70 / 0.38 / 0.53** display levels (8-bit, R/G/B) at
   the median and **1.75 / 0.99 / 1.36** at p90, over flat regions (local
   gradient under 0.004 per export pixel at 3000 px), more than 15 degrees from
   the seam, and unclipped. The best global exposure there is 0.004 stops, so
   the Normal pair is an identity and cannot tell sRGB from BT.709.
3. **Samples.** 13 frames spread over the D-Log M clip, both lenses, with the
   same masks: 7 frames to fit, 6 held out (1.29 M held-out samples). Neutral
   samples (channels within 0.015 code of each other) fit the curve and the
   exposure; all samples fit the matrix; the two alternate.
4. **Model.** `display = OETF(2^e · M709 · M · curve(code))`, with the
   curve in the form above and OpenOSV's constraints: code 0.4 -> 0.18,
   continuity at the cut, `lin(0) >= 0`, strictly increasing,
   `slope2 / slope <= 3`. `yShift` and `slope` are held at the Osmo values,
   because the form has two exact degeneracies and they fix the gauge. The fit
   lands on the same two bounds the Osmo fit does (`lin(0) = 0`, ratio 3). The
   matrix rows sum to 1; its determinant is 1.0752 and all three implied
   primaries have positive luminance, but the red one lies outside the
   spectral locus (Z < 0), so it is a fitted transform, not a physical sensor
   primary. `e` is fitted separately and is not in the curve.
5. **Display transfer.** BT.709, not sRGB. Both reach the floor on held-out
   frames, but BT.709 fits the toe (codes under 0.2: 3.1 levels off at the
   median against 11.9 for sRGB) and survives a cross-lens test that sRGB fails:
   fitted on lens 0 and scored on lens 1, BT.709 reaches p90 4.0-5.4 levels,
   sRGB 13.5-14.0, and the Osmo curve 10.0-11.2 (5.8-6.2 through sRGB).
   HLG, which OpenOSV found behind the Osmo LUT, reaches only 1.1 levels at
   the median here.

Held-out result, display levels, median and p90 per channel (R/G/B), each
curve with its own fitted exposure:

| through BT.709 | exposure | p50 | p90 |
|---|---|---|---|
| Normal control (the floor) | 0.00 | 0.70 / 0.38 / 0.53 | 1.75 / 0.99 / 1.36 |
| Osmo 360 curve and matrix | -0.50 | 3.90 / 3.71 / 4.19 | 10.42 / 10.04 / 10.25 |
| Avata 360 fit | -0.20 | **0.57 / 0.50 / 0.60** | **1.69 / 1.45 / 1.72** |

By code, the fit is within a level of DJI from code 0.3 to 0.8, 2-3 levels
bright below 0.3 (few samples; the `lin(0) >= 0` bound will not crush the toe
as DJI's rendering does), and 4.6 levels bright above 0.8. The Osmo curve is
6-9 levels dark from code 0.2 to 0.5 and 31-42 levels bright above 0.7.
Against the Osmo curve the fit is up to 0.18 stops brighter below grey and 1.0
stop darker by code 0.85.

**Spirula displays through sRGB, not BT.709**, so the viewport does not reach
that floor. Through sRGB with the best single exposure (-0.45), the fit is
1.19 / 1.37 / 1.70 levels off at the median and 5.0 / 6.1 / 6.5 at p90,
against 1.89 / 1.77 / 1.88 and 5.9 / 5.7 / 5.9 for the Osmo curve (-0.73).
A curve fitted through sRGB instead reaches 0.55 / 0.51 / 0.61, but it puts
the display transfer's difference into the "linear" light and fails the
cross-lens test above, so it was not shipped.

Method check: the same procedure run on the Osmo 360 pair (clip 0129 against
its DJI Studio export, 7 frames, 3 held out) finds HLG behind the export, as
OpenOSV did, and lands within 0.04 stops of `kDlogMOsmo360` from code 0.15 to
0.45, 0.10 stops dark at 0.6 and 0.22 at 0.8. Its fitted exposure is about one
stop above OpenOSV's scene scale (-0.88 against -1.90 stops), which is not
explained. The Avata fit sits up to 1.0 stop from the Osmo curve, four times
further than the method's own error on the Osmo pair.

Limits: one clip, one indoor room lit by daylight, a scene of mostly white
and beige surfaces, and clipped samples excluded, so saturated colours and
highlights above code 0.8 are weakly constrained. The matrix sends some saturated codes
outside Rec.2020 (code 0.6 / 0.3 / 0.2 gives -0.068 blue).

### OpenOSV NOTICE

The parts of OpenOSV's NOTICE that pertain to this port are in
`LICENSES/NOTICE-OpenOSV.txt`, verbatim, with a short preface. It includes
the NOTICE's bullet on the `kDlogMDjiRefit` curve, which is not ported,
because the Osmo 360 bullet's "of the same form" refers back to it. The files
that carry the port -- `src/core/DlogM.h` and `src/shaders/dlogm.slang` -- say
so in an SPDX header, and the licence text is `LICENSES/Apache-2.0.txt`.
`tools/package_macos.sh` copies `LICENSE` and `LICENSES/` into the app bundle
(`Contents/Resources`), so the macOS DMG carries both (Apache-2.0 §4(a), §4(d)).
That also covers the Apache-2.0 code the tree already had (`shaders/ppisp.slang`,
`shaders/harmonics.slang`), which shipped without the licence text before.

## Tests

| test | holds |
|---|---|
| `dlogm_osmo360` | the curve's anchors on both branches, continuity at the cut, the round trip (2e-5), the matrix's row sums, determinant and layout, and code -> Rec.709 through spirula's own Rec.2020 matrix |
| `color_resolution_test` | what `resolve_color` makes of the two flags, the refusals, the `hdr` preset, the transfer left alone, the seed colours (including `off` against a run with no flag), the compare panel's source decode, and the exposure gain on the seeds and the compare source; for `dlogm-avata360`, that the refusal, the compare source and the seeds use the Avata constants |
| `dlogm_avata360` | the same for the Avata 360 constants, plus the 3x slope bound, the names, and that `input_curve_to_rec2020` sends each curve to its own constants |
| `gt_decode_dlogm` | each curve's device decode against its host curve and the mean-luma mirror, the misuse guard, and that `engine_reset` clears it |
| `dataset_color_test` | `auto` against the record, including an Avata 360 dataset resolving to `dlogm-avata360` and a two-camera D-Log M dataset being refused |
| `dlogm_session_test` | the flag through `TrainerSession::setup_engine` and one real step: the decode is armed, the uploaded GT is decoded, and the brightness match measures decoded light; again at +1 stop, where both read the doubled light |

None of them reads D-Log M footage; the footage behind the Avata 360
constants is described above. What was checked for the decode itself is ordinary footage encoded to D-Log M codes with the inverse curve.
One real 3840² Osmo 360 frame through the trainer's GT upload comes back at
0.002 display levels RMS from 16-bit codes (0.49 from 8-bit). A 69-photo
scene at 1/8 resolution, encoded to 8-bit codes and trained for 3000 steps with
the flag, lands its decoded eval GT within 51.8 dB of the original photos and
its renders at 21.3-21.8 dB against them, next to 20.8 dB for the same scene
trained from the photos directly; without the flag, 15.7 dB. Those are short
runs on one scene: they show the decode is wired end to end, not that D-Log M
trains better or worse than a normal clip.
