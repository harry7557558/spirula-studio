# The "splat x img" VRAM category

`SS_PROFILE=1` buckets every pool buffer (`core/PoolSlots.h`). **splat x img**
is the scratch whose size follows *(splats seen per image)* rather than the
splat count: the packed projection's compaction, the projected screen rows,
and the splat-tile intersection list. It is the category that decides whether
a large scene with heavy inter-image overlap fits, because it is the only one
that grows with the batch as well as with the model.

Three numbers describe a step, and every buffer below is one of them:

| symbol | meaning | art_gallery, `--cap-max 15000000` |
|---|---|---|
| `C * N` | (camera, gaussian) pairs the projection tests | 5 x 15.0M = 75M |
| `nnz` | pairs that survive the visibility test | 15.1M |
| `n_isects` | (splat, tile) pairs the raster consumes | 22M - 36M |

## Only this category grows

Splat buffers are sized for `cap_max`, so everything else a run allocates is
fixed once the first densify step has run -- which is what lets the trainer
forecast its peak from this category alone. Scratch requested per LIVE splat
that lives across a step (the sub-batch score sum, the raster backward's
world gradients, the PCA regularizer's caches) is Splat too:
`POOL_LIVE_SPLAT_TABLE` in `core/PoolSlots.h` makes the pool allocate it for
`cap_max` on first use, scaling the request by cap / live. Measured on bonsai
at 1/4 resolution, 228k -> 666k splats: splat stays at 1042.6 MiB (revised) /
1019.7 MiB (MCMC); without the table it climbs 11 MiB over the same run and
`other` another 15 MiB. A new buffer whose length follows the live count goes
in that table, or the forecast under-reads the peak -- unless it dies inside
one call, in which case it belongs in the arena (the densify scratch below).

The arena follows the same rule for its per-splat slices: one carved in the
Splat category asks the arena for cap / live times its bytes
(`DevicePool::_arena_bump`), so a phase's per-splat part is `cap_max`-sized
from the first step that carves it and only the per-pair slices (SplatXImg)
grow. `arena_phases()` reports the three apart -- `pixels`, `splats` (per
pair) and `capped` -- which is what the forecast projects from.

**Nothing a schedule turns on later allocates later.** A feature with a start
step reserves from step 0: the PCA regularizer counts its work buffer against
the raster-backward phase with `DevicePool::arena_expect` and allocates its
caches (docs/notes/pca-reg.md), and the calibration warp acquires the warped
ground truth's buffers before `--calib-warp-start-iter` (6 bytes a pixel that
used to appear at step 1000). A run that cannot hold them fails in its first
steps. A new feature with a start step does the same.

**A trial of something larger gives back what it took.** The pool only
grows, so the binning-tile search's rejected finer trials used to leave the
pair list and the tile phase sized for a bin the run never adopted -- 2.5-3x
a step's pairs. It now marks the pool before such a trial and releases to the
mark when the trial is rejected (`DevicePool::arena_mark` /
`arena_release`; docs/notes/binning-tile-size.md).

What those three rules and the regularizer's work buffer came to (Vulkan,
RTX 5070, 1M cap, process memory in MiB: held at the end of the run, and the
most at any step -- the difference is the search's finer trials, a dozen
steps each):

| run | before, end / most | after, end / most |
|---|---|---|
| garden 1/4, 30k | 500 / 504 | 457 / 487-504 |
| garden 1/4, floater suppression strong | 541 / 547 | 518 / 521-529 |
| bonsai 1/2, 30k | 605 / 608 | 518 / 582 |
| bonsai 1/2, strong | 659 / 663 | 575 / 583 |
| 360 preset, 5650 views, 10k | 1112 / 1112 | 1055 / 1106 |
| 360 preset, strong | 1259 / 1259 | 1202 / 1244 |

Time and held-out PSNR did not move: garden 5:10, 5:11 before and 5:12, 5:06,
5:11 after (26.97, 26.92 dB against 26.84, 26.97, 26.99); with strong 6:09,
6:06 against 6:14, 6:06 (26.15, 26.15 against 26.23, 26.14); bonsai 7:26
against 7:15 (29.88, 29.92) and 8:17 against 8:24, 8:29 with strong (28.81,
28.86). The bonsai times differ by which of 16 and 32 px the search settled
on, a tie it resolves either way in either build. london resumed at 15M
splats for 300 steps: 336-349 ms a step before, 334-338 after; with strong
386-393 and 388-396, the pool 9490 MiB before and 9364 after.

