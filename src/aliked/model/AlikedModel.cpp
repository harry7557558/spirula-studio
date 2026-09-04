// ALIKED's forward pass, next to its PyTorch reference.
//
// Reading order matches the network: encoder, feature aggregation, score head,
// DKD (detect), SDDH (describe). There is no computation graph -- each op runs
// when called and writes into an arena tensor -- so this file is meant to be
// read against nets/aliked.py and nets/blocks.py line by line.
//
// The three conventions worth knowing before reading anything else, because
// each is a place where a plausible guess is wrong:
//
//   * The encoder runs at FULL resolution and aggregates 128 channels there.
//     That is what makes the working resolution (1600 px, COLMAP's default for
//     ALIKED against 3200 for SIFT) a memory decision and not a speed one.
//   * The deformable convolutions' offsets are (dy, dx) interleaved per tap --
//     torchvision's layout. The descriptor head's offsets are the same
//     network's, and are grouped [x0..xM-1, y0..yM-1] instead. Two conventions
//     in one model; see sddh_positions in shaders/aliked.slang.
//   * Upsampling is align_corners=True, and so is every normalized coordinate
//     downstream of it.

#include "aliked/Aliked.h"

#include "aliked/Common.h"
#include "aliked/model/Fetch.h"
#include "aliked/model/Weights.h"
#include "nn/Ops.h"
#include "nn/Tensor.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include "nn/vk/EmbeddedSpirv.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <vector>

// This library's SPIR-V blobs. Registered by an explicit call from load(),
// not by a static initializer: ss_aliked is a static archive, and an
// object nothing references is not linked -- the kernels would come back "no
// shader module". See nn/vk/EmbeddedSpirv.h.
NN_DECLARE_EMBEDDED_MODULES(aliked)

namespace aliked {
namespace {

using nn::Act;
using nn::ConvOpts;
using nn::DType;
using nn::LinearOpts;
using nn::Tensor;

constexpr int kPadDivisor = 32;   // the encoder's total downsampling factor

int64_t round_up(int64_t v, int64_t m) { return (v + m - 1) / m * m; }

// The arena is a bump allocator that refuses to grow while anything is live,
// so the whole forward pass has to be sized before it starts.
//
// This is also the honest statement of what ALIKED costs: the aggregated map
// is `dim` channels at FULL resolution, and at COLMAP's default working size
// for this extractor (1600 px) that one tensor is ~1 GB. Everything else is
// small next to it. Halving it by keeping the aggregate in f16 is the obvious
// next move and is not done yet -- the row normalization would need an f16
// output path, which is a second numeric path under a kernel that currently
// has one.
int64_t plan_arena_bytes(int64_t Hp, int64_t Wp, const AlikedHparams& hp) {
    const int64_t P = Hp * Wp;
    auto at = [&](int64_t div, int64_t ch) { return P / (div * div) * ch; };

    // What stays live from the start of the encoder to the end of the
    // descriptor head.
    int64_t live = P * 3                    // the padded input
                   + P * hp.c1              // x1
                   + at(2, hp.c1)           // p2
                   + at(2, hp.c2)           // x2
                   + at(8, hp.c2)           // p3
                   + at(8, hp.c3)           // x3
                   + at(32, hp.c3)          // p4
                   + at(32, hp.c4)          // x4
                   + P * hp.dim             // the aggregate
                   + P;                     // the score map

    // The largest scoped transient on top of that. Aggregation is the peak: a
    // full-resolution projection plus its upsampled copy. The encoder blocks
    // (2 x their own output) and the score head (16 channels) are both below
    // it, and the NMS masks (2 ints per pixel) are too.
    const int64_t transient = 2 * P * hp.dim4;

    // The convolution column workspace (nn::conv2d chunks to ~32 MiB) plus
    // slack for the detector's lists and the descriptor head's small matrices,
    // whose size depends on the keypoint count rather than the image.
    return (live + transient) * 4 + (96ll << 20);
}

}  // namespace

struct Extractor::Impl {
    AlikedWeights weights;
    vk::Arena     arena{"aliked"};

