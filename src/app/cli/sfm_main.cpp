// spirula-sfm: the SfM pipeline CLI. Subcommands are the stage graph
// (src/sfm/README.md), each reading and writing files on disk so any one of
// them can be replaced by COLMAP's equivalent to bisect a failure:
//
//   spirula-sfm auto    <image_dir> -o <workspace>      all of the below
//   spirula-sfm extract <image|dir> -o <features>
//   spirula-sfm match   <features>  -o <matches.bin>
//   spirula-sfm map     <matches.bin> <features> -o <sparse/>
//   spirula-sfm merge   <sparse/>   -o <merged/>
//   spirula-sfm ba      <bal_problem.txt>               solver benchmark
//
// This file is presentation and plumbing only: what a flag *means* lives in
// sfm/SfmConfig.h's descriptor table, which is also what `--help` prints and
// what the GUI will edit. Flags that do not name one scalar field are parsed
// here, before the table is offered the token, so a hand-parsed name always
// wins -- `map --audit` (run an audit pass) has to beat the table's `--audit`
// / `--no-audit` switch for the audit pass.
//
// The self-checks are separate binaries (src/sfm/tests/, one per area).
#include "app/Tools.h"
#include "sfm/Pipeline.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <cmath>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "core/ColorSpace.h"
#include "core/Env.h"
#include "core/ExrImage.h"
#include "sfm/SfmConfig.h"
#include "sfm/core/Cancel.h"
#include "sfm/core/Manifest.h"
#include "sfm/core/Events.h"
#include "sfm/core/Progress.h"
#include "sfm/core/CameraSetup.h"
#include "sfm/core/FeatureCompaction.h"
#include "sfm/core/Log.h"
#include "sfm/core/Features.h"
#include "sfm/core/Image.h"
#include "sfm/core/ImageLoader.h"
#include "sfm/core/Mask.h"
#include "sfm/core/Matches.h"
#include "sfm/feature/Matcher.h"
#include "sfm/feature/PairSelection.h"
#include "sfm/feature/Pairing.h"
#include "sfm/feature/Sift.h"
#include "sfm/feature/Verification.h"
#include "sfm/geometry/TwoView.h"
#include "sfm/map/Assemble.h"
#include "sfm/map/Mapper.h"
#include "sfm/map/MetricGauge.h"
#include "sfm/map/Orient.h"
#include "sfm/map/Merge.h"

#include "i18n/catalog/Sfm.h"
#include "i18n/catalog/Cli.h"
#include "i18n/catalog/SfmHelp.h"

// `spirula-sfm ba`, in sfm_ba.cpp. It prints its own help.
int cmdBa(int argc, char** argv);
void printBaHelp(FILE* out);

// Set by the build (cmake/SsOptions.cmake reads it from pyproject.toml).
#ifndef SS_VERSION
#define SS_VERSION "dev"
#endif

namespace fs = std::filesystem;

// ---- merging (D43) ----
// Bundle-adjust whatever absorbed something: a merged model is two separately
// optimized halves glued along a seam never optimized as one.
struct MergeSummary {
    size_t before = 0, after = 0, merges = 0, refused = 0;
    double seconds = 0, ba_seconds = 0;
};
using namespace sfm;

// Every line this tool prints goes out tagged and translated; see
// sfm/core/Log.h for the mechanism and for what stays English.
namespace L = sfm::slog;
namespace M = spirula::i18n::msg::sfm;
namespace CM = spirula::i18n::msg::cli;
namespace H = spirula::i18n::msg::sfmhelp;
using sfm::slog::Tag;

// How this tool was invoked ("spirula sfm" as dispatched); see app/Tools.h.
// The examples in the command tables below are written against the historical
// name and rewritten at print time.
static const char* kProgram = "spirula sfm";

// Ctrl-C sets the token the library polls, so a run unwinds and frees the
// device instead of dying inside a Vulkan submit. A second one is the user
// saying they meant it: restore the default and let it kill the process.
static std::atomic<bool> g_interrupted{false};

extern "C" void sfmOnInterrupt(int sig) {
    if (g_interrupted.exchange(true)) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
}

static std::string with_program_name(const char* text) {
    return app::help_text(text, "spirula-sfm");
}

// ---------------------------------------------------------------------------
// Command help
// ---------------------------------------------------------------------------
// One record per subcommand: what the top-level list shows, the argument
// syntax, the paragraph that says what the stage does, the flags this file
// parses by hand (the table prints the rest), and worked examples. Kept
// together so the six help screens stay in one shape.
struct CommandInfo {
    const char* name;
    uint32_t mask;
    // The prose is translated (i18n/catalog/SfmHelp.h); the syntax and the
    // examples are what the reader types, so they are not.
    const spirula::i18n::Msg* summary;              // one line, for `--help`
    const char* usage;                              // argument syntax
    const spirula::i18n::Msg* description[4];       // one per paragraph, then null
    void (*own_options)(FILE*);
    const char* examples;                           // pre-wrapped, indented
    bool exit_status;                               // print `auto`'s footer
};

// A description paragraph, wrapped where the language allows rather than where
// an English hand-wrap once put a newline.
static void printParagraph(FILE* out, const spirula::i18n::Msg& m) {
    for (const std::string& line : spirula::i18n::wrap(m.get(), 94))
        std::fprintf(out, "  %s\n", line.c_str());
}

// One line of a command's own (hand-parsed) options, in the table's layout.
static void helpLine(FILE* out, const char* flag, const char* value, const char* help) {
    printOptionLine(out, flag, value, help);
}

