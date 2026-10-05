# Video decoding (`src/video/`)

Video decoding without ffmpeg, on top of `src/nn/`'s Vulkan runtime: the pixels
come out of `VK_KHR_video_decode_*` and everything the driver will not do for
itself — containers, bitstream parsing, reference picture management — is here.

**Compiled only with `-DSS_ENABLE_PATENTED=ON`, which is OFF by default.**
The H.264 / H.265 / AV1 bitstream parsers are the one piece of this repository
carrying third-party patent exposure, and this is a GPLv3 tree. With the option
off, `src/video/` is neither compiled nor linked and every consumer falls back
to an external ffmpeg: the GUI says why in the dataset panel, and `spirula sam
video` / `extract` say so and exit. See `cmake/SsOptions.cmake` and
`docs/build.md`.

Turning it on buys roughly 15x faster frame extraction (a 127-second 1080p30
clip in ten seconds), masking that rides along on the same device pass, and no
ffmpeg to install.

```
 file ──► Demuxer ──► CodecDecoder ──► vkCmdDecodeVideoKHR ──► YCbCr image
          mp4/mkv     H264/H265/AV1      (video queue)             │
                                                                   ▼
                                            luma plane ──► sharpness (compute)
                                            all planes ──► RGB (compute)
```

| | |
|---|---|
| containers | ISO-BMFF (`mp4`, `mov`, `m4v`, `insv`) and Matroska (`mkv`, `webm`), picked by content rather than extension. Multiple video tracks are enumerated, not merged — an Insta360 `.insv` is two fisheye streams. |
| stills | HEIF (`heic`, `heif`, `hif`): an H.265 image or a grid of them — see "HEIF stills" below. |
| codecs | H.264 (Baseline/Main/High), H.265 (Main/Main10/RExt), AV1 (Main/High/Professional) — whatever the *device* also advertises. |
| not supported | fragmented MP4 (`moof`), laced Matroska blocks, field-coded (interlaced) H.264/H.265, slice groups (FMO). Each is reported by name. |

## What a `CodecDecoder` is for

A hardware decoder does entropy decoding, motion compensation and filtering. It
does **not** track reference-picture state, and Vulkan makes that the
application's job. So per coded frame the codec layer produces:

* the bitstream the driver should see — Annex-B for H.264/H.265 (3-byte start
  codes; NVIDIA's H.265 slice-header parser assumes that prefix length at the
  offsets it is handed), the frame's OBUs for AV1;
* the `StdVideo*` picture-info structure;
* which DPB slot the picture activates and which slots it references.

The three implementations differ in where that state lives, which is why they
share an interface rather than code:

- **H.264** mutates its DPB in place: sliding-window or MMCO marking, with POC
  types 0/1/2 all in play. `H264Decoder.cpp`. It also shifts the `FrameNum`s it
  hands the driver so that no reference's exceeds the current picture's:
  NVIDIA 595 builds RefPicList0 without FrameNumWrap. A 16-frame `frame_num`
  cycle with four references went wrong 17 frames into every 158-frame GOP and
  stayed wrong to the next IDR; ffmpeg's Vulkan hwaccel does the same on that
  driver, while its NVDEC path does not.
- **H.265** rebuilds the reference picture set from scratch for every picture,
  so most of that file is `st_ref_pic_set()` and the POC bookkeeping around it.
  `H265Decoder.cpp`
- **AV1** has no parameter-set NAL units at all: quantization, segmentation,
  loop filter, CDEF, loop restoration, tiling and global motion are re-sent in
  every frame header, and each has a StdVideo struct to fill. That file is a
  complete section-5.9 frame-header parser. `Av1Decoder.cpp`

AV1 also puts several coded frames in one temporal unit — one shown frame plus
the hidden alternate references it was predicted from — so a packet can yield
several pictures (`PictureInfo::more_in_packet`), and `show_existing_frame`
re-outputs a picture decoded earlier, which is why pool images are pinned while
a DPB slot still names them.

## Two decode output layouts

`VkVideoDecodeCapabilitiesKHR` reports one of two arrangements and both are
implemented:

- **DISTINCT** — the driver writes a standalone output image alongside the
  reference it sets up. Nothing more to do.
