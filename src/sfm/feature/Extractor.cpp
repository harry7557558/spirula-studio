#include "sfm/core/Log.h"
#include "i18n/catalog/Sfm.h"
#include "sfm/feature/Extractor.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <vector>

#if SS_HAVE_ALIKED
#include "aliked/Aliked.h"
#endif
#if SS_HAVE_LOMA
#include "loma/Loma.h"
#endif

namespace sfm {
namespace {

class SiftFrontend : public IFeatureExtractor {
public:
    explicit SiftFrontend(const SiftOptions& opt) : ext_(opt) {}
    FeatureSet extract(const GrayImage& img) override { return ext_.extract(img); }
    const char* name() const override { return "sift"; }

private:
    SiftExtractor ext_;
};

#if SS_HAVE_ALIKED

// ALIKED behind the same contract.
//
// The conversions in both directions are the whole of it, and each is a
// convention this repository already fixed elsewhere:
//
//   * input -- the loader's optional RGB companion buffer, which `extract`
//     already asks for so it can colour the point cloud. ALIKED normalizes by
//     1/255 and nothing else, so the bytes go straight through.
//   * output -- keypoints have no scale and no orientation, because the
//     detector has neither. They carry a detection score instead, which
//     `FeatureSet::rank` and features.bin v5 exist for. Leaving `scale` at 0
//     is deliberate: a fabricated scale would be silently wrong wherever one
//     is used as a size.
class AlikedFrontend : public IFeatureExtractor {
public:
    explicit AlikedFrontend(const AlikedOptions& opt) : opt_(opt) {
        ext_.load(opt.model);
        aopts_.max_num_features = opt.max_num_features;
        aopts_.min_score = (float)opt.min_score;
    }

    const char* name() const override { return "aliked"; }
    bool wantsColor() const override { return true; }

    FeatureSet extract(const GrayImage& img) override {
        FeatureSet fs;
        fs.width = img.width;
        fs.height = img.height;
        fs.dim = (uint32_t)ext_.descriptorDim();
        fs.dtype = DType::F32;
        if (!img.hasColor())
            throw std::runtime_error(
                "ALIKED needs a colour image; the loader decoded luma only");

        const aliked::Features f =
            ext_.extract(img.rgb.data(), img.width, img.height, aopts_);

        // Canonical order by position, exactly as GPU SIFT emits (D16). The
        // extractor's own order comes from a partial_sort by score, and a
        // score-sorted index would bias every downstream tie-break -- the
        // matcher's cap, the mapper taking the first 3D point a feature
        // corresponds to -- toward high-scoring features.
        std::vector<uint32_t> idx(f.keypoints.size());
        for (uint32_t i = 0; i < idx.size(); i++) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](uint32_t a, uint32_t b) {
            const aliked::Keypoint& p = f.keypoints[a];
            const aliked::Keypoint& q = f.keypoints[b];
            if (p.x != q.x) return p.x < q.x;
            if (p.y != q.y) return p.y < q.y;
            return p.score > q.score;
        });

        const size_t n = idx.size();
        fs.keypoints.resize(n);
        fs.descriptors.resize(n * fs.dim * sizeof(float));
        float* dst = reinterpret_cast<float*>(fs.descriptors.data());
        for (size_t i = 0; i < n; i++) {
            const aliked::Keypoint& k = f.keypoints[idx[i]];
            fs.keypoints[i] = {k.x, k.y, /*scale=*/0.0f, /*orientation=*/0.0f, k.score};
            std::memcpy(dst + i * fs.dim, &f.descriptors[(size_t)idx[i] * fs.dim],
                        fs.dim * sizeof(float));
        }
        if (opt_.verbose)
            slog::err(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_features,
                      {(long long)n});
        return fs;
    }

private:
    AlikedOptions           opt_;
    aliked::Extractor       ext_;
    aliked::ExtractOptions  aopts_;
};

#endif  // SS_HAVE_ALIKED

#if SS_HAVE_LOMA

// LoMa behind the same contract. What differs from ALIKED is the descriptors:
// 128-D or 256-D depending on the variant, and NOT unit norm, so a cosine
// threshold has to normalize for itself (COLMAP's LOMA_BRUTEFORCE does).
class LomaFrontend : public IFeatureExtractor {
public:
    explicit LomaFrontend(const LomaOptions& opt) : opt_(opt) {
        ext_.load(opt.detector_model.empty() ? "loma-dad" : opt.detector_model,
                  opt.descriptor_model);
        lopts_.max_num_features = opt.max_num_features;
        lopts_.min_score = (float)opt.min_score;
    }

    const char* name() const override { return "loma"; }
    bool wantsColor() const override { return true; }

