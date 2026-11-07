#include "sfm/feature/LearnedMatcher.h"

#include "sfm/feature/Extractor.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <vector>

#if SS_HAVE_ALIKED
#include "aliked/model/LightGlue.h"
#endif
#if SS_HAVE_LOMA
#include "loma/Loma.h"
#endif
#if SS_HAVE_FLEXGLUE
#include "flexglue/FlexGlue.h"
#include "i18n/catalog/Sfm.h"
#include "sfm/core/Image.h"
#include "sfm/core/Log.h"

#include <chrono>
#include <list>
#include <unordered_map>
#endif
#if SS_HAVE_ALIKED || SS_HAVE_LOMA || SS_HAVE_FLEXGLUE
#include "nn/Device.h"
#endif

namespace sfm {
namespace {

#if SS_HAVE_ALIKED || SS_HAVE_LOMA || SS_HAVE_FLEXGLUE
// LightGlue and LoMa run on the inference layer's process-wide device, so
// their `device` field alone selects nothing; the run's resolved identity is
// committed before either loads a checkpoint (see Extractor.cpp).
inline void configureLearnedDevice(const std::string& selector) {
    if (!selector.empty()) nn::configure_device(selector);
}
#endif

#if SS_HAVE_ALIKED

class LightGlueMatcher : public IFeatureMatcher {
public:
    explicit LightGlueMatcher(const MatchOptions& match, const LightGlueOptions& opt)
        : match_(match), opt_(opt) {
        configureLearnedDevice(opt.device_selector);
        lg_.load(opt.model);
        mopt_.min_score = (float)opt.min_score;
    }

    const char* name() const override { return "lightglue"; }

    std::vector<FeatureMatch> match(const FeatureSet& a, const FeatureSet& b) override {
        std::vector<FeatureMatch> out;
        if (a.count() == 0 || b.count() == 0) return out;
        check(a);
        check(b);

        std::vector<float> ka, kb;
        const std::vector<aliked::Match> m =
            lg_.match(view(a, ka), view(b, kb), mopt_);

        out.reserve(m.size());
        // The cap keeps the smallest distances; LightGlue reports a confidence
        // in (0, 1], and its complement is a monotone map onto one.
        std::vector<float> dist;
        dist.reserve(m.size());
        for (const aliked::Match& x : m) {
            out.push_back({x.i, x.j});
            dist.push_back(1.0f - x.score);
        }
        keepClosest(out, dist, match_.max_num_matches);
        return out;
    }

private:
    static void check(const FeatureSet& f) {
        if (f.dtype != DType::F32)
            throw std::runtime_error(
                "LightGlue needs float descriptors; these are uint8. It is trained for "
                "a specific frontend -- use --features aliked-n16rot or aliked-n32");
    }

    // The keypoints LightGlue sees must be in the frame its `image_size` names.
    // FeatureSet reports them in the SOURCE image's pixels (D46), and
    // width/height are that same frame, so the two already agree -- but only
    // because scaleKeypoints ran. Passing extract_width here instead would
    // scale every keypoint by the downscale factor and quietly ruin the
    // positional encoding.
    // `xy` is filled with the packed [n, 2] the model wants: Keypoint carries
    // five floats, so its x/y are strided, not an array of pairs.
    static aliked::MatchInput view(const FeatureSet& f, std::vector<float>& xy) {
        xy.resize((size_t)f.count() * 2);
        for (uint32_t i = 0; i < f.count(); i++) {
            xy[(size_t)i * 2] = f.keypoints[i].x;
            xy[(size_t)i * 2 + 1] = f.keypoints[i].y;
        }
        aliked::MatchInput in;
        in.keypoints = xy.data();
        in.descriptors = reinterpret_cast<const float*>(f.descData());
        in.n = f.count();
        in.width = f.width;
        in.height = f.height;
        return in;
    }