- **COINCIDE** — the decode target *is* the DPB slot, which is what NVIDIA
  reports for H.264 and H.265. The picture then has to be copied out before that
  slot is recycled, so `recordDecode` appends a `vkCmdCopyImage` (one region per
  plane; multi-planar images cannot be copied with a COLOR aspect) after
  `vkCmdEndVideoCodingKHR`. About 30 µs at 1080p.

## Queues

Decoding runs on the video-decode queue family with its own command pool and
timeline semaphore; the conversion runs on the compute queue through the normal
`vk::Stream`. Output images are created `VK_SHARING_MODE_CONCURRENT` across the
two families so no ownership transfer is needed, and `Stream::waitOn` attaches
the decode's timeline value to the next compute submission.

Two things bear repeating:

1. **`vkCmdBeginVideoCodingKHR` binds resources; the decode operation activates
   slots.** The picture being reconstructed is therefore listed with
   `slotIndex = -1`. Passing its real slot index is a validation error until
   some earlier decode has activated it — which is not the case the first time
   around.
2. Layout transitions for the pool images happen on whichever queue is about to
   use them, with `VK_PIPELINE_STAGE_ALL_COMMANDS_BIT` on both sides. A video
   queue supports very few pipeline stages; ALL_COMMANDS is always legal.

## The entry-point table lives one device, not one process

`video_api()` resolves `VK_KHR_video_*` through `vkGetDeviceProcAddr`, and the
table it hands out is valid **only for the device it was resolved from**.
`nn::shutdown()` is not a process-exit hook — the GUI calls it after every
dataset job to hand the 2 GB of segmentation weights back — so a second video
open runs on a second device, and a table cached for the life of the process
then points at loader trampolines that were unmapped with the first one. The
symptom is a segfault in an unnamed frame under `createSession`, one open too
late to look like a lifetime bug.

So the table is keyed on `vk::Context::generation()` and re-resolved when that
changes. Anything else that caches device-derived state across a `shutdown()`
owes the same check; `vk::Stream` and `vk::Pipelines` instead drop their whole
`Impl` and re-initialise lazily, which is the other valid answer.

## Why frames are handles

`VideoPipeline::next()` returns a `FrameHandle`, not an image. Decoding and
*using* a frame are separate because the caller that motivates this — picking
the sharpest frame of every window — should not pay to convert and download the
frames it discards. Pixels are touched only when `queueSharpness()` or
`toImage()` ask for it, and the handle keeps the picture alive until released.
Presentation order needs a reorder queue anyway, so the pool and the explicit
release are not extra machinery.

The pool is sized `max_reorder + lookahead + max_dpb_slots + 4` pictures. At 4K
that is a few hundred megabytes, which is the price of holding a blur-selection
window in decoded form rather than re-decoding it.

## Frame numbers, and seeking by them

`FrameHandle::index` is the frame's place in the container's **presentation**
order, taken from the sample table (`Packet::display_index`) rather than
counted as pictures come out. Counting drifts: on a file the decoder loses a
picture in, every later frame would be renamed, and after a seek the count
would restart. Naming them from the container leaves a gap instead.

`VideoPipeline::seek()` jumps to the sync sample at or before a frame — the
reorder queue, the DPB pins and the codec's reference state all go, and
decoding resumes from the keyframe. It is what makes the GUI's frame slider
usable on a long capture: the last frame of a fifteen-minute GoPro clip is
35 s of decoding without it and 0.09 s with, and the frames it hands back are
bit-identical (154 probes over the local corpus, one keyframe interval apart
either side of the boundaries). Matroska has no seek here — no cue parsing —
and `seekSync()` returning false just means reading from the start.

## Colour

`color_matrix()` folds bit depth, studio/full range and the BT.601/709/2020
coefficients into one 3×4 matrix, so the shader has a single code path. Chroma
is box-averaged over the same source footprint as luma rather than bilinearly
interpolated; against ffmpeg's `swscale` that is worth a maximum error of about
3/255 on a 1080p frame, all of it at chroma edges.

Film grain synthesis is parsed but not requested (`apply_grain = 0`). It is a
cosmetic post-process, the frames feed a segmentation model, and asking for it
would force the distinct-output path on drivers that would rather not.

## HEIF stills

