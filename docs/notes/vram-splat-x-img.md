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
| `proj.aabb` | 16 x `nnz` | float4, clamped to the image |
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

## Remaining opportunities, largest first

1. **Lifetime aliasing for phase-local scratch** (~550-1200 MiB). `isect.ids_a/b`
   are allocated, sorted and dead inside `do_intersect_tile_generic` -- no
   caller reads the returned key array. `raster_bwd.v_screen` is not allocated
   until the backward. They can never be live at once, so one allocation could
   serve both. The pool is one `device_malloc` per slot with no notion of
   lifetime, so this needs a real change: an arena with explicit scoping, and a
   fail-fast in-use flag rather than a comment, because getting a lifetime
   wrong here corrupts silently. The viewer/eval render path shares these
   slots, so it has to be part of the analysis.
2. **`proj.aabb` as 4 x int16** (16 -> 8 bytes per `nnz`, 115 MiB here). The
   projection already clamps the box to `[0, W-1] x [0, H-1]`, and image sizes
   are now checked against 32767, so the values fit exactly; round outward
   (floor the min, ceil the max) to stay conservative. Wide but mechanical:
   the type is threaded through both projections, the intersector, three
   backward launchers and the meshing raster in both backends, and the
   backward passes only use it as an "is this pair valid" predicate.
3. **Packed screen rows** (up to 230 MiB). `opac` and `rgb` would survive fp16
   comfortably; `xy`, `conic` and `depth` would not. The blocker is
   `raster_bwd.v_screen`, which is accumulated with float atomics -- fp16
   atomic add is not portable to the Vulkan baseline, so only the forward row
   could shrink, and then the two buffers stop sharing a layout.
4. **Splitting one training image into tile bands** -- low priority. It bounds
   `n_isects` per launch rather than per image, which is the only lever that
   helps the 4K case without touching precision. It does not help the
   `nnz`-sized buffers at all, which is why it is below the three above.