    MatchOptions           match_;
    LightGlueOptions       opt_;
    aliked::Matcher        lg_;
    aliked::MatchOptions   mopt_;
};

#endif  // SS_HAVE_ALIKED

#if SS_HAVE_LOMA

// LoMa's matcher, behind the same interface as LightGlue's. It refuses a
// descriptor of the wrong width: each variant was trained against one, so a
// 128-D input to a 256-D matcher is a shape error, not a degraded result.
class LomaFeatureMatcher : public IFeatureMatcher {
public:
    LomaFeatureMatcher(const MatchOptions& match, const std::string& model,
                       const LomaMatchOptions& opt)
        : match_(match) {
        configureLearnedDevice(opt.device_selector);
        m_.load(model);
        mopt_.min_score = (float)opt.min_score;
    }

    const char* name() const override { return "loma"; }

    std::vector<FeatureMatch> match(const FeatureSet& a, const FeatureSet& b) override {
        std::vector<FeatureMatch> out;
        if (a.count() == 0 || b.count() == 0) return out;
        check(a);
        check(b);

        std::vector<float> ka, kb;
        const std::vector<loma::Match> m = m_.match(view(a, ka), view(b, kb), mopt_);

        out.reserve(m.size());
        // As LightGlue's above: the confidence's complement is the distance.
        std::vector<float> dist;
        dist.reserve(m.size());
        for (const loma::Match& x : m) {
            out.push_back({x.i, x.j});
            dist.push_back(1.0f - x.score);
        }
        keepClosest(out, dist, match_.max_num_matches);
        return out;
    }

private:
    void check(const FeatureSet& f) const {
        if (f.dtype != DType::F32)
            throw std::runtime_error(
                "the LoMa matcher needs float descriptors; these are uint8. Use "
                "--features loma-b128 or loma-b");
        if ((int)f.dim != m_.descriptorDim())
            throw std::runtime_error(
                "the LoMa matcher wants " + std::to_string(m_.descriptorDim()) +
                "-D descriptors and these are " + std::to_string(f.dim) +
                "-D; --features and --matcher name different variants");
    }

    // Keypoints in the SOURCE image's pixels (D46), which is the frame
    // width/height names -- true only because scaleKeypoints ran. Passing
    // extract_width instead would scale every keypoint and ruin the encoding.
    static loma::MatchInput view(const FeatureSet& f, std::vector<float>& xy) {
        xy.resize((size_t)f.count() * 2);
        for (uint32_t i = 0; i < f.count(); i++) {
            xy[(size_t)i * 2] = f.keypoints[i].x;
            xy[(size_t)i * 2 + 1] = f.keypoints[i].y;
        }
        loma::MatchInput in;
        in.keypoints = xy.data();
        in.descriptors = reinterpret_cast<const float*>(f.descData());
        in.n = f.count();
        in.width = f.width;
        in.height = f.height;
        return in;
    }

    MatchOptions       match_;
    loma::Matcher      m_;
    loma::MatchOptions mopt_;
};

#endif  // SS_HAVE_LOMA


#if SS_HAVE_FLEXGLUE

// FlexGlue behind the same interface. It needs the IMAGE of each feature set,
// so it only runs through matchBatch, which names the sets by index; the dense
// features of the last `cache_size` images stay on the device.
class FlexGlueFeatureMatcher : public IFeatureMatcher {
public:
    FlexGlueFeatureMatcher(const MatchOptions& match, const FlexGlueMatchOptions& opt)
        : match_(match), opt_(opt) {
        configureLearnedDevice(opt.device_selector);
        model_.load(opt.model);
        mopt_.filter_threshold = (float)opt.min_score;
        mopt_.max_distance_px = (float)opt.max_distance;
        mopt_.min_covisibility = (float)opt.min_covisibility;
        mopt_.early_stop_decay = (float)opt.early_stop_decay;
    }

