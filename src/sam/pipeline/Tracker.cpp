// Video tracking: masklet bookkeeping, the per-instance memory bank, and the
// propagate -> detect -> associate loop.
//
// The model work per instance is memory attention + the SAM decoder + the
// memory encoder; everything else here is host logic ported from the
// reference's tracker: the hotstart confirmation window, the masklet detection
// score, eviction, and mutual exclusion between instance masks.

#include "sam/Common.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "sam/pipeline/PostProcess.h"
#include "sam/pipeline/Session.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

namespace sam {

namespace {

float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

// Device buffers for memory-bank slots are keyed by a small integer that is
// recycled as instances come and go, so the VRAM report stays readable and the
// allocation count stays bounded by (live instances x num_maskmem).
//
// The high half of the key is the tracker's own id, because VramPool is
// process-wide while this allocator is not: a masking prompt of "people; cars"
// builds one Tracker per phrase, and without the split both would number their
// slots from zero and write their memory banks over each other.
class SlotAllocator {
public:
    SlotAllocator() {
        static std::atomic<uint32_t> next_tracker{0};
        base_ = (next_tracker++ & 0xffffu) << 16;
    }

    uint32_t acquire() {
        if (!free_.empty()) {
            uint32_t s = free_.back();
            free_.pop_back();
            return s;
        }
        NN_CHECK(next_ < 0xffffu, "memory-bank slots exhausted for one tracker");
        return base_ | next_++;
    }
    void release(uint32_t s) { free_.push_back(s); }

    // Frees the device memory behind every slot this tracker has ever used --
    // its own only, which is why the pool cannot simply be told to drop the
    // whole MemBankFeat slot.
    void freeAll() {
        if (vk::Context::initialized())
            for (uint32_t i = 0; i < next_; ++i)
                vk::VramPool::get().release(vk::PoolSlot::MemBankFeat, base_ | i);
        free_.clear();
        next_ = 0;
    }

private:
    std::vector<uint32_t> free_;
    uint32_t base_ = 0;    // this tracker's key range, in the high 16 bits
    uint32_t next_ = 0;    // next unused index within it
};

struct MemorySlot {
    uint32_t sub = 0;          // VramPool sub-index of the [G, G, mem_dim] features
    int      frame = -1;
    bool     conditioning = false;
};

struct ObjectPointer {
    int                frame = -1;
    std::vector<float> data;   // [neck_dim]
};

struct Masklet {
    int   id = -1;
    int   first_frame = -1;
    int   last_seen = -1;
    float last_score = 0.0f;
    bool  confirmed = false;
    // Masklet Detection Score: +1 on a frame where the instance was matched or
    // visibly propagated, -1 otherwise. Drives both confirmation and eviction.
    int   mds = 0;
    std::vector<MemorySlot>   memory;
    std::vector<ObjectPointer> pointers;
};

// One propagation's result, already on the host.
struct Propagated {
    bool  ok = false;
    std::vector<float> mask_logits;   // [mask_size * mask_size]
    float iou = 0.0f;
    float obj_logit = 0.0f;
    std::vector<float> sam_token;     // [neck_dim]
    Mask  mask;                       // at original resolution
    float coverage = 0.0f;
};

}  // namespace

struct Tracker::Impl {
    Session::Impl& s;
    VideoParams    params;
    int            frame_index = 0;
    int            next_id = 1;
    std::vector<Masklet> active;
    std::vector<Masklet> pending;
    SlotAllocator  slots;

    explicit Impl(Session::Impl& sess, const VideoParams& p) : s(sess), params(p) {}

    // ---- memory bank ----
    // `frame` is the frame the memory DESCRIBES, which is not always the one
    // the tracker is about to process: addInstance runs after trackFrame has
    // already advanced the counter.
    void storeMemory(Masklet& ml, const nn::Tensor& mask_logits, int64_t mh, int64_t mw,
                     bool conditioning, float obj_logit, int frame);
    void storeObjectPointer(Masklet& ml, const std::vector<float>& token, float obj_logit,
                            int frame);
    void evict(Masklet& ml);

