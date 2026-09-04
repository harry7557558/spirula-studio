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

namespace sfm {
namespace {

#if SS_HAVE_ALIKED

class LightGlueMatcher : public IFeatureMatcher {
public:
    explicit LightGlueMatcher(const MatchOptions& match, const LightGlueOptions& opt)
        : match_(match), opt_(opt) {
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
        for (const aliked::Match& x : m) {
            // FeatureMatch carries a *distance*, and everything downstream that
            // reads it sorts ascending (the max_num_matches cap). LightGlue
            // reports a confidence in (0, 1], so the distance is its
            // complement -- a monotone map, which is all that ordering needs.
            out.push_back({x.i, x.j, 1.0f - x.score});
        }
        if (match_.max_num_matches > 0 && out.size() > match_.max_num_matches) {
            std::partial_sort(out.begin(), out.begin() + match_.max_num_matches, out.end(),
                              [](const FeatureMatch& p, const FeatureMatch& q) {
                                  return p.distance < q.distance;
                              });
            out.resize(match_.max_num_matches);
        }
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
        in.descriptors = reinterpret_cast<const float*>(f.descriptors.data());
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
        // FeatureMatch carries a DISTANCE and everything downstream sorts it
        // ascending; the matcher reports a confidence in (0, 1], so the
        // distance is its complement -- monotone, which is all ordering needs.
        for (const loma::Match& x : m) out.push_back({x.i, x.j, 1.0f - x.score});
        if (match_.max_num_matches > 0 && out.size() > match_.max_num_matches) {
            std::partial_sort(out.begin(), out.begin() + match_.max_num_matches, out.end(),
                              [](const FeatureMatch& p, const FeatureMatch& q) {
                                  return p.distance < q.distance;
                              });
            out.resize(match_.max_num_matches);
        }
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
        in.descriptors = reinterpret_cast<const float*>(f.descriptors.data());
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

}  // namespace

bool isLearnedMatcher(const std::string& type) {
    return type == "lightglue" || isLomaType(type);
}

std::unique_ptr<IFeatureMatcher> createFeatureMatcher(const std::string& type,
                                                      const MatchOptions& match,
                                                      const LightGlueOptions& lightglue,
                                                      const LomaMatchOptions& loma_opt) {
    if (type == "bruteforce") return std::make_unique<BruteForceMatcher>(match);
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
    throw std::runtime_error("unknown matcher '" + type +
                             "' (expected bruteforce, lightglue, loma-b or loma-b128)");
}

}  // namespace sfm
