#pragma once
// The SAM 3 forward passes, one function per module.
//
// Each takes the loaded model, an Arena for its intermediates, device-resident
// inputs and a device-resident output. Nothing here ever copies an activation
// to the host: sam3.cpp routes every inter-stage tensor through CPU vectors
// because ggml's graph allocator forced it to, and that alone costs it hundreds
// of megabytes of PCIe traffic per frame.

#include "sam/Common.h"
#include "sam/model/SamModel.h"
#include "nn/Ops.h"
#include "nn/vk/Memory.h"

#include <cstdint>
#include <vector>

namespace sam {
namespace model {

// ---------------------------------------------------------------------------
// Shared attention helpers
// ---------------------------------------------------------------------------

// nn.MultiheadAttention with a fused in_proj_weight [3D, D] / in_proj_bias [3D]:
// the layout every transformer in the detector uses. Q, K and V read from
// independent sources, because pre-norm blocks feed Q/K the position-augmented
// features and V the plain ones.
//
// `residual`, when set, is added to the out_proj result (and may alias `out`).
void mha_fused(vk::Arena& arena, const nn::Tensor& out, const nn::Tensor& q_src,
               const nn::Tensor& k_src, const nn::Tensor& v_src, int64_t nq, int64_t nkv,
               const nn::Tensor& in_w, const nn::Tensor& in_b, const nn::Tensor& out_w,
               const nn::Tensor& out_b, int n_heads,
               nn::AttnBias bias_mode = nn::AttnBias::None, const nn::Tensor& bias = {},
               const nn::Tensor& residual = {});

// SAM's Attention module: separate q/k/v projections and an internal dimension
// that may be smaller than the embedding dimension (downsample_rate = 2 gives
// 128 over 8 heads).
void mha_split(vk::Arena& arena, const nn::Tensor& out, const nn::Tensor& q_src,
               const nn::Tensor& k_src, const nn::Tensor& v_src, int64_t nq, int64_t nkv,
               const nn::Tensor& qw, const nn::Tensor& qb, const nn::Tensor& kw,
               const nn::Tensor& kb, const nn::Tensor& vw, const nn::Tensor& vb,
               const nn::Tensor& ow, const nn::Tensor& ob, int n_heads,
               const nn::Tensor& residual = {});

// An MLP of `n` Linear layers with ReLU between them (the reference's `MLP`
// helper: bbox_embed, hypernetworks, IoU head, object-score head, ...).
// `prefix` is a printf format taking the layer index, e.g.
// "sam_dec.iou_prediction_head.layers.%d".
void mlp_relu(vk::Arena& arena, const nn::Tensor& out, const nn::Tensor& x,
              const SamModel& m, const char* prefix, int n_layers,
              nn::Act final_act = nn::Act::None);

// ---------------------------------------------------------------------------
// Image backbone
// ---------------------------------------------------------------------------

struct ImageFeatures {
    // Channel-last [S, S, neck_dim] FPN levels, finest first:
    // 0 = 4x grid, 1 = 2x, 2 = 1x, 3 = 0.5x.
    nn::Tensor det[4];
    nn::Tensor trk[4];
    int grid = 0;
    int orig_width = 0;
    int orig_height = 0;
    bool valid() const { return trk[2].valid(); }
};

// `image` is the preprocessed input, [img_size, img_size, 3] f32 channel-last.
// Results land in pool slots so they survive the arena rewind and can be reused
// across several prompts on the same frame. Dispatches on the model family.
void encode_image(const SamModel& m, vk::Arena& arena, const nn::Tensor& image,
                  ImageFeatures& out);

// SAM 3: the Perception Encoder ViT + two SimpleFPN necks, filling det[] and
// trk[] at 4x / 2x / 1x / 0.5x of the token grid.
void encode_image_vit(const SamModel& m, vk::Arena& arena, const nn::Tensor& image,
                      ImageFeatures& out);
// SAM 2: Hiera + FpnNeck, filling trk[0..2] at strides 4 / 8 / 16. The
// stride-32 level is computed for the top-down path and dropped (`scalp`).
void encode_image_hiera(const SamModel& m, vk::Arena& arena, const nn::Tensor& image,
                        ImageFeatures& out);

// Host-side preprocessing: bilinear resize to img_size in uint8, then
// (x/255 - mean) / std -- 0.5/0.5 for SAM 3, ImageNet statistics for SAM 2.
// Kept on the host and in double precision because it is the one step that must
// match torchvision's resize bit for bit -- a half-pixel difference here shifts
// every downstream feature.
std::vector<float> preprocess_image(const SamModel& m, const uint8_t* rgb, int width,
                                    int height);

// ---------------------------------------------------------------------------
// Text encoder
// ---------------------------------------------------------------------------

// `token_ids` has exactly text_ctx_len entries. Writes [ctx_len, text_out_dim].
void encode_text(const SamModel& m, vk::Arena& arena,
                 const std::vector<int32_t>& token_ids, const nn::Tensor& out);

// ---------------------------------------------------------------------------
// Detector (PCS path)
// ---------------------------------------------------------------------------

struct Exemplar {
    float cx, cy, w, h;  // normalized [0, 1], center form
    int   label;         // 0 = positive, 1 = negative
};

// Geometry (exemplar) encoder. Writes [n_boxes + 1, D]; the last token is CLS.
// With no exemplars this is the CLS token alone, which is what the reference
// feeds the fusion encoder for a text-only prompt.
void encode_geometry(const SamModel& m, vk::Arena& arena,
                     const std::vector<Exemplar>& boxes, const nn::Tensor& img_feats,
                     const nn::Tensor& img_pe, const nn::Tensor& out);

// Fusion encoder: 6 pre-norm layers of image self-attention + cross-attention
// to the prompt tokens. In-place on `feats` ([grid*grid, D]).
void fusion_encoder(const SamModel& m, vk::Arena& arena, const nn::Tensor& feats,
                    const nn::Tensor& pos, const nn::Tensor& prompt, int64_t n_prompt,
                    const nn::Tensor& prompt_bias, bool prefix_done = false);

void fusion_encoder_prefix(const SamModel& m, vk::Arena& arena,
                           const nn::Tensor& feats, const nn::Tensor& pos);

struct DetrOutputs {
    nn::Tensor queries;        // [num_queries, D]  -- post-norm object queries
    nn::Tensor boxes;          // [num_queries, 4]  -- cxcywh in [0, 1]
    nn::Tensor class_scores;   // [num_queries]     -- logits
    nn::Tensor presence;       // [1]               -- logit
};

void detr_decoder(const SamModel& m, vk::Arena& arena, const nn::Tensor& enc_feats,
                  const nn::Tensor& enc_pos, const nn::Tensor& prompt, int64_t n_prompt,
                  const nn::Tensor& prompt_bias, const nn::Tensor& prompt_valid,
                  DetrOutputs& out);

// Segmentation head: cross-attend the fusion output to the prompt, run the
// pixel decoder over the FPN levels, and dot the per-query mask embedding with
// the pixel embedding. Writes [n_queries, mask_size*mask_size] logits.
void segmentation_head(const SamModel& m, vk::Arena& arena, const nn::Tensor& enc_feats,
                       const ImageFeatures& feats, const nn::Tensor& queries,
                       int64_t n_queries, const nn::Tensor& prompt, int64_t n_prompt,
                       const nn::Tensor& prompt_bias, const nn::Tensor& out_masks);

// ---------------------------------------------------------------------------
// SAM decoder (PVS / tracking path)
// ---------------------------------------------------------------------------

struct SamDecoderOutputs {
    nn::Tensor masks;        // [num_mask_tokens, mask_size*mask_size] logits
    nn::Tensor iou;          // [num_mask_tokens]
    nn::Tensor obj_score;    // [1] logit
    nn::Tensor mask_tokens;  // [num_mask_tokens, D] -- source of the object pointer
};

// `image_embed` is [grid, grid, D] (memory-conditioned when tracking, plus
// no_mem_embed on a first frame); `sparse` is [n_tokens, D].
void sam_mask_decoder(const SamModel& m, vk::Arena& arena, const nn::Tensor& image_embed,
                      const nn::Tensor& sparse, int64_t n_sparse,
                      const nn::Tensor& feat_s0, const nn::Tensor& feat_s1,
                      SamDecoderOutputs& out);

// ---------------------------------------------------------------------------
// Video memory
// ---------------------------------------------------------------------------

// Memory encoder: mask logits + backbone features -> [grid, grid, mem_out_dim].
// `mask_logits` is [mh, mw] at any resolution.
void encode_memory(const SamModel& m, vk::Arena& arena, const nn::Tensor& mask_logits,
                   int64_t mh, int64_t mw, const nn::Tensor& pix_feats,
                   const nn::Tensor& out);

// Memory attention: conditions the current frame's tracker features on the
// memory bank. `prompt`/`prompt_pos` are [n_mem, mem_out_dim] built by the
// tracker; the last `n_ptr_tokens` entries are object-pointer tokens and are
// excluded from the key-side rotary encoding, exactly as in the reference.
void memory_attention(const SamModel& m, vk::Arena& arena, const nn::Tensor& curr,
                      const nn::Tensor& curr_pos, const nn::Tensor& prompt,
                      const nn::Tensor& prompt_pos, int64_t n_mem, int n_ptr_tokens,
                      const nn::Tensor& out);

}  // namespace model
}  // namespace sam