    // ---- encoder pieces -------------------------------------------------

    // gate(bn2(conv2(gate(bn1(conv1(x)))))). BatchNorm was folded into each
    // conv's weight and bias at load, so both convs carry a bias here and
    // neither has one in the checkpoint.
    //
    // Every block writes into a caller-allocated `out` and scopes its own
    // temporaries. That is not a style choice: the arena is a bump allocator,
    // so a temporary returned past its block would stay live for the rest of
    // the forward pass -- at full resolution that is 64 MB per block that
    // nothing reads again.
    void convBlock(const Tensor& out, const Tensor& x, const char* prefix,
                   int64_t cout) {
        const int64_t H = x.shape[0], W = x.shape[1];
        vk::ArenaScope scope(arena);
        Tensor h = nn::arena_tensor(arena, DType::F32, H, W, cout);
        conv3x3(h, x, weights.getf("%s.conv1.weight", prefix),
                weights.getf("%s.conv1.bias", prefix), Act::Selu);
        conv3x3(out, h, weights.getf("%s.conv2.weight", prefix),
                weights.getf("%s.conv2.bias", prefix), Act::Selu);
    }

    // gate(bn2(conv2(gate(bn1(conv1(x))))) + downsample(x)).
    //
    // The second gate is OUTSIDE the residual add. That is torchvision's
    // BasicBlock shape, and it is not what a reading of the module source
    // suggests -- the exported graph settles it: /block2/gate_1/Selu takes
    // /block2/Add_output_0 as its input. Gating before the add instead cost a
    // 6x drop in keypoint agreement with the reference and left the
    // descriptors uncorrelated, while still looking like a working detector.
    void resBlock(const Tensor& out, const Tensor& x, const char* prefix, int64_t cout,
                  bool deformable) {
        const int64_t H = x.shape[0], W = x.shape[1];
        const float max_offset = 0.25f * (float)std::max(H, W);
        vk::ArenaScope scope(arena);

        Tensor h = nn::arena_tensor(arena, DType::F32, H, W, cout);
        if (deformable)
            deform3x3(h, x, prefix, 1, max_offset, Act::Selu);
        else
            conv3x3(h, x, weights.getf("%s.conv1.weight", prefix),
                    weights.getf("%s.conv1.bias", prefix), Act::Selu);

        if (deformable)
            deform3x3(out, h, prefix, 2, max_offset, Act::None);
        else
            conv3x3(out, h, weights.getf("%s.conv2.weight", prefix),
                    weights.getf("%s.conv2.bias", prefix), Act::None);

        // The 1x1 shortcut projection, which every ResBlock here has (they all
        // change width).
        Tensor id = nn::arena_tensor(arena, DType::F32, H, W, cout);
        ConvOpts o;
        o.bias = weights.getf("%s.downsample.bias", prefix);
        nn::conv2d(arena, id, x, weights.getf("%s.downsample.weight", prefix), 1, 1, o);

        nn::add(out.view(H * W, cout), out.view(H * W, cout), id.view(H * W, cout), 1.0f,
                1.0f, Act::Selu);
    }

    void conv3x3(const Tensor& out, const Tensor& in, const Tensor& w, const Tensor& bias,
                 Act act) {
        ConvOpts o;
        o.pad_y = o.pad_x = 1;
        o.bias = bias;
        o.act = act;
        nn::conv2d(arena, out, in, w, 3, 3, o);
    }

