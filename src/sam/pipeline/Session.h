#pragma once
// Session::Impl -- internal, shared with the Tracker.
//
// The Tracker drives the same model and arena as the Session it was built
// from, so it reaches in here rather than duplicating state. Both live in
// src/pipeline; nothing outside it includes this header.

#include "sam/Common.h"
#include "sam/model/Modules.h"
#include "sam/model/SamModel.h"
#include "sam/Sam.h"
#include "nn/vk/Memory.h"

#include <string>
#include <vector>

namespace sam {

struct Session::Impl {
    SamModel            model;
    vk::Arena            arena{"activations"};
    model::ImageFeatures feats;
    bool                 loaded = false;
    std::string          error;
    // What loadModel last uploaded, so asking for the same thing again is free.
    // The mask preview re-initializes its Masker on every edit -- a click, a
    // word typed -- and re-reading a 700 MB checkpoint for each of those is a
    // three-second stall in a panel whose whole point is to be immediate.
    ModelParams          loaded_params;
    // The physical identity the loaded model sits on, canonical uuid:<hex>.
    // Session::loadModel compares this against the live device, so a second
    // request spelled differently but naming the same GPU reuses the weights.
    std::string          device_selector;

    // Text + exemplar tokens, the padding bias every prompt-facing attention
    // applies, and the pooling weights DotProductScoring needs. Returns the
    // token count. Arena-backed: valid until the caller's scope rewinds.
    int64_t buildPrompt(const ConceptPrompt& prompt, nn::Tensor& out_prompt,
                        nn::Tensor& out_bias, nn::Tensor& out_valid);

    // Point/box prompt tokens for the SAM decoder. Returns the token count.
    int64_t buildSparsePrompt(const VisualPrompt& prompt, nn::Tensor& out_sparse);

    // The text encoder is 24 transformer blocks over a fixed 32-token context
    // and depends on nothing but the prompt, yet the tracker calls buildPrompt
    // once per frame with the same words every time. The encoded rows live in
    // PoolSlot::TextFeat and are reused until the token ids change.
    std::vector<int32_t> text_cache_ids;
    bool                 text_cache_valid = false;

    Result runConcept(const ConceptPrompt& prompt);

    // Resize + threshold + read back one mask per surviving detection.
    void exportMasks(const nn::Tensor& masks, int64_t n, int64_t mask_size,
                     std::vector<Detection>& dets, const std::vector<int>& keep,
                     Result& result);

    Result collectSamMasks(const model::SamDecoderOutputs& dec, bool multimask,
                           int64_t mask_size);
};

}  // namespace sam
