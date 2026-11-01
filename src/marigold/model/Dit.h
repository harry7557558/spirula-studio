#pragma once
// Qwen-Image-Edit-2509's transformer at Marigold v2's one timestep. What
// depends only on that timestep or on the prompt was spent at install time
// (model/Install.h), so a pass is the 60 double-stream blocks over a batch of
// images, each with its own copy of the prompt's tokens.

#include "marigold/model/Fetch.h"
#include "marigold/model/Streamer.h"
#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/io/Safetensors.h"
#include "nn/io/StageDump.h"
#include "nn/vk/Memory.h"

#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace marigold {

class Dit {
public:
    void load(const nn::SafetensorsFile& base);
    void loadTask(Task t, const nn::SafetensorsFile& task);
    void release();

    // mu: B posterior means [h, w, 16]; z: B latents [h, w, 16] out, in the
    // VAE's own scale. h and w are even. `dump` takes the first image's stages.
    void forward(nn::vk::Arena& a, Streamer& s, Task t, const std::vector<nn::Tensor>& mu,
                 const std::vector<nn::Tensor>& z, const nn::StageDump* dump = nullptr);

    int textTokens(Task t) const { return text_tokens_[(int)t]; }
    // The arena one pass over `batch` images of `tokens` image tokens takes.
    uint64_t arenaBytes(int64_t tokens, int batch) const;
    // The persistent scratch a projection is expanded into.
    static uint64_t scratchBytes();
    uint64_t deviceBytes() const;

private:
    nn::Tensor expand(const BlockLin& l, const BlockLinear& L, const nn::Tensor& row_scale);
    nn::Tensor mod(int block, int stream, int which) const;
    const std::pair<nn::Tensor, nn::Tensor>& rope(int h, int w, Task t);

    nn::WeightStore w_;
    nn::WeightStore task_w_[2];
    int text_tokens_[2] = {};
    nn::DevicePtr w16_ = 0, ba_ = 0, bias_ = 0;
    // (h, w, task) -> image and text RoPE tables, [n, 64, 2] (cos, sin)
    std::map<std::tuple<int, int, int>, std::pair<nn::Tensor, nn::Tensor>> rope_;
    nn::WeightStore rope_w_;
};

}  // namespace marigold
