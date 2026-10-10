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
#include "sfm/map/SensorPriors.h"
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
    size_t reused = 0;            // features an earlier run had already written
    uint64_t features = 0;
    uint64_t features_new = 0;    // ... of which this run extracted
    // Masking (D39), all zero when no mask directory was given.
    size_t masked_images = 0;     // images that found a mask file
    size_t unmasked_images = 0;   // images that did not
    size_t mask_unreadable = 0;   // found a mask file but could not decode it
    uint64_t masked_out = 0;      // keypoints dropped by masks
    std::string first_unmasked;   // an example, for the warning
    bool warned_empty = false;    // "this mask masked out everything", warned once
    bool warned_exif_mirror = false;   // "the tag also asked for a mirror", ditto
    // Over the images this run decoded (GrayImage::gain, ::peak).
    size_t decoded = 0;
    float gain_min = 1.0f, gain_max = 1.0f;
    float peak = 0.0f;
};

struct MatchStats {
    size_t images = 0, pairs = 0, kept = 0, scored = 0;
    uint64_t inliers = 0, putative = 0;
    double select_seconds = 0;
};

// A telemetry file read once per run, with the queries the sensor priors and
// the gauge fit make of it.
struct LoadedCapture {
    SensorCapture cap;
    SensorTimeline timeline;
    TelemetryCarrier carrier = TelemetryCarrier::None;
};

struct SensorCaptures {
    std::vector<std::unique_ptr<LoadedCapture>> loaded;
    bool empty() const { return loaded.empty(); }
    std::vector<SensorCapture> caps() const {
        std::vector<SensorCapture> out;
        for (const auto& lc : loaded) out.push_back(lc->cap);
        return out;
    }
};

// Every telemetry file the config names, or none when `--sensor-gauge none`.
SensorCaptures loadSensorCaptures(const SfmConfig& cfg, bool verbose);

// The sensors as a prior source over `db`'s images (sfm/map/SensorPriors.h),
// uncalibrated; null without telemetry or with every use of it switched off.
std::unique_ptr<TelemetryPriors> makeSensorPriors(const SfmConfig& cfg,
                                                  const SensorCaptures& sensors,
                                                  const MatchesDatabase& db,
                                                  const std::vector<uint32_t>& cam_ids);

// What --metric-gps auto reads off a capture to pick a mode.
struct MetricGpsEvidence {
    bool positions_file = false;
    int telemetry_gps = 0;   // telemetry files with a usable GPS track
    int telemetry_dji = 0;   // ... of them a DJI djmd track (an Avata or Osmo .OSV)
    int exif_fixes = 0;
    int exif_no_alt = 0;
    int exif_phone = 0;      // ... fixes a phone maker's camera wrote
    std::string phone_make;  // one of those makers, for the log
};

enum class MetricGpsWhy {
    Explicit, Positions, DjiTelemetry, OtherTelemetry, ExifAltitude, ExifNoAltitude,
    ExifPhone, NoGps
};

struct MetricGpsChoice {
    std::string mode;   // none | horizontal | full
    MetricGpsWhy why = MetricGpsWhy::NoGps;
};

// `full` for a DJI telemetry track (barometric altitude) or EXIF fixes with an
// altitude; `horizontal` where the altitude is a phone's or an action camera's
// (D75), or missing; `none` without GPS or beside a positions file.
MetricGpsChoice resolveMetricGps(const MetricGpsEvidence& e);
bool isPhoneMake(const std::string& make);
MetricGpsEvidence metricGpsEvidence(const SfmConfig& cfg, const SensorCaptures& sensors,
                                    const std::string& imagedir);
// Replaces an `auto` cfg.metric_gps with the mode it resolves to, and logs why;
// an explicit mode is returned as is.
MetricGpsChoice applyMetricGpsAuto(SfmConfig& cfg, const SensorCaptures& sensors,
                                   const std::string& imagedir);

// Each image's EXIF GPS as a prior source (ExifGpsPriors), an equirect group in
// `cams` declaring its up unless --no-level-erp; null when no image under
// `imagedir` carries a fix, or with --no-sensor-map and --no-sensor-pairs.
std::unique_ptr<ExifGpsPriors> makeExifGpsPriors(const SfmConfig& cfg, const std::string& imagedir,
                                                 const MatchesDatabase& db, const CameraSetup& cams,
                                                 bool verbose);

// Calibrate `priors` against the gyro from pairs and their matches (a
// sample's putative ones, or the database's verified ones), reporting per group.
void calibrateSensorPriors(TelemetryPriors& priors, const std::vector<FeatureSet>& feats,
                           const std::vector<std::pair<uint32_t, uint32_t>>& pairs,
                           const std::vector<std::vector<FeatureMatch>>& matches,
                           const std::vector<Camera>& cams, const TwoViewOptions& tvopt,
                           int threads, bool verbose);
