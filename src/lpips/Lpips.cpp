// LPIPS's forward pass, next to torchmetrics' functional/image/lpips.py.
//
// A view the workspace cannot hold is scored in tiles. A tile starts on a
// multiple of the nets' total stride and carries a halo as wide as the deepest
// tap's receptive field, so every tap position it owns is computed from the
// same pixels and the same padding as in the whole image: tiled and untiled
// scores differ by summation order only (lpips_test checks it).

#include "lpips/Lpips.h"

#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/WeightStore.h"
#include "nn/core/Error.h"
#include "nn/core/Parallel.h"
#include "nn/io/TorchCheckpoint.h"
#include "nn/vk/Context.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstdio>
#include <exception>
#include <vector>

// Registered from load() and score(), not a static initializer: ss_lpips is a
// static archive (nn/vk/EmbeddedSpirv.h).
NN_DECLARE_EMBEDDED_MODULES(lpips)

namespace lpips {
namespace {

namespace vk = nn::vk;
using nn::DType;
using nn::Tensor;

// A conv of torchvision's `features`, always followed by its ReLU.
struct Conv {
    int feature, k, stride, pad;
};

// The layers between two LPIPS taps: an optional MaxPool2d(pool, 2), then
// convs whose last ReLU is the tap.
struct Stage {
    int pool;
    std::vector<Conv> convs;
};

// torchmetrics' Alexnet and Vgg16 slices.
const std::vector<Stage>& stages(Net net) {
    static const std::vector<Stage> alex = {
        {0, {{0, 11, 4, 2}}},
        {3, {{3, 5, 1, 2}}},
        {3, {{6, 3, 1, 1}}},
        {0, {{8, 3, 1, 1}}},
        {0, {{10, 3, 1, 1}}},
    };
    static const std::vector<Stage> vgg = {
        {0, {{0, 3, 1, 1}, {2, 3, 1, 1}}},
        {2, {{5, 3, 1, 1}, {7, 3, 1, 1}}},
        {2, {{10, 3, 1, 1}, {12, 3, 1, 1}, {14, 3, 1, 1}}},
        {2, {{17, 3, 1, 1}, {19, 3, 1, 1}, {21, 3, 1, 1}}},
        {2, {{24, 3, 1, 1}, {26, 3, 1, 1}, {28, 3, 1, 1}}},
    };
    return net == Net::Vgg ? vgg : alex;
}

constexpr int kPoolStride = 2;
// Both nets' total stride: a tile's origin is a multiple of it.
constexpr int64_t kAlign = 16;
constexpr int kGroup = 256;   // lpips.slang's WG
// torchmetrics' ScalingLayer.
constexpr float kShift[3] = {-0.030f, -0.088f, -0.188f};
constexpr float kScale[3] = {0.458f, 0.448f, 0.450f};
// nn::conv2d's column workspace (kMaxColBytes) and one allocation's alignment.
constexpr uint64_t kColBytes = (32ull << 20) + 256;

int64_t conv_out(int64_t n, const Conv& c) { return (n + 2 * c.pad - c.k) / c.stride + 1; }

uint64_t bytes_of(int64_t floats) { return ((uint64_t)floats * 4 + 255) & ~255ull; }
int64_t groups_of(int64_t n) { return (n + kGroup - 1) / kGroup; }

// One axis of a net's activations: each stage's input (after its pool) and tap.
struct Axis {
    std::vector<int64_t> in, tap;
    bool ok = true;
};

Axis axis(const std::vector<Stage>& st, int64_t n) {
    Axis a;
    int64_t cur = n;
    for (const Stage& s : st) {
        if (s.pool) {
            a.ok = a.ok && cur >= s.pool;
            cur = a.ok ? (cur - s.pool) / kPoolStride + 1 : 1;
        }
        a.in.push_back(cur);
        for (const Conv& c : s.convs) {
            a.ok = a.ok && cur + 2 * c.pad >= c.k;
            cur = a.ok ? conv_out(cur, c) : 1;
        }
        a.tap.push_back(cur);
    }
    return a;
}

std::vector<int64_t> tap_strides(const std::vector<Stage>& st) {
    std::vector<int64_t> out;
    int64_t stride = 1;
    for (const Stage& s : st) {
        if (s.pool) stride *= kPoolStride;
        for (const Conv& c : s.convs) stride *= c.stride;
        out.push_back(stride);
    }
    return out;
}

// How far past its own stride-aligned pixels a tap position reads, worst tap,
// rounded up to kAlign: the halo a tile needs.
int64_t halo(const std::vector<Stage>& st) {
    struct Layer { int k, s, p; };
    std::vector<Layer> layers;
    int64_t need = 0, stride = 1;
    for (const Stage& s : st) {
        if (s.pool) layers.push_back({s.pool, kPoolStride, 0});
        for (const Conv& c : s.convs) layers.push_back({c.k, c.stride, c.pad});
        stride = 1;
        for (const Layer& l : layers) stride *= l.s;
        // Input pixels [lo, hi] reach tap position 0.
        int64_t lo = 0, hi = 0;
        for (auto it = layers.rbegin(); it != layers.rend(); ++it) {
            lo = lo * it->s - it->p;
            hi = hi * it->s - it->p + it->k - 1;
        }
        need = std::max({need, -lo, hi - stride + 1});
    }
    return (need + kAlign - 1) / kAlign * kAlign;
}

// A tile's share of one axis, in input pixels: it owns [own0, own1) and reads
// [in0, in1).
struct Span {
    int64_t own0, own1, in0, in1;
};

std::vector<Span> spans(int64_t n, int64_t tile, int64_t halo_px) {
    std::vector<Span> out;
    for (int64_t a = 0; a < n; a += tile) {
        const int64_t b = std::min(n, a + tile);
        out.push_back({a, b, std::max<int64_t>(0, a - halo_px), std::min(n, b + halo_px)});
    }
    return out;
}

// The tap position a tile boundary at input pixel `x` falls on, for an axis of
// `n` pixels whose tap is `len` long. Neighbours share the boundary, so the
// tiles' windows partition the tap.
int64_t tap_bound(int64_t x, int64_t n, int64_t stride, int64_t len) {
    return x >= n ? len : std::min(x / stride, len);
}

// Must match DistanceParams in shaders/lpips.slang.
struct DistanceParams {
    uint64_t partial, ref, pred, weight;
    uint32_t w, channels, x0, y0, ow, oh, groups_per_row;
};

uint64_t default_workspace() {
    const auto b = vk::Context::get().memoryBudget();
    if (!b.available_bytes) return 1ull << 30;
    return std::clamp<uint64_t>(b.available_bytes / 2, 256ull << 20, 3ull << 30);
}

}  // namespace

const char* net_name(Net net) { return net == Net::Vgg ? "vgg" : "alex"; }

struct Scorer::Impl {
    Net net = Net::Alex;
    bool loaded = false;
    nn::WeightStore weights;
    std::vector<int64_t> channels;   // per tap
    vk::Arena arena{"lpips"};

