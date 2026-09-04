// DaD's forward pass: a VGG-11 encoder, four ConvRefiners summing a
// single-channel logit map, then softmax over the WHOLE map, 3x3 NMS, top-K
// and a sub-pixel soft-argmax.
//
// Read against LoMa's dad.py and its sample_keypoints. The one
// thing not obvious from either: the density is a softmax over all H*W
// pixels, so a "score" is a share of ONE image's total and is not comparable
// across images -- which is why min_score defaults to 0.

#include "loma/model/Model.h"

#include "loma/Common.h"
#include "loma/model/Dump.h"
#include "loma/model/Fetch.h"
#include "nn/Ops.h"
#include "nn/vk/Memory.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cstring>
#include <numeric>
#include <vector>

namespace loma {
namespace {

using nn::DType;
using nn::Tensor;

struct PeakParams {
    uint64_t out, counter, probs;
    uint32_t H, W;
    float    min_score;
    uint32_t cap, groups_per_row;
};

}  // namespace

Detector::~Detector() {
    if (offsets_) vk::device_free(offsets_);
}

void Detector::load(const std::string& path, vk::Arena* arena) {
    pyr_.arena = arena;
    pyr_.prefix = "det";
    pyr_.w.load(resolve_model(path, "detector"), "det");
    NN_CHECK(pyr_.hp().head == 1,
             "'%s' emits %lld detector channels; DaD has one prototype",
             pyr_.w.path().c_str(), (long long)pyr_.hp().head);

    // The soft-argmax offsets, as a [2, 9] matrix so the weighted sum is one
    // nn::linear. A neighbour one cell away is exactly one pixel away, and
    // patch_gather's column order is ky*3 + kx.
    float w[18];
    for (int ky = 0; ky < 3; ++ky)
        for (int kx = 0; kx < 3; ++kx) {
            w[ky * 3 + kx] = (float)(kx - 1);
            w[9 + ky * 3 + kx] = (float)(ky - 1);
        }
    offsets_ = vk::device_alloc(sizeof w, "loma-dad-offsets");
    vk::Stream::get().upload(offsets_, w, sizeof w);
    vk::Stream::get().sync();
}

int64_t Detector::planBytes(int64_t H, int64_t W) const {
    const PyramidHparams& p = pyr_.hp();
    // The normalized image, the logits and the probabilities; the pyramid's
    // own live set and its largest scoped peak come from planPyramid, which
    // sits beside the code that allocates them.
    int64_t live = H * W * (3 + 2 * p.head);
    int64_t transient = 0;
    Pyramid::planPyramid(p, Pyramid::planLevels(p, H, W), live, transient);
    // The peak list, the convolution column workspace (nn::conv2d chunks to
    // ~32 MiB) and the per-keypoint matrices, whose size follows the count.
    return (live + transient) * 4 + (96ll << 20);
}

DetectorOut Detector::run(const Tensor& img, int64_t H, int64_t W, int max_features,
                          float min_score, uint32_t cap) {
    vk::Arena& arena = *pyr_.arena;
    const int64_t P = H * W;

    std::vector<Level> taps;
    pyr_.encode(taps, img, H, W);
    std::vector<Level> levels(taps.rbegin(), taps.rend());

    Tensor logits = nn::arena_tensor(arena, DType::F32, H, W, 1, 1, /*ndim=*/3);
    pyr_.cascade(logits, levels, /*bicubic_head=*/true);

    Tensor probs = nn::arena_tensor(arena, DType::F32, H, W, 1, 1, /*ndim=*/3);
    nn::softmax_rows(probs.view(1, P), logits.view(1, P));
    dump_tensor("det_logits", logits, {H, W});
    dump_tensor("det_probs", probs, {H, W});

    std::vector<uint32_t> cand;
    uint32_t n_cand = 0;
    {
        vk::ArenaScope scope(arena);
        Tensor list = nn::arena_tensor(arena, DType::I32, (int64_t)cap, 3);
        Tensor counter = nn::arena_tensor(arena, DType::I32, 4);
        vk::Stream::get().zero(counter.ptr, 16);

        PeakParams pp{};
        pp.out = list.ptr;
        pp.counter = counter.ptr;
        pp.probs = probs.ptr;
        pp.H = (uint32_t)H;
        pp.W = (uint32_t)W;
        pp.min_score = min_score;
        pp.cap = cap;
        vk::Stream::get().dispatchFlat("loma.peak_collect", vk::SpecList{0u, 0u}, P, 256,
                                       &pp, sizeof(pp), &pp.groups_per_row);
        vk::Stream::get().download(&n_cand, counter.ptr, 4);
        if (n_cand > cap) {
            NN_LOG_WARN("[loma] peak list saturated (%u > %u); raise max_candidates\n",
                        n_cand, cap);
            n_cand = cap;
        }
        if (n_cand) {
            cand.resize((size_t)n_cand * 3);
            vk::Stream::get().download(cand.data(), list.ptr, (uint64_t)n_cand * 12);
        }
    }

    DetectorOut out;
    if (n_cand == 0) return out;

    // Top-K by probability, as DaD's topk ranks. The tie-break is by position:
    // the GPU appended these through an atomic, so their order varies run to
    // run and would make a reconstruction irreproducible.
    std::vector<uint32_t> order(n_cand);
    std::iota(order.begin(), order.end(), 0u);
    auto score_of = [&](uint32_t i) {
        float s;
        std::memcpy(&s, &cand[(size_t)i * 3 + 2], 4);
        return s;
    };
    const uint32_t keep = std::min<uint32_t>(
        n_cand, max_features > 0 ? (uint32_t)max_features : n_cand);
    std::partial_sort(order.begin(), order.begin() + keep, order.end(),
                      [&](uint32_t a, uint32_t b) {
                          const float sa = score_of(a), sb = score_of(b);
                          if (sa != sb) return sa > sb;
                          if (cand[(size_t)a * 3 + 1] != cand[(size_t)b * 3 + 1])
                              return cand[(size_t)a * 3 + 1] < cand[(size_t)b * 3 + 1];
                          return cand[(size_t)a * 3] < cand[(size_t)b * 3];
                      });
    order.resize(keep);

    std::vector<int32_t> centers((size_t)keep * 2);
    out.score.resize(keep);
    for (uint32_t i = 0; i < keep; ++i) {
        centers[(size_t)i * 2] = (int32_t)cand[(size_t)order[i] * 3];
        centers[(size_t)i * 2 + 1] = (int32_t)cand[(size_t)order[i] * 3 + 1];
        out.score[i] = score_of(order[i]);
    }

    // Sub-pixel: softmax over the 3x3 patch of the LOGITS at temperature 0.5,
    // times the +-1 pixel offsets. patch_gather, softmax_rows and a [2, 9]
    // linear are exactly that, so this needs no kernel of its own.
    std::vector<float> offs((size_t)keep * 2);
    {
        vk::ArenaScope scope(arena);
        Tensor tc = nn::arena_tensor(arena, DType::I32, keep, 2);
        vk::Stream::get().upload(tc.ptr, centers.data(), (uint64_t)keep * 8);

        Tensor patch = nn::arena_tensor(arena, DType::F32, keep, 9);
        nn::patch_gather(patch, logits, tc, 3);
        nn::unary(patch, patch, nn::Act::None, /*pre_scale=*/1.0f / 0.5f);
        nn::softmax_rows(patch, patch);

        Tensor xy = nn::arena_tensor(arena, DType::F32, keep, 2);
        nn::Tensor wm(offsets_, DType::F32, 2, 9);
        nn::linear(xy, patch, wm);
        vk::Stream::get().download(offs.data(), xy.ptr, (uint64_t)keep * 8);
    }

    // The grid DaD samples on puts pixel (c, r) at ((2c+1)/W - 1, (2r+1)/H - 1),
    // i.e. the pixel's centre, and one pixel of offset is 2/W of it.
    out.xy.resize((size_t)keep * 2);
    for (uint32_t i = 0; i < keep; ++i) {
        const float cx = (float)centers[(size_t)i * 2] + 0.5f + offs[(size_t)i * 2];
        const float cy = (float)centers[(size_t)i * 2 + 1] + 0.5f + offs[(size_t)i * 2 + 1];
        out.xy[(size_t)i * 2] = 2.0f * cx / (float)W - 1.0f;
        out.xy[(size_t)i * 2 + 1] = 2.0f * cy / (float)H - 1.0f;
    }
    return out;
}

}  // namespace loma
