# Swin Transformer (`src/swin/`)

The Swin v1 backbone that `src/birefnet/` (Swin-T / Swin-L) and `src/gdino/`
(Swin-T / Swin-B) both run, on the inference layer. One forward pass, two
weight spellings: the Microsoft original that BiRefNet ships and Hugging Face's
port that Grounding DINO ships. `stage_weights` maps either onto one set of
names and does the one-off work there:

- q, k and v are stacked into one `[3C, C]` projection (Hugging Face stores
  three), so a window's attention inputs are one GEMM;
- the relative position table is expanded to a dense `[heads, N, N]` bias per
  block, which attention reads directly (`AttnBias::Full`);
- shifted blocks use `AttnBias::Window`: the same bias plus Swin's -100 wherever
  a query and key came from different regions of the rolled map. The region
  ids are computed once per padded size and cached (`Backbone::labels`).

The cyclic shift and the zero padding to whole windows are folded into
`nn::window_partition` / `window_unpartition` (`shift`, and `accumulate` for
the residual), and PatchMerging's gather is `nn::patch_merge`. All of it is
general and lives in `nn/`, tested in `nn_ops_test`.

What differs between the two spellings beyond names: Hugging Face drops the
shift once a stage is no larger than one window (`Config::clamp_window`); the
original pads instead. Neither case arises at the sizes either model runs.
