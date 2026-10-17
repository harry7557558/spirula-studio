# `spirula encode`

Encodes raw RGB frames from standard input into a video on the GPU. The GUI's
render mode pipes its frames into it. Vulkan builds with `SS_BUILD_SAM=ON` and
`SS_ENABLE_PATENTED=ON` only, on a device with a video-encode queue.

```text
spirula encode --size WIDTHxHEIGHT [--fps RATE] [--codec h264|h265|av1]
               [--quality 0|1|2] [--spherical] -o FILE  < frames.rgb
spirula encode --probe
```

Frames are `rgb24`, rows top to bottom, `WIDTH * HEIGHT * 3` bytes each, read
until the input ends. `FILE` is an MP4, or a raw stream when its name ends in
`.h264`, `.264`, `.h265`, `.265`, `.hevc` or `.obu`.

| flag | default | meaning |
|---|---|---|
| `--size WIDTHxHEIGHT` | required | Frame size. An odd size is encoded one pixel larger. |
| `--fps RATE` | 30 | Frame rate written to the MP4. |
| `--codec h264\|h265\|av1` | `h264` | Codec. An unknown name falls back to `h264`. |
| `--quality 0\|1\|2` | 1 | 0 best, 1 standard, 2 smallest. |
| `--spherical` | off | Mark the MP4 as a 360 video. |
| `-o`, `--output FILE` | required | Where to write. |
| `--probe` | | List the codecs this GPU can encode, one line each with the largest frame it takes (`h264 4096 4096`), and exit. |
| `-h`, `--help` | | Show help and exit. |

Exit status: 0 on success (for `--probe`: at least one codec works), 1 when
encoding fails, 2 when `--size` or `-o` is missing.
