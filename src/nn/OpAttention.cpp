#include "nn/core/Error.h"
#include "nn/OpMetal.h"
#include "nn/Ops.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>

namespace nn {

namespace {

struct AttnParams {
    uint64_t out, q, k, v, bias, part_ml, labels;
    uint32_t nq, nk, n_heads;
    float    scale;
    uint32_t q_stride, k_stride, v_stride, out_stride;
    uint32_t bias_stride_h, bias_stride_q, keys_per_split, n_splits;
};

struct CombineParams {
    uint64_t out, part, part_ml;
    uint32_t nq, n_heads, n_splits, out_stride, groups_per_row, _pad0, _pad1;
};

struct RopeParams {
    uint64_t x, freqs;
    uint32_t n, n_heads, head_dim, batch, row_stride, groups_per_row;
    uint32_t split_half;
};

// BR in attention.slang: one workgroup owns this many queries, whatever the
// head dim -- Q is staged in 16-dim chunks, so the shared budget no longer
// scales with head_dim.
constexpr uint32_t kQueriesPerBlock = 64;

// Flash-decoding thresholds. `flash_attn` is bound by occupancy rather than by
// bandwidth (src/nn/README.md has the measurements), so a problem that does not
// produce enough workgroups to cover the device runs at half speed no matter
// how the arithmetic is arranged -- SAM 2's memory attention is 4096 queries
// over ONE head, which is 64 workgroups. Splitting the key range fixes that at
// the cost of a partial buffer and a combine pass.
//
// The target is a workgroup count, not a device query: core Vulkan does not
// expose an SM/CU count, and 512 covers everything from a 20-CU laptop part to
// a 128-SM desktop one. The floor on keys per split keeps the combine pass and
// the per-slice softmax setup from eating the win on short key ranges.
constexpr uint32_t kTargetBlocks = 512;
constexpr uint32_t kMinKeysPerSplit = 512;
// BC in attention.slang; a split has to start on a key-tile boundary.
constexpr uint32_t kKeyTile = 32;

// Q K^T and P V, weighted 2x: until flash_attn has a rate of its own it borrows
// another kernel's, and it reaches 15-20% of peak to a GEMM's ~40% (src/nn/README.md).
double attention_work(int64_t nq, int64_t nk, const AttnOpts& o) {
    return 2.0 * 4.0 * nq * nk * o.head_dim * o.n_heads * o.batch;
}

struct KeySplit {
    uint32_t splits = 1, per = 0;  // per: keys a split covers, when splits > 1
};

// Decided for `plan_nq` x `plan_batch`, the whole problem when a call is one
// slice of it, so a query's result does not depend on how the submit budget
// happened to slice the call.
KeySplit key_split(int64_t plan_nq, int plan_batch, int64_t nk, const AttnOpts& o) {
    KeySplit ks;
    const uint32_t plan_gx = (uint32_t)((plan_nq + kQueriesPerBlock - 1) / kQueriesPerBlock);
    const uint32_t blocks = plan_gx * (uint32_t)o.n_heads * (uint32_t)plan_batch;
    if (o.arena && plan_batch == 1 && blocks < kTargetBlocks &&
        nk >= 2 * (int64_t)kMinKeysPerSplit) {
        const uint32_t splits = std::min(kTargetBlocks / std::max(blocks, 1u),
                                         (uint32_t)(nk / kMinKeysPerSplit));
        // Round the slice up to a whole key tile, then recount so no workgroup
        // is handed an empty range.
        if (splits > 1) {
            ks.per = (uint32_t)(((nk + splits - 1) / splits + kKeyTile - 1) / kKeyTile) *
                     kKeyTile;
            ks.splits = (uint32_t)((nk + ks.per - 1) / ks.per);
        }
    }
    return ks;
}

// `spec` is shared with the combine pass; `espec` adds what the coop kernel needs.
struct AttnKernel {
    const char*  entry;
    vk::SpecList spec, espec;
};

AttnKernel attention_kernel(const Tensor& q, const Tensor& k, const AttnOpts& o,
                            uint32_t splits) {
    // Tensor cores carry Q @ K^T when the head dim is a multiple of the fragment's
    // K: Hiera's 96 is, its 72 is not, and the rest are 16, 32, 64 or 256.
    // attention_coop.slang says why P @ V stays on the scalar path.
    const bool coop = o.allow_coop && coop_matrix_enabled() && o.head_dim % 16 == 0;
    AttnKernel kn;
    kn.entry = coop ? "attention_coop.flash_attn_coop" : "attention.flash_attn";
    kn.spec = {(uint32_t)o.head_dim, (uint32_t)o.bias_mode, (uint32_t)(q.dtype == DType::F16),
               (uint32_t)(k.dtype == DType::F16), splits > 1 ? 1u : 0u};
    kn.espec = kn.spec;
    if (coop)
        kn.espec.values[kn.espec.count++] = 256u / vk::Context::get().preferredSubgroupSize();
    return kn;
}

// `plan_nq` x `plan_batch` is the whole problem when this is one slice of it.
void attention_one(const Tensor& out, const Tensor& q, const Tensor& k, const Tensor& v,
                   int64_t nq, int64_t nk, const AttnOpts& o, int64_t plan_nq,
                   int plan_batch) {
    // The kernel splits head_dim across 16 lanes and caps the per-thread
    // accumulator array at 256/16, so 256 is the ceiling. Any dim up to that is
    // fine -- a dim that does not divide 16 just leaves some lanes idle in the
    // last chunk (Hiera runs 96 and 72).
    NN_CHECK(o.head_dim <= 256, "attention: head_dim %d exceeds the 256 ceiling",
               o.head_dim);
    NN_CHECK(out.dtype == DType::F32, "attention output must be f32");

    check_span("attention", {out, q, k, v, o.bias});

    const uint32_t dim = (uint32_t)(o.n_heads * o.head_dim);
    AttnParams p{};
    p.out = out.ptr;
    p.q = q.ptr;
    p.k = k.ptr;
    p.v = v.ptr;
    p.bias = vk::or_fallback(o.bias.ptr);
    p.part_ml = vk::or_fallback(0);
    p.labels = vk::or_fallback(o.labels.ptr);
    p.nq = (uint32_t)nq;
    p.nk = (uint32_t)nk;
    p.n_heads = (uint32_t)o.n_heads;
    p.scale = o.scale > 0.0f ? o.scale : 1.0f / std::sqrt((float)o.head_dim);
    p.q_stride = (uint32_t)(o.q_stride > 0 ? o.q_stride : dim);
    p.k_stride = (uint32_t)(o.k_stride > 0 ? o.k_stride : dim);
    p.v_stride = (uint32_t)(o.v_stride > 0 ? o.v_stride : dim);
    p.out_stride = (uint32_t)(o.out_stride > 0 ? o.out_stride : dim);
    p.bias_stride_h = (uint32_t)(o.bias_stride_h > 0 ? o.bias_stride_h : nq * nk);
    p.bias_stride_q = (uint32_t)(o.bias_stride_q > 0 ? o.bias_stride_q : nk);

    if (o.bias_mode == AttnBias::PerKey)
        NN_CHECK(o.bias.valid() && o.bias.numel() >= nk,
                   "attention: PerKey bias needs %lld entries", (long long)nk);
    if (o.bias_mode == AttnBias::Full)
        NN_CHECK(o.bias.valid(), "attention: Full bias mode needs a bias tensor");
    if (o.bias_mode == AttnBias::Window)
        NN_CHECK(o.bias.valid() && o.labels.valid() && nq == nk &&
                     o.labels.numel() >= (int64_t)o.batch * nq,
                 "attention: Window mode needs a bias, and labels for %d x %lld tokens",
                 o.batch, (long long)nq);

    const uint32_t gx = (uint32_t)((nq + kQueriesPerBlock - 1) / kQueriesPerBlock);
    NN_CHECK(gx <= 65535, "attention: %lld queries exceed the dispatch grid cap",
               (long long)nq);

    const KeySplit ks = key_split(plan_nq, plan_batch, nk, o);
    const uint32_t splits = ks.splits;
    if (splits > 1) {
        p.keys_per_split = ks.per;
        p.n_splits = splits;
    }
    const AttnKernel kn = attention_kernel(q, k, o, splits);

    const double work = attention_work(nq, nk, o);
    if (splits <= 1) {
        vk::Stream::get().dispatch(kn.entry, kn.espec, gx, (uint32_t)o.n_heads,
                                   (uint32_t)o.batch, &p, sizeof(p), work);
        return;
    }

    // The partials are dead the moment the combine is recorded, and the stream
    // barriers every dispatch, so rewinding the arena here cannot let a later op
    // race them.
    vk::ArenaScope scope(*o.arena);
    const int64_t part_elems = (int64_t)splits * nq * dim;
    Tensor part = arena_tensor(*o.arena, DType::F32, part_elems);
    Tensor part_ml = arena_tensor(*o.arena, DType::F32,
                                  (int64_t)splits * o.n_heads * nq * 2);
    p.out = part.ptr;
    p.out_stride = dim;
    p.part_ml = part_ml.ptr;
    vk::Stream::get().dispatch(kn.entry, kn.espec, gx, (uint32_t)o.n_heads, splits, &p,
                               sizeof(p), work);

    CombineParams cp{};
    cp.out = out.ptr;
    cp.part = part.ptr;
    cp.part_ml = part_ml.ptr;
    cp.nq = (uint32_t)nq;
    cp.n_heads = (uint32_t)o.n_heads;
    cp.n_splits = splits;
    cp.out_stride = (uint32_t)(o.out_stride > 0 ? o.out_stride : dim);
    vk::Stream::get().dispatchFlat("attention.flash_attn_combine", kn.spec, nq * dim, 256,
                                   &cp, sizeof(cp), &cp.groups_per_row);
}

}  // namespace

// SAM's memory attention is ~190 GFLOP in one dispatch, ~2 s on a 2-CU iGPU and
// past its watchdog, so a problem over the submit budget runs as slices: query
// blocks, or batch items when the batch stride is derived from nq.
void attention(const Tensor& out, const Tensor& q, const Tensor& k, const Tensor& v,
               int64_t nq, int64_t nk, const AttnOpts& o) {
    NN_CHECK(o.head_dim > 0 && o.n_heads > 0, "attention: head_dim/n_heads unset");
    if (metal_attention(out, q, k, v, nq, nk, o)) return;
    const AttnKernel kn = attention_kernel(q, k, o, key_split(nq, o.batch, nk, o).splits);
    const double cap = vk::Stream::get().workCap(kn.entry, kn.espec);
    const double work = attention_work(nq, nk, o);
    if (cap <= 0 || work <= cap) return attention_one(out, q, k, v, nq, nk, o, nq, o.batch);

    const int64_t dim = (int64_t)o.n_heads * o.head_dim;
    const int64_t qs = o.q_stride > 0 ? o.q_stride : dim;
    const int64_t os = o.out_stride > 0 ? o.out_stride : dim;
    AttnOpts so = o;
    so.q_stride = qs;
    so.out_stride = os;
    so.bias_stride_h = o.bias_stride_h > 0 ? o.bias_stride_h : nq * nk;
    so.bias_stride_q = o.bias_stride_q > 0 ? o.bias_stride_q : nk;

    if (o.batch > 1) {
        const int64_t per = std::max<int64_t>(1, (int64_t)(cap / (work / o.batch)));
        const int64_t ks = o.k_stride > 0 ? o.k_stride : dim;
        const int64_t vs = o.v_stride > 0 ? o.v_stride : dim;
        for (int64_t b0 = 0; b0 < o.batch; b0 += per) {
            so.batch = (int)std::min<int64_t>(per, o.batch - b0);
            if (o.bias_mode == AttnBias::Window) so.labels = o.labels.offsetElems(b0 * nq);
            attention_one(out.offsetElems(b0 * nq * os), q.offsetElems(b0 * nq * qs),
                          k.offsetElems(b0 * nk * ks), v.offsetElems(b0 * nk * vs), nq, nk,
                          so, nq, o.batch);
        }
        return;
    }
    // A window's labels are indexed by absolute query and key, so a lone window
    // is not sliced; one is ws^2 tokens and nowhere near the budget.
    if (o.bias_mode == AttnBias::Window) return attention_one(out, q, k, v, nq, nk, o, nq, 1);
    const int64_t per =
        std::max<int64_t>(1, (int64_t)(cap / (work / nq)) / kQueriesPerBlock) *
        kQueriesPerBlock;
    for (int64_t q0 = 0; q0 < nq; q0 += per) {
        if (o.bias_mode == AttnBias::Full) so.bias = o.bias.offsetElems(q0 * so.bias_stride_q);
        attention_one(out.offsetElems(q0 * os), q.offsetElems(q0 * qs), k, v,
                      std::min(per, nq - q0), nk, so, nq, 1);
    }
}

void rope(const Tensor& x, const Tensor& freqs, int n_heads, int head_dim, int64_t n,
          int batch, int64_t row_stride, bool split_half) {
    NN_CHECK(x.dtype == DType::F32, "rope operates in place on f32");
    NN_CHECK(head_dim > 0 && n_heads > 0 && n > 0 && batch > 0 && (head_dim & 1) == 0,
             "rope: dimensions must be positive and head_dim even");
    NN_CHECK(freqs.numel() >= n * (head_dim / 2) * 2,
               "rope: frequency table is too small for %lld tokens", (long long)n);
    RopeParams p{};
    p.x = x.ptr;
    p.freqs = freqs.ptr;
    p.n = (uint32_t)n;
    p.n_heads = (uint32_t)n_heads;
    p.head_dim = (uint32_t)head_dim;
    p.batch = (uint32_t)batch;
    p.row_stride = (uint32_t)(row_stride > 0 ? row_stride : (int64_t)n_heads * head_dim);
    p.split_half = split_half ? 1u : 0u;
    const int64_t total = (int64_t)batch * n * n_heads * (head_dim / 2);
    const KernelName entry = span_entry("misc.rope_apply", {x, freqs});
    vk::Stream::get().dispatchFlat(entry, {0u, 0u}, total, 256, &p, sizeof(p),
                                   &p.groups_per_row);
}

}  // namespace nn
