#pragma once

// The stages of a run, and `auto`'s ordering of them.
//
// One implementation, two front ends: `spirula sfm` parses a command line and
// calls these, and an in-process front end calls the same functions with its
// own sinks installed (docs/notes/sfm-in-process-plan.md). Nothing here parses
// arguments or prints outside slog.
//
// Stages still read and write the files they always did, so `spirula sfm
// extract | match | map` remains the way to bisect a failure.

#include "sfm/SfmConfig.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/Events.h"
#include "sfm/core/FeatureCompaction.h"
#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/core/Log.h"
#include "sfm/core/Mask.h"
#include "sfm/core/Matches.h"
#include "sfm/core/Model.h"
#include "sfm/feature/Pairing.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/MetricGauge.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace sfm {

// ---------------------------------------------------------------------------
// What a stage reports about itself
// ---------------------------------------------------------------------------

struct ExtractStats {
    size_t images = 0, failed = 0, unreadable = 0;
    uint64_t features = 0;
    // Masking (D39), all zero when no mask directory was given.
    size_t masked_images = 0;     // images that found a mask file
    size_t unmasked_images = 0;   // images that did not
    size_t mask_unreadable = 0;   // found a mask file but could not decode it
    uint64_t masked_out = 0;      // keypoints dropped by masks
    std::string first_unmasked;   // an example, for the warning
    bool warned_empty = false;    // "this mask masked out everything", warned once
};

struct MatchStats {
    size_t images = 0, pairs = 0, kept = 0, scored = 0;
    uint64_t inliers = 0, putative = 0;
    double select_seconds = 0;
};

// Calibrated verification (D45). A fisheye pair verified on raw pixels asks a
// pinhole fundamental matrix to explain rays 100 deg off axis, which it cannot
// represent at all; the correspondences that carry the wide field of view are
// thrown away as outliers. With a camera model in hand we verify on unit
// bearings instead, where the epipolar constraint is exact at any FOV.
//
// The model needs a focal length before it can produce bearings, and the
// geometric default (diag/pi) is off by ~1.7x on a 200 deg lens, which is
// enough to warp the bearings and lose most of the benefit -- so unless one is
// given we search for it on a sample of pairs first (`bootstrapFocal`).
struct VerifyCalibration {
    CameraSetupOptions setup;
    size_t sample_pairs = 150;  // pairs per group used by the focal search
    // outputs
    CameraSetup cameras;        // what the grouping decided, for the caller to reuse
    bool used_bearings = false;
};

// ---------------------------------------------------------------------------
// Stages
// ---------------------------------------------------------------------------

// Features for every image under `imagedir`, written to `outdir` mirroring the
// image tree. Non-zero on a failure that stops the stage.
int extractDirectory(const std::string& imagedir, const std::filesystem::path& outdir,
                     const SfmConfig& cfg, ExtractStats& stats);

// Pairing, matching and two-view verification over a feature directory.
int matchFeatureDir(const std::string& featdir, const SfmConfig& cfg, PairMode mode,
                    bool verify, std::vector<FeatureSet>& feats, MatchesDatabase& db,
                    MatchStats& stats, VerifyCalibration* calib = nullptr);

std::vector<Reconstruction> runMapper(Mapper& mapper, const MatchesDatabase& db,
                                      const std::vector<FeatureSet>& feats, SfmConfig& cfg,
                                      AssembleStats& ast);

// The run's rigs over `db`'s image names (sfm/core/Rig.h), reported when
// verbose. Throws std::runtime_error on a definition that does not resolve.
RigTable buildRigs(const MatchesDatabase& db, const SfmConfig& cfg, bool verbose);

// The passes that run after the mapper: merge, audit, grow, prune, reseed.
std::vector<Reconstruction> finishModels(Mapper& mapper,
                                         std::vector<Reconstruction> models,
                                         const SfmConfig& cfg, bool verbose,
                                         double& seconds);

// What a model's frame means, written beside it as `gauge.txt`. `up` and
// `scale` are tokens rather than prose ("sensors", "gps", "cameras", "none"):
// the file is read by programs, and the log is where the sentences are.
struct ModelGauge {
    bool oriented = false;   // +Z is up because something measured it
    bool metric = false;     // one unit is one metre
    std::string up = "none";
    std::string scale = "none";
    double scale_sigma = 0;  // relative; 0 when nothing estimated one
};

// Levelling, centring and the metric gauge. False when no metric frame fitted.
bool fixGauge(std::vector<Reconstruction>& models, const SfmConfig& cfg,
              const std::string& imagedir, bool verbose,
              std::vector<ModelGauge>& gauge);

