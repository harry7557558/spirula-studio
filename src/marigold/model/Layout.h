#pragma once
// The shape of Marigold v2's network (Qwen-Image-Edit-2509's DiT and VAE), and
// the tensor names of the files the installer writes. The converter and the
// loader both read this, so every name is spelled once.

#include <cstdint>
#include <cstdio>
#include <string>

namespace marigold {

constexpr int kDim = 3072;
constexpr int kHeads = 24;
constexpr int kHeadDim = 128;
constexpr int kLayers = 60;
constexpr int kMlp = 12288;
constexpr int kTextDim = 3584;   // the prompt embeddings: Qwen2.5-VL's width
constexpr int kLatent = 16;      // VAE channels; a token is a 2x2 patch of them
constexpr int kPatch = 16;       // image pixels per token side
constexpr int kRank = 128;       // every LoRA; alpha is 128 too, so its scale is 1

// The VAE's per-channel latent statistics (its config.json): the transformer
// sees (mu - mean) / std.
inline const float kLatentMean[kLatent] = {-0.7571f, -0.7089f, -0.9113f, 0.1075f,
                                           -0.1745f, 0.9653f,  -0.1517f, 1.5508f,
                                           0.4134f,  -0.0715f, 0.5517f,  -0.3632f,
                                           -0.1922f, -0.9497f, 0.2503f,  -0.2921f};
inline const float kLatentStd[kLatent] = {2.8184f, 1.4541f, 2.3275f, 2.6558f, 1.2196f, 1.7708f,
                                          2.6052f, 2.0743f, 3.2687f, 2.1526f, 2.8652f, 1.5579f,
                                          1.6382f, 1.1253f, 2.8251f, 1.9160f};

// One projection of a block. q, k and v are stored fused, so one expansion
// and one GEMM serve all three; `parts` source matrices stack into it.
struct BlockLinear {
    const char* name;
    const char* src[3];
    int         parts;
    int64_t     N, K;
};

enum Lin { kImgQkv, kTxtQkv, kImgOut, kTxtOut, kImgFc1, kImgFc2, kTxtFc1, kTxtFc2, kLinCount };

inline const BlockLinear kBlockLinears[kLinCount] = {
    {"img_qkv", {"attn.to_q", "attn.to_k", "attn.to_v"}, 3, 3 * kDim, kDim},
    {"txt_qkv", {"attn.add_q_proj", "attn.add_k_proj", "attn.add_v_proj"}, 3, 3 * kDim, kDim},
    {"img_out", {"attn.to_out.0"}, 1, kDim, kDim},
    {"txt_out", {"attn.to_add_out"}, 1, kDim, kDim},
    {"img_fc1", {"img_mlp.net.0.proj"}, 1, kMlp, kDim},
    {"img_fc2", {"img_mlp.net.2"}, 1, kDim, kMlp},
    {"txt_fc1", {"txt_mlp.net.0.proj"}, 1, kMlp, kDim},
    {"txt_fc2", {"txt_mlp.net.2"}, 1, kDim, kMlp},
};

// The per-head q/k RMSNorm weights: image q, image k, text q, text k.
inline const char* const kBlockNorms[4] = {"attn.norm_q", "attn.norm_k",
                                            "attn.norm_added_q", "attn.norm_added_k"};

// The four whole-model projections. txt_in and norm_out.linear only ever see a
// constant input, so they are spent at install time; img_in and proj_out run.
inline const char* const kGlobalLinears[4] = {"img_in", "txt_in", "norm_out.linear",
                                               "proj_out"};

inline std::string block_tensor(int i, const char* what, const char* suffix = "") {
    char buf[96];
    std::snprintf(buf, sizeof buf, "blocks.%d.%s%s", i, what, suffix);
    return buf;
}

// __metadata__["format"] of the two installed files; a change to what either
// holds bumps the number and makes an old install a reinstall.
constexpr const char* kBaseFormat = "spirula-marigold-v2-base/1";
constexpr const char* kTaskFormat = "spirula-marigold-v2-task/1";

}  // namespace marigold