    // The window of one tap a tile owns, in the tile's tap coordinates, and
    // where its partial sums start.
    struct Window {
        int64_t x0, y0, w, h;
        int64_t at;
    };

    // What run_tile() allocates, allocation for allocation: each stage's
    // outputs stack on its inputs, which shrink geometrically, so the peak is
    // the first stage's working set.
    uint64_t tile_bytes(int64_t th, int64_t tw, int images) const {
        const std::vector<Stage>& st = stages(net);
        const Axis ay = axis(st, th), ax = axis(st, tw);
        int64_t groups = 0;
        for (size_t t = 0; t < st.size(); ++t)
            groups += (images - 1) * groups_of(ay.tap[t] * ax.tap[t]);
        uint64_t base = bytes_of(std::max<int64_t>(groups, 1)) + images * bytes_of(th * tw * 3);
        uint64_t peak = base;
        for (size_t s = 0; s < st.size(); ++s) {
            const int64_t tap = ay.tap[s] * ax.tap[s] * channels[s];
            const bool last = s + 1 == st.size();
            const bool pool_next = !last && st[s + 1].pool;
            if (!last)
                base += images * bytes_of(pool_next ? ay.in[s + 1] * ax.in[s + 1] * channels[s] : tap);
            const uint64_t own = pool_next || last ? bytes_of(tap) : 0;
            uint64_t inner = kColBytes;
            int64_t h = ay.in[s], w = ax.in[s];
            for (size_t j = 0; j + 1 < st[s].convs.size(); ++j) {
                const Conv& c = st[s].convs[j];
                h = conv_out(h, c);
                w = conv_out(w, c);
                inner += bytes_of(h * w * weights.getf("features.%d.weight", c.feature).shape[0]);
            }
            peak = std::max(peak, base + own + (images > 1 ? own : 0) + inner);
        }
        return peak + (1ull << 20);
    }