## What each buffer costs

Per element, after the 2026-08-30 pass:

| buffer | size | note |
|---|---|---|
| `proj.mask` | `C*N / 8` bytes | visibility bitmask |
| `proj.block_count`, `proj.scan` | `C*N / 32` bytes each | one popcount per 128-thread workgroup, and its scan |
| `proj.screen` | 40 x `nnz` | `SCR2_STRIDE` floats: xy, conic, opac, depth, rgb |
| `raster_bwd.v_screen` | 40 x `nnz` | its gradient, fp32 for the atomics |
| `proj.aabb` | 8 x `nnz` | 16-bit fixed point, shaders/aabb16.h |
| `proj.depths` / `camera_ids` / `gaussian_ids` | 4 x `nnz` each | |
| `isect.tiles_per_splat`, `isect.cum_tiles` | 4 x `nnz` each | arena |
| `isect.ids_a/b` | 8 x one band each | sort key `(tile_id << 32) \| depth`, arena |
| `isect.flat_b` | 4 x one band | the sort's alternate payload, arena |
| `isect.flat_a` | 4 x `n_isects` | the sorted list the raster reads |

## The bitmask compaction

The packed projection is two passes: one that projects every `(camera,
gaussian)` pair and records whether it is visible, and one that projects the
survivors again and writes them compacted. The link between them used to be a
full `int32` mask over `C*N` plus a full `int32` inclusive scan over `C*N` --
8 bytes per pair, 572 MiB of the 3324 MiB above, and two `C*N`-sized scan
passes of bandwidth.

It is now a bitmask. `shaders/packed_mask.h` fixes the geometry both backends
share: `SS_PMASK_BLOCK` threads per workgroup, `SS_PMASK_WORDS` words of bits.
The mask pass ORs its predicate into groupshared words and writes one popcount
per workgroup; only those `C*N / 128` counts are scanned; the compaction pass
recovers its output slot as *(scanned count of earlier workgroups) + (bits set
below this thread)*. 8 bytes per pair becomes 0.16, and the scan shrinks by
128x. Measured: 572.20 MiB -> 13.42 MiB.

Both passes MUST launch with `SS_PMASK_BLOCK` threads or the bit-to-word
mapping diverges silently, which is why the constant lives in a shared header
rather than twice.

## The 2^31 limits, and where they are checked

`flatten_ids`, the per-tile offsets and the compaction's output index are all
`int32`, so a batch is bounded at 2^31 splat-tile pairs and 2^31 (camera,
gaussian) pairs. Both are now checked rather than wrapped:
`packed_check_pair_count` (`kernels/projection/ProjectionPackedFwd.cuh`) and
`intersect_check_isect_count` / `intersect_check_tile_count` /
`intersect_check_image_size` (`kernels/tile/IntersectTile.cuh`).

Testing the `int32` prefix-sum total for negative is NOT enough, and a 91 GB
device is where that shows: past 2^32 the sum wraps back to a plausible
positive, the key buffers are then sized from it, and `intersect_tile_write`
walks its `while (cur_idx < max_idx)` window off the end -- an unkillable GPU
spin at ~6 W with no memory traffic, plus raw stores into whatever follows.
So `intersect_tile_count` accumulates the **exact** total itself, as a (low
word, carry) pair built with one `InterlockedAdd` per workgroup, and the host
checks that 64-bit value before the scan runs. The write pass additionally
clamps its window to the real allocation, so a miscount can never scatter.

## The 4 GiB rule for any one buffer

A shader may not reach past 4 GiB from a base pointer by *indexing*: drivers
fold the `OpPtrAccessChain` element offset into 32 bits, so `base[i]` wraps
onto the start of the buffer and corrupts it silently -- no error, no fault.
Widening the index to 64-bit in the SPIR-V does not help; the fold happens
below that. What does work is building the byte address explicitly, which is
what `elem_at()` (`backend/vulkan/shaders/int64_compat.slang`) does, and a
base pointer the host has already offset is fine at any size.

