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
| `isect.tiles_per_splat`, `isect.cum_tiles` | 4 x `nnz` each | |
| `isect.ids_a/b` | 8 x `n_isects` each | sort key `(tile_id << 32) \| depth` |
| `isect.flat_a/b` | 4 x `n_isects` each | sort payload |

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
`intersect_check_image_size` (`kernels/tile/IntersectTile.cuh`). The tile
counts are accumulated in `int32`: a total past 2^31 shows up negative, and
wrapping past 2^32 would need ~51 GB of keys to have been allocated first, so
the negative test is the whole check.

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

### Adding a buffer to a phase

1. Add its row to `POOL_ALIAS_TABLE`. If the phase is new, add the enumerator
   and the one `pool_begin_phase()` call that opens it -- put that call in the
   launcher that allocates the phase's first buffer, not in the engine layer.
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
  and raises the arena's target, so a growing `n_isects` degrades to today's
  behavior for one step rather than overflowing.
- The VRAM report marks aliased rows `arena` in the cap column (they own
  nothing) and prints the arena's one allocation as `of which alias arena`, so
  the category caps and the pool total still add up.

### Candidates not yet taken

`raster_bwd.accum_weight` (114 MiB) would fit the `RasterBwd` phase and balance
it against the intersect phase, for a net ~53 MiB. It is not tagged because
the non-split training path can leave `engine().fwd.accum_weight` pointing at
the previous step's buffer when a step produces none -- harmless today (stale
but plausible numbers), garbage once the bytes are reused. Fix that stale view
the way `_engine_train_step_split_batch` already does, then tag it.

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

1. **Packed screen rows** (up to 230 MiB). `opac` and `rgb` would survive fp16
   comfortably; `xy`, `conic` and `depth` would not. The blocker is
   `raster_bwd.v_screen`, which is accumulated with float atomics -- fp16
   atomic add is not portable to the Vulkan baseline, so only the forward row
   could shrink, and then the two buffers stop sharing a layout.
2. **Splitting one training image into tile bands** -- low priority. It bounds
   `n_isects` per launch rather than per image, which is the only lever that
   helps the 4K case without touching precision. It does not help the
   `nnz`-sized buffers at all, which is why it is below the one above.
