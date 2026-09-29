#pragma once
// Grounding DINO (Liu et al., 2023) on the inference layer: an image and some
// noun phrases in, boxes out -- the detector half of lang-segment-anything,
// whose other half is SAM 2 (src/sam/). src/gdino/README.md.

#include "nn/io/Fetch.h"
#include "nn/io/Image.h"

#include <memory>
#include <string>
#include <vector>

namespace gdino {

struct ModelSource {
    const char*   id;
    nn::FetchFile weights;
    nn::FetchFile vocab;
};

const ModelSource* find_model_source(const std::string& id);
std::string model_id_list();

// The checkpoint and tokenizer vocabulary an id or a path stands for, fetched
// when missing. A path takes the vocab.txt beside it, else bert-base-uncased's.
// Throws nn::Error.
struct Resolved {
    std::string weights, vocab;
};
Resolved resolve_model(const std::string& id_or_path);

// Whether `path` is a Grounding DINO checkpoint, by its tensor names.
bool is_checkpoint(const std::string& path);

struct Detection {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;   // pixels of the image handed in
    float score = 0;                         // max over the prompt's tokens
    int   phrase = -1;                       // index into the phrases asked for
};

struct DetectOptions {
    // lang-segment-anything's default: a box is kept when one prompt token's
    // probability exceeds this. Its phrase is the one holding the best token.
    float box_threshold = 0.3f;
    // Hugging Face's resize: the short side to this, the long side capped.
    int short_side = 800;
    int long_side_cap = 1333;
};

class Detector {
public:
    Detector();
    ~Detector();
    Detector(const Detector&) = delete;
    Detector& operator=(const Detector&) = delete;

    // An id from model_id_list() or a path. Throws nn::Error.
    void load(const std::string& id_or_path);
    bool loaded() const;
    void unload();
    uint64_t deviceBytes() const;

    // Every phrase in one pass, joined the way the model was trained on
    // ("cat . dog ."), each box labelled with the phrase it matched.
    std::vector<Detection> detect(const nn::Image& image, const std::vector<std::string>& phrases,
                                  const DetectOptions& opts = {});

    // The raw head on an already-normalized [H, W, 3] input: logits [900, T]
    // and cxcywh boxes [900, 4], for parity checks. `ids` is the token sequence.
    void forwardNormalized(const std::vector<float>& hwc, int H, int W,
                           const std::vector<int32_t>& ids, std::vector<float>& logits,
                           std::vector<float>& boxes);
    std::vector<int32_t> tokenize(const std::string& text) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace gdino
