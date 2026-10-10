#include "sfm/Progressive.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>

#include "sfm/core/Cancel.h"
#include "sfm/core/Events.h"
#include "sfm/core/Log.h"
#include "sfm/core/Progress.h"
#include "sfm/feature/LearnedMatcher.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/feature/Pairing.h"
#include "sfm/map/Assemble.h"
#include "i18n/TimeFormat.h"
#include "i18n/catalog/Sfm.h"

namespace fs = std::filesystem;

namespace sfm {

namespace L = sfm::slog;
namespace M = spirula::i18n::msg::sfm;
using sfm::slog::Tag;
using spirula::i18n::format_duration;

namespace {

// A feature pass runs the ladder's last few attempts again, not all of it: the
// model it continues from already stands, and only the new pairs need the
// loose start.
constexpr size_t kTailAttempts = 3;
// What a feature pass may cost the largest model's mean reprojection error.
constexpr double kPassErrorGrowth = 1.10;

double meanError(const Reconstruction& m, const std::vector<FeatureSet>& feats) {
    double mean = 0, median = 0;
    size_t n = 0;
    reprojStats(m, feats, mean, median, n);
    return mean;
}

// The model on screen after an attempt or a pass, coloured the way the
// mapper's own snapshots are: from the colours sampled at extraction.
void snapshot(const Reconstruction& m, const std::vector<FeatureSet>& feats) {
    sfm::progress::model(m, /*force=*/true, [&](const Point3D& p, uint8_t rgb[3]) {
        uint32_t acc[3] = {0, 0, 0}, n = 0;
        for (const TrackElement& e : p.track) {
            const FeatureSet& fs = feats[e.image_id];
            if (!fs.hasColors()) continue;
            const uint8_t* c = &fs.colors[(size_t)e.point2D_idx * 3];
            for (int k = 0; k < 3; k++) acc[k] += c[k];
            n++;
        }
        if (n)
            for (int k = 0; k < 3; k++) rgb[k] = (uint8_t)((acc[k] + n / 2) / n);
    });
}

int lerpGeometric(int a, int b, double t) {
    if (a <= 0 || b <= 0) return std::max(a, b);
    return (int)std::lround(a * std::pow((double)b / a, t));
}

}  // namespace

ProgressiveAligner::ProgressiveAligner(MatchesDatabase& loose, std::vector<FeatureSet>& feats,
                                       const SfmConfig& cfg, const VerifyCalibration& calib,
                                       const RigTable& rigs, const SequenceTable& seqs,
                                       const std::string& image_dir, const fs::path& workspace)
    : loose_(loose), feats_(feats), cfg_(cfg), calib_(calib), rigs_(rigs), seqs_(seqs),
      image_dir_(image_dir), workspace_(workspace), errors_(cfg.progressiveErrors()) {}

VerificationOptions ProgressiveAligner::verifyOptions(double error, BearingCache& bc,
                                                      std::vector<Camera>& percam) const {
    VerificationOptions vo;
    vo.two_view = cfg_.twoview;
    vo.two_view.ransac.max_error = error;
    vo.num_threads = cfg_.threads;
    vo.match_batch_pairs = cfg_.match.batch_pairs;
    vo.report = false;
    const CameraSetup& cs = calib_.cameras;
    if (cs.anyWide()) {
        bc = precomputeBearings(feats_, perImageCameras(cs, feats_.size()), !cs.mixed(),
                                cfg_.threads);
        vo.bearings = &bc;
    }
    if (calib_.priors && cfg_.sensor_verify && calib_.priors->anyRotation()) {
        percam = perImageCameras(cs, feats_.size());
        vo.priors = calib_.priors.get();
        vo.cameras = &percam;
    }
    return vo;
}

MatchesDatabase ProgressiveAligner::reverify(double error) const {
    BearingCache bc;
    std::vector<Camera> percam;
    const VerificationOptions vo = verifyOptions(error, bc, percam);
    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    pairs.reserve(loose_.pairs.size());
    for (const TwoViewMatches& t : loose_.pairs) pairs.emplace_back(t.image1, t.image2);
    auto stored = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& out) {
        out.resize(e - b);
        for (size_t p = b; p < e; p++) out[p - b] = loose_.pairs[p].matches.toVector();
    };
    MatchesDatabase db;
    db.images = loose_.images;
    db.cameras = loose_.cameras;
    db.camera_ids = loose_.camera_ids;
    db.focal_prior = loose_.focal_prior;
    db.focal_measured = loose_.focal_measured;
    db.pairs = verifyPairs(feats_, pairs, stored, vo);
    return db;
}

