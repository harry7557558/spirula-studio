# Local-PCA regularizer

PCAsplat (Matias et al., arXiv 2610.11011) regularizes the splats' centres
directly in space instead of through renders, so it reaches splats no camera
sees well. For each splat `i` and its `K - 1` nearest centres, with
`C_i` the covariance of those `K` centres about their mean and
`lambda_1 <= lambda_2 <= lambda_3` its eigenvalues (`v_m` the eigenvectors):

```
L_PCA    = mean_i [ lambda_1 + gamma * (lambda_3 - lambda_2) ]          (w_PCA)
L_normal = w_normal * mean_i (1 - |<v_1, n_i>|) + w_surfel * mean_i min(s_i)
```

`n_i` is the rotation column of the splat's smallest scale. `lambda_1` is the
neighbourhood's mean squared distance to its fitted plane, so lowering it pulls
centres onto a common surface; the `gamma` term spreads them evenly across it.
The paper ramps `w_PCA` from 1 to 10 after 7k steps of photometric-only
training, `gamma = 0.001`, and gives no `K`, `w_normal` or `w_surfel`.

## The gradient

With fixed neighbourhoods and distinct eigenvalues,
`d lambda_m / d mu_j = (2/K) <v_m, mu_j - mean> v_m` (Greenbaum et al.), so a
member's gradient is `G (mu_j - mean)` with `G` the weighted sum of
`v_m v_m^T`. The normal term's gradient goes to the quaternion only (`v_1` is
held fixed), the surfel term's to the smallest log scale. The `1/N` of the
means is the live splat count, as for every other per-splat term.

The default mean optimizer (`--use-scale-agnostic-mean`) divides a mean's
step by the splat's screen radius this step. A splat outside every camera of
the step has radius 0, and any gradient on its mean then moves it by ~1/eps:
the first bicycle run with the paper's weights sent centres to 1e16 within a
step of the regularizer starting, and the scene never recovered. So with that
optimizer only splats the step projected take the mean gradient -- occluded
ones still do, out-of-view ones on the steps that see them -- while every
centre still counts towards its neighbours' PCA. Plain Adam
(`--use-scale-agnostic-mean 0`) takes it everywhere, as the paper does.

Each thread owns one neighbourhood, scatters `G (mu_j - mean)` into its
members' mean gradients with float atomics, and writes its own splat's
quaternion and scale gradient. The eigenpairs are five cyclic Jacobi sweeps on
the 3x3 covariance, which reach float rounding; the covariance is accumulated
about the owner's centre, which is a member, so the shift costs no precision.

## Scale (`--pca-reg-scale-agnostic`)

The terms do not change under a translation or a rotation of the scene: they
are eigenvalues of a covariance and an angle between two directions. They do
change with its scale, which the photometric loss does not: scaling by `s`
multiplies every `lambda` by `s^2` and `min(s_i)` by `s`. Training runs in the
dataset's own units (bicycle's cameras span 4.36 of them), so the same weights
are a different balance against the images on every capture -- a model in
millimetres would get 10^6 times the pull of one in metres. Within a scene,
the paper's mean is dominated by its sparsest neighbourhoods: on bicycle's 1M
splats before any regularization (k = 16), `lambda_1` runs from 7e-8 to 166,
and 1% of the neighbourhoods carry 55% of its sum.

`--pca-reg-scale-agnostic` measures each term in the units of what it is about:

```
L_PCA    = mean_i [ lambda_1 + gamma * (lambda_3 - lambda_2) ] / T_i,    T_i = lambda_1 + lambda_2 + lambda_3
L_surfel = mean_i min(s_i) / max(s_i)
```

`lambda_1 / T` is the surface variation of Pauly et al. (2002): 0 on a plane,
1/3 for an isotropic blob, 0.12 at the median of that bicycle model. The
normal term is an angle already. With the flag, the regularizer is invariant
to any similarity of the scene, and its gradients move with the splats:
`pca_reg_parity` transforms a cloud by one (scale 7.3) and checks it, along
with the paper's terms growing by `s^2` and `s`.

`T_i` and `max(s_i)` are held fixed in the gradient, so every neighbourhood
and splat is pulled in the same direction as by the paper's term, by
`1 / T_i` (or `1 / max(s_i)`) as much. Differentiating through `T_i` would
add a push that spreads the neighbourhood within its plane to keep `T_i`
constant: the derivative of a scale-free term has no component that shrinks
or grows the neighbourhood. A neighbourhood whose spread is below the float
rounding of its centres (`T_i <= 1e-12 |mu_i|^2`) contributes nothing.