void resolveImageNames(std::vector<Reconstruction>& models, const std::string& imagedir);
void recolorPoints(std::vector<Reconstruction>& models, const SfmConfig& cfg);
void splitCamerasBySize(std::vector<Reconstruction>& models,
                        const std::vector<FeatureSet>& feats);
void writeModels(const std::vector<Reconstruction>& models,
                 const std::filesystem::path& dir, bool verbose,
                 const std::vector<ModelGauge>& gauge = {}, const RigTable* rigs = nullptr);

// ---------------------------------------------------------------------------
// Reporting helpers the summary is built from
// ---------------------------------------------------------------------------

void reprojStats(const Reconstruction& rec, const std::vector<FeatureSet>& feats,
                 double& mean, double& median, size_t& nobs);
size_t distinctRegistered(const std::vector<Reconstruction>& models);
void printExtraModels(const std::vector<Reconstruction>& models,
                      const std::vector<FeatureSet>& feats);
void printFolderCoverage(const std::vector<Reconstruction>& models,
                         const MatchesDatabase& db);
void printAssembly(const AssembleStats& ast, size_t models,
                   slog::Tag tag = slog::Tag::Map);
void printCameraSetup(slog::Tag tag, const CameraSetup& cs,
                      const CameraSetupOptions& sopt, size_t nimages);
void reportFeatureCompaction(const FeatureCompactionStats& stats);
void warnIfMasksLookInverted(const ExtractStats& st);

// Per-keypoint colours while the image is hot, then back to source pixels.
void sampleFeatureColors(FeatureSet& fs, const GrayImage& img);
void finishFeatures(FeatureSet& fs, const GrayImage& img);

// An EXR capture states its own colour space; adopt it for any of
// `image-gamut` / `image-linear` that `seen` does not already name.
void adoptExrColorSpace(SfmConfig& cfg, const std::string& imagedir,
                        const std::set<std::string>& seen);

bool holdsImagesOutside(const std::filesystem::path& root,
                        const std::filesystem::path& nested);

// Seconds on a steady clock; the one place a stage times itself.
double now();

// ---------------------------------------------------------------------------
// `auto`
// ---------------------------------------------------------------------------

// The sinks a run reports through, installed for as long as this object
// lives. Nothing set means the CLI's own behaviour: print, no events, no
// cancelling. One run per process, which is what makes the sinks global.
class RunContext {
public:
    RunContext() = default;
    RunContext(const RunContext&) = delete;
    RunContext& operator=(const RunContext&) = delete;
    ~RunContext();

    void set_log(slog::Sink s);
    void set_events(events::Sink s);
    void set_cancel(const std::atomic<bool>* flag);
    void set_progress_dir(const std::string& dir);
};

struct AutoInputs {
    std::string image_dir;
    std::string workspace;
    // The front end said where the masks are; do not look for a sibling.
    bool mask_dir_explicit = false;
    // Flags the front end set by hand, so preset fan-out and the EXR colour
    // space adoption know what they may not overwrite.
    std::set<std::string> explicit_flags;
    // What the presets moved, reported in the run header.
    std::vector<PresetChange> preset_changes;
};

struct AutoResult {
    // 0 ok, 2 failed, 3 partial, 4 no metric frame -- the CLI's exit codes,
    // as data, so a front end need not read one number for two facts.
    int exit_code = 0;
    int64_t registered = 0, images = 0, points = 0, models = 0;
    double mean_reproj = 0.0, median_reproj = 0.0;
    bool partial = false;
    bool metric = true;
    std::filesystem::path sparse_dir;
};

// Every stage, in order, from a finalized config. Throws `Cancelled` when the
// run was stopped; other failures come back in `exit_code`.
AutoResult run_auto(SfmConfig& cfg, const AutoInputs& in);

// What a run was asked for: a finalized config and the inputs beside it.
struct AutoRequest {
    SfmConfig cfg;
    AutoInputs in;
    // Where snapshots go; the caller installs it, since a front end running
    // the pipeline in its own process routes them differently.
    std::string progress_dir;
    bool wants_help = false;
};

// Read a settings list -- `spirula sfm auto`'s own arguments, and what a front
// end with no command line hands over rather than keeping a second mapping of
// its own. "" on success, otherwise the sentence to show the user.
//
// Presets fill in first, then the manifest, then finalize(): an explicit
// setting beats the file and the file beats a preset's guess.
std::string parse_auto_args(const std::vector<std::string>& args, AutoRequest& out);

}  // namespace sfm
