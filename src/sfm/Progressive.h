#pragma once

// Progressive alignment (docs/notes/sfm-progressive-alignment.md): the mapper
// runs once per pixel error, loose to tight, every attempt continuing from the
// last; then the images still out of the largest model are detected again with
// more features and resolution and the tail of the ladder runs once more, a
// pass kept only if the model gained images. Matches are verified at the
// loosest error and re-verified per attempt, so the mapper never sees a pair
// verified looser than its own gate (D47).

#include "sfm/Pipeline.h"
#include "sfm/feature/Verification.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace sfm {

struct ProgressiveAttempt {
    double error = 0;          // extraction pixels
    size_t pairs = 0;          // verified pairs the attempt mapped with
    uint32_t largest = 0;      // images in the largest model after it
    uint32_t registered = 0;   // distinct images in any model
    size_t models = 0;
    double seconds = 0;
};

struct ProgressivePass {
    int pass = 0;
    size_t targets = 0;
    int image_size = 0, features = 0;
    size_t pairs = 0, verified = 0;
    uint32_t before = 0, after = 0;   // images in the largest model
    double error_before = 0, error_after = 0;
    double seconds = 0;
    bool kept = false;
};

class ProgressiveAligner {
public:
    // `loose` was verified at cfg.progressive_error_start and `feats` hold every
    // row of the feature files (no compaction). Feature passes append to the one
    // and replace entries of the other; all of them must outlive the aligner.
    ProgressiveAligner(MatchesDatabase& loose, std::vector<FeatureSet>& feats,
                       const SfmConfig& cfg, const VerifyCalibration& calib,
                       const RigTable& rigs, const SequenceTable& seqs,
                       const std::string& image_dir, const std::filesystem::path& workspace);

    std::vector<Reconstruction> run(AssembleStats& ast);

    // The last attempt's, at the end error: what the finishing passes run with.
    Mapper& mapper() { return *mapper_; }
    const std::vector<ProgressiveAttempt>& attempts() const { return attempts_; }
    const std::vector<ProgressivePass>& passes() const { return passes_; }

    // One line per attempt and per feature pass; read by programs, so untranslated.
    bool writeReport(const std::filesystem::path& path) const;

private:
    VerificationOptions verifyOptions(double error, BearingCache& bc,
                                      std::vector<Camera>& percam) const;
    MatchesDatabase reverify(double error) const;
    void startAttempt(double error, const MatchesDatabase& db);
    void ladder(std::vector<Reconstruction>& models, size_t from, AssembleStats& ast);
    std::vector<uint32_t> targets(const std::vector<Reconstruction>& models) const;
    void featurePasses(std::vector<Reconstruction>& models, AssembleStats& ast);
    void announce();

    MatchesDatabase& loose_;
    std::vector<FeatureSet>& feats_;
    SfmConfig cfg_;
    const VerifyCalibration& calib_;
    const RigTable& rigs_;
    const SequenceTable& seqs_;
    std::string image_dir_;
    std::filesystem::path workspace_;
    std::vector<double> errors_;
    std::unique_ptr<MatchesDatabase> db_;   // the current attempt's, past the first
    std::unique_ptr<Mapper> mapper_;
    std::vector<ProgressiveAttempt> attempts_;
    std::vector<ProgressivePass> passes_;
    double start_ = 0;
    int steps_ = 0;   // attempts and passes begun, for the progress bar
    bool in_pass_ = false;
};

}  // namespace sfm
