# Packed SH storage layouts

When `sh_value_bits != 32` the canonical SH parameter store is a packed
`uint8`/`uint16` cell buffer plus a `float2` (min, max) bounds table, decoded
per cell as `mm.x + (mm.y - mm.x) * (q / kQMax)` (`QuantizedTensor<BITS, 256>`,
`core/Tensor.h`). The SH *Adam state* (`QuantizedAdamState<8, 256>`) uses the
same cell addressing over its own buffer. FPBO keeps gradients in registers;
the separate backward uses fp32 or the gradient codec, according to the
quantization level.

One *cell* is one `(splat, SH basis j, channel ch)` triple. A splat owns
`R = 3 * num_sh_buffer` of them, where `num_sh_buffer` is the buffer's max SH
count, not the runtime warmup degree. A row with spherical-beta lobes
(`core/ColorBasis.h`) counts them in `num_sh_buffer` too, after the bands:
every layout below addresses a lobe's six numbers as cells like any other,
and they share the bands' bounds.

Two layouts exist, and every reader picks between them with the
`(bounds_stride, pair_pitch)` pair that `_sh_load_q8` / `_sh_load_q16`
(`shaders/harmonics.slang`) and their Vulkan mirror `_sh_load_qw`
(`backend/vulkan/shaders/sh_quant.slang`) take.

## Per-cell-block AoS -- `bounds_stride = 256`, `pair_pitch = 0`

