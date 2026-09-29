#pragma once
// The loaded Grounding DINO and its forward pass, in the order of Hugging
// Face's modeling_grounding_dino.py: text tower, backbone + projections,
// fusion encoder, two-stage query selection, deformable decoder.

#include "gdino/Tokenizer.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/vk/Memory.h"
#include "swin/Swin.h"

#include <map>
#include <string>
#include <utility>
#include <vector>

namespace gdino {

struct Hparams {
    int d_model = 256;
    int enc_layers = 6, dec_layers = 6;
    int enc_heads = 8, dec_heads = 8;
    int enc_points = 4, dec_points = 4;
    int levels = 4;
    int queries = 900;
    int text_layers = 12, text_heads = 12, text_dim = 768;
    int max_text = 256;
};

// The head's raw output for one image: logits over the prompt's tokens and
// normalized cx, cy, w, h boxes, both for every query.
struct HeadOutput {
    int queries = 0, tokens = 0;
    std::vector<float> logits;   // [queries, tokens]
    std::vector<float> boxes;    // [queries, 4]
};

class Model {
public:
    ~Model();

    void load(const std::string& weights, const std::string& vocab);
    bool loaded() const { return loaded_; }
    void release();
    uint64_t deviceBytes() const { return w_.deviceBytes(); }
    const Tokenizer& tokenizer() const { return tok_; }

    // `image` is [H, W, 3] f32 on the host, ImageNet-normalized.
    HeadOutput run(const std::vector<float>& image, int H, int W,
                   const std::vector<int32_t>& ids);

private:
    struct Levels {
        int n = 0;
        int h[4] = {}, w[4] = {};
        int64_t tokens() const {
            int64_t t = 0;
            for (int l = 0; l < n; ++l) t += (int64_t)h[l] * w[l];
            return t;
        }
    };
    // Everything that depends on the feature map sizes only, built once per
    // input size: the position embeddings, the encoder's reference points, and
    // the two-stage proposals with the rows they invalidate.
    struct Geometry {
        nn::DevicePtr blob = 0;
        nn::Tensor pos;      // [N, 256]  sine + level embedding
        nn::Tensor refs;     // [N, 2]
        nn::Tensor valid;    // [N]       1 where the proposal is inside (0.01, 0.99)
        std::vector<float> proposals;   // [N, 4] logits; +inf where invalid
    };
    // The text tower's output for one token sequence -- it does not depend on
    // the image, so a capture's frames all reuse it.
    struct Text {
        std::vector<int32_t> ids;
        nn::DevicePtr blob = 0;
        nn::Tensor feat;           // [T, 256] after text_projection
        nn::Tensor mask4;          // [4, T, T]  the enhancer's block mask
        nn::Tensor pos;            // [T, 256]   sine of the per-phrase positions
    };

    nn::Tensor get(const std::string& n) const { return w_.get(n); }
    const Geometry& geometry(const Levels& lv);
    const Text& text(const std::vector<int32_t>& ids);
    void encode_text(Text& t);
    uint64_t planArena(int H, int W, int T) const;

    void linear(const nn::Tensor& out, const nn::Tensor& x, const std::string& name,
                nn::Act act = nn::Act::None, const nn::Tensor& residual = {});
    void layer_norm(const nn::Tensor& out, const nn::Tensor& x, const std::string& name,
                    const nn::Tensor& residual = {}, float eps = 1e-5f);
    void mlp(const nn::Tensor& out, const nn::Tensor& x, const std::string& name, int layers);

    Hparams       hp_;
    nn::WeightStore w_;
    swin::Backbone bb_;
    Tokenizer     tok_;
    nn::vk::Arena arena_{"gdino"};
    std::map<std::pair<int, int>, Geometry> geo_;
    Text          text_;
    bool          loaded_ = false;
};

}  // namespace gdino
