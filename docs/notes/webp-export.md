# Lossless background export

`spirula export-bg <checkpoint_dir|run_dir|state.tar> [output.webp]` writes a
1024×512 equirectangular SH background. A run directory resolves through the
same latest-checkpoint selection as resume. Without an output argument, the
image goes in the input directory, or beside an explicitly named `state.tar`.
Explicit output filenames are used unchanged. Checkpoint saves also write
`background.webp` automatically. Training's `--background-export-height` setting
(default 512, range 1–8192) controls the saved image height; width is twice that
value. The setting is saved in config.json and GUI presets and restored on resume.

`src/core/WebpWriter.cpp` is a portable, dependency-free VP8L encoder for packed
8-bit RGB and RGBA. It preserves all channel values, including RGB at zero
alpha. Dimensions are limited to 1–16384 per axis. The format follows the
[WebP lossless specification](https://developers.google.com/speed/webp/docs/webp_lossless_bitstream_specification).

The encoder chooses among left, top, average and clamped-gradient predictors
in 32×32 tiles, subtracts green from the residual red and blue channels, and
uses adaptive canonical Huffman codes. Back references use the preceding
pixel or row, with runs capped at 4096 pixels. The frequency floor is raised
until code lengths fit the format's limit. There is one entropy group and no
color cache, palette, lossy mode, animation or metadata support. Encoding
uses image-sized scratch storage; it is not a streaming writer.

No ffmpeg, cwebp, downloaded library or temporary PNG is involved. Export
returns failure for invalid inputs and failed file writes. Lossless refers
to the rendered RGB8 image: exporting an HDR SH field still applies the
saved gamut matrix and selected display transfer, then clamps/quantizes to eight bits.

## Verification

Both development builds include `webp_writer_test` and `background_export_test`.
`background_checkpoint` exercises GPU checkpoint reload and scene switching;
the CUDA build needs `SS_BUILD_BACKEND_TESTS=ON` for this test.

When system libwebp headers and a library are available, **only the writer test**
links them to decode and compare every pixel independently. Without them it
reports that pixel checks were skipped; passing a directory to
`webp_writer_test` emits `.webp` and matching packed `.raw` fixtures for an
external decoder. Production binaries never link libwebp.

The corpus covers RGB/RGBA, transparent colors, tile edges, maximum-width and
maximum-height strips, long runs, gradients, noise and randomized sizes.
For 2048×1024 RGB inputs, the initial test measured 816 bytes for a constant
image, 72,198 bytes for its smooth gradient and 6,292,136 bytes for noise.
These are synthetic fixtures from `webp_writer_test`, not compression ratios
promised for arbitrary backgrounds.