void ProgressiveAligner::startAttempt(double error, const MatchesDatabase& db) {
    cfg_.mapper.max_reproj_error = error;
    cfg_.manager.merge.filter_reproj_error = error;
    mapper_ = std::make_unique<Mapper>(db, feats_, cfg_.mapper, calib_.cameras.ids, &rigs_,
                                       &seqs_,
                                       cfg_.sensor_map ? calib_.positionPriors() : nullptr);
}

void ProgressiveAligner::ladder(std::vector<Reconstruction>& models, size_t from,
                                AssembleStats& ast) {
    const bool verbose = !cfg_.quiet;
    for (size_t k = from; k < errors_.size(); k++) {
        cancel::check();
        const double t0 = now();
        const double e = errors_[k];
        ProgressiveAttempt a;
        a.error = e;
        // The mapper holds a reference to its database, so the old one goes
        // only after the new mapper replaces the mapper built on it.
        std::unique_ptr<MatchesDatabase> prev = std::move(db_);
        if (k == 0) {
            startAttempt(e, loose_);
            a.pairs = loose_.pairs.size();
        } else {
            db_ = std::make_unique<MatchesDatabase>(reverify(e));
            startAttempt(e, *db_);
            a.pairs = db_->pairs.size();
        }
        prev.reset();
        if (!in_pass_) announce();
        if (verbose)
            L::out(Tag::Map, M::progressive_attempt,
                   {(long long)(k + 1), (long long)errors_.size(), L::num(e, 1),
                    (long long)a.pairs});
        ast = AssembleStats();
        if (models.empty()) {
            models = runMapper(*mapper_, k == 0 ? loose_ : *db_, feats_, cfg_, ast);
        } else {
            // What the tighter gate no longer supports is dropped by the
            // refinement itself; growth then retries every image from scratch.
            for (Reconstruction& m : models)
                if (m.numRegistered() >= 2) m = mapper_->refine(m);
            AssembleOptions ao = cfg_.assemble;
            ao.verbose = verbose;
            ao.tag = "map";
            models = assembleModels(*mapper_, std::move(models), cfg_.manager, ao, ast);
        }
        // Tracks grown at a looser gate are trimmed by it, never rebuilt.
        if (k + 1 == errors_.size())
            for (Reconstruction& m : models)
                if (m.numRegistered() >= 2) m = mapper_->retriangulate(m);
        std::set<uint32_t> any;
        for (const Reconstruction& m : models) {
            a.largest = std::max(a.largest, m.numRegistered());
            for (const auto& kv : m.images)
                if (kv.second.registered) any.insert(kv.first);
        }
        a.registered = (uint32_t)any.size();
        a.models = models.size();
        a.seconds = now() - t0;
        attempts_.push_back(a);
        if (!models.empty()) snapshot(models.front(), feats_);
        if (verbose)
            L::out(Tag::Map, M::progressive_attempt_done,
                   {(long long)(k + 1), (long long)errors_.size(), (long long)a.largest,
                    (long long)a.registered, (long long)a.models,
                    format_duration(a.seconds)});
    }
}