The same weight means something else under the flag. On that bicycle model,
the per-splat gradient of `L_PCA` is 1.2x the paper's at the 1st percentile,
132x at the median and 8000x at the 99th (`1 / T_i` at 1.4, 141 and 8400).
A median splat pulled as hard as before needs about 1/130 of the weight; the
sparse background is pulled less than before. A global normalizer, the
cameras' extent, would also be scale-invariant, but would keep the sparse
background's dominance.

Bicycle, the default preset, 10k steps, `morton-best`, k = 16 from step 1000,
CUDA; flatness scored afterwards on the exact 16 nearest:

| terms, `w_PCA` | train, s | rgb_loss, 7k-10k | mean `lambda_1 / T` | median `lambda_1 / T` | median `lambda_1` |
|---|---|---|---|---|---|
| none | 89.5 | 0.086 | 0.124 | 0.123 | 8.4e-4 |
| paper's, 1 to 10 | 98.2 | 0.078 | 0.065 | 0.049 | 1.3e-4 |
| scale-agnostic, 1 to 10 | 97.7 | 0.110 | 0.0037 | 0.0007 | 8.8e-7 |
| scale-agnostic, 0.01 to 0.1 | 97.4 | 0.078 | 0.053 | 0.038 | 1.2e-4 |

(rgb_loss is the logged training loss, averaged over the 31 logged steps.)

The flag is compiled, not branched on: the neighbourhood pass exists per scale
mode (`pca_reg_owner_k*_sa`, the Vulkan entries `pca_owner_k*_sa`). It costs
the same as the paper's: on the 1M model, the tree search 10.78 ms against
10.67 on CUDA (14.0 and 14.0 on Vulkan), the cached list 1.93 against 1.97
(3.25 and 3.28). The Morton methods' neighbour choice depends on the
axes, since the curve walks a fixed grid: a rotation changes which
approximate neighbours they find, not the terms on given neighbours.

## Where it runs

`engine_pca_reg_step` (`engine/EnginePcaReg.cpp`) runs after the backward and
before the optim step. On the fused 3dgs / mip path the optimizer has no world
gradient buffers of its own, so the regularizer writes `pca_reg.v_*` and the
fused kernel loads them as world gradients (both backends already guard each
channel); elsewhere it adds into `engine().grad` scaled by `1 / grad_scale`.
The non-fused quantized-gradient path has no fp32 buffer and refuses.

Everything recomputed per step -- the sort, the Morton-ordered centres, the
search tree, the fused path's gradients -- is in the `RasterBwd` phase of the
alias arena (`POOL_ALIAS_TABLE`): the backward leaves that phase open with the
fused step's screen gradients in it, and nothing reads either after the optim
step. It adds to that phase, so the arena grows by whatever the phase now
exceeds the largest other one by. Only the cached methods own memory, booked
for `cap_max`.

## Finding the neighbours (`--pca-reg-knn`)

All of them start from one ordering: the centres' bounding cube, a 63-bit
Morton code (21 bits an axis, written as two 32-bit words so the Vulkan
shaders need no 64-bit integers), and a radix sort of `(code, index)`. The
sort's two key buffers then hold the Morton-ordered centres, and one of its
value buffers is the order, so the ordering costs 24 bytes a splat of arena.

| method | per step | owned memory per splat |
|---|---|---|
| `exact` | sort, tree, search | 0 |
| `exact-cached` | read cached ids | 4 (k + 1) bytes |
| `morton` | sort, window | 0 |
| `morton-cached` | gather by cached order, window | 4 bytes |
| `morton-best` | sort, best k of a window | 0 |
| `morton-best-cached` | read cached ids | 4 (k + 1) bytes |

- `exact`: a binary radix tree (LBVH) over the sorted keys, built bottom-up in
  one launch (Apetrei 2014): each point climbs while it is the second child to
  arrive, and the sibling's box is read with atomics, since another
  multiprocessor wrote it. Internal node `p` splits `[first, last]` after point
  `p`; `nodes[2p]`, `nodes[2p + 1]` are its box, `meta[p] = (left, right,
  first, last)`, a child `0x80000000 | point` when it is a single point, and
  `ranges[n - 1]` is the root. A node over at most 16 points is scanned rather
  than descended. 52 bytes a splat of arena, on the steps it is built.
  The query seeds its k nearest from `4(k + 1)` Morton neighbours, then
  descends nearest-first. The wave descends together, into any node one of its
  lanes still needs: its 32 queries are consecutive in Morton order. Exact,
  ties aside; `pca_reg_parity` holds it to brute force.
- `morton`: the `K` consecutive points around the splat in Morton order. No
  search, but a window that straddles a jump in the curve mixes distant pieces.
