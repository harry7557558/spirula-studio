# Local covariance regularization

The reference is [PCAsplat, arXiv 2610.11011v1](https://arxiv.org/html/2610.11011v1),
Eqs. (2)–(5). Each neighborhood contains its owner and `pca_reg_neighbors`
other centers, so the paper's `K` is this setting plus one. Eigenvalues are
ordered `lambda_1 <= lambda_2 <= lambda_3`, with eigenvectors `v_m`.

## Paper audit

The center covariance, division by `K`, eigenspectrum objective, smallest-scale
normal, quaternion normalization, and division by the number of live splats
in commit `91027e0c456dbc9a726422be735f7461d0035c43` agree with the published
formulas:

```
C_i      = mean_j [(mu_j - mean(mu)) (mu_j - mean(mu))^T]
L_PCA    = mean_i [lambda_1 + gamma * (lambda_3 - lambda_2)]
L_normal = mean_i [1 - abs(dot(v_1, n_i))]
L_surfel = mean_i min(s_i)
```

There was a missing derivative of the published normal objective: holding
`v_1` fixed updated only the owner's quaternion. The implemented derivative
now also propagates through the eigenvector and covariance. This follows the
published objective; the paper does not specify a stop-gradient on that normal.
For a covariance adjoint `G`, center gradients are `2 G (mu_j - mean) / K`.
The normal's contribution is

```
u = sum_(m=2,3) v_m * dot(v_m, dL/dv_1) / (lambda_1 - lambda_m)
G_normal = (u v_1^T + v_1 u^T) / 2
```

A repeated smallest eigenvalue has no unique normal. Normal alignment and
normal transparency are skipped when `lambda_2 - lambda_1 <= 1e-6 trace(C)`;
this is a relative conditioning guard, not a different eigenspectrum penalty.
At equal tangent eigenvalues the isotropy term uses the zero subgradient.
Covariance uses centered second moments in a second pass to avoid subtracting
nearly equal raw moments. Five cyclic Jacobi sweeps solve the 3x3 system.

Matching formulas does not reproduce the paper's full experiment:

- The paper uses a RaDe-GS base with PGSR multi-view geometric and NCC losses.
  These local terms do not supply those constraints.
- It starts PCA at 7k iterations and schedules its weight from 1 to 10, with
  `gamma=0.001`. It does not specify `K`, the normal/surfel weights, or the
  precise interpolation of that schedule.
- Scene units matter: scaling coordinates by `a` multiplies the paper's PCA
  loss by `a^2` and surfel loss by `a`. Sparse neighborhoods can dominate the
  mean. `pca_reg_scale_agnostic=false` does not change scene normalization.
- The separate `use_scale_agnostic_mean` optimizer divides a step by the
  current screen radius. Out-of-view centers have radius zero, so PCA center
  gradients are suppressed there to avoid unbounded steps. Set this optimizer
  flag to false for all-center updates as in the paper. Occluded but projected
  centers still receive gradients. Rotation, scale and opacity are not gated.
- Cached/approximate neighbor searches, densification, image losses, and Adam
  settings can change the result. For formula comparisons use `exact`.

## Covariance modes

`--pca-reg-covariance centers` is the default and the paper's covariance.
`opacity-centers` uses normalized opacity weights. `gaussian-mixture` also
includes each Gaussian's covariance, by the law of total covariance:

```
a_j = sigmoid(opacity_logit_j), beta_j = a_j / sum_k a_k
m   = sum_j beta_j mu_j
C   = sum_j beta_j [(mu_j-m)(mu_j-m)^T + Sigma_j]
Sigma_j = R_j diag(exp(2 log_scale_j)) R_j^T
```

For `opacity-centers`, omit `Sigma_j`; for `centers`, also set all weights
equal. This is a mixture of normalized Gaussian distributions with opacity
as mixture weight, not a volume-weighted integral of unnormalized kernels.
It measures both center thickness and the spatial extent of the splats.

All losses use the selected covariance. Mixture gradients reach centers,
log scales, quaternions and opacity logits, including weight normalization:

```
dL/dmu_j = 2 beta_j G (mu_j - m)
dL/dSigma_j = beta_j G
dL/dlogit_j = beta_j (1-a_j) [G : ((mu_j-m)(mu_j-m)^T + Sigma_j - C)]
```

The analogous expression without `Sigma_j` applies to opacity-weighted
centers. Fully transparent neighborhoods with summed opacity at most `1e-20`
are skipped. Opacity weighting can reduce the influence of a bad splat by
reducing its opacity; it is not exclusively a positional constraint.

## Effective rank

`--pca-reg-erank-weight` adds a mean over neighborhoods of

```
p_m = lambda_m / trace(C)
erank(C) = exp(-sum_m p_m log(p_m))
L_erank = max(erank(C) - 2, 0)^2
```

This dimensionless penalty is active only above rank two. The implementation
clamps roundoff-negative eigenvalues to zero and smooths the probabilities
as `(p + 1e-8)/(1 + 3e-8)`. It differentiates the trace, probabilities and
entropy. This differs from the existing per-splat `erank_reg` shape penalty.

Rank below two is allowed: this term alone does not prevent lines, and a
strongly elongated volume can also have effective rank below two. Exactly
isotropic covariance is stationary for any differentiable spectral penalty;
photometric gradients or perturbations must break that symmetry. Retaining a
small PCA thickness/isotropy term can be useful.

## Transparency along the normal

`--pca-reg-transparency-weight` adds the mean remaining transmittance of an
orthographic line through each owner's center, directed along its PCA normal.
Each neighbor, including the owner, contributes its projected Gaussian:

```
d_j = mu_j - mu_i, A_j = inverse(Sigma_j), n = v_1
rho_j^2 = min_t (d_j - t n)^T A_j (d_j - t n)
alpha_j = min(0.999, sigmoid(logit_j) exp(-rho_j^2/2))
T_i = product_j (1 - alpha_j)
L_transparency = mean_i T_i
```

Whitening by the Gaussian's rotation and inverse scales evaluates the line
minimum without a matrix inverse or subtracting large quadratic forms.
Transmittance is accumulated in log space. It is independent of ordering and
normal sign, so no depth sort or second direction is needed. Gradients include
the query center, neighbor centers, Gaussian shapes, opacity, and PCA normal.
The opacity cap matches the usual splat compositing bound; its saturated
branch has zero derivative. The term is invariant to scene scale.

This is a local coverage proxy over a finite KNN set, with one line per
center. It does not render a full view, sample holes between centers, enforce
thinness, or penalize layers separated along that line. Raising the owner's
opacity can satisfy it. Use it with geometric regularization and start weakly,
especially for vegetation, glass, or scenes with genuine transparency.

## Scale (`--pca-reg-scale-agnostic`)

The scale-agnostic variant divides each PCA term by its neighborhood trace
and the surfel term by the splat's largest scale. These denominators are
**held fixed in the gradient**: this is a reweighted contraction objective,
not the full derivative of the displayed ratio. It avoids a denominator
gradient that could reduce the ratio by spreading centers or enlarging splats.
The fixed denominators also apply in mixture mode. Normal alignment, effective
rank and transparency already have no units; effective rank differentiates
its normalization in both scale modes.

A zero-spread covariance contributes no normalized eigenspectrum gradient.
The numerical floor is `trace(C) > 1e-30`, independent of the world's origin.
The finite-difference checks freeze only these documented denominators.

## Starting experiments

Both new weights default to zero, covariance defaults to `centers`, and all
terms are enabled at `pca_reg_start_iter` when `pca_reg_neighbors > 0`. The new
weights are constant after that start; `pca_reg_weight(_final)` schedules only
the PCA eigenspectrum term. Diagnostics `pca_erank` and `pca_transparency` are
unweighted means, reported when the corresponding term is enabled.

These are starting ranges, not dataset-validated recommendations:

| Experiment | Settings |
|---|---|
| Paper units | `pca_reg_scale_agnostic=false`, `use_scale_agnostic_mean=false`, `pca_reg_covariance=centers`, `pca_reg_knn=exact`, PCA weight `1 -> 10`, tangent `0.001` |
| Normalized baseline | PCA weight `0.01 -> 0.1`, tangent `0.001`, normal `0.01`, surfel `0`, scale-agnostic true |
| Rank ablation | New rank weight `0.01`, then `0.03`, `0.1`; transparency `0` |
| Coverage ablation | Transparency weight `0.001`, then `0.003`, `0.01`; hold geometry settings fixed |
| Mixture ablation | Compare `centers`, `opacity-centers`, `gaussian-mixture` at the same normalized weights |

Try 15 or 31 neighbors initially and start at 7k before moving earlier.
For an isolated new-loss ablation, set **both** `pca_reg_weight` and
`pca_reg_weight_final` to zero, and normal/surfel weights to zero; the final
PCA weight otherwise still defaults to one. Normal alignment now moves
centers too, so old normal-weight tuning is not directly transferable.

## Execution and validation

The CUDA exports and Vulkan entries use the same Slang math. Vulkan specializes
on covariance mode, enabled normal/surfel/rank/transparency/PCA terms, and
neighbor-search mode. Zero weights remove their derivative branches at pipeline
creation; weight schedules do not create new pipelines while their nonzero
mask stays unchanged. CUDA has a separate specialization for center covariance
with both new weights zero, removing the optional mixture/coverage code.
Covariance and gradient accumulation reuse each search;
no extra neighbor lists or per-neighbor matrices are stored. Opacity gradients
need one extra float per splat when active.

All shared quaternion, scale and opacity gradient writes are atomic, including
the owner's contributions. The optimizers receive opacity gradients
without treating regularizer-only updates as camera observations.

`pca_reg_parity` checks double-precision forward values and finite differences
for all four parameter channels, each covariance mode, both scale modes,
separate losses and their combination. Its larger clouds test exact neighbors,
cached lists, approximate search, degenerate neighborhoods, visibility gates,
and similarity transformations. Opacity checks cover zero and saturated logits.
`pca_reg_step` exercises all three primitives,
split and fused optimizers, fp32 and quantized states, and unseen splats.
`pca_reg_bench` accepts optional covariance
index (`0`, `1`, `2`), rank weight, and transparency weight after its iteration
count for performance comparisons.

Steady-state timings on an RTX 5070, `uniform:100000 16 30`, in milliseconds
per neighborhood pass (sort/tree construction excluded; both scale modes are
available, this table uses paper units). The baseline enables PCA, normal and
surfel; rank adds weight `0.03`, transparency adds `0.003`.

| Terms / covariance | CUDA exact search | CUDA cached list | Vulkan exact search | Vulkan cached list |
|---|---:|---:|---:|---:|
| Baseline / centers | 0.83 | 0.17 | 0.82 | 0.21 |
| + rank / centers | 0.86 | 0.18 | 0.77 | 0.23 |
| + rank / mixture | 1.15 | 0.62 | 1.00 | 0.56 |
| + rank + transparency / mixture | 1.56 | 1.14 | 1.54 | 1.09 |

The original CUDA center-only implementation measured 0.66 / 0.14 ms for
exact search / cached lists on the same cloud; the corrected normal derivative
and centered covariance add work. Small differences between adjacent rows are
timing noise. Pipeline compilation and submit-budget calibration are warmed
before measurement. First use of a Vulkan specialization still incurs that
one-time compilation cost. Real neighborhood density and opacity affect cost;
these measurements do not establish training quality or full-step speed.

## Where it runs

`engine_pca_reg_step` (`engine/EnginePcaReg.cpp`) runs after the backward and
before the optim step. On the fused 3dgs / mip path the optimizer has no world
gradient buffers of its own, so the regularizer writes `pca_reg.v_*` and the
fused kernel loads them as world gradients (both backends already guard each
channel); elsewhere it adds into `engine().grad` scaled by `1 / grad_scale`.
A run with this regularizer leaves the non-fused path's gradient accumulators
in fp32 (`keep_fp32_grad`), quantization or not, because that is the buffer it
adds into. The fused path is unaffected; an engine caller that sets neither
gets the refusal in `engine_pca_reg_step`.

Everything recomputed per step -- the sort, the Morton-ordered centres, the
search tree, the fused path's gradients -- is in the `RasterBwd` phase of the
alias arena (`POOL_ALIAS_TABLE`): the backward leaves that phase open with the
fused step's screen gradients in it, and nothing reads either after the optim
step. It adds to that phase, so the arena grows by whatever the phase now
exceeds the largest other one by. Only the cached methods own memory, booked
for `cap_max`.

## One workgroup over the window

Best-of-window (`morton-best`, the default) with `pca_reg_uses_slab` true runs
`pca_reg_slab` in shaders/pca_reg.slang: 128 owners, the 256 ranks their windows
can reach copied once into shared memory, 26.6 KB against Vulkan's guaranteed
32 KB. It allocates nothing. Three phases, each ending at a group barrier:

1. Each thread fills two slots: world centre, opacity weight, `Sigma`, splat id.
2. Each thread is an owner. The k nearest are a min/max chain over packed
   `distance | slot` keys, one register a neighbour. It publishes `G / total`,
   `-(G : C) / total` and its mean, and sets its bit in every chosen target's
   `g_pca_who` row.
3. Each thread takes two targets and walks only the owners that chose them,
   summing `H = sum G_o / total_o`, `g = sum H_o d` and `s = sum d^T H_o d - (G_o : C_o) / total_o`.
   One flush then gives

```
dL/dmu    = 2 a g
dL/dSigma = a H
dL/dlogit = a (1 - a) (s + H : Sigma)
```

`d` stays relative to each owner, `(t - q_o) - mean_o`, so precision does not
depend on where the group sits; anchoring the slab to one splat loses the
whole group to a floater at the group's first rank.

A tree or cached-list neighbour can be any rank, so `exact`, the cached methods'
reuse steps, transparency and `--pca-reg-window` above 128 use the
neighbour-at-a-time path. `pca_reg_parity` holds the two to each other on the
lists the slab chose, every covariance and scale mode.

`pca_reg_bench` on bicycle's 1M splats, k = 31, gaussian-mixture, rank weight
0.01, RTX 5070, ms per pass (the extreme floater-suppression defaults):

| | Vulkan | CUDA |
|---|---:|---:|
| neighbour-at-a-time | 7.48 | 8.74 |
| slab | 2.13 | 1.67 |

What the 7.5 ms was: 352 float atomics an owner cost about 3 ms, the window
search 2.3, and the neighbour math and loads 2.3; the atomics overlap the rest,
so removing only them gave 4.55. On the slab, 26 atomics an owner remain, and
ablation splits the 2.13 ms into 0.9 scan and chain, 0.6 target walk, 0.6 the
rest. A centre-out scan with an early reject measured no faster, since every
lane of a warp must agree to skip.

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

### Initial center-only implementation: search measurements

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

### Initial center-only implementation: training measurements

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
