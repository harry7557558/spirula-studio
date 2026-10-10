#pragma once
// The shape of a MoGe-2 or MoGe-3 checkpoint, read off the file rather than
// keyed to the model id.
//
// The three published variants differ in encoder width, depth and how many
// blocks the head reads, and every one of those is visible in the file: the
// widths in the weight shapes, the tapped blocks in which LayerNorm each
// `/encoder/norm*` node consumes. Deriving them means one code path, and a
// checkpoint that is not MoGe-2 fails while being read.

namespace moge {

struct Hparams {
    // ---- encoder (DINOv2, no register tokens) ----------------------------
    int embed_dim = 0;
    int depth = 0;          // transformer blocks
    int num_heads = 0;      // embed_dim / 64; DINOv2 keeps head_dim 64 at every
                            // width, and nothing in the file says so
    int patch = 14;
    int mlp_hidden = 0;
    int pos_grid = 0;       // 37, the grid the positional embedding is stored for
    // The blocks whose output the head reads, each through the same final
    // LayerNorm. vit-small and vit-base tap 2, vit-large taps 4.
    int n_taps = 0;
    int taps[4] = {-1, -1, -1, -1};

    // ---- neck and heads (ConvStack) --------------------------------------
    // Five levels at 1x, 2x, 4x, 8x and 16x the patch grid. Only the first
    // width follows the encoder; the rest are the same in every variant.
    static constexpr int kLevels = 5;
    int ch[kLevels] = {0, 0, 0, 0, 0};
    bool has_normal = false, has_mask = false, has_scale = false;

    // ---- refiner (MoGe-3's sparse 3D U-Net) ------------------------------
    // Absent from MoGe-2. Levels and widths come from the weights; the pooling
    // factors and the depth binning only from the .pt's model_config.
    static constexpr int kMaxRefinerLevels = 8;
    bool has_refiner = false;
    int refiner_levels = 0;
    int refiner_ch[kMaxRefinerLevels] = {};
    int refiner_pool[kMaxRefinerLevels] = {};   // factor from level i to i + 1
    float refiner_depth_resolution = 256.0f;    // log-depth bins per unit

    // ImageNet statistics over 0..1 values, carried in the graph as
    // `encoder.image_mean` and `encoder.image_std`; these are the fallback.
    float image_mean[3] = {0.485f, 0.456f, 0.406f};
    float image_std[3] = {0.229f, 0.224f, 0.225f};

    // The token budget MoGe's own inference offers, which is NOT in the ONNX
    // export -- it lives in `num_tokens_range` in the PyTorch config, and is
    // the same [1200, 3600] for all three variants.
    static constexpr int kMinTokens = 1200;
    static constexpr int kMaxTokens = 3600;
    // LayerNorm epsilon, spelled out by every norm node in the export.
    static constexpr float kNormEps = 1e-6f;

    int head_dim() const { return embed_dim / num_heads; }
};

}  // namespace moge