Every buffer whose length follows `n_isects` or `nnz` goes through `elem_at`.
The measured failure without it: `isect_ids` is 8 bytes per intersection, so
it crosses 4 GiB at 537M pairs -- on mandeltorus at 7680x7680 that is ~13M
splats, where PSNR fell from 21.7 to 16.0 and the step got 17x slower with
nothing reported. Under `SS_EMULATE_INT64` (`.noint64` blobs) `gindex_t` is
32-bit and `elem_at` degrades to plain indexing; `span_ok` in
`backend/vulkan/SortScanVulkan.cpp` rejects those spans on the host instead.

## Rejected: LichtFeld's two-pass 32-bit sort

Sorting `(tile, depth)` as one 64-bit key can be replaced by ordering the
splats by depth first and then *stable*-sorting the intersection list on the
bare 32-bit tile id -- both radix sorts here are stable, so ties keep the
depth order. That halves the key buffers (`isect.ids_a/b`: 8 bytes per
intersection to 4).

Implemented and measured on art_gallery, then reverted. Memory fell 170 MiB,
but GPU time rose 236 ms per 10 iterations (~4%):

| kernel | 64-bit key | depth pre-sort |
|---|---|---|
| sort (downsweep + hist + spine) | 1276 ms | 1103 ms |
| `intersect_tile_count` | 115 ms | 333 ms |
| `intersect_tile_write` | 179 ms | 359 ms |

Two things went wrong. The counting and key-write passes have to walk the
splats in depth order, so their reads of `proj.aabb` and the screen row become
a random gather -- ~2.9x and ~2x more sector traffic, +398 ms, which is more
than the sort saves. And the sort itself gained far less than the 3x drop in
bytes moved: the Vulkan downsweep's per-element cost is dominated by its
ranking barriers rather than key width, so a 32-bit key pair ran at 92 GB/s
against 191 GB/s for the 64-bit one.

Worth revisiting only together with:

- **The rank refinement.** Keep both passes in the original (coalesced) order
  and give each splat its slice base as `cum_ordered[rank[i] - 1]`, where
  `rank` is the inverse permutation. That trades the two gathers for three
  4-byte random accesses over `nnz`; estimated to recover ~100-140 ms of the
  398, i.e. still short.
- **A higher `n_isects / nnz` ratio.** art_gallery is the worst case for this
  trade at 1.95 (small frames, so a splat covers ~2 tiles). At 4K the sort
  cost scales with `n_isects` while the gather cost scales with `nnz`, so the
  same change should come out ahead. Measure before believing it.

## Sorting in bands

The sort needs both key buffers and a second payload buffer: 20 bytes a pair
that are dead once the offsets are written, beside the 4 the raster keeps. On
a scene whose views see most of the model through many tiles that scratch IS
the category -- ZipNeRF london at `--cap-max 15000000` (fisheye, 3504x2336):
14.3M visible pairs, 119M splat-tile pairs, 2.0 GiB of keys in the arena.

So the intersector sorts a band of tile rows at a time. Both passes take a row
range (`band_row0/1`): a band's count pass, scan, key write and sort run on
that band alone, the keys count tiles from the band's first
(`key_tile_base`), and the sorted payload lands in the band's stretch of
`isect.flat_a` -- the sort starts in whichever payload buffer leaves it there
(the Vulkan sort swaps once per 8 key bits; CUB's swap count is learned per
key width and a miss costs one copy). `intersect_offset` adds the band's base.

- **How many** (`intersect_bands_for`, `kernels/tile/IntersectTile.cuh`):
  enough that a band's 20 bytes a pair fit under what the arena's OTHER
  phases already hold, so the intersector stops being the phase that sizes
  it; never for under 6M pairs, never below 4M pairs a band, and at most
  2^26 / `nnz` bands (2 to 16), because every band reads all `nnz` boxes
  twice. With no other phase yet (a run's first forward, a viewer-only
  session) a band is 16M pairs. The VRAM forecast replays this same function
  (docs/notes/train-forecast.md), so change it there and nowhere else.