    void forward(const Stage& stage, const Tensor& in, const Tensor& out) {
        vk::ArenaScope scope(arena);
        Tensor cur = in;
        for (size_t j = 0; j < stage.convs.size(); ++j) {
            const Conv& c = stage.convs[j];
            const Tensor w = weights.getf("features.%d.weight", c.feature);
            const Tensor o = j + 1 < stage.convs.size()
                                 ? nn::arena_tensor(arena, DType::F32, conv_out(cur.shape[0], c),
                                                    conv_out(cur.shape[1], c), w.shape[0])
                                 : out;
            nn::ConvOpts opts;
            opts.stride_y = opts.stride_x = c.stride;
            opts.pad_y = opts.pad_x = c.pad;
            opts.act = nn::Act::Relu;
            opts.bias = weights.getf("features.%d.bias", c.feature);
            nn::conv2d(arena, o, cur, w, c.k, c.k, opts);
            cur = o;
        }
    }

    void distance(const Tensor& ref, const Tensor& pred, size_t tap, const Window& win,
                  const Tensor& partial, int64_t at) {
        if (!win.w || !win.h) return;
        DistanceParams p{};
        p.partial = partial.ptr + (uint64_t)at * 4;
        p.ref = ref.ptr;
        p.pred = pred.ptr;
        p.weight = weights.getf("lin%d", (int)tap).ptr;
        p.w = (uint32_t)ref.shape[1];
        p.channels = (uint32_t)ref.shape[2];
        p.x0 = (uint32_t)win.x0;
        p.y0 = (uint32_t)win.y0;
        p.ow = (uint32_t)win.w;
        p.oh = (uint32_t)win.h;
        const int64_t n = win.w * win.h;
        vk::Stream::get().dispatchFlat("lpips.distance", {}, n, kGroup, &p, sizeof(p),
                                       &p.groups_per_row, (double)n * (double)p.channels * 8);
    }

