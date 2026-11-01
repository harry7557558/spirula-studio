// The public surface: a checkpoint, an arena, and one image at a time.
//
// The device produces the three heads' RAW maps at the caller's resolution and
// everything after that is host arithmetic -- the exp remap, the normalization,
// the sigmoid, and the shift that turns an affine point map into a depth map.
// All four are one pass over a map we have to download anyway.

#include "moge/Moge.h"

#include "moge/Common.h"
#include "moge/model/Fetch.h"
#include "moge/model/Model.h"
#include "moge/model/Recover.h"
#include "nn/Ops.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Stream.h"

#include "core/Env.h"
#include "external/npy.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>

namespace moge {
namespace {

using nn::DType;
using nn::Tensor;

// Resolved and created once: every dump after the first would otherwise race
// to make the same directory, and npy::write_npy throws rather than creating
// one for itself.
const char* dump_dir() {
    static const char* dir = [] {
        const char* d = spirula::env("MOGE_DUMP");
        if (d) {
            std::error_code ec;
            std::filesystem::create_directories(d, ec);
        }
        return d;
    }();
    return dir;
}

void write_npy(const char* name, const float* data, const std::vector<int64_t>& shape) {
    npy::shape_t s;
    for (int64_t d : shape) s.push_back((unsigned long)d);
    npy::npy_data_ptr<float> d{data, s, false};
    npy::write_npy(std::string(dump_dir()) + "/" + name + ".npy", d);
}

// torch's nearest-neighbour resize at a given output SIZE: src = floor(dst *
// in/out). MoGe fits the shift on a 64x64 sample of the point map and this is
// how it picks the pixels.
int nearest_index(int o, int n_out, int n_in) {
    const int i = (int)((int64_t)o * n_in / n_out);
    return std::min(std::max(i, 0), n_in - 1);
}

}  // namespace

bool dump_enabled() { return dump_dir() != nullptr; }

void dump_tensor(const char* name, const Tensor& t, const std::vector<int64_t>& shape) {
    if (!dump_dir()) return;
    std::vector<float> host((size_t)t.numel());
    nn::tensor_to_host(t, host.data(), t.numel());
    write_npy(name, host.data(), shape);
}

struct Predictor::Impl {
    Model model;
};

Predictor::Predictor() : impl_(new Impl) {}
Predictor::~Predictor() { delete impl_; }

int Predictor::patchSize() { return 14; }

// Unlike sam/, aliked/ and metric3d/ this library carries no SPIR-V of its own
// -- every kernel it runs is a general op -- so there is no module set to
// register here (nn/vk/Pipelines.cpp registers the inference layer's).
void Predictor::load(const std::string& model) {
    impl_->model.weights.load(resolve_model(model));
}

bool Predictor::loaded() const { return impl_->model.weights.loaded(); }

uint64_t Predictor::deviceBytes() const { return impl_->model.weights.deviceBytes(); }

uint64_t Model::planArenaBytes(int64_t gh, int64_t gw, int64_t H, int64_t W) const {
    const Hparams& h = hp();
    const int64_t D = h.embed_dim;
    const int64_t N = 1 + gh * gw;

    // The five-level stack, and the deepest transient inside it: one level's
    // upsampled copy plus the residual block's two temporaries at the level
    // above. The neck and each head pay this, one at a time.
    int64_t stack = 0, transient = 0;
    for (int l = 0; l < Hparams::kLevels; ++l) {
        stack += (gh << l) * (gw << l) * h.ch[l];
        if (l + 1 >= Hparams::kLevels) continue;
        const int64_t up = (gh << (l + 1)) * (gw << (l + 1));
        transient = std::max(transient,
                             up * std::max(h.ch[l], h.ch[l + 1]) + 2 * up * h.ch[l + 1]);
    }

    // The widest thing live inside an encoder block: the running sequence, the
    // norm output, one temporary, and the fused qkv with the attention output
    // (or the MLP's hidden layer, which never overlaps them).
    const int64_t enc = gh * gw * D +
                        3 * N * D + std::max<int64_t>(4 * N * D, N * h.mlp_hidden);
    const int64_t heads = 2 * stack + (gh << 4) * (gw << 4) * 3;
    // The three output maps, the raw upload and the resampled network input.
    const int64_t io = H * W * 7 + H * W * 3 + (gh * h.patch) * (gw * h.patch) * 3;

    const int64_t peak = io + std::max(enc, heads + transient);
    // nn::conv2d chunks its column workspace to ~32 MiB and flash-decoding
    // takes its partials from the same arena.
    return (uint64_t)peak * 4 + (128ull << 20);
}

Prediction Predictor::predict(const float* rgb, int width, int height,
                              const PredictOptions& opts) {
    Model& m = impl_->model;
    NN_CHECK(m.weights.loaded(), "moge::Predictor::predict before load()");
    NN_CHECK(rgb != nullptr && width > 0 && height > 0,
             "moge::Predictor::predict: empty image");
    const Hparams& h = m.weights.hparams();
    const int64_t P = h.patch;
    NN_CHECK(width >= P && height >= P, "%dx%d is smaller than one %lldx%lld patch",
             width, height, (long long)P, (long long)P);

    // ---- the token grid ----------------------------------------------------
    // About `num_tokens` patches at the image's own aspect ratio, capped at
    // what the image holds -- more would upsample into the network for nothing.
    const double ar = (double)width / (double)height;
    const int64_t cap = (width / P) * (height / P);
    const double tokens = (double)std::min<int64_t>(std::max(opts.num_tokens, 1), cap);
    const int64_t gh = std::max<int64_t>(1, std::lround(std::sqrt(tokens / ar)));
    const int64_t gw = std::max<int64_t>(1, std::lround(std::sqrt(tokens * ar)));
    const int64_t ih = gh * P, iw = gw * P;

    m.arena.reserve(m.planArenaBytes(gh, gw, height, width));
    const uint64_t reserved = m.arena.capacity();
    vk::ArenaScope root(m.arena);
    m.ensureUv(gh, gw, ar);

    // ---- the input ---------------------------------------------------------
    Tensor image = nn::arena_tensor(m.arena, DType::F32, ih, iw, 3);
    {
        vk::ArenaScope scope(m.arena);
        Tensor src = nn::arena_tensor(m.arena, DType::F32, height, width, 3);
        // The graph normalizes AFTER the resize; doing it before is the same
        // number, because a bilinear resample is a convex combination and the
        // normalization is affine per channel.
        std::vector<float> host((size_t)height * width * 3);
        const float sc[3] = {1.0f / h.image_std[0], 1.0f / h.image_std[1],
                             1.0f / h.image_std[2]};
        const float off[3] = {-h.image_mean[0] / h.image_std[0],
                              -h.image_mean[1] / h.image_std[1],
                              -h.image_mean[2] / h.image_std[2]};
        nn::parallel_for(height, [&](int64_t y0, int64_t y1) {
            for (int64_t y = y0; y < y1; ++y) {
                const float* in = rgb + (size_t)y * width * 3;
                float* dst = host.data() + (size_t)y * width * 3;
                for (int64_t x = 0; x < width; ++x)
                    for (int c = 0; c < 3; ++c)
                        dst[x * 3 + c] = in[x * 3 + c] * sc[c] + off[c];
            }
        });
        nn::tensor_from_host(src, host.data(), (int64_t)host.size());
        nn::resize_bilinear(image, src);
        if (dump_enabled()) {
            std::vector<float> chw((size_t)height * width * 3);
            for (int64_t y = 0; y < height; ++y)
                for (int64_t x = 0; x < width; ++x)
                    for (int c = 0; c < 3; ++c)
                        chw[((size_t)c * height + y) * width + x] =
                            rgb[((size_t)y * width + x) * 3 + c];
            write_npy("input", chw.data(), {1, 3, height, width});
            // The token budget after clamping, so the reference is fed the same
            // integer and lands on the same patch grid.
            const float nt = (float)tokens;
            write_npy("num_tokens", &nt, {1});
        }
    }

    // ---- the network -------------------------------------------------------
    const bool want_points = opts.want_depth;
    const bool want_normal = opts.want_normal && h.has_normal;
    const bool want_mask = h.has_mask;
    const Features f = m.encode(image);
    Outputs o;
    m.decode(f, height, width, want_points, want_normal, want_mask, &o,
             std::max(opts.refine_steps, 0));

    // Growing the arena rebases it, so an under-reserve is silent corruption
    // rather than a fault: whatever was allocated first comes back as whatever
    // now sits at its old address. Check rather than trust planArenaBytes().
    NN_CHECK(m.arena.capacity() == reserved,
             "the arena grew from %.0f to %.0f MB mid-pass; planArenaBytes is wrong",
             (double)reserved / 1e6, (double)m.arena.capacity() / 1e6);

    const size_t n = (size_t)height * width;
    std::vector<float> points, normal;
    Prediction out;
    out.width = width;
    out.height = height;
    if (o.points.valid()) {
        points.resize(n * 3);
        nn::tensor_to_host(o.points, points.data(), o.points.numel());
    }
    if (o.normal.valid()) {
        normal.resize(n * 3);
        nn::tensor_to_host(o.normal, normal.data(), o.normal.numel());
    }
    if (o.mask.valid()) {
        out.mask.resize(n);
        nn::tensor_to_host(o.mask, out.mask.data(), o.mask.numel());
    }
    if (o.scale.valid()) nn::tensor_to_host(o.scale, &out.metric_scale, 1);

    // ---- the remap, the normalization and the sigmoid -----------------------
    nn::parallel_for((int64_t)n, [&](int64_t i0, int64_t i1) {
        for (int64_t i = i0; i < i1; ++i) {
            if (!points.empty()) {
                // remap_output='exp': z is exponentiated and xy is a slope.
                const float z = std::exp(points[(size_t)i * 3 + 2]);
                points[(size_t)i * 3 + 0] *= z;
                points[(size_t)i * 3 + 1] *= z;
                points[(size_t)i * 3 + 2] = z;
            }
            if (!normal.empty()) {
                float* v = &normal[(size_t)i * 3];
                const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                const float inv = 1.0f / std::fmax(len, 1e-12f);
                for (int c = 0; c < 3; ++c) v[c] *= inv;
            }
            if (!out.mask.empty())
                out.mask[(size_t)i] = 1.0f / (1.0f + std::exp(-out.mask[(size_t)i]));
        }
    });
    if (dump_enabled()) {
        if (!points.empty()) write_npy("points", points.data(), {1, height, width, 3});
        if (!normal.empty()) write_npy("normal", normal.data(), {1, height, width, 3});
        if (!out.mask.empty()) write_npy("mask", out.mask.data(), {1, height, width});
        write_npy("scale", &out.metric_scale, {1});
    }

    // ---- the shift that makes the point map a depth map ---------------------
    const double diag = std::sqrt((double)width * width + (double)height * height);
    const bool calibrated = opts.fx > 0.0f && opts.fy > 0.0f;
    const double cx = opts.cx >= 0.0f ? opts.cx : 0.5 * width;
    const double cy = opts.cy >= 0.0f ? opts.cy : 0.5 * height;
    // MoGe's own unit: the focal relative to half the image diagonal, which is
    // what makes `focal * xy / z` land on the normalized view plane.
    out.focal = calibrated ? (float)(2.0 * opts.fx / diag) : 0.0f;
    // Non-square pixels are absorbed into the v target rather than carried as a
    // second focal, which keeps this MoGe's own one-parameter objective and is
    // exact wherever fx == fy.
    const double vy = calibrated ? (double)opts.fx / opts.fy : 1.0;

    if (!points.empty()) {
        constexpr int kGrid = 64;
        std::vector<float> sp, suv;
        sp.reserve((size_t)kGrid * kGrid * 3);
        suv.reserve((size_t)kGrid * kGrid * 2);
        for (int oy = 0; oy < kGrid; ++oy) {
            const int y = nearest_index(oy, kGrid, height);
            for (int ox = 0; ox < kGrid; ++ox) {
                const int x = nearest_index(ox, kGrid, width);
                const size_t i = (size_t)y * width + x;
                if (!out.mask.empty() && out.mask[i] <= opts.mask_threshold) continue;
                sp.push_back(points[i * 3 + 0]);
                sp.push_back(points[i * 3 + 1]);
                sp.push_back(points[i * 3 + 2]);
                suv.push_back((float)(2.0 * ((double)x + 0.5 - cx) / diag));
                suv.push_back((float)(2.0 * ((double)y + 0.5 - cy) / diag * vy));
            }
        }
        const Recovered r =
            recover_shift(sp.data(), suv.data(), (int)(sp.size() / 3), out.focal);
        out.shift = r.shift;
        out.focal = r.focal;

        out.depth.resize(n);
        for (size_t i = 0; i < n; ++i) {
            const float z = points[i * 3 + 2] + out.shift;
            const bool ok = z > 0.0f &&
                            (out.mask.empty() || out.mask[i] > opts.mask_threshold);
            out.depth[i] = ok ? z * out.metric_scale : 0.0f;
        }
    }

    if (!normal.empty()) {
        out.normal.swap(normal);
        if (!out.mask.empty())
            for (size_t i = 0; i < n; ++i)
                if (out.mask[i] <= opts.mask_threshold)
                    out.normal[i * 3] = out.normal[i * 3 + 1] = out.normal[i * 3 + 2] = 0.0f;
    }
    if (!opts.want_mask) std::vector<float>().swap(out.mask);

    NN_LOG_DEBUG("[moge] %dx%d -> %lldx%lld tokens, scale %.3f, shift %.4f, focal %.4f, "
                 "arena %.0f MB\n",
                 width, height, (long long)gh, (long long)gw, (double)out.metric_scale,
                 (double)out.shift, (double)out.focal,
                 (double)m.arena.highWater() / 1e6);
    return out;
}

}  // namespace moge