void calibrateSensorPriorsFromDatabase(TelemetryPriors& priors, const MatchesDatabase& db,
                                       const std::vector<FeatureSet>& feats,
                                       const std::vector<Camera>& cams,
                                       const TwoViewOptions& tvopt, int threads, bool verbose);

// One camera per image, from a setup.
std::vector<Camera> perImageCameras(const CameraSetup& cs, size_t num_images);

// Calibrated verification (D45): a fisheye pair is verified on unit bearings,
// which needs a focal, so one is searched on a sample of pairs first unless
// given (sfm/feature/Verification.h, `bootstrapFocal`).
struct VerifyCalibration {
    CameraSetupOptions setup;
    size_t sample_pairs = 150;  // pairs per group used by the focal search
    // The run's telemetry, for the sensor priors; null skips them.
    const SensorCaptures* sensors = nullptr;
    // outputs
    CameraSetup cameras;        // what the grouping decided, for the caller to reuse
    bool used_bearings = false;
    // The sensors over this database, calibrated where the pairs allowed.
    std::unique_ptr<TelemetryPriors> priors;
    // Where the images are, for their EXIF GPS when no telemetry covers them.
    std::string image_dir;
    std::unique_ptr<ExifGpsPriors> exif_priors;
    // The source mapping and GPS pairing use: the telemetry's, else the EXIF's.
    PriorSource* positionPriors() const {
        if (priors) return priors.get();
        return exif_priors.get();
    }
};

// ---------------------------------------------------------------------------
// Stages
// ---------------------------------------------------------------------------

// Features for every image under `imagedir` (or each stem in `only`), into
// `outdir` mirroring the tree. `reuse` keeps a file newer than its image and mask,
// with `trusted` an older one too; a file no image maps to is removed.
int extractDirectory(const std::string& imagedir, const std::filesystem::path& outdir,
                     const SfmConfig& cfg, ExtractStats& stats, bool reuse = false,
                     std::vector<std::filesystem::path>* trusted = nullptr,
                     const std::set<std::string>* only = nullptr);

// Where an interrupted run's matching left its work, and what those files must
// carry to be this run's (sfm/core/Resume.h). Null is a run that starts over.
struct MatchResume {
    std::filesystem::path dir;
    std::string signature;
};

// Every features.bin under `featdir`, in the sorted order that fixes the image
// indices everything downstream uses. Fills `db.images` alongside. Non-zero on
// a failure that stops the stage.
int loadFeatureDir(const std::string& featdir, const SfmConfig& cfg, bool with_descriptors,
                   std::vector<FeatureSet>& feats, MatchesDatabase& db);

// Pairing, matching and two-view verification over a feature directory.
int matchFeatureDir(const std::string& featdir, const SfmConfig& cfg, PairMode mode,
                    bool verify, std::vector<FeatureSet>& feats, MatchesDatabase& db,
                    MatchStats& stats, VerifyCalibration* calib = nullptr,
                    const MatchResume* res = nullptr);

std::vector<Reconstruction> runMapper(Mapper& mapper, const MatchesDatabase& db,
                                      const std::vector<FeatureSet>& feats, SfmConfig& cfg,
                                      AssembleStats& ast);

// The run's rigs over `db`'s image names (sfm/core/Rig.h), reported when
// verbose. Throws std::runtime_error on a definition that does not resolve.
RigTable buildRigs(const MatchesDatabase& db, const SfmConfig& cfg, bool verbose);

// The run's sequences the same way (sfm/core/Sequence.h).
SequenceTable buildSequences(const MatchesDatabase& db, const SfmConfig& cfg, bool verbose);

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
              std::vector<ModelGauge>& gauge, const SensorCaptures* sensors = nullptr);

void resolveImageNames(std::vector<Reconstruction>& models, const std::string& imagedir);
void recolorPoints(std::vector<Reconstruction>& models, const SfmConfig& cfg);
void splitCamerasBySize(std::vector<Reconstruction>& models,
                        const std::vector<FeatureSet>& feats);
void writeModels(const std::vector<Reconstruction>& models,
                 const std::filesystem::path& dir, bool verbose,
                 const std::vector<ModelGauge>& gauge = {}, const RigTable* rigs = nullptr);
void writeRigs(const std::filesystem::path& dir, const Reconstruction& m, const RigTable* rigs);
// The rigs.txt writeRigs left beside a model, as a table over its image names
// with each member's calibration in `m.rigs`. Empty when there is no file.
RigTable readRigs(const std::filesystem::path& dir, Reconstruction& m);

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

// An EXR or a TIFF with an ICC profile states its own colour space; adopt it
// for any of `image-gamut` / `image-linear` that `seen` does not already name.
void adoptFileColorSpace(SfmConfig& cfg, const std::string& imagedir,
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