- **Which rows**: the band's share of the pairs at the pairs-per-row the last
  band measured (`intersect_band_rows`), halved and recounted when a band
  comes out more than 1.25x its share (`kIsectBandSlack`).
- **Identical output.** The row-or-column sweep of the ellipse test is chosen
  on the whole box before the rows are cut, so a banded splat keeps exactly
  its tiles; `render_parity` and `meshing_parity` dumps are byte-identical
  under `SS_ISECT_BANDS=3` and `=7` on both backends (`SS_ISECT_BANDS=<n>`
  forces the count, `=1` turns banding off).

london to step 8100 (the cap is reached at 5500), Vulkan, RTX 5070, MiB:

| | one sort | bands |
|---|---|---|
| owned `splat x img` | 1733 | 1256 |
| arena | 1956 | 839 |
| pool + scratch | 10404 | 8696 |

The 1.7 GiB is the keys (16 bytes of 113M-119M pairs down to four bands'
worth), `isect.flat_b` leaving the owned buffers, and 114 MiB of the Splat
category below. Time is unchanged: 300 steps resumed from the same 15M
checkpoint ran 344 ms a step before and 339 after (two alternating pairs,
~2% apart run to run), and the run of the pair that drew the 119M-pair view
peaked at 10397 MiB before and 8601 after. A band costs one more count and
write pass over the visible pairs -- 0.5 and 1 ms at 14M of them, against a
165 ms sub-batch -- which is what the 2^26 / `nnz` limit keeps bounded.

What is left of the category there: the screen rows (547 MiB) and the list
(433), then the arena, which the tile phase still sizes at four bands (0.77
GiB of per-splat slices against the raster backward's 0.65).

## The phase arena

`isect.ids_a/b` and the tile counts are allocated, used and dead inside
`do_intersect_tile_generic` -- no caller reads the key array it returns.
`raster_bwd.v_screen` does not exist until the backward, and is gone by the
next forward. They can never be live at once, so they share one allocation.

`POOL_ALIAS_TABLE` (`core/PoolSlots.h`) is the whole declaration: one row per
buffer, naming the `PoolPhase` its bytes are valid in. A phase-tagged slot
owns nothing; `DevicePool` carves it out of a single arena with a bump
allocator, and `pool_begin_phase()` resets the cursor. The arena therefore
costs the LARGEST phase instead of the sum:

Three paired art_gallery runs at `--cap-max 15000000` (the arena and the
intersection buffers both follow `n_isects`, which moves with the cameras a run
happens to sample, so pair the measurements):

| | alias off | alias on |
|---|---|---|
| pool + scratch, MiB | 6594.65 / 6537.27 / 6544.67 | 5971.80 / 6078.39 / 6021.85 |
| arena, MiB | - | 653.17 / 724.61 / 686.69 |
| GPU-exec, ms per 10 iters | 6256 / 5762 / 5923 | 5611 / 5942 / 6005 |

535 MiB off the pool on average (8.2%), for no measurable time: a bump
allocation is a pointer add, and the arena is reallocated only when a phase
asks for more than it holds. The saving is `raster_bwd.v_screen` in full --
the intersect phase is the larger of the two, so it sets the arena and the
gradient buffer rides along free.

The multi-scale loss's scratch is a third phase, `Loss`
([vram-high-res.md](vram-high-res.md)); at tens of megapixels it is the
largest of the three and sets the arena. The perceptual losses' scratch (the
SSIM blur and coverage maps, ~27 bytes a pixel) is in it too: they run once per
pyramid scale inside one `Loss` instance, so they take `acquire_reused`, which
hands a later scale back the first slice -- scale 0's, the largest.

`Densify` holds what a densify launcher allocates and drops before it returns:
the relocation mask and destination list, the draw's sort keys and index
buffers, the MCMC probabilities. Each launcher opens its own instance (they
run one after another, and each re-acquires the same draw buffers). 28 bytes
a splat on Vulkan, of `cap_max` from the first densify step on.

Bicycle at 1/4, `--cap-max 13000000`, RTX 3070, the step trained to the cap
(`SS_VRAM_TRACE` for the peak):

| | before | after |
|---|---|---|
| splat, MiB | 4567 | 4220 |
| image, MiB | 66.9 | 39.8 |
| arena, MiB | 296 | 347 |
| pool + scratch, MiB | 5420 | 5085 |
| process peak, MiB | 5548 | 5086 |

