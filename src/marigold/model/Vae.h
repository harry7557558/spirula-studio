#pragma once
// Qwen-Image's VAE on one frame. It is Wan 2.1's causal video autoencoder; a
// causal 3D conv sees a single frame through its last temporal tap only, so the
// installer stored those taps and this is a 2D conv net. Feature maps are
// channel-last, as everywhere in nn/.

#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/io/Safetensors.h"
#include "nn/vk/Memory.h"

#include <cstdint>
#include <string>

namespace marigold {

class Vae {
public:
    // `prefix` "vae.encoder." takes quant_conv too, "vae.decoder." post_quant_conv.
    void load(const nn::SafetensorsFile& f, bool encoder, const char* tag);
    void release();

    // rgb [H, W, 3] in [-1, 1] -> the posterior mean [H/8, W/8, 16]. Marigold
    // samples the posterior; its std is ~2e-4 against unit-scale latents.
    void encode(nn::vk::Arena& a, const nn::Tensor& rgb, const nn::Tensor& mu) const;
    // z [h, w, 16], unnormalized -> [8h, 8w, 3], not yet clamped to [-1, 1].
    void decode(nn::vk::Arena& a, const nn::Tensor& z, const nn::Tensor& rgb) const;

    // Arena the larger of the two passes needs at H x W.
    static uint64_t arenaBytes(int H, int W);
    uint64_t deviceBytes() const { return w_.deviceBytes(); }

private:
    nn::Tensor res_block(nn::vk::Arena& a, const nn::Tensor& x, int cout,
                         const std::string& p) const;
    void attention(nn::vk::Arena& a, const nn::Tensor& x, const std::string& p) const;
    nn::Tensor up(nn::vk::Arena& a, const nn::Tensor& x, const std::string& p) const;

    nn::WeightStore w_;
};

}  // namespace marigold
