// Session: the public entry points for single-image work.
//
// Ownership: the Session owns the model, the activation arena and the current
// frame's backbone features. Backbone output lives in VramPool slots so it
// survives the arena rewind between prompts -- encode once, prompt many times,
// which is the whole point of the split.

#include "sam/pipeline/Session.h"

#include "core/VulkanDeviceSelection.h"
#include "sam/Common.h"
#include "nn/Device.h"
#include "nn/core/Error.h"
#include "nn/core/Log.h"
#include "sam/pipeline/PostProcess.h"
#include "nn/vk/EmbeddedSpirv.h"
#include "nn/vk/Pipelines.h"
#include "nn/vk/Stream.h"

#include <algorithm>
#include <cmath>
#include <cstring>

NN_DECLARE_EMBEDDED_MODULES(sam)

namespace sam {

namespace {

float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

// The arena is sized for the peak of the image encoder, which is the deepest
// stage: the neck's 4x transposed convolution holds a 288x288x256 output, its
// GEMM staging buffer and the im2col columns at once. Measured against the
// released SAM 3 checkpoint at its native 1008x1008, concept segmentation and
// video tracking both peak at 317 MiB (`spirula-sam --vram` prints it), so
// this is roughly 1.6x headroom. Overflowing it is a descriptive error, not a
// corruption: nn::Arena refuses to grow while pointers are live.
constexpr uint64_t kArenaReserve = 512ull << 20;

// Hiera's peak is driven by its widest block, which holds several copies of the
// stride-4 token map at once: measured 301 MiB for Hiera-T and 426 MiB for
// Hiera-L at 1024x1024, i.e. ~45 bytes per (stride-4 token x embed dim). The
// arena refuses to grow once a forward pass is under way, so this has to be
// right up front rather than discovered.
uint64_t arena_reserve_for(const SamModel& m) {
    if (m.family() != Family::Sam2) return kArenaReserve;
    const uint64_t tokens = (uint64_t)(m.imgSize() / 4) * (uint64_t)(m.imgSize() / 4);
    const uint64_t need = tokens * (uint64_t)m.hp().hiera_embed_dim * 64;
    return std::max(kArenaReserve, need);
}

// Whether a load request would produce the model already on the device. The
// device is not compared here: it is an identity, checked by loadModel against
// the live context. `validation` is a Context property, not a model one.
bool same_model(const ModelParams& a, const ModelParams& b) {
    return a.model_path == b.model_path && a.img_size == b.img_size;
}

}  // namespace

// ================
// Session
// ================

Session::Session() : impl_(new Impl()) {}
Session::~Session() { unload(); }

void Session::unload() {
    const bool was_loaded = impl_->loaded;
    impl_->loaded = false;
    impl_->device_selector.clear();
    impl_->text_cache_valid = false;
    impl_->text_cache_ids.clear();
    // Nothing to give back, or the device is already gone (nn::shutdown() ran
    // first, which frees all of this wholesale).
    if (!was_loaded || !vk::Context::initialized()) return;
    // The pool is grow-only and process-wide, so nothing here is freed by the
    // members going out of scope: a destroyed Session would otherwise leave
    // its ~2 GB of weights resident until the process exits, which is exactly
    // when an interactive application wants them gone.
    //
    // Everything a model or a frame owns, which is every slot except the
    // memory bank (a Tracker's, released with it) and the Stream's own staging
    // and parameter rings.
    static constexpr vk::PoolSlot kOwned[] = {
        vk::PoolSlot::Weights,     vk::PoolSlot::RopeTable,
        vk::PoolSlot::PosEncTable, vk::PoolSlot::PromptEncCache,
        vk::PoolSlot::BackboneFeat, vk::PoolSlot::NeckDet,
        vk::PoolSlot::NeckTrk,     vk::PoolSlot::NeckPe,
        vk::PoolSlot::TextFeat,    vk::PoolSlot::PromptFeat,
        vk::PoolSlot::FusionFeat,  vk::PoolSlot::MaskLogits,
    };
    try {
        vk::Stream::get().sync();
        impl_->feats = model::ImageFeatures{};
        impl_->model = SamModel{};
        for (vk::PoolSlot s : kOwned) vk::VramPool::get().releaseSlot(s);
        impl_->arena.reset();
        impl_->arena.release();
    } catch (const std::exception& e) {
        NN_LOG_ERROR("unload: %s\n", e.what());
    }
}

bool Session::isLoaded() const { return impl_->loaded; }
bool Session::supportsTextPrompts() const {
    return impl_->loaded && impl_->model.canTextPrompt();
}
bool Session::imageEncoded() const { return impl_->feats.valid(); }
const std::string& Session::lastError() const { return impl_->error; }

bool freeze_device(const std::string& device, std::string& error, bool validation,
                   bool profile) {
    try {
        // Empty defaults inherit the live or configured identity.
        if (device.empty() && !nn::current_device_selector().empty()) {
            error.clear();
            return true;
        }
        const std::string recorded = nn::configured_device_selector();
        const std::string request = device.empty() ? recorded : device;
        const spirula::vkselect::Request req =
            spirula::vkselect::requestFrom(request, !request.empty());
        // Registers the resolved physical identity without creating a logical
        // device; a malformed, missing, ambiguous or unusable value throws the
        // resolver's own sentence here, before anything is allocated.
        nn::configure_device(req.text, validation, profile);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        NN_LOG_ERROR("device selection: %s\n", e.what());
        return false;
    }
}

bool Session::loadModel(const ModelParams& params) {
    NN_ENSURE_EMBEDDED_MODULES(sam);
    // Validate the request before reuse; loaded weights cannot satisfy another GPU.
    if (!freeze_device(params.device, impl_->error, params.validation,
                       params.profile))
        return false;
    // Reuse only when the loaded model is the one asked for and the live device
    // is the same canonical UUID.
    if (impl_->loaded && same_model(impl_->loaded_params, params) &&
        impl_->device_selector == nn::current_device_selector()) {
        impl_->error.clear();
        return true;
    }
    try {
        impl_->model.load(params.model_path, params.img_size);
        impl_->arena.reserve(arena_reserve_for(impl_->model));
        impl_->text_cache_valid = false;
        impl_->loaded = true;
        impl_->loaded_params = params;
        // What the weights actually landed on -- the live context is the only
        // thing that can say, and it is what a later reuse compares against.
        impl_->device_selector = nn::current_device_selector();
        impl_->error.clear();
        return true;
    } catch (const std::exception& e) {
        impl_->error = e.what();
        NN_LOG_ERROR("loadModel: %s\n", e.what());
        return false;
    }
}

bool Session::encodeImage(const Image& image) {
    if (!impl_->loaded) {
        impl_->error = "no model loaded";
        return false;
    }
    if (image.empty() || image.channels != 3) {
        impl_->error = "encodeImage: expected a non-empty 3-channel RGB image";
        return false;
    }
    try {
        impl_->arena.reset();
        vk::ArenaScope scope(impl_->arena);

        const int S = impl_->model.imgSize();
        auto host = model::preprocess_image(impl_->model, image.data.data(), image.width,
                                            image.height);
        nn::Tensor img = nn::arena_tensor(impl_->arena, nn::DType::F32, S, S, 3);
        nn::tensor_from_host(img, host.data(), (int64_t)host.size());

        model::encode_image(impl_->model, impl_->arena, img, impl_->feats);
        impl_->feats.orig_width = image.width;
        impl_->feats.orig_height = image.height;
        vk::Stream::get().sync();
        impl_->error.clear();
        return true;
    } catch (const std::exception& e) {
        impl_->error = e.what();
        NN_LOG_ERROR("encodeImage: %s\n", e.what());
        impl_->feats = model::ImageFeatures{};
        return false;
    }
}

// ---------------------------------------------------------------------------
// Prompt assembly shared by the concept path and the tracker
// ---------------------------------------------------------------------------

int64_t Session::Impl::buildPrompt(const ConceptPrompt& prompt, nn::Tensor& out_prompt,
                                   nn::Tensor& out_bias, nn::Tensor& out_valid) {
    const Hparams& h = model.hp();
    const int64_t D = h.neck_dim;
    const int64_t L = h.text_ctx_len;

    std::vector<int32_t> ids;
    if (!prompt.text.empty()) {
        NN_CHECK(model.canTextPrompt(),
                   "this checkpoint has no text encoder, so a text prompt cannot be "
                   "used (it is a visual-only export)");
        ids = model.tokenizer().encode(prompt.text, (int)L);
    } else {
        // No text: the reference still runs the encoder over an empty prompt
        // ([SOT, EOT] padded), which is not the same as skipping it -- the
        // fusion encoder and the scorer both expect the tokens to be there.
        ids.assign((size_t)L, 0);
        if (model.canTextPrompt()) {
            ids[0] = model.tokenizer().sotToken();
            ids[1] = model.tokenizer().eotToken();
        }
    }

    std::vector<model::Exemplar> exemplars;
    auto add_boxes = [&](const std::vector<Box>& boxes, int label) {
        for (const Box& b : boxes) {
            model::Exemplar e;
            e.cx = (b.x0 + b.x1) * 0.5f / (float)feats.orig_width;
            e.cy = (b.y0 + b.y1) * 0.5f / (float)feats.orig_height;
            e.w = (b.x1 - b.x0) / (float)feats.orig_width;
            e.h = (b.y1 - b.y0) / (float)feats.orig_height;
            e.label = label;
            exemplars.push_back(e);
        }
    };
    add_boxes(prompt.pos_exemplars, 0);
    add_boxes(prompt.neg_exemplars, 1);

    const int64_t n_geo = (int64_t)exemplars.size() + 1;  // + CLS
    const int64_t T = L + n_geo;

    out_prompt = nn::arena_tensor(arena, nn::DType::F32, T, D);
    {
        const nn::Tensor rows = out_prompt.slice0(0, L);
        const uint64_t bytes = (uint64_t)L * D * sizeof(float);
        if (text_cache_valid && text_cache_ids == ids) {
            vk::Stream::get().copy(rows.ptr,
                                   vk::VramPool::get().lookup(vk::PoolSlot::TextFeat, 0),
                                   bytes);
        } else {
            model::encode_text(model, arena, ids, rows);
            nn::Tensor cached =
                nn::pool_tensor(vk::PoolSlot::TextFeat, 0, nn::DType::F32, L, D);
            vk::Stream::get().copy(cached.ptr, rows.ptr, bytes);
            text_cache_ids = ids;
            text_cache_valid = true;
        }
    }

    const int G = model.grid();
    model::encode_geometry(model, arena, exemplars, feats.det[2].view((int64_t)G * G, D),
                           model.neckPe().view((int64_t)G * G, D),
                           out_prompt.slice0(L, n_geo));

    // Padding tokens are masked out of every attention that reads the prompt,
    // and excluded from the mean the scorer pools over.
    std::vector<float> bias((size_t)T, 0.0f);
    std::vector<float> valid((size_t)T, 0.0f);
    int n_valid = 0;
    for (int64_t i = 0; i < L; ++i) {
        const bool ok = ids[(size_t)i] != 0;
        bias[(size_t)i] = ok ? 0.0f : -1e9f;
        if (ok) ++n_valid;
    }
    n_valid += (int)n_geo;
    const float inv = 1.0f / (float)std::max(n_valid, 1);
    for (int64_t i = 0; i < L; ++i) valid[(size_t)i] = (ids[(size_t)i] != 0) ? inv : 0.0f;
    for (int64_t i = L; i < T; ++i) valid[(size_t)i] = inv;

    out_bias = nn::arena_tensor(arena, nn::DType::F32, T);
    out_valid = nn::arena_tensor(arena, nn::DType::F32, T);
    nn::tensor_from_host(out_bias, bias.data(), T);
    nn::tensor_from_host(out_valid, valid.data(), T);
    return T;
}

// ---------------------------------------------------------------------------
// Promptable concept segmentation
// ---------------------------------------------------------------------------

Result Session::Impl::runConcept(const ConceptPrompt& prompt) {
    const Hparams& h = model.hp();
    const int64_t D = h.neck_dim;
    const int G = model.grid();
    const int64_t N = (int64_t)G * G;
    const int64_t S = (int64_t)G * 4;
    Result result;

    vk::ArenaScope scope(arena);

    nn::Tensor prompt_t, bias_t, valid_t;
    const int64_t T = buildPrompt(prompt, prompt_t, bias_t, valid_t);

    // The fusion encoder rewrites the image features in place, so it gets its
    // own copy: the pool copy must stay pristine for the next prompt.
    nn::Tensor fused = nn::arena_tensor(arena, nn::DType::F32, N, D);
    nn::strided_copy(fused, feats.det[2].view(N, D), N, D, D, D);
    nn::Tensor pe = model.neckPe().view(N, D);
    model::fusion_encoder(model, arena, fused, pe, prompt_t, T, bias_t);

    model::DetrOutputs det;
    model::detr_decoder(model, arena, fused, pe, prompt_t, T, bias_t, valid_t, det);

    // ---- score, threshold, NMS on the host ----
    const int64_t NQ = h.ddec_num_queries;
    std::vector<float> scores((size_t)NQ), boxes((size_t)NQ * 4), presence(1);
    nn::tensor_to_host(det.class_scores, scores.data(), NQ);
    nn::tensor_to_host(det.boxes, boxes.data(), NQ * 4);
    nn::tensor_to_host(det.presence, presence.data(), 1);
    const float presence_prob = sigmoid(presence[0]);

    struct Candidate { int query; float score; Box box; };
    std::vector<Candidate> cands;
    for (int64_t q = 0; q < NQ; ++q) {
        // The reference clamps the logit to [-12, 12] before the sigmoid.
        const float logit = std::max(-12.0f, std::min(12.0f, scores[(size_t)q]));
        const float score = sigmoid(logit) * presence_prob;
        if (score < prompt.score_threshold) continue;
        cands.push_back({(int)q, score,
                         cxcywh_to_xyxy(boxes[(size_t)(q * 4 + 0)], boxes[(size_t)(q * 4 + 1)],
                                        boxes[(size_t)(q * 4 + 2)], boxes[(size_t)(q * 4 + 3)],
                                        feats.orig_width, feats.orig_height)});
    }
    NN_LOG_INFO("[pcs] %zu queries over threshold %.2f (presence %.3f)\n", cands.size(),
                  prompt.score_threshold, presence_prob);

    std::vector<Detection> dets;
    dets.reserve(cands.size());
    for (const Candidate& c : cands) {
        Detection d;
        d.box = c.box;
        d.score = c.score;
        d.iou_score = c.score;
        dets.push_back(std::move(d));
    }
    std::vector<int> keep = nms(dets, prompt.nms_threshold);
    if (prompt.max_detections > 0 && (int)keep.size() > prompt.max_detections) {
        // `nms` returns highest-score-first, so truncating keeps the best.
        keep.resize((size_t)prompt.max_detections);
    }
    if (keep.empty()) return result;

    // ---- masks for the survivors only ----
    // The segmentation head is the expensive part of this path; decoding all
    // 200 queries when three survive would triple the cost of a typical prompt.
    std::vector<int32_t> query_ids;
    query_ids.reserve(keep.size());
    for (int k : keep) query_ids.push_back((int32_t)cands[(size_t)k].query);

    const int64_t n_sel = (int64_t)query_ids.size();
    nn::Tensor ids = nn::arena_tensor(arena, nn::DType::I32, n_sel);
    vk::Stream::get().upload(ids.ptr, query_ids.data(), query_ids.size() * 4);
    nn::Tensor sel = nn::arena_tensor(arena, nn::DType::F32, n_sel, D);
    nn::gather_rows(sel, det.queries, ids);

    nn::Tensor masks = nn::arena_tensor(arena, nn::DType::F32, n_sel, S * S);
    model::segmentation_head(model, arena, fused, feats, sel, n_sel, prompt_t, T, bias_t,
                             masks);

    exportMasks(masks, n_sel, S, dets, keep, result);
    return result;
}

// Resize each mask row to the original resolution, threshold at 0 and pull back
// the bytes. Both steps run on device (resample.slang::resize_binarize) so a
// 200-instance frame moves 200 x W x H BYTES instead of floats.
void Session::Impl::exportMasks(const nn::Tensor& masks, int64_t n, int64_t mask_size,
                                std::vector<Detection>& dets,
                                const std::vector<int>& keep, Result& result) {
    const int W = feats.orig_width, H = feats.orig_height;
    const int64_t bytes = (int64_t)W * H;

    vk::ArenaScope scope(arena);
    nn::Tensor out_u8 = nn::arena_tensor(arena, nn::DType::U8, (bytes + 3) / 4 * 4);
    for (int64_t i = 0; i < n; ++i) {
        nn::resize_binarize(out_u8, masks.slice0(i, 1).view(mask_size, mask_size), H, W,
                            0.0f);
        Detection& d = dets[(size_t)keep[(size_t)i]];
        d.mask.width = W;
        d.mask.height = H;
        d.mask.data.resize((size_t)bytes);
        d.mask.iou_score = d.score;
        d.mask.instance_id = (int)i + 1;
        d.instance_id = (int)i + 1;
        vk::Stream::get().download(d.mask.data.data(), out_u8.ptr, (uint64_t)bytes);
        result.detections.push_back(std::move(d));
    }
}

Result Session::segmentConcept(const ConceptPrompt& prompt) {
    if (!impl_->loaded || !impl_->feats.valid()) {
        impl_->error = "segmentConcept: call encodeImage() first";
        return {};
    }
    if (impl_->model.visualOnly()) {
        impl_->error = "segmentConcept: this checkpoint is a visual-only export and has "
                       "no detector; use segmentVisual() or the visual tracker";
        return {};
    }
    try {
        NN_SCOPED_TIMER("segmentConcept");
        Result r = impl_->runConcept(prompt);
        impl_->error.clear();
        return r;
    } catch (const std::exception& e) {
        impl_->error = e.what();
        NN_LOG_ERROR("segmentConcept: %s\n", e.what());
        return {};
    }
}

// ---------------------------------------------------------------------------
// Promptable visual segmentation
// ---------------------------------------------------------------------------

int64_t Session::Impl::buildSparsePrompt(const VisualPrompt& prompt,
                                         nn::Tensor& out_sparse) {
    const int64_t D = model.hp().sam_embed_dim;
    const int num_pos_feats = (int)(D / 2);
    const int img = model.imgSize();

    // The reference's ordering: box corners first (labels 2 and 3), then the
    // user's positive and negative clicks, then one padding token. The padding
    // token stays even when a box is present, because the box is merged into
    // the point stream rather than passed separately.
    struct Tok { float x, y; int label; };
    std::vector<Tok> toks;
    if (prompt.use_box) {
        toks.push_back({prompt.box.x0, prompt.box.y0, 2});
        toks.push_back({prompt.box.x1, prompt.box.y1, 3});
    }
    for (const Point& p : prompt.pos_points) toks.push_back({p.x, p.y, 1});
    for (const Point& p : prompt.neg_points) toks.push_back({p.x, p.y, 0});
    toks.push_back({0.0f, 0.0f, -1});

    const int64_t n = (int64_t)toks.size();
    std::vector<float> host((size_t)(n * D));
    std::vector<float> enc((size_t)D);
    for (int64_t i = 0; i < n; ++i) {
        float* dst = host.data() + (size_t)(i * D);
        if (toks[(size_t)i].label < 0) {
            std::memcpy(dst, model.notAPointEmbed(), (size_t)D * sizeof(float));
            continue;
        }
        // Original pixels -> model input pixels -> pixel centers -> [0, 1].
        const float px = toks[(size_t)i].x / (float)feats.orig_width * (float)img + 0.5f;
        const float py = toks[(size_t)i].y / (float)feats.orig_height * (float)img + 0.5f;
        pe_encode_coord(enc.data(), px / (float)img, py / (float)img, model.peGaussian(),
                        num_pos_feats);
        const float* pe = model.pointEmbed(toks[(size_t)i].label);
        for (int64_t d = 0; d < D; ++d) dst[d] = enc[(size_t)d] + pe[d];
    }

    out_sparse = nn::arena_tensor(arena, nn::DType::F32, n, D);
    nn::tensor_from_host(out_sparse, host.data(), (int64_t)host.size());
    return n;
}

Result Session::segmentVisual(const VisualPrompt& prompt) {
    if (!impl_->loaded || !impl_->feats.valid()) {
        impl_->error = "segmentVisual: call encodeImage() first";
        return {};
    }
    if (prompt.pos_points.empty() && prompt.neg_points.empty() && !prompt.use_box) {
        impl_->error = "segmentVisual: give at least one point or a box";
        return {};
    }
    try {
        NN_SCOPED_TIMER("segmentVisual");
        const Hparams& h = impl_->model.hp();
        const int64_t D = h.sam_embed_dim;
        const int G = impl_->model.grid();
        const int64_t N = (int64_t)G * G;
        const int64_t S = (int64_t)G * 4;

        vk::ArenaScope scope(impl_->arena);

        nn::Tensor sparse;
        const int64_t n_sparse = impl_->buildSparsePrompt(prompt, sparse);

        // Single-image PVS has no memory, so the features enter with the
        // learned "no memory" embedding added (directly_add_no_mem_embed).
        nn::Tensor embed = nn::arena_tensor(impl_->arena, nn::DType::F32, N, D);
        nn::add(embed, impl_->feats.trk[2].view(N, D),
                impl_->model.w().get("no_mem_embed"));

        model::SamDecoderOutputs dec;
        model::sam_mask_decoder(impl_->model, impl_->arena, embed, sparse, n_sparse,
                                impl_->feats.trk[0], impl_->feats.trk[1], dec);

        Result r = impl_->collectSamMasks(dec, prompt.multimask, S);
        impl_->error.clear();
        return r;
    } catch (const std::exception& e) {
        impl_->error = e.what();
        NN_LOG_ERROR("segmentVisual: %s\n", e.what());
        return {};
    }
}

Result Session::Impl::collectSamMasks(const model::SamDecoderOutputs& dec, bool multimask,
                                      int64_t mask_size) {
    const Hparams& h = model.hp();
    const int64_t NM = h.num_mask_tokens();
    const int64_t D = h.sam_embed_dim;
    const int W = feats.orig_width, H = feats.orig_height;
    Result result;

    std::vector<float> iou((size_t)NM), obj(1), tokens((size_t)(NM * D));
    nn::tensor_to_host(dec.iou, iou.data(), NM);
    nn::tensor_to_host(dec.obj_score, obj.data(), 1);
    nn::tensor_to_host(dec.mask_tokens, tokens.data(), NM * D);
    const float obj_score = sigmoid(obj[0]);

    // Slot 0 is the single-mask output; slots 1..3 are the ambiguity-resolving
    // ones, which is what multimask returns.
    const int64_t begin = multimask ? 1 : 0;
    const int64_t end = multimask ? NM : 1;

    vk::ArenaScope scope(arena);
    const int64_t bytes = (int64_t)W * H;
    nn::Tensor out_u8 = nn::arena_tensor(arena, nn::DType::U8, (bytes + 3) / 4 * 4);

    for (int64_t k = begin; k < end; ++k) {
        nn::resize_binarize(out_u8, dec.masks.slice0(k, 1).view(mask_size, mask_size), H, W,
                            0.0f);
        Detection d;
        d.mask.width = W;
        d.mask.height = H;
        d.mask.data.resize((size_t)bytes);
        vk::Stream::get().download(d.mask.data.data(), out_u8.ptr, (uint64_t)bytes);
        d.mask.iou_score = iou[(size_t)k];
        d.mask.obj_score = obj_score;
        d.mask.instance_id = (int)k;
        d.score = iou[(size_t)k];
        d.iou_score = iou[(size_t)k];
        d.instance_id = (int)k;
        d.sam_token.assign(tokens.begin() + (size_t)(k * D),
                           tokens.begin() + (size_t)((k + 1) * D));
        mask_bounding_box(d.mask, d.box);
        result.detections.push_back(std::move(d));
    }
    return result;
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------

std::string Session::vramReport() const {
    std::string s;
    char line[256];
    uint64_t total = 0;
    for (const auto& e : vk::VramPool::get().breakdown()) {
        std::snprintf(line, sizeof line, "  %-18s %-12s %8.1f MiB\n", e.name.c_str(),
                      vk::vram_category_name(e.category), e.capacity / 1048576.0);
        s += line;
        total += e.capacity;
    }
    // The high-water mark is what the reserve should be tuned against; it is
    // the one number here that is not simply a capacity we chose.
    std::snprintf(line, sizeof line, "  %-18s %-12s %8.1f MiB (peak %.1f MiB)\n", "arena",
                  "workspace", impl_->arena.capacity() / 1048576.0,
                  impl_->arena.highWater() / 1048576.0);
    s += line;
    total += impl_->arena.capacity();
    std::snprintf(line, sizeof line, "  %-18s %-12s %8.1f MiB\n", "TOTAL", "",
                  total / 1048576.0);
    s += line;
    return s;
}

void Session::printProfile() { vk::Stream::get().report(); }

}  // namespace sam