    FeatureSet extract(const GrayImage& img) override {
        FeatureSet fs;
        fs.width = img.width;
        fs.height = img.height;
        fs.dim = (uint32_t)ext_.descriptorDim();
        fs.dtype = DType::F32;
        if (!img.hasColor())
            throw std::runtime_error(
                "LoMa needs a colour image; the loader decoded luma only");

        const loma::Features f =
            ext_.extract(img.rgb.data(), img.width, img.height, lopts_);

        // Canonical order by position, exactly as GPU SIFT emits (D16): the
        // extractor's own order is by score, and a score-sorted index would
        // bias every downstream tie-break toward high-scoring features.
        std::vector<uint32_t> idx(f.keypoints.size());
        for (uint32_t i = 0; i < idx.size(); i++) idx[i] = i;
        std::sort(idx.begin(), idx.end(), [&](uint32_t a, uint32_t b) {
            const loma::Keypoint& p = f.keypoints[a];
            const loma::Keypoint& q = f.keypoints[b];
            if (p.x != q.x) return p.x < q.x;
            if (p.y != q.y) return p.y < q.y;
            return p.score > q.score;
        });

        const size_t n = idx.size();
        fs.keypoints.resize(n);
        fs.descriptors.resize(n * fs.dim * sizeof(float));
        float* dst = reinterpret_cast<float*>(fs.descriptors.data());
        for (size_t i = 0; i < n; i++) {
            const loma::Keypoint& k = f.keypoints[idx[i]];
            fs.keypoints[i] = {k.x, k.y, /*scale=*/0.0f, /*orientation=*/0.0f, k.score};
            std::memcpy(dst + i * fs.dim, &f.descriptors[(size_t)idx[i] * fs.dim],
                        fs.dim * sizeof(float));
        }
        if (opt_.verbose)
            slog::err(slog::Tag::Extract, spirula::i18n::msg::sfm::sift_features,
                      {(long long)n});
        return fs;
    }

private:
    LomaOptions          opt_;
    loma::Extractor      ext_;
    loma::ExtractOptions lopts_;
};

#endif  // SS_HAVE_LOMA

}  // namespace

bool isAlikedType(const std::string& type) { return type.rfind("aliked", 0) == 0; }
// The five released variants, spelled out: a near miss should be a usage error
// at parse time, not a download that 404s halfway through a run.
bool isLomaType(const std::string& type) {
    return type == "loma-b" || type == "loma-b128" || type == "loma-r" ||
           type == "loma-l" || type == "loma-g";
}

int lomaDescriptorDim(const std::string& variant) {
#if SS_HAVE_LOMA
    const std::string d = loma::descriptor_for_matcher(variant);
    if (d == "loma-dedode-b") return 128;
    if (d == "loma-dedode-g") return 256;
#else
    (void)variant;
#endif
    return 0;
}

int defaultMaxImageSize(const std::string& type) {
    return (isAlikedType(type) || isLomaType(type)) ? 1600 : 3200;
}

std::unique_ptr<IFeatureExtractor> createFeatureExtractor(const std::string& type,
                                                          const SiftOptions& sift,
                                                          const AlikedOptions& aliked,
                                                          const LomaOptions& loma_opt) {
    if (type == "sift") return std::make_unique<SiftFrontend>(sift);
    if (isAlikedType(type)) {
#if SS_HAVE_ALIKED
        AlikedOptions opt = aliked;
        // --features names the checkpoint, so an explicit --aliked-model is
        // only needed to point at a file on disk.
        if (opt.model.empty() || isAlikedType(opt.model)) opt.model = type;
        return std::make_unique<AlikedFrontend>(opt);
#else
        throw std::runtime_error(
            "this build has no learned frontend: '" + type +
            "' needs the inference layer, which is SS_BUILD_SAM=ON");
#endif
    }
    if (isLomaType(type)) {
#if SS_HAVE_LOMA
        LomaOptions opt = loma_opt;
        opt.variant = type;
        // --features names the variant, and the variant names the descriptor:
        // loma-b128 was trained on DeDoDe-B, the other four on DeDoDe-G.
        if (opt.descriptor_model.empty())
            opt.descriptor_model = loma::descriptor_for_matcher(type);
        if (opt.descriptor_model.empty())
            throw std::runtime_error(
                "unknown LoMa variant '" + type +
                "' (expected loma-b, loma-b128, loma-r, loma-l or loma-g)");
        return std::make_unique<LomaFrontend>(opt);
#else
        (void)loma_opt;
        throw std::runtime_error(
            "this build has no learned frontend: '" + type +
            "' needs the inference layer, which is SS_BUILD_SAM=ON");
#endif
    }
    (void)loma_opt;
    throw std::runtime_error("unknown feature type '" + type +
                             "' (expected sift, aliked-n16rot, aliked-n32, loma-b or "
                             "loma-b128)");
}

}  // namespace sfm