static void ownOptionsAuto(FILE* out) {
    helpLine(out, "-o, --output DIR", H::word_required.get(),
             H::opt_auto_output.get());
    helpLine(out, "--manifest FILE", "", H::opt_manifest.get());
    helpLine(out, "--rig PREFIX,PREFIX,...", "", H::opt_rig.get());
    helpLine(out, "--no-masks", "", H::opt_no_masks.get());
    helpLine(out, "--no-manage", "", H::opt_no_manage_auto.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsExtract(FILE* out) {
    helpLine(out, "-o, --output DIR|FILE", "features", H::opt_extract_output.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMatch(FILE* out) {
    helpLine(out, "-o, --output FILE", "", H::opt_match_output.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMap(FILE* out) {
    helpLine(out, "-o, --output DIR", "", H::opt_map_output.get());
    helpLine(out, "--rig PREFIX,PREFIX,...", "", H::opt_rig.get());
    helpLine(out, "--audit", "", H::opt_map_audit.get());
    helpLine(out, "--no-manage", "", H::opt_no_manage_map.get());
    helpLine(out, "--progress-dir DIR", "", H::opt_progress_dir.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}
static void ownOptionsMerge(FILE* out) {
    helpLine(out, "-o, --output DIR", "", H::opt_merge_output.get());
    helpLine(out, "-h, --help", "", H::opt_help.get());
}

static const CommandInfo kCommands[] = {
    {"auto", CMD_AUTO, &H::sum_auto,
     "[IMAGE_DIR|DATASET_DIR] -o WORKSPACE [options]",
     {&H::desc_auto_1, &H::desc_auto_2, &H::desc_auto_3},
     ownOptionsAuto,
     "  spirula-sfm auto images/ -o workspace/\n"
     "  spirula-sfm auto -o ws/                       # ./images and ./masks, all defaults\n"
     "  spirula-sfm auto DATASET/ -o ws/ --data-type video --quality medium\n"
     "  spirula-sfm auto images/ -o ws/ --camera-model opencv-fisheye --focal 520\n"
     "  spirula-sfm auto images/ -o ws/ --camera-model cam0=thin-prism-fisheye",
     /*exit_status=*/true},

    {"extract", CMD_EXTRACT, &H::sum_extract,
     "<IMAGE|DIR> [-o OUT] [options]",
     {&H::desc_extract_1, &H::desc_extract_2, nullptr},
     ownOptionsExtract,
     "  spirula-sfm extract images/ -o features/\n"
     "  spirula-sfm extract images/ -o features/ --masks masks/ --max-features 4096\n"
     "  spirula-sfm extract photo.jpg -o photo.bin",
     false},

    {"match", CMD_MATCH, &H::sum_match,
     "<FEATURE_DIR> -o MATCHES.BIN [options]",
     {&H::desc_match_1, &H::desc_match_2, nullptr},
     ownOptionsMatch,
     "  spirula-sfm match features/ -o matches.bin\n"
     "  spirula-sfm match features/ -o matches.bin --pairs prefilter --threads 8\n"
     "  spirula-sfm match features/ -o matches.bin --camera-model opencv \\\n"
     "                             --camera-model cam0=thin-prism-fisheye --focal cam0=520",
     false},

    {"map", CMD_MAP, &H::sum_map,
     "<MATCHES.BIN> <FEATURE_DIR> -o SPARSE_DIR [options]",
     {&H::desc_map_1, &H::desc_map_2, nullptr},
     ownOptionsMap,
     "  spirula-sfm map matches.bin features/ -o sparse/ --images images/\n"
     "  spirula-sfm map matches.bin features/ -o sparse/ --max-models 1\n"
     "  spirula-sfm map matches.bin features/ --resume sparse/ --check\n"
     "  spirula-sfm map matches.bin features/ -o sparse/ --resume sparse/ --audit",
     false},

    {"merge", CMD_MERGE, &H::sum_merge,
     "<SPARSE_DIR|MODEL_DIR> [more...] -o DIR [options]",
     {&H::desc_merge_1, &H::desc_merge_2, &H::desc_merge_3, nullptr},
     ownOptionsMerge,
     "  spirula-sfm merge sparse/ -o merged/\n"
     "  spirula-sfm merge sparse/ --in-place\n"
     "  spirula-sfm merge runA/sparse/0 runB/sparse/0 -o merged/ --min-common 5\n"
     "  spirula-sfm merge ws/sparse --in-place --metric-gps horizontal --images ws/images",
     false},
};

static const CommandInfo* findCommand(const std::string& name) {
    for (const CommandInfo& c : kCommands)
        if (name == c.name) return &c;
    return nullptr;
}

static void printCommandHelp(const CommandInfo& c) {
    FILE* out = stdout;
    std::fprintf(out, "%s %s -- %s\n\n", kProgram, c.name, c.summary->get());
    std::fprintf(out, "%s\n  %s %s %s\n\n", H::label_usage.get(), kProgram, c.name,
                 c.usage);
    std::fprintf(out, "%s\n", H::label_description.get());
    for (int i = 0; i < 4 && c.description[i]; i++) {
        if (i) std::fprintf(out, "\n");
        printParagraph(out, *c.description[i]);
    }
    std::fprintf(out, "\n%s\n", H::label_options.get());
    c.own_options(out);
    // Defaults are printed from a fresh config, which is exactly what the
    // command starts from; `auto` says separately what its presets then move.
    SfmConfig defaults;
    printConfigOptions(out, c.mask, defaults);
    std::fprintf(out, "\n%s\n%s\n", H::label_examples.get(),
                 with_program_name(c.examples).c_str());
    if (c.exit_status) {
        std::fprintf(out, "\n%s\n", H::label_exit_status.get());
        std::fprintf(out, "  0  %s\n", H::exit_0.get());
        std::fprintf(out, "  1  %s\n", H::exit_1.get());
        std::fprintf(out, "  2  %s\n", H::exit_2.get());
        std::fprintf(out, "  3  %s\n", H::exit_3.get());
        std::fprintf(out, "  4  %s\n", H::exit_4.get());
    }
}

static void printTopHelp(FILE* out) {
    std::fprintf(out, "%s %s -- %s\n\n", kProgram, SS_VERSION, H::tagline.get());
    std::fprintf(out, "%s\n  %s <command> [options]\n  %s <command> --help\n\n",
                 H::label_usage.get(), kProgram, kProgram);
    std::fprintf(out, "%s\n", H::label_commands.get());
    for (const CommandInfo& c : kCommands)
        std::fprintf(out, "  %-9s %s\n", c.name, c.summary->get());
    std::fprintf(out, "  %-9s %s\n", "ba", H::sum_ba.get());
    std::fprintf(out, "\n%s\n", H::label_options.get());
    std::fprintf(out, "  %-15s %s\n", "-h, --help", H::opt_help.get());
    std::fprintf(out, "  %-15s %s\n", "-V, --version", H::opt_version.get());
    std::fprintf(out, "\n");
    for (const std::string& line : spirula::i18n::wrap(H::top_note.get(), 78))
        std::fprintf(out, "%s\n", line.c_str());
    std::fprintf(out, "\n%s\n", H::label_environment.get());
    std::fprintf(out, "  %-19s %s\n", "SS_SFM_MAP_PROF=1", H::env_map_prof.get());
}

// A usage error, in the shape every command-line tool uses: what was wrong, and
// where to look. Never exit code 2 or 3 -- `auto` spends those on the quality
// of the reconstruction, and a batch script must be able to tell them apart.
static int usageError(const char* cmd, const std::string& msg) {
    std::fprintf(stderr, "%s %s: %s\n", kProgram, cmd,
                 spirula::i18n::format(CM::error_line, {msg}).c_str());
    std::fprintf(stderr, "%s\n",
                 spirula::i18n::format(
                     CM::usage_try_help,
                     {std::string(kProgram) + " " + cmd + " --help"}).c_str());
    return 1;
}

// Offer one token to the descriptor table. Returns 1 handled, 0 not a flag of
// this command, -1 usage error (already reported).
static int tableFlag(SfmConfig& cfg, uint32_t cmd, const char* cmdname, const std::string& a,
                     int argc, char** argv, int& i, std::set<std::string>& seen) {
    std::string err;
    FieldResult r = setConfigField(cfg, cmd, a, argc, argv, i, seen, err);
    if (r == FieldResult::Ok) return 1;
    if (r == FieldResult::Error) { usageError(cmdname, err); return -1; }
    return 0;
}

// --camera-model / --focal / --distortion: a bare value sets the dataset-wide
// default (which is a table field), PREFIX=VALUE names one camera group (which
// is not).
static bool cameraOverride(SfmConfig& cfg, OverrideKind kind, const std::string& v,
                           std::set<std::string>& seen, std::string& err) {
    const char* flag = kind == OverrideKind::Focal        ? "focal"
                       : kind == OverrideKind::Distortion ? "distortion"
                                                          : "camera-model";
    const char* form = kind == OverrideKind::Focal        ? "F or PREFIX=F"
                       : kind == OverrideKind::Distortion ? "k1,k2,... or PREFIX=k1,k2,..."
                                                          : "MODEL or PREFIX=MODEL";
    if (!parseCameraOverride(v, kind, cfg.camera.overrides)) {
        err = std::string("bad --") + flag + " '" + v + "' (" + form + ")";
        return false;
    }
    if (v.find('=') != std::string::npos) return true;  // per-group only
    switch (kind) {
        case OverrideKind::Focal:
            cfg.focal = std::atof(v.c_str());
            break;
        case OverrideKind::Distortion:
            cfg.distortion = v;
            break;
        case OverrideKind::Model: {
            CamModel m;
            if (!parseCamModelName(v, m)) {
                err = "unknown --camera-model '" + v + "'";
                return false;
            }
            cfg.camera_model = v;
            break;
        }
    }
    seen.insert(flag);
    return true;
}

// The three flags above, recognized by name; false for anything else.
static bool cameraOverrideFlag(const std::string& a, OverrideKind& kind) {
    if (a == "--camera-model") kind = OverrideKind::Model;
    else if (a == "--focal") kind = OverrideKind::Focal;
    else if (a == "--distortion") kind = OverrideKind::Distortion;
    else return false;
    return true;
}

// Did the command line say anything about cameras? If not, `map` keeps the
// setup verification recorded in matches.bin rather than deriving its own (D47).
static bool sawCameraFlags(const std::set<std::string>& seen) {
    static const char* kCameraFlags[] = {"camera-mode", "camera-model", "focal", "distortion",
                                         "exif-focal", "exif-groups", "exif-focal-tol"};
    for (const char* f : kCameraFlags)
        if (seen.count(f)) return true;
    return false;
}






// The CLI's progress lines, derived from the event stream rather than printed
// where the work happens -- so the GUI's bar and this text cannot disagree
// about what a stage is doing. A quiet run still emits, and still traces.
static bool g_print_progress = false;

static void printEvent(const sfm::Event& e) {
    using K = sfm::Event::Kind;
    // SS_SFM_EVENT_TRACE=1 shows the stream a front end sees. The one way to
    // check that what the GUI reads and what the CLI prints are the same run.
    static const bool trace = spirula::env("SFM_EVENT_TRACE") != nullptr;
    if (trace) {
        static const char* kKind[] = {"stage-begin", "stage-end", "progress",
                                      "image", "pair", "model", "result"};
        static const char* kStage[] = {"extract", "match", "map",
                                       "merge", "orient", "finish"};
        L::diag(Tag::Run, "[ev] %-11s %-7s done=%lld/%lld reg=%lld pts=%lld %s",
                kKind[(int)e.kind], kStage[(int)e.stage], (long long)e.done,
                (long long)e.total, (long long)e.registered, (long long)e.points,
                e.name.c_str());
    }
    // The same facts, for a front end watching this process from outside.
    // A no-op without --progress-dir.
    sfm::progress::status(e);
    if (!g_print_progress) return;
    switch (e.kind) {
        case K::ImageExtracted:
            if (e.masked)
                L::out(Tag::Extract, M::extract_progress_masked,
                       {(long long)e.done, (long long)e.total, e.name,
                        (long long)e.features, (long long)e.masked});
            else
                L::out(Tag::Extract, M::extract_progress,
                       {(long long)e.done, (long long)e.total, e.name,
                        (long long)e.features});
            break;
        case K::Progress: {
            // A line a second on a fast GPU and one every five minutes on an
            // Apple M2 matching 8192-feature images, which reads as a hung
            // program. Rate-limit by time, keeping the count for a parser.
            static double last = 0.0;
            if (e.stage != sfm::Stage::Match) break;
            const double t = now();
            if (e.done != e.total && t - last < 2.0) break;
            last = t;
            L::err(Tag::Match, M::match_progress,
                   {(long long)e.done, (long long)e.total});
            break;
        }
        default:
            break;
    }
}

static void installEventPrinter(const SfmConfig& cfg) {
    g_print_progress = !cfg.quiet;
    sfm::events::set_sink(printEvent);
}















// Mean/median reprojection error straight from a model, with no FeatureSets in
// hand: Image::points2D already carries the keypoint coordinates. `merge`
// starts from models on disk, where that is all there is. Observations behind
// the camera are excluded rather than counted as infinite, as reprojStats does.
static void modelReprojStats(const Reconstruction& rec, double& mean, double& median,
                             size_t& nobs) {
    std::vector<double> e;
    for (const auto& kv : rec.points3D)
        for (const TrackElement& t : kv.second.track) {
            auto img = rec.images.find(t.image_id);
            if (img == rec.images.end() || !img->second.registered) continue;
            auto cam = rec.cameras.find(img->second.camera_id);
            if (cam == rec.cameras.end() || t.point2D_idx >= img->second.points2D.size()) continue;
            double r = reprojErrorAt(cam->second, img->second.pose,
                                     img->second.points2D[t.point2D_idx], kv.second.xyz);
            if (r < 1e29) e.push_back(r);
        }
    nobs = e.size();
    mean = median = 0;
    if (e.empty()) return;
    double s = 0;
    for (double v : e) s += v;
    mean = s / e.size();
    std::nth_element(e.begin(), e.begin() + e.size() / 2, e.end());
    median = e[e.size() / 2];
}


static std::vector<Reconstruction> mergeModels(std::vector<Reconstruction> models,
                                               const MergeOptions& mo, bool refine, int device,
                                               MergeSummary& sum) {
    sum.before = sum.after = models.size();
    if (models.size() < 2) return models;
    const double t0 = now();
    MergeSession session(std::move(models), mo);
    sum.merges = session.runAuto();
    for (const MergeAttempt& a : session.log())
        if (!a.merged) sum.refused++;
    if (refine && sum.merges) {
        const double tb = now();
        VkContext ctx;  // one device + pipeline set for every model, as the mapper does
        for (size_t i : session.changed()) {
            Reconstruction& m = session.modelMut(i);
            BundleOptions bo;
            bo.device = device;
            bo.verbose = false;
            bo.shared_ctx = &ctx;
            double cost = runGlobalBA(m, bo);
            size_t robs = 0, rpts = 0;
            filterModel(m, mo.filter_reproj_error, mo.min_tri_angle_deg, robs, rpts);
            if (mo.verbose)
                L::err(Tag::Merge, M::merge_rebundled,
                       {(long long)i, cost, (long long)robs, (long long)rpts,
                        (long long)m.points3D.size()});
        }
        sum.ba_seconds = now() - tb;
    }
    std::vector<Reconstruction> out = session.take();
    sum.after = out.size();
    sum.seconds = now() - t0;
    return out;
}

// ---- reading models back off disk ----
// An input is either one model directory or a `sparse/` holding 0, 1, 2, ...
// Numbered sub-directories are read in numeric order, so index 0 stays the
// model with the most structure and therefore the natural anchor.
static bool isModelDir(const fs::path& p) {
    return fs::exists(p / "cameras.bin") && fs::exists(p / "images.bin");
}

static bool collectModelDirs(const std::string& input, std::vector<fs::path>& out) {
    fs::path p(input);
    if (!fs::is_directory(p)) {
        L::fail(Tag::Map, M::map_not_a_directory, {input});
        return false;
    }
    if (isModelDir(p)) {
        out.push_back(p);
        return true;
    }
    std::vector<std::pair<long, fs::path>> numbered;
    for (const auto& e : fs::directory_iterator(p)) {
        if (!e.is_directory() || !isModelDir(e.path())) continue;
        const std::string n = e.path().filename().string();
        if (n.empty() || n.find_first_not_of("0123456789") != std::string::npos) continue;
        numbered.emplace_back(std::stol(n), e.path());
    }
    if (numbered.empty()) {
        L::fail(Tag::Map, M::map_no_model_in, {input});
        return false;
    }
    std::sort(numbered.begin(), numbered.end());
    for (auto& kv : numbered) out.push_back(kv.second);
    return true;
}

static bool readModels(const std::string& dir, std::vector<Reconstruction>& models, bool verbose) {
    std::vector<fs::path> dirs;
    if (!collectModelDirs(dir, dirs)) return false;
    for (const fs::path& d : dirs) {
        try {
            models.push_back(Reconstruction::readBinary(d.string()));
        } catch (const std::exception& e) {
            L::fail(Tag::Map, M::map_cannot_read, {d.string(), e.what()});
            return false;
        }
        if (verbose)
            L::out(Tag::Map, M::map_read_model,
                   {d.string(), models.back().numRegistered(),
                    (long long)models.back().points3D.size()});
    }
    return true;
}






// -----------------------------------------------------------------------
// extract
// -----------------------------------------------------------------------





static int cmdExtract(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string image, output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("extract")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("extract", "--output: missing value");
            output = argv[++i];
            continue;
        }
        int r = tableFlag(cfg, CMD_EXTRACT, "extract", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("extract", "unknown option " + a);
        if (!image.empty())
            return usageError("extract", "unexpected argument '" + a + "'");
        image = a;
    }
    if (std::string err = cfg.finalize(CMD_EXTRACT); !err.empty())
        return usageError("extract", err);
    installEventPrinter(cfg);
    if (image.empty())
        return usageError("extract", "an image or a directory of images is required");

    // ---- directory (batch) ----
    if (fs::is_directory(image)) {
        adoptExrColorSpace(cfg, image, seen);
        fs::path outdir = output.empty() ? fs::path("features") : fs::path(output);
        ExtractStats st;
        int rc = extractDirectory(image, outdir, cfg, st);
        if (rc) return rc;
        L::out(Tag::Extract, M::extract_dir_done,
               {(long long)st.images, (unsigned long long)st.features});
        if (st.masked_out)
            L::out(Tag::Extract, M::extract_dir_masked,
                   {(unsigned long long)st.masked_out,
                    (long long)st.masked_images});
        if (st.failed || st.unreadable)
            L::out(Tag::Extract, M::extract_dir_failed,
                   {(long long)st.failed, (long long)st.unreadable});
        warnIfMasksLookInverted(st);
        return (st.failed || st.unreadable) ? 1 : 0;
    }

    // ---- single image ----
    // One image has no relative path to key on, so the mask is looked up by
    // filename -- MaskIndex's basename fallback resolves it either way.
    std::string maskpath;
    if (!cfg.mask_dir.empty()) {
        maskpath = MaskIndex(cfg.mask_dir).find(fs::path(image).filename().generic_string());
        if (maskpath.empty())
            L::warn(Tag::Extract, M::match_no_mask_for,
                    {image, cfg.mask_dir});
    }
    GrayImage img = loadGrayImage(image, cfg.max_image_size, /*want_color=*/true, maskpath,
                                  cfg.image_gamut, cfg.image_is_linear, cfg.flip_mask);
    if (cfg.sift.verbose)
        L::err(Tag::Extract, M::extract_to_gray,
               {image, img.width, img.height});
    std::unique_ptr<IFeatureExtractor> ext =
        createFeatureExtractor(cfg.features, cfg.sift, cfg.aliked, cfg.loma);
    FeatureSet fset = ext->extract(img);
    sampleFeatureColors(fset, img);
    uint32_t masked_out = applyMask(fset, img.mask);
    finishFeatures(fset, img);
    L::out(Tag::Extract, M::extract_one_done,
           {fset.count(), fset.dim, fset.width, fset.height});
    if (fset.exif_focal > 0)
        L::out(Tag::Extract, M::extract_one_exif_focal,
               {L::num(fset.exif_focal, 1)});
    if (!img.mask.empty())
        L::out(Tag::Extract, M::extract_one_mask,
               {img.mask.width, img.mask.height, masked_out});
    if (!output.empty()) {
        writeFeatures(output, fset);
        if (cfg.sift.verbose) L::err(Tag::Extract, M::wrote_file, {output});
    }
    return 0;
}

// -----------------------------------------------------------------------
// match
// -----------------------------------------------------------------------




static int cmdMatch(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string featdir, output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("match")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("match", "--output: missing value");
            output = argv[++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return usageError("match", "--progress-dir: missing value");
            sfm::progress::set_dir(argv[++i]);
            continue;
        }
        if (OverrideKind kind; cameraOverrideFlag(a, kind)) {
            if (i + 1 >= argc) return usageError("match", a + ": missing value");
            std::string err;
            if (!cameraOverride(cfg, kind, argv[++i], seen, err))
                return usageError("match", err);
            continue;
        }
        int r = tableFlag(cfg, CMD_MATCH, "match", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("match", "unknown option " + a);
        if (!featdir.empty()) return usageError("match", "unexpected argument '" + a + "'");
        featdir = a;
    }
    if (std::string err = cfg.finalize(CMD_MATCH); !err.empty())
        return usageError("match", err);
    installEventPrinter(cfg);
    if (featdir.empty()) return usageError("match", "a feature directory is required");

    VerifyCalibration calib;
    calib.setup = cfg.camera;
    std::vector<FeatureSet> feats;
    MatchesDatabase db;
    MatchStats stats;
    if (int rc = matchFeatureDir(featdir, cfg, cfg.pairMode(), cfg.verify, feats, db, stats,
                                 &calib))
        return rc;
    if (cfg.verify)
        L::out(Tag::Match, M::match_done_inliers,
               {(long long)stats.kept, (long long)stats.pairs,
                (unsigned long long)stats.inliers,
                (unsigned long long)stats.putative});
    else
        L::out(Tag::Match, M::match_done_raw,
               {(long long)stats.kept, (long long)stats.pairs,
                (unsigned long long)stats.inliers});
    if (!output.empty()) {
        writeMatches(output, db);
        if (!cfg.quiet) L::err(Tag::Match, M::wrote_file, {output});
    }
    return 0;
}

// -----------------------------------------------------------------------
// map
// -----------------------------------------------------------------------

static int cmdMap(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::string matchesPath, output;
    bool audit_first = false;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("map")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("map", "--output: missing value");
            output = argv[++i];
            continue;
        }
        if (a == "--progress-dir") {
            if (i + 1 >= argc) return usageError("map", "--progress-dir: missing value");
            sfm::progress::set_dir(argv[++i]);
            continue;
        }
        // Before the table, which also owns "audit" -- as the assembler's
        // audit stage (--no-audit). Here the positive spelling is the one-shot
        // pass over adopted models, which is what it has always meant.
        if (a == "--audit") { audit_first = true; continue; }
        if (a == "--no-manage") {
            cfg.manager.do_merge = cfg.manager.do_grow = cfg.manager.do_reseed = false;
            cfg.manager.do_audit = cfg.manager.do_split = cfg.manager.do_duplicate_split = false;
            continue;
        }
        if (OverrideKind kind; cameraOverrideFlag(a, kind)) {
            if (i + 1 >= argc) return usageError("map", a + ": missing value");
            std::string err;
            if (!cameraOverride(cfg, kind, argv[++i], seen, err))
                return usageError("map", err);
            continue;
        }
        if (a == "--rig") {
            if (i + 1 >= argc) return usageError("map", "--rig: missing value");
            RigDef d;
            if (std::string err = parseRigArg(argv[++i], d); !err.empty())
                return usageError("map", err);
            cfg.rigs.push_back(std::move(d));
            continue;
        }
        int r = tableFlag(cfg, CMD_MAP, "map", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("map", "unknown option " + a);
        if (matchesPath.empty()) matchesPath = a;
        else if (cfg.feature_dir.empty()) cfg.feature_dir = a;
        else return usageError("map", "unexpected argument '" + a + "'");
    }
    if (std::string err = cfg.finalize(CMD_MAP); !err.empty()) return usageError("map", err);
    if (matchesPath.empty() || cfg.feature_dir.empty())
        return usageError("map", "a match database and a feature directory are required");

    MapperOptions& opt = cfg.mapper;
    ManagerOptions& mgopt = cfg.manager;
    const std::string& featdir = cfg.feature_dir;

    MatchesDatabase db = readMatches(matchesPath);
    std::optional<FeatureCompactionPlan> compaction;
    if (cfg.compact_unused_features) compaction.emplace(buildFeatureCompactionPlan(db));
    std::vector<FeatureSet> feats(db.images.size());
    {
        // Descriptors are skipped: matching is over, and on a 5000-image
        // capture they are several gigabytes of file that nothing downstream
        // reads. Parallel because it is pure per-file work landing by index.
        const unsigned hc = std::thread::hardware_concurrency();
        int nt = cfg.threads > 0 ? cfg.threads : (hc > 0 ? (int)hc : 1);
        nt = std::max(1, std::min<int>(nt, (int)db.images.size()));
        std::atomic<size_t> next{0};
        std::mutex err_mtx;
        std::string first_error;  // a bad file must report itself, not terminate
        std::vector<std::thread> pool;
        for (int t = 0; t < nt; t++)
            pool.emplace_back([&] {
                for (size_t i = next++; i < db.images.size(); i = next++) {
                    try {
                        FeatureSet loaded =
                            readFeatures(featdir + "/" + db.images[i].name + ".bin", false);
                        if (compaction)
                            feats[i] = compactFeatureSet(std::move(loaded),
                                                         compaction->old_to_new[i],
                                                         compaction->compact_counts[i]);
                        else
                            feats[i] = std::move(loaded);
                    } catch (const std::exception& e) {
                        std::lock_guard<std::mutex> lk(err_mtx);
                        if (first_error.empty()) first_error = e.what();
                    }
                }
            });
        for (std::thread& th : pool) th.join();
        if (!first_error.empty()) {
            L::err_raw(Tag::Map, first_error);
            return 1;
        }
    }
    if (compaction) {
        remapMatches(db, *compaction, feats);
        const FeatureCompactionStats stats = compaction->stats;
        // old_to_new is the only temporary proportional to the original row count.
        compaction.reset();
        if (opt.verbose) reportFeatureCompaction(stats);
    }

    // The camera setup, in order of authority: what the command line asked for,
    // else what verification recorded in matches.bin (D47), else derived here.
    const bool cam_args = sawCameraFlags(seen) || !cfg.camera.overrides.empty();
    CameraSetup cs;
    const bool from_db = !cam_args && loadCameraSetup(db, cs);
    if (!from_db) cs = buildCameras(db.images, feats, cfg.camera);
    if (opt.verbose) {
        printCameraSetup(Tag::Map, cs, cfg.camera, db.images.size());
        if (from_db)
            L::err(Tag::Map, M::map_camera_setup_from_db, {matchesPath});
        else if (cam_args && db.hasCameras())
            L::err(Tag::Map, M::map_camera_setup_rebuilt, {matchesPath});
    }

    // A fisheye group with no focal prior and none recorded (D45/D46/D47).
    // `spirula-sfm auto` and `spirula-sfm match` measure this before verifying, on raw putative
    // matches, and it now travels in the match database; this is the fallback
    // for a matches.bin that predates that or arrived from elsewhere. The
    // sample here is verified *inliers*, so if the verification was itself
    // calibrated the answer is pulled towards the focal it used -- which is
    // exactly why carrying the measured one is better than re-deriving it.
    if (!from_db && db.pairs.size() >= 8) {
        std::vector<std::pair<uint32_t, uint32_t>> sample;
        std::vector<std::vector<FeatureMatch>> sm;
        const size_t want = 200 * std::max<size_t>(1, cs.count());
        size_t stride = std::max<size_t>(1, db.pairs.size() / want);
        for (size_t p = 0; p < db.pairs.size() && sample.size() < want; p += stride) {
            sample.push_back({db.pairs[p].image1, db.pairs[p].image2});
            sm.push_back(db.pairs[p].matches);
        }
        bootstrapGroupFocals(feats, cs.ids, sample, sm, cs.cameras, cs.focal_given,
                             cs.focal_measured, TwoViewOptions{}, 200, 0, opt.verbose);
        for (uint32_t id : cs.focal_measured) cs.focal_known.insert(id);
    }
    opt.initial_cameras = cs.cameras;
    opt.known_focal_cameras = cs.focal_known;
    opt.given_focal_cameras = cs.focal_given;
    opt.measured_focal_cameras = cs.focal_measured;

    RigTable rigs;
    try {
        rigs = buildRigs(db, cfg, opt.verbose);
    } catch (const std::runtime_error& e) {
        L::fail(Tag::Map, M::rig_bad, {e.what()});
        return 1;
    }
    Mapper mapper(db, feats, opt, cs.ids, &rigs);
    std::vector<Reconstruction> models;
    AssembleStats ast;
    if (cfg.resume.empty()) {
        models = runMapper(mapper, db, feats, cfg, ast);
    } else {
        // Adopt what a previous run wrote and work on it instead. The models
        // must come from this database (image ids are positions in it); adopt()
        // checks the names and says so if they do not.
        if (!readModels(cfg.resume, models, opt.verbose)) return 1;
        // point2D_idx indexes this run's feature arrays. A model written with a
        // different --compact-unused-features would index other keypoints, and
        // the mapper can only drop those observations, not recover them.
        for (const Reconstruction& m : models)
            for (const auto& kv : m.images) {
                if (!kv.second.registered || kv.first >= feats.size()) continue;
                if (kv.second.points2D.size() == feats[kv.first].count()) continue;
                L::err_raw(Tag::Map,
                           "resumed model image '" + kv.second.name + "' holds " +
                               std::to_string(kv.second.points2D.size()) +
                               " keypoints but this run's features have " +
                               std::to_string(feats[kv.first].count()) +
                               "; --compact-unused-features must match the run that wrote " +
                               cfg.resume);
                return 1;
            }
        L::out(Tag::Map, M::map_resumed,
               {(long long)models.size(),
                (long long)distinctRegistered(models),
                (long long)db.images.size()});
    }
    if (models.empty()) {
        L::fail(Tag::Map, M::map_no_model_to_work_with);
        return 1;
    }

    // Diagnostic (D45): does each model agree with the two-view geometries it
    // was built from? A model welded together at a wrong relative pose is
    // internally perfect and fails no other test, but the verified pairs that
    // span the weld do not hold, and the images fall into disconnected groups.
    if (cfg.check) {
        for (size_t i = 0; i < models.size(); i++) {
            Mapper::SplitStats ss;
            std::vector<Reconstruction> parts = mapper.splitInconsistent(
                models[i], mgopt.merge.max_reproj_error, mgopt.seam_min_pair_fraction,
                mgopt.split_min_matches, mgopt.split_min_group, &ss);
            printf("model %zu: %u images, %zu points | %zu/%zu inner pairs hold (%.0f%%)\n"
                   "    pair agreement p05/p10/p25/p50 %.2f/%.2f/%.2f/%.2f | "
                   "%zu group(s), largest %zu",
                   i, models[i].numRegistered(), models[i].points3D.size(), ss.pairs_agree,
                   ss.pairs_tested, ss.pairs_tested ? 100.0 * ss.pairs_agree / ss.pairs_tested : 0.0,
                   ss.percentile(0.05), ss.percentile(0.10), ss.percentile(0.25),
                   ss.percentile(0.50), ss.groups, ss.largest);
            if (parts.size() > 1) {
                printf(" -> would split into");
                for (const Reconstruction& p : parts) printf(" %u", p.numRegistered());
                if (ss.dropped_images) printf(" (%zu images dropped)", ss.dropped_images);
            }
            printf("\n");
            DuplicateReport dr =
                findDuplicateStructure(models[i], mgopt.duplicate, mapper.matchedPredicate());
            printf("    duplicate structure: %zu of %zu co-located pairs share no points "
                   "and never matched (%.0f%%; %zu more share none but did match)\n",
                   dr.conflicts, dr.colocated, 100.0 * dr.ratio(), dr.unmatched_but_seen);
            // The cut is reported whenever there are conflicts at all, not only
            // when the rate passes: the whole point is that the rate is not the
            // criterion (D46), so both numbers have to be visible side by side.
            if (!dr.pairs.empty()) {
                size_t dropped = 0;
                DuplicateCut cut;
                std::vector<Reconstruction> dp = splitDuplicateStructure(
                    models[i], dr, mgopt.split_min_group, &dropped, &cut,
                    mgopt.duplicate.min_fold_overlap);
                printf("    cut: %zu group(s), severs %.2f%% of co-visibility (%llu of %llu)%s",
                       cut.groups, 100.0 * cut.fraction(), (unsigned long long)cut.severed,
                       (unsigned long long)(cut.severed + cut.kept),
                       foldSplitAccepted(dr, cut, mgopt.duplicate) ? " -- folded" : " -- kept whole");
                if (dp.size() > 1) {
                    printf(" -> would split into");
                    for (const Reconstruction& p : dp) printf(" %u", p.numRegistered());
                    if (dropped) printf(" (%zu images dropped)", dropped);
                }
                printf("\n");
            }
        }
        return 0;
    }

    // Audit models that came from elsewhere before doing anything with them:
    // every image's pose is checked against the correspondence graph, and the
    // ones the model cannot support are re-registered (D44).
    if (audit_first) {
        for (size_t i = 0; i < models.size(); i++) {
            Mapper::AuditStats as;
            models[i] = mapper.audit(models[i], &as);
            printf("audit model %zu: %u images checked, %u unsupported, %u re-registered, "
                   "%u dropped -> %u images\n",
                   i, as.checked, as.unsupported, as.reregistered, as.deregistered,
                   models[i].numRegistered());
        }
    }

    {
        double t_finish = 0;
        models = finishModels(mapper, std::move(models), cfg, opt.verbose, t_finish);
    }
    printAssembly(ast, models.size());

    resolveImageNames(models, cfg.image_dir);
    // The mapper reported its own stage when run() returned; the passes that
    // assemble its models add to the same counters.
    g_map_prof.report(0, "map");

    const Reconstruction& rec = models.front();
    double mean = 0, median = 0;
    size_t nobs = 0;
    reprojStats(rec, feats, mean, median, nobs);
    L::out(Tag::Map, M::map_reconstruction_summary,
           {rec.numRegistered(), (long long)db.images.size(),
            (long long)rec.points3D.size(), L::num(mean, 3), L::num(median, 3),
            (long long)nobs});
    if (models.size() > 1) {
        L::out(Tag::Map, M::map_models_covering,
               {(long long)models.size(),
                (long long)distinctRegistered(models),
                (long long)db.images.size()});
        printExtraModels(models, feats);
    }
    std::vector<sfm::ModelGauge> map_gauge;
    const bool map_metric = fixGauge(models, cfg, cfg.image_dir, opt.verbose, map_gauge);
    recolorPoints(models, cfg);
    splitCamerasBySize(models, feats);
    if (!output.empty()) writeModels(models, output, opt.verbose, map_gauge, &rigs);
    return map_metric ? 0 : 4;
}

// -----------------------------------------------------------------------
// merge: fold the models of a fragmented capture back together (D43)
// -----------------------------------------------------------------------

static int cmdMerge(int argc, char** argv) {
    SfmConfig cfg;
    std::set<std::string> seen;
    std::vector<std::string> inputs;
    std::string output;
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { printCommandHelp(*findCommand("merge")); return 0; }
        if (a == "--output" || a == "-o") {
            if (i + 1 >= argc) return usageError("merge", "--output: missing value");
            output = argv[++i];
            continue;
        }
        int r = tableFlag(cfg, CMD_MERGE, "merge", a, argc, argv, i, seen);
        if (r < 0) return 1;
        if (r > 0) continue;
        if (a[0] == '-') return usageError("merge", "unknown option " + a);
        inputs.push_back(a);
    }
    if (std::string err = cfg.finalize(CMD_MERGE); !err.empty()) return usageError("merge", err);
    if (inputs.empty()) return usageError("merge", "at least one model directory is required");
    if (output.empty() && !cfg.in_place)
        return usageError("merge", "--output is required (or --in-place)");

    const MergeOptions& mo = cfg.merge;
    std::vector<fs::path> dirs;
    for (const std::string& in : inputs)
        if (!collectModelDirs(in, dirs)) return 1;
    if (cfg.in_place) {
        if (inputs.size() != 1)
            return usageError("merge", "--in-place takes exactly one input directory");
        if (output.empty()) output = inputs[0];
    }
    // Writing merged models over their own inputs is destructive (model 0 is
    // replaced and the absorbed ones are removed), so it has to be asked for.
    if (!cfg.in_place)
        for (const fs::path& d : dirs)
            if (fs::exists(output) && fs::equivalent(fs::path(output), d.parent_path())) {
                L::fail(Tag::Merge, CM::sfm_merge_output_is_input, {output});
                return 1;
            }

    std::vector<Reconstruction> models;
    for (const fs::path& d : dirs) {
        try {
            models.push_back(Reconstruction::readBinary(d.string()));
        } catch (const std::exception& e) {
            L::fail(Tag::Merge, M::map_cannot_read, {d.string(), e.what()});
            return 1;
        }
        L::out(Tag::Merge, M::map_read_model,
               {d.string(), models.back().numRegistered(),
                (long long)models.back().points3D.size()});
    }
    // A metric reference re-gauges a model instead of joining it to another,
    // which is the one thing this command does that one model can want (D74).
    const bool metric = cfg.metric_gps != "none" || !cfg.metric_positions.empty() ||
                        (!cfg.telemetry_inputs.empty() && cfg.sensor_gauge != "none");
    if (models.size() < 2 && !metric) {
        L::fail(Tag::Merge, M::merge_need_two, {(long long)models.size()});
        return 1;
    }
    if (models.size() < 2) L::out(Tag::Merge, M::merge_metric_only, {});
    else {
        const size_t covered_before = distinctRegistered(models);

        MergeSummary sum;
        models = mergeModels(std::move(models), mo, cfg.merge_ba, cfg.device, sum);

        L::out(Tag::Merge, M::merge_summary,
               {(long long)sum.before, (long long)sum.after, L::num(sum.seconds, 2),
                (long long)sum.merges, (long long)sum.refused});
        if (sum.ba_seconds > 0)
            L::out(Tag::Merge, M::merge_ba_seconds, {L::num(sum.ba_seconds, 2)});
        for (size_t i = 0; i < models.size(); i++) {
            double mean = 0, median = 0;
            size_t nobs = 0;
            modelReprojStats(models[i], mean, median, nobs);
            L::out(Tag::Merge, M::merge_model_line,
                   {(long long)i, models[i].numRegistered(),
                    (long long)models[i].points3D.size(), L::num(mean, 3),
                    L::num(median, 3), (long long)nobs});
        }
        const size_t covered_after = distinctRegistered(models);
        if (covered_after != covered_before)
            L::out(Tag::Merge, M::merge_survived,
                   {(long long)covered_after, (long long)covered_before});
    }

    std::vector<sfm::ModelGauge> merge_gauge;
    const bool merge_metric = fixGauge(models, cfg, cfg.image_dir, mo.verbose, merge_gauge);
    recolorPoints(models, cfg);
    writeModels(models, fs::path(output), mo.verbose, merge_gauge);
    // In place, the models that were absorbed must not stay behind as stale
    // directories claiming to be reconstructions.
    if (cfg.in_place)
        for (const fs::path& d : dirs) {
            const std::string name = d.filename().string();
            if (d.parent_path() != fs::path(output) || name.empty() ||
                name.find_first_not_of("0123456789") != std::string::npos)
                continue;
            long idx = std::stol(name);
            if (idx >= (long)models.size()) {
                fs::remove_all(d);
                L::out(Tag::Merge, M::merge_removed_absorbed, {d.string()});
            }
        }
    return merge_metric ? 0 : 4;
}

// -----------------------------------------------------------------------
// auto: one command from an image directory to a COLMAP sparse model
// -----------------------------------------------------------------------
//
// The equivalent of COLMAP's `automatic_reconstructor`: pick sane settings from
// two knobs (quality, data type), run extract -> match -> map in one process,
// and lay the workspace out the way COLMAP does so existing tooling reads it.
// The presets themselves live in SfmConfig.cpp's applyPresets, next to the
// table they move, and report what they moved.

static int cmdAuto(int argc, char** argv) {
    std::vector<std::string> args(argv, argv + argc);
    AutoRequest req;
    if (std::string err = parse_auto_args(args, req); !err.empty())
        return usageError("auto", err);
    if (req.wants_help) { printCommandHelp(*findCommand("auto")); return 0; }
    if (!req.progress_dir.empty()) sfm::progress::set_dir(req.progress_dir);
    installEventPrinter(req.cfg);
    return run_auto(req.cfg, req.in).exit_code;
}

int spirula_sfm_main(int argc, char** argv) {
    app::set_program_name(argc > 0 ? argv[0] : nullptr, "spirula sfm");
    kProgram = app::program_name().c_str();
    std::signal(SIGINT, sfmOnInterrupt);
    sfm::cancel::set_token(&g_interrupted);
    if (argc < 2) {
        printTopHelp(stderr);
        return 1;
    }
    // Accept `--flag=value` as well as `--flag value`, everywhere. Only tokens
    // that start with `--` are split, and only at their first `=`, so a value
    // that itself contains one (`--camera-model cam0=opencv-fisheye`) is
    // untouched. Every subcommand parses positionally, so this is the one place
    // that has to know.
    std::vector<std::string> store;
    std::vector<char*> args;
    store.reserve((size_t)argc * 2);
    args.reserve((size_t)argc * 2);
    for (int i = 0; i < argc; i++) {
        std::string a = argv[i];
        size_t eq = a.find('=');
        if (i >= 2 && a.size() > 2 && a[0] == '-' && a[1] == '-' && eq != std::string::npos &&
            eq > 2) {
            store.push_back(a.substr(0, eq));
            store.push_back(a.substr(eq + 1));
        } else {
            store.push_back(std::move(a));
        }
    }
    for (std::string& a : store) args.push_back(&a[0]);
    argc = (int)args.size();
    argv = args.data();

    std::string cmd = argv[1];
    if (cmd == "--help" || cmd == "-h" || cmd == "help") {
        // `spirula-sfm help <command>` is the same as `<command> --help`.
        if (argc > 2) {
            if (std::string(argv[2]) == "ba") { printBaHelp(stdout); return 0; }
            if (const CommandInfo* c = findCommand(argv[2])) { printCommandHelp(*c); return 0; }
            std::fprintf(stderr, "%s: error: unknown command '%s'\n", kProgram, argv[2]);
            return 1;
        }
        printTopHelp(stdout);
        return 0;
    }
    if (cmd == "--version" || cmd == "-V" || cmd == "version") {
        std::printf("%s %s\n", kProgram, SS_VERSION);
        return 0;
    }
    // One catch for every subcommand. Setup failures throw rather than return
    // -- a checkpoint that will not download, a matcher handed the wrong kind
    // of descriptor -- and those messages are written to be read by the person
    // who typed the command, not by a terminate handler.
    try {
        if (cmd == "auto") return cmdAuto(argc - 2, argv + 2);
        if (cmd == "extract") return cmdExtract(argc - 2, argv + 2);
        if (cmd == "match") return cmdMatch(argc - 2, argv + 2);
        if (cmd == "map") return cmdMap(argc - 2, argv + 2);
        if (cmd == "merge") return cmdMerge(argc - 2, argv + 2);
        if (cmd == "ba") return cmdBa(argc - 2, argv + 2);
    } catch (const sfm::Cancelled&) {
        L::err(Tag::Run, M::run_cancelled);
        return 130;   // the shell's convention for SIGINT, 128 + 2
    } catch (const std::exception& e) {
        std::fprintf(stderr, "%s %s: error: %s\n", kProgram, cmd.c_str(), e.what());
        return 1;
    }
    std::fprintf(stderr, "%s: error: unknown command '%s'\n", kProgram, cmd.c_str());
    std::fprintf(stderr, "%s\n",
                 spirula::i18n::format(
                     CM::usage_try_help,
                     {std::string(kProgram) + " --help"}).c_str());
    return 1;
}