- `morton-best`: the `k` nearest among `--pca-reg-window` points of the curve
  (default `4K`), which repairs most of those jumps.
- `-cached`: the search runs on the first step, every
  `--pca-reg-refresh-every` steps (default `--refine-every`) and on the step
  after any densification -- relocation and growth renumber the splats, which
  `PcaRegState::splat_epoch` tracks. In between, the neighbour lists (or the
  Morton order) are reused while the centres move.

The k nearest so far live in registers: the neighbourhood pass is compiled for
capacities 8, 16 and 32 (the smallest that holds k), so every index into the
list is static and an insert is a compare-swap chain.

### What the search costs

`pca_reg_bench <splat.ply> [k] [iters]` times each part on a trained model.
Bicycle's 1M splats after 10k steps of the default preset, k = 16, RTX 5070,
device time per call:

| part | CUDA, ms | Vulkan, ms |
|---|---|---|
| order (box, keys, sort, gather) | 0.26 | 0.59 |
| LBVH build | 0.42 | 0.46 |
| gather by a stored order | 0.05 | 0.06 |
| neighbourhoods, window | 1.70 | 2.59 |
| neighbourhoods, best of 68 | 1.72 | 2.01 |
| neighbourhoods, cached list | 1.88 | 3.21 |
| neighbourhoods, tree search | 10.6 | 12.0 |

The neighbourhood pass without any search -- the moments, the eigenpairs, the
51 scattered atomic adds and the normal term -- is the floor every method
pays. The exact search costs about 5x it. What was measured on the way:

- The first search kept the k nearest in an array indexed by a running count,
  which spills to local memory: 21 ms on CUDA, and best-of-window 5.4 ms.
- Per-lane descents diverge; descending as a wave took the search from 21 to
  13 ms on CUDA (34 to 24 on Vulkan) on the first tree.
- That first tree was a balanced binary tree over leaves of 32 consecutive
  sorted points. Its node ranges cut across Morton prefixes, so its boxes are
  loose: a host replay visited 170 nodes and scanned 656 candidates a query,
  against 73 and 61 for a radix tree with buckets of 8. The radix tree took
  Vulkan from 21 to 12.7 ms; CUDA, then at 12.9, barely moved.
- What is left looks latency-bound: 128 registers (capacity 16) allow 16 warps
  a multiprocessor, and each node visit is a chain of dependent loads. A
  uniform cube of 1M points does more work (162 nodes, 593 candidates a query)
  in 7.4 ms, so bicycle's extra time is likely its uneven density. Not
  measured: splitting the search into its own lower-register kernel, or nodes
  that carry their children's boxes.
- No effect: a wider seed window, a branch-free skip, a 32-deep stack.

### What it costs a training run

Bicycle (`images`, 1237x822), the default preset, 10k steps, at the 1M
cap from step 5.3k; `--pca-reg-neighbors 16 --pca-reg-start-iter 1000` and the
default weights otherwise. CUDA, RTX 5070. The regularizer column is its
device time a step at 1M splats (`SS_PCA_REG_TIMING`), averaged across rebuild
and reuse steps; densification runs through step 10k, so the cached methods
rebuild every 100 steps.

| `--pca-reg-knn` | train, s | ms a step at 1M | regularizer, ms | arena, MiB | owned, MiB | peak (nvidia-smi), MiB |
|---|---|---|---|---|---|---|
| off | 89.5 | 10.3 | - | 61.6 | - | 706 |
| `exact` | 159.8 | 20.3 | 9.86 | 113.8 | - | 758 |
| `exact-cached` | 95.6 | 11.4 | 1.14 | 113.7 | 64.9 | 824 |
| `morton` | 94.5 | 11.2 | 1.01 | 64.2 | - | 708 |
| `morton-cached` | 93.3 | 11.0 | 0.81 | 64.3 | 3.8 | 714 |
| `morton-best` | 98.2 | 11.7 | 1.50 | 64.2 | - | 710 |
| `morton-best-cached` | 93.9 | 11.1 | 0.93 | 64.1 | 64.9 | 776 |

The cached exact search pays the LBVH's arena on its rebuild steps, so its
peak carries both that and its lists. How close the approximations come, as
recall of the exact k nearest on the 1M model (`pca_reg_bench`): the Morton
window finds 41-43% of them at k = 8, 16 and 31, the best of a `4(k + 1)`
window 66%.

`SS_PCA_REG_TIMING=<steps>` prints the device time of the ordering, the tree
and the neighbourhood pass during training, averaged over that many steps. The
loss terms are logged as `pca_lambda1`, `pca_tangent` and `pca_normal`
(unweighted means; the first two over `T_i` under `--pca-reg-scale-agnostic`).
