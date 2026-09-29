#include "gdino/GroundingDino.h"
#include "gdino/Model.h"

#include "nn/core/Error.h"
#include "nn/io/Safetensors.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

namespace gdino {
namespace {

// SHA-256 is the LFS object id each file is published under; ModelScope's
// IDEA-Research mirror carries the same bytes. Both sizes share one tokenizer.
#define GDINO_VOCAB                                                                    \
    {"bert-base-uncased-vocab.txt",                                                    \
     "https://huggingface.co/IDEA-Research/grounding-dino-tiny/resolve/main/vocab.txt", \
     "07eced375cec144d27c900241f3e339478dec958f92fddbc551f295c992038a3", 231508ull,     \
     "https://modelscope.cn/models/IDEA-Research/grounding-dino-tiny/resolve/master/vocab.txt"}

const ModelSource kSources[] = {
    {"gdino-tiny",
     {"grounding-dino-tiny.safetensors",
      "https://huggingface.co/IDEA-Research/grounding-dino-tiny/resolve/main/model.safetensors",
      "1a2412ef99bd74bcd3c2a246fa1e48581f8889a1300c9051974741314fc042f3", 689359096ull,
      "https://modelscope.cn/models/IDEA-Research/grounding-dino-tiny/resolve/master/"
      "model.safetensors"},
     GDINO_VOCAB},
    {"gdino-base",
     {"grounding-dino-base.safetensors",
      "https://huggingface.co/IDEA-Research/grounding-dino-base/resolve/main/model.safetensors",
      "5548f844c928c4b6f411fa8cbcc2bfa8dbbba437cb1d513975519f93c2a9ed21", 933400872ull,
      "https://modelscope.cn/models/IDEA-Research/grounding-dino-base/resolve/master/"
      "model.safetensors"},
     GDINO_VOCAB},
};

#undef GDINO_VOCAB

// GroundingDinoImageProcessor's get_size_with_aspect_ratio, rounding included.
void target_size(int h, int w, int size, int max_size, int& oh, int& ow) {
    double raw = -1.0;
    const double mn = std::min(h, w), mx = std::max(h, w);
    if (max_size > 0 && mx / mn * size > max_size) {
        raw = max_size * mn / mx;
        size = (int)std::lround(raw);
    }
    if ((h <= w && h == size) || (w <= h && w == size)) {
        oh = h;
        ow = w;
    } else if (w < h) {
        ow = size;
        oh = (int)(raw > 0 ? raw * h / w : (double)size * h / w);
    } else {
        oh = size;
        ow = (int)(raw > 0 ? raw * w / h : (double)size * w / h);
    }
}

}  // namespace

const ModelSource* find_model_source(const std::string& id) {
    for (const ModelSource& s : kSources)
        if (id == s.id) return &s;
    return nullptr;
}

std::string model_id_list() {
    std::string s;
    for (const ModelSource& m : kSources) s += (s.empty() ? "" : ", ") + std::string(m.id);
    return s;
}

Resolved resolve_model(const std::string& id_or_path) {
    Resolved r;
    if (const ModelSource* src = find_model_source(id_or_path)) {
        r.weights = nn::ensure_file(src->weights, "gdino");
        r.vocab = nn::ensure_file(src->vocab, "gdino");
        return r;
    }
    std::error_code ec;
    NN_CHECK(fs::exists(id_or_path, ec),
             "'%s' is neither a known Grounding DINO id (%s) nor a file that exists",
             id_or_path.c_str(), model_id_list().c_str());
    r.weights = id_or_path;
    const fs::path beside = fs::path(id_or_path).parent_path() / "vocab.txt";
    r.vocab = fs::exists(beside, ec) ? beside.string()
                                     : nn::ensure_file(kSources[0].vocab, "gdino");
    return r;
}

bool is_checkpoint(const std::string& path) {
    try {
        const nn::SafetensorsFile f(path);
        return f.has("model.text_projection.weight") && f.has("model.query_position_embeddings.weight");
    } catch (const std::exception&) {
        return false;
    }
}

struct Detector::Impl {
    Model m;
};

Detector::Detector() : impl_(new Impl) {}
Detector::~Detector() = default;

void Detector::load(const std::string& id_or_path) {
    const Resolved r = resolve_model(id_or_path);
    impl_->m.load(r.weights, r.vocab);
}

bool Detector::loaded() const { return impl_->m.loaded(); }
void Detector::unload() { impl_->m.release(); }
uint64_t Detector::deviceBytes() const { return impl_->m.deviceBytes(); }

std::vector<int32_t> Detector::tokenize(const std::string& text) const {
    return impl_->m.tokenizer().encode(text);
}

void Detector::forwardNormalized(const std::vector<float>& hwc, int H, int W,
                                 const std::vector<int32_t>& ids, std::vector<float>& logits,
                                 std::vector<float>& boxes) {
    HeadOutput o = impl_->m.run(hwc, H, W, ids);
    logits = std::move(o.logits);
    boxes = std::move(o.boxes);
}

std::vector<Detection> Detector::detect(const nn::Image& image,
                                        const std::vector<std::string>& phrases,
                                        const DetectOptions& opts) {
    NN_CHECK(loaded(), "gdino: no model loaded");
    std::vector<Detection> dets;
    if (phrases.empty() || image.empty()) return dets;

    // "a . b . c .", built phrase by phrase so each one's token span is known.
    // A '.' or '?' inside a phrase would read as a separator, so it goes.
    const Tokenizer& tok = impl_->m.tokenizer();
    std::vector<int32_t> ids{Tokenizer::kCls};
    std::vector<std::pair<int, int>> spans;
    for (const std::string& raw : phrases) {
        std::string p = raw;
        for (char& c : p)
            if (c == '.' || c == '?') c = ' ';
        std::vector<int32_t> t = tok.encode(p);
        if (t.size() <= 2) continue;
        if (ids.size() + t.size() > 255) break;
        spans.emplace_back((int)ids.size(), (int)(ids.size() + t.size() - 2));
        ids.insert(ids.end(), t.begin() + 1, t.end() - 1);
        ids.push_back(Tokenizer::kPeriod);
    }
    ids.push_back(Tokenizer::kSep);
    if (spans.empty()) return dets;
    std::vector<int> phrase_of(spans.size());
    {
        size_t k = 0;
        for (size_t i = 0; i < phrases.size() && k < spans.size(); ++i) {
            std::string p = phrases[i];
            for (char& c : p)
                if (c == '.' || c == '?') c = ' ';
            if (tok.encode(p).size() > 2) phrase_of[k++] = (int)i;
        }
    }

    int oh = 0, ow = 0;
    target_size(image.height, image.width, opts.short_side, opts.long_side_cap, oh, ow);
    const nn::Image resized = nn::resize_image(image, ow, oh);
    static const float kMean[3] = {0.485f, 0.456f, 0.406f}, kStd[3] = {0.229f, 0.224f, 0.225f};
    std::vector<float> hwc((size_t)oh * ow * 3);
    for (size_t i = 0; i < (size_t)oh * ow; ++i)
        for (int c = 0; c < 3; ++c)
            hwc[i * 3 + c] = (resized.data[i * 3 + c] / 255.0f - kMean[c]) / kStd[c];

    const HeadOutput o = impl_->m.run(hwc, oh, ow, ids);
    const int T = o.tokens;
    for (int q = 0; q < o.queries; ++q) {
        const float* lg = &o.logits[(size_t)q * T];
        float score = 0.0f;
        for (int t = 0; t < T; ++t) score = std::max(score, 1.0f / (1.0f + std::exp(-lg[t])));
        if (score <= opts.box_threshold) continue;
        int best = 0;
        float best_p = -1.0f;
        for (size_t k = 0; k < spans.size(); ++k)
            for (int t = spans[k].first; t < spans[k].second; ++t) {
                const float p = 1.0f / (1.0f + std::exp(-lg[t]));
                if (p > best_p) { best_p = p; best = (int)k; }
            }
        const float* b = &o.boxes[(size_t)q * 4];
        Detection d;
        d.x0 = std::max(0.0f, (b[0] - 0.5f * b[2]) * image.width);
        d.y0 = std::max(0.0f, (b[1] - 0.5f * b[3]) * image.height);
        d.x1 = std::min((float)image.width, (b[0] + 0.5f * b[2]) * image.width);
        d.y1 = std::min((float)image.height, (b[1] + 0.5f * b[3]) * image.height);
        d.score = score;
        d.phrase = phrase_of[(size_t)best];
        dets.push_back(d);
    }
    std::sort(dets.begin(), dets.end(),
              [](const Detection& a, const Detection& b) { return a.score > b.score; });
    return dets;
}

}  // namespace gdino