Cell index is `R * gid + 3 * j + ch`, one bound per 256 consecutive cells.
This is what the non-FPBO optimizer path
(`FusedAppearanceOptim.cu`'s per-cell Adam kernel) writes: one thread per
cell, so its 256-thread block IS the bound block.

### Vulkan separate backward and Adam

The quantized projection backward uses fixed, unrolled SH/lobe indices in
both its bounds pass and its encode pass. This lets the driver scalarize the
gradient arrays. Inactive warmup cells still decode and re-encode with zero
contribution. Whole-word stores and masked atomic edge stores preserve the
packed gradient layout.

The separate quantized SH Adam combines neighboring cell codes with subgroup
reads instead of workgroup staging. Each packed word spans at most four
lanes, fitting the minimum eight-lane subgroup. Tail lanes contribute zero;
all lanes still reach the bounds reductions.

The quantized SH Adam pipeline specializes the colour-row stride, so splat
and cell addressing uses constant integer divisors. The launcher's slice
alignment and packed storage are unchanged. On an RTX 5070 this measured
11.645 against 11.982 ms for `fpbo_bench 5000000 40 3 0 adam`, medians of
three interleaved pairs. The 2.8% gain is modest; bounds, codec arithmetic
and optimizer precision remain the same.

RTX 5070, 5M splats, SH3, 40 timed calls, medians of three interleaved runs:

| benchmark mode | before (ms/call) | after (ms/call) |
|---|---:|---:|
| `fpbo_bench 5000000 40 3 0 qgrad` | 12.918 | 9.793 |
| `fpbo_bench 5000000 40 3 0 adam` | 12.770 | 12.548 |

The Adam improvement is small relative to the timing spread. These changes
preserve quantization and checkpoint formats. Parity covers SH4, warmup
bands, odd SH/lobe rows, partial optimizer words, native/fallback atomics,
and subgroup widths 8, 32 and 64.

Mip-NeRF 360 bicycle, `images_4`, 5M cap, 10k iterations, quantization level 1,
FPBO disabled: training took 5m 23.475s against 5m 42.841s, 5.6% less time.
After iteration 8,000, steps averaged 42.807 ms against 45.331 ms. These are
single independent training runs, with only the output paths differing in
their saved configurations.

## Per-splat-block, word-paired and transposed -- FPBO

`pair_pitch = 512`; `bounds_stride = 512 * Rw` where `Rw = (R + 1) / 2` is
the u32 words per splat. One bound per 256 splats, covering all their cells.
For splat `gid` in block `b = gid >> 8` at lane `l = gid & 255`, cell `c` is at

    idx = (b * Rw + (c >> 1)) * 512 + l * 2 + (c & 1)

i.e. a splat's cells sit two per u32 word, and the words are transposed across
the 256 splats of the block. `sh_cell_at` / `sh_fpbo_base` in
`shaders/harmonics.slang` are the only places that spell this out.

The transpose is what makes it worth having: FPBO runs one thread per splat,
so under the AoS layout a wave's load or store for one coefficient touches 32
different cache lines. Transposed, it touches two. Measured on a 5M-splat
bicycle step (RTX 4080 Super), the fused kernel went 13.8 -> 8.5 ms on CUDA
and 20.3 -> 13.1 ms on Vulkan, and the writeback no longer needs the
groupshared staging round that used to hide the strided stores.

The bounds index still falls out of `idx / bounds_stride`, because
`(c >> 1) * 512 + l * 2 + (c & 1) < Rw * 512` for every cell of the block --
so the decode path needs no separate block index.

### Vulkan FPBO staging

The quantized update retains updated SH values and the Adam codec's `(u,
log_s)` until the block-wide bounds reductions finish. Both the update and
encode loops use a constant 24-coefficient bound, `[ForceUnroll]`, and a guard
on the active degree. Fixed indices let the driver scalarize the staging
arrays and eliminate inactive bands. The colour VJP reads source coefficients
on demand, preserving the SH clamp and arithmetic order.

RTX 5070, `fpbo_bench 5000000 40 3 0`, medians of three interleaved runs:

| | dynamic staging | fixed indices + coefficient reader |
|---|---|---|
| ms/call | 39.307 | 14.442 |
| registers/thread | 255 | 240 |
| shared bytes/workgroup | 49,152 | 1,024 |

Pipeline statistics come from `VK_KHR_pipeline_executable_properties`.
The coefficient reader alone measured 37.788 ms/call against 39.306 in its
paired baseline: the staging loops account for most of the gain. Neither
change alters the packed layout, bounds reductions, or checkpoint format.

Mip-NeRF 360 bicycle, `images_4`, 5M cap, 10k iterations, quantization level 1:
FPBO training took 4m 15.364s against 6m 55.579s, 38.6% less time. After
iteration 8,000, steps averaged 32.528 ms against 57.314 ms. These are single
independent training runs; the fixed-input benchmark isolates the shader change.

## Allocation

The transposed layout addresses whole blocks, so both buffers are sized
`ceil(N / 256) * 256 * Rw * 2` cells rather than `N * R`: the tail block is
rounded up, and an odd `R` leaves one unused cell per splat.

## Past 2^32 cells

100M splats at SH3 is 4.6e9 cells, so cell indices are 64-bit on both
backends. CUDA needs nothing beyond that (`ShQuantAddr`, `core/Tensor.h`, is
`int64_t` throughout). Vulkan needs two more things:

- Every packed read and write goes through `elem_at`
  (`backend/vulkan/shaders/int64_compat.slang`). A plain `buf[i]` past 4 GiB
  wraps onto the start of the buffer -- drivers fold the `OpPtrAccessChain`
  element index into 32 bits.
- The bound index is `(cell >> 8) / (bounds_stride >> 8)`. That is exact for
  both layouts because both strides are a multiple of 256 (AoS is 256; FPBO
  is `Rw * 512`, and a splat's cells never leave its own block), and it keeps
  the divide in u32. A 64-bit divide is not an option here: Intel/ANV's
  compiler effectively hangs lowering the one slang emits.

Per-splat work hoists one 64-bit base -- `sh_addr_base`, or a word pointer
for the FPBO writeback and the grad-quant run -- and walks it with 32-bit
offsets, so the per-cell instruction count is what it was before.

The per-cell optimizer entries (`fused_adam_fwd`, `fused_adam_q`,
`fused_adam_qq`, `fused_adamtr_rgb_sh`) still take a u32 cell index. Their
launchers slice the parameter into runs of whole 256-cell blocks of whole
splats and advance every pointer per slice, so the shaders never see an index
they cannot hold (`optim_slice_cells`, `backend/vulkan/kernels/Optimizer.cpp`).
Only SH ever needs more than one slice.
