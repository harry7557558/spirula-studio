# Delaunay on splat point clouds: why every loop is bounded

`src/mesh/Delaunay3D.cpp` is a port of geogram's parallel Bowyer-Watson
tetrahedralization. Geogram assumes points in general position and exact
predicates; meshing hands it neither, so several loops that are provably finite
under those assumptions are not finite here. This note records what the input
actually looks like, the three failures that came out of it, and the numbers
that chose the fixes. Read it before relaxing any bound in that file.

## The input is degenerate by construction

`generate_point_cloud()` (`src/mesh/OccupancyEvaluator.cpp`, kernel in
`src/mesh/Meshing.cu`) emits **7 points per Gaussian**: the centre and the six
endpoints `m ± k·σ_i·a_i` of the principal axes. Per splat that is

- three exactly collinear triples (`m-o`, `m`, `m+o`), the centre being exactly
  the midpoint,
- three sets of five coplanar points,
- six points on a common ellipsoid — an exactly cospherical sextuple whenever
  the splat is isotropic.

A trained scene adds near-duplicates where a whole splat is smaller than an ulp
of its centre, which collapses `m ± o` onto `m`. A thin axis alone does not:
each axis's offset is floored at a quarter of the largest
(`kSampleMinAxisRatio`, [2dgs.md](2dgs.md#meshing)). Without the floor, a
10M-point sample of a 15M-splat scene (coordinates spanning ±350) had 1508
exactly repeated points, and a 2dgs model collapses every splat's third axis.

None of this is pathological data — it is what every mesh export feeds in.

## Failure 1: the inexact walk never terminates (the reported hang)

`locate_inexact()` walks tet to tet toward the query point using plain double
orientation tests, and geogram bounds it with `max_iter = 2500` because those
tests are inconsistent near degeneracies and the walk can cycle. The bound is
written as

```c++
if(--max_iter != 0) goto still_walking;   // geogram's form
```

which does not end the walk at zero: it falls out to the next facet of the
*already advanced* tet, and a further step there decrements 0 to `2^32-1` and
re-arms the walk for four billion more iterations — repeatedly. One worker
thread then spins at 100% CPU while the rest of the process idles, which is
exactly the "stuck for 8 days with almost no hardware resources used" report.

Observed with the counters in the harness: on the 200k-point lattice below one
thread sat in a single `locate_inexact()` call for 4.8 billion steps and
counting (~57M steps/s). On the real `bicycle_4` cloud (7M points) the 2500-step
budget was hit 5 times in one run — each of those is a coin flip on whether the
run finishes in 30 seconds or never.

The fix decrements outside the facet loop and returns the current tet, which is
what the exhausted walk was always meant to hand back to the exact `locate()`.

## Failure 2: unbounded loops that a bound makes harmless

Once the walk is bounded, several other loops still are not:

| loop | old bound | now |
|---|---|---|
| exact `locate()` walk | `2*max_used_t_+16` — 1.5e9 steps of long-double `orient_3d` at 15M splats, and it overflows `index_t` past 2^31 cells | `MAX_WALK_STEPS` (2^20) |
| random restart in `locate()` / `locate_inexact()` | none — spins while no live unowned cell is drawn | `MAX_RANDOM_TRIES` (4096) |
| rollback retry in `run()` | none — a cell held by a thread that has already exited is retried forever | `MAX_ROLLBACKS` (2^20) |
| `--work_end_` at `work_begin_ == 0` | wraps to `NO_INDEX` and reads 2^32 entries past `reorder_` | loop guard |

Exceeding a bound drops one point. That is the same outcome the pre-existing
"genuine failure" path already had, and it is preferable to a run that never
ends: at 7 points per splat, losing a handful changes nothing downstream.

## Failure 3: a pinched conflict zone corrupts the mesh, then segfaults

The set of tets whose circumsphere contains the new point is a topological ball
only if the in-sphere predicate is consistent. `in_sphere_3d_SOS()` is a
long-double filter with a Simulation-of-Simplicity tie-break, so on cospherical
clusters it answers from the perturbation, and the conflict zone can come back
**pinched**: some directed edge of its boundary belongs to two facets.

`Cavity` detects exactly that — but the old one was fixed-size (128 facets) and,
on overflow, insertion fell through to `stellate_conflict_zone_iterative()`,
which checks nothing. On the lattice cloud 63% of the over-128 cavities were
pinched, and stellating one builds adjacency that points at `NO_INDEX`; reading
it back indexes `cell_to_v_store_[4*0xFFFFFFFF]` and the process dies. That
crash reproduces on 1.4M lattice points at both 1 and 8 threads.

A manifold boundary is necessary but not sufficient. It can still be a boundary
the point cannot see all of, and stellating one of those produces inverted and
zero-volume tetrahedra — which is worse than it sounds, because `locate()` walks
by orientation and gets lost among them. So insertion now requires both: the
boundary is manifold, and `p` sees the outer side of every finite boundary facet
(`cavity_is_star_shaped()`), which is exactly the condition for every cell
`stellate_cavity()` builds to come out positive.

Three changes follow from that:

- the cavity **grows** (generation-stamped open addressing, cleared in O(1)) so
  every insertion goes through the checked path, and the unchecked one is gone,
- a cavity that fails either test falls back to splitting the located
  tetrahedron alone, whose boundary is a tetrahedron and therefore always both,
- that split is taken only when `p` is strictly inside it, so it cannot create
  an inverted cell either; otherwise the point is dropped.

**The output is now always a valid complex** — every returned tetrahedron is
positively oriented, on every input tried. Degeneracy costs points, never
validity, which is the trade the rest of the pipeline can actually absorb.

**Why not just drop the pinched point.** Measured on 208k lattice points, one
thread: dropping gives 505k tets in 36 s. Dropping leaves the cospherical
cluster untriangulated, so the next point in it sees a larger conflict zone, is
pinched too, and the cascade eats half the cloud. The 1→4 split breaks the
cluster up, the zones stay small, and the same case takes 1.0 s.

The star-shape test costs one `orient_3d` per boundary facet on every insertion.
It pays for itself: 1.4M lattice points on one thread went from over 13 minutes
(inverted cells, walks wandering among them) to 5.7 s, and the surface cloud is
within noise of not having it.

## What real data does

Nothing, which is the point. Measured single-threaded, fixed vs unfixed:

| cloud | points | threads | tetrahedra | inverted | points kept |
|---|---|---|---|---|---|
| `bicycle_4` | 7.0M | 1 | 46,716,110 — *identical* to the unfixed run | 0 | 100% |
| 15M splats | 105M | 16 | 703,818,771 in 152 s | 0 | 100% |
| synthetic surface splats | 0.7M | 1 | 4,248,559 — *identical* | 0 | 100% |
| lattice (worst case) | 1.4M | 1 | 5,158,580 | 0 | 95.5% |

The 15M dataset is the scene the 8-day report came from. `bicycle_4` produced
zero pinched cavities and 46 cavities over 128 facets in a whole run, and came
out 4.8 s *faster* than before. Every gram of this is for the lattice column.

## Reproducing

`delaunay_degenerate` builds both clouds in process and runs them under a
watchdog, since the failure is a walk that never returns rather than a wrong
answer. The lattice case is the one that hangs the unfixed code; the surface
case is the shape a trained scene converges to and must stay fast.

## Still open

The predicates are filtered long double, not exact. Exact arithmetic
(Shewchuk-style adaptive expansions) would make the conflict zone provably a
ball and retire the pinch fallback altogether; the bounds above would stay as
belt and braces. Nothing in the current pipeline needs it — real scenes produce
no pinched cavities at all — so it has not been done.
