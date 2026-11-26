#include "roma/model/Refiner.h"

#include "roma/model/Layers.h"
#include "nn/core/Error.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Stream.h"

#include <cmath>

NN_DECLARE_EMBEDDED_MODULES(roma)

namespace spirula::roma {
namespace {

struct DisplacementParams {
    uint64_t out, warp;
    uint32_t h, w, groups_per_row;
    float scale_x, scale_y;
};

struct UpdateParams {
    uint64_t out_warp, out_conf, warp, confidence, delta_warp, delta_conf;
    uint32_t h, w, confidence_channels, groups_per_row;
};

void refinement_block(Arena& arena, const Weights& weights, const std::string& name,
                       const Tensor& state, const Tensor& tmp) {
    nn::ConvOpts o;
    o.pad_x = o.pad_y = 2;
    o.bias = weights.get(name + ".conv_depthwise.bias");
    nn::conv2d_depthwise(tmp, state, weights.get(name + ".conv_depthwise.weight"), 5, 5, o);
    batch_norm(weights, name + ".norm", tmp, tmp, Act::Relu);
    conv_layer(arena, weights, name + ".conv_pointwise", state, tmp);
}

void concatenate(const Tensor& out, const Tensor& in, int offset) {
    nn::strided_copy(out.offsetElems(offset), in, out.rows(), in.cols(), in.cols(), out.cols());
}

}  // namespace

void fine_features(Arena& arena, const Weights& weights, const Tensor& rgb,
                   const std::array<Tensor, 3>& output) {
    ArenaScope scope(arena);
    NN_CHECK(rgb.ndim == 3 && rgb.dtype == DType::F32 && rgb.shape[2] == 3 &&
             rgb.shape[0] >= 4 && rgb.shape[1] >= 4, "RoMa fine features expect an RGB image of at least 4x4");
    const int indices[][4] = {{0, 3, -1, -1}, {7, 10, -1, -1}, {14, 17, 20, 23}};
    const int channels[] = {64, 128, 256};
    Tensor normalized = map(arena, rgb.shape[0], rgb.shape[1], 3);
    nn::add(normalized, rgb, weights.get("input.mean"), 1, -1);
    nn::div(normalized, normalized, weights.get("input.std"));
    for (int level = 0; level < 3; ++level) {
        ArenaScope level_scope(arena);
        const Tensor& out = output[level];
        NN_CHECK(out.valid() && out.dtype == DType::F32 && out.ndim == 3 &&
                 out.shape[0] == rgb.shape[0] / (int64_t(1) << level) &&
                 out.shape[1] == rgb.shape[1] / (int64_t(1) << level) && out.shape[2] == channels[level],
                 "RoMa fine feature output has an incompatible shape");
        Tensor x = normalized;
        if (level) {
            x = map(arena, out.shape[0], out.shape[1], channels[level - 1]);
            nn::maxpool2x2(x, output[level - 1]);
        }
        Tensor tmp = map(arena, out.shape[0], out.shape[1], out.shape[2]);
        for (int index : indices[level]) {
            if (index < 0) continue;
            const std::string p = "refiner_features.layers.";
            conv_layer(arena, weights, p + std::to_string(index), tmp, x);
            batch_norm(weights, p + std::to_string(index + 1), out, tmp, Act::Relu);
            x = out;
        }
    }
}

void refine(Arena& arena, const Weights& weights, int patch,
            const Tensor& features_a, const Tensor& features_b,
            const Tensor& warp, const Tensor& confidence,
            const Tensor& output_warp, const Tensor& output_confidence,
            float scale_x, float scale_y) {
    NN_ENSURE_EMBEDDED_MODULES(roma);
    ArenaScope scope(arena);
    NN_CHECK(patch == 1 || patch == 2 || patch == 4, "RoMa refiner scale must be 1, 2 or 4");
    const int feat = patch == 4 ? 256 : patch == 2 ? 128 : 64;
    const int proj = patch == 4 ? 192 : patch == 2 ? 48 : 12;
    const int disp = patch == 4 ? 79 : patch == 2 ? 23 : 8;
    const int radius = patch == 4 ? 3 : patch == 2 ? 1 : -1;
    const int hidden = patch == 4 ? 512 : patch == 2 ? 128 : 32;
    const int64_t h = features_a.shape[0], w = features_a.shape[1];
    auto valid_map = [&](const Tensor& t, int c) {
        return t.valid() && t.dtype == DType::F32 && t.ndim == 3 &&
            t.shape[0] == h && t.shape[1] == w && t.shape[2] == c;
    };
    NN_CHECK(h > 0 && w > 0 && valid_map(features_a, feat) && valid_map(features_b, feat) &&
             valid_map(warp, 2) && (valid_map(confidence, 1) || valid_map(confidence, 4)) &&
             valid_map(output_warp, 2) && valid_map(output_confidence, 4), "RoMa refiner map shapes differ");
    NN_CHECK(std::isfinite(scale_x) && std::isfinite(scale_y) && scale_x > 0 && scale_y > 0,
             "RoMa refiner displacement scales must be positive");
    const std::string name = "refiners." + std::to_string(patch);
    Tensor state = map(arena, h, w, hidden);
    {
        ArenaScope input_scope(arena);
        Tensor a = map(arena, h, w, proj), b = map(arena, h, w, proj);
        linear_layer(weights, name + ".proj", a, features_a);
        linear_layer(weights, name + ".proj", b, features_b);
        concatenate(state, a, 0);
        Tensor sampled = map(arena, h, w, proj);
        nn::grid_sample_points(sampled, b, warp, false);
        concatenate(state, sampled, proj);
        Tensor displacement = map(arena, h, w, 2);
        DisplacementParams p{displacement.ptr, warp.ptr, (uint32_t)h, (uint32_t)w, 0, scale_x, scale_y};
        const auto entry = nn::span_entry("roma.displacement", {displacement, warp});
        nn::vk::Stream::get().dispatchFlat(entry, {}, h * w, 256, &p, sizeof p, &p.groups_per_row);
        Tensor embedding = map(arena, h, w, disp);
        conv_layer(arena, weights, name + ".disp_emb", embedding, displacement);
        concatenate(state, embedding, 2 * proj);
        if (radius >= 0) {
            Tensor correlation = map(arena, h, w, (2 * radius + 1) * (2 * radius + 1));
            nn::local_correlation(correlation, a, b, warp, radius);
            concatenate(state, correlation, 2 * proj + disp);
        }
    }
    {
        ArenaScope blocks_scope(arena);
        Tensor tmp = map(arena, h, w, hidden);
        refinement_block(arena, weights, name + ".block1", state, tmp);
        for (int block = 0; block < 8; ++block)
            refinement_block(arena, weights, name + ".hidden_blocks." + std::to_string(block), state, tmp);
    }
    Tensor delta_warp = map(arena, h, w, 2), delta_conf = map(arena, h, w, 4);
    conv_layer(arena, weights, name + ".warp_head", delta_warp, state);
    conv_layer(arena, weights, name + ".confidence_head", delta_conf, state);
    UpdateParams p{output_warp.ptr, output_confidence.ptr, warp.ptr, confidence.ptr, delta_warp.ptr,
                   delta_conf.ptr, (uint32_t)h, (uint32_t)w, (uint32_t)confidence.shape[2], 0};
    const auto entry = nn::span_entry("roma.update", {output_warp, output_confidence, warp, confidence, delta_warp, delta_conf});
    nn::vk::Stream::get().dispatchFlat(entry, {}, h * w, 256, &p, sizeof p, &p.groups_per_row);
}

}  // namespace spirula::roma