    ~FlexGlueFeatureMatcher() override {
        if (!opt_.verbose || pairs_ == 0) return;
        namespace L = sfm::slog;
        namespace M = spirula::i18n::msg::sfm;
        L::err(L::Tag::Match, M::match_flexglue_summary,
               {(long long)rejected_, (long long)pairs_, L::num(opt_.min_covisibility, 2),
                (long long)stopped_, L::num(match_s_ / (double)pairs_, 3),
                L::num(describe_s_ / (double)std::max<size_t>(described_, 1), 3)});
        const flexglue::Model::SelfCacheStats c = model_.selfCacheStats();
        L::diag(L::Tag::Match,
                "[flexglue] layer-1 self cache: %llu hits, %llu misses, %llu evictions "
                "(capacity %d); sample repeats: %zu reused, %zu not kept (%.0f MB budget)",
                (unsigned long long)c.hits, (unsigned long long)c.misses,
                (unsigned long long)c.evictions, c.capacity, done_hits_, done_refused_,
                kDoneBudget / 1048576.0);
    }

    const char* name() const override { return "flexglue"; }

    std::vector<FeatureMatch> match(const FeatureSet&, const FeatureSet&) override {
        throw std::runtime_error(
            "FlexGlue reads the images, so it matches through matchBatch, which names them");
    }

    void matchBatch(const std::vector<FeatureSet>& feats,
                    const std::vector<std::pair<uint32_t, uint32_t>>& pairs, size_t begin,
                    size_t end, std::vector<std::vector<FeatureMatch>>& out) override {
        if (opt_.image_paths.size() != feats.size())
            throw std::runtime_error(
                "FlexGlue has " + std::to_string(opt_.image_paths.size()) +
                " image paths for " + std::to_string(feats.size()) +
                " feature sets; it needs the image folder (--flexglue-images)");
        out.assign(end - begin, {});
        for (size_t k = begin; k < end; ++k) {
            const uint32_t i = pairs[k].first, j = pairs[k].second;
            const FeatureSet &a = feats[i], &b = feats[j];
            if (a.count() == 0 || b.count() == 0) continue;
            // The focal and gyro searches match a sample before verification
            // matches every pair; the repeat takes the sample's result.
            const uint64_t key = ((uint64_t)i << 32) | j;
            if (auto hit = done_.find(key); hit != done_.end()) {
                done_bytes_ -= hit->second.size() * sizeof(FeatureMatch);
                out[k - begin] = std::move(hit->second);
                done_.erase(hit);
                ++done_hits_;
                continue;
            }
            const auto fa = dense(i), fb = dense(j);
            std::vector<float> ka, kb;
            const auto t0 = std::chrono::steady_clock::now();
            const flexglue::MatchResult r =
                model_.match(*fa, view(a, ka), *fb, view(b, kb), mopt_);
            match_s_ += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0)
                            .count();
            ++pairs_;
            if (!r.covisible_pair) ++rejected_;
            if (r.stop_layer < 9) ++stopped_;
            std::vector<FeatureMatch>& m = out[k - begin];
            std::vector<float> dist;
            for (uint32_t q = 0; q < a.count(); ++q)
                if (r.matches0[q] >= 0) {
                    m.push_back({q, (uint32_t)r.matches0[q]});
                    dist.push_back(1.0f - r.scores0[q]);
                }
            keepClosest(m, dist, match_.max_num_matches);
            const size_t bytes = m.size() * sizeof(FeatureMatch);
            if (done_bytes_ + bytes <= kDoneBudget) {
                done_[key] = m;
                done_bytes_ += bytes;
            } else {
                ++done_refused_;
            }
        }
    }

private:
    // Keypoints in the source image's pixels (D46), the frame width/height names.
    static flexglue::Keypoints view(const FeatureSet& f, std::vector<float>& xy) {
        xy.resize((size_t)f.count() * 2);
        for (uint32_t i = 0; i < f.count(); i++) {
            xy[(size_t)i * 2] = f.keypoints[i].x;
            xy[(size_t)i * 2 + 1] = f.keypoints[i].y;
        }
        flexglue::Keypoints k;
        k.xy = xy.data();
        k.n = f.count();
        k.width = f.width;
        k.height = f.height;
        return k;
    }