A phone's `.heic` is an ISO-BMFF file with no `moov`: the `meta` box names a
primary item, and on every recent iPhone that item is a `grid` of H.265
tiles (45 tiles of 640x896 for a 24 MP frame), each tile an independent intra
picture sharing one `hvcC`. `Heif.cpp` reads the item tables, hands the tiles
to `VideoPipeline` as a stream of key frames through an in-memory `Demuxer`,
and composes, crops (`clap`), turns (`irot`) and mirrors (`imir`) the result
in the order the item lists those properties. The EXIF item comes back as a
JPEG APP1 payload, which is what keeps the focal-length prior when the photo
becomes a JPEG in a dataset (`app/gui/HeifPhoto.h`).

Four things decided by measurement or by the spec's history:

1. **Main Still Picture needs no mapping.** The tiles are profile 3 and
   NVIDIA 610 accepts that profile for an H.265 session as it is.
2. **`imir` is read the amended way**: HEIF Amd 2 turned the first edition's
   `axis` into `mode` and inverted it, so 0 flips top-bottom. libheif, libavif
   and ffmpeg all read it so; on four rotated and mirrored copies of a real
   photo this decoder matches libheif 1.23 to 66.6 dB.
3. **Unspecified colour is BT.601.** A tile's VUI says full-range BT.601 on
   an iPhone; with no `nclx` and no VUI matrix, the pipeline's own
   height-based guess would pick by the TILE's height, so HEIF asks for
   BT.601 explicitly (`ConvertOpts`), as libheif and ffmpeg do.
4. **One decode at a time.** `decode_heif()` holds a process-wide lock across
   the whole pipeline: callers are worker pools, and the compute stream is
   one per process.

Against ffmpeg 8.1 on a 24 MP iPhone photo the pixels agree to 50 dB (at
most 2 levels, uniform across tiles); decode is ~95 ms a frame, most of it
the per-tile RGB conversion. An essential property it does not apply, a
non-H.265 image, or an overlay (`iovl`) is refused by name, and the app falls
back to ffmpeg. Alpha, depth and gain-map auxiliary images are ignored.

## Encode

`VideoEncoder` is the other direction: RGB frames in, H.264 High, H.265 Main
or AV1 Main out, through `VK_KHR_video_encode_*`. It is what `spirula encode` runs
(`src/app/cli/encode_main.cpp`), and that is what the GUI's render mode pipes
its frames into -- a separate process, because it needs this layer's Vulkan
device beside the engine's. `Mp4Writer` puts the stream in an MP4.

```
rgb24 ──► upload ──► rgb_to_nv12 (compute) ──► buffer→image copy ──► encode queue
          (Stream)   BT.709, studio range        (compute queue)       I then P frames
```

The shape of the stream is fixed and simple on purpose: a key frame every two
seconds, P frames each referencing the one before, two DPB slots (an image
each where the device allows), constant QP where the driver offers it
(`RATE_CONTROL_MODE_DISABLED`), a VBR target where it does not. AV1 is the
exception to "the one before" -- see 3 below. The parameter sets written into the file are the ones
`vkGetEncodedVideoSessionParametersKHR` returns, overrides included, never the
ones asked for.

Two things the drivers taught, both on NVIDIA 595:

1. **H.265's smallest coding block is 16x16.** An SPS allowing 8x8 is
   accepted, and the hardware then leaves out split flags a conforming
   decoder reads: NVDEC played the stream, ffmpeg's software decoder lost
   sync three CTB rows into every P frame and filled the rest green.
2. **The encode-feedback query comes back in 32-bit words** whatever
   `VK_QUERY_RESULT_64_BIT` asks for, so it is read as 32-bit.
3. **An AV1 P frame's reconstruction does not work as a reference.** A P
   frame predicted from the key frame decodes cleanly (any DPB slot); one
   predicted from a P frame drifts from the first such frame on, static
   content included, and no reference signalling, DPB layout, frame id,
   rate-control mode or picture structure tried changed a byte of the
   picture data. So every AV1 P frame predicts from its key frame: correct,
   about two and a half times the size of H.265 on a moving camera. The GUI
   gives AV1 to ffmpeg (SVT-AV1) when it has it.
4. **AV1 with a render size apart from the frame size is rejected** by
   decoders, so an odd width or height comes out one pixel larger, as it
   does for H.264 and H.265, whose 4:2:0 crop is in pairs of pixels.

`spirula encode --probe` encodes two small frames with each codec and prints
the ones that worked with the largest frame each takes: a device can list a
codec and still refuse a session.
H.264 is capped at the device's H.264 limit (4096 x 4096 on NVIDIA), so an 8K
360 video wants H.265.