    // offset = offset_conv(x).clamp(+-max(h,w)/4); deform_conv2d(x, offset, w).
    void deform3x3(const Tensor& out, const Tensor& in, const char* prefix, int idx,
                   float max_offset, Act act) {
        const int64_t H = in.shape[0], W = in.shape[1];
        vk::ArenaScope scope(arena);
        Tensor offset = nn::arena_tensor(arena, DType::F32, H, W, 2 * 3 * 3);
        ConvOpts oo;
        oo.pad_y = oo.pad_x = 1;
        oo.bias = weights.getf("%s.conv%d.offset_conv.bias", prefix, idx);
        nn::conv2d(arena, offset, in,
                   weights.getf("%s.conv%d.offset_conv.weight", prefix, idx), 3, 3, oo);

        ConvOpts co;
        co.pad_y = co.pad_x = 1;
        co.bias = weights.getf("%s.conv%d.regular_conv.bias", prefix, idx);
        co.act = act;
        nn::deform_conv2d(arena, out, in, offset,
                          weights.getf("%s.conv%d.regular_conv.weight", prefix, idx), 3, 3,
                          max_offset, co);
    }
};

// ---------------------------------------------------------------------------

Extractor::Extractor() : impl_(new Impl) {}
Extractor::~Extractor() { delete impl_; }
bool Extractor::loaded() const { return impl_->weights.loaded(); }
int  Extractor::descriptorDim() const { return impl_->weights.hparams().desc_dim; }

void Extractor::load(const std::string& model) {
    NN_ENSURE_EMBEDDED_MODULES(aliked);
    impl_->weights.load(resolve_model(model));
}

Features Extractor::extract(const uint8_t* rgb, int width, int height,
                            const ExtractOptions& opts) {
    NN_CHECK(impl_->weights.loaded(), "Extractor::extract before load()");
    NN_CHECK(rgb != nullptr && width > 0 && height > 0, "Extractor::extract: empty image");
    Impl& im = *impl_;
    const AlikedHparams& hp = im.weights.hparams();
    vk::Arena& arena = im.arena;

    Features out;
    out.width = width;
    out.height = height;
    out.desc_dim = hp.desc_dim;

    const int64_t Wp = round_up(width, kPadDivisor);
    const int64_t Hp = round_up(height, kPadDivisor);
    NN_CHECK(Hp / 32 >= 1 && Wp / 32 >= 1, "image too small for ALIKED");

    // The descriptor head gathers the full-resolution aggregate at arbitrary
    // keypoint positions, so that one tensor has to be reachable from its base:
    // 2880 px square at dim 128 without the 64-bit kernels, 5792 with them.
    const int64_t agg_bytes = Hp * Wp * hp.dim * 4;
    const int64_t reach = nn::span_reach(nn::DType::F32);
    if (agg_bytes > reach) {
        const int fit = (int)((double)std::max(width, height) *
                              std::sqrt((double)reach / (double)agg_bytes)) /
                        kPadDivisor * kPadDivisor;
        nn::fail("ALIKED cannot run at %lldx%lld: its %d-channel aggregate spans %lld "
                 "bytes, past the %lld a kernel can index from one pointer here. Cap "
                 "the working resolution at %d px (--max-image-size).",
                 (long long)Wp, (long long)Hp, hp.dim, (long long)agg_bytes,
                 (long long)reach, fit);
    }

    arena.reserve((uint64_t)plan_arena_bytes(Hp, Wp, hp));
    vk::ArenaScope root(arena);

    // ---- input: /255, padded to a multiple of 32 by edge replication ----
    //
    // COLMAP's InputPadder, done on the host for the same reason it does it
    // there: it is one pass over the image next to a backbone that is hundreds,
    // and it keeps the exact replicate-right-and-bottom rule visible.
    Tensor img = nn::arena_tensor(arena, DType::F32, Hp, Wp, 3);
    {
        std::vector<float> host((size_t)Hp * Wp * 3);
        for (int64_t y = 0; y < Hp; ++y) {
            const int64_t sy = std::min<int64_t>(y, height - 1);
            for (int64_t x = 0; x < Wp; ++x) {
                const int64_t sx = std::min<int64_t>(x, width - 1);
                const uint8_t* src = rgb + ((size_t)sy * width + sx) * 3;
                float* dst = host.data() + ((size_t)y * Wp + x) * 3;
                dst[0] = src[0] * (1.0f / 255.0f);
                dst[1] = src[1] * (1.0f / 255.0f);
                dst[2] = src[2] * (1.0f / 255.0f);
            }
        }
        nn::tensor_from_host(img, host.data(), (int64_t)host.size());
    }

    // ---- encoder ----
    // x1 full res, x2 at 1/2, x3 at 1/8, x4 at 1/32 -- which is where the
    // pad-to-32 requirement comes from.
    Tensor x1 = nn::arena_tensor(arena, DType::F32, Hp, Wp, hp.c1);
    im.convBlock(x1, img, "block1", hp.c1);

    Tensor p2 = nn::arena_tensor(arena, DType::F32, Hp / 2, Wp / 2, hp.c1);
    nn::avgpool(p2, x1, 2);
    Tensor x2 = nn::arena_tensor(arena, DType::F32, Hp / 2, Wp / 2, hp.c2);
    im.resBlock(x2, p2, "block2", hp.c2, /*deformable=*/false);

    Tensor p3 = nn::arena_tensor(arena, DType::F32, Hp / 8, Wp / 8, hp.c2);
    nn::avgpool(p3, x2, 4);
    Tensor x3 = nn::arena_tensor(arena, DType::F32, Hp / 8, Wp / 8, hp.c3);
    im.resBlock(x3, p3, "block3", hp.c3, /*deformable=*/true);

    Tensor p4 = nn::arena_tensor(arena, DType::F32, Hp / 32, Wp / 32, hp.c3);
    nn::avgpool(p4, x3, 4);
    Tensor x4 = nn::arena_tensor(arena, DType::F32, Hp / 32, Wp / 32, hp.c4);
    im.resBlock(x4, p4, "block4", hp.c4, /*deformable=*/true);

    // ---- aggregation: gate(conv_i(x_i)) upsampled to full res, concatenated ----
    Tensor x1234 = nn::arena_tensor(arena, DType::F32, Hp, Wp, hp.dim);
    {
        const Tensor* blocks[4] = {&x1, &x2, &x3, &x4};
        for (int i = 0; i < 4; ++i) {
            vk::ArenaScope scope(arena);
            const Tensor& src = *blocks[i];
            const int64_t h = src.shape[0], w = src.shape[1];
            Tensor proj = nn::arena_tensor(arena, DType::F32, h, w, hp.dim4);
            ConvOpts o;
            o.act = Act::Selu;   // the 4 projections' gates -- see the Selu count
            nn::conv2d(arena, proj, src, im.weights.getf("conv%d.weight", i + 1), 1, 1, o);

            const Tensor* full = &proj;
            Tensor up;
            if (h != Hp || w != Wp) {
                up = nn::arena_tensor(arena, DType::F32, Hp, Wp, hp.dim4);
                nn::resize_bilinear(up, proj, /*align_corners=*/true);
                full = &up;
            }
            // torch.cat along channels, as a strided write into the slot this
            // level owns.
            nn::strided_copy(x1234.offsetElems(i * hp.dim4), full->view(Hp * Wp, hp.dim4),
                             Hp * Wp, hp.dim4, hp.dim4, hp.dim);
        }
    }

    // ---- score head: sigmoid(conv(...)) over the UNnormalized aggregate ----
    // The explicit ndim=3 matters: a trailing 1 would otherwise be inferred
    // away and the single-channel map would arrive at conv2d as [H, W].
    Tensor score = nn::arena_tensor(arena, DType::F32, Hp, Wp, 1, 1, /*ndim=*/3);
    {
        vk::ArenaScope scope(arena);
        Tensor s0 = nn::arena_tensor(arena, DType::F32, Hp, Wp, 8);
        ConvOpts o;
        o.act = Act::Selu;
        nn::conv2d(arena, s0, x1234, im.weights.get("score_head.0.weight"), 1, 1, o);

        Tensor s1 = nn::arena_tensor(arena, DType::F32, Hp, Wp, 4);
        im.conv3x3(s1, s0, im.weights.get("score_head.2.weight"), {}, Act::Selu);
        Tensor s2 = nn::arena_tensor(arena, DType::F32, Hp, Wp, 4);
        im.conv3x3(s2, s1, im.weights.get("score_head.4.weight"), {}, Act::Selu);
        im.conv3x3(score, s2, im.weights.get("score_head.6.weight"), {}, Act::Sigmoid);
    }

    // The descriptor head reads the CHANNEL-normalized aggregate; the score
    // head above read it raw. In place, so the 128-channel full-resolution map
    // exists once.
    nn::l2_normalize_rows(x1234.view(Hp * Wp, hp.dim), x1234.view(Hp * Wp, hp.dim));

    // ---- DKD: NMS, top-K, sub-pixel refinement ----
    struct NmsParams {
        uint64_t mask, supp, scores;
        uint32_t H, W;
        int32_t  radius;
        uint32_t groups_per_row;
    };
    struct CollectParams {
        uint64_t out, counter, mask, scores;
        uint32_t H, W;
        int32_t  border;
        float    min_score;
        uint32_t cap, groups_per_row;
    };

    std::vector<uint32_t> cand;
    uint32_t n_cand = 0;
    {
        vk::ArenaScope scope(arena);
        const int64_t px = Hp * Wp;
        Tensor mask = nn::arena_tensor(arena, DType::I32, px);
        Tensor supp = nn::arena_tensor(arena, DType::I32, px);
        Tensor list = nn::arena_tensor(arena, DType::I32, (int64_t)opts.max_candidates, 3);
        Tensor counter = nn::arena_tensor(arena, DType::I32, 4);
        vk::Stream::get().zero(counter.ptr, 16);

        NmsParams np{};
        np.mask = mask.ptr;
        np.supp = supp.ptr;
        np.scores = score.ptr;
        np.H = (uint32_t)Hp;
        np.W = (uint32_t)Wp;
        np.radius = opts.nms_radius;
        const vk::SpecList spec{0u, 0u};
        vk::Stream::get().dispatchFlat("aliked.nms_init", spec, px, 256, &np, sizeof(np),
                                       &np.groups_per_row);
        for (int round = 0; round < 2; ++round) {
            vk::Stream::get().dispatchFlat("aliked.nms_suppress", spec, px, 256, &np,
                                           sizeof(np), &np.groups_per_row);
            vk::Stream::get().dispatchFlat("aliked.nms_recover", spec, px, 256, &np,
                                           sizeof(np), &np.groups_per_row);
        }

        CollectParams cp{};
        cp.out = list.ptr;
        cp.counter = counter.ptr;
        cp.mask = mask.ptr;
        cp.scores = score.ptr;
        cp.H = (uint32_t)Hp;
        cp.W = (uint32_t)Wp;
        cp.border = opts.nms_radius;
        cp.min_score = opts.min_score;
        cp.cap = opts.max_candidates;
        vk::Stream::get().dispatchFlat("aliked.nms_collect", spec, px, 256, &cp,
                                       sizeof(cp), &cp.groups_per_row);

        vk::Stream::get().download(&n_cand, counter.ptr, 4);
        if (n_cand > opts.max_candidates) {
            NN_LOG_WARN("[aliked] candidate list saturated (%u > %u); raise "
                        "max_candidates\n",
                        n_cand, opts.max_candidates);
            n_cand = opts.max_candidates;
        }
        if (n_cand) {
            cand.resize((size_t)n_cand * 3);
            vk::Stream::get().download(cand.data(), list.ptr, (uint64_t)n_cand * 12);
        }
    }
    if (n_cand == 0) return out;

    // Top-K by the peak's own score, which is what DKD's top_k mode ranks by.
    // The tie-break is by position, for the same reason GPU SIFT's is (D16):
    // the GPU appended these through an atomic, so their order varies run to
    // run, and an unstable order here would make the whole reconstruction
    // irreproducible.
    std::vector<uint32_t> order((size_t)n_cand);
    std::iota(order.begin(), order.end(), 0u);
    auto score_of = [&](uint32_t i) {
        float s;
        std::memcpy(&s, &cand[(size_t)i * 3 + 2], 4);
        return s;
    };
    const uint32_t keep =
        std::min<uint32_t>(n_cand, opts.max_num_features > 0
                                       ? (uint32_t)opts.max_num_features
                                       : n_cand);
    auto better = [&](uint32_t a, uint32_t b) {
        const float sa = score_of(a), sb = score_of(b);
        if (sa != sb) return sa > sb;
        if (cand[(size_t)a * 3 + 1] != cand[(size_t)b * 3 + 1])
            return cand[(size_t)a * 3 + 1] < cand[(size_t)b * 3 + 1];
        return cand[(size_t)a * 3] < cand[(size_t)b * 3];
    };
    std::partial_sort(order.begin(), order.begin() + keep, order.end(), better);
    order.resize(keep);

    std::vector<int32_t> centers((size_t)keep * 2);
    for (uint32_t i = 0; i < keep; ++i) {
        centers[(size_t)i * 2] = (int32_t)cand[(size_t)order[i] * 3];
        centers[(size_t)i * 2 + 1] = (int32_t)cand[(size_t)order[i] * 3 + 1];
    }

    struct RefineParams {
        uint64_t out_xy, out_score, centers, scores;
        uint32_t H, W, N;
        int32_t  radius;
        float    temperature;
        uint32_t groups_per_row;
    };
    struct CenterParams {
        uint64_t out, xy;
        uint32_t N, groups_per_row;
    };
    struct SddhPosParams {
        uint64_t out, xy, offsets;
        uint32_t N, M, W, H;
        float    max_offset;
        uint32_t groups_per_row;
    };

    const int64_t N = keep;
    const int64_t M = hp.M, K = hp.K, D = hp.desc_dim;
    std::vector<float> host_xy((size_t)N * 2), host_score((size_t)N);
    std::vector<float> host_desc((size_t)N * D);
    {
        vk::ArenaScope scope(arena);
        Tensor tc = nn::arena_tensor(arena, DType::I32, N, 2);
        vk::Stream::get().upload(tc.ptr, centers.data(), (uint64_t)N * 8);

        Tensor xy = nn::arena_tensor(arena, DType::F32, N, 2);
        Tensor kscore = nn::arena_tensor(arena, DType::F32, N);
        RefineParams rp{};
        rp.out_xy = xy.ptr;
        rp.out_score = kscore.ptr;
        rp.centers = tc.ptr;
        rp.scores = score.ptr;
        rp.H = (uint32_t)Hp;
        rp.W = (uint32_t)Wp;
        rp.N = (uint32_t)N;
        rp.radius = opts.nms_radius;
        rp.temperature = opts.temperature;
        const vk::SpecList spec{0u, 0u};
        vk::Stream::get().dispatchFlat("aliked.refine_keypoints", spec, N, 256, &rp,
                                       sizeof(rp), &rp.groups_per_row);

        // ---- SDDH ----
        // The patch centre is the truncation of the REFINED position, not the
        // peak the refinement started from; they differ by a pixel often
        // enough to matter.
        Tensor ic = nn::arena_tensor(arena, DType::I32, N, 2);
        CenterParams cp{};
        cp.out = ic.ptr;
        cp.xy = xy.ptr;
        cp.N = (uint32_t)N;
        vk::Stream::get().dispatchFlat("aliked.to_int_centers", spec, N * 2, 256, &cp,
                                       sizeof(cp), &cp.groups_per_row);

        Tensor patches = nn::arena_tensor(arena, DType::F32, N, hp.dim * K * K);
        nn::patch_gather(patches, x1234, ic, (int)K);

        Tensor off1 = nn::arena_tensor(arena, DType::F32, N, 2 * M);
        LinearOpts l1;
        l1.bias = im.weights.get("desc_head.offset_conv.0.bias");
        l1.act = Act::Selu;
        nn::linear(off1, patches, im.weights.get("desc_head.offset_conv.0.weight"), l1);

        Tensor off2 = nn::arena_tensor(arena, DType::F32, N, 2 * M);
        LinearOpts l2;
        l2.bias = im.weights.get("desc_head.offset_conv.2.bias");
        nn::linear(off2, off1, im.weights.get("desc_head.offset_conv.2.weight"), l2);

        Tensor pos = nn::arena_tensor(arena, DType::F32, N * M, 2);
        SddhPosParams sp{};
        sp.out = pos.ptr;
        sp.xy = xy.ptr;
        sp.offsets = off2.ptr;
        sp.N = (uint32_t)N;
        sp.M = (uint32_t)M;
        sp.W = (uint32_t)Wp;
        sp.H = (uint32_t)Hp;
        sp.max_offset = 0.25f * (float)std::max(Hp, Wp);
        vk::Stream::get().dispatchFlat("aliked.sddh_positions", spec, N * M, 256, &sp,
                                       sizeof(sp), &sp.groups_per_row);

        Tensor feat = nn::arena_tensor(arena, DType::F32, N * M, hp.dim);
        nn::grid_sample_points(feat, x1234, pos, /*align_corners=*/true);

        // sf_conv is a 1x1 conv over the channel axis at every (keypoint,
        // position), i.e. one GEMM over N*M rows. No bias in the checkpoint.
        //
        // A separate output, NOT in place: every output column re-reads the
        // whole input row, so a GEMM writing over its own input races with
        // itself. (Elementwise ops here do alias safely, and `residual` below
        // is explicitly allowed to -- one thread per element. A matmul is the
        // case where that reasoning does not carry.)
        Tensor sfeat = nn::arena_tensor(arena, DType::F32, N * M, hp.dim);
        LinearOpts lsf;
        lsf.act = Act::Selu;
        nn::linear(sfeat, feat, im.weights.get("desc_head.sf_conv.weight"), lsf);

        // einsum('ncp,pcd->nd') as M accumulating matmuls. Each reads the
        // p-th position's row out of the [N, M, C] buffer, which is what
        // x_row_stride is for -- no gather, no permute.
        Tensor descs = nn::arena_tensor(arena, DType::F32, N, D);
        const Tensor agg = im.weights.get("desc_head.agg_weights_t");
        for (int64_t p = 0; p < M; ++p) {
            LinearOpts la;
            la.x_row_stride = M * hp.dim;
            if (p > 0) la.residual = descs;
            nn::linear(descs, sfeat.offsetElems(p * hp.dim).view(N, hp.dim),
                       agg.slice0(p, 1).view(D, hp.dim), la);
        }
        nn::l2_normalize_rows(descs, descs);

        vk::Stream::get().download(host_xy.data(), xy.ptr, (uint64_t)N * 8);
        vk::Stream::get().download(host_score.data(), kscore.ptr, (uint64_t)N * 4);
        vk::Stream::get().download(host_desc.data(), descs.ptr, (uint64_t)N * D * 4);
    }

    // ---- to COLMAP's frame, dropping the padding and the weak ----
    //
    // ALIKED puts the top-left pixel's CENTRE at (0, 0); COLMAP (and this
    // repository) put its CORNER there, hence the +0.5. Keypoints found in the
    // replicated padding are outside the original bounds and go.
    out.keypoints.reserve((size_t)N);
    out.descriptors.reserve((size_t)N * D);
    for (int64_t i = 0; i < N; ++i) {
        if (host_score[(size_t)i] < opts.min_score) continue;
        const float px = host_xy[(size_t)i * 2] + 0.5f;
        const float py = host_xy[(size_t)i * 2 + 1] + 0.5f;
        if (px < 0.0f || px >= (float)width || py < 0.0f || py >= (float)height) continue;
        out.keypoints.push_back({px, py, host_score[(size_t)i]});
        out.descriptors.insert(out.descriptors.end(), host_desc.begin() + (size_t)i * D,
                               host_desc.begin() + (size_t)(i + 1) * D);
    }
    NN_LOG_DEBUG("[aliked] %dx%d (padded %lldx%lld): %u candidates -> %lld -> %zu\n",
                 width, height, (long long)Wp, (long long)Hp, n_cand, (long long)N,
                 out.keypoints.size());
    return out;
}

}  // namespace aliked