    // Adds each pred's per-tap sums over the window this tile owns.
    void run_tile(const std::vector<const float*>& src, int64_t W, int64_t H, const Span& sy,
                  const Span& sx, const Axis& fy, const Axis& fx, bool normalize,
                  std::vector<std::vector<double>>& sums) {
        const std::vector<Stage>& st = stages(net);
        const std::vector<int64_t> strides = tap_strides(st);
        const int images = (int)src.size();
        const int64_t th = sy.in1 - sy.in0, tw = sx.in1 - sx.in0;
        const Axis ay = axis(st, th), ax = axis(st, tw);
        vk::ArenaScope tile(arena);

        std::vector<Window> win(st.size());
        int64_t groups = 0;
        for (size_t t = 0; t < st.size(); ++t) {
            const int64_t s = strides[t];
            const int64_t y0 = tap_bound(sy.own0, H, s, fy.tap[t]);
            const int64_t y1 = tap_bound(sy.own1, H, s, fy.tap[t]);
            const int64_t x0 = tap_bound(sx.own0, W, s, fx.tap[t]);
            const int64_t x1 = tap_bound(sx.own1, W, s, fx.tap[t]);
            Window& w = win[t];
            w = {x0 - sx.in0 / s, y0 - sy.in0 / s, x1 - x0, y1 - y0, groups};
            NN_CHECK(w.x0 >= 0 && w.y0 >= 0 && w.x0 + w.w <= ax.tap[t] && w.y0 + w.h <= ay.tap[t],
                     "LPIPS tile at %lld,%lld: tap %d's window leaves the tile", (long long)sx.in0,
                     (long long)sy.in0, (int)t);
            groups += (images - 1) * groups_of(w.w * w.h);
        }
        const Tensor partial = nn::arena_tensor(arena, DType::F32, std::max<int64_t>(groups, 1));

        std::vector<Tensor> x(images);
        std::vector<float> host((size_t)(th * tw * 3));
        for (int k = 0; k < images; ++k) {
            x[k] = nn::arena_tensor(arena, DType::F32, th, tw, 3);
            nn::parallel_for(th, [&](int64_t y0, int64_t y1) {
                for (int64_t y = y0; y < y1; ++y) {
                    const float* row = src[k] + ((sy.in0 + y) * W + sx.in0) * 3;
                    float* dst = host.data() + y * tw * 3;
                    for (int64_t i = 0; i < tw * 3; ++i) {
                        const int c = (int)(i % 3);
                        const float v = normalize ? 2.0f * row[i] - 1.0f : row[i];
                        dst[i] = (v - kShift[c]) / kScale[c];
                    }
                }
            });
            vk::Stream::get().upload(x[k].ptr, host.data(), host.size() * sizeof(float));
        }

        for (size_t s = 0; s < st.size(); ++s) {
            const bool last = s + 1 == st.size();
            const int pool_next = last ? 0 : st[s + 1].pool;
            const int64_t C = channels[s], gh = ay.tap[s], gw = ax.tap[s];
            std::vector<Tensor> next(images);
            if (!last)
                for (Tensor& t : next)
                    t = pool_next ? nn::arena_tensor(arena, DType::F32, ay.in[s + 1], ax.in[s + 1], C)
                                  : nn::arena_tensor(arena, DType::F32, gh, gw, C);
            // Without a pool after it the tap IS the next stage's input.
            const bool own = pool_next || last;
            vk::ArenaScope work(arena);
            const Tensor ref = own ? nn::arena_tensor(arena, DType::F32, gh, gw, C) : next[0];
            forward(st[s], x[0], ref);
            if (pool_next) nn::maxpool(next[0], ref, pool_next, kPoolStride);
            for (int k = 1; k < images; ++k) {
                vk::ArenaScope one(arena);
                const Tensor tap = own ? nn::arena_tensor(arena, DType::F32, gh, gw, C) : next[k];
                forward(st[s], x[k], tap);
                distance(ref, tap, s, win[s], partial,
                         win[s].at + (k - 1) * groups_of(win[s].w * win[s].h));
                if (pool_next) nn::maxpool(next[k], tap, pool_next, kPoolStride);
            }
            x = next;
        }

        std::vector<float> host_partial((size_t)groups);
        if (groups)
            vk::Stream::get().download(host_partial.data(), partial.ptr,
                                       host_partial.size() * sizeof(float));
        for (size_t t = 0; t < st.size(); ++t) {
            const int64_t n = groups_of(win[t].w * win[t].h);
            for (int k = 1; k < images; ++k) {
                double sum = 0.0;
                const float* p = host_partial.data() + win[t].at + (k - 1) * n;
                for (int64_t i = 0; i < n; ++i) sum += p[i];
                sums[k - 1][t] += sum;
            }
        }
    }
};

Scorer::Scorer() : impl_(new Impl) {}

Scorer::~Scorer() {
    try {
        unload();
    } catch (const std::exception&) {}
    delete impl_;
}

bool Scorer::loaded() const { return impl_->loaded; }

void Scorer::unload() {
    Impl& m = *impl_;
    if (!m.loaded) return;
    m.loaded = false;
    m.channels.clear();
    m.weights.release();
    m.arena.release();
}

void Scorer::load(Net net, const Checkpoints& files) {
    NN_ENSURE_EMBEDDED_MODULES(lpips);
    unload();
    Impl& m = *impl_;
    const char* arch = net == Net::Vgg ? "VGG16" : "AlexNet";
    nn::TorchCheckpoint backbone(files.backbone), linear(files.linear);
    nn::TorchCheckpoint::Reader rb(backbone), rl(linear);
    int64_t cin = 3;
    char name[64];
    for (size_t s = 0; s < stages(net).size(); ++s) {
        for (const Conv& c : stages(net)[s].convs) {
            std::snprintf(name, sizeof name, "features.%d.weight", c.feature);
            NN_CHECK(backbone.has(name), "%s has no %s: not torchvision's %s",
                     files.backbone.c_str(), name, arch);
            nn::OnnxTensor w = rb.read(name);
            NN_CHECK(w.shape.size() == 4 && w.shape[1] == cin && w.shape[2] == c.k &&
                         w.shape[3] == c.k,
                     "%s: %s is not the %dx%d conv over %lld channels %s has there",
                     files.backbone.c_str(), name, c.k, c.k, (long long)cin, arch);
            const int64_t cout = w.shape[0];
            m.weights.stage(name, w.shape, std::move(w.data), false);
            std::snprintf(name, sizeof name, "features.%d.bias", c.feature);
            NN_CHECK(backbone.has(name), "%s has no %s", files.backbone.c_str(), name);
            nn::OnnxTensor b = rb.read(name);
            NN_CHECK(b.data.size() == (size_t)cout, "%s: %s has %zu values for %lld channels",
                     files.backbone.c_str(), name, b.data.size(), (long long)cout);
            m.weights.stage(name, {cout}, std::move(b.data), false);
            cin = cout;
        }
        m.channels.push_back(cin);
        std::snprintf(name, sizeof name, "lin%d.model.1.weight", (int)s);
        NN_CHECK(linear.has(name), "%s has no %s: not an LPIPS linear-layer checkpoint",
                 files.linear.c_str(), name);
        nn::OnnxTensor l = rl.read(name);
        NN_CHECK(l.data.size() == (size_t)cin,
                 "%s: %s weighs %zu channels, but %s's tap %d has %lld -- the %s file?",
                 files.linear.c_str(), name, l.data.size(), arch, (int)s, (long long)cin,
                 net == Net::Vgg ? "alex" : "vgg");
        std::snprintf(name, sizeof name, "lin%d", (int)s);
        m.weights.stage(name, {cin}, std::move(l.data), false);
    }
    m.weights.upload("lpips");
    m.net = net;
    m.loaded = true;
}

std::vector<double> Scorer::score(const float* ref, const std::vector<const float*>& preds,
                                  int width, int height, bool normalize, const ScoreOptions& opts) {
    NN_ENSURE_EMBEDDED_MODULES(lpips);
    Impl& m = *impl_;
    NN_CHECK(m.loaded, "lpips::Scorer::score before load()");
    NN_CHECK(ref && width > 0 && height > 0, "lpips::Scorer::score: empty image");
    if (preds.empty()) return {};
    const std::vector<Stage>& st = stages(m.net);
    const Axis fy = axis(st, height), fx = axis(st, width);
    NN_CHECK(fy.ok && fx.ok, "a %dx%d view is too small for LPIPS %s", width, height,
             net_name(m.net));

    std::vector<const float*> src{ref};
    src.insert(src.end(), preds.begin(), preds.end());
    const int images = (int)src.size();
    const int64_t H = height, W = width, halo_px = halo(st);

    int64_t tile = std::max(H, W);
    if (opts.tile > 0) {
        tile = (opts.tile + kAlign - 1) / kAlign * kAlign;
    } else {
        const uint64_t budget = opts.workspace_bytes ? opts.workspace_bytes : default_workspace();
        if (m.tile_bytes(H, W, images) > budget) {
            tile = (std::max(H, W) - 1) / kAlign * kAlign;
            while (tile >= kAlign &&
                   m.tile_bytes(std::min(H, tile + 2 * halo_px), std::min(W, tile + 2 * halo_px),
                                images) > budget)
                tile -= kAlign;
            NN_CHECK(tile >= kAlign,
                     "LPIPS %s needs more than %llu MiB of device memory even in %lld-pixel tiles",
                     net_name(m.net), (unsigned long long)(budget >> 20), (long long)kAlign);
        }
    }
    const std::vector<Span> ys = spans(H, tile, halo_px), xs = spans(W, tile, halo_px);
    int64_t th = 0, tw = 0;
    for (const Span& s : ys) th = std::max(th, s.in1 - s.in0);
    for (const Span& s : xs) tw = std::max(tw, s.in1 - s.in0);
    m.arena.reserve(m.tile_bytes(th, tw, images));

    std::vector<std::vector<double>> sums(preds.size(), std::vector<double>(st.size(), 0.0));
    for (const Span& sy : ys)
        for (const Span& sx : xs) m.run_tile(src, W, H, sy, sx, fy, fx, normalize, sums);

    std::vector<double> out(preds.size(), 0.0);
    for (size_t k = 0; k < preds.size(); ++k)
        for (size_t t = 0; t < st.size(); ++t)
            out[k] += sums[k][t] / (double)(fy.tap[t] * fx.tap[t]);
    return out;
}

}  // namespace lpips
