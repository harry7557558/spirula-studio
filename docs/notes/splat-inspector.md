# Per-splat buffer inspector (developer view)

`SS_DEV_MODE=1` adds every per-splat pool buffer to the training viewport's
buffer menu, and a field beside it for an expression over them. It is a
developer tool: Vulkan only, training sessions only (not a model opened from a
file), and not in the web viewer.

## Using it

- The menu lists the built-in views, then one entry per attribute. An entry is
  grayed while its buffer holds nothing -- gradients before the first step,
  refine scratch before the first refine, the Adam layout this run did not
  pick. Hovering one names the pool buffer it reads.
- The field takes an expression and applies it on Enter; picking an attribute
  from the menu puts its name there to start from. For example
  `ln((abs(g1_mean_x)+abs(g1_mean_y)+abs(g1_mean_z))/sqrt(max(g2_mean_x,g2_mean_y,g2_mean_z,1e-18)))`.
- Each live splat's value becomes a flat colour: the 0.5th..99.5th percentile
  of the finite values spans the ramp, the tails saturate, NaN sits at the
  bottom. The render then blends those colours like any other, and the image
  goes through the same min-max turbo as every single-channel view. The
  existing `refinement_score` view renders through the same path.

## Names

`<attribute>_<component>`, with a prefix for what is stored about it:

| prefix | buffer |
|---|---|
| (none) | the parameter |
| `v_` | its gradient |
| `g1_`, `g2_` | its Adam first and second moment |

Attributes: `mean` x y z, `quat` w x y z, `scale` x y z (log), `opacity`
(logit), `dc` r g b, and `sh_norm` -- the L2 norm of the SH bands past the DC,
without spherical-beta lobes, since 45 coefficients do not fit a menu. Each
name reads whichever store the run uses: fp32, or the quantized value, Adam
state or gradient codec in either layout.

The densification buffers follow (`radii`, `accum_score`, `accum_count`,
`visit_renders`, `visit_idle`, ...), then the refine scratch named after its
pool slot (`mcmc_sample_probs`, `wswr_out_idx`, ...), as the last refine left
it. Integer buffers read as their value.

## Expressions

`+ - * /`, `^` or `**` (right-associative, above unary minus: `-x^2` is
`-(x^2)`), comparisons `< <= > >= == !=` giving 1 or 0, parentheses, and
`abs sqrt ln log log2 log10 exp sin cos tanh sigmoid floor sign pow min max
clamp where`. `min`/`max` take any number of arguments; `where(c, a, b)` is
`c != 0 ? a : b`. Limits: 16 deep, 32 distinct names, 256 program words
(`shaders/splat_expr.h`). An error names the column; the field turns red and
shows it on hover.

## How it works

- `engine/EngineInspect.cpp` holds the table, `kSources`: per row the pool
  slot, its codec, its cell layout and its bound granularity. Rows sharing a
  name are the stores one attribute can live in; a run allocates one, and the
  first live one is read.
- `core/ExprProgram.cpp` compiles the text straight to a postfix program in
  the format `shaders/splat_expr.h` defines -- one word per op -- and also
  evaluates it on the host, which is what the parity test compares against.
- `backend/vulkan/shaders/splat_expr.slang` runs that program once per splat
  with a fixed 16-float stack, decoding every variable through the very
  codecs the optimizer kernels use (`optim_quant.slang`, `sh_addr.slang`).
  The program and its variable table go through the kernel-params ring, so
  nothing is allocated for them.
- The values come back to the host (4 bytes a splat), are normalized there,
  and go back as the debug forward's DC override, as `refinement_score` always
  did. VRAM: `viewer.splat_expr` (4 bytes a splat) on top of the
  `debug.features_dc` the existing views already use.

A new per-splat buffer is one row in `kSources`. A store that is neither plain
nor one of the four codecs needs a case in `_xp_decode`.

`raster_bwd.v_world` is left out: it is a fallback scratch whose packed row
depends on the primitive, and the gradients it feeds are listed as `v_*`.

## Tests

- `expr_program_test` -- the compiler: precedence, functions, limits, errors.
- `splat_inspect_parity` -- one seeded scene trained one step three ways
  (fp32, quantization level 1, level 1 through the fused optimizer). A plain
  buffer must read back exactly what the engine holds; every attribute the
  quantized runs decode must match the fp32 run's plain floats to the codec's
  precision; compound expressions on the device must match `eval_expr` over
  the device's own reads.

## If it is too slow

The interpreter is one dispatch that loops over the ops for each splat, which
is nothing next to the debug forward it feeds. Should a hot path ever want an expression -- the
region program partitioned and ROI training evaluate is the likely one -- the
postfix program maps one op to one SPIR-V instruction, so a minimal emitter
can turn it into a pipeline of its own without another front end.