std::vector<uint32_t> ProgressiveAligner::targets(const std::vector<Reconstruction>& models) const {
    std::vector<char> in_main(feats_.size(), 0);
    if (!models.empty())
        for (const auto& kv : models.front().images)
            if (kv.second.registered && kv.first < in_main.size()) in_main[kv.first] = 1;
    std::vector<uint64_t> inliers(feats_.size(), 0);
    for (const TwoViewMatches& t : loose_.pairs) {
        inliers[t.image1] += t.matches.size();
        inliers[t.image2] += t.matches.size();
    }
    std::vector<uint32_t> out;
    for (uint32_t i = 0; i < feats_.size(); i++)
        if (!in_main[i] && inliers[i] >= (uint64_t)cfg_.progressive_min_matches) out.push_back(i);
    return out;
}

void ProgressiveAligner::featurePasses(std::vector<Reconstruction>& models, AssembleStats& ast) {
    const bool verbose = !cfg_.quiet;
    const bool sift = cfg_.features == "sift";
    const bool loma = isLomaType(cfg_.features);
    const int base_features = sift   ? cfg_.sift.max_num_features
                              : loma ? cfg_.loma.max_num_features
                                     : cfg_.aliked.max_num_features;
    const int end_features = cfg_.progressive_max_features_end > 0
                                 ? cfg_.progressive_max_features_end
                                 : 4 * base_features;
    const int passes = std::max(1, cfg_.progressive_feature_steps);
    const size_t tail = errors_.size() > kTailAttempts ? errors_.size() - kTailAttempts : 0;
    std::vector<std::string> names(loose_.images.size());
    for (size_t i = 0; i < names.size(); i++) names[i] = loose_.images[i].name;
    const FileOrder order = fileOrder(names);
    // Where each image's current rows live: a kept pass moves its targets to
    // that pass's folder, and a later pass reloads them from there.
    std::vector<fs::path> source(names.size(), workspace_ / "features");

    int dry = 0;
    for (int p = 1; p <= passes && dry < cfg_.progressive_patience; p++) {
        cancel::check();
        const std::vector<uint32_t> tg = targets(models);
        if (tg.empty() || models.empty()) break;
        if (cfg_.progressive_time > 0 && now() - start_ > 60.0 * cfg_.progressive_time) {
            if (verbose)
                L::out(Tag::Map, M::progressive_time_up,
                       {L::num(cfg_.progressive_time, 1), (long long)(p - 1)});
            break;
        }
        const double t0 = now();
        const double frac = (double)p / passes;
        announce();
        int end_size = cfg_.progressive_image_size_end;
        if (end_size <= 0) {
            for (uint32_t t : tg)
                end_size = std::max({end_size, feats_[t].width, feats_[t].height});
            // A learned extractor's memory grows with the pixel count: LoMa
            // needs 14.6 GB at 3869 px.
            if (!sift) end_size = std::min(end_size, 2 * cfg_.max_image_size);
        }
        const fs::path passdir = workspace_ / "features.progressive" / std::to_string(p);
        SfmConfig pc = cfg_;
        pc.max_image_size = lerpGeometric(cfg_.max_image_size, end_size, frac);
        const int nf = lerpGeometric(base_features, end_features, frac);
        pc.sift.max_num_features = pc.aliked.max_num_features = pc.loma.max_num_features = nf;
        // The last pass also lowers SIFT's contrast floor: flat and dark texture.
        if (sift && p == passes) pc.sift.peak_threshold *= 0.5;
        pc.sift.verbose = pc.aliked.verbose = pc.loma.verbose = false;

        const uint32_t before = models.front().numRegistered();
        const double before_err = meanError(models.front(), feats_);
        if (verbose)
            L::out(Tag::Map, M::progressive_feature_pass,
                   {(long long)p, (long long)passes, (long long)tg.size(),
                    (long long)pc.max_image_size, (long long)nf});

        // Everything a refused pass has to put back.
        std::vector<std::pair<uint32_t, FeatureSet>> old_feats;
        for (uint32_t t : tg) old_feats.emplace_back(t, feats_[t]);
        const std::vector<TwoViewMatches> old_pairs = loose_.pairs;
        const std::vector<uint32_t> old_counts = [&] {
            std::vector<uint32_t> c;
            for (const ImageEntry& im : loose_.images) c.push_back(im.num_features);
            return c;
        }();
        std::vector<Reconstruction> old_models = models;
        std::unique_ptr<MatchesDatabase> old_db = std::move(db_);
        std::unique_ptr<Mapper> old_mapper = std::move(mapper_);
        const size_t old_attempts = attempts_.size();
        auto restore = [&] {
            for (auto& kv : old_feats) feats_[kv.first] = std::move(kv.second);
            loose_.pairs = old_pairs;
            for (size_t i = 0; i < old_counts.size(); i++)
                loose_.images[i].num_features = old_counts[i];
            mapper_.reset();  // before the database it refers to
            db_ = std::move(old_db);
            mapper_ = std::move(old_mapper);
            models = std::move(old_models);
            attempts_.resize(old_attempts);
        };

        std::vector<std::pair<uint32_t, uint32_t>> pairs;
        size_t verified = 0;
        // A pass is an extra the model does not need, so whatever stops it --
        // most often the GPU running out at the larger size -- costs the pass,
        // never the run. A cancel still ends the run.
        try {
            // ---- detect the targets again ----
            std::set<std::string> only;
            for (uint32_t t : tg) only.insert(names[t]);
            ExtractStats est;
            if (extractDirectory(image_dir_, passdir, pc, est, /*reuse=*/false, nullptr,
                                 &only)) {
                restore();
                break;
            }
            std::vector<char> is_target(feats_.size(), 0);
            for (uint32_t t : tg) {
                is_target[t] = 1;
                feats_[t] = readFeatures((passdir / (names[t] + ".bin")).string(), true, true);
                loose_.images[t].num_features = feats_[t].count();
            }

            // ---- match them against the largest model, with the run's matcher ----
            for (const TwoViewMatches& t : loose_.pairs)
                if (is_target[t.image1] || is_target[t.image2])
                    pairs.emplace_back(t.image1, t.image2);
            loose_.pairs.erase(std::remove_if(loose_.pairs.begin(), loose_.pairs.end(),
                                              [&](const TwoViewMatches& t) {
                                                  return is_target[t.image1] || is_target[t.image2];
                                              }),
                               loose_.pairs.end());
            std::vector<uint32_t> partners;
            for (const auto& kv : models.front().images)
                if (kv.second.registered) partners.push_back(kv.first);
            for (uint32_t i : partners)
                feats_[i] = readFeatures((source[i] / (names[i] + ".bin")).string(), true, true);
            PairSelectionOptions popt = cfg_.prefilter;
            popt.num_neighbors *= 2;
            const auto chosen = prefilterPairsFor(feats_, popt, tg, partners, &order);
            pairs.insert(pairs.end(), chosen.begin(), chosen.end());
            for (const auto& q : sequentialPairs((uint32_t)names.size(), 2 * cfg_.overlap, false,
                                                 folderRuns(names)))
                if (is_target[q.first] || is_target[q.second]) pairs.push_back(q);
            std::sort(pairs.begin(), pairs.end());
            pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());

            std::unique_ptr<IFeatureMatcher> matcher =
                createFeatureMatcher(cfg_.matcher, cfg_.match, cfg_.lightglue, cfg_.loma_match);
            auto matchFn = [&](size_t b, size_t e, std::vector<std::vector<FeatureMatch>>& out) {
                matcher->matchBatch(feats_, pairs, b, e, out);
            };
            BearingCache bc;
            std::vector<Camera> percam;
            const VerificationOptions vo = verifyOptions(errors_.front(), bc, percam);
            std::vector<TwoViewMatches> fresh = verifyPairs(feats_, pairs, matchFn, vo);
            matcher.reset();
            for (uint32_t i : partners) feats_[i].dropDescriptors();
            for (uint32_t t : tg) feats_[t].dropDescriptors();
            verified = fresh.size();
            for (TwoViewMatches& t : fresh) loose_.pairs.push_back(std::move(t));

            // ---- the ladder's tail, from the largest model alone ----
            // The other models' observations of the targets index their old rows.
            models.resize(1);
            in_pass_ = true;
            ladder(models, tail, ast);
            in_pass_ = false;
        } catch (const Cancelled&) {
            throw;
        } catch (const std::exception& e) {
            in_pass_ = false;
            restore();
            if (!models.empty()) snapshot(models.front(), feats_);
            L::warn(Tag::Map, M::progressive_feature_failed,
                    {(long long)p, (long long)passes, std::string(e.what())});
            break;
        }

        const uint32_t after = models.empty() ? 0 : models.front().numRegistered();
        const double after_err = models.empty() ? 0 : meanError(models.front(), feats_);
        const bool kept = after > before && after_err <= kPassErrorGrowth * before_err;
        if (verbose)
            L::out(Tag::Map, kept ? M::progressive_feature_kept : M::progressive_feature_undone,
                   {(long long)p, (long long)passes, (long long)pairs.size(),
                    (long long)verified, (long long)before, (long long)after,
                    L::num(before_err, 2), L::num(after_err, 2), format_duration(now() - t0)});
        ProgressivePass rec;
        rec.pass = p;
        rec.targets = tg.size();
        rec.image_size = pc.max_image_size;
        rec.features = nf;
        rec.pairs = pairs.size();
        rec.verified = verified;
        rec.before = before;
        rec.after = after;
        rec.error_before = before_err;
        rec.error_after = after_err;
        rec.seconds = now() - t0;
        rec.kept = kept;
        if (kept) {
            dry = 0;
            for (uint32_t t : tg) source[t] = passdir;
        } else {
            restore();
            dry++;
            snapshot(models.front(), feats_);
        }
        passes_.push_back(rec);
    }
}