    // ---- per-instance model work ----
    Propagated propagate(const Masklet& ml);
    // Decoder logits [S, S] -> a binary mask at the source resolution. The
    // threshold, the resize and the byte packing are one kernel, so the host
    // never sees a float mask.
    Mask maskFromLogits(const nn::Tensor& logits);
    // Runs the SAM decoder from an interactive prompt against the current
    // frame, unconditioned by memory, and returns the mask slot to use.
    int64_t decodePrompt(const VisualPrompt& prompt, model::SamDecoderOutputs& dec,
                         std::vector<float>& iou, float& obj_logit,
                         std::vector<float>& token);

    // ---- bookkeeping ----
    void updateMasklets();
    Result buildResult(const std::map<int, Mask>& masks);
};

// ---------------------------------------------------------------------------
// Memory bank
// ---------------------------------------------------------------------------

void Tracker::Impl::storeMemory(Masklet& ml, const nn::Tensor& mask_logits, int64_t mh,
                                int64_t mw, bool conditioning, float obj_logit,
                                int frame) {
    const Hparams& h = s.model.hp();
    const int G = s.model.grid();
    const int64_t MD = h.mem_out_dim;

    MemorySlot slot;
    slot.sub = slots.acquire();
    slot.frame = frame;
    slot.conditioning = conditioning;
    nn::Tensor dst = nn::pool_tensor(vk::PoolSlot::MemBankFeat, slot.sub, nn::DType::F32,
                                     G, G, MD);
    model::encode_memory(s.model, s.arena, mask_logits, mh, mw, s.feats.trk[2], dst);

    // An occluded frame gets the learned "no object" spatial embedding added,
    // so the memory carries the absence rather than a stale mask.
    if (obj_logit <= 0.0f && !s.model.noObjEmbedSpatial().empty()) {
        vk::ArenaScope scope(s.arena);
        nn::Tensor emb = nn::arena_tensor(s.arena, nn::DType::F32, MD);
        nn::tensor_from_host(emb, s.model.noObjEmbedSpatial().data(), MD);
        nn::add(dst.view((int64_t)G * G, MD), dst.view((int64_t)G * G, MD), emb);
    }

    ml.memory.push_back(slot);
    // Sliding window: drop the oldest NON-conditioning slot first, so a
    // user-provided or detector-provided frame survives.
    int keep = h.num_maskmem;
    if (params.max_memory_frames > 0)
        keep = std::min(keep, params.max_memory_frames);
    while ((int)ml.memory.size() > keep) {
        auto it = std::find_if(ml.memory.begin(), ml.memory.end(),
                               [](const MemorySlot& m) { return !m.conditioning; });
        if (it == ml.memory.end()) it = ml.memory.begin() + 1;
        // The sub-index goes back to the free list but the buffer STAYS
        // allocated: it is exactly the size the next slot needs, and freeing it
        // would drain the queue (Allocator::free's contract) in the middle of a
        // frame. VRAM is bounded by the high-water number of live slots.
        slots.release(it->sub);
        ml.memory.erase(it);
    }
}

void Tracker::Impl::storeObjectPointer(Masklet& ml, const std::vector<float>& token,
                                       float obj_logit, int frame) {
    const int64_t D = s.model.hp().neck_dim;
    ObjectPointer ptr;
    ptr.frame = frame;
    ptr.data.assign((size_t)D, 0.0f);

    if (obj_logit <= 0.0f || token.empty()) {
        // Not visible: the reference substitutes the learned no-object pointer
        // rather than projecting a meaningless token.
        ptr.data = s.model.noObjPtr();
        ptr.data.resize((size_t)D, 0.0f);
    } else {
        // A 3-layer 256->256 MLP per instance per frame: ~200 kFLOP, far below
        // the cost of the dispatch it would take to run it on device.
        std::vector<float> cur(token.begin(), token.begin() + (size_t)D);
        std::vector<float> next((size_t)D);
        for (int layer = 0; layer < 3; ++layer) {
            const std::vector<float>& w = s.model.objPtrProjW(layer);
            const std::vector<float>& b = s.model.objPtrProjB(layer);
            for (int64_t o = 0; o < D; ++o) {
                float acc = b[(size_t)o];
                const float* row = w.data() + (size_t)(o * D);
                for (int64_t i = 0; i < D; ++i) acc += row[i] * cur[(size_t)i];
                next[(size_t)o] = (layer < 2) ? std::fmax(acc, 0.0f) : acc;
            }
            cur.swap(next);
        }
        ptr.data = cur;
    }

    ml.pointers.push_back(std::move(ptr));
    while ((int)ml.pointers.size() > s.model.hp().max_obj_ptrs)
        ml.pointers.erase(ml.pointers.begin());
}

void Tracker::Impl::evict(Masklet& ml) {
    // Same reasoning as the sliding window: recycle the slot index, keep the
    // buffer. Tracker::reset() is the only thing that hands VRAM back, and it
    // is not on any per-frame path.
    for (const MemorySlot& m : ml.memory) slots.release(m.sub);
    ml.memory.clear();
    ml.pointers.clear();
}

// ---------------------------------------------------------------------------
// Propagation
// ---------------------------------------------------------------------------

Mask Tracker::Impl::maskFromLogits(const nn::Tensor& logits) {
    const int64_t S = (int64_t)s.model.grid() * 4;
    const int W = s.feats.orig_width, H = s.feats.orig_height;
    const int64_t bytes = (int64_t)W * H;

    vk::ArenaScope scope(s.arena);
    nn::Tensor out_u8 = nn::arena_tensor(s.arena, nn::DType::U8, (bytes + 3) / 4 * 4);
    nn::resize_binarize(out_u8, logits.view(S, S), H, W, 0.0f);

    Mask mask;
    mask.width = W;
    mask.height = H;
    mask.data.resize((size_t)bytes);
    vk::Stream::get().download(mask.data.data(), out_u8.ptr, (uint64_t)bytes);
    return mask;
}

Propagated Tracker::Impl::propagate(const Masklet& ml) {
    Propagated out;
    if (ml.memory.empty()) return out;

    const Hparams& h = s.model.hp();
    const int G = s.model.grid();
    const int64_t N = (int64_t)G * G;
    const int64_t D = h.neck_dim;
    const int64_t MD = h.mem_out_dim;
    const int64_t S = (int64_t)G * 4;

    vk::ArenaScope scope(s.arena);

    // ---- assemble the memory prompt ----
    const int n_slots = (int)ml.memory.size();
    const int n_ptrs = (int)std::min<size_t>(ml.pointers.size(),
                                             (size_t)h.max_obj_ptrs);
    const int split = (int)(D / MD);              // 256 / 64 = 4 tokens per pointer
    const int64_t n_ptr_tokens = (int64_t)n_ptrs * split;
    const int64_t n_spatial = (int64_t)n_slots * N;
    const int64_t n_mem = n_spatial + n_ptr_tokens;

    nn::Tensor prompt = nn::arena_tensor(s.arena, nn::DType::F32, n_mem, MD);
    nn::Tensor prompt_pos = nn::arena_tensor(s.arena, nn::DType::F32, n_mem, MD);

    for (int i = 0; i < n_slots; ++i) {
        const MemorySlot& slot = ml.memory[(size_t)i];
        nn::Tensor src(vk::VramPool::get().lookup(vk::PoolSlot::MemBankFeat, slot.sub),
                       nn::DType::F32, N, MD);
        nn::strided_copy(prompt.slice0((int64_t)i * N, N), src, N, MD, MD, MD);

        // Spatial PE is the same grid encoding for every slot; only the
        // temporal component differs, and the reference indexes it backwards
        // (num_maskmem - t_pos - 1) so the most recent frame gets the LAST
        // embedding.
        nn::Tensor pos = prompt_pos.slice0((int64_t)i * N, N);
        nn::strided_copy(pos, s.model.memPe().view(N, MD), N, MD, MD, MD);

        // Which maskmem_tpos_enc entry this slot gets. SAM 2 indexes by how
        // many frames back the slot is -- the previous frame takes entry 0 and
        // a conditioning frame the last one. sam3.cpp orders it from the
        // slot's position in the bank instead, and that is what the SAM 3 path
        // has been validated against, so both live here.
        int enc;
        if (h.family == Family::Sam2) {
            enc = slot.conditioning
                      ? h.num_maskmem - 1
                      : std::min(std::max(frame_index - slot.frame - 1, 0),
                                 h.num_maskmem - 1);
        } else {
            const int t_pos = slot.conditioning ? 0 : (n_slots - i);
            enc = (t_pos < h.num_maskmem) ? (h.num_maskmem - t_pos - 1) : -1;
        }
        if (enc >= 0) {
            const std::vector<float>& tp = s.model.maskMemTpos();
            if ((int64_t)tp.size() >= (int64_t)(enc + 1) * MD) {
                nn::Tensor t = nn::arena_tensor(s.arena, nn::DType::F32, MD);
                nn::tensor_from_host(t, tp.data() + (size_t)enc * MD, MD);
                nn::add(pos, pos, t);
            }
        }
    }

    if (n_ptr_tokens > 0) {
        // Each 256-dim pointer becomes four 64-dim tokens; its positional
        // encoding is a 1-D sine of the frame distance, projected to 64 dims.
        std::vector<float> host((size_t)(n_ptr_tokens * MD));
        std::vector<float> host_pos((size_t)(n_ptr_tokens * MD));
        std::vector<float> sine((size_t)D), proj((size_t)MD);
        const std::vector<float>& tw = s.model.objPtrTposW();
        const std::vector<float>& tb = s.model.objPtrTposB();
        for (int p = 0; p < n_ptrs; ++p) {
            const ObjectPointer& op = ml.pointers[(size_t)p];
            int rel = std::abs(frame_index - op.frame);
            if (rel < 1) rel = 1;
            sine_pe_1d(sine.data(), (float)rel / (float)(h.max_obj_ptrs - 1), (int)D);
            for (int64_t o = 0; o < MD; ++o) {
                float acc = tb.empty() ? 0.0f : tb[(size_t)o];
                if (!tw.empty()) {
                    const float* row = tw.data() + (size_t)(o * D);
                    for (int64_t i = 0; i < D; ++i) acc += row[i] * sine[(size_t)i];
                }
                proj[(size_t)o] = acc;
            }
            for (int t = 0; t < split; ++t) {
                const size_t base = (size_t)((p * split + t) * MD);
                std::memcpy(host.data() + base, op.data.data() + (size_t)(t * MD),
                            (size_t)MD * sizeof(float));
                std::memcpy(host_pos.data() + base, proj.data(),
                            (size_t)MD * sizeof(float));
            }
        }
        nn::tensor_from_host(prompt.slice0(n_spatial, n_ptr_tokens), host.data(),
                             (int64_t)host.size());
        nn::tensor_from_host(prompt_pos.slice0(n_spatial, n_ptr_tokens), host_pos.data(),
                             (int64_t)host_pos.size());
    }

    // ---- memory attention + mask decode ----
    nn::Tensor cond = nn::arena_tensor(s.arena, nn::DType::F32, N, D);
    model::memory_attention(s.model, s.arena, s.feats.trk[2].view(N, D),
                            s.model.neckPe().view(N, D), prompt, prompt_pos, n_mem,
                            (int)n_ptr_tokens, cond);

    // Propagation carries no user prompt; the reference still feeds a single
    // "not a point" token rather than an empty sparse set.
    nn::Tensor sparse = nn::arena_tensor(s.arena, nn::DType::F32, 1, D);
    nn::tensor_from_host(sparse, s.model.notAPointEmbed(), D);

    model::SamDecoderOutputs dec;
    model::sam_mask_decoder(s.model, s.arena, cond.view(G, G, D), sparse, 1,
                            s.feats.trk[0], s.feats.trk[1], dec);

    // Slot 0 is the single-mask output; slots 1..3 resolve ambiguity. SAM 3
    // tracking always takes slot 0, while SAM 2 decodes all three and keeps
    // whichever the IoU head rates highest -- multimask_output_for_tracking,
    // with a propagation step counting as a zero-point prompt.
    std::vector<float> iou((size_t)h.num_mask_tokens()), obj(1);
    nn::tensor_to_host(dec.iou, iou.data(), h.num_mask_tokens());
    nn::tensor_to_host(dec.obj_score, obj.data(), 1);
    int64_t best = 0;
    if (h.use_multimask(/*n_points=*/0, /*init_frame=*/false)) {
        best = 1;
        for (int64_t k = 2; k < h.num_mask_tokens(); ++k)
            if (iou[(size_t)k] > iou[(size_t)best]) best = k;
    }
    out.iou = iou[(size_t)best];
    out.obj_logit = obj[0];
    out.sam_token.resize((size_t)D);
    nn::tensor_to_host(dec.mask_tokens.slice0(best, 1), out.sam_token.data(), D);

    out.mask_logits.resize((size_t)(S * S));
    nn::tensor_to_host(dec.masks.slice0(best, 1), out.mask_logits.data(), S * S);

    out.mask = maskFromLogits(dec.masks.slice0(best, 1));
    size_t fg = 0;
    for (uint8_t v : out.mask.data) fg += (v > 127) ? 1 : 0;
    out.coverage = (float)fg / (float)std::max<size_t>(out.mask.data.size(), 1);
    out.ok = true;
    return out;
}

// ---------------------------------------------------------------------------
// Bookkeeping
// ---------------------------------------------------------------------------

void Tracker::Impl::updateMasklets() {
    // Promote or drop pending masklets once the confirmation window closes.
    for (auto it = pending.begin(); it != pending.end();) {
        const int age = frame_index - it->first_frame;
        if (age >= params.hotstart_delay) {
            if (it->mds > 0) {
                it->confirmed = true;
                active.push_back(std::move(*it));
            } else {
                evict(*it);
            }
            it = pending.erase(it);
        } else {
            ++it;
        }
    }
    // Evict tracks that have gone unseen too long.
    for (auto it = active.begin(); it != active.end();) {
        if (frame_index - it->last_seen > params.max_keep_alive) {
            evict(*it);
            it = active.erase(it);
        } else {
            ++it;
        }
    }
}

Result Tracker::Impl::buildResult(const std::map<int, Mask>& masks) {
    Result result;
    auto emit = [&](const Masklet& ml) {
        auto it = masks.find(ml.id);
        if (it == masks.end() || it->second.data.empty()) return;
        Detection d;
        d.instance_id = ml.id;
        d.score = ml.last_score;
        d.iou_score = ml.last_score;
        d.mask = it->second;
        d.mask.instance_id = ml.id;
        d.mask.iou_score = ml.last_score;
        mask_bounding_box(d.mask, d.box);
        result.detections.push_back(std::move(d));
    };
    for (const Masklet& ml : active) emit(ml);
    for (const Masklet& ml : pending) emit(ml);

    resolve_overlaps(result.detections);
    for (Detection& d : result.detections) {
        if (d.mask.data.empty()) continue;
        fill_holes(d.mask.data.data(), d.mask.width, d.mask.height, params.fill_hole_area);
        remove_sprinkles(d.mask.data.data(), d.mask.width, d.mask.height,
                         params.fill_hole_area);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Interactive prompts
// ---------------------------------------------------------------------------

int64_t Tracker::Impl::decodePrompt(const VisualPrompt& prompt,
                                    model::SamDecoderOutputs& dec,
                                    std::vector<float>& iou, float& obj_logit,
                                    std::vector<float>& token) {
    const Hparams& h = s.model.hp();
    const int64_t D = h.sam_embed_dim;
    const int G = s.model.grid();
    const int64_t N = (int64_t)G * G;
    const int64_t NM = h.num_mask_tokens();

    nn::Tensor sparse;
    const int64_t n_sparse = s.buildSparsePrompt(prompt, sparse);

    // No memory yet, so the features enter with the learned "no memory"
    // embedding added (directly_add_no_mem_embed).
    nn::Tensor embed = nn::arena_tensor(s.arena, nn::DType::F32, N, D);
    nn::add(embed, s.feats.trk[2].view(N, D), s.model.w().get("no_mem_embed"));
    model::sam_mask_decoder(s.model, s.arena, embed, sparse, n_sparse, s.feats.trk[0],
                            s.feats.trk[1], dec);

    iou.resize((size_t)NM);
    std::vector<float> obj(1);
    nn::tensor_to_host(dec.iou, iou.data(), NM);
    nn::tensor_to_host(dec.obj_score, obj.data(), 1);
    obj_logit = obj[0];

    // buildSparsePrompt appends a padding token; _use_multimask counts only the
    // real ones, with a box contributing its two corners.
    const int n_points = (int)(prompt.pos_points.size() + prompt.neg_points.size()) +
                         (prompt.use_box ? 2 : 0);
    int64_t best = 0;
    if (h.use_multimask(n_points, /*init_frame=*/true)) {
        best = 1;
        for (int64_t k = 2; k < NM; ++k)
            if (iou[(size_t)k] > iou[(size_t)best]) best = k;
    }
    token.resize((size_t)D);
    nn::tensor_to_host(dec.mask_tokens.slice0(best, 1), token.data(), D);
    return best;
}

// ---------------------------------------------------------------------------
// Public surface
// ---------------------------------------------------------------------------

Tracker::Tracker(Session& session, const VideoParams& params)
    : impl_(new Impl(*session.impl_, params)) {}
Tracker::~Tracker() { reset(); }

int Tracker::frameIndex() const { return impl_->frame_index; }

void Tracker::reset() {
    for (Masklet& ml : impl_->active) impl_->evict(ml);
    for (Masklet& ml : impl_->pending) impl_->evict(ml);
    impl_->active.clear();
    impl_->pending.clear();
    impl_->slots.freeAll();
    impl_->frame_index = 0;
    impl_->next_id = 1;
}

Result Tracker::propagateFrame(const Image& frame) {
    VideoParams p = impl_->params;
    p.text_prompt.clear();
    VideoParams saved = impl_->params;
    impl_->params = p;
    Result r = trackFrame(frame);
    impl_->params = saved;
    return r;
}

Result Tracker::propagateEncoded() {
    VideoParams saved = impl_->params;
    impl_->params.text_prompt.clear();
    Result r = trackEncoded();
    impl_->params = saved;
    return r;
}

Result Tracker::trackFrame(const Image& frame) {
    Session::Impl& s = impl_->s;
    if (!s.loaded) {
        s.error = "trackFrame: no model loaded";
        return {};
    }
    try {
        // The Session's public encodeImage would work, but going through Impl
        // keeps the error handling in one place.
        s.arena.reset();
        vk::ArenaScope scope(s.arena);
        const int size = s.model.imgSize();
        auto host = model::preprocess_image(s.model, frame.data.data(), frame.width,
                                            frame.height);
        nn::Tensor img = nn::arena_tensor(s.arena, nn::DType::F32, size, size, 3);
        nn::tensor_from_host(img, host.data(), (int64_t)host.size());
        model::encode_image(s.model, s.arena, img, s.feats);
        s.feats.orig_width = frame.width;
        s.feats.orig_height = frame.height;
    } catch (const std::exception& e) {
        s.error = e.what();
        NN_LOG_ERROR("trackFrame: %s\n", e.what());
        return {};
    }
    return trackEncoded();
}

Result Tracker::trackEncoded() {
    Session::Impl& s = impl_->s;
    if (!s.loaded || !s.feats.valid()) {
        s.error = "trackEncoded: encode a frame first";
        return {};
    }
    try {
        const int fi = impl_->frame_index;
        NN_LOG_INFO("[track] frame %d: %zu active, %zu pending\n", fi,
                      impl_->active.size(), impl_->pending.size());

        // ---- 1. propagate everything that has memory ----
        std::map<int, Mask> masks;
        std::map<int, Propagated> props;
        auto propagate_into = [&](Masklet& ml) {
            Propagated p = impl_->propagate(ml);
            if (!p.ok) return;
            ml.last_score = p.iou;
            ml.last_seen = fi;
            ml.mds += (p.coverage > 0.001f && p.obj_logit > 0.0f) ? 1 : -1;
            masks[ml.id] = p.mask;
            props[ml.id] = std::move(p);
        };
        for (Masklet& ml : impl_->active) propagate_into(ml);
        for (Masklet& ml : impl_->pending) propagate_into(ml);

        // ---- 2. detect, when a text prompt is configured ----
        // With detect_every > 1 the detector runs periodically and the memory
        // bank carries the instances in between. That is what makes this a
        // video pipeline rather than per-frame segmentation, and on SAM 3 the
        // detector is about half the frame.
        const int every = std::max(1, impl_->params.detect_every);
        Result detections;
        if ((fi % every) == 0 && !impl_->params.text_prompt.empty() &&
            s.model.canTextPrompt()) {
            ConceptPrompt cp;
            cp.text = impl_->params.text_prompt;
            cp.score_threshold = impl_->params.score_threshold;
            cp.nms_threshold = impl_->params.nms_threshold;
            detections = std::move(s.runConcepts({cp}).front());
        }

        // ---- 3. associate detections with tracks by mask IoU ----
        // Both active and pending tracks take part, so a newly detected object
        // is not re-created as a fresh instance on every frame of its
        // confirmation window.
        std::vector<Masklet*> all;
        for (Masklet& ml : impl_->active) all.push_back(&ml);
        for (Masklet& ml : impl_->pending) all.push_back(&ml);

        std::vector<bool> det_used(detections.detections.size(), false);
        for (Masklet* ml : all) {
            auto mit = masks.find(ml->id);
            if (mit == masks.end() || mit->second.data.empty()) continue;
            int best = -1;
            float best_iou = impl_->params.assoc_iou_threshold;
            for (size_t j = 0; j < detections.detections.size(); ++j) {
                if (det_used[j]) continue;
                const Mask& dm = detections.detections[j].mask;
                if (dm.width != mit->second.width || dm.height != mit->second.height)
                    continue;
                const float iou = mask_iou(mit->second.data.data(), dm.data.data(),
                                           mit->second.data.size());
                if (iou > best_iou) {
                    best_iou = iou;
                    best = (int)j;
                }
            }
            if (best >= 0) {
                det_used[(size_t)best] = true;
                ml->last_seen = fi;
                if (!ml->confirmed) ++ml->mds;
            }
        }

        // ---- 4. unmatched detections become pending masklets ----
        for (size_t j = 0; j < detections.detections.size(); ++j) {
            if (det_used[j]) continue;
            const Detection& d = detections.detections[j];
            if (d.mask.data.empty()) continue;

            Masklet ml;
            ml.id = impl_->next_id++;
            ml.first_frame = fi;
            ml.last_seen = fi;
            ml.last_score = d.score;
            ml.mds = 1;

            // Seed the memory from the detector's mask so the next frame has
            // something to propagate. Without this a pending masklet is inert.
            {
                vk::ArenaScope scope(s.arena);
                const int64_t S = (int64_t)s.model.grid() * 4;
                // The detector hands back a binary mask; the memory encoder
                // wants logits. Map 0/255 to -/+5 (the reference's constant --
                // sigmoid(5) = 0.993, close enough to a real prediction) and
                // sample it down to the decoder's resolution on the host: the
                // nearest-sample loop is what the reference does, and it avoids
                // pushing a full-resolution float mask across PCIe.
                std::vector<float> logits_host((size_t)(S * S));
                for (int64_t y = 0; y < S; ++y) {
                    const int64_t sy =
                        std::min<int64_t>(y * d.mask.height / S, d.mask.height - 1);
                    for (int64_t x = 0; x < S; ++x) {
                        const int64_t sx =
                            std::min<int64_t>(x * d.mask.width / S, d.mask.width - 1);
                        logits_host[(size_t)(y * S + x)] =
                            d.mask.data[(size_t)(sy * d.mask.width + sx)] > 127 ? 5.0f
                                                                                : -5.0f;
                    }
                }
                nn::Tensor logits = nn::arena_tensor(s.arena, nn::DType::F32, S, S);
                nn::tensor_from_host(logits, logits_host.data(), S * S);
                impl_->storeMemory(ml, logits, S, S, /*conditioning=*/true, 1.0f, fi);
            }
            // PCS has no SAM token, so the reference stores the learned
            // no-object pointer for the seeding frame.
            impl_->storeObjectPointer(ml, {}, -1.0f, fi);
            masks[ml.id] = d.mask;
            impl_->pending.push_back(std::move(ml));
        }

        // ---- 5. write this frame into every live memory bank ----
        auto commit = [&](Masklet& ml) {
            auto it = props.find(ml.id);
            if (it == props.end()) return;
            vk::ArenaScope scope(s.arena);
            const int64_t S = (int64_t)s.model.grid() * 4;
            nn::Tensor logits = nn::arena_tensor(s.arena, nn::DType::F32, S, S);
            nn::tensor_from_host(logits, it->second.mask_logits.data(), S * S);
            impl_->storeMemory(ml, logits, S, S, /*conditioning=*/false,
                               it->second.obj_logit, fi);
            impl_->storeObjectPointer(ml, it->second.sam_token, it->second.obj_logit, fi);
        };
        for (Masklet& ml : impl_->active) commit(ml);
        for (Masklet& ml : impl_->pending) commit(ml);

        impl_->updateMasklets();
        Result result = impl_->buildResult(masks);
        ++impl_->frame_index;
        vk::Stream::get().sync();
        s.error.clear();
        return result;
    } catch (const std::exception& e) {
        s.error = e.what();
        NN_LOG_ERROR("trackEncoded: %s\n", e.what());
        return {};
    }
}

int Tracker::addInstance(const VisualPrompt& prompt, Mask* mask_out) {
    Session::Impl& s = impl_->s;
    if (!s.loaded || !s.feats.valid()) {
        s.error = "addInstance: encode a frame first";
        return -1;
    }
    try {
        const int64_t S = (int64_t)s.model.grid() * 4;

        vk::ArenaScope scope(s.arena);
        model::SamDecoderOutputs dec;
        std::vector<float> iou, token;
        float obj = 0.0f;
        const int64_t slot = impl_->decodePrompt(prompt, dec, iou, obj, token);

        Masklet ml;
        ml.id = impl_->next_id++;
        // The frame index points at the NEXT frame to process, so the instance
        // belongs to the one just encoded.
        ml.first_frame = std::max(0, impl_->frame_index - 1);
        ml.last_seen = ml.first_frame;
        ml.last_score = iou[(size_t)slot];
        ml.confirmed = true;   // a user-provided instance skips the hotstart
        ml.mds = 1;

        impl_->storeMemory(ml, dec.masks.slice0(slot, 1).view(S, S), S, S,
                           /*conditioning=*/true, obj, ml.first_frame);
        impl_->storeObjectPointer(ml, token, obj, ml.first_frame);
        if (mask_out) {
            *mask_out = impl_->maskFromLogits(dec.masks.slice0(slot, 1));
            mask_out->instance_id = ml.id;
            mask_out->iou_score = ml.last_score;
        }
        impl_->active.push_back(std::move(ml));

        s.error.clear();
        NN_LOG_INFO("[track] added instance %d (iou %.3f)\n", impl_->next_id - 1,
                      iou[(size_t)slot]);
        return impl_->next_id - 1;
    } catch (const std::exception& e) {
        s.error = e.what();
        NN_LOG_ERROR("addInstance: %s\n", e.what());
        return -1;
    }
}

bool Tracker::refineInstance(int instance_id, const std::vector<Point>& pos_points,
                             const std::vector<Point>& neg_points, Mask* mask_out) {
    Session::Impl& s = impl_->s;
    Masklet* target = nullptr;
    for (Masklet& ml : impl_->active)
        if (ml.id == instance_id) target = &ml;
    if (!target)
        for (Masklet& ml : impl_->pending)
            if (ml.id == instance_id) target = &ml;
    if (!target) {
        s.error = "refineInstance: unknown instance id";
        return false;
    }
    if (!s.feats.valid()) {
        s.error = "refineInstance: encode a frame first";
        return false;
    }

    try {
        const int64_t S = (int64_t)s.model.grid() * 4;

        vk::ArenaScope scope(s.arena);
        VisualPrompt vp;
        vp.pos_points = pos_points;
        vp.neg_points = neg_points;
        model::SamDecoderOutputs dec;
        std::vector<float> iou, token;
        float obj = 0.0f;
        const int64_t slot = impl_->decodePrompt(vp, dec, iou, obj, token);

        target->last_score = iou[(size_t)slot];
        target->last_seen = std::max(0, impl_->frame_index - 1);
        // The refinement replaces the instance's conditioning memory, which is
        // what makes the correction stick on later frames.
        impl_->storeMemory(*target, dec.masks.slice0(slot, 1).view(S, S), S, S,
                           /*conditioning=*/true, obj, target->last_seen);
        impl_->storeObjectPointer(*target, token, obj, target->last_seen);
        if (mask_out) {
            *mask_out = impl_->maskFromLogits(dec.masks.slice0(slot, 1));
            mask_out->instance_id = target->id;
            mask_out->iou_score = target->last_score;
        }
        s.error.clear();
        return true;
    } catch (const std::exception& e) {
        s.error = e.what();
        NN_LOG_ERROR("refineInstance: %s\n", e.what());
        return false;
    }
}

}  // namespace sam