    // The dense features of image `i`, decoded at full size as COLMAP reads
    // the file, then squashed to 512x512.
    std::shared_ptr<flexglue::DenseFeatures> dense(uint32_t i) {
        auto it = cache_.find(i);
        if (it != cache_.end()) {
            lru_.splice(lru_.begin(), lru_, it->second.second);
            return it->second.first;
        }
        const auto t0 = std::chrono::steady_clock::now();
        const GrayImage img = loadGrayImage(opt_.image_paths[i], 1 << 30, true, "", opt_.gamut,
                                            opt_.is_linear, false,
                                            opt_.apply_exif_orientation, "", opt_.exposure);
        if (!img.hasColor())
            throw std::runtime_error("FlexGlue could not read " + opt_.image_paths[i]);
        auto f = model_.describe(img.rgb.data(), img.width, img.height);
        describe_s_ +=
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        ++described_;
        lru_.push_front(i);
        cache_[i] = {f, lru_.begin()};
        while ((int)cache_.size() > std::max(opt_.cache_size, 2)) {
            cache_.erase(lru_.back());
            lru_.pop_back();
        }
        return f;
    }

    MatchOptions            match_;
    FlexGlueMatchOptions    opt_;
    flexglue::Model         model_;
    flexglue::MatchOptions  mopt_;
    std::list<uint32_t>     lru_;
    std::unordered_map<uint32_t, std::pair<std::shared_ptr<flexglue::DenseFeatures>,
                                           std::list<uint32_t>::iterator>> cache_;
    // Pairs held for a repeat: an entry leaves on its first reuse, and once the
    // budget is spent nothing new is held, so the samples (matched first) stay.
    static constexpr size_t kDoneBudget = 256ull << 20;
    std::unordered_map<uint64_t, std::vector<FeatureMatch>> done_;
    size_t done_bytes_ = 0, done_hits_ = 0, done_refused_ = 0;
    size_t pairs_ = 0, rejected_ = 0, stopped_ = 0, described_ = 0;
    double match_s_ = 0, describe_s_ = 0;
};

#endif  // SS_HAVE_FLEXGLUE

}  // namespace

bool isLearnedMatcher(const std::string& type) {
    return type == "lightglue" || type == "flexglue" || isLomaType(type);
}

bool matcherReadsDescriptors(const std::string& type) {
    return type == "lightglue" || isLomaType(type);
}

std::unique_ptr<IFeatureMatcher> createFeatureMatcher(const std::string& type,
                                                      const MatchOptions& match,
                                                      const LightGlueOptions& lightglue,
                                                      const LomaMatchOptions& loma_opt,
                                                      const FlexGlueMatchOptions& flexglue) {
    if (type == "bruteforce") return std::make_unique<BruteForceMatcher>(match);
    if (type == "flexglue") {
#if SS_HAVE_FLEXGLUE
        return std::make_unique<FlexGlueFeatureMatcher>(match, flexglue);
#else
        (void)flexglue;
        throw std::runtime_error(
            "this build has no learned matcher: --matcher flexglue needs the "
            "inference layer, which is SS_BUILD_SAM=ON");
#endif
    }
    if (type == "lightglue") {
#if SS_HAVE_ALIKED
        return std::make_unique<LightGlueMatcher>(match, lightglue);
#else
        throw std::runtime_error(
            "this build has no learned matcher: --matcher lightglue needs the "
            "inference layer, which is SS_BUILD_SAM=ON");
#endif
    }
    if (isLomaType(type)) {
#if SS_HAVE_LOMA
        // --matcher names the variant, so an explicit --loma-matcher-model is
        // only needed to point at a file on disk.
        const std::string model = loma_opt.model.empty() ? type : loma_opt.model;
        return std::make_unique<LomaFeatureMatcher>(match, model, loma_opt);
#else
        (void)loma_opt;
        throw std::runtime_error(
            "this build has no learned matcher: --matcher " + type +
            " needs the inference layer, which is SS_BUILD_SAM=ON");
#endif
    }
    (void)loma_opt;
    (void)flexglue;
    throw std::runtime_error("unknown matcher '" + type +
                             "' (expected bruteforce, lightglue, flexglue, loma-b or loma-b128)");
}

}  // namespace sfm