### Reservations: growing before anything is live

An arena that grows only at a phase switch has to overflow first: a call that
needs more than it holds gets private allocations for the slices that do not
fit, and the arena grows at the next switch. That overflow is a transient the
size of the growth, held beside the old arena, and the end-of-step VRAM
sample never sees it -- 128 MiB above the steady pool at 13M splats in the run
above, and as large as the intersect phase at tens of millions.

So an opener that knows its size says so: `pool_begin_phase(p, bytes)` grows
the arena to `bytes` plus what that phase has carved past its reservation
before (the tile phase's appearance image), while nothing is live. The raster
backward reserves `v_screen`, every densify launcher its lists. The tile
intersector learns `n_isects` only after its count pass; when that does not
fit (`phase_reserve`) it reopens the phase reserved and counts again -- one
count pass, on the steps the intersections grow. `Loss` and `ImageBwd` do not
reserve: they are sized by the image and grow only with the resolution.
Overflow buffers that remain are freed before the arena regrows, never held
beside the new one.

### Adding a buffer to a phase

1. Add its row to `POOL_ALIAS_TABLE`. If the phase is new, add the enumerator
   and the one `pool_begin_phase()` call that opens it -- put that call in the
   launcher that allocates the phase's first buffer, not in the engine layer,
   and give it the phase's size if the launcher can compute it.
2. Run the parity suite with `SS_POOL_ALIAS_POISON=1`, which fills the arena
   with 0xff at every phase switch. A buffer that is still being read turns
   into NaNs the tests catch instead of the plausible stale data they would
   not. A deliberately wrong row (tagging `isect.offsets`, which the raster
   backward reads) fails `raster_bwd_parity` even without the poison, and
   hangs with it -- that is the shape of the failure to expect.
3. `SS_POOL_ALIAS=0` turns the whole mechanism off (every slot owns its
   allocation again). It changes memory layout and nothing else, so it is both
   the A/B lever and the escape hatch if a lifetime claim turns out wrong.

### What the mechanism checks for you

- Acquiring a phase-tagged slot while a different phase is open throws, naming
  the buffer and both phases. That is the ordering bug caught for free.
- A request the arena cannot fit yet takes a private allocation for that call
  and raises the arena's target, so an unreserved phase that grows degrades to
  owned buffers for one call rather than overflowing.
- A second acquire of one slot in one phase throws, except through
  `acquire_reused`, which returns the first slice when it fits.
- The VRAM report marks aliased rows `arena` in the cap column (they own
  nothing) and prints the arena's one allocation as `of which alias arena`, so
  the category caps and the pool total still add up.

### Per-splat scratch that no longer owns memory

`raster_bwd.accum_weight` (114 MiB) and `fused_proj_bwd.cam_bounds` (4 bytes a
splat) are in `POOL_ALIAS_TABLE` under `RasterBwd`. Stale views in the
non-split training path are cleared before forward/backward and after densify,
saving ~53 to 114 MiB peak VRAM during backward without lifetime hazards.

The oversize split channel's draw weights (8 bytes a splat, held for `cap_max`)
are gone: the draw's key kernel computes `(oversize + floor) * score^blend`
itself from the accumulator it is handed (`add_splats_with_long_axis_split_tensor`'s
`oversize_accum`), which is the same arithmetic one launch earlier.

The projective-depth angular bound (4 bytes a splat) is a `RasterBwd` slice
when one backward covers the step -- the fused optimizer path, where it is
written after the raster backward and read by the optimizer step. A step that
accumulates over passes (split batches, unequal faces) keeps the owned buffer:
its bound is a maximum over passes, and the arena is reset between them.

Together, bonsai at `--cap-max 2000000`: the Splat category went from 172 to
160 bytes a splat at SH degree 0 and 356 to 344 at 3 on the fused path, 204 to
196 at degree 0 on the split one.

The PCA regularizer's scratch (`--floater-suppression strong`) is one slice,
`pca_reg.work`, in which the gradients take the sort's place: 32 bytes a splat
on the fused path where six slices took 52 (docs/notes/pca-reg.md).

## The packed AABB

`proj.aabb` is 8 bytes per visible pair instead of 16: 16 bits per edge, two
edges to a uint32 (`shaders/aabb16.h`, with `core/AabbQuant.cuh` and
`backend/vulkan/shaders/aabb16.slang` as the two implementations). 231 -> 116
MiB on art_gallery, and the same halving for the non-packed `[C, N]` box the
viewer and meshing paths allocate.

Measured before vs after on the SAME backend, which isolates the quantization
from everything else: `engine_train_parity` unchanged (tight max_abs 0,
rel_rms 0 over 12 optimizer steps; loose rel_rms 2.8e-10), `meshing_parity` /
`fpbo_parity` / `projqgrad_parity` bit-identical, and `engine_render_parity`'s
8-bit output byte-identical with 388 of 4.9M raw floats moved. What does move
is `render_parity`'s tile-offset array by up to 2 entries -- the extra tiles,
below the run-to-run spread in `n_isects`.

An edge is a uniform partition of [0, image extent], NOT an integer pixel.
Integer pixels look tempting because the projection already clamps the box
into the frame, but they round a sub-pixel splat up to a whole pixel -- and
mid/low-resolution frames, where the wasted high bits would be, are exactly
where splats ARE pixel-sized. The step is extent/65535: 0.009 px at 608 wide,
0.09 px at 6000.

Min edges round down and max edges up, which buys two things. The decoded box
always contains the float one, so a splat can gain a tile but never lose one
(measured: ~0.1% more splat-tile pairs, which the ellipse test then re-clamps
in the mode training uses). And the order is preserved, so "is this box empty"
-- the cull marker every backward pass tests -- is a comparison on the packed
halves that needs no scale at all.

The consumers that need the scale need the image size with it: the tile
intersector gained `image_width`/`image_height` parameters, and the 3DGUT
rasterizers already had them (they take the AABB *center* as the ellipse
center, since 3DGUT stores none of its own).

## Remaining opportunities, largest first

1. **The bilateral grids of a capture with thousands of views** (55 KiB a
   view: 37 of 16-bit cells, 18 of optimizer state; 298 of the 1105 MiB the
   360-camera preset holds at 5650 views and 1M splats). A step touches the
   grids of its own views only, so the table can live on the host with a
   device cache of a few hundred slots: the kernels already index it through
   the batch's camera indices, and a slot index goes in their place. What it
   needs: write-back of a step's slots (an asynchronous read, one step late),
   the checkpoint writing and reading host rows under the same names, and the
   TV readout taken over the cached slots. Lossless; not done.
2. **Packed screen rows** (up to 230 MiB). `opac` and `rgb` would survive fp16
   comfortably; `xy`, `conic` and `depth` would not. The blocker is
   `raster_bwd.v_screen`, which is accumulated with float atomics -- fp16
   atomic add is not portable to the Vulkan baseline, so only the forward row
   could shrink, and then the two buffers stop sharing a layout.
3. **The split path's gradient accumulators** (73 bytes a splat at SH 3, 28
   at SH 0, for `cap_max`). A step over several passes cannot use the fused
   optimizer, so it accumulates quantized world gradients. The SH part is
   rank-limited -- one RGB gradient per camera centre times that centre's
   basis -- so storing the RGB gradients per pass would be 6 bytes a pass
   instead of 45: 33 bytes a splat saved at two passes, nothing past seven,
   and only where every pass has one camera centre. At SH 0 there is no such
   structure: the 28 bytes are five 16-bit accumulators. Not done.
4. **Rasterizing in the sort's bands.** The bands bound the sort's scratch;
   the 4-byte list still holds every pair. Rasterizing band by band would
   bound that too, at the cost of every raster kernel learning a row range.

What does not pay: moving more per-pixel buffers into the arena. At high
resolution the loss is the arena's largest phase, and the buffers still owned
(the rendered maps, the normals from depth and their gradients, the loss map)
are all live across it, so they can only join the phase that sizes the arena.
The viewer's camera thumbnails are sized by the camera count now
(`viewer_thumbnail_size`: 64 px up to 4096 cameras, down to 16 px so that all
of them fit 64 MiB; they were 350 MiB at 22k images).