bool ProgressiveAligner::writeReport(const fs::path& path) const {
    std::ofstream f(path);
    if (!f) return false;
    f << "# attempt error_px pairs largest aligned models seconds\n";
    for (size_t k = 0; k < attempts_.size(); k++) {
        const ProgressiveAttempt& a = attempts_[k];
        f << "attempt " << k + 1 << ' ' << a.error << ' ' << a.pairs << ' ' << a.largest << ' '
          << a.registered << ' ' << a.models << ' ' << a.seconds << '\n';
    }
    f << "# pass images image_size features pairs verified largest_before largest_after "
         "error_before error_after seconds outcome\n";
    for (const ProgressivePass& p : passes_)
        f << "pass " << p.pass << ' ' << p.targets << ' ' << p.image_size << ' ' << p.features
          << ' ' << p.pairs << ' ' << p.verified << ' ' << p.before << ' ' << p.after << ' '
          << p.error_before << ' ' << p.error_after << ' ' << p.seconds << ' '
          << (p.kept ? "kept" : "undone") << '\n';
    return (bool)f;
}

// The mapper begins stages of its own (seed, refine) inside an attempt, so the
// step is announced again each time rather than once.
void ProgressiveAligner::announce() {
    const int64_t total = (int64_t)errors_.size() +
                          (cfg_.progressive_features ? std::max(1, cfg_.progressive_feature_steps) : 0);
    const int64_t done = std::min<int64_t>(total, (int64_t)(++steps_));
    events::stage_begin(Stage::Progressive, total);
    events::progress(Stage::Progressive, done, total);
}

std::vector<Reconstruction> ProgressiveAligner::run(AssembleStats& ast) {
    start_ = now();
    steps_ = 0;
    std::vector<Reconstruction> models;
    ladder(models, 0, ast);
    if (cfg_.progressive_features) featurePasses(models, ast);
    if (!models.empty()) snapshot(models.front(), feats_);
    return models;
}

}  // namespace sfm
